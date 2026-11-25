#pragma once
#include "FloatingPanel.h"
#include "SamplePack.h"
#include "Theme.h"
#include <tracktion_engine/tracktion_engine.h>
#include <functional>

// Rack de pads: 16 pads (notas 36 a 51) sobre el sampler de la pista elegida. Cada pad puede tener varias capas (sonidos que suenan a la vez,
// por ejemplo un bombo y su distorsión), y su propio volumen, panorama y afinación. Clic = tocar el pad; soltar un archivo sobre un pad lo carga.
class PadRackPanel : public FloatingPanel, public juce::FileDragAndDropTarget
{
public:
    static constexpr int kFirst = 36, kPads = 16;
    std::function<te::AudioTrack*()> getTrack;
    std::function<void (te::AudioTrack*, int)> onAudition;
    std::function<void (const juce::String&)> onMessage;
    std::function<void()> onChanged;
    int selPad = kFirst;

    explicit PadRackPanel (te::Edit& e) : edit (e) { title = "Rack de pads"; icon = icons::Steps; setDesign (640, 548); }

    // ---- lógica (pública para las pruebas) ----
    te::SamplerPlugin* sampler() const { if (auto* t = track()) for (auto* p : t->pluginList.getPlugins()) if (auto* s = dynamic_cast<te::SamplerPlugin*> (p)) return s; return nullptr; }
    te::AudioTrack* track() const { return getTrack ? getTrack() : nullptr; }
    std::vector<int> soundsAt (int note) const { std::vector<int> v; if (auto* sp = sampler()) for (int i = 0; i < sp->getNumSounds(); ++i) if (sp->getMinKey (i) == note && sp->getMaxKey (i) == note) v.push_back (i); return v; }
    juce::String padName (int note) const { auto s = soundsAt (note); auto* sp = sampler(); return s.empty() || sp == nullptr ? juce::String() : sp->getSoundName (s[0]); }
    int numLayers (int note) const { return (int) soundsAt (note).size(); }
    float padVolume (int note) const { auto s = soundsAt (note); auto* sp = sampler(); return s.empty() || sp == nullptr ? 0.f : sp->getSoundGainDb (s[0]); }
    float padPan (int note) const { auto s = soundsAt (note); auto* sp = sampler(); return s.empty() || sp == nullptr ? 0.f : sp->getSoundPan (s[0]); }
    float padTune (int note) const { auto s = soundsAt (note); auto* sp = sampler(); return s.empty() || sp == nullptr ? 0.f : (float) (note - sp->getKeyNote (s[0])); }

