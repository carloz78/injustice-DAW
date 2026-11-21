#pragma once
#include "Instruments.h"
#include "Ideas.h"
#include "TrackUtil.h"

// Plantillas de género: un proyecto nuevo con pistas con nombre, instrumentos, mezcla y una canción de ~28 compases ya escrita.
namespace templates
{
    namespace te = tracktion;

    struct Def { const char* name; int style; int root; const char* kit; const char* bass; const char* chords; bool sustained; const char* lead; const char* pad; };
    inline const std::vector<Def>& list()
    {
        static const std::vector<Def> d {
            { "Gabber",      ideas::Gabber,    29, "Kit INJUSTICE", "Bajo Gabber", "Stab Hardcore", false, "Lead Hoover",   "Pad Oscuro" },
            { "Hardcore",    ideas::Hardcore,  29, "Kit Hardcore",  "Bajo Rumble", "Stab Menor",    false, "Lead Screech",  "Pad Cuerdas" },
            { "Uptempo",     ideas::Uptempo,   28, "Kit INJUSTICE", "Bajo Gabber", "Stab Hardcore", false, "Lead Squelch",  "Pad Oscuro" },
            { "Hardstyle",   ideas::Hardstyle, 29, "Kit INJUSTICE", "Bajo Rumble", "Lead Supersaw", true,  "Lead Screech",  "Pad" },
            { "Techno",      ideas::Techno,    33, "Kit 909",       "Bajo Acid",   "Stab Menor",    false, "Pluck",         "Pad Oscuro" },
            { "Trance",      ideas::Trance,    33, "Kit 909",       "Bajo Sub",    "Stab Rave",     false, "Lead Trance",   "Pad Cuerdas" },
            { "Drum & Bass", ideas::DnB,       31, "Kit 909",       "Bajo Reese",  "Stab Menor",    false, "Lead Chip",     "Pad" },
            { "House",       ideas::House,     36, "Kit 909",       "Bajo Sub",    "Stab Rave",     false, "Pluck",         "Pad Cuerdas" } };
        return d;
    }
    inline int& pending() { static int v = -1; return v; }   // plantilla elegida para el proyecto que se abre a continuación

    inline int synthIdx (const char* name) { for (size_t k = 0; k < instruments::list().size(); ++k) if (juce::String (instruments::list()[k].name) == name) return (int) k; return 0; }
    inline int kitIdx (const char* name) { for (size_t k = 0; k < instruments::kits().size(); ++k) if (juce::String (instruments::kits()[k].name) == name) return (int) k; return 0; }

    // Secciones: intro 4, bajada 1 de 8, ruptura 4, bajada 2 de 8, final 4 (28 compases)
    inline const int* secStart() { static const int s[5] { 0, 4, 12, 16, 24 }; return s; }
    inline const int* secBars() { static const int s[5] { 4, 8, 4, 8, 4 }; return s; }

    // Devuelve cuántas pistas creó (0 si no hay paquete de sonidos).
    inline int apply (te::Edit& e, const juce::File& packsRoot, int idx)
    {
        if (idx < 0 || idx >= (int) list().size() || ! samplepack::ready (packsRoot)) return 0;
        const auto& d = list()[(size_t) idx]; auto tracks = te::getAudioTracks (e);
        struct Spec { const char* name; int role; int mask; int colour; float db; float pan; };   // rol: 0 bombo, 1 caja/clap/fills, 2 hats, 3 bajo, 4 acordes, 5 melodía, 6 pad
        const Spec specs[] {
            { "Bombo",          0, 1 | 2 | 8 | 16, 0, -2.f, 0.f },   { "Caja / Clap", 1, 2 | 8 | 16, 1, -4.f, 0.f }, { "Hats", 2, 31, 2, -9.f, 0.15f },
            { "Bajo",           3, 2 | 8,          3, -5.f, 0.f },   { "Acordes",     4, 2 | 8,      4, -10.f, -0.2f }, { "Melodia", 5, 8 | 4, 5, -8.f, 0.2f },
            { "Pad",            6, 1 | 4 | 16,     6, -13.f, 0.f } };
        e.tempoSequence.getTempo (0)->setBpm (ideas::styleBpm (d.style));
        int made = 0; const int seed = 1000 + idx * 17;
        for (size_t i = 0; i < std::size (specs); ++i)
        {
            if ((int) i >= tracks.size()) break; auto* t = tracks[(int) i]; auto& sp = specs[i];
            t->setName (juce::String::fromUTF8 (sp.name)); trackutil::setColour (e, t, sp.colour);
            if (auto* vp = t->getVolumePlugin()) { vp->setVolumeDb (sp.db); vp->setPan (sp.pan); }
            switch (sp.role)
            {
                case 0: case 1: case 2: instruments::addKit (e, *t, packsRoot, kitIdx (d.kit)); break;
                case 3: instruments::addSynth (e, *t, synthIdx (d.bass)); break;
                case 4: instruments::addSynth (e, *t, synthIdx (d.chords)); break;
                case 5: instruments::addSynth (e, *t, synthIdx (d.lead)); break;
                default: instruments::addSynth (e, *t, synthIdx (d.pad)); break;
            }
            for (int s = 0; s < 5; ++s)
            {
                if (! (sp.mask & (1 << s))) continue; const int bars = secBars()[s]; const int sd = seed + s * 3;
                std::vector<ideas::N> n;
                switch (sp.role)
                {
                    case 0: n = ideas::drums (d.style, bars, sd, ideas::kKick); break;
                    case 1: n = ideas::drums (d.style, bars, sd, ideas::kSnare); break;
                    case 2: n = ideas::drums (d.style, bars, sd, ideas::kHats); break;
                    case 3: n = ideas::bass (d.style, bars, seed, d.root); break;
                    case 4: n = ideas::chords (d.style, bars, seed, d.root + 24, d.sustained); break;
                    case 5: n = ideas::melody (d.style, bars, seed + (s == 3 ? 1 : 0), d.root + 36); break;
                    default: n = ideas::chords (d.style, bars, seed, d.root + 12, true); break;
                }
                static const char* sn[] { "Intro", "Bajada 1", "Ruptura", "Bajada 2", "Final" };
                if (! n.empty()) ideas::insert (e, *t, secStart()[s] * 4.0, bars, n, juce::String (sp.name) + " - " + sn[s]);
            }
            ++made;
        }
        if (made >= 7) { trackutil::joinGroup (e, *tracks[1], *tracks[0], juce::String::fromUTF8 ("Bater\xc3\xad" "a")); trackutil::joinGroup (e, *tracks[2], *tracks[0]); trackutil::joinGroup (e, *tracks[4], *tracks[3], "Sintes"); trackutil::joinGroup (e, *tracks[5], *tracks[3]); trackutil::joinGroup (e, *tracks[6], *tracks[3]); }
        return made;
    }
}
