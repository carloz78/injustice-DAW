#pragma once
#include "FloatingPanel.h"
#include "AudioEdit.h"
#include "Theme.h"
#include <tracktion_engine/tracktion_engine.h>
#include <functional>

// Editor de audio (estilo FL Studio): muestra la onda del clip de audio elegido y, debajo, su volumen, fundidos, tono, estirado de tiempo
// y los efectos precalculados (quitar DC, normalizar, invertir, invertir polaridad, intercambiar estéreo, recortar...).
// Los cambios de tono, velocidad, volumen y fundidos son del clip (se pueden deshacer). Los efectos precalculados crean un archivo nuevo en
// "Editados" y el clip pasa a usarlo, así el original nunca se toca.
class AudioEditorPanel : public FloatingPanel, private juce::Timer
{
public:
    std::function<te::WaveAudioClip*()> getClip;   // clip de audio elegido en la Playlist (si hay uno solo)
    std::function<void (const juce::String&)> onMessage;
    std::function<void()> onChanged;

    explicit AudioEditorPanel (te::Edit& e) : edit (e)
    {
        title = juce::String::fromUTF8 ("Editor de audio"); icon = icons::Playlist; setDesign (640, 646); startTimerHz (8);
    }
    ~AudioEditorPanel() override { stopTimer(); }

    // ---- estado público (también para las pruebas) ----
    te::WaveAudioClip* clip() const { return dynamic_cast<te::WaveAudioClip*> (cur.get()); }
    void setClip (te::WaveAudioClip* c) { if (c == clip()) return; cur = c; selA = selB = -1.0; reload(); repaint(); }
    bool hasSelection() const { return selA >= 0 && selB - selA > 0.0005; }
    void setSelection (double a, double b) { selA = juce::jlimit (0.0, 1.0, std::min (a, b)); selB = juce::jlimit (0.0, 1.0, std::max (a, b)); repaint(); }
    void setView (double a, double b) { a = juce::jlimit (0.0, 1.0, a); b = juce::jlimit (0.0, 1.0, b); if (b - a < minSpan()) b = std::min (1.0, a + minSpan()); vA = a; vB = b; repaint(); }
    double viewStart() const { return vA; }
    double viewEnd() const { return vB; }
    double fileSeconds() const { return sr > 0 ? (double) total / sr : 0.0; }
    float filePeakDb() const { return pkDb; }

    static juce::StringArray effectIds() { return { "dc", "norm", "rev", "pol", "swap", "fadein", "fadeout", "trim", "silence", "crop", "loopx", "declick" }; }
    static juce::String effectName (const juce::String& id)
    {
        if (id == "dc") return juce::String::fromUTF8 ("Quitar DC"); if (id == "norm") return "Normalizar"; if (id == "rev") return "Invertir"; if (id == "pol") return juce::String::fromUTF8 ("Invertir polaridad");
        if (id == "swap") return juce::String::fromUTF8 ("Intercambiar est\xc3\xa9reo"); if (id == "fadein") return "Fundido de entrada"; if (id == "fadeout") return "Fundido de salida"; if (id == "trim") return "Recortar silencios";
        if (id == "silence") return juce::String::fromUTF8 ("Silenciar selecci\xc3\xb3n"); if (id == "crop") return juce::String::fromUTF8 ("Quedarse con la selecci\xc3\xb3n"); if (id == "loopx") return "Bucle sin clic"; if (id == "declick") return "Quitar clics"; return id;
    }

    // Aplica un efecto. Devuelve true si cambió algo. Los que cambian el archivo crean uno nuevo y lo ponen en el clip.
    bool run (const juce::String& id)
    {
        auto* wc = clip(); if (wc == nullptr) { say (juce::String::fromUTF8 ("Elige primero un clip de audio en la Playlist")); return false; }
        const int sa = hasSelection() ? (int) std::lround (selA * total) : 0, sb = hasSelection() ? (int) std::lround (selB * total) : 0;
        if (id == "norm")   // el volumen del clip sube o baja; el archivo no cambia
        {
            audioedit::Buf b; double r = 0; if (! audioedit::readFile (wc->getOriginalFile(), b, r)) { say (juce::String::fromUTF8 ("No se pudo leer el audio")); return false; }
            const float pk = audioedit::peak (b); if (pk < 1e-5f) { say (juce::String::fromUTF8 ("El clip est\xc3\xa1 en silencio")); return false; }
            edit.getUndoManager().beginNewTransaction (juce::String::fromUTF8 ("Normalizar")); wc->setGainDB (juce::jlimit (-24.f, 24.f, -juce::Decibels::gainToDecibels (pk) - 0.3f)); finish (juce::String::fromUTF8 ("Normalizado: ") + juce::String (wc->getGainDB(), 1) + " dB"); return true;
        }
        audioedit::Buf b; double rate = 0; if (! audioedit::readFile (wc->getOriginalFile(), b, rate)) { say (juce::String::fromUTF8 ("No se pudo leer el audio")); return false; }
        const int n = b.getNumSamples(); bool lenChanged = false; juce::String msg = effectName (id);
        if (id == "dc") audioedit::removeDC (b, sa, sb);
        else if (id == "rev") audioedit::reverse (b, sa, sb);
        else if (id == "pol") audioedit::invertPolarity (b, sa, sb);
        else if (id == "swap") { if (! audioedit::swapChannels (b, sa, sb)) { say (juce::String::fromUTF8 ("Este audio es mono: no hay canales que intercambiar")); return false; } }
        else if (id == "fadein") audioedit::fade (b, true, sa, hasSelection() ? sb : std::min (n, (int) std::lround (rate * 0.05)));
        else if (id == "fadeout") audioedit::fade (b, false, hasSelection() ? sa : std::max (0, n - (int) std::lround (rate * 0.05)), sb);
        else if (id == "declick") audioedit::declick (b, rate, sa, sb, (double) declickMs);
        else if (id == "silence") { if (! hasSelection()) { say (juce::String::fromUTF8 ("Marca primero un tramo en la onda")); return false; } audioedit::silence (b, sa, sb); }
        else if (id == "crop") { if (! hasSelection()) { say (juce::String::fromUTF8 ("Marca primero un tramo en la onda")); return false; } b = audioedit::crop (b, sa, sb); lenChanged = true; }
        else if (id == "trim") { const auto r = audioedit::findAudible (b); if (r.size() < 1 || (r.a == 0 && r.b == n)) { say (juce::String::fromUTF8 ("No hay silencios que recortar")); return false; } b = audioedit::crop (b, r.a, r.b); lenChanged = true; }
        else if (id == "loopx") { const int x = std::min (n / 4, (int) std::lround (rate * 0.02)); if (x < 2) { say (juce::String::fromUTF8 ("El audio es demasiado corto")); return false; } b = audioedit::loopCrossfade (b, x); lenChanged = true; }
        else return false;
        const auto out = audioedit::editedDir().getChildFile (wc->getOriginalFile().getFileNameWithoutExtension().upToFirstOccurrenceOf ("__", false, false) + "__" + id + "_" + juce::String (juce::Time::currentTimeMillis()) + ".wav");
        if (! audioedit::writeWav (out, b, rate)) { say (juce::String::fromUTF8 ("No se pudo guardar el audio editado")); return false; }
        edit.getUndoManager().beginNewTransaction (effectName (id));
        wc->getSourceFileReference().setToDirectFileReference (out, true);
        if (lenChanged) { wc->setOffset (te::TimeDuration()); wc->setLength (te::TimeDuration::fromSeconds ((double) b.getNumSamples() / rate / std::max (0.01, wc->getSpeedRatio())), true); selA = selB = -1.0; }
        lastId = id; lastMs = juce::Time::getMillisecondCounter(); finish (msg); return true;
    }