    bool createEmptyKit()
    {
        auto* t = track(); if (t == nullptr) { say (juce::String::fromUTF8 ("Elige primero una pista")); return false; } if (sampler() != nullptr) return true;
        edit.getUndoManager().beginNewTransaction ("Kit de pads"); auto p = edit.getPluginCache().createNewPlugin (te::SamplerPlugin::xmlTypeName, {}); if (p == nullptr) return false; t->pluginList.insertPlugin (p, 0, nullptr); finish ({}); return true;
    }
    // Añade un sonido como capa del pad. Devuelve un texto de error vacío si salió bien.
    juce::String addLayer (int note, const juce::File& f)
    {
        auto* sp = sampler(); if (sp == nullptr) return juce::String::fromUTF8 ("Esta pista no tiene un kit de pads");
        if (! f.existsAsFile()) return juce::String::fromUTF8 ("El archivo no existe"); if (note < kFirst || note >= kFirst + kPads) return "Pad fuera de rango"; if (numLayers (note) >= 6) return juce::String::fromUTF8 ("Un pad admite hasta 6 capas");
        const float vol = padVolume (note), pan = padPan (note), tune = padTune (note); const bool had = numLayers (note) > 0;
        edit.getUndoManager().beginNewTransaction (juce::String::fromUTF8 ("Capa de pad")); const auto err = sp->addSound (f.getFullPathName(), f.getFileNameWithoutExtension(), 0.0, 0.0, 0.0f); if (err.isNotEmpty()) return err;
        const int i = sp->getNumSounds() - 1; sp->setSoundParams (i, note - (int) tune, note, note); sp->setSoundOpenEnded (i, true); if (had) sp->setSoundGains (i, vol, pan);
        finish ({}); return {};
    }
    juce::String loadSample (int note, const juce::File& f) { clearPad (note, false); return addLayer (note, f); }
    bool clearPad (int note, bool report = true)
    {
        auto* sp = sampler(); auto s = soundsAt (note); if (sp == nullptr || s.empty()) return false; edit.getUndoManager().beginNewTransaction ("Quitar pad");
        for (int k = (int) s.size() - 1; k >= 0; --k) sp->removeSound (s[(size_t) k]); finish (report ? juce::String::fromUTF8 ("Pad vac\xc3\xado") : juce::String()); return true;
    }
    void setPadVolume (int note, float db) { applyGain (note, juce::jlimit (-60.f, 12.f, db), padPan (note)); }
    void setPadPan (int note, float pan) { applyGain (note, padVolume (note), juce::jlimit (-1.f, 1.f, pan)); }
    void setPadTune (int note, float semis)
    {
        auto* sp = sampler(); if (sp == nullptr) return; const int st = juce::jlimit (-24, 24, (int) std::lround (semis)); edit.getUndoManager().beginNewTransaction (juce::String::fromUTF8 ("Afinaci\xc3\xb3n del pad"));
        for (int i : soundsAt (note)) sp->setSoundParams (i, note - st, note, note); finish ({});
    }
    void audition (int note) { selPad = note; if (onAudition) if (auto* t = track()) if (numLayers (note) > 0) onAudition (t, note); repaint(); }

