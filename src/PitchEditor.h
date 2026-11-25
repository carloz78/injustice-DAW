#pragma once
#include "FloatingPanel.h"
#include "AudioEdit.h"
#include "AudioToMidi.h"
#include "FxTune.h"
#include <thread>
#include <cmath>
#include <atomic>
#include <memory>

// INJUSTICE AUTO: editor de afinación manual (gráfico). Analiza el clip de audio elegido (una sola voz), lo divide en notas que se ven sobre
// un piano roll, deja moverlas / partirlas / unirlas / dibujar la curva de tono, y al aplicar crea un archivo nuevo en "Editados" con la
// voz corregida (cambiador de tono de fase con formantes conservados) y el clip pasa a usarlo. El original nunca se toca.
namespace pitched
{
struct Note { int f0 = 0, f1 = 0; float det = 60.f, tgt = 60.f, keep = 1.f; bool man = false; };
struct Track
{
    double sr = 44100.0; int hop = 512, win = 2048; std::vector<float> midi; std::vector<Note> notes;
    int frames() const { return (int) midi.size(); }
    double tOf (double f) const { return (f * hop + win * 0.5 - hop * 0.5) / sr; }   // inicio (s) del fotograma f
    double fOf (double t) const { return (t * sr - win * 0.5 + hop * 0.5) / hop; }
};
inline float medianOf (std::vector<float> v) { if (v.empty()) return 60.f; std::nth_element (v.begin(), v.begin() + (long) v.size() / 2, v.end()); return v[v.size() / 2]; }
inline void segment (Track& t)
{
    t.notes.clear(); const int n = t.frames(); int f = 0;
    while (f < n)
    {
        if (t.midi[(size_t) f] < 0.f) { ++f; continue; }
        const int s = f; int e = f, gap = 0, outl = 0, cnt = 1; float mean = t.midi[(size_t) f]; int g = f + 1;
        while (g < n)
        {
            const float m = t.midi[(size_t) g];
            if (m < 0.f) { if (++gap > 2) break; ++g; continue; }
            gap = 0; if (std::abs (m - mean) > .7f) { if (++outl >= 2) break; ++g; continue; }
            outl = 0; ++cnt; mean += (m - mean) / (float) std::min (cnt, 10); e = g; ++g;
        }
        if (e + 1 - s >= 4) { Note nt; nt.f0 = s; nt.f1 = e + 1; std::vector<float> v; for (int i = s; i <= e; ++i) if (t.midi[(size_t) i] >= 0.f) v.push_back (t.midi[(size_t) i]); nt.det = nt.tgt = medianOf (v); t.notes.push_back (nt); }
        f = e + 1;
    }
}
inline Track analyse (const std::vector<float>& mono, double sr, std::atomic<float>* prog, std::atomic<bool>* cancel)
{
    Track t; t.sr = sr; const int W = t.win, hop = t.hop, n = (int) mono.size(); if (n < W + hop) return t;
    const int frames = (n - W) / hop + 1; t.midi.assign ((size_t) frames, -1.f); const float gate = juce::Decibels::decibelsToGain (-50.f);
    for (int f = 0; f < frames; ++f)
    {
        if (cancel != nullptr && cancel->load()) break; if (prog != nullptr && (f & 31) == 0) prog->store ((float) f / (float) frames);
        const float* p = mono.data() + (size_t) f * hop; double e = 0; for (int i = 0; i < W; ++i) e += (double) p[i] * p[i];
        if (std::sqrt (e / W) < gate) continue; const double hz = audio2midi::yin (p, W, sr, 70.0, 1000.0, .15f); if (hz > 0.0) t.midi[(size_t) f] = (float) (69.0 + 12.0 * std::log2 (hz / 440.0));
    }
    for (int f = 1; f + 1 < frames; ++f)   // un fotograma suelto (salto de octava) se alinea con sus vecinos
    {
        const float a = t.midi[(size_t) f - 1], b = t.midi[(size_t) f], c = t.midi[(size_t) f + 1];
        if (a >= 0.f && b >= 0.f && c >= 0.f && std::abs (a - c) < .8f && std::abs (b - a) > 1.2f) t.midi[(size_t) f] = .5f * (a + c);
    }
    segment (t); if (prog != nullptr) prog->store (1.f); return t;
}
inline int maskFor (int key, int scale)
{
    const int rel = injfx::InjTunePro::scaleMask (scale); int m = 0; for (int d = 0; d < 12; ++d) if (rel & (1 << d)) m |= 1 << ((d + key) % 12); return m;
}
// Curva de corrección (semitonos por fotograma) a partir de las notas y del trazo a mano (NaN = sin trazo), suavizada con la transición pedida.
inline std::vector<float> corrections (const Track& t, const std::vector<float>& drawn, float transMs)
{
    const int F = t.frames(); std::vector<float> c ((size_t) F, 0.f);
    for (auto& nt : t.notes)
        for (int f = nt.f0; f < nt.f1 && f < F; ++f) { const float m = t.midi[(size_t) f]; c[(size_t) f] = m >= 0.f ? (nt.tgt + (m - nt.det) * nt.keep) - m : nt.tgt - nt.det; }
    for (int f = 0; f < F && f < (int) drawn.size(); ++f) if (! std::isnan (drawn[(size_t) f]) && t.midi[(size_t) f] >= 0.f) c[(size_t) f] = drawn[(size_t) f] - t.midi[(size_t) f];
    const int w = std::max (1, (int) std::lround (transMs * 0.001 * t.sr / t.hop)); if (w <= 1) return c;
    std::vector<double> ps ((size_t) F + 1, 0.0); for (int f = 0; f < F; ++f) ps[(size_t) f + 1] = ps[(size_t) f] + c[(size_t) f];
    std::vector<float> o ((size_t) F, 0.f); const int h = w / 2;
    for (int f = 0; f < F; ++f) { const int a = std::max (0, f - h), b = std::min (F, f + h + 1); o[(size_t) f] = (float) ((ps[(size_t) b] - ps[(size_t) a]) / (double) (w)); }   // los bordes se atenúan hacia 0
    return o;
}
// Renderiza la voz corregida. Las partes sin corrección salen idénticas al original.
inline audioedit::Buf render (const audioedit::Buf& in, const Track& t, const std::vector<float>& drawn, float transMs, bool keepFormants)
{
    using injfx::PVShift; const int n = in.getNumSamples(), nc = in.getNumChannels(), LAT = PVShift::LAT; audioedit::Buf out; out.makeCopyOf (in); if (t.frames() < 2 || n < 1) return out;
    const auto cs = corrections (t, drawn, transMs); const int F = t.frames();
    auto at = [&] (double fp) -> float { fp = juce::jlimit (0.0, (double) F - 1.001, fp); const int i = (int) fp; const float fr = (float) (fp - i); return cs[(size_t) i] * (1.f - fr) + cs[(size_t) i + 1] * fr; };
    for (int c = 0; c < nc; ++c)
    {
        auto core = std::make_unique<PVShift>(); core->reset (t.sr); core->keepFormants = keepFormants; core->formantRatio = 1.f; float ratio = 1.f; const float* x = in.getReadPointer (c); float* o = out.getWritePointer (c);
        for (int k = 0; k < n + LAT; ++k)
        {
            if ((k & 63) == 0) ratio = std::pow (2.f, at ((k - t.win) / (double) t.hop) / 12.f);
            const float y = core->run (k < n ? x[k] : 0.f, ratio); const int j = k - LAT; if (j < 0 || j >= n) continue;
            const float w = juce::jlimit (0.f, 1.f, std::abs (at ((j - t.win * 0.5) / (double) t.hop)) * 25.f); o[j] = x[j] * (1.f - w) + y * w;
        }
    }
    return out;
}
} // namespace pitched

