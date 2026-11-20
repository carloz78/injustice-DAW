#pragma once
#include "FloatingPanel.h"
#include "TrackUtil.h"
#include <tracktion_engine/tracktion_engine.h>
namespace te = tracktion;

// Step Sequencer: edita un StepClip real de la Playlist (clip de patrón). Cada fila reproduce un sample con el sampler del motor.
class StepSeqPanel : public FloatingPanel, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    std::function<void()> onClipChanged;
    explicit StepSeqPanel (te::Edit& e) : edit (e)
    {
        title = "Step Sequencer"; icon = icons::Steps; setDesign (1130, kHead + 96 + 4 * 76 + kPanel + 26); startTimerHz (20);
    }

    // Crea (si hace falta) el clip de patrón en la primera pista libre, con su sampler.
    // Los samples del secuenciador suenan completos (no se cortan al terminar el paso).
    void openAll() { if (sampler) for (int i = 0; i < sampler->getNumSounds(); ++i) if (! sampler->isSoundOpenEnded (i)) sampler->setSoundOpenEnded (i, true); }
    bool inPatternMode() const { return patternMode; }
    // Reproduce solo el patrón en bucle: silencia las demás pistas mientras dura y lo restaura al parar.
    void startPattern()
    {
        ensureClip(); if (! clip) return; auto& tc = edit.getTransport();
        endPattern(); if (tc.isPlaying()) tc.stop (false, false);
        auto* host = dynamic_cast<te::AudioTrack*> (clip->getTrack()); if (host == nullptr) return;
        oldLoop = (bool) tc.looping; oldRange = tc.getLoopRange(); oldSolo.clear();
        for (auto* t : te::getAudioTracks (edit)) { oldSolo.push_back ({ t, t->isSolo (false) }); t->setSolo (t == host); }
        const auto s = clip->getPosition().getStart();
        tc.setLoopRange (te::TimeRange (s, edit.tempoSequence.toTime (edit.tempoSequence.toBeats (s) + te::BeatDuration::fromBeats (stepCount() * 0.25))));
        tc.looping = true; patternMode = true; tc.setPosition (s); tc.play (false);
    }
    void endPattern()
    {
        if (! patternMode) return; patternMode = false; auto& tc = edit.getTransport();
        for (auto& [t, so] : oldSolo) for (auto* a : te::getAudioTracks (edit)) if (a == t) a->setSolo (so);
        oldSolo.clear(); tc.looping = oldLoop; tc.setLoopRange (oldRange);
    }
    void validate() { if (clip != nullptr && clip->getTrack() == nullptr) { clip = nullptr; sampler = nullptr; samplerPtr = nullptr; repaint(); } }   // el clip pudo borrarse con Deshacer
    void ensureClip()
    {
        validate();
        if (clip != nullptr) return;
        auto tracks = te::getAudioTracks (edit);
        for (auto* t : tracks) for (auto* c : t->getClips()) if (auto* sc = dynamic_cast<te::StepClip*> (c)) { clip = sc; sampler = findSampler (t); openAll(); return; }
        te::AudioTrack* host = nullptr; for (auto* t : tracks) if (t->getClips().isEmpty()) { host = t; break; }
        if (host == nullptr) { if (tracks.isEmpty()) return; host = tracks.getLast(); }
        auto len = edit.tempoSequence.toTime (te::BeatPosition::fromBeats (16.0));
        clip = dynamic_cast<te::StepClip*> (host->insertNewClip (te::TrackItem::Type::step, juce::String::fromUTF8 ("Patrón"), te::TimeRange (te::TimePosition(), len), nullptr));
        if (clip == nullptr) return;
        samplerPtr = edit.getPluginCache().createNewPlugin (te::SamplerPlugin::xmlTypeName, {});
        host->pluginList.insertPlugin (samplerPtr, 0, nullptr); sampler = dynamic_cast<te::SamplerPlugin*> (samplerPtr.get());
        static const char* names[] { "Kick", "Clap", "Hat", "Open Hat", "Perc 5", "Perc 6", "Perc 7", "Perc 8" };
        for (int i = 0; i < clip->getChannels().size() && i < 8; ++i) { auto& ch = *clip->getChannels()[i]; ch.name = names[i]; ch.noteNumber = 36 + i; }
        clip->getPattern (0).setNumNotes (16); rows = 4;
        if (onClipChanged) onClipChanged();
    }
    // Para la captura de pantalla y pruebas
    void setStep (int row, int step, bool on) { ensureClip(); if (clip) clip->getPattern (0).setNote (row, step, on); }
    bool getStep (int row, int step) { ensureClip(); return clip && clip->getPattern (0).getNote (row, step); }
    te::StepClip* getClip() { ensureClip(); return clip.get(); }
    int numPatterns() { ensureClip(); return patterns().size(); }
    void newPattern() { ensureClip(); addPattern (false); }
    void pickPattern (int i) { ensureClip(); selectPattern (i); }
    void setRowVolume (int r, float db) { setVol (r, db); }
    void setRowPan (int r, float pn) { setPan (r, pn); }
    float rowVolume (int r) { const int i = soundForRow (r); return i >= 0 && sampler ? sampler->getSoundGainDb (i) : 0.f; }
    void setStepVelocity (int r, int st, int v) { ensureClip(); if (clip) clip->getPattern (0).setVelocity (r, st, v); }
    void assignSample (int row, const juce::File& f)
    {
        ensureClip(); if (! clip || ! sampler) return;
        int existing = soundForRow (row); if (existing >= 0) sampler->removeSound (existing);
        const auto err = sampler->addSound (f.getFullPathName(), f.getFileNameWithoutExtension(), 0.0, 0.0, 1.0f);
        if (err.isEmpty()) { const int note = clip->getChannels()[row]->noteNumber; const int idx = sampler->getNumSounds() - 1; sampler->setSoundParams (idx, note, note, note); sampler->setSoundOpenEnded (idx, true); }   // abierto: el sample suena completo aunque el paso sea corto
        repaint();
    }

