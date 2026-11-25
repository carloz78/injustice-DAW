#pragma once
#include <tracktion_engine/tracktion_engine.h>
#include <algorithm>
#include <array>
#include <vector>

// Armonía del proyecto: tonalidad (nota + escala) y pista de acordes. Se guarda en el proyecto:
// la tonalidad como propiedades injKeyRoot / injKeyScale de edit.state y los acordes como hijos INJCHORD.
namespace harmony
{
    namespace te = tracktion;

    struct ScaleDef { const char* name; std::vector<int> steps; };
    inline const std::vector<ScaleDef>& scales()
    {
        static const std::vector<ScaleDef> s {
            { "Sin escala (todas las notas)", { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
            { "Menor natural", { 0, 2, 3, 5, 7, 8, 10 } },
            { "Mayor", { 0, 2, 4, 5, 7, 9, 11 } },
            { "Frigio (oscuro, muy gabber)", { 0, 1, 3, 5, 7, 8, 10 } },
            { "Menor arm\xc3\xb3nica", { 0, 2, 3, 5, 7, 8, 11 } },
            { "Frigio dominante", { 0, 1, 4, 5, 7, 8, 10 } },
            { "Dorio", { 0, 2, 3, 5, 7, 9, 10 } },
            { "Lidio", { 0, 2, 4, 6, 7, 9, 11 } },
            { "Mixolidio", { 0, 2, 4, 5, 7, 9, 10 } },
            { "Locrio", { 0, 1, 3, 5, 6, 8, 10 } },
            { "Menor mel\xc3\xb3" "dica", { 0, 2, 3, 5, 7, 9, 11 } },
            { "Pentat\xc3\xb3nica menor", { 0, 3, 5, 7, 10 } },
            { "Pentat\xc3\xb3nica mayor", { 0, 2, 4, 7, 9 } },
            { "Blues", { 0, 3, 5, 6, 7, 10 } },
            { "Tonos enteros", { 0, 2, 4, 6, 8, 10 } },
        };
        return s;
    }
    inline const char* noteName (int pc) { static const char* n[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return n[((pc % 12) + 12) % 12]; }

    struct Key { int root = 0; int scale = 0; bool active() const { return scale > 0; } };
    inline Key getKey (te::Edit& e) { Key k; k.root = juce::jlimit (0, 11, (int) e.state.getProperty ("injKeyRoot", 0)); k.scale = juce::jlimit (0, (int) scales().size() - 1, (int) e.state.getProperty ("injKeyScale", 0)); return k; }
    inline void setKey (te::Edit& e, Key k) { e.getUndoManager().beginNewTransaction ("Tonalidad"); e.state.setProperty ("injKeyRoot", juce::jlimit (0, 11, k.root), &e.getUndoManager()); e.state.setProperty ("injKeyScale", juce::jlimit (0, (int) scales().size() - 1, k.scale), &e.getUndoManager()); }
    inline bool inScale (Key k, int note) { if (! k.active()) return true; const int d = (((note - k.root) % 12) + 12) % 12; for (int s : scales()[(size_t) k.scale].steps) if (s == d) return true; return false; }
    inline bool isRoot (Key k, int note) { return k.active() && (((note - k.root) % 12) + 12) % 12 == 0; }
    inline juce::String keyText (Key k) { return k.active() ? juce::String (noteName (k.root)) + " " + juce::String::fromUTF8 (scales()[(size_t) k.scale].name).upToFirstOccurrenceOf (" (", false, false) : juce::String::fromUTF8 ("Sin tonalidad"); }
    // nota más cercana dentro de la escala (si hay empate, la de abajo)
    inline int snapToScale (Key k, int note)
    {
        if (! k.active() || inScale (k, note)) return note;
        for (int d = 1; d < 12; ++d) { if (inScale (k, note - d)) return note - d; if (inScale (k, note + d)) return note + d; }
        return note;
    }

    // ---- acordes ----
    struct Quality { const char* sym; const char* name; std::vector<int> iv; };
    inline const std::vector<Quality>& qualities()
    {
        static const std::vector<Quality> q {
            { "", "mayor", { 0, 4, 7 } }, { "m", "menor", { 0, 3, 7 } }, { "5", "potencia (5)", { 0, 7 } }, { "7", "dominante 7", { 0, 4, 7, 10 } }, { "m7", "menor 7", { 0, 3, 7, 10 } }, { "maj7", "mayor 7", { 0, 4, 7, 11 } },
            { "dim", "disminuido", { 0, 3, 6 } }, { "sus4", "sus4", { 0, 5, 7 } }, { "sus2", "sus2", { 0, 2, 7 } }, { "add9", "add9", { 0, 4, 7, 14 } }, { "m9", "menor 9", { 0, 3, 7, 10, 14 } }, { "mb5", "menor b5 (semidisminuido)", { 0, 3, 6, 10 } },
        };
        return q;
    }
    struct Chord { juce::ValueTree v; double beat = 0, len = 4; int root = 0; int quality = 0; double end() const { return beat + len; } juce::String name() const { return juce::String (noteName (root)) + qualities()[(size_t) quality].sym; } };
    inline std::vector<int> pitches (const Chord& c, int base = 48) { std::vector<int> o; for (int i : qualities()[(size_t) c.quality].iv) o.push_back (base + ((c.root - base) % 12 + 12) % 12 + i); return o; }

    inline std::vector<Chord> list (te::Edit& e)
    {
        std::vector<Chord> out;
        for (auto c : e.state) if (c.hasType ("INJCHORD")) out.push_back ({ c, (double) c.getProperty ("beat", 0.0), std::max (0.25, (double) c.getProperty ("len", 4.0)), juce::jlimit (0, 11, (int) c.getProperty ("root", 0)), juce::jlimit (0, (int) qualities().size() - 1, (int) c.getProperty ("q", 0)) });
        std::sort (out.begin(), out.end(), [] (const Chord& a, const Chord& b) { return a.beat < b.beat; });
        return out;
    }
    inline int indexAt (te::Edit& e, double beat) { auto l = list (e); for (int i = 0; i < (int) l.size(); ++i) if (beat >= l[(size_t) i].beat - 1e-9 && beat < l[(size_t) i].end() - 1e-9) return i; return -1; }
    // "Am", "F#m7", "Bb5", "Csus4"... Devuelve false si no se entiende.
    inline bool parse (juce::String s, int& root, int& quality)
    {
        s = s.trim(); if (s.isEmpty()) return false; const auto L = s[0]; static const int base[] { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G
        const auto up = juce::CharacterFunctions::toUpperCase (L); if (up < 'A' || up > 'G') return false;
        int r = base[up - 'A']; int i = 1;
        if (i < s.length() && (s[i] == '#' || s[i] == (juce::juce_wchar) 0x266f)) { ++r; ++i; } else if (i < s.length() && (s[i] == 'b' || s[i] == (juce::juce_wchar) 0x266d)) { --r; ++i; }
        const auto rest = s.substring (i); int best = -1; size_t bestLen = 0;
        for (size_t q = 0; q < qualities().size(); ++q) { const juce::String sym (qualities()[q].sym); if (rest == sym && sym.length() >= bestLen) { best = (int) q; bestLen = (size_t) sym.length(); } }
        if (best < 0 && (rest == "min" || rest == "-")) best = 1; if (best < 0 && rest == "maj") best = 0; if (best < 0) return false;
        root = ((r % 12) + 12) % 12; quality = best; return true;
    }
    inline bool add (te::Edit& e, double beat, double len, int root, int quality)
    {
        beat = std::max (0.0, beat); if (len < 0.25) return false; if (indexAt (e, beat) >= 0) return false;
        for (auto& c : list (e)) if (c.beat > beat + 1e-9) { len = std::min (len, c.beat - beat); break; }
        if (len < 0.25 - 1e-9) return false;
        juce::ValueTree v ("INJCHORD"); v.setProperty ("beat", beat, nullptr); v.setProperty ("len", len, nullptr); v.setProperty ("root", root, nullptr); v.setProperty ("q", quality, nullptr);
        e.getUndoManager().beginNewTransaction ("Nuevo acorde"); e.state.addChild (v, -1, &e.getUndoManager()); return true;
    }
    inline void remove (te::Edit& e, int i) { auto l = list (e); if (i < 0 || i >= (int) l.size()) return; e.getUndoManager().beginNewTransaction ("Quitar acorde"); e.state.removeChild (l[(size_t) i].v, &e.getUndoManager()); }
    inline void clearAll (te::Edit& e) { auto l = list (e); if (l.empty()) return; e.getUndoManager().beginNewTransaction ("Quitar acordes"); for (auto& c : l) e.state.removeChild (c.v, &e.getUndoManager()); }
    inline bool set (te::Edit& e, int i, int root, int quality) { auto l = list (e); if (i < 0 || i >= (int) l.size()) return false; e.getUndoManager().beginNewTransaction ("Editar acorde"); l[(size_t) i].v.setProperty ("root", root, &e.getUndoManager()); l[(size_t) i].v.setProperty ("q", quality, &e.getUndoManager()); return true; }
    inline double setLen (te::Edit& e, int i, double len) { auto l = list (e); if (i < 0 || i >= (int) l.size()) return 0; len = std::max (0.25, len); if (i + 1 < (int) l.size()) len = std::min (len, l[(size_t) i + 1].beat - l[(size_t) i].beat); e.getUndoManager().beginNewTransaction ("Duraci\xc3\xb3n del acorde"); l[(size_t) i].v.setProperty ("len", len, &e.getUndoManager()); return len; }
    inline bool moveTo (te::Edit& e, int i, double beat)
    {
        auto l = list (e); if (i < 0 || i >= (int) l.size()) return false; beat = std::max (0.0, beat); const double len = l[(size_t) i].len;
        for (int k = 0; k < (int) l.size(); ++k) if (k != i && beat < l[(size_t) k].end() - 1e-9 && beat + len > l[(size_t) k].beat + 1e-9) return false;
        e.getUndoManager().beginNewTransaction ("Mover acorde"); l[(size_t) i].v.setProperty ("beat", beat, &e.getUndoManager()); return true;
    }

    // Acorde por nombre de notas: busca raíz y calidad cuyo conjunto de notas coincida exactamente (con las notas presentes, sin importar la octava).
    inline bool detectChord (const std::vector<int>& notes, int& root, int& quality)
    {
        std::array<bool, 12> has {}; int n = 0; for (int p : notes) { const int pc = ((p % 12) + 12) % 12; if (! has[(size_t) pc]) { has[(size_t) pc] = true; ++n; } }
        if (n < 2) return false;
        int bestScore = -1;
        for (int r = 0; r < 12; ++r) for (size_t q = 0; q < qualities().size(); ++q)
        {
            std::array<bool, 12> want {}; int cnt = 0; for (int i : qualities()[q].iv) { const int pc = (r + i) % 12; if (! want[(size_t) pc]) { want[(size_t) pc] = true; ++cnt; } }
            if (want != has) continue;
            const int score = 100 - (int) q;   // si hay empate gana la calidad más común (primera de la lista)
            if (score > bestScore) { bestScore = score; root = r; quality = (int) q; }
        }
        return bestScore >= 0;
    }

    // Tonalidad probable de un conjunto de notas (perfiles de Krumhansl). Devuelve false si no hay notas.
    inline bool detectKey (const std::vector<int>& notes, Key& out)
    {
        if (notes.empty()) return false;
        static const double maj[] { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 }, mnr[] { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
        double h[12] {}; for (int p : notes) h[((p % 12) + 12) % 12] += 1.0;
        auto corr = [&] (const double* prof, int r) { double mh = 0, mp = 0; for (int i = 0; i < 12; ++i) { mh += h[i]; mp += prof[i]; } mh /= 12; mp /= 12; double num = 0, dh = 0, dp = 0; for (int i = 0; i < 12; ++i) { const double a = h[(i + r) % 12] - mh, b = prof[i] - mp; num += a * b; dh += a * a; dp += b * b; } return dh <= 0 || dp <= 0 ? -2.0 : num / std::sqrt (dh * dp); };
        double best = -3; for (int r = 0; r < 12; ++r) { const double a = corr (maj, r), b = corr (mnr, r); if (a > best) { best = a; out = { r, 2 }; } if (b > best) { best = b; out = { r, 1 }; } }
        return best > -2;
    }

    // Mínimo desplazamiento (en semitonos, de -6 a +5) que deja más notas dentro de la escala de la tonalidad. En empate gana el menor movimiento.
    inline int bestShift (Key k, const std::vector<int>& notes)
    {
        if (! k.active() || notes.empty()) return 0; int best = 0, bestIn = -1;
        for (int s = -6; s <= 5; ++s) { int in = 0; for (int p : notes) in += inScale (k, p + s) ? 1 : 0; if (in > bestIn || (in == bestIn && std::abs (s) < std::abs (best))) { bestIn = in; best = s; } }
        return best;
    }
    // Nota del acorde más cercana a `note` (conserva el registro).
    inline int snapToChord (const Chord& c, int note)
    {
        int best = note, bd = 99; for (int i : qualities()[(size_t) c.quality].iv) { const int pc = (c.root + i) % 12; for (int o = -1; o <= 10; ++o) { const int cand = o * 12 + pc; const int d = std::abs (cand - note); if (d < bd || (d == bd && cand < best)) { bd = d; best = cand; } } }
        return juce::jlimit (0, 127, best);
    }
}