    // controles del clip (en unidades de uso)
    float volDb() const { auto* w = clip(); return w ? w->getGainDB() : 0.f; }
    void setVolDb (float v) { if (auto* w = clip()) { edit.getUndoManager().beginNewTransaction ("Volumen del clip"); w->setGainDB (juce::jlimit (-24.f, 24.f, v)); finish ({}); } }
    float fadeIn() const { auto* w = clip(); return w ? (float) w->getFadeIn().inSeconds() : 0.f; }
    float fadeOut() const { auto* w = clip(); return w ? (float) w->getFadeOut().inSeconds() : 0.f; }
    void setFade (bool in, float sec)
    {
        auto* w = clip(); if (w == nullptr) return; const auto d = te::TimeDuration::fromSeconds (juce::jlimit (0.0, w->getPosition().getLength().inSeconds() * 0.5, (double) sec));
        edit.getUndoManager().beginNewTransaction ("Fundido del clip"); if (in) w->setFadeIn (d); else w->setFadeOut (d); finish ({});
    }
    float pitch() const { auto* w = clip(); return w ? w->getPitchChange() : 0.f; }
    void setPitch (float st)
    {
        auto* w = clip(); if (w == nullptr) return; edit.getUndoManager().beginNewTransaction ("Tono del clip");
        if (w->getTimeStretchMode() == te::TimeStretcher::disabled) w->setTimeStretchMode (te::TimeStretcher::rubberbandMelodic); w->setPitchChange (juce::jlimit (-24.f, 24.f, st)); finish ({});
    }
    double speed() const { auto* w = clip(); return w ? w->getSpeedRatio() : 1.0; }
    void setSpeed (double ratio)
    {
        auto* w = clip(); if (w == nullptr) return; ratio = juce::jlimit (0.25, 4.0, ratio); const double old = w->getSpeedRatio(); if (std::abs (old - ratio) < 1e-6) return;
        edit.getUndoManager().beginNewTransaction ("Velocidad del clip"); if (w->getTimeStretchMode() == te::TimeStretcher::disabled) w->setTimeStretchMode (te::TimeStretcher::rubberbandMelodic);
        const double newLen = w->getPosition().getLength().inSeconds() * old / ratio; w->setSpeedRatio (ratio); w->setLength (te::TimeDuration::fromSeconds (newLen), true); finish ({});
    }
    int stretchMode() const { auto* w = clip(); if (w == nullptr) return 0; const auto m = w->getTimeStretchMode(); return m == te::TimeStretcher::disabled ? 0 : m == te::TimeStretcher::rubberbandPercussive ? 2 : 1; }
    void setStretchMode (int m)
    {
        auto* w = clip(); if (w == nullptr) return; edit.getUndoManager().beginNewTransaction (juce::String::fromUTF8 ("Modo de estirado"));
        w->setTimeStretchMode (m == 0 ? te::TimeStretcher::disabled : m == 2 ? te::TimeStretcher::rubberbandPercussive : te::TimeStretcher::rubberbandMelodic); finish ({});
    }
    static juce::String stretchModeName (int m) { return m == 0 ? juce::String::fromUTF8 ("Sin estirar") : m == 2 ? juce::String ("Percusivo") : juce::String::fromUTF8 ("Mel\xc3\xb3" "dico"); }