protected:
    static constexpr int kRowH = 76, kBtn = 52, kNameX = 86, kNameW = 230, kStepX0 = 336, kPanel = 150, kMaxPat = 8;
    int panelY() { return kHead + 96 + rows * kRowH + 8; }
    juce::Rectangle<float> volR() { return { 16.f, (float) panelY() + 58, 290.f, 16.f }; }
    juce::Rectangle<float> panR() { return { 16.f, (float) panelY() + 108, 290.f, 16.f }; }
    juce::Rectangle<float> velR() { return { (float) kStepX0, (float) panelY() + 26, (float) designW - kStepX0 - 26.f, 100.f }; }
    juce::Rectangle<float> tabR (int i) { return { 462.f + i * 60.f, (float) kHead + 18, 54, 56 }; }
    int stepCount() { return clip ? clip->getPattern (0).getNumNotes() : 16; }
    float stepW() { return (float) (designW - kStepX0 - 26) / (float) stepCount(); }
    juce::Rectangle<float> stepR (int row, int s) { const float w = stepW(); return { kStepX0 + s * w + 2, (float) (kHead + 96 + row * kRowH) + 8, w - 6, kRowH - 16.f }; }
    juce::Rectangle<float> powerR (int row) { return { 16.f, (float) (kHead + 96 + row * kRowH) + 10, 58, kRowH - 20.f }; }
    juce::Rectangle<float> nameR (int row) { return { (float) kNameX, (float) (kHead + 96 + row * kRowH) + 10, (float) kNameW, kRowH - 20.f }; }
    juce::Rectangle<float> playR() { return { 16.f, (float) kHead + 18, 98, 56 }; }
    juce::Rectangle<float> stopR() { return { 126.f, (float) kHead + 18, 58, 56 }; }
    juce::Rectangle<float> stepsR() { return { 208.f, (float) kHead + 18, 236, 56 }; }
    juce::Rectangle<float> addR() { return { (float) designW - 86.f, (float) kHead + 18, 62, 56 }; }

    void paintBody (juce::Graphics& g) override
    {
        using namespace theme; ensureClip();
        auto& tc = edit.getTransport(); const bool playing = tc.isPlaying();
        auto btn = [&] (juce::Rectangle<float> r, bool on) { g.setColour (on ? accent : juce::Colour (0xff211f27)); g.fillRoundedRectangle (r, 8.f); };
        btn (playR(), playing && patternMode); icons::draw (g, icons::Play, playR(), juce::Colours::white);
        btn (stopR(), false); icons::draw (g, icons::Stop, stopR(), juce::Colours::white);
        btn (stepsR(), false); g.setColour (text); g.setFont (juce::FontOptions (22.f)); g.drawText (juce::String (stepCount()) + " Steps   v", stepsR().toNearestInt().reduced (16, 0), juce::Justification::centredLeft);
        btn (addR(), false); g.setColour (rows < 8 ? text : textDim); g.drawLine (addR().getCentreX() - 11, addR().getCentreY(), addR().getCentreX() + 11, addR().getCentreY(), 2.4f); g.drawLine (addR().getCentreX(), addR().getCentreY() - 11, addR().getCentreX(), addR().getCentreY() + 11, 2.4f);
        if (clip == nullptr) return;
        int cur = -1; if (playing) { const double b = edit.tempoSequence.toBeats (tc.getPosition()).inBeats() - edit.tempoSequence.toBeats (clip->getPosition().getStart()).inBeats(); if (b >= 0) cur = (int) std::floor (std::fmod (b, stepCount() * 0.25) / 0.25); }
        for (int r = 0; r < rows; ++r)
        {
            const bool on = isOn (r);
            g.setColour (juce::Colour (0xff211f27)); g.fillRoundedRectangle (powerR (r), 8.f); g.setColour (on ? accentHi : textDim);
            { auto pr = powerR (r).withSizeKeepingCentre (26, 26); g.drawEllipse (pr.reduced (1), 2.4f); g.fillRect (pr.getCentreX() - 1.2f, pr.getY() - 4, 2.4f, 14.f); }
            g.setColour (juce::Colour (0xff211f27)); g.fillRoundedRectangle (nameR (r), 8.f);
            g.setColour (text); g.setFont (juce::FontOptions (22.f)); g.drawText (clip->getChannels()[r]->name.get(), nameR (r).toNearestInt().withTrimmedLeft (18).withTrimmedRight (46).withTrimmedBottom (14), juce::Justification::centredLeft);
            g.setColour (hasSample (r) ? accentHi : textDim.withAlpha (.7f)); g.setFont (juce::FontOptions (11.f));
            g.drawText (hasSample (r) ? sampleName (r) : juce::String ("sin sample"), nameR (r).toNearestInt().withTrimmedLeft (18).withTrimmedRight (46).withTop ((int) nameR (r).getBottom() - 19).withHeight (14), juce::Justification::centredLeft);
            icons::draw (g, icons::Chevron, nameR (r).removeFromRight (46), text);
            for (int s = 0; s < stepCount(); ++s)
            {
                const bool v = clip->getPattern (0).getNote (r, s); auto sr = stepR (r, s);
                g.setColour (v ? (on ? juce::Colour (0xffcf26f5) : juce::Colour (0xff6b3a78)) : (((s / 4) & 1) ? juce::Colour (0xff2a2830) : juce::Colour (0xff24222a))); g.fillRoundedRectangle (sr, 7.f);
                if (s == cur) { g.setColour (juce::Colours::white.withAlpha (.85f)); g.drawRoundedRectangle (sr.reduced (.5f), 7.f, 2.f); }
            }
            if (r == selRow) { g.setColour (accentHi); g.fillRoundedRectangle (nameR (r).getX() - 6.f, nameR (r).getY() + 6, 3.f, nameR (r).getHeight() - 12, 1.5f); }
        }
        paintTabsAndPanel (g);
    }
    void paintTabsAndPanel (juce::Graphics& g)
    {
        using namespace theme; const auto pats = patterns();
        g.setColour (textDim); g.setFont (juce::FontOptions (12.f)); g.drawText ("Patrones", 462, kHead + 2, 120, 14, juce::Justification::centredLeft);
        for (int i = 0; i <= pats.size() && i <= kMaxPat; ++i)
        {
            auto r = tabR (i); const bool plus = i == pats.size(), cur = ! plus && pats[i] == clip.get();
            if (plus && pats.size() >= kMaxPat) break;
            g.setColour (cur ? accent : juce::Colour (0xff211f27)); g.fillRoundedRectangle (r, 8.f);
            g.setColour (cur ? juce::Colours::white : (plus ? textDim : text)); g.setFont (juce::FontOptions (plus ? 28.f : 22.f)); g.drawText (plus ? "+" : juce::String (i + 1), r.toNearestInt(), juce::Justification::centred);
        }
        // panel inferior: sonido de la fila elegida
        auto box = juce::Rectangle<float> (12.f, (float) panelY() - 2, (float) designW - 24.f, (float) kPanel + 4); g.setColour (juce::Colour (0xff1d1c24)); g.fillRoundedRectangle (box, 10.f);
        selRow = juce::jlimit (0, rows - 1, selRow); const bool has = hasSample (selRow); const int si = soundForRow (selRow);
        g.setColour (text); g.setFont (juce::FontOptions (14.f, juce::Font::bold)); g.drawText (clip->getChannels()[selRow]->name.get(), 20, panelY() + 4, 290, 20, juce::Justification::centredLeft);
        auto slider = [&] (juce::Rectangle<float> r, float frac, const juce::String& label, bool centred)
        {
            g.setColour (has ? text : textDim); g.setFont (juce::FontOptions (13.f)); g.drawText (label, (int) r.getX(), (int) r.getY() - 22, 290, 18, juce::Justification::centredLeft);
            g.setColour (juce::Colour (0xff2e2c36)); g.fillRoundedRectangle (r.withSizeKeepingCentre (r.getWidth(), 6.f), 3.f);
            g.setColour (has ? accent : textDim.withAlpha (.4f)); const float x = r.getX() + frac * r.getWidth();
            if (centred) g.fillRoundedRectangle (juce::Rectangle<float> (std::min (x, r.getCentreX()), r.getCentreY() - 3, std::abs (x - r.getCentreX()), 6.f), 3.f); else g.fillRoundedRectangle (juce::Rectangle<float> (r.getX(), r.getCentreY() - 3, x - r.getX(), 6.f), 3.f);
            g.setColour (has ? accentHi : textDim); g.fillEllipse (x - 8, r.getCentreY() - 8, 16, 16);
        };
        const float db = si >= 0 ? sampler->getSoundGainDb (si) : 0.f, pn = si >= 0 ? sampler->getSoundPan (si) : 0.f;
        slider (volR(), juce::jmap (db, -40.f, 6.f, 0.f, 1.f), has ? "Volumen   " + juce::String (db, 1) + " dB" : juce::String ("Volumen"), false);
        slider (panR(), (pn + 1.f) * .5f, has ? juce::String ("Pan   ") + (std::abs (pn) < .02f ? juce::String ("centro") : juce::String (juce::roundToInt (std::abs (pn) * 100)) + (pn < 0 ? "% izq." : "% der.")) : juce::String ("Pan"), true);
        g.setColour (text); g.setFont (juce::FontOptions (13.f)); g.drawText ("Fuerza", (int) velR().getX(), panelY() + 4, 500, 18, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff17161c)); g.fillRoundedRectangle (velR(), 6.f);
        for (int s = 0; s < stepCount(); ++s)
        {
            const float w = velR().getWidth() / (float) stepCount(); const int v = clip->getPattern (0).getVelocity (selRow, s); const bool on = clip->getPattern (0).getNote (selRow, s);
            const float h = (velR().getHeight() - 6.f) * (float) v / 127.f;
            g.setColour (on ? juce::Colour (0xffcf26f5) : juce::Colour (0xff3a3842)); g.fillRoundedRectangle (velR().getX() + s * w + 3, velR().getBottom() - 3 - h, w - 6, h, 3.f);
        }
    }
    void bodyDown (const juce::MouseEvent& e) override
    {
        ensureClip(); if (! clip) return; auto p = e.position; auto& tc = edit.getTransport();
        if (playR().contains (p)) { if (tc.isPlaying() && patternMode) { tc.stop (false, false); endPattern(); } else startPattern(); repaint(); return; }
        if (stopR().contains (p)) { tc.stop (false, false); endPattern(); tc.setPosition (clip->getPosition().getStart()); repaint(); return; }
        if (stepsR().contains (p)) { static const int opts[] { 8, 12, 16, 24, 32 }; int i = 0; for (int k = 0; k < 5; ++k) if (opts[k] == stepCount()) i = k; setSteps (opts[(i + 1) % 5]); return; }
        if (addR().contains (p)) { if (rows < 8) { ++rows; setDesign (designW, kHead + 96 + rows * kRowH + kPanel + 26); } repaint(); return; }
        for (int i = 0; i < patterns().size() + 1 && i < kMaxPat + 1; ++i) if (tabR (i).contains (p))
        { if (i < patterns().size()) { if (e.mods.isPopupMenu()) patternMenu (i); else selectPattern (i); } else if (patterns().size() < kMaxPat) addPattern (false); return; }
        if (volR().expanded (0, 10).contains (p)) { drag = Drag::vol; bodyDrag (e); return; }
        if (panR().expanded (0, 10).contains (p)) { drag = Drag::pan; bodyDrag (e); return; }
        if (velR().contains (p)) { drag = Drag::vel; bodyDrag (e); return; }
        for (int r = 0; r < rows; ++r)
        {
            if (powerR (r).contains (p)) { setOn (r, ! isOn (r)); return; }
            if (nameR (r).contains (p)) { selRow = r; if (p.x > nameR (r).getRight() - 52) rowMenu (r); else repaint(); return; }
            for (int s = 0; s < stepCount(); ++s) if (stepR (r, s).expanded (3).contains (p)) { selRow = r; auto pat = clip->getPattern (0); paint_ = ! pat.getNote (r, s); pat.setNote (r, s, paint_); lastCell = { r, s }; painting = true; if (onClipChanged) onClipChanged(); repaint(); return; }
        }
    }
    juce::String tipAt (juce::Point<float> p) override
    {
        if (playR().contains (p)) return juce::String::fromUTF8 ("Reproducir patrón");
        if (stopR().contains (p)) return "Detener";
        if (stepsR().contains (p)) return "Cantidad de pasos";
        if (addR().contains (p)) return "Agregar fila";
        for (int i = 0; i <= patterns().size() && i < kMaxPat + 1; ++i) if (tabR (i).contains (p)) return i == patterns().size() ? juce::String::fromUTF8 ("Patrón nuevo") : juce::String::fromUTF8 ("Patrón ") + juce::String (i + 1);
        for (int r = 0; r < rows; ++r)
        {
            if (powerR (r).contains (p)) return "Activar fila";
            if (nameR (r).contains (p)) return p.x > nameR (r).getRight() - 52 ? juce::String ("Sample") : juce::String();
        }
        if (volR().expanded (0, 10).contains (p)) return "Volumen";
        if (panR().expanded (0, 10).contains (p)) return "Pan";
        if (velR().contains (p)) return "Fuerza";
        return {};
    }
    void bodyDouble (const juce::MouseEvent& e) override
    {
        if (volR().expanded (0, 10).contains (e.position)) { setVol (selRow, 0.f); repaint(); }
        else if (panR().expanded (0, 10).contains (e.position)) { setPan (selRow, 0.f); repaint(); }
    }
    void bodyDrag (const juce::MouseEvent& e) override   // arrastrar sobre varias celdas pinta/borra
    {
        if (! clip) return;
        if (drag == Drag::vol) { setVol (selRow, juce::jmap (juce::jlimit (0.f, 1.f, (e.position.x - volR().getX()) / volR().getWidth()), -40.f, 6.f)); repaint(); return; }
        if (drag == Drag::pan) { float v = juce::jlimit (-1.f, 1.f, (e.position.x - panR().getX()) / panR().getWidth() * 2.f - 1.f); setPan (selRow, std::abs (v) < .04f ? 0.f : v); repaint(); return; }
        if (drag == Drag::vel)
        {
            const int st = juce::jlimit (0, stepCount() - 1, (int) ((e.position.x - velR().getX()) / (velR().getWidth() / (float) stepCount())));
            clip->getPattern (0).setVelocity (selRow, st, juce::jlimit (4, 127, (int) juce::jmap (e.position.y, velR().getBottom(), velR().getY(), 4.f, 127.f))); repaint(); return;
        }
        if (! painting) return;
        for (int r = 0; r < rows; ++r) for (int s = 0; s < stepCount(); ++s)
            if (stepR (r, s).expanded (3).contains (e.position) && lastCell != juce::Point<int> (r, s)) { clip->getPattern (0).setNote (r, s, paint_); lastCell = { r, s }; repaint(); return; }
    }
    void bodyUp (const juce::MouseEvent&) override { painting = false; drag = Drag::none; }

    bool isInterestedInFileDrag (const juce::StringArray& f) override { return f.size() > 0; }
    void filesDropped (const juce::StringArray& f, int x0, int y0) override
    {
        const float x = x0 / scale, y = y0 / scale;
        for (int r = 0; r < rows; ++r) if (nameR (r).expanded (0, 6).contains (x, y)) { assignSample (r, juce::File (f[0])); return; }
    }