    void paintBody (juce::Graphics& g) override
    {
        using namespace theme; auto* sp = sampler(); const bool has = track() != nullptr;
        if (! has || sp == nullptr)
        {
            g.setColour (textFaint); g.setFont (fonts::ui (14.f)); g.drawFittedText (has ? juce::String::fromUTF8 ("Esta pista no tiene un kit de pads.\nPulsa el bot\xc3\xb3n para crear uno vac\xc3\xado y suelta samples sobre los pads.") : juce::String::fromUTF8 ("Elige una pista en la Playlist o el Mixer."), { 20, 70, designW - 40, 200 }, juce::Justification::centred, 4);
            if (has) drawBtn (g, makeKitR(), juce::String::fromUTF8 ("Crear kit de pads"), true); return;
        }
        for (int i = 0; i < kPads; ++i)
        {
            const int note = kFirst + i; auto r = padR (note); const bool on = numLayers (note) > 0, sel = note == selPad;
            g.setColour (on ? panelHi.withAlpha (.9f) : bgBar); g.fillRoundedRectangle (r, rad + 2.f); g.setColour (sel ? accent : boxLine); g.drawRoundedRectangle (r.reduced (.5f), rad + 2.f, sel ? 2.f : 1.f);
            if (on) { g.setColour (accent.withAlpha (.8f)); g.fillRect (r.getX() + 8.f, r.getY() + 8.f, 22.f, 3.f); }
            g.setColour (on ? text : textFaint); g.setFont (fonts::ui (12.5f)); g.drawFittedText (on ? padName (note) : juce::String::fromUTF8 ("vac\xc3\xado"), r.toNearestInt().reduced (8, 18), juce::Justification::centredLeft, 2);
            g.setColour (textDim); g.setFont (fonts::digits (10.5f)); g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 3), r.toNearestInt().reduced (8, 4), juce::Justification::bottomLeft);
            if (numLayers (note) > 1) { g.setColour (accentHi); g.drawText (juce::String (numLayers (note)) + " capas", r.toNearestInt().reduced (8, 4), juce::Justification::bottomRight); }
        }
        auto box = editR(); g.setColour (bgBar); g.fillRoundedRectangle (box, rad); g.setColour (boxLine); g.drawRoundedRectangle (box.reduced (.5f), rad, 1.f);
        const bool on = numLayers (selPad) > 0; g.setColour (textDim); g.setFont (fonts::ui (12.5f, true));
        g.drawText (juce::String::fromUTF8 ("Pad ") + juce::MidiMessage::getMidiNoteName (selPad, true, true, 3) + (on ? juce::String::fromUTF8 ("  \xc2\xb7  ") + padName (selPad) : juce::String()), (int) box.getX() + 12, (int) box.getY() + 6, (int) box.getWidth() - 24, 22, juce::Justification::centredLeft, true);
        for (auto& c : ctls()) drawCtl (g, c, on);
        drawBtn (g, loadR(), juce::String::fromUTF8 ("Cargar sample\xe2\x80\xa6"), true); drawBtn (g, layerR(), juce::String::fromUTF8 ("A\xc3\xb1" "adir capa\xe2\x80\xa6"), on); drawBtn (g, clearR(), "Vaciar pad", on);
    }
    void bodyDown (const juce::MouseEvent& e) override
    {
        const auto p = e.position; auto* sp = sampler();
        if (track() != nullptr && sp == nullptr) { if (makeKitR().contains (p)) createEmptyKit(); return; } if (sp == nullptr) return;
        for (int i = 0; i < kPads; ++i) if (padR (kFirst + i).contains (p)) { audition (kFirst + i); return; }
        const auto cs = ctls(); for (size_t i = 0; i < cs.size(); ++i) if (cs[i].r.contains (p) && numLayers (selPad) > 0) { dragCtl = (int) i; dragX = p.x; dragVal = cs[i].get(); return; }
        if (loadR().contains (p)) chooseFile (false); else if (layerR().contains (p) && numLayers (selPad) > 0) chooseFile (true); else if (clearR().contains (p)) clearPad (selPad);
    }
    void bodyDrag (const juce::MouseEvent& e) override
    {
        if (dragCtl < 0) return; const auto cs = ctls(); const auto& c = cs[(size_t) dragCtl]; c.set (juce::jlimit (c.lo, c.hi, dragVal + (e.position.x - dragX) / c.r.getWidth() * (c.hi - c.lo) * (e.mods.isShiftDown() ? .1 : 1.0))); repaint();
    }
    void bodyUp (const juce::MouseEvent&) override { dragCtl = -1; }
    void bodyDouble (const juce::MouseEvent& e) override { for (auto& c : ctls()) if (c.r.contains (e.position) && numLayers (selPad) > 0) { c.set (c.def); repaint(); return; } }
    juce::String tipAt (juce::Point<float> p) override
    {
        for (int i = 0; i < kPads; ++i) if (padR (kFirst + i).contains (p)) return juce::String::fromUTF8 ("Clic = tocar. Suelta aqu\xc3\xad un archivo de audio para cargarlo");
        for (auto& c : ctls()) if (c.r.contains (p)) return juce::String::fromUTF8 ("Arrastra (Mayús = fino). Doble clic = valor original");
        return {};
    }
    bool isInterestedInFileDrag (const juce::StringArray& f) override { return sampler() != nullptr && f.size() > 0; }
    void filesDropped (const juce::StringArray& files, int x, int y) override
    {
        const juce::Point<float> p ((float) x / scale, (float) y / scale);
        for (int i = 0; i < kPads; ++i) if (padR (kFirst + i).contains (p)) { selPad = kFirst + i; const auto err = loadSample (selPad, juce::File (files[0])); if (err.isNotEmpty()) say (err); return; }
    }