    // ---------------------------------------------------------------- dibujo (estilo FL Studio: gris azulado, tarjetas, perillas lisas)
    void paintBody (juce::Graphics& g) override
    {
        using namespace theme; using juce::Colour; auto* w = clip(); const bool on = w != nullptr; const float W = (float) designW, H = (float) designH;
        auto frame = juce::Rectangle<float> (0, 0, W, H).reduced (1);
        { juce::ColourGradient bg (bgBar2.brighter (.06f), 0, 0, bgBar2, 0, H, false); g.setGradientFill (bg); g.fillRoundedRectangle (frame, 3.f);
          juce::Path tb; tb.addRoundedRectangle (frame.getX(), frame.getY(), frame.getWidth(), (float) kHead, 3.f, 3.f, true, true, false, false); juce::ColourGradient hg (bgBar.brighter (.12f), 0, 0, bgBar, 0, (float) kHead, false); g.setGradientFill (hg); g.fillPath (tb);
          g.setColour (seam); g.fillRect (frame.getX(), (float) kHead, frame.getWidth(), 1.f); g.setColour (barLine.withAlpha (.8f)); g.drawRoundedRectangle (frame.reduced (.5f), 3.f, 1.f); }
        // barra de título: pestaña con el icono de onda, nombre del clip y los botones de la ventana
        { auto tab = juce::Rectangle<float> (12.f, 8.f, 150.f, 32.f); g.setColour (panel); g.fillRoundedRectangle (tab, 6.f); g.setColour (accent); g.fillRect (tab.getX() + 6.f, tab.getBottom() - 2.f, tab.getWidth() - 12.f, 2.f);
          const float bx[] { 0, 4, 8, 12 }, bh[] { 4, 10, 14, 8 }; for (int i = 0; i < 4; ++i) g.fillRect (tab.getX() + 14.f + bx[i], tab.getCentreY() - bh[i] * .5f, 2.f, bh[i]);
          g.setColour (text); g.setFont (theme::fonts::ui (13.f, true)); g.drawText (juce::String::fromUTF8 ("Editor de audio"), tab.withTrimmedLeft (38.f).toNearestInt(), juce::Justification::centredLeft);
          g.setColour (textDim); g.setFont (theme::fonts::ui (13.f)); g.drawText (on ? w->getName() : juce::String::fromUTF8 ("Elige un clip de audio en la Playlist"), 176, 0, 300, kHead, juce::Justification::centredLeft, true);
          const auto mx = maxR(), cx = closeR(); g.setColour (textDim.brighter (.25f)); g.drawRect (mx.getCentreX() - 6.f, mx.getCentreY() - 6.f, 12.f, 12.f, 1.3f);
          g.drawLine (cx.getCentreX() - 6, cx.getCentreY() - 6, cx.getCentreX() + 6, cx.getCentreY() + 6, 1.4f); g.drawLine (cx.getCentreX() - 6, cx.getCentreY() + 6, cx.getCentreX() + 6, cx.getCentreY() - 6, 1.4f); }
        // tarjetas
        card (g, cardL()); card (g, cardR());
        // ---- izquierda
        section (g, "Archivo", 30.f, 70.f, 264.f);
        { auto fr = juce::Rectangle<float> (30.f, 92.f, 264.f, 30.f); inset (g, fr);
          g.setColour (accent); const float bx[] { 0, 3, 6, 9, 12 }, bh[] { 4, 9, 13, 7, 4 }; for (int i = 0; i < 5; ++i) g.fillRect (fr.getX() + 11.f + bx[i], fr.getCentreY() - bh[i] * .5f, 1.6f, bh[i]);
          g.setColour (on ? text : textFaint); g.setFont (theme::fonts::ui (12.5f)); g.drawText (on ? w->getOriginalFile().getFileName() : juce::String ("-"), fr.toNearestInt().withTrimmedLeft (34).withTrimmedRight (28), juce::Justification::centredLeft, true);
          g.setColour (accentHi); juce::Path fo; fo.addRoundedRectangle (fr.getRight() - 24.f, fr.getCentreY() - 5.f, 14.f, 10.f, 1.5f); g.fillPath (fo); }
        if (on)
        {
            float x = 30.f; auto chip = [&] (const juce::String& val, const juce::String& unit) { g.setFont (theme::fonts::digits (11.f)); const float cw = 6.1f; const float wv2 = 14.f + (float) (val.length() + unit.length()) * cw + (unit.isEmpty() ? 0.f : 5.f);
                g.setColour (boxFill); g.fillRoundedRectangle (x, 130.f, wv2, 22.f, 4.f); g.setColour (text); g.drawText (val, juce::Rectangle<float> (x + 7.f, 130.f, wv2, 22.f).toNearestInt(), juce::Justification::centredLeft); 
                if (unit.isNotEmpty()) { g.setColour (textDim); g.drawText (unit, juce::Rectangle<float> (x + 7.f + (float) val.length() * cw + 5.f, 130.f, wv2, 22.f).toNearestInt(), juce::Justification::centredLeft); } x += wv2 + 6.f; };
            chip (juce::String (fileSeconds(), 2), "s"); chip (juce::String ((int) std::lround (sr)), "Hz"); chip (chans > 1 ? juce::String::fromUTF8 ("Est\xc3\xa9reo") : juce::String ("Mono"), {}); chip (juce::String (pkDb, 1), "dB");
        }
        section (g, juce::String::fromUTF8 ("Tramo seleccionado"), 30.f, 170.f, 264.f);
        section (g, "Quitar clics", 30.f, 272.f, 264.f);
        { auto d = declickR(); inset (g, d); g.setColour (on ? text : textFaint); g.setFont (theme::fonts::ui (12.5f)); g.drawText (juce::String::fromUTF8 ("Fundido de ") + juce::String (declickMs) + " ms", d.toNearestInt().withTrimmedLeft (12), juce::Justification::centredLeft); arrow (g, d, on, true); }
        // ---- derecha
        section (g, "Estirar tiempo", 330.f, 70.f, 280.f);
        section (g, "Efectos precalculados", 330.f, 194.f, 280.f);
        for (auto& k : knobs()) drawKnob (g, k, on);
        { auto r = modeR(); inset (g, r); g.setColour (on ? text : textFaint); g.setFont (theme::fonts::ui (12.f)); g.drawText (on ? stretchModeName (stretchMode()) : juce::String ("-"), r.toNearestInt().withTrimmedLeft (9), juce::Justification::centredLeft); arrow (g, r, on, false); }
        for (auto& b : buttons()) drawAction (g, b, on);
        // ---- onda
        drawWave (g, on);
    }