class PitchEditorPanel : public FloatingPanel, private juce::Timer
{
public:
    std::function<void (const juce::String&)> onMessage; std::function<void()> onChanged;
    enum Tool { tSelect, tPencil, tSplit, tCurve, tCenter, tMerge, tMarquee };
    explicit PitchEditorPanel (te::Edit& e) : edit (e)
    {
        title = "INJUSTICE AUTO"; icon = icons::Mixer; scale = .6f; setDesign (1774, 887); bgImg = injfx::skinImage ("pe_bg_png"); icd = injfx::skinImage ("pe_icd_png"); icw = injfx::skinImage ("pe_icw_png"); pur = injfx::skinImage ("pe_pur_png");
        fm.registerBasicFormats(); startTimerHz (30);
    }
    ~PitchEditorPanel() override { stopTimer(); stopPlay(); job.reset(); }
    void setDeviceManager (juce::AudioDeviceManager* d) { dev = d; }
    // ---- estado público (también para las pruebas) ----
    te::WaveAudioClip* clip() const { return dynamic_cast<te::WaveAudioClip*> (cur.get()); }
    void setClip (te::WaveAudioClip* c) { if (c == clip() && src.getNumSamples() > 0) return; cur = c; reload(); repaint(); }
    bool ready() const { return hasData; }
    bool busy() const { return job != nullptr; }
    int numNotes() const { return (int) trk.notes.size(); }
    pitched::Note& noteAt (int i) { return trk.notes[(size_t) i]; }
    bool waitReady (int ms) { const auto t0 = juce::Time::getMillisecondCounter(); while (job != nullptr && (int) (juce::Time::getMillisecondCounter() - t0) < ms) { poll(); juce::Thread::sleep (5); } poll(); return hasData; }
    void setScale (int k, int s) { key = juce::jlimit (0, 11, k); scaleIdx = juce::jlimit (0, 11, s); if (autoMode) autoCorrect(); repaint(); }
    void setSnap (int m) { snapMode = juce::jlimit (0, 2, m); }
    void setAuto (bool a) { autoMode = a; if (a) autoCorrect(); repaint(); }
    void setTool (int t) { tool = juce::jlimit (0, 6, t); repaint(); }
    void setTargetOf (int i, float midi) { if (i < 0 || i >= numNotes()) return; pushUndo(); trk.notes[(size_t) i].tgt = midi; trk.notes[(size_t) i].man = true; repaint(); }
    void setKeepOf (int i, float k) { if (i < 0 || i >= numNotes()) return; pushUndo(); trk.notes[(size_t) i].keep = juce::jlimit (0.f, 2.f, k); trk.notes[(size_t) i].man = true; repaint(); }
    bool splitAt (int i, double t) { return doSplit (i, t); }
    bool mergeNext (int i) { return doMerge (i); }
    void autoCorrect()
    {
        if (! hasData) return; pushUndo(); const int mask = pitched::maskFor (key, scaleIdx);
        for (auto& n : trk.notes) if (! n.man) n.tgt = injfx::InjTunePro::nearestNote (n.det, mask);
        std::fill (drawn.begin(), drawn.end(), std::nanf ("")); repaint();
    }
    bool undo() { if (undos.empty()) return false; redos.push_back ({ trk.notes, drawn }); auto s = undos.back(); undos.pop_back(); trk.notes = s.notes; drawn = s.drawn; repaint(); return true; }
    bool redo() { if (redos.empty()) return false; undos.push_back ({ trk.notes, drawn }); auto s = redos.back(); redos.pop_back(); trk.notes = s.notes; drawn = s.drawn; repaint(); return true; }
    audioedit::Buf renderNow() const { return pitched::render (src, trk, drawn, transMs, keepFormants); }
    bool apply()
    {
        auto* wc = clip(); if (wc == nullptr || ! hasData) { say (juce::String::fromUTF8 ("Elige un clip de audio con una voz y espera el an\xc3\xa1lisis")); return false; }
        stopPlay(); auto b = renderNow();
        const auto out = audioedit::editedDir().getChildFile (wc->getOriginalFile().getFileNameWithoutExtension().upToFirstOccurrenceOf ("__", false, false) + "__afinado_" + juce::String (juce::Time::currentTimeMillis()) + ".wav");
        if (! audioedit::writeWav (out, b, sr)) { say (juce::String::fromUTF8 ("No se pudo guardar el audio afinado")); return false; }
        edit.getUndoManager().beginNewTransaction (juce::String::fromUTF8 ("Afinaci\xc3\xb3n (Injustice Auto)")); wc->getSourceFileReference().setToDirectFileReference (out, true);
        say (juce::String::fromUTF8 ("Afinaci\xc3\xb3n aplicada al clip")); if (onChanged) onChanged(); reload(); return true;
    }
    double fileSeconds() const { return sr > 0 ? (double) src.getNumSamples() / sr : 0.0; }

    // ---- reproducción de la vista previa (voz corregida) ----
    void startPlay (double from)
    {
        stopPlay(); if (! hasData || dev == nullptr || dev->getCurrentAudioDevice() == nullptr) { if (dev == nullptr || dev->getCurrentAudioDevice() == nullptr) say (juce::String::fromUTF8 ("No hay tarjeta de audio para la vista previa")); return; }
        playBuf = std::make_unique<juce::AudioBuffer<float>> (renderNow()); mem = std::make_unique<juce::MemoryAudioSource> (*playBuf, false, false); transport.setSource (mem.get(), 0, nullptr, sr);
        if (! attached) { player.setSource (&transport); dev->addAudioCallback (&player); attached = true; }
        transport.setPosition (juce::jlimit (0.0, fileSeconds(), from)); transport.start(); playing = true;
    }
    void stopPlay() { if (playing) { cursor = transport.getCurrentPosition(); } transport.stop(); transport.setSource (nullptr); mem.reset(); playing = false; if (dev && attached) { dev->removeAudioCallback (&player); player.setSource (nullptr); attached = false; } }
    bool isPlaying() const { return playing; }

