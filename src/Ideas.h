#pragma once
#include <tracktion_engine/tracktion_engine.h>
#include <algorithm>
#include <vector>

// Generador de ideas: patrones de batería, bajo, melodía y acordes por código (sin derechos de autor), con variaciones.
// Lo usan el menú Herramientas > Generador de ideas y las plantillas de género.
namespace ideas
{
    namespace te = tracktion;

    enum Style { Gabber, Hardcore, Uptempo, Hardstyle, Techno, Trance, DnB, House, kNumStyles };
    inline const char* styleName (int s) { static const char* n[] { "Gabber", "Hardcore", "Uptempo", "Hardstyle", "Techno", "Trance", "Drum & Bass", "House" }; return n[juce::jlimit (0, kNumStyles - 1, s)]; }
    inline double styleBpm (int s) { static const double b[] { 172, 180, 190, 150, 135, 138, 174, 124 }; return b[juce::jlimit (0, kNumStyles - 1, s)]; }

    struct N { int pitch; double start, len; int vel; };   // start y len en pulsos (negras) desde el inicio del clip

    // notas de los kits de batería: 36 bombo, 38/40 caja, 39 clap, 37 rim, 42 hat cerrado, 44 pedal, 46 hat abierto, 41/43/45 toms, 49 crash, 56 cowbell, 70 shaker
    enum Part { kAll = 0, kKick = 1, kSnare = 2, kHats = 3 };
    inline bool inPart (int pitch, int part)
    {
        if (part == kAll) return true;
        if (part == kKick) return pitch == 36 || pitch == 35 || pitch == 34;
        if (part == kHats) return pitch == 42 || pitch == 44 || pitch == 46 || pitch == 56 || pitch == 70;
        return ! (inPart (pitch, kKick) || inPart (pitch, kHats));   // caja, clap, rim, toms, crash
    }

    inline std::vector<N> drums (int style, int bars, int seed, int part = kAll)
    {
        juce::Random r (seed * 7919 + style * 131 + 17); std::vector<N> v;
        auto add = [&] (int bar, int step, int pitch, int vel, double len = 0.2) { v.push_back ({ pitch, bar * 4.0 + step * 0.25, len, juce::jlimit (1, 127, vel) }); };
        const bool fast = style == Gabber || style == Hardcore || style == Uptempo;
        for (int b = 0; b < bars; ++b)
        {
            const bool lastOf4 = (b % 4) == 3, crashBar = (b % 8) == 0;
            if (style == DnB)
            {
                add (b, 0, 36, 120); add (b, 10, 36, 112); if (r.nextFloat() < 0.4f) add (b, 2, 36, 90);
                add (b, 4, 38, 118); add (b, 12, 38, 118); if (r.nextFloat() < 0.5f) add (b, 15, 38, 60); if (r.nextFloat() < 0.4f) add (b, 7, 38, 55);
                for (int s = 0; s < 16; s += 2) add (b, s, 42, s % 4 == 0 ? 90 : 72); if (r.nextFloat() < 0.6f) add (b, 14, 46, 85, 0.4);
            }
            else
            {
                for (int s : { 0, 4, 8, 12 }) add (b, s, 36, s == 0 ? 127 : 118, 0.3);
                if (fast) { const float pr = style == Gabber ? 0.25f : (style == Hardcore ? 0.4f : 0.5f); for (int s : { 6, 14 }) if (r.nextFloat() < pr) add (b, s, 36, 96, 0.2); }
                add (b, 4, style == Hardcore || style == Uptempo ? 38 : 39, 112); add (b, 12, style == Hardcore || style == Uptempo ? 38 : 39, 112);
                if (style == Hardcore || style == Uptempo) { add (b, 4, 39, 85); add (b, 12, 39, 85); }
                if (style == Techno || style == House) { if (r.nextFloat() < 0.6f) add (b, 7, 37, 70); if (r.nextFloat() < 0.5f) add (b, 15, 37, 64); }
                for (int s : { 2, 6, 10, 14 }) add (b, s, 46, 92 + (int) r.nextInt (12), 0.45);
                if (style == House) for (int s = 0; s < 16; ++s) add (b, s, 70, 50 + (s % 2) * 18 + (int) r.nextInt (10), 0.15);
                else for (int s = 1; s < 16; s += 2) if (r.nextFloat() < (fast ? 0.55f : 0.8f)) add (b, s, 42, 60 + (int) r.nextInt (22), 0.15);
                if (crashBar) add (b, 0, 49, 100, 1.5);
                if (lastOf4 && b + 1 < bars + 1)   // redoble al final de cada 4 compases
                {
                    for (int k = 0; k < 4; ++k) add (b, 12 + k, 38, 78 + 11 * k, 0.2);
                    if (b % 8 == 7) { add (b, 8, 45, 100, 0.4); add (b, 10, 43, 104, 0.4); add (b, 11, 41, 108, 0.4); }
                }
            }
        }
        v.erase (std::remove_if (v.begin(), v.end(), [part] (const N& n) { return ! inPart (n.pitch, part); }), v.end());
        return v;
    }

