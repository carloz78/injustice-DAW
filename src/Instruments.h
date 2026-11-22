#pragma once
#include <tracktion_engine/tracktion_engine.h>
#include "SamplePack.h"
#include "Audition.h"

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
            { "Pad", "Pads" },
            { "Lead Screech", "Leads" }, { "Lead Trance", "Leads" }, { "Lead Chip", "Leads" }, { "Lead Squelch", "Leads" },
            { "Stab Hardcore", "Leads" }, { "Stab Menor", "Leads" },
            { "Bajo Gabber", "Bajos" }, { "Bajo 808", "Bajos" }, { "Bajo Donk", "Bajos" }, { "Bajo Rumble", "Bajos" },
            { "Pad Oscuro", "Pads" }, { "Pad Cuerdas", "Pads" },
            { "Campana", "Leads" }, { "Barrido Ruido", "FX" } };
        return p;
    }

    // Índices ordenados por grupo (Leads, Bajos, Pads, FX) para mostrar en menús y en el navegador
    inline std::vector<int> sortedIdx()
    {
        std::vector<int> v; for (const char* g : { "Leads", "Bajos", "Pads", "FX" }) for (size_t k = 0; k < list().size(); ++k) if (juce::String (list()[k].group) == g) v.push_back ((int) k);
        return v;
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
            case 7: // Pad
                osc (0, 3, 3, 0, 0, 0, 0.2f, 70); osc (1, 4, 1, 12, 0, -6, 0, 0);
                amp (0.6f, 0.8f, 80, 1.2f); filt (1, 95, 10, 0, 30, 0.5f, 100); fx (0, 0.4f, 0.45f, 0); break;
            case 8: // Lead Screech (chillido gabber)
                osc (0, 3, 5, 0, 0, 0, 0.2f, 40); osc (1, 2, 1, 12, 7, -6, 0, 0);
                amp (0.002f, 0.2f, 90, 0.12f); filt (1, 118, 55, 0.1f, 60, 0.12f, 70); fx (0.7f, 0, 0.1f, 0); break;
            case 9: // Lead Trance
                osc (0, 3, 5, 0, 0, 0, 0.18f, 50); osc (1, 3, 3, 12, 0, -7, 0.15f, 40);
                amp (0.004f, 0.35f, 70, 0.2f); filt (1, 105, 15, 0.25f, 50, 0.25f, 40); fx (0, 0.3f, 0.3f, 0.35f); break;
            case 10: // Lead Chip (cuadrada)
                osc (0, 2, 1, 0, 0, 0, 0, 0); osc (1, 2, 1, 12, 5, -9, 0, 0);
                amp (0.001f, 0.15f, 70, 0.06f); s.filterTypeValue = 0; fx (0, 0, 0.1f, 0.25f); break;
            case 11: // Lead Squelch (acido agudo)
                osc (0, 3, 1, 0, 0, 0, 0, 0); s.voiceModeValue = 0;
                amp (0.002f, 0.2f, 50, 0.06f); filt (1, 55, 85, 0.6f, 55, 0.14f, 0); fx (0.35f, 0, 0, 0.15f); break;
            case 12: // Stab Hardcore (tres notas)
                osc (0, 3, 3, 0, 0, 0, 0.2f, 40); osc (1, 3, 3, 3, 0, -2, 0.2f, 40); osc (2, 3, 3, 7, 0, -2, 0.2f, 40); osc (3, 3, 1, -12, 0, -6, 0, 0);
                amp (0.001f, 0.14f, 20, 0.08f); filt (1, 100, 20, 0.3f, 40, 0.12f, 20); fx (0.5f, 0, 0.1f, 0); break;
            case 13: // Stab Menor (acorde menor suave)
                osc (0, 3, 3, 0, 0, 0, 0.12f, 40); osc (1, 3, 3, 3, 0, -1, 0.12f, 40); osc (2, 3, 3, 7, 0, -1, 0.12f, 40); osc (3, 3, 3, 12, 0, -5, 0.12f, 40);
                amp (0.003f, 0.5f, 40, 0.4f); filt (1, 100, 12, 0.2f, 40, 0.4f, 30); fx (0, 0.3f, 0.35f, 0.2f); break;
            case 14: // Bajo Gabber (seno saturado)
                osc (0, 1, 1, 0, 0, 0, 0, 0); osc (1, 3, 1, -12, 0, -9, 0, 0);
                amp (0.001f, 0.3f, 80, 0.08f); filt (1, 85, 10, 0.1f, 20, 0.2f, 60); fx (0.85f, 0, 0, 0); break;
            case 15: // Bajo 808
                osc (0, 1, 1, 0, 0, 0, 0, 0);
                amp (0.001f, 0.9f, 15, 0.25f); s.filterTypeValue = 0; fx (0.3f, 0, 0, 0); break;
            case 16: // Bajo Donk
                osc (0, 2, 1, 0, 0, 0, 0, 0); osc (1, 1, 1, -12, 0, -3, 0, 0);
                amp (0.001f, 0.12f, 0, 0.05f); filt (1, 70, 40, 0.4f, 40, 0.1f, 0); fx (0.25f, 0, 0, 0); break;
            case 17: // Bajo Rumble
                osc (0, 3, 3, -12, 0, 0, 0.3f, 20); osc (1, 1, 1, -12, 0, -4, 0, 0);
                amp (0.002f, 0.4f, 85, 0.2f); filt (1, 55, 25, 0.1f, 10, 0.3f, 40); fx (0.6f, 0, 0.25f, 0); break;
            case 18: // Pad Oscuro
                osc (0, 3, 3, -12, 0, 0, 0.25f, 80); osc (1, 4, 1, 0, 0, -5, 0, 0);
                amp (1.0f, 1.0f, 85, 1.8f); filt (1, 72, 10, 0.2f, 20, 1.0f, 60); fx (0, 0.5f, 0.6f, 0.15f); break;
            case 19: // Pad Cuerdas
                osc (0, 3, 7, 0, 0, 0, 0.3f, 100); osc (1, 3, 5, 12, 0, -8, 0.25f, 80);
                amp (0.35f, 0.6f, 85, 0.9f); filt (1, 105, 8, 0, 40, 0.5f, 100); fx (0, 0.4f, 0.4f, 0); break;
            case 20: // Campana
                osc (0, 1, 1, 0, 0, 0, 0, 0); osc (1, 4, 1, 19, 0, -5, 0, 0); osc (2, 1, 1, 28, 0, -10, 0, 0);
                amp (0.001f, 0.9f, 0, 0.6f); s.filterTypeValue = 0; fx (0, 0, 0.35f, 0.3f); break;
            default: // Barrido Ruido
                osc (0, 5, 1, 0, 0, 0, 0, 0);
                amp (0.6f, 0.5f, 60, 0.6f); filt (1, 40, 40, 0.8f, 0, 1.2f, 20); fx (0, 0, 0.35f, 0); break;
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

    // Kits de batería: cada sonido del paquete en una nota (Do1 = bombo)
    struct KitDef { const char* name; std::vector<std::pair<int, const char*>> map; };
    inline const std::vector<KitDef>& kits()
    {
        static const std::vector<KitDef> k {
            { "Kit INJUSTICE", { { 36, "Kicks/Kick Gabber 01.wav" }, { 35, "Kicks/Kick Burst 01.wav" }, { 34, "Kicks/Kick Gabber 12.wav" }, { 37, "Percusiones/Rim 01.wav" },
                                 { 38, "Snares/Snare Hype 01.wav" }, { 40, "Snares/Snare Crowd 01.wav" }, { 39, "Claps/Clap Gabber 01.wav" }, { 41, "Percusiones/Tom 909 03.wav" },
                                 { 43, "Percusiones/Tom 909 07.wav" }, { 45, "Percusiones/Tom 909 09.wav" }, { 42, "Hats/Hat Corto 02.wav" }, { 44, "Hats/Hat Tick 01.wav" },
                                 { 46, "Hats/Hat Abierto 02.wav" }, { 49, "Percusiones/Crash 01.wav" }, { 51, "Percusiones/Ride 01.wav" }, { 56, "Percusiones/Cowbell 01.wav" } } },
            { "Kit 909", { { 36, "Kicks/Kick Dash 01.wav" }, { 35, "Kicks/Kick Dash 09.wav" }, { 34, "Kicks/Kick Basic 01.wav" }, { 37, "Percusiones/Rim 02.wav" },
                           { 38, "Snares/Snare Brillante 01.wav" }, { 40, "Snares/Snare Brillante 07.wav" }, { 39, "Claps/Clap 909 01.wav" }, { 41, "Percusiones/Tom 909 02.wav" },
                           { 43, "Percusiones/Tom 909 05.wav" }, { 45, "Percusiones/Tom 909 08.wav" }, { 42, "Hats/Hat Corto 01.wav" }, { 44, "Hats/Hat Tick 02.wav" },
                           { 46, "Hats/Hat Abierto 01.wav" }, { 49, "Percusiones/Crash 02.wav" }, { 51, "Percusiones/Ride 02.wav" }, { 54, "Percusiones/Tambourine 01.wav" }, { 70, "Percusiones/Shaker 01.wav" } } },
            { "Kit 808", { { 36, "Kicks/Kick Sub 06.wav" }, { 35, "Kicks/Kick Sub 10.wav" }, { 34, "Kicks/Kick Sub 01.wav" }, { 37, "Percusiones/Clave 01.wav" },
                           { 38, "Snares/Snare Oscuro 01.wav" }, { 40, "Snares/Snare Oscuro 06.wav" }, { 39, "Claps/Clap 808 01.wav" }, { 41, "Percusiones/Tom 808 02.wav" },
                           { 43, "Percusiones/Tom 808 04.wav" }, { 45, "Percusiones/Tom 808 07.wav" }, { 42, "Hats/Hat Corto 05.wav" }, { 44, "Hats/Hat Tick 03.wav" },
                           { 46, "Hats/Hat Abierto 05.wav" }, { 49, "Percusiones/Crash 03.wav" }, { 56, "Percusiones/Cowbell 03.wav" }, { 62, "Percusiones/Conga 02.wav" }, { 70, "Percusiones/Shaker 03.wav" } } },
            { "Kit Hardcore", { { 36, "Kicks/Kick Burst 05.wav" }, { 35, "Kicks/Kick Gabber 20.wav" }, { 34, "Kicks/Kick Sub 03.wav" }, { 37, "Percusiones/Rim 04.wav" },
                                { 38, "Snares/Snare Crowd 03.wav" }, { 40, "Snares/Snare Hype 05.wav" }, { 39, "Claps/Clap Gabber 05.wav" }, { 41, "Percusiones/Tom 808 01.wav" },
                                { 43, "Percusiones/Tom 808 05.wav" }, { 45, "Percusiones/Tom 909 04.wav" }, { 42, "Hats/Hat Corto 09.wav" }, { 44, "Hats/Hat Tick 05.wav" },
                                { 46, "Hats/Hat Abierto 08.wav" }, { 49, "Percusiones/Crash 05.wav" }, { 56, "Percusiones/Cowbell 05.wav" }, { 51, "Percusiones/Ride 04.wav" } } } };
        return k;
    }

    // Notas de muestra para escuchar un instrumento al elegirlo (por nombre de sonido o de kit)
    inline std::vector<audition::Player::Ev> demo (const juce::String& name)
    {
        using E = audition::Player::Ev;
        for (auto& k : kits()) if (name == k.name) return { E { 36, 0, 200 }, E { 42, 220, 320 }, E { 38, 400, 650 }, E { 42, 560, 660 }, E { 36, 700, 900 }, E { 36, 880, 1080 }, E { 38, 1000, 1300 } };
        juce::String grp; for (auto& p : list()) if (name == p.name) grp = p.group;
        if (name == "Campana") return { E { 72, 0, 1400 } };
        if (grp == "Bajos") return { E { 41, 0, 650 }, E { 44, 750, 1300 } };
        if (grp == "Pads") return { E { 53, 0, 1600 }, E { 56, 0, 1600 }, E { 60, 0, 1600 } };
        if (grp == "FX") return { E { 48, 0, 1400 } };
        if (name == "Stab Hardcore" || name == "Stab Menor" || name == "Stab Rave") return { E { 60, 0, 260 }, E { 60, 450, 710 } };
        return { E { 65, 0, 450 }, E { 68, 520, 970 }, E { 72, 1050, 1500 } };
    }

    // Devuelve nullptr si el paquete todavía se está generando (así la interfaz no se congela).
    inline te::Plugin::Ptr addKit (te::Edit& e, te::AudioTrack& t, const juce::File& packsRoot, int kitIdx = 0)
    {
        if (! samplepack::ready (packsRoot)) return nullptr;
        const auto pack = samplepack::packDir (packsRoot); const auto& kd = kits()[(size_t) juce::jlimit (0, (int) kits().size() - 1, kitIdx)];
        removeInstruments (t);
        auto p = e.getPluginCache().createNewPlugin (te::SamplerPlugin::xmlTypeName, {}); if (p == nullptr) return nullptr;
        t.pluginList.insertPlugin (p, 0, nullptr);
        if (auto* sp = dynamic_cast<te::SamplerPlugin*> (p.get()))
            for (auto& m : kd.map)
            {
                auto f = pack.getChildFile (m.second); if (! f.existsAsFile()) continue;
                if (sp->addSound (f.getFullPathName(), f.getFileNameWithoutExtension(), 0.0, 0.0, 1.0f).isEmpty())
                { const int i = sp->getNumSounds() - 1; sp->setSoundParams (i, m.first, m.first, m.first); sp->setSoundOpenEnded (i, true); }
            }
        p->state.setProperty ("injPreset", kd.name, nullptr);
        return p;
    }
}