    // ---------------------------------------------------------------- dibujo
    void paint (juce::Graphics& g) override
    {
        g.addTransform (juce::AffineTransform::scale (scale)); g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (bgImg, juce::Rectangle<float> (0.f, 0.f, 1774.f, 887.f)); paintToolbar (g); paintGrid (g); paintStrip (g); paintBottom (g);
        if (job != nullptr) { g.setColour (juce::Colour (0xcc2a1840)); g.setFont (theme::fonts::ui (26.f, true)); g.drawText (juce::String::fromUTF8 ("Analizando\xe2\x80\xa6 ") + juce::String (juce::roundToInt (job->prog.load() * 100.f)) + " %", juce::Rectangle<float> (GX0, GY0, GX1 - GX0, GY1 - GY0), juce::Justification::centred); }
        else if (! hasData) { g.setColour (juce::Colour (0xcc2a1840)); g.setFont (theme::fonts::ui (24.f, true)); g.drawText (juce::String::fromUTF8 (msgNoData), juce::Rectangle<float> (GX0, GY0, GX1 - GX0, GY1 - GY0), juce::Justification::centred); }
    }
    void paintBody (juce::Graphics&) override {}
    void bodyDown (const juce::MouseEvent&) override {} void bodyDrag (const juce::MouseEvent&) override {} void bodyUp (const juce::MouseEvent&) override {}
    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto p = e.position / scale; drag = D_None; const bool right = e.mods.isRightButtonDown();
        if (p.y < 118.f && p.x > 560.f && p.x < 1260.f) { drag = D_Window; dragOff = e.getEventRelativeTo (getParentComponent()).getPosition() - getPosition(); return; }
        if (p.y < 100.f && p.x < 500.f) { drag = D_Window; dragOff = e.getEventRelativeTo (getParentComponent()).getPosition() - getPosition(); return; }
        for (int k = 0; k < 7; ++k) if (toolRect (k).contains (p)) { setTool (k); return; }
        if (juce::Rectangle<float> (866.f, 134.f, 72.f, 60.f).contains (p)) { snapOn = ! snapOn; repaint(); return; }
        if (juce::Rectangle<float> (1338.f, 24.f, 94.f, 60.f).contains (p)) { setAuto (true); return; }
        if (juce::Rectangle<float> (1432.f, 24.f, 98.f, 60.f).contains (p)) { setAuto (false); return; }
        if (juce::Rectangle<float> (1548.f, 27.f, 61.f, 54.f).contains (p)) { undo(); return; } if (juce::Rectangle<float> (1615.f, 27.f, 61.f, 54.f).contains (p)) { redo(); return; }
        if (juce::Rectangle<float> (1688.f, 27.f, 60.f, 54.f).contains (p)) { gearMenu(); return; }
        if (juce::Rectangle<float> (966.f, 150.f, 182.f, 46.f).contains (p)) { snapMenu(); return; } if (juce::Rectangle<float> (1198.f, 150.f, 212.f, 46.f).contains (p)) { scaleMenu(); return; } if (juce::Rectangle<float> (1458.f, 150.f, 214.f, 46.f).contains (p)) { viewMenu(); return; }
        if (juce::Rectangle<float> (44.f, 808.f, 74.f, 56.f).contains (p)) { if (playing) stopPlay(); else startPlay (cursor); return; }
        if (juce::Rectangle<float> (122.f, 808.f, 74.f, 56.f).contains (p)) { stopPlay(); cursor = 0.0; return; }
        if (juce::Rectangle<float> (222.f, 808.f, 70.f, 56.f).contains (p)) { apply(); return; }
        if (juce::Rectangle<float> (1678.f, 807.f, 60.f, 56.f).contains (p)) { toggleMax(); return; }
        if (juce::Rectangle<float> (1371.f, 810.f, 52.f, 52.f).contains (p)) { zoomBy (1.5, -1.0); return; } if (juce::Rectangle<float> (1599.f, 810.f, 52.f, 52.f).contains (p)) { zoomBy (1.0 / 1.5, -1.0); return; }
        if (juce::Rectangle<float> (1426.f, 816.f, 170.f, 40.f).contains (p)) { drag = D_Zoom; setZoomFromX (p.x); return; }
        if (juce::Rectangle<float> (326.f, 816.f, 770.f, 40.f).contains (p)) { drag = D_Seek; seekFromX (p.x); return; }
        if (stripRect().contains (p)) { beginStrip (p); return; }
        if (p.y >= 224.f && p.y < GY0 && p.x >= GX0) { drag = D_Seek2; cursor = timeAt (p.x); if (playing) transport.setPosition (cursor); repaint(); return; }
        if (! hasData || ! gridRect().contains (p)) return;
        const int hit = noteHit (p);
        if (right) { if (hit >= 0) { pushUndo(); auto& n = trk.notes[(size_t) hit]; n.tgt = n.det; n.keep = 1.f; n.man = false; repaint(); } return; }
        gStart = p; dragFrame = -1;
        if (tool == tSelect)
        {
            if (hit >= 0)
            {
                if (e.mods.isShiftDown()) selected[(size_t) hit] = ! selected[(size_t) hit]; else if (! selected[(size_t) hit]) { std::fill (selected.begin(), selected.end(), false); selected[(size_t) hit] = true; }
                pushUndo(); orig.clear(); for (auto& n : trk.notes) orig.push_back (n.tgt); drag = D_Move;
            }
            else { if (! e.mods.isShiftDown()) std::fill (selected.begin(), selected.end(), false); drag = D_Marquee; marq = { p, p }; }
        }
        else if (tool == tMarquee) { if (! e.mods.isShiftDown()) std::fill (selected.begin(), selected.end(), false); drag = D_Marquee; marq = { p, p }; }
        else if (tool == tPencil) { pushUndo(); drag = D_Pencil; paintAt (p); }
        else if (tool == tSplit) { if (hit >= 0) doSplit (hit, timeAt (p.x)); }
        else if (tool == tCurve) { if (hit >= 0) { pushUndo(); if (! selected[(size_t) hit]) { std::fill (selected.begin(), selected.end(), false); selected[(size_t) hit] = true; } orig.clear(); for (auto& n : trk.notes) orig.push_back (n.keep); drag = D_Keep; } }
        else if (tool == tCenter) { if (hit >= 0) { pushUndo(); auto& n = trk.notes[(size_t) hit]; n.tgt = injfx::InjTunePro::nearestNote (n.tgt, pitched::maskFor (key, scaleIdx)); n.man = true; repaint(); } }
        else if (tool == tMerge) { if (hit >= 0) doMerge (hit); }
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto p = e.position / scale;
        if (drag == D_Window && getParentComponent()) { auto q = e.getEventRelativeTo (getParentComponent()).getPosition() - dragOff; auto pb = getParentComponent()->getLocalBounds(); setTopLeftPosition (juce::jlimit (-getWidth() + 120, juce::jmax (0, pb.getWidth() - 120), q.x), juce::jlimit (0, juce::jmax (0, pb.getHeight() - 60), q.y)); return; }
        if (drag == D_Zoom) { setZoomFromX (p.x); return; } if (drag == D_Seek) { seekFromX (p.x); return; } if (drag == D_Seek2) { cursor = timeAt (p.x); if (playing) transport.setPosition (cursor); repaint(); return; }
        if (drag == D_StripL || drag == D_StripR || drag == D_StripM) { dragStrip (p); return; }
        if (drag == D_Move)
        {
            const float d = (gStart.y - p.y) / (float) RH; const int mask = pitched::maskFor (key, scaleIdx);
            for (size_t i = 0; i < trk.notes.size(); ++i) if (selected[i])
            {
                float t = orig[i] + d; if (snapOn && ! e.mods.isShiftDown()) t = snapMode == 1 ? injfx::InjTunePro::nearestNote (t, mask) : std::round (t);
                trk.notes[i].tgt = t; trk.notes[i].man = true;
            }
            repaint();
        }
        else if (drag == D_Marquee) { marq.second = p; const auto r = juce::Rectangle<float> (marq.first, marq.second); for (size_t i = 0; i < trk.notes.size(); ++i) if (r.intersects (noteRect (trk.notes[i]))) selected[i] = true; repaint(); }
        else if (drag == D_Pencil) paintAt (p);
        else if (drag == D_Keep) { const float k = juce::jlimit (0.f, 2.f, 1.f + (gStart.y - p.y) / 90.f); for (size_t i = 0; i < trk.notes.size(); ++i) if (selected[i]) { trk.notes[i].keep = juce::jlimit (0.f, 2.f, orig[i] + (k - 1.f)); trk.notes[i].man = true; } repaint(); }
    }
    void mouseUp (const juce::MouseEvent&) override { drag = D_None; repaint(); }
    void mouseDoubleClick (const juce::MouseEvent&) override {}
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const auto p = e.position / scale; if (! gridRect().contains (p) && ! stripRect().contains (p)) return;
        if (e.mods.isCtrlDown() || e.mods.isCommandDown()) zoomBy (w.deltaY > 0 ? 1.0 / 1.25 : 1.25, timeAt (p.x));
        else if (e.mods.isShiftDown()) { const double s = (vB - vA) * .15 * (w.deltaY > 0 ? -1.0 : 1.0); panView (s); }
        else { pLow = juce::jlimit (24, 100, pLow + (w.deltaY > 0 ? 1 : -1)); repaint(); }
    }
    juce::String tipAt (juce::Point<float>) override { return {}; }

