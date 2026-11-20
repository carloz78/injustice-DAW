#pragma once
#include <tracktion_engine/tracktion_engine.h>
#include "SamplePack.h"

// Instrumentos incluidos: sintetizador interno (4OSC de Tracktion) con sonidos listos + kit de batería con el paquete propio.
namespace instruments
{
    namespace te = tracktion;

    struct Preset { const char* name; const char* group; };
    inline const std::vector<Preset>& list()
    {
        static const std::vector<Preset> p {
            { "Lead Hoover", "Leads" }, { "Lead Supersaw", "Leads" }, { "Stab Rave", "Leads" }, { "Pluck", "Leads" },
            { "Bajo Reese", "Bajos" }, { "Bajo Sub", "Bajos" }, { "Bajo Acid", "Bajos" },
            { "Pad", "Pads" } };
        return p;
    }

    inline void apply (te::FourOscPlugin& s, int idx)
    {
        using P = te::AutomatableParameter::Ptr;
        auto set = [] (const P& p, float v) { if (p != nullptr) p->setParameter (v, juce::sendNotification); };
        auto osc = [&] (int i, int wave, int voices, float tune, float fine, float level, float detune, float spread)
        {
            auto& o = *s.oscParams[i]; o.waveShapeValue = wave; o.voicesValue = voices;
            set (o.tune, tune); set (o.fineTune, fine); set (o.level, level); set (o.detune, detune); set (o.spread, spread);
        };
        auto amp = [&] (float a, float d, float su, float r) { set (s.ampAttack, a); set (s.ampDecay, d); set (s.ampSustain, su); set (s.ampRelease, r); };
        auto filt = [&] (int type, float freq, float res, float amount, float key, float fd, float fs)
        {
            s.filterTypeValue = type; set (s.filterFreq, freq); set (s.filterResonance, res); set (s.filterAmount, amount); set (s.filterKey, key);
            set (s.filterAttack, 0.f); set (s.filterDecay, fd); set (s.filterSustain, fs); set (s.filterRelease, 0.2f);
        };
        // todo apagado por defecto
        for (int i = 0; i < 4; ++i) osc (i, i == 0 ? 3 : 0, 1, 0, 0, -100, 0, 0);
        s.distortionOnValue = false; s.reverbOnValue = false; s.delayOnValue = false; s.chorusOnValue = false; s.voiceModeValue = 2; set (s.ampVelocity, 60.f);
        auto fx = [&] (float dist, float chorus, float reverb, float delay)
        {
            if (dist > 0) { s.distortionOnValue = true; set (s.distortion, dist); }
            if (chorus > 0) { s.chorusOnValue = true; set (s.chorusMix, chorus); }
            if (reverb > 0) { s.reverbOnValue = true; set (s.reverbSize, 0.6f); set (s.reverbMix, reverb); }
            if (delay > 0) { s.delayOnValue = true; set (s.delayMix, delay); }
        };
        set (s.masterLevel, -6.f);
        switch (idx)
        {
            case 0: // Lead Hoover
                osc (0, 3, 5, 0, 0, 0, 0.25f, 60); osc (1, 3, 3, -12, 0, -3, 0.2f, 40); osc (2, 2, 1, 12, 0, -12, 0, 0);
                amp (0.005f, 0.3f, 85, 0.25f); filt (1, 100, 20, 0.2f, 40, 0.3f, 40); fx (0.35f, 0.35f, 0.15f, 0); break;
            case 1: // Lead Supersaw
                osc (0, 3, 7, 0, 0, 0, 0.3f, 80); osc (1, 3, 5, 12, 0, -8, 0.25f, 60);
                amp (0.01f, 0.4f, 80, 0.3f); filt (1, 112, 10, 0.1f, 40, 0.3f, 60); fx (0, 0.3f, 0.25f, 0.2f); break;
            case 2: // Stab Rave
                osc (0, 3, 3, 0, 0, 0, 0.15f, 30); osc (1, 3, 3, 4, 0, -3, 0.15f, 30); osc (2, 3, 3, 7, 0, -3, 0.15f, 30);
                amp (0.001f, 0.18f, 30, 0.12f); filt (1, 90, 25, 0.3f, 30, 0.15f, 20); fx (0, 0.3f, 0.2f, 0.2f); break;
            case 3: // Pluck
                osc (0, 3, 1, 0, 0, 0, 0, 0); osc (1, 2, 1, 12, 0, -10, 0, 0);
                amp (0.001f, 0.25f, 0, 0.15f); filt (1, 60, 20, 0.5f, 40, 0.2f, 0); fx (0, 0, 0.15f, 0.2f); break;
            case 4: // Bajo Reese
                osc (0, 3, 2, 0, 0, 0, 0.1f, 20); osc (1, 3, 1, 0, 12, 0, 0, 0); osc (2, 1, 1, -12, 0, -3, 0, 0);
                amp (0.003f, 0.3f, 90, 0.15f); filt (1, 70, 15, 0.15f, 20, 0.3f, 40); fx (0.25f, 0, 0, 0); break;
            case 5: // Bajo Sub
                osc (0, 1, 1, 0, 0, 0, 0, 0); osc (1, 4, 1, 12, 0, -18, 0, 0);
                amp (0.003f, 0.2f, 100, 0.1f); s.filterTypeValue = 0; break;
            case 6: // Bajo Acid
                osc (0, 3, 1, 0, 0, 0, 0, 0); s.voiceModeValue = 0;
                amp (0.002f, 0.25f, 60, 0.08f); filt (1, 40, 70, 0.5f, 50, 0.18f, 0); fx (0.2f, 0, 0, 0); break;
            default: // Pad
                osc (0, 3, 3, 0, 0, 0, 0.2f, 70); osc (1, 4, 1, 12, 0, -6, 0, 0);
                amp (0.6f, 0.8f, 80, 1.2f); filt (1, 95, 10, 0, 30, 0.5f, 100); fx (0, 0.4f, 0.45f, 0); break;
        }
    }

