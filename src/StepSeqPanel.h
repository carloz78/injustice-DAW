#pragma once
#include "FloatingPanel.h"
#include <tracktion_engine/tracktion_engine.h>
namespace te = tracktion;

// Step Sequencer: edita un StepClip real de la Playlist (clip de patrón). Cada fila reproduce un sample con el sampler del motor.
class StepSeqPanel : public FloatingPanel, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    std::function<void()> onClipChanged;
    explicit StepSeqPanel (te::Edit& e) : edit (e)
    {
        title = "Step Sequencer"; icon = icons::Steps; setDesign (1130, kHead + 96 + 4 * 76 + 26); startTimerHz (20);
    }

    // Crea (si hace falta) el clip de patrón en la primera pista libre, con su sampler.
    void validate() { if (clip != nullptr && clip->getTrack() == nullptr) { clip = nullptr; sampler = nullptr; samplerPtr = nullptr; repaint(); } }   // el clip pudo borrarse con Deshacer
    void ensureClip()
    {
        validate();
        if (clip != nullptr) return;
        auto tracks = te::getAudioTracks (edit);
        for (auto* t : tracks) for (auto* c : t->getClips()) if (auto* sc = dynamic_cast<te::StepClip*> (c)) { clip = sc; sampler = findSampler (t); return; }
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
    void assignSample (int row, const juce::File& f)
    {
        ensureClip(); if (! clip || ! sampler) return;
        int existing = soundForRow (row); if (existing >= 0) sampler->removeSound (existing);
        const auto err = sampler->addSound (f.getFullPathName(), f.getFileNameWithoutExtension(), 0.0, 0.0, 1.0f);
        if (err.isEmpty()) { const int note = clip->getChannels()[row]->noteNumber; sampler->setSoundParams (sampler->getNumSounds() - 1, note, note, note); }
        repaint();
    }

protected:
    static constexpr int kRowH = 76, kBtn = 52, kNameX = 86, kNameW = 230, kStepX0 = 336;
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
        btn (playR(), playing); icons::draw (g, icons::Play, playR(), juce::Colours::white);
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
        }
    }
    void bodyDown (const juce::MouseEvent& e) override
    {
        ensureClip(); if (! clip) return; auto p = e.position; auto& tc = edit.getTransport();
        if (playR().contains (p)) { if (tc.isPlaying()) tc.stop (false, false); else { tc.setPosition (clip->getPosition().getStart()); tc.play (false); } repaint(); return; }
        if (stopR().contains (p)) { tc.stop (false, false); tc.setPosition (clip->getPosition().getStart()); repaint(); return; }
        if (stepsR().contains (p)) { static const int opts[] { 8, 12, 16, 24, 32 }; int i = 0; for (int k = 0; k < 5; ++k) if (opts[k] == stepCount()) i = k; setSteps (opts[(i + 1) % 5]); return; }
        if (addR().contains (p)) { if (rows < 8) { ++rows; setDesign (designW, kHead + 96 + rows * kRowH + 26); } repaint(); return; }
        for (int r = 0; r < rows; ++r)
        {
            if (powerR (r).contains (p)) { setOn (r, ! isOn (r)); return; }
            if (nameR (r).contains (p)) { rowMenu (r); return; }
            for (int s = 0; s < stepCount(); ++s) if (stepR (r, s).expanded (3).contains (p)) { auto pat = clip->getPattern (0); paint_ = ! pat.getNote (r, s); pat.setNote (r, s, paint_); lastCell = { r, s }; painting = true; if (onClipChanged) onClipChanged(); repaint(); return; }
        }
    }
    void bodyDrag (const juce::MouseEvent& e) override   // arrastrar sobre varias celdas pinta/borra
    {
        if (! painting || ! clip) return;
        for (int r = 0; r < rows; ++r) for (int s = 0; s < stepCount(); ++s)
            if (stepR (r, s).expanded (3).contains (e.position) && lastCell != juce::Point<int> (r, s)) { clip->getPattern (0).setNote (r, s, paint_); lastCell = { r, s }; repaint(); return; }
    }
    void bodyUp (const juce::MouseEvent&) override { painting = false; }

    bool isInterestedInFileDrag (const juce::StringArray& f) override { return f.size() > 0; }
    void filesDropped (const juce::StringArray& f, int x0, int y0) override
    {
        const float x = x0 / scale, y = y0 / scale;
        for (int r = 0; r < rows; ++r) if (nameR (r).expanded (0, 6).contains (x, y)) { assignSample (r, juce::File (f[0])); return; }
    }

private:
    te::Edit& edit; te::StepClip::Ptr clip; te::Plugin::Ptr samplerPtr; te::SamplerPlugin* sampler = nullptr; int rows = 4; bool painting = false, paint_ = true; juce::Point<int> lastCell;
    juce::StringArray dummy;
    std::unique_ptr<juce::FileChooser> chooser;

    void timerCallback() override { validate(); if (isVisible() && edit.getTransport().isPlaying()) repaint(); }
    te::SamplerPlugin* findSampler (te::AudioTrack* t) { for (auto* p : t->pluginList.getPlugins()) if (auto* s = dynamic_cast<te::SamplerPlugin*> (p)) return s; return nullptr; }
    bool hasSample (int r) { return soundForRow (r) >= 0; }
    juce::String sampleName (int r) { const int i = soundForRow (r); return i >= 0 && sampler ? sampler->getSoundName (i) : juce::String(); }
    int soundForRow (int row) { if (! sampler || ! clip) return -1; const int note = clip->getChannels()[row]->noteNumber; for (int i = 0; i < sampler->getNumSounds(); ++i) if (sampler->getMinKey (i) == note || sampler->getMinKey (i) == 0) if (sampler->getMaxKey (i) == note || sampler->getMaxKey (i) == 0) return i; return -1; }
    bool isOn (int r) { return clip == nullptr || ! (bool) clip->state.getProperty ("injOff" + juce::String (r), false); }
    void setOn (int r, bool on)
    {
        clip->state.setProperty ("injOff" + juce::String (r), ! on, nullptr);
        const int note = clip->getChannels()[r]->noteNumber; const int i = soundForRow (r);   // apagado = el sample deja de responder a su nota
        if (i >= 0 && sampler) sampler->setSoundParams (i, on ? note : 0, on ? note : 0, on ? note : 0);
        repaint();
    }
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