    // ---- notas por grados de la escala menor natural ----
    inline bool& majorMode() { static bool m = false; return m; }   // false: menor natural (por defecto); true: mayor
    inline int degreePitch (int root, int deg)
    {
        static const int mi[7] { 0, 2, 3, 5, 7, 8, 10 }, ma[7] { 0, 2, 4, 5, 7, 9, 11 }; const int* sc = majorMode() ? ma : mi; const int o = (int) std::floor (deg / 7.0), d = ((deg % 7) + 7) % 7;
        return juce::jlimit (0, 127, root + 12 * o + sc[d]);
    }
    inline std::vector<int> progression (int seed)
    {
        static const int P[4][4] { { 0, 5, 2, 6 }, { 0, 6, 5, 6 }, { 0, 0, 5, 4 }, { 0, 3, 5, 6 } };   // i-VI-III-VII, i-VII-VI-VII, i-i-VI-v, i-iv-VI-VII
        static const int M[4][4] { { 0, 4, 5, 3 }, { 0, 5, 3, 4 }, { 0, 3, 4, 3 }, { 0, 5, 1, 4 } };   // I-V-vi-IV, I-vi-IV-V, I-IV-V-IV, I-vi-ii-V
        const auto& p = (majorMode() ? M : P)[(seed & 0x7fffffff) % 4]; return { p[0], p[1], p[2], p[3] };
    }

    inline std::vector<N> bass (int style, int bars, int seed, int root)
    {
        juce::Random r (seed * 104729 + style * 31 + 5); std::vector<N> v; const auto prog = progression (seed);
        for (int b = 0; b < bars; ++b)
        {
            const int deg = prog[(size_t) (b % 4)]; const int p = degreePitch (root, deg), hi = p + 12; const double o = b * 4.0;
            auto add = [&] (int step, int pitch, double lenSteps, int vel) { v.push_back ({ pitch, o + step * 0.25, lenSteps * 0.25 * 0.95, vel }); };
            switch (style)
            {
                case Gabber: case Hardcore: case Uptempo:
                    for (int s : { 2, 6, 10, 14 }) add (s, (s == 14 && (b % 2)) ? hi : p, 2, 104);
                    break;
                case Hardstyle:
                    for (int s : { 2, 6, 10, 14 }) add (s, p, 3, 112);
                    break;
                case Techno:
                    for (int s = 0; s < 16; ++s) if (s % 4 != 0) add (s, r.nextFloat() < 0.25f ? hi : p, 1, 84 + (int) r.nextInt (20));
                    break;
                case Trance:
                    for (int s : { 2, 6, 10, 14 }) { add (s, p, 1, 100); add (s + 1, p, 1, 80); }
                    break;
                case DnB:
                    add (0, p, 6, 110); add (6, p, 2, 95); add (10, r.nextFloat() < 0.5f ? hi : p, 4, 105); add (14, degreePitch (root, deg + 2), 2, 90);
                    break;
                default:   // House
                    for (int s : { 2, 6, 10, 14 }) add (s, p, 2, 100);
                    break;
            }
        }
        return v;
    }

    inline std::vector<N> melody (int style, int bars, int seed, int root)
    {
        juce::Random r (seed * 15485863 + style * 97 + 3); std::vector<N> v; const auto prog = progression (seed);
        static const int rh[6][8] { { 0, 3, 6, 8, 11, 14, -1, -1 }, { 0, 2, 4, 6, 8, 10, 12, 14 }, { 0, 3, 6, 10, 12, 15, -1, -1 }, { 0, 4, 6, 8, 12, 14, -1, -1 }, { 2, 6, 10, 14, -1, -1, -1, -1 }, { 0, 1, 3, 4, 7, 8, 11, 12 } };
        const int* pat = rh[(seed & 0x7fffffff) % 6]; std::vector<int> steps; for (int k = 0; k < 8; ++k) if (pat[k] >= 0) steps.push_back (pat[k]);
        std::vector<int> motif; int d = 2 + (int) r.nextInt (4); for (size_t k = 0; k < steps.size(); ++k) { d = juce::jlimit (-2, 9, d + (int) r.nextInt (5) - 2); motif.push_back (d); }
        for (int b = 0; b < bars; ++b)
        {
            const int shift = prog[(size_t) (b % 4)]; const bool varBar = (b % 4) == 3;
            for (size_t k = 0; k < steps.size(); ++k)
            {
                int dd = motif[k] + shift; if (varBar && k + 2 >= steps.size()) dd += (r.nextBool() ? 2 : -1);
                const int next = k + 1 < steps.size() ? steps[k + 1] : 16; const double len = std::max (1, next - steps[k]) * 0.25 * 0.9;
                v.push_back ({ degreePitch (root, dd), b * 4.0 + steps[k] * 0.25, std::max (0.2, k + 1 == steps.size() ? std::min (len, 1.0) : len), 90 + (int) r.nextInt (30) });
            }
        }
        return v;
    }