    inline void removeInstruments (te::AudioTrack& t)
    {
        for (auto* p : t.pluginList.getPlugins())
            if (dynamic_cast<te::FourOscPlugin*> (p) || dynamic_cast<te::SamplerPlugin*> (p) || p->isSynth()) p->deleteFromParent();
    }

    inline te::Plugin::Ptr addSynth (te::Edit& e, te::AudioTrack& t, int idx)
    {
        removeInstruments (t);
        auto p = e.getPluginCache().createNewPlugin (te::FourOscPlugin::xmlTypeName, {}); if (p == nullptr) return nullptr;
        t.pluginList.insertPlugin (p, 0, nullptr);
        if (auto* f = dynamic_cast<te::FourOscPlugin*> (p.get())) apply (*f, idx);
        p->state.setProperty ("injPreset", list()[(size_t) idx].name, nullptr);
        return p;
    }

    // Kit de batería: cada sonido del paquete en una nota (Do1 = bombo)
    inline te::Plugin::Ptr addKit (te::Edit& e, te::AudioTrack& t, const juce::File& samplesRoot)
    {
        const auto pack = samplepack::ensure (samplesRoot);
        struct S { int note; const char* file; };
        static const S map[] { { 36, "Kicks/Kick Gabber 01.wav" }, { 35, "Kicks/Kick Punch.wav" }, { 37, "Percusiones/Perc Rim.wav" }, { 38, "Snares/Snare Hardcore.wav" },
                               { 39, "Claps/Clap Normal.wav" }, { 40, "Snares/Snare Seco.wav" }, { 41, "Percusiones/Tom Grave.wav" }, { 42, "Hats/Hat Cerrado 01.wav" },
                               { 43, "Percusiones/Tom Medio.wav" }, { 44, "Hats/Hat Cerrado 02.wav" }, { 46, "Hats/Hat Abierto.wav" }, { 49, "FX/FX Impacto.wav" }, { 56, "Percusiones/Perc Cowbell.wav" }, { 34, "Kicks/Kick Gabber 03 Duro.wav" } };
        removeInstruments (t);
        auto p = e.getPluginCache().createNewPlugin (te::SamplerPlugin::xmlTypeName, {}); if (p == nullptr) return nullptr;
        t.pluginList.insertPlugin (p, 0, nullptr);
        if (auto* sp = dynamic_cast<te::SamplerPlugin*> (p.get()))
            for (auto& m : map)
            {
                auto f = pack.getChildFile (m.file); if (! f.existsAsFile()) continue;
                if (sp->addSound (f.getFullPathName(), f.getFileNameWithoutExtension(), 0.0, 0.0, 1.0f).isEmpty())
                { const int i = sp->getNumSounds() - 1; sp->setSoundParams (i, m.note, m.note, m.note); sp->setSoundOpenEnded (i, true); }
            }
        p->state.setProperty ("injPreset", "Kit INJUSTICE", nullptr);
        return p;
    }
}
