#pragma once
#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>
#include <vector>

// Herramientas de notas MIDI sin dependencias del motor: arpegiador, humanizar, legato, rampas de velocidad, rasgueo, octavas...
// Trabajan sobre listas de notas (tiempos en pulsos = negras) y devuelven la lista nueva; el clip se reescribe en Main/PlaylistView.
namespace miditools
{
    struct Nt { int pitch = 60; double start = 0, len = 1; int vel = 100; double end() const { return start + len; } };
    using Notes = std::vector<Nt>;

    // ------------------------------------------------------------ arpegiador
    enum Pattern { Up, Down, UpDown, DownUp, Random, AsPlayed, kNumPatterns };
    inline const char* patternName (int p) { static const char* n[] { "Arriba", "Abajo", "Sube y baja", "Baja y sube", "Al azar", "Como se toc\xc3\xb3" }; return n[juce::jlimit (0, kNumPatterns - 1, p)]; }
    inline constexpr int kNumRates = 7;
    inline double rateBeats (int r) { static const double b[] { 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0, 1.0 / 12.0 }; return b[juce::jlimit (0, kNumRates - 1, r)]; }
    inline const char* rateName (int r) { static const char* n[] { "1/4", "1/8", "1/16", "1/32", "1/8 tresillo", "1/16 tresillo", "1/32 tresillo" }; return n[juce::jlimit (0, kNumRates - 1, r)]; }

    struct ArpSet { int pattern = Up, rate = 2, octaves = 1, gate = 80, swing = 0, seed = 1; };   // gate y swing en % del paso
    inline ArpSet& arpSettings() { static ArpSet s; return s; }   // últimos ajustes usados (los comandos rápidos los reutilizan)

    // Agrupa las notas en acordes: las que empiezan casi a la vez (0.06 pulsos) son un mismo acorde.
    inline std::vector<Notes> chordsOf (Notes n)
    {
        std::stable_sort (n.begin(), n.end(), [] (const Nt& a, const Nt& b) { return a.start < b.start; });
        std::vector<Notes> out; for (auto& x : n) { if (out.empty() || x.start - out.back().front().start > 0.06) out.push_back ({}); out.back().push_back (x); }
        return out;
    }

    // Orden de tonos de un arpegio (sin repetir el extremo en sube-y-baja).
    inline std::vector<int> arpOrder (Notes chord, const ArpSet& s, int chordIdx)
    {
        std::vector<int> played; for (auto& x : chord) played.push_back (x.pitch);
        std::vector<int> sorted = played; std::sort (sorted.begin(), sorted.end()); sorted.erase (std::unique (sorted.begin(), sorted.end()), sorted.end());
        const int oct = juce::jlimit (1, 4, s.octaves); std::vector<int> base = s.pattern == AsPlayed ? played : sorted, full;
        for (int o = 0; o < oct; ++o) for (int p : base) if (p + 12 * o <= 127) full.push_back (p + 12 * o);
        if (full.empty()) return full;
        std::vector<int> r = full, rev (full.rbegin(), full.rend());
        switch (s.pattern)
        {
            case Down: return rev;
            case UpDown: { if (full.size() > 2) r.insert (r.end(), rev.begin() + 1, rev.end() - 1); return r; }
            case DownUp: { r = rev; if (full.size() > 2) r.insert (r.end(), full.begin() + 1, full.end() - 1); return r; }
            case Random: { juce::Random rnd (s.seed * 7919 + chordIdx * 131 + 7); std::vector<int> o; int last = -1; for (size_t i = 0; i < 64; ++i) { int k = rnd.nextInt ((int) full.size()); if (full.size() > 1 && k == last) k = (k + 1) % (int) full.size(); last = k; o.push_back (full[(size_t) k]); } return o; }
            default: return r;   // Up, AsPlayed
        }
    }

    // Convierte cada acorde en un arpegio que dura lo mismo que el acorde. Devuelve las notas nuevas (máximo 4096).
    inline Notes arpeggiate (const Notes& in, const ArpSet& s, double clipLen)
    {
        Notes out; const double step = rateBeats (s.rate); int ci = 0;
        for (auto& ch : chordsOf (in))
        {
            double a = ch.front().start, b = a; int vmax = 1; for (auto& x : ch) { b = std::max (b, x.end()); vmax = std::max (vmax, x.vel); }
            auto ord = arpOrder (ch, s, ci++); if (ord.empty()) continue;
            const int steps = std::max (1, (int) std::floor ((b - a) / step + 1e-6));
            for (int i = 0; i < steps; ++i)
            {
                double st = a + i * step; if ((i % 2) == 1) st += step * juce::jlimit (0, 75, s.swing) / 100.0;
                if (st >= clipLen - 1e-9 || (int) out.size() >= 4096) break;
                Nt n; n.pitch = ord[(size_t) i % ord.size()]; n.start = st; n.len = std::max (1.0 / 64.0, step * juce::jlimit (5, 100, s.gate) / 100.0); n.len = std::min (n.len, clipLen - st); n.vel = vmax;
                out.push_back (n);
            }
        }
        return out;
    }