private:
    struct Ctl { juce::Rectangle<float> r; juce::String name, unit; double lo, hi, def; std::function<double()> get; std::function<void (double)> set; int dec = 1; };
    te::Edit& edit; int dragCtl = -1; float dragX = 0.f; double dragVal = 0; std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<float> padR (int note) const { const int i = note - kFirst, col = i % 4, row = 3 - i / 4; return { 20.f + (float) col * 153.f, 62.f + (float) row * 92.f, 145.f, 84.f }; }
    juce::Rectangle<float> editR() const { return { 20.f, 436.f, (float) designW - 40.f, 100.f }; }
    juce::Rectangle<float> loadR() const { return { 20.f + 12.f + 420.f, 436.f + 34.f, 150.f, 26.f }; }
    juce::Rectangle<float> layerR() const { return { 20.f + 12.f + 420.f, 436.f + 66.f, 74.f, 26.f }; }
    juce::Rectangle<float> clearR() const { return { 20.f + 12.f + 420.f + 78.f, 436.f + 66.f, 72.f, 26.f }; }
    juce::Rectangle<float> makeKitR() const { return { (float) designW * .5f - 90.f, 250.f, 180.f, 34.f }; }
    std::vector<Ctl> ctls()
    {
        std::vector<Ctl> v; auto b = editR(); const float w = 400.f; auto row = [&] (int i) { return juce::Rectangle<float> (b.getX() + 12.f, b.getY() + 34.f + (float) i * 21.f, w, 19.f); };
        v.push_back ({ row (0), "Volumen", "dB", -60, 12, 0, [this] { return (double) padVolume (selPad); }, [this] (double x) { setPadVolume (selPad, (float) x); }, 1 });
        v.push_back ({ row (1), "Panorama", "", -1, 1, 0, [this] { return (double) padPan (selPad); }, [this] (double x) { setPadPan (selPad, std::round ((float) x * 20.f) / 20.f); }, 2 });
        v.push_back ({ row (2), juce::String::fromUTF8 ("Afinaci\xc3\xb3n"), "st", -24, 24, 0, [this] { return (double) padTune (selPad); }, [this] (double x) { setPadTune (selPad, (float) x); }, 0 });
        return v;
    }
    void drawCtl (juce::Graphics& g, const Ctl& c, bool on)
    {
        using namespace theme; g.setColour (panelHi.withAlpha (on ? .5f : .25f)); g.fillRoundedRectangle (c.r, rad);
        const double v = on ? c.get() : c.def; const float t = (float) ((v - c.lo) / (c.hi - c.lo)), d = (float) ((c.def - c.lo) / (c.hi - c.lo));
        if (on) { const float x0 = c.r.getX() + c.r.getWidth() * std::min (t, d), x1 = c.r.getX() + c.r.getWidth() * std::max (t, d); g.setColour (accent.withAlpha (.55f)); g.fillRect (x0, c.r.getBottom() - 3.f, std::max (2.f, x1 - x0), 2.f); }
        g.setColour (on ? text : textFaint); g.setFont (fonts::ui (11.5f)); g.drawText (c.name, c.r.toNearestInt().withTrimmedLeft (8), juce::Justification::centredLeft, true);
        g.setColour (on ? accentHi : textFaint); g.setFont (fonts::digits (11.5f)); g.drawText (juce::String (v, c.dec) + " " + c.unit, c.r.toNearestInt().withTrimmedRight (8), juce::Justification::centredRight);
    }
    void drawBtn (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool on)
    {
        using namespace theme; g.setColour (panelHi.withAlpha (on ? .7f : .3f)); g.fillRoundedRectangle (r, rad); g.setColour (boxLine); g.drawRoundedRectangle (r.reduced (.5f), rad, 1.f);
        g.setColour (on ? text : textFaint); g.setFont (fonts::ui (12.f)); g.drawText (t, r.toNearestInt().reduced (4, 0), juce::Justification::centred, true);
    }
    void chooseFile (bool layer)
    {
        const int note = selPad; chooser = std::make_unique<juce::FileChooser> (juce::String::fromUTF8 (layer ? "A\xc3\xb1" "adir una capa" : "Cargar un sample"), samplepack::packsDir(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, note, layer] (const juce::FileChooser& fc)
        { const auto f = fc.getResult(); if (f == juce::File()) return; const auto err = layer ? addLayer (note, f) : loadSample (note, f); if (err.isNotEmpty()) say (err); });
    }
    void applyGain (int note, float db, float pan) { auto* sp = sampler(); if (sp == nullptr) return; edit.getUndoManager().beginNewTransaction ("Volumen del pad"); for (int i : soundsAt (note)) sp->setSoundGains (i, db, pan); finish ({}); }
    void say (const juce::String& s) { if (onMessage) onMessage (s); }
    void finish (const juce::String& msg) { if (msg.isNotEmpty()) say (msg); if (onChanged) onChanged(); repaint(); }
};