    void bodyDown (const juce::MouseEvent& e) override
    {
        const auto p = e.position; if (clip() == nullptr) return;
        if (zoomOutR().contains (p)) { zoomAt (centerFrac(), 2.0); repaint(); return; }
        if (zoomInR().contains (p)) { zoomAt (centerFrac(), .5); repaint(); return; }
        if (zoomAllR().contains (p)) { vA = 0.0; vB = 1.0; repaint(); return; }
        if (miniR().contains (p)) { dragMini = true; panTo ((double) ((p.x - miniR().getX()) / miniR().getWidth())); return; }
        if (waveR().contains (p) || rulerR().contains (p)) { dragSel = true; selA = selB = xToFrac (p.x); anchor = selA; repaint(); return; }
        const auto ks = knobs(); for (size_t i = 0; i < ks.size(); ++i) if (ks[i].hit.contains (p)) { dragK = (int) i; dragY = p.y; dragVal = ks[i].get(); return; }
        if (modeR().contains (p)) { modeMenu(); return; }
        if (declickR().contains (p)) { declickMenu(); return; }
        for (auto& b : buttons()) if (b.r.contains (p)) { run (b.id); return; }
    }
    void bodyDrag (const juce::MouseEvent& e) override
    {
        if (dragMini) { panTo ((double) ((e.position.x - miniR().getX()) / miniR().getWidth())); return; }
        if (dragSel) { selB = xToFrac (e.position.x); selA = std::min (anchor, selB); selB = std::max (anchor, selB); repaint(); return; }
        if (dragK >= 0) { const auto ks = knobs(); const auto& k = ks[(size_t) dragK]; const double v = dragVal + (dragY - e.position.y) / 160.0 * (k.hi - k.lo) * (e.mods.isShiftDown() ? .1 : 1.0); k.set (juce::jlimit (k.lo, k.hi, v)); repaint(); }
    }
    void bodyUp (const juce::MouseEvent&) override { if (dragSel && ! hasSelection()) selA = selB = -1.0; dragSel = false; dragMini = false; dragK = -1; repaint(); }
    void bodyMove (const juce::MouseEvent& e) override { hot = e.position; repaint(); }
    void bodyDouble (const juce::MouseEvent& e) override
    {
        if (waveR().contains (e.position)) { selA = selB = -1.0; repaint(); return; }
        if (clip() == nullptr) return; for (auto& k : knobs()) if (k.hit.contains (e.position)) { k.set (k.def); repaint(); return; }
    }
    void bodyWheel (const juce::MouseEvent& e, const juce::MouseWheelDetails& wd) override
    {
        if (clip() == nullptr) return;
        if (waveR().contains (e.position) || rulerR().contains (e.position)) { zoomAt (xToFrac (e.position.x), wd.deltaY > 0 ? .8 : 1.25); repaint(); return; }
        for (auto& k : knobs()) if (k.hit.contains (e.position)) { const double st = (k.hi - k.lo) / 100.0 * (e.mods.isShiftDown() ? .2 : 1.0); k.set (juce::jlimit (k.lo, k.hi, k.get() + (wd.deltaY > 0 ? st : -st))); repaint(); return; }
    }
    juce::String tipAt (juce::Point<float> p) override
    {
        if (waveR().contains (p) || rulerR().contains (p)) return juce::String::fromUTF8 ("Arrastra para marcar un tramo (los efectos se aplican a ese tramo). Rueda = zoom. Doble clic quita la selecci\xc3\xb3n");
        if (miniR().contains (p)) return juce::String::fromUTF8 ("Toda la onda. Arrastra para moverte cuando hay zoom");
        if (zoomOutR().contains (p)) return "Alejar"; if (zoomInR().contains (p)) return "Acercar"; if (zoomAllR().contains (p)) return juce::String::fromUTF8 ("Ver toda la onda");
        for (auto& k : knobs()) if (k.hit.contains (p)) return juce::String::fromUTF8 ("Arrastra arriba o abajo (May\xc3\xbas = fino), o usa la rueda. Doble clic = valor original");
        if (declickR().contains (p)) return juce::String::fromUTF8 ("Quitar clics: fundidos cortos en los extremos (elige cu\xc3\xa1nto dura)");
        for (auto& b : buttons()) if (b.r.contains (p)) return b.id == "fadein" ? juce::String::fromUTF8 ("Fundido de entrada en el tramo (o en los primeros 50 ms)") : b.id == "fadeout" ? juce::String::fromUTF8 ("Fundido de salida en el tramo (o en los \xc3\xbaltimos 50 ms)") : effectName (b.id);
        return {};
    }

private:
    struct Knob { juce::Point<float> c; juce::Rectangle<float> hit; juce::String name, unit; double lo, hi, def; std::function<double()> get; std::function<void (double)> set; int dec = 1; juce::Colour col; };
    struct Btn { juce::Rectangle<float> r; juce::String id, label; bool led = false; };
    struct Col { float lo = 0.f, hi = 0.f, rms = 0.f; };
    te::Edit& edit; te::Clip::Ptr cur; double sr = 0, selA = -1, selB = -1, anchor = 0, dragVal = 0, vA = 0.0, vB = 1.0, laneA = -1, laneB = -1; int total = 0, chans = 0, declickMs = 2, laneCols = -1; float pkDb = -120.f, dragY = 0.f; int dragK = -1; bool dragSel = false, dragMini = false;
    juce::Point<float> hot { -1.f, -1.f };
    std::vector<Col> lane, ov; audioedit::Buf cache; juce::File lastFile; juce::String lastId; juce::uint32 lastMs = 0;

    // ---- geometría (coordenadas de diseño)
    juce::Rectangle<float> cardL() const { return { 16.f, 58.f, 292.f, 330.f }; }
    juce::Rectangle<float> cardR() const { return { 316.f, 58.f, 308.f, 330.f }; }
    juce::Rectangle<float> waveCard() const { return { 16.f, 398.f, 608.f, 232.f }; }
    juce::Rectangle<float> rulerR() const { return { 16.f, 432.f, 608.f, 20.f }; }
    juce::Rectangle<float> waveR() const { return { 16.f, 452.f, 608.f, 138.f }; }
    juce::Rectangle<float> miniR() const { return { 16.f, 592.f, 608.f, 38.f }; }
    juce::Rectangle<float> zoomOutR() const { return { 28.f, 403.f, 24.f, 24.f }; }
    juce::Rectangle<float> zoomInR() const { return { 56.f, 403.f, 24.f, 24.f }; }
    juce::Rectangle<float> zoomAllR() const { return { 84.f, 403.f, 42.f, 24.f }; }
    juce::Rectangle<float> modeR() const { return { 526.f, 108.f, 84.f, 28.f }; }
    juce::Rectangle<float> declickR() const { return { 30.f, 294.f, 264.f, 30.f }; }
    std::vector<Knob> knobs()
    {
        const juce::Colour blue = theme::accentHi, green = theme::accent;
        std::vector<Knob> v; auto mk = [&] (float cx, float cy, const juce::String& nm, const juce::String& un, double lo, double hi, double def, std::function<double()> g, std::function<void (double)> s, int dec, juce::Colour col)
        { Knob k; k.c = { cx, cy }; k.hit = { cx - 31.f, cy - 29.f, 62.f, 80.f }; k.name = nm; k.unit = un; k.lo = lo; k.hi = hi; k.def = def; k.get = std::move (g); k.set = std::move (s); k.dec = dec; k.col = col; return k; };
        v.push_back (mk (361.f, 122.f, "Tono", "st", -24, 24, 0, [this] { return (double) pitch(); }, [this] (double x) { setPitch ((float) std::round (x * 10.0) / 10.f); }, 1, blue));
        v.push_back (mk (423.f, 122.f, "Velocidad", "x", 0.25, 4.0, 1.0, [this] { return speed(); }, [this] (double x) { setSpeed (std::round (x * 100.0) / 100.0); }, 2, blue));
        v.push_back (mk (485.f, 122.f, "Tiempo", "s", 0.05, 60.0, std::max (0.05, fileSeconds()), [this] { return fileSeconds() / std::max (0.01, speed()); }, [this] (double x) { if (fileSeconds() > 0) setSpeed (std::round (fileSeconds() / std::max (0.05, x) * 100.0) / 100.0); }, 2, blue));
        v.push_back (mk (361.f, 317.f, "Volumen", "dB", -24, 24, 0, [this] { return (double) volDb(); }, [this] (double x) { setVolDb ((float) x); }, 1, green));
        v.push_back (mk (423.f, 317.f, "Entrada", "s", 0, 4, 0, [this] { return (double) fadeIn(); }, [this] (double x) { setFade (true, (float) x); }, 2, blue));
        v.push_back (mk (485.f, 317.f, "Salida", "s", 0, 4, 0, [this] { return (double) fadeOut(); }, [this] (double x) { setFade (false, (float) x); }, 2, blue));
        return v;
    }
    std::vector<Btn> buttons()
    {
        std::vector<Btn> v;
        const juce::String fx[] { "dc", "norm", "rev", "pol", "trim", "swap" };
        const juce::String fl[] { juce::String::fromUTF8 ("Quitar nivel DC"), "Normalizar", "Invertir", juce::String::fromUTF8 ("Invertir polaridad"), "Quitar silencios", "Intercambiar L/R" };
        for (int i = 0; i < 6; ++i) { Btn b; b.r = { 330.f + (float) (i / 3) * 144.f, 216.f + (float) (i % 3) * 22.f, 138.f, 22.f }; b.id = fx[i]; b.label = fl[i]; b.led = true; v.push_back (b); }
        const juce::String ids[] { "silence", "crop", "fadein", "fadeout", "loopx" };
        const juce::String lab[] { "Silenciar tramo", "Solo el tramo", "Fundir entrada", "Fundir salida", "Bucle sin clic" };
        const juce::Rectangle<float> rr[] { { 30.f, 192.f, 128.f, 30.f }, { 166.f, 192.f, 128.f, 30.f }, { 30.f, 228.f, 128.f, 30.f }, { 166.f, 228.f, 128.f, 30.f }, { 30.f, 340.f, 264.f, 30.f } };
        for (int i = 0; i < 5; ++i) { Btn b; b.r = rr[i]; b.id = ids[i]; b.label = lab[i]; v.push_back (b); }
        return v;
    }
    double xToFrac (float x) const { auto wv = waveR(); return vA + juce::jlimit (0.0, 1.0, (double) ((x - wv.getX()) / wv.getWidth())) * (vB - vA); }
    double centerFrac() const { return (vA + vB) * .5; }
    double minSpan() const { return total > 0 ? std::max (64.0 / (double) total, 1e-5) : 1.0; }
    void zoomAt (double focus, double factor)
    {
        const double span = vB - vA, ns = juce::jlimit (minSpan(), 1.0, span * factor); const double f = span > 0 ? (focus - vA) / span : .5;
        vA = focus - f * ns; vB = vA + ns; if (vA < 0.0) { vB -= vA; vA = 0.0; } if (vB > 1.0) { vA -= vB - 1.0; vB = 1.0; } vA = std::max (0.0, vA);
    }
    void panTo (double centre) { const double span = vB - vA; vA = juce::jlimit (0.0, 1.0 - span, centre - span * .5); vB = vA + span; repaint(); }