private:
    enum Drag { D_None, D_Window, D_Zoom, D_Seek, D_Seek2, D_StripL, D_StripR, D_StripM, D_Move, D_Marquee, D_Pencil, D_Keep };
    struct Snap { std::vector<pitched::Note> notes; std::vector<float> drawn; };
    static constexpr float GX0 = 212.f, GX1 = 1718.f, GY0 = 266.f, GY1 = 692.f; static constexpr double RH = (692.0 - 266.0) / 13.0;
    static constexpr const char* msgNoData = "Elige un clip de audio con una voz (Playlist) y abre el editor";
    juce::Rectangle<float> gridRect() const { return { GX0, GY0, GX1 - GX0, GY1 - GY0 }; }
    juce::Rectangle<float> stripRect() const { return { 60.f, 713.f, 1656.f, 60.f }; }
    juce::Rectangle<float> toolRect (int k) const { static const float a[7] { 102, 183, 282, 376, 471, 562, 653 }, b[7] { 180, 280, 374, 467, 560, 650, 737 }; return { a[k], 134.f, b[k] - a[k], 60.f }; }
    void say (const juce::String& m) { if (onMessage) onMessage (m); }
    double dur() const { return std::max (0.01, fileSeconds()); }
    double minSpan() const { return std::min (0.5, dur()); }
    float xOf (double t) const { return (float) (GX0 + (t - vA) / (vB - vA) * (GX1 - GX0)); }
    double timeAt (float x) const { return vA + (x - GX0) / (GX1 - GX0) * (vB - vA); }
    float yOf (double m) const { return (float) (GY1 - (m - pLow + .5) * RH); }
    float midiAtY (float y) const { return (float) (pLow - .5 + (GY1 - y) / RH); }
    juce::Rectangle<float> noteRect (const pitched::Note& n) const { const float x0 = xOf (trk.tOf (n.f0)), x1 = xOf (trk.tOf (n.f1)), y = yOf (n.tgt); return { x0, y - (float) RH * .6f, std::max (4.f, x1 - x0), (float) RH * 1.2f }; }
    int noteHit (juce::Point<float> p) const { for (int i = (int) trk.notes.size() - 1; i >= 0; --i) if (noteRect (trk.notes[(size_t) i]).contains (p)) return i; return -1; }
    void pushUndo() { undos.push_back ({ trk.notes, drawn }); if (undos.size() > 60) undos.erase (undos.begin()); redos.clear(); }
    float snapped (float m) const { if (! snapOn) return m; return snapMode == 1 ? injfx::InjTunePro::nearestNote (m, pitched::maskFor (key, scaleIdx)) : (snapMode == 0 ? std::round (m) : m); }
    void paintAt (juce::Point<float> p)
    {
        const double f = trk.fOf (timeAt (p.x)); const int fi = (int) std::lround (f); const float m = snapped (midiAtY (p.y));
        if (dragFrame < 0) dragFrame = fi; const int a = std::min (dragFrame, fi), b = std::max (dragFrame, fi);
        for (int i = std::max (0, a); i <= b && i < (int) drawn.size(); ++i) drawn[(size_t) i] = m; dragFrame = fi; repaint();
    }
    bool doSplit (int i, double t)
    {
        if (i < 0 || i >= numNotes()) return false; auto n = trk.notes[(size_t) i]; const int f = (int) std::lround (trk.fOf (t)); if (f - n.f0 < 2 || n.f1 - f < 2) return false;
        pushUndo(); auto a = n, b = n; a.f1 = f; b.f0 = f; trk.notes[(size_t) i] = a; trk.notes.insert (trk.notes.begin() + i + 1, b); selected.assign (trk.notes.size(), false); repaint(); return true;
    }
    bool doMerge (int i)
    {
        if (i < 0 || i + 1 >= numNotes()) return false; auto& a = trk.notes[(size_t) i]; const auto b = trk.notes[(size_t) i + 1]; if (b.f0 - a.f1 > 8) return false;
        pushUndo(); std::vector<float> v; for (int f = a.f0; f < b.f1; ++f) if (trk.midi[(size_t) f] >= 0.f) v.push_back (trk.midi[(size_t) f]); const float d = pitched::medianOf (v);
        a.det = d; a.f1 = b.f1; a.man = a.man || b.man; trk.notes.erase (trk.notes.begin() + i + 1); selected.assign (trk.notes.size(), false); repaint(); return true;
    }
    // ---- vista
    void setView (double a, double b)
    {
        const double span = juce::jlimit (minSpan(), dur(), b - a); a = juce::jlimit (0.0, dur() - span, a); vA = a; vB = a + span; repaint();
    }
    void panView (double d) { setView (vA + d, vB + d); }
    void zoomBy (double factor, double around)
    {
        const double c = around < 0.0 ? .5 * (vA + vB) : around, span = (vB - vA) * factor, fr = (c - vA) / std::max (1e-9, vB - vA); setView (c - fr * span, c - fr * span + span);
    }
    float zoomNorm() const { const double s = std::max (minSpan() + 1e-6, vB - vA); const double d = dur(); return d <= minSpan() + 1e-6 ? 0.f : (float) (std::log (s / d) / std::log (minSpan() / d)); }
    void setZoomFromX (float x) { const double z = juce::jlimit (0.0, 1.0, (x - 1430.0) / 160.0), d = dur(), span = d * std::pow (minSpan() / d, z), c = .5 * (vA + vB); setView (c - span / 2, c + span / 2); }
    void seekFromX (float x) { cursor = juce::jlimit (0.0, dur(), (x - 333.0) / 757.0 * dur()); if (playing) transport.setPosition (cursor); repaint(); }
    float stripX (double t) const { return (float) (77.0 + t / dur() * 1625.0); }
    double stripT (float x) const { return (x - 77.0) / 1625.0 * dur(); }
    void beginStrip (juce::Point<float> p)
    {
        const float a = stripX (vA), b = stripX (vB); sOff = stripT (p.x) - vA;
        if (std::abs (p.x - a) < 14.f) drag = D_StripL; else if (std::abs (p.x - b) < 14.f) drag = D_StripR; else if (p.x > a && p.x < b) drag = D_StripM; else { const double c = stripT (p.x), s = vB - vA; setView (c - s / 2, c + s / 2); drag = D_StripM; sOff = s / 2; }
    }
    void dragStrip (juce::Point<float> p)
    {
        const double t = stripT (p.x);
        if (drag == D_StripL) setView (std::min (t, vB - minSpan()), vB); else if (drag == D_StripR) setView (vA, std::max (t, vA + minSpan())); else setView (t - sOff, t - sOff + (vB - vA));
    }
    // ---- carga y análisis
    void reload()
    {
        job.reset(); hasData = false; trk = {}; drawn.clear(); selected.clear(); undos.clear(); redos.clear(); src.setSize (0, 0); peaksData.clear(); stopPlay(); cursor = 0.0;
        auto* wc = clip(); if (wc == nullptr) return; double r = 0; audioedit::Buf b; if (! audioedit::readFile (wc->getOriginalFile(), b, r, 91.0) || r < 8000.0) { say (juce::String::fromUTF8 ("No se pudo leer el audio")); return; }
        if (b.getNumSamples() > (int) (90.0 * r)) { say (juce::String::fromUTF8 ("El clip dura m\xc3\xa1s de 90 s: corta un tramo m\xc3\xa1s corto para afinarlo")); return; }
        src = std::move (b); sr = r; peaksData = audioedit::peaks (src, 1625); vA = 0.0; vB = dur();
        auto j = std::make_shared<Job>(); j->sr = sr; const int n = src.getNumSamples(); j->mono.resize ((size_t) n);
        for (int i = 0; i < n; ++i) { float s = 0.f; for (int c = 0; c < src.getNumChannels(); ++c) s += src.getSample (c, i); j->mono[(size_t) i] = s / (float) src.getNumChannels(); }
        Job* jp = j.get(); j->th = std::thread ([jp] { jp->tr = pitched::analyse (jp->mono, jp->sr, &jp->prog, &jp->cancel); jp->done.store (true); }); job = j; prog.store (0.f);
    }
    void poll()
    {
        if (job != nullptr && job->done.load())
        {
            trk = std::move (job->tr); job.reset(); hasData = trk.frames() > 0 && ! trk.notes.empty(); drawn.assign ((size_t) trk.frames(), std::nanf ("")); selected.assign (trk.notes.size(), false);
            if (hasData) { std::vector<float> d; for (auto& n : trk.notes) d.push_back (n.det); pLow = juce::jlimit (24, 100, juce::roundToInt (pitched::medianOf (d)) - 6); if (autoMode) autoCorrect(); undos.clear(); }
            else say (juce::String::fromUTF8 ("No se encontr\xc3\xb3 una voz con tono en este clip"));
            repaint();
        }
    }
    void timerCallback() override
    {
        poll(); if (playing) { if (! transport.isPlaying()) { cursor = 0.0; stopPlay(); } else cursor = transport.getCurrentPosition(); }
        if (isVisible() && (playing || job != nullptr)) repaint();
    }
    // ---- menús
    void popup (juce::PopupMenu& m, std::function<void (int)> cb) { juce::Component::SafePointer<PitchEditorPanel> sp (this); m.showMenuAsync (juce::PopupMenu::Options(), [sp, cb] (int r) { if (sp != nullptr && r > 0) { cb (r); sp->repaint(); } }); }
    void snapMenu() { juce::PopupMenu m; const char* n[] { "Notes", "Scale", "Free" }; for (int i = 0; i < 3; ++i) m.addItem (1 + i, n[i], true, snapMode == i); popup (m, [this] (int r) { snapMode = r - 1; }); }
    void viewMenu() { juce::PopupMenu m; const char* n[] { "Pitch", "Pitch + Wave" }; for (int i = 0; i < 2; ++i) m.addItem (1 + i, n[i], true, viewMode == i); popup (m, [this] (int r) { viewMode = r - 1; }); }
    void scaleMenu()
    {
        juce::PopupMenu m, ks; for (int i = 0; i < 12; ++i) ks.addItem (100 + i, kNames[i], true, key == i); m.addSubMenu ("Key", ks);
        for (int i = 0; i < 11; ++i) m.addItem (200 + i, scaleNames[i], true, scaleIdx == i);
        popup (m, [this] (int r) { if (r >= 200) setScale (key, r - 200); else setScale (r - 100, scaleIdx); });
    }
    void gearMenu()
    {
        juce::PopupMenu m; m.addItem (1, juce::String::fromUTF8 ("Aplicar al clip"), hasData); m.addItem (2, juce::String::fromUTF8 ("Conservar formantes"), true, keepFormants); m.addSeparator();
        for (int i = 0; i < 3; ++i) m.addItem (10 + i, juce::String::fromUTF8 ("Transici\xc3\xb3n entre notas: ") + juce::String (kTrans[i], 0) + " ms", true, std::abs (transMs - kTrans[i]) < .5f);
        m.addSeparator(); m.addItem (3, juce::String::fromUTF8 ("Corregir todo a la escala"), hasData); m.addItem (4, juce::String::fromUTF8 ("Quitar todas las correcciones"), hasData); m.addItem (5, juce::String::fromUTF8 ("Volver a analizar")); m.addItem (6, "Cerrar");
        popup (m, [this] (int r)
        {
            if (r == 1) apply(); else if (r == 2) keepFormants = ! keepFormants; else if (r >= 10 && r < 13) transMs = kTrans[r - 10]; else if (r == 3) { for (auto& n : trk.notes) n.man = false; autoCorrect(); }
            else if (r == 4) { pushUndo(); for (auto& n : trk.notes) { n.tgt = n.det; n.keep = 1.f; n.man = false; } std::fill (drawn.begin(), drawn.end(), std::nanf ("")); } else if (r == 5) { auto* c = clip(); cur = nullptr; setClip (c); } else if (r == 6 && onClose) onClose();
        });
    }
    // ---- dibujo de los bloques
    void cellDraw (juce::Graphics& g, const juce::Image& atlas, int cell, float cx, float cy) { g.drawImage (atlas, juce::roundToInt (cx - 52.f), juce::roundToInt (cy - 32.f), 104, 64, cell * 104, 0, 104, 64, false); }
    void paintToolbar (juce::Graphics& g)
    {
        for (int k = 0; k < 7; ++k) { const auto r = toolRect (k); const float cx = r.getCentreX(); if (k == tool) { cellDraw (g, pur, k, cx, 165.f); cellDraw (g, icw, k, cx, 165.f); } else cellDraw (g, icd, k, cx, 165.f); }
        if (snapOn) { cellDraw (g, pur, 7, 902.f, 165.f); cellDraw (g, icw, 7, 902.f, 165.f); } else cellDraw (g, icd, 7, 902.f, 165.f);
        cellDraw (g, pur, autoMode ? 8 : 9, autoMode ? 1385.f : 1481.f, 54.f);
        const auto ink = juce::Colour (0xff1c1a24); g.setFont (theme::fonts::ui (30.f, true));
        g.setColour (autoMode ? juce::Colours::white : ink); g.drawText ("AUTO", juce::Rectangle<float> (1338.f, 28.f, 94.f, 52.f), juce::Justification::centred, false); g.setColour (autoMode ? ink : juce::Colours::white); g.drawText ("EDIT", juce::Rectangle<float> (1432.f, 28.f, 98.f, 52.f), juce::Justification::centred, false);
        g.setFont (theme::fonts::ui (30.f, true)); g.setColour (ink);
        const juce::String sn[] { "Notes", "Scale", "Free" }; g.drawText (sn[snapMode], juce::Rectangle<float> (975.f, 152.f, 130.f, 44.f), juce::Justification::centredLeft, false);
        g.drawText (scaleIdx == 0 ? juce::String ("Chromatic") : juce::String (kNames[key]) + " " + scaleNames[scaleIdx], juce::Rectangle<float> (1208.f, 152.f, 170.f, 44.f), juce::Justification::centredLeft, false);
        g.drawText (viewMode == 0 ? "Pitch" : "Pitch + Wave", juce::Rectangle<float> (1468.f, 152.f, 170.f, 44.f), juce::Justification::centredLeft, false);
    }
    void paintGrid (juce::Graphics& g)
    {
        juce::Path clipP; clipP.addRoundedRectangle (56.f, 226.f, 1662.f, 466.f, 10.f); juce::Graphics::ScopedSaveState ss (g); g.reduceClipRegion (clipP);
        g.setColour (juce::Colour (0xfff6f0fe)); g.fillRect (56.f, 226.f, 1662.f, 466.f);
        const int mask = pitched::maskFor (key, scaleIdx);
        for (int r = 0; r < 13; ++r)
        {
            const int m = pLow + r; const int pc = ((m % 12) + 12) % 12; const bool blk = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; const float y = (float) (GY1 - (r + 1) * RH);
            g.setColour (blk ? juce::Colour (0xffe9def9) : juce::Colour (0xfff2eafd)); g.fillRect (GX0, y, GX1 - GX0, (float) RH);
            if (scaleIdx != 0 && ! (mask & (1 << pc))) { g.setColour (juce::Colour (0x14301060)); g.fillRect (GX0, y, GX1 - GX0, (float) RH); }
            g.setColour (juce::Colour (0x22806aa8)); g.drawHorizontalLine ((int) y, GX0, GX1);
            // teclas
            g.setColour (blk ? juce::Colour (0xff2b2631) : juce::Colour (0xfffbf8ff)); if (blk) g.fillRoundedRectangle (56.f, y + 3.f, 68.f, (float) RH - 6.f, 3.f); else g.fillRect (56.f, y, 68.f, (float) RH);
            g.setColour (juce::Colour (0xffeee6fb)); g.fillRect (124.f, y, 81.f, (float) RH);
            const bool inS = (mask & (1 << pc)) != 0; g.setColour (juce::Colour (0xff1c1a24).withAlpha (inS ? 1.f : .55f)); g.setFont (theme::fonts::ui (24.f, inS)); g.drawText (juce::String (kNames[pc]) + juce::String (m / 12 - 1), juce::Rectangle<float> (124.f, y, 82.f, (float) RH), juce::Justification::centred, false);
        }
        g.setColour (juce::Colour (0xfff6f0fe)); g.fillRect (GX0 - 7.f, 226.f, 1511.f, 38.f); g.fillRect (56.f, 226.f, 156.f, 38.f);
        // tiempo
        const double span = vB - vA, pps = (GX1 - GX0) / span; static const double st[] { .01, .02, .05, .1, .25, .5, 1, 2, 5, 10, 15, 30, 60 }; double step = 60; for (double s : st) if (s * pps >= 100.0) { step = s; break; }
        g.setFont (theme::fonts::ui (24.f, true));
        for (double t = std::ceil (vA / step) * step; t <= vB + 1e-9; t += step)
        {
            const float x = xOf (t); g.setColour (juce::Colour (0x40806aa8)); g.drawVerticalLine ((int) x, GY0, GY1); g.setColour (juce::Colour (0xff1c1a24).withAlpha (.85f));
            g.drawText (step < 1.0 ? juce::String (t, 2) : juce::String (t, 1), juce::Rectangle<float> (x + 6.f, 228.f, 100.f, 30.f), juce::Justification::centredLeft, false);
        }
        g.setColour (juce::Colour (0x30806aa8)); for (double t = std::ceil (vA / (step / 4)) * (step / 4); t <= vB; t += step / 4) g.drawVerticalLine ((int) xOf (t), GY0, GY1);
        { juce::Graphics::ScopedSaveState s2 (g); g.reduceClipRegion (gridRect().toNearestInt());
            if (viewMode == 1 && src.getNumSamples() > 0 && ! peaksData.empty()) { juce::Path w; const int n = (int) peaksData.size(); const float cy = (float) (GY0 + (GY1 - GY0) * .5); bool first = true;
                for (int i = 0; i < n; ++i) { const double t = (i + .5) / n * dur(); const float x = xOf (t); if (x < GX0 - 3.f || x > GX1 + 3.f) continue; const float a = peaksData[(size_t) i].second * 100.f; if (first) { w.startNewSubPath (x, cy - a); first = false; } else w.lineTo (x, cy - a); }
                for (int i = n - 1; i >= 0; --i) { const double t = (i + .5) / n * dur(); const float x = xOf (t); if (x < GX0 - 3.f || x > GX1 + 3.f) continue; w.lineTo (x, cy - peaksData[(size_t) i].first * 100.f); }
                w.closeSubPath(); g.setColour (juce::Colour (0x30a050e0)); g.fillPath (w); }
            if (hasData) paintNotes (g);
        }
        if (cursor >= vA && cursor <= vB) { const float x = xOf (cursor); g.setColour (juce::Colour (0xff8a20d8)); g.fillRect (x - 1.f, GY0 - 4.f, 2.f, GY1 - GY0 + 4.f); juce::Path tri; tri.addTriangle (x - 10.f, GY0 - 16.f, x + 10.f, GY0 - 16.f, x, GY0 + 2.f); g.setColour (juce::Colour (0xffa040f0)); g.fillPath (tri); g.setColour (juce::Colour (0xff6a10b0)); g.strokePath (tri, juce::PathStrokeType (1.5f)); }
    }
    void paintNotes (juce::Graphics& g)
    {
        for (size_t i = 0; i < trk.notes.size(); ++i)
        {
            const auto& n = trk.notes[i]; const auto r = noteRect (n); if (r.getRight() < GX0 || r.getX() > GX1) continue; const bool sel = i < selected.size() && selected[i];
            juce::ColourGradient gr (juce::Colour (sel ? 0xe8e0a8ff : 0xdce8c4fa), r.getX(), r.getY(), juce::Colour (sel ? 0xe8c070f4 : 0xdcd69af2), r.getX(), r.getBottom(), false); g.setGradientFill (gr); g.fillRoundedRectangle (r, 15.f);
            g.setColour (sel ? juce::Colour (0xff7a14c8) : juce::Colour (0xffb064ee)); g.drawRoundedRectangle (r.reduced (1.f), 15.f, sel ? 3.f : 2.f);
            if (std::abs (n.tgt - n.det) > .05f) { juce::Path o; bool f1 = true; for (int f = n.f0; f < n.f1; ++f) { const float m = trk.midi[(size_t) f]; if (m < 0.f) continue; const float x = xOf (trk.tOf (f) + .5 * trk.hop / trk.sr), y = yOf (m); if (f1) { o.startNewSubPath (x, y); f1 = false; } else o.lineTo (x, y); } g.setColour (juce::Colour (0x908060a8)); g.strokePath (o, juce::PathStrokeType (1.2f)); }
            juce::Path c; bool f1 = true; for (int f = n.f0; f < n.f1; ++f) { const float m = trk.midi[(size_t) f]; if (m < 0.f) continue; const float cm = n.tgt + (m - n.det) * n.keep; const float x = xOf (trk.tOf (f) + .5 * trk.hop / trk.sr), y = yOf (cm); if (f1) { c.startNewSubPath (x, y); f1 = false; } else c.lineTo (x, y); }
            g.setColour (juce::Colour (0xff6a1aa8)); g.strokePath (c, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        { juce::Path d; bool run = false; for (int f = 0; f < (int) drawn.size(); ++f) { if (std::isnan (drawn[(size_t) f])) { run = false; continue; } const float x = xOf (trk.tOf (f) + .5 * trk.hop / trk.sr), y = yOf (drawn[(size_t) f]); if (! run) { d.startNewSubPath (x, y); run = true; } else d.lineTo (x, y); }
          g.setColour (juce::Colour (0xffd020a0)); g.strokePath (d, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
        if (drag == D_Marquee) { const auto r = juce::Rectangle<float> (marq.first, marq.second); g.setColour (juce::Colour (0x208a20d8)); g.fillRect (r); g.setColour (juce::Colour (0xff8a20d8)); const float dl[] { 6.f, 4.f }; juce::Path rp; rp.addRectangle (r); juce::Path dp; juce::PathStrokeType (1.5f).createDashedStroke (dp, rp, dl, 2); g.fillPath (dp); }
    }
    void paintStrip (juce::Graphics& g)
    {
        juce::Path cp; cp.addRoundedRectangle (58.f, 714.f, 1660.f, 58.f, 10.f); juce::Graphics::ScopedSaveState ss (g); g.reduceClipRegion (cp); g.setColour (juce::Colour (0xfff4eefd)); g.fillRect (58.f, 714.f, 1660.f, 58.f);
        if (src.getNumSamples() > 0)
        {
            const float cy = 742.f; juce::Path w; const int n = (int) peaksData.size(); for (int i = 0; i < n; ++i) { const float x = 77.f + (float) (i + .5) / n * 1625.f, a = peaksData[(size_t) i].second * 27.f; if (i == 0) w.startNewSubPath (x, cy - a); else w.lineTo (x, cy - a); }
            for (int i = n - 1; i >= 0; --i) w.lineTo (77.f + (float) (i + .5) / n * 1625.f, cy - peaksData[(size_t) i].first * 27.f); w.closeSubPath(); g.setColour (juce::Colour (0xd8c690f4)); g.fillPath (w);
            const float a = stripX (vA), b = stripX (vB); g.setColour (juce::Colour (0x18301060)); g.fillRect (58.f, 714.f, a - 58.f, 58.f); g.fillRect (b, 714.f, 1718.f - b, 58.f);
            for (float x : { a, b }) { g.setColour (juce::Colour (0xff8a20d8)); g.fillRoundedRectangle (x - 2.f, 716.f, 4.f, 54.f, 2.f); g.fillRect (x - 2.f, 716.f, x == a ? 9.f : -9.f, 3.f); g.fillRect (x - 2.f, 767.f, x == a ? 9.f : -9.f, 3.f);
                g.setColour (juce::Colour (0xfffbf8ff)); g.fillEllipse (x - 12.f, cy - 12.f, 24.f, 24.f); g.setColour (juce::Colour (0xffc8b8e8)); g.drawEllipse (x - 12.f, cy - 12.f, 24.f, 24.f, 1.5f); g.setColour (juce::Colour (0xff6a10b0)); g.fillEllipse (x - 4.f, cy - 4.f, 8.f, 8.f); }
        }
    }
    void paintBottom (juce::Graphics& g)
    {
        // posición
        const double d = dur(); const float px = (float) (333.0 + juce::jlimit (0.0, 1.0, cursor / d) * 757.0);
        g.setColour (juce::Colour (0xffd4c4ee)); g.fillRoundedRectangle (333.f, 829.f, 757.f, 12.f, 6.f); g.setColour (juce::Colour (0xffc0aee0)); g.drawRoundedRectangle (333.5f, 829.5f, 756.f, 11.f, 5.5f, 1.f);
        juce::ColourGradient gp (juce::Colour (0xffb050f8), 333.f, 829.f, juce::Colour (0xff7a14c8), 333.f, 841.f, false); g.setGradientFill (gp); g.fillRoundedRectangle (333.f, 829.f, std::max (12.f, px - 333.f), 12.f, 6.f);
        g.setColour (juce::Colours::black.withAlpha (.18f)); g.fillEllipse (px - 15.f, 838.f - 14.f + 3.f, 30.f, 30.f); g.setColour (juce::Colour (0xfffbf8ff)); g.fillEllipse (px - 15.f, 835.f - 15.f, 30.f, 30.f); g.setColour (juce::Colour (0xffc8b8e8)); g.drawEllipse (px - 15.f, 835.f - 15.f, 30.f, 30.f, 1.5f);
        juce::Path ar; ar.addTriangle (px - 5.f, 829.f, px - 5.f, 841.f, px + 6.f, 835.f); g.setColour (juce::Colour (0xff7a14c8)); g.fillPath (ar);
        // tiempo
        auto fmt = [] (double s) { const int m = (int) (s / 60.0); return juce::String (m) + ":" + juce::String (s - m * 60.0, 3).paddedLeft ('0', 6); };
        g.setColour (juce::Colour (0xff1c1a24)); g.setFont (theme::fonts::ui (27.f, true)); g.drawText (fmt (cursor) + " / " + fmt (d), juce::Rectangle<float> (1119.f, 817.f, 204.f, 36.f), juce::Justification::centred, false);
        // zoom
        const float zx = 1430.f + zoomNorm() * 160.f; g.setColour (juce::Colour (0xffd4c4ee)); g.fillRoundedRectangle (1430.f, 832.f, 160.f, 7.f, 3.5f);
        g.setColour (juce::Colour (0xff8a20d8)); g.fillRoundedRectangle (1430.f, 832.f, std::max (7.f, zx - 1430.f), 7.f, 3.5f);
        g.setColour (juce::Colours::black.withAlpha (.15f)); g.fillEllipse (zx - 15.f, 838.f - 14.f, 30.f, 30.f); g.setColour (juce::Colour (0xfffbf8ff)); g.fillEllipse (zx - 15.f, 835.f - 15.f, 30.f, 30.f); g.setColour (juce::Colour (0xffc8b8e8)); g.drawEllipse (zx - 15.f, 835.f - 15.f, 30.f, 30.f, 1.5f);
        if (playing) { g.setColour (juce::Colour (0x40ffffff)); g.fillRoundedRectangle (44.f, 808.f, 74.f, 56.f, 12.f); }
    }
    te::Edit& edit; te::Clip::Ptr cur; juce::AudioFormatManager fm; juce::Image bgImg, icd, icw, pur;
    audioedit::Buf src; double sr = 44100.0; std::vector<std::pair<float, float>> peaksData; pitched::Track trk; std::vector<float> drawn; std::vector<bool> selected; std::vector<float> orig; bool hasData = false;
    std::vector<Snap> undos, redos; int key = 0, scaleIdx = 0, snapMode = 0, viewMode = 0, tool = tSelect, pLow = 55; bool autoMode = false, snapOn = true, keepFormants = true; float transMs = 30.f;
    double vA = 0.0, vB = 1.0, cursor = 0.0, sOff = 0.0; Drag drag = D_None; juce::Point<float> gStart; juce::Point<int> dragOff; std::pair<juce::Point<float>, juce::Point<float>> marq; int dragFrame = -1;
    struct Job { std::vector<float> mono; double sr = 44100.0; pitched::Track tr; std::atomic<bool> done { false }, cancel { false }; std::atomic<float> prog { 0.f }; std::thread th; ~Job() { cancel = true; if (th.joinable()) th.join(); } };
    std::shared_ptr<Job> job; std::atomic<float> prog { 0.f };
    juce::AudioDeviceManager* dev = nullptr; juce::AudioTransportSource transport; juce::AudioSourcePlayer player; std::unique_ptr<juce::AudioBuffer<float>> playBuf; std::unique_ptr<juce::MemoryAudioSource> mem; bool attached = false, playing = false;
    static inline const char* kNames[12] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static inline const char* scaleNames[11] { "Chromatic", "Major", "Minor", "Harm. Minor", "Pent. Major", "Pent. Minor", "Dorian", "Phrygian", "Mixolydian", "Blues", "Whole Tone" };
    static inline const float kTrans[3] { 10.f, 30.f, 60.f };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchEditorPanel)
};