    // ------------------------------------------------------------ otras herramientas
    // Humanizar: mueve cada nota un poco al azar (tiempo en pulsos, velocidad en pasos).
    inline Notes humanize (Notes n, double timing, int vel, int seed, double clipLen)
    {
        juce::Random r (seed * 104729 + 3);
        for (auto& x : n) { x.start = juce::jlimit (0.0, std::max (0.0, clipLen - 1.0 / 64.0), x.start + (r.nextDouble() * 2.0 - 1.0) * timing); x.vel = juce::jlimit (1, 127, x.vel + (int) std::lround ((r.nextDouble() * 2.0 - 1.0) * vel)); x.len = std::min (x.len, std::max (1.0 / 64.0, clipLen - x.start)); }
        return n;
    }
    // Legato: cada nota dura hasta que empieza la siguiente (la última no cambia).
    inline Notes legato (Notes n)
    {
        for (auto& x : n) { double nx = 1e18; for (auto& y : n) if (y.start > x.start + 0.06) nx = std::min (nx, y.start); if (nx < 1e17) x.len = std::max (1.0 / 64.0, nx - x.start); }
        return n;
    }
    // Rampa de velocidad: de `from` a `to` según el orden de las notas en el tiempo.
    inline Notes velRamp (Notes n, int from, int to)
    {
        if (n.empty()) return n; double a = 1e18, b = -1e18; for (auto& x : n) { a = std::min (a, x.start); b = std::max (b, x.start); }
        for (auto& x : n) { const double t = b - a < 1e-9 ? 0.0 : (x.start - a) / (b - a); x.vel = juce::jlimit (1, 127, (int) std::lround (from + (to - from) * t)); }
        return n;
    }
    // Rasgueo: en cada acorde, las notas entran una tras otra de grave a agudo, separadas `gap` pulsos, y terminan cuando terminaba el acorde.
    inline Notes strum (const Notes& in, double gap, bool down = true)
    {
        Notes out;
        for (auto& ch : chordsOf (in)) { auto c = ch; std::stable_sort (c.begin(), c.end(), [down] (const Nt& a, const Nt& b) { return down ? a.pitch < b.pitch : a.pitch > b.pitch; });
            for (size_t i = 0; i < c.size(); ++i) { Nt x = c[i]; const double e = x.end(); x.start += gap * (double) i; x.len = std::max (1.0 / 64.0, e - x.start); out.push_back (x); } }
        return out;
    }
    // Repetir cada nota `times` veces dentro de su duración (efecto de redoble, como un "ratchet").
    inline Notes ratchet (const Notes& in, int times, double clipLen)
    {
        Notes out; times = juce::jlimit (2, 8, times);
        for (auto& x : in) { const double p = x.len / times; for (int i = 0; i < times && (int) out.size() < 4096; ++i) { Nt y = x; y.start = x.start + i * p; y.len = std::max (1.0 / 64.0, p * 0.85); if (y.start < clipLen - 1e-9) out.push_back (y); } }
        return out;
    }
    // Una misma nota no puede sonar encima de sí misma: se acorta la anterior donde empieza la siguiente.
    inline Notes noOverlap (Notes n)
    {
        for (auto& x : n) for (auto& y : n) if (&x != &y && x.pitch == y.pitch && y.start > x.start + 1e-9 && y.start < x.end() - 1e-9) x.len = std::max (1.0 / 64.0, y.start - x.start);
        return n;
    }
    inline Notes fixedLen (Notes n, double len) { for (auto& x : n) x.len = len; return n; }
    // Doblar con otra voz: suma (o resta) `semi` semitonos como notas añadidas (octava, quinta...).
    inline Notes addVoice (const Notes& in, int semi) { Notes out = in; for (auto& x : in) if (x.pitch + semi >= 0 && x.pitch + semi <= 127 && (int) out.size() < 4096) { Nt y = x; y.pitch += semi; out.push_back (y); } return out; }
    // Inversión melódica: refleja los tonos alrededor de la nota media.
    inline Notes mirrorPitch (Notes n)
    {
        if (n.empty()) return n; int lo = 127, hi = 0; for (auto& x : n) { lo = std::min (lo, x.pitch); hi = std::max (hi, x.pitch); }
        for (auto& x : n) x.pitch = juce::jlimit (0, 127, lo + hi - x.pitch);
        return n;
    }
    // Quedarse con una de cada dos notas (o la mitad más fuerte).
    inline Notes thin (const Notes& in) { auto s = in; std::stable_sort (s.begin(), s.end(), [] (const Nt& a, const Nt& b) { return a.start < b.start; }); Notes o; for (size_t i = 0; i < s.size(); i += 2) o.push_back (s[i]); return o; }
    // Swing de semicorcheas: las notas en la segunda semicorchea de cada corchea se retrasan `amount` (0..0.75) de una semicorchea.
    inline Notes swing16 (Notes n, double amount)
    {
        for (auto& x : n) { const double k = std::fmod (x.start, 0.5); if (std::abs (k - 0.25) < 0.02) x.start += 0.25 * amount; }
        return n;
    }
    // ---- herramientas de notas extra ----
    inline Notes transpose (Notes n, int semi) { Notes o; for (auto x : n) { x.pitch += semi; if (x.pitch >= 0 && x.pitch <= 127) o.push_back (x); } return o; }
    inline Notes velScale (Notes n, double k) { for (auto& x : n) x.vel = juce::jlimit (1, 127, (int) std::lround (x.vel * k)); return n; }
    // acerca la fuerza de todas las notas hacia 90 (mitad de camino): parejas sin perder el matiz
    inline Notes velSquash (Notes n) { for (auto& x : n) x.vel = juce::jlimit (1, 127, (int) std::lround (x.vel + (90 - x.vel) * 0.5)); return n; }
    inline Notes lenScale (Notes n, double k) { for (auto& x : n) x.len = std::max (1.0 / 64.0, x.len * k); return n; }
    inline Notes shiftBeats (Notes n, double d) { Notes o; for (auto x : n) { x.start += d; if (x.start >= 0.0) o.push_back (x); } return o; }
    // apila un acorde sobre cada nota con los intervalos dados (en semitonos)
    inline Notes stack (const Notes& in, std::initializer_list<int> iv)
    {
        Notes o = in; for (auto& x : in) for (int i : iv) if (x.pitch + i >= 0 && x.pitch + i <= 127 && (int) o.size() < 4096) { Nt y = x; y.pitch += i; o.push_back (y); } return o;
    }
    // deja una sola nota por golpe: la más aguda (top) o la más grave
    inline Notes keepExtreme (const Notes& in, bool top)
    {
        Notes o; for (auto& x : in) { bool best = true; for (auto& y : in) if (std::abs (y.start - x.start) < 1e-4 && (top ? y.pitch > x.pitch : y.pitch < x.pitch)) best = false; if (best) o.push_back (x); } return o;
    }
    // acento: las notas que caen en un tiempo suben a 120 y las demás bajan un 15 %
    inline Notes accent (Notes n) { for (auto& x : n) { const bool beat = std::abs (x.start - std::round (x.start)) < 0.02; x.vel = beat ? 120 : juce::jlimit (1, 127, (int) std::lround (x.vel * 0.85)); } return n; }
    // notas fantasma: las semicorcheas que no caen en corchea bajan a 45
    inline Notes ghost (Notes n) { for (auto& x : n) { const double k = std::fmod (x.start, 0.5); if (std::abs (k - 0.25) < 0.02) x.vel = std::min (x.vel, 45); } return n; }
    // lleva las notas a la nota más cercana de la escala (pasos en semitonos desde la raíz)
    inline Notes toScaleSteps (Notes n, int root, const std::vector<int>& steps)
    {
        for (auto& x : n) { int best = x.pitch, bd = 99; for (int d = -6; d <= 6; ++d) { const int p = x.pitch + d, pc = ((p - root) % 12 + 12) % 12; for (int st : steps) if (st == pc && std::abs (d) < bd) { bd = std::abs (d); best = p; } } x.pitch = juce::jlimit (0, 127, best); }
        return n;
    }
    // notas al revés en el tiempo no: aquí "medio tiempo" separa las posiciones al doble sin cambiar la duración
    inline Notes halfTime (Notes n) { for (auto& x : n) x.start *= 2.0; return n; }
    inline Notes doubleTime (Notes n) { for (auto& x : n) x.start *= 0.5; return n; }
}