    inline std::vector<N> chords (int style, int bars, int seed, int root, bool sustained)
    {
        juce::Random r (seed * 32452843 + style * 53 + 11); std::vector<N> v; const auto prog = progression (seed);
        static const int rh[4][4] { { 0, 3, 6, 10 }, { 2, 6, 10, 14 }, { 0, 4, 8, 12 }, { 3, 6, 11, 14 } }; const int* pat = rh[(seed & 0x7fffffff) % 4];
        for (int b = 0; b < bars; ++b)
        {
            const int deg = prog[(size_t) (b % 4)];
            if (sustained) { for (int k : { 0, 2, 4 }) v.push_back ({ degreePitch (root, deg + k), b * 4.0, 3.9, 78 + (int) r.nextInt (10) }); }
            else for (int k = 0; k < 4; ++k) for (int t : { 0, 2, 4 }) v.push_back ({ degreePitch (root, deg + t), b * 4.0 + pat[k] * 0.25, 0.22, 96 + (int) r.nextInt (16) });
        }
        return v;
    }


    // ---- "IA" de ideas: entiende una frase (género, BPM, tonalidad, ánimo) y la convierte en parámetros. Es por reglas, sin red neuronal ni internet. ----
    struct Prompt { int style = -1; double bpm = 0; int root = -1; bool major = false; bool minorSet = false; int role = -1; int energy = 0; juce::StringArray understood; };
    inline juce::String fold (juce::String t)
    {
        t = t.toLowerCase(); const char* from[] { "\xc3\xa1", "\xc3\xa9", "\xc3\xad", "\xc3\xb3", "\xc3\xba", "\xc3\xbc", "\xc3\xb1" }; const char* to[] { "a", "e", "i", "o", "u", "u", "n" };
        for (int i = 0; i < 7; ++i) t = t.replace (juce::String::fromUTF8 (from[i]), to[i]); return " " + t + " ";
    }
    inline bool has (const juce::String& t, const char* w) { return t.contains (w); }
    inline Prompt parsePrompt (const juce::String& text)
    {
        Prompt p; const auto t = fold (text); auto note = [&] (const juce::String& s) { p.understood.add (s); };
        struct S { const char* w; int style; double bpm; }; static const S st[] {
            { "frenchcore", Hardcore, 200 }, { "speedcore", Uptempo, 220 }, { "terror", Uptempo, 210 }, { "uptempo", Uptempo, 0 }, { "gabber", Gabber, 0 }, { "gabba", Gabber, 0 }, { "hardcore", Hardcore, 0 },
            { "hardstyle", Hardstyle, 0 }, { "rawstyle", Hardstyle, 155 }, { "psytrance", Trance, 145 }, { "trance", Trance, 0 }, { "acid", Techno, 0 }, { "techno", Techno, 0 },
            { "drum and bass", DnB, 0 }, { "drum & bass", DnB, 0 }, { "dnb", DnB, 0 }, { "jungle", DnB, 168 }, { "drum n bass", DnB, 0 }, { "tech house", House, 126 }, { "house", House, 0 }, { "rave", Hardcore, 0 } };
        for (auto& s : st) if (has (t, s.w)) { p.style = s.style; if (s.bpm > 0) p.bpm = s.bpm; note (juce::String ("Estilo: ") + styleName (s.style)); break; }
        // BPM: "170 bpm", "a 170", o un número suelto entre 60 y 300
        { auto digitsAround = [&] (int from) { int v = 0, n = 0; for (int i = from; i < t.length() && t[i] >= '0' && t[i] <= '9'; ++i) { v = v * 10 + (t[i] - '0'); ++n; } return n >= 2 && n <= 3 ? v : 0; };
          const int at = t.indexOf ("bpm"); int v = 0;
          if (at > 0) { int e = at - 1; while (e > 0 && t[e] == ' ') --e; int b = e; while (b > 0 && t[b - 1] >= '0' && t[b - 1] <= '9') --b; v = digitsAround (b); }
          if (v == 0) for (int i = 1; i < t.length() - 1 && v == 0; ++i) if (t[i] >= '0' && t[i] <= '9' && (t[i - 1] == ' ' || t[i - 1] == 'a')) { const int d = digitsAround (i); if (d >= 60 && d <= 300) v = d; i += 2; }
          if (v >= 60 && v <= 300) { p.bpm = v; note (juce::String (v) + " BPM"); } }
        // tonalidad: "la menor", "do# mayor", "a minor", "am"
        { struct K { const char* w; int r; }; static const K ks[] { { "do#", 1 }, { "reb", 1 }, { "re#", 3 }, { "mib", 3 }, { "fa#", 6 }, { "solb", 6 }, { "sol#", 8 }, { "lab", 8 }, { "la#", 10 }, { "sib", 10 },
              { "do", 0 }, { "re", 2 }, { "mi", 4 }, { "fa", 5 }, { "sol", 7 }, { "la", 9 }, { "si", 11 } };
          static const char* en[] { "c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b" };
          const bool mn = has (t, "menor") || has (t, "minor"), mj = has (t, "mayor") || has (t, "major");
          if (mn || mj) { for (auto& k : ks) if (has (t, (juce::String (" ") + k.w + " " + (mn ? "menor" : "mayor")).toRawUTF8()) || has (t, (juce::String (" en ") + k.w + " ").toRawUTF8())) { p.root = k.r; break; }
              if (p.root < 0) for (int r = 11; r >= 0; --r) if (has (t, (juce::String (" ") + en[r] + (mn ? " minor" : " major")).toRawUTF8())) { p.root = r; break; }
              if (p.root < 0) for (auto& k : ks) if (has (t, (juce::String (" ") + k.w + " ").toRawUTF8())) { p.root = k.r; break; }
              p.major = mj && ! mn; p.minorSet = true; if (p.root >= 0) note (juce::String ("Tonalidad: ") + en[p.root] + (p.major ? " mayor" : " menor")); } }
        if (! p.minorSet) { if (has (t, "feliz") || has (t, "alegre") || has (t, "happy") || has (t, "euforic") || has (t, "uplifting") || has (t, "brillante") || has (t, "luminos")) { p.major = true; note ("\xc3\x81nimo alegre: tonalidad mayor"); }
                            else if (has (t, "oscur") || has (t, "dark") || has (t, "triste") || has (t, "sad") || has (t, "tenebros") || has (t, "agresiv")) note ("\xc3\x81nimo oscuro: tonalidad menor"); }
        if (has (t, "agresiv") || has (t, "duro") || has (t, "brutal") || has (t, "pesado") || has (t, "heavy")) { p.energy = 1; note ("Energ\xc3\xada alta"); }
        if (has (t, "suave") || has (t, "chill") || has (t, "relaj") || has (t, "calm")) { p.energy = -1; note ("Energ\xc3\xada baja"); }
        if (has (t, "solo bateria") || has (t, "solo drums") || has (t, "solo la bateria") || has (t, "un ritmo") || has (t, "beat de")) { p.role = 0; note ("Solo bater\xc3\xada"); }
        else if (has (t, "solo bajo") || has (t, "solo el bajo") || has (t, "linea de bajo")) { p.role = 1; note ("Solo bajo"); }
        else if (has (t, "melodia") && has (t, "solo")) { p.role = 2; note ("Solo melod\xc3\xada"); }
        else if (has (t, "solo acordes") || has (t, "progresion")) { p.role = 3; note ("Solo acordes"); }
        return p;
    }

    inline te::MidiClip* insert (te::Edit& e, te::AudioTrack& t, double startBeat, int bars, const std::vector<N>& notes, const juce::String& name)
    {
        auto T = [&] (double b) { return te::TimePosition::fromSeconds (e.tempoSequence.toTime (te::BeatPosition::fromBeats (b)).inSeconds()); };
        auto c = t.insertMIDIClip (name, te::TimeRange (T (startBeat), T (startBeat + bars * 4.0)), nullptr); if (c == nullptr) return nullptr;
        for (auto& n : notes) c->getSequence().addNote (n.pitch, te::BeatPosition::fromBeats (n.start), te::BeatDuration::fromBeats (n.len), n.vel, 0, &e.getUndoManager());
        return c.get();
    }
}