private:
    te::Edit& edit; te::StepClip::Ptr clip; te::Plugin::Ptr samplerPtr; te::SamplerPlugin* sampler = nullptr; bool patternMode = false, oldLoop = false; te::TimeRange oldRange; std::vector<std::pair<te::AudioTrack*, bool>> oldSolo; int rows = 4, selRow = 0; enum class Drag { none, vol, pan, vel }; Drag drag = Drag::none; bool painting = false, paint_ = true; juce::Point<int> lastCell;
    juce::StringArray dummy;
    std::unique_ptr<juce::FileChooser> chooser;

    void timerCallback() override { validate(); if (patternMode && ! edit.getTransport().isPlaying()) endPattern(); if (isVisible() && edit.getTransport().isPlaying()) repaint(); }
    te::SamplerPlugin* findSampler (te::AudioTrack* t) { for (auto* p : t->pluginList.getPlugins()) if (auto* s = dynamic_cast<te::SamplerPlugin*> (p)) return s; return nullptr; }
    bool hasSample (int r) { return soundForRow (r) >= 0; }
    juce::String sampleName (int r) { const int i = soundForRow (r); return i >= 0 && sampler ? sampler->getSoundName (i) : juce::String(); }
    int soundForRow (int row) { if (! sampler || ! clip) return -1; const int note = clip->getChannels()[row]->noteNumber; for (int i = 0; i < sampler->getNumSounds(); ++i) if (sampler->getMinKey (i) == note || sampler->getMinKey (i) == 0) if (sampler->getMaxKey (i) == note || sampler->getMaxKey (i) == 0) return i; return -1; }
    bool isOn (int r) { return clip == nullptr || clip->getTrack() == nullptr || ! (bool) clip->getTrack()->state.getProperty ("injOff" + juce::String (r), false); }
    void setOn (int r, bool on)
    {
        if (clip->getTrack()) clip->getTrack()->state.setProperty ("injOff" + juce::String (r), ! on, nullptr);
        const int note = clip->getChannels()[r]->noteNumber; const int i = soundForRow (r);   // apagado = el sample deja de responder a su nota
        if (i >= 0 && sampler) sampler->setSoundParams (i, on ? note : 0, on ? note : 0, on ? note : 0);
        repaint();
    }
    juce::Array<te::StepClip*> patterns()
    {
        juce::Array<te::StepClip*> out; if (clip == nullptr || clip->getTrack() == nullptr) return out;
        if (auto* ct = dynamic_cast<te::ClipTrack*> (clip->getTrack())) for (auto* c : ct->getClips()) if (auto* sc = dynamic_cast<te::StepClip*> (c)) out.add (sc);
        std::sort (out.begin(), out.end(), [] (te::StepClip* a, te::StepClip* b) { return a->getPosition().getStart() < b->getPosition().getStart(); });
        return out;
    }
    void selectPattern (int i)
    {
        auto pats = patterns(); if (i < 0 || i >= pats.size() || pats[i] == clip.get()) return;
        const bool was = patternMode; if (was) { edit.getTransport().stop (false, false); endPattern(); }
        clip = pats[i]; if (was) startPattern(); repaint(); if (onClipChanged) onClipChanged();
    }
    void addPattern (bool duplicate)
    {
        auto pats = patterns(); if (pats.isEmpty() || ! clip) return; auto* host = dynamic_cast<te::AudioTrack*> (clip->getTrack()); if (host == nullptr) return;
        const double endB = edit.tempoSequence.toBeats (pats.getLast()->getPosition().getEnd()).inBeats(); const double lenB = edit.tempoSequence.toBeats (clip->getPosition().getEnd()).inBeats() - edit.tempoSequence.toBeats (clip->getPosition().getStart()).inBeats();
        const auto st = edit.tempoSequence.toTime (te::BeatPosition::fromBeats (std::ceil (endB / 4.0) * 4.0));
        te::StepClip::Ptr n;
        if (duplicate) { if (auto* c = trackutil::copyClipTo (edit, *clip, *host, st)) n = dynamic_cast<te::StepClip*> (c); }
        else
        {
            n = dynamic_cast<te::StepClip*> (host->insertNewClip (te::TrackItem::Type::step, juce::String::fromUTF8 ("Patrón ") + juce::String (pats.size() + 1), te::TimeRange (st, edit.tempoSequence.toTime (te::BeatPosition::fromBeats (std::ceil (endB / 4.0) * 4.0 + lenB))), nullptr));
            if (n) { for (int i = 0; i < n->getChannels().size() && i < clip->getChannels().size(); ++i) { n->getChannels()[i]->name = clip->getChannels()[i]->name.get(); n->getChannels()[i]->noteNumber = clip->getChannels()[i]->noteNumber.get(); } n->getPattern (0).setNumNotes (stepCount()); }
        }
        if (n) { clip = n; if (onClipChanged) onClipChanged(); } repaint();
    }
    void patternMenu (int i)
    {
        juce::PopupMenu m; m.addItem (1, juce::String::fromUTF8 ("Duplicar este patrón")); m.addItem (2, juce::String::fromUTF8 ("Eliminar este patrón"), patterns().size() > 1); m.addItem (3, juce::String::fromUTF8 ("Vaciar este patrón (borra todos sus golpes)"));
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (toLocal (tabR (i)))), [this, i] (int id)
        {
            auto pats = patterns(); if (i >= pats.size()) return; selectPattern (i);
            if (id == 1 && pats.size() < kMaxPat) addPattern (true);
            else if (id == 2 && pats.size() > 1) { te::StepClip::Ptr dead = pats[i]; clip = pats[i == 0 ? 1 : i - 1]; dead->removeFromParent(); if (onClipChanged) onClipChanged(); repaint(); }
            else if (id == 3) { clip->getPattern (0).clear(); repaint(); }
        });
    }
    void setVol (int r, float db) { const int i = soundForRow (r); if (i >= 0 && sampler) sampler->setSoundGains (i, juce::jlimit (-40.f, 6.f, db), sampler->getSoundPan (i)); }
    void setPan (int r, float pn) { const int i = soundForRow (r); if (i >= 0 && sampler) sampler->setSoundGains (i, sampler->getSoundGainDb (i), juce::jlimit (-1.f, 1.f, pn)); }
    void setSteps (int n)
    {
        clip->getPattern (0).setNumNotes (n);
        const double beats = n * 0.25; auto len = edit.tempoSequence.toTime (edit.tempoSequence.toBeats (clip->getPosition().getStart()) + te::BeatDuration::fromBeats (std::max (16.0, beats))) - clip->getPosition().getStart();
        juce::ignoreUnused (len); repaint();
    }
    void rowMenu (int r)
    {
        juce::PopupMenu m; m.addItem (1, "Cargar sample..."); m.addItem (2, "Quitar sample", hasSample (r)); m.addItem (3, "Renombrar fila...");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (toLocal (nameR (r)))), [this, r] (int id)
        {
            if (id == 1) { chooser = std::make_unique<juce::FileChooser> ("Elige un sample", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg"); chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, r] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) assignSample (r, fc.getResult()); }); }
            else if (id == 2) { const int i = soundForRow (r); if (i >= 0 && sampler) sampler->removeSound (i); repaint(); }
            else if (id == 3) { auto* aw = new juce::AlertWindow ("Renombrar fila", "", juce::MessageBoxIconType::NoIcon); aw->addTextEditor ("n", clip->getChannels()[r]->name.get()); aw->addButton ("Aceptar", 1); aw->addButton ("Cancelar", 0);
                aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, r, aw] (int res) { if (res == 1) { clip->getChannels()[r]->name = aw->getTextEditorContents ("n"); repaint(); } delete aw; })); }
        });
    }
};