    // ---- piezas de dibujo
    void card (juce::Graphics& g, juce::Rectangle<float> r)
    {
        using juce::Colour; using namespace theme; juce::ColourGradient cg (panel.brighter (.05f), 0, r.getY(), panel, 0, r.getBottom(), false); g.setGradientFill (cg); g.fillRoundedRectangle (r, 8.f);
        g.setColour (boxLine); g.drawRoundedRectangle (r.reduced (.5f), 8.f, 1.f); g.setColour (juce::Colours::white.withAlpha (.05f)); g.fillRect (r.getX() + 8.f, r.getY() + 1.f, r.getWidth() - 16.f, 1.f);
    }
    void section (juce::Graphics& g, const juce::String& t, float x, float y, float w)
    {
        using juce::Colour; using namespace theme; g.setColour (text); g.setFont (theme::fonts::ui (12.f, true)); g.drawText (t, (int) x, (int) y, (int) w, 16, juce::Justification::centredLeft);
        const float tw = juce::GlyphArrangement::getStringWidth (theme::fonts::ui (12.f, true), t) + 8.f; juce::ColourGradient lg (barLine, x + tw, y + 8.f, barLine.withAlpha (0.f), x + w, y + 8.f, false); g.setGradientFill (lg); g.fillRect (x + tw, y + 8.f, w - tw, 1.f);
    }
    void inset (juce::Graphics& g, juce::Rectangle<float> r) { using juce::Colour; using namespace theme; g.setColour (boxFill); g.fillRoundedRectangle (r, 5.f); g.setColour (boxFill); g.drawRoundedRectangle (r.reduced (.5f), 5.f, 1.f); g.setColour (juce::Colours::black.withAlpha (.25f)); g.fillRect (r.getX() + 4.f, r.getY() + 1.f, r.getWidth() - 8.f, 1.f); }
    void arrow (juce::Graphics& g, juce::Rectangle<float> r, bool on, bool down)
    {
        using namespace theme; const float x = r.getRight() - 14.f, y = r.getCentreY(); juce::Path t; if (down) t.addTriangle (x - 4.f, y - 2.f, x + 4.f, y - 2.f, x, y + 3.f); else t.addTriangle (x - 2.f, y - 4.f, x - 2.f, y + 4.f, x + 3.f, y);
        g.setColour (on ? iconIdle : textFaint); g.fillPath (t);
    }
    void drawAction (juce::Graphics& g, const Btn& b, bool on)
    {
        using juce::Colour; using namespace theme; const bool needSel = (b.id == "silence" || b.id == "crop"); const bool live = on && (! needSel || hasSelection()); const bool lit = b.id == lastId && juce::Time::getMillisecondCounter() - lastMs < 900; const bool over = live && b.r.contains (hot);
        auto r = b.r; const Colour tx = live ? iconIdle : textFaint;
        if (b.led)
        {
            const float cx = r.getX() + 7.f, cy = r.getCentreY(); g.setColour (boxFill); g.fillEllipse (cx - 7.f, cy - 7.f, 14.f, 14.f);
            if (lit) { g.setColour (accent.withAlpha (.35f)); g.fillEllipse (cx - 10.f, cy - 10.f, 20.f, 20.f); juce::ColourGradient lg (accentHi.brighter (.4f), cx - 2.f, cy - 3.f, accentDim, cx + 6.f, cy + 6.f, true); lg.addColour (.45, accent); g.setGradientFill (lg); g.fillEllipse (cx - 6.f, cy - 6.f, 12.f, 12.f); }
            else { g.setColour (panelHi); g.fillEllipse (cx - 5.5f, cy - 5.5f, 11.f, 11.f); }
            g.setColour (lit ? juce::Colours::white : over ? text.brighter (.2f) : tx); g.setFont (theme::fonts::ui (12.f)); g.drawText (b.label, r.toNearestInt().withTrimmedLeft (22), juce::Justification::centredLeft, true); return;
        }
        juce::ColourGradient cg (over ? panelHi.brighter (.12f) : panelHi, 0, r.getY(), over ? panel.brighter (.12f) : panel, 0, r.getBottom(), false);
        g.setGradientFill (lit ? juce::ColourGradient (accentDim, 0, r.getY(), accentDim.darker (.3f), 0, r.getBottom(), false) : cg); g.fillRoundedRectangle (r, 5.f);
        g.setColour (lit ? accentHi : seam); g.drawRoundedRectangle (r.reduced (.5f), 5.f, 1.f); g.setColour (juce::Colours::white.withAlpha (.06f)); g.fillRect (r.getX() + 5.f, r.getY() + 1.f, r.getWidth() - 10.f, 1.f);
        // icono pequeño a la izquierda del texto
        const float ix = r.getX() + 14.f, iy = r.getCentreY() - 6.f; g.setColour (tx.withAlpha (.9f)); juce::Path ic;
        if (b.id == "fadein") { ic.addTriangle (ix, iy + 12.f, ix + 12.f, iy, ix + 12.f, iy + 12.f); g.fillPath (ic); }
        else if (b.id == "fadeout") { ic.addTriangle (ix, iy, ix + 12.f, iy + 12.f, ix, iy + 12.f); g.fillPath (ic); }
        else if (b.id == "silence") { ic.addTriangle (ix + 6.f, iy, ix + 6.f, iy + 12.f, ix, iy + 6.f); ic.addRectangle (ix, iy + 4.f, 3.f, 4.f); g.fillPath (ic); g.drawLine (ix + 8.f, iy + 2.f, ix + 13.f, iy + 10.f, 1.5f); }
        else if (b.id == "crop") { ic.startNewSubPath (ix + 3.f, iy); ic.lineTo (ix + 3.f, iy + 9.f); ic.lineTo (ix + 12.f, iy + 9.f); ic.startNewSubPath (ix, iy + 3.f); ic.lineTo (ix + 9.f, iy + 3.f); ic.lineTo (ix + 9.f, iy + 12.f); g.strokePath (ic, juce::PathStrokeType (1.5f)); }
        else { ic.addCentredArc (ix + 6.f, iy + 6.f, 5.f, 5.f, 0.f, .6f, 5.4f, true); g.strokePath (ic, juce::PathStrokeType (1.5f)); juce::Path ah; ah.addTriangle (ix + 9.f, iy - 1.f, ix + 13.f, iy + 3.f, ix + 8.f, iy + 4.f); g.fillPath (ah); }
        g.setColour (lit ? juce::Colours::white : over ? text.brighter (.2f) : tx); g.setFont (theme::fonts::ui (12.f)); g.drawText (b.label, r.toNearestInt().withTrimmedLeft (30), juce::Justification::centredLeft, true);
    }
    // perilla lisa: arco de valor con brillo, cuerpo mate, aguja blanca
    void drawKnob (juce::Graphics& g, const Knob& k, bool on)
    {
        using juce::Colour; using namespace theme; const float cx = k.c.x, cy = k.c.y, a0 = -2.356f, a1 = 2.356f; const double v = on ? k.get() : k.def;
        auto ang = [&] (double x) { return a0 + (float) ((juce::jlimit (k.lo, k.hi, x) - k.lo) / (k.hi - k.lo)) * (a1 - a0); }; const float aa = ang (v);
        const float ra = 26.f; juce::Path track; track.addCentredArc (cx, cy, ra, ra, 0.f, a0, a1, true); g.setColour (juce::Colours::white.withAlpha (.075f)); g.strokePath (track, juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (on && aa > a0 + .01f)
        {
            juce::Path val; val.addCentredArc (cx, cy, ra, ra, 0.f, a0, aa, true); g.setColour (k.col.withAlpha (.22f)); g.strokePath (val, juce::PathStrokeType (8.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            juce::ColourGradient lg (k.col, cx - ra, cy + ra, juce::Colours::white, cx + ra, cy - ra, false); g.setGradientFill (lg); g.strokePath (val, juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        for (int i = 3; i >= 1; --i) { g.setColour (juce::Colours::black.withAlpha (.13f)); g.fillEllipse (cx - 19.5f - (float) i, cy - 19.5f - (float) i + 3.f, 39.f + 2.f * (float) i, 39.f + 2.f * (float) i); }
        juce::ColourGradient bg (panelHi.brighter (.45f), cx - 6.f, cy - 8.f, boxFill, cx + 26.f, cy - 8.f, true); bg.addColour (.55, panelHi); g.setGradientFill (bg); g.fillEllipse (cx - 19.5f, cy - 19.5f, 39.f, 39.f);
        juce::ColourGradient rim (juce::Colours::white.withAlpha (.34f), 0, cy - 19.f, juce::Colours::black.withAlpha (.5f), 0, cy + 19.f, false); rim.addColour (.45, juce::Colours::white.withAlpha (.04f)); g.setGradientFill (rim); g.drawEllipse (cx - 19.f, cy - 19.f, 38.f, 38.f, 1.2f);
        juce::ColourGradient ig (boxFill, 0, cy - 14.f, panelHi, 0, cy + 14.f, false); g.setGradientFill (ig); g.fillEllipse (cx - 14.f, cy - 14.f, 28.f, 28.f); g.setColour (juce::Colours::white.withAlpha (.07f)); g.drawEllipse (cx - 14.f, cy - 14.f, 28.f, 28.f, 1.f);
        const juce::Point<float> p1 (cx + std::sin (aa) * 8.f, cy - std::cos (aa) * 8.f), p2 (cx + std::sin (aa) * 18.f, cy - std::cos (aa) * 18.f);
        if (on) { g.setColour (k.col.withAlpha (.3f)); g.drawLine ({ p1, p2 }, 5.f); }
        g.setColour (on ? juce::Colours::white : textFaint); g.drawLine ({ p1, p2 }, 2.4f);
        g.setColour (on ? textDim : textFaint); g.setFont (theme::fonts::ui (10.f)); g.drawText (k.name.toUpperCase(), (int) cx - 34, (int) cy + 28, 68, 13, juce::Justification::centred);
        auto vb = juce::Rectangle<float> (cx - 28.f, cy + 43.f, 56.f, 17.f); g.setColour (boxFill); g.fillRoundedRectangle (vb, 3.f);
        g.setColour (on ? text : textFaint); g.setFont (theme::fonts::digits (10.5f)); g.drawText (on ? juce::String (v, k.dec) + " " + k.unit : juce::String ("-"), vb.toNearestInt(), juce::Justification::centred);
    }
    static std::vector<Col> computeCols (const audioedit::Buf& b, double a, double bnd, int cols)
    {
        std::vector<Col> out ((size_t) std::max (2, cols)); const int n = b.getNumSamples(), nc = b.getNumChannels(); if (n < 1 || nc < 1) return out;
        const double s0 = a * n, s1 = bnd * n;
        for (int k = 0; k < cols; ++k)
        {
            int x0 = (int) (s0 + (s1 - s0) * k / cols), x1 = (int) (s0 + (s1 - s0) * (k + 1) / cols); x0 = juce::jlimit (0, n - 1, x0); x1 = juce::jlimit (x0 + 1, n, std::max (x1, x0 + 1)); float lo = 0.f, hi = 0.f; double sq = 0;
            for (int c = 0; c < nc; ++c) { auto* d = b.getReadPointer (c); for (int i = x0; i < x1; ++i) { lo = std::min (lo, d[i]); hi = std::max (hi, d[i]); sq += (double) d[i] * d[i]; } }
            out[(size_t) k] = { std::max (-1.f, lo), std::min (1.f, hi), std::min (1.f, (float) std::sqrt (sq / (double) ((x1 - x0) * nc))) };
        }
        return out;
    }
    void drawWave (juce::Graphics& g, bool on)
    {
        using namespace theme; using juce::Colour; auto wc = waveCard(); card (g, wc); const double dur = std::max (0.001, fileSeconds());
        // barra de herramientas: zoom y lecturas
        auto sq = [&] (juce::Rectangle<float> r, const juce::String& t) { juce::ColourGradient sg (panelHi.brighter (.15f), 0, r.getY(), panelHi, 0, r.getBottom(), false); g.setGradientFill (sg); g.fillRoundedRectangle (r, 4.f); g.setColour (seam); g.drawRoundedRectangle (r.reduced (.5f), 4.f, 1.f); g.setColour (r.contains (hot) && on ? accentHi : text); g.setFont (theme::fonts::ui (13.f)); g.drawText (t, r.toNearestInt(), juce::Justification::centred); };
        sq (zoomOutR(), juce::String::fromUTF8 ("\xe2\x88\x92")); sq (zoomInR(), "+"); sq (zoomAllR(), "Todo");
        auto readout = [&] (float x, const juce::String& lab, const juce::String& val) { g.setColour (textDim); g.setFont (theme::fonts::ui (11.5f)); g.drawText (lab, (int) x - 64, 403, 62, 24, juce::Justification::centredRight); auto b = juce::Rectangle<float> (x, 405.f, 70.f, 20.f); g.setColour (boxFill); g.fillRoundedRectangle (b, 4.f); g.setColour (accent); g.setFont (theme::fonts::digits (11.5f)); g.drawText (val, b.toNearestInt(), juce::Justification::centred); };
        readout (434.f, juce::String::fromUTF8 ("Selecci\xc3\xb3n"), hasSelection() ? juce::String ((selB - selA) * dur, 3) + " s" : juce::String ("-"));
        const auto wv = waveR(); readout (548.f, "Cursor", on && wv.contains (hot) ? juce::String (xToFrac (hot.x) * dur, 3) + " s" : juce::String ("-"));
        // fondo
        g.setColour (boxFill); g.fillRect (wv); g.setColour (bgBar); g.fillRect (rulerR());
        if (! on) { g.setColour (textFaint); g.setFont (theme::fonts::ui (13.f)); g.drawFittedText (juce::String::fromUTF8 ("Aqu\xc3\xad ver\xc3\xa1s la onda del clip."), wv.toNearestInt().reduced (40, 20), juce::Justification::centred, 2); g.setColour (seam); g.fillRect (miniR()); return; }
        const int cols = (int) wv.getWidth() * 2; if (laneCols != cols || laneA != vA || laneB != vB) { lane = computeCols (cache, vA, vB, cols); laneCols = cols; laneA = vA; laneB = vB; }
        // regla de tiempo de lo que se ve
        const double t0 = vA * dur, t1 = vB * dur, span = t1 - t0; const double steps[] { .001, .002, .005, .01, .02, .05, .1, .2, .5, 1, 2, 5, 10, 30, 60, 120 }; double st = 120; for (double q : steps) if (span / q <= 8.0) { st = q; break; }
        g.setFont (theme::fonts::digits (9.5f));
        for (double t = std::ceil (t0 / st) * st; t <= t1 + 1e-9; t += st)
        {
            const float x = wv.getX() + (float) ((t - t0) / span) * wv.getWidth(); g.setColour (barLine); g.fillRect (x, rulerR().getBottom() - 7.f, 1.f, 7.f); g.setColour (juce::Colours::white.withAlpha (.035f)); g.fillRect (x, wv.getY(), 1.f, wv.getHeight());
            if (x < wv.getRight() - 46.f) { g.setColour (textDim); g.drawText (juce::String (t, st < .01 ? 3 : st < 1 ? 2 : 0) + " s", (int) x + 4, (int) rulerR().getY() + 3, 52, 12, juce::Justification::centredLeft); }
            for (int m = 1; m < 5; ++m) { const float xm = x + (float) m * (float) (st / span) * wv.getWidth() / 5.f; if (xm < wv.getRight()) { g.setColour (gridLine); g.fillRect (xm, rulerR().getBottom() - 4.f, 1.f, 4.f); } }
        }
        const float mid = wv.getCentreY(), hh = wv.getHeight() * .5f - 5.f;
        const float lv[] { .5f, .25f }; for (int q = 0; q < 2; ++q) for (int sgn = -1; sgn <= 1; sgn += 2) { g.setColour (juce::Colours::white.withAlpha (.05f)); g.fillRect (wv.getX(), mid + (float) sgn * lv[q] * hh, wv.getWidth(), 1.f); }
        const float dx = wv.getWidth() / (float) (cols - 1);
        juce::Path pe; pe.startNewSubPath (wv.getX(), mid - lane[0].hi * hh); for (int i = 1; i < cols; ++i) pe.lineTo (wv.getX() + (float) i * dx, mid - lane[(size_t) i].hi * hh); for (int i = cols - 1; i >= 0; --i) pe.lineTo (wv.getX() + (float) i * dx, mid - lane[(size_t) i].lo * hh); pe.closeSubPath();
        g.setColour (accent.withAlpha (.2f)); g.fillPath (pe, juce::AffineTransform::translation (0.f, 0.f)); g.setColour (accent.withAlpha (.13f)); g.strokePath (pe, juce::PathStrokeType (4.f));
        { juce::ColourGradient pg (accent, 0, wv.getY(), accent, 0, wv.getBottom(), false); pg.addColour (.5, accentDim); g.setGradientFill (pg); g.fillPath (pe); g.setColour (accentHi.withAlpha (.7f)); g.strokePath (pe, juce::PathStrokeType (1.f)); }
        juce::Path pr; pr.startNewSubPath (wv.getX(), mid - lane[0].rms * hh); for (int i = 1; i < cols; ++i) pr.lineTo (wv.getX() + (float) i * dx, mid - lane[(size_t) i].rms * hh); for (int i = cols - 1; i >= 0; --i) pr.lineTo (wv.getX() + (float) i * dx, mid + lane[(size_t) i].rms * hh); pr.closeSubPath();
        { juce::ColourGradient rg (Colour (0xfffff0dc), 0, mid - hh, Colour (0xfffff0dc), 0, mid + hh, false); rg.addColour (.5, juce::Colours::white); g.setGradientFill (rg); g.fillPath (pr); }
        g.setColour (juce::Colours::white.withAlpha (.4f)); g.fillRect (wv.getX(), mid, wv.getWidth(), 1.f);
        g.setFont (theme::fonts::digits (9.f)); g.setColour (textFaint); g.drawText ("0", (int) wv.getX() + 5, (int) mid - 12, 20, 10, juce::Justification::centredLeft); g.drawText (juce::String::fromUTF8 ("\xe2\x88\x92" "6"), (int) wv.getX() + 5, (int) (mid - .5f * hh) - 11, 24, 10, juce::Justification::centredLeft); g.drawText (juce::String::fromUTF8 ("\xe2\x88\x92" "12"), (int) wv.getX() + 5, (int) (mid - .25f * hh) - 11, 24, 10, juce::Justification::centredLeft);
        // selección
        if (hasSelection())
        {
            const float sx = wv.getX() + (float) ((selA - vA) / (vB - vA)) * wv.getWidth(), ex = wv.getX() + (float) ((selB - vA) / (vB - vA)) * wv.getWidth(); const float x0 = juce::jlimit (wv.getX(), wv.getRight(), sx), x1 = juce::jlimit (wv.getX(), wv.getRight(), ex);
            if (x1 > x0) { g.setColour (accent.withAlpha (.15f)); g.fillRect (x0, wv.getY(), x1 - x0, wv.getHeight()); g.setColour (accentHi); if (sx >= wv.getX()) { g.fillRect (sx, wv.getY(), 2.f, wv.getHeight()); g.fillRect (sx - 5.f, wv.getY(), 12.f, 9.f); } if (ex <= wv.getRight()) { g.fillRect (ex - 2.f, wv.getY(), 2.f, wv.getHeight()); g.fillRect (ex - 7.f, wv.getY(), 12.f, 9.f); } }
        }
        if (wv.contains (hot) && ! dragSel) { g.setColour (juce::Colours::white.withAlpha (.55f)); g.fillRect (hot.x, wv.getY(), 1.f, wv.getHeight()); }
        // minimapa
        auto mr = miniR(); g.setColour (seam); g.fillRect (mr);
        { const int mc = (int) mr.getWidth(); if ((int) ov.size() != mc) ov = computeCols (cache, 0.0, 1.0, mc); const float mm = mr.getCentreY(), mh = mr.getHeight() * .5f - 4.f; g.setColour (textDim.withAlpha (.8f)); for (int i = 0; i < mc; ++i) { const float a = std::max (.6f, std::max (std::abs (ov[(size_t) i].lo), ov[(size_t) i].hi) * mh); g.fillRect (mr.getX() + (float) i, mm - a, 1.f, a * 2.f); } }
        { auto vr = juce::Rectangle<float> (mr.getX() + (float) vA * mr.getWidth(), mr.getY() + 2.f, std::max (3.f, (float) (vB - vA) * mr.getWidth()), mr.getHeight() - 4.f); g.setColour (accent.withAlpha (.18f)); g.fillRect (vr); g.setColour (accentHi); g.drawRect (vr, 1.f);
          if (hasSelection()) { g.setColour (Colour (0x55ffffff)); g.fillRect (mr.getX() + (float) selA * mr.getWidth(), mr.getY(), std::max (1.f, (float) (selB - selA) * mr.getWidth()), 3.f); } }
    }
    void modeMenu()
    {
        if (clip() == nullptr) return; juce::PopupMenu m; for (int i = 0; i < 3; ++i) m.addItem (1 + i, stretchModeName (i), true, stretchMode() == i);
        m.showMenuAsync (juce::PopupMenu::Options(), [this] (int r) { if (r >= 1) setStretchMode (r - 1); });
    }
    void declickMenu()
    {
        if (clip() == nullptr) return; juce::PopupMenu m; m.addSectionHeader (juce::String::fromUTF8 ("Aplicar fundidos cortos en los extremos")); const int ms[] { 1, 2, 5, 10 }; for (int i = 0; i < 4; ++i) m.addItem (1 + i, juce::String (ms[i]) + " ms", true, declickMs == ms[i]);
        m.showMenuAsync (juce::PopupMenu::Options(), [this, ms] (int r) { if (r >= 1 && r <= 4) { declickMs = ms[r - 1]; run ("declick"); } });
    }
    void say (const juce::String& s) { if (onMessage) onMessage (s); }
    void finish (const juce::String& msg) { reload(); if (msg.isNotEmpty()) say (msg); if (onChanged) onChanged(); repaint(); }
    void reload()
    {
        lane.clear(); ov.clear(); laneCols = -1; vA = 0.0; vB = 1.0; cache.setSize (0, 0); total = 0; sr = 0; chans = 0; pkDb = -120.f; auto* w = clip(); title = juce::String::fromUTF8 ("Editor de audio"); if (w == nullptr) return; title = title + juce::String::fromUTF8 ("  \xc2\xb7  ") + w->getName();
        double r = 0; if (audioedit::readFile (w->getOriginalFile(), cache, r)) { sr = r; total = cache.getNumSamples(); chans = cache.getNumChannels(); pkDb = audioedit::peakDb (cache); lastFile = w->getOriginalFile(); }
    }
    void timerCallback() override
    {
        if (isVisible() && lastMs != 0 && juce::Time::getMillisecondCounter() - lastMs < 1000) repaint();
        if (! isVisible() || ! getClip) return; auto* w = getClip(); if (w != nullptr && w != clip()) setClip (w);
        else if (w != nullptr && w->getOriginalFile() != lastFile) { reload(); repaint(); }
    }
};
