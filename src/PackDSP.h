#pragma once
// Síntesis del paquete de sonidos INJUSTICE (C++ puro, sin JUCE).
// Todo se genera a 88.2 kHz (sobremuestreo x2) con osciladores polyBLEP y filtros biquad RBJ,
// y al final se decima a 44.1 kHz con un FIR de sinc ventaneada (Blackman) para que no haya aliasing.
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>

namespace packdsp
{
    constexpr double OUT = 44100.0, OS = 2.0, FS = OUT * OS, TAU = 6.283185307179586;
    using Buf = std::vector<float>;

    struct Rng { uint32_t s; explicit Rng (uint32_t seed) : s (seed * 2654435761u + 12345u) {} float next() { s = s * 1664525u + 1013904223u; return (float) (s >> 8) * (1.f / 8388608.f) - 1.f; } };
    inline uint32_t& vseed() { static uint32_t v = 0; return v; }   // cada generador de ruido usa una semilla distinta: dos claps o hats nunca comparten la misma forma de onda
    inline Buf make (double secs) { return Buf ((size_t) (secs * FS), 0.f); }
    inline double ex (double t, double tau) { return std::exp (-t / tau); }
    inline double V (int i, double salt) { const double x = i * 0.6180339887 + salt * 0.3819660113; return x - std::floor (x); }
    inline double mx (double a, double b, double v) { return a + (b - a) * v; }
    inline double mtof (double m) { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }

    // Biquad RBJ (0 = pasa-bajos, 1 = pasa-altos, 2 = pasa-banda)
    struct Bq
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void set (int type, double fc, double q)
        {
            fc = std::min (fc, FS * 0.45); fc = std::max (fc, 10.0);
            const double w = TAU * fc / FS, c = std::cos (w), al = std::sin (w) / (2.0 * q), a0 = 1.0 + al;
            if (type == 0) { b0 = (1 - c) / 2; b1 = 1 - c; b2 = (1 - c) / 2; }
            else if (type == 1) { b0 = (1 + c) / 2; b1 = -(1 + c); b2 = (1 + c) / 2; }
            else { b0 = al; b1 = 0; b2 = -al; }
            a1 = -2 * c; a2 = 1 - al; b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
        }
        float tick (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return (float) y; }
    };
    inline void run (Buf& b, int type, double fc, double q) { Bq f; f.set (type, fc, q); for (auto& x : b) x = f.tick (x); }

    // polyBLEP
    inline double pb (double t, double dt) { if (t < dt) { t /= dt; return t + t - t * t - 1.0; } if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; } return 0.0; }
    inline double sawv (double p, double dt) { return 2.0 * p - 1.0 - pb (p, dt); }
    inline double sqv (double p, double dt) { double q = p + 0.5; q -= std::floor (q); return sawv (p, dt) - sawv (q, dt); }

    // ---------- Percusión ----------
    inline Buf kick (double f0, double f1, double pd, double ad, double drive, double click, double len, double post = 1.0, int shape = 0, double lp = 11000.0)
    {
        auto b = make (len); double ph = 0; Rng r (7u + 977u * vseed()++); const double nd = std::tanh (drive); Bq cl; cl.set (2, 3200, 0.7);
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / FS, f = f1 + (f0 - f1) * ex (t, pd); ph += TAU * f / FS;
            const double env = ex (t, ad) * std::min (1.0, t / 0.0005);
            const double x = std::sin (ph) * drive; double sh;
            switch (shape) { case 1: sh = std::max (-1.0, std::min (1.0, x)) * nd; break;                       // recorte duro
                             case 2: sh = std::sin (x * 1.1) * nd; break;                                          // pliegue de onda
                             case 3: sh = (std::tanh (x + 0.35 * std::abs (x)) - 0.12) * nd; break;                // asimétrica
                             default: sh = std::tanh (x); }
            double s = sh / nd * env;
            s += click * cl.tick (r.next()) * ex (t, 0.0025) * 2.0;
            b[i] = (float) s;
        }
        run (b, 0, lp, 0.707); if (post > 1.0) for (auto& x : b) x = (float) std::tanh (x * post);
        return b;
    }
    inline Buf snare (double tone, double td, double nd, double noiseAmt, double hpf, double drive, double len = 0.5)
    {
        auto b = make (len); Rng r (11u + 977u * vseed()++); double p1 = 0, p2 = 0; Buf n = make (len);
        for (auto& x : n) x = r.next();
        run (n, 1, hpf, 0.7); run (n, 0, 10500, 0.7);
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / FS, bend = 1.0 + 0.35 * ex (t, 0.012);
            p1 += TAU * tone * bend / FS; p2 += TAU * tone * 1.83 * bend / FS;
            const double tones = (0.7 * std::sin (p1) + 0.4 * std::sin (p2)) * ex (t, td);
            const double nz = n[i] * ex (t, nd) * std::min (1.0, t / 0.0003);
            b[i] = (float) std::tanh ((tones + noiseAmt * nz) * drive);
        }
        return b;
    }
    inline Buf clap (double tail, double centre, int bursts, double spacing, double q, double drive)
    {
        const double len = std::min (1.4, bursts * spacing + tail * 4.0 + 0.02);
        auto b = make (len); Rng r (23u + 977u * vseed()++); Buf n2 = make (len);
        for (size_t i = 0; i < b.size(); ++i) { const float v = r.next(); b[i] = v; n2[i] = v; }
        run (b, 2, centre, q); run (n2, 2, centre * 2.2, 1.0);
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / FS; double e = 0;
            for (int k = 0; k < bursts; ++k) { const double st = k * spacing; if (t >= st && t < st + spacing) e = 0.85 * ex (t - st, 0.0035); }
            if (t >= bursts * spacing) e = ex (t - bursts * spacing, tail);
            b[i] = (float) std::tanh ((b[i] + 0.45 * n2[i]) * e * 2.0 * drive);
        }
        return b;
    }
    // Núcleo metálico: seis cuadradas (las frecuencias del circuito 808/909)
    inline Buf metal (double len, double scale)
    {
        static const double fr[6] { 205.3, 304.4, 369.6, 522.7, 540.0, 800.0 };
        auto b = make (len); double ph[6] { 0, 0, 0, 0, 0, 0 };
        for (size_t i = 0; i < b.size(); ++i)
        {
            double s = 0;
            for (int k = 0; k < 6; ++k) { const double dt = fr[k] * scale / FS; ph[k] += dt; if (ph[k] >= 1.0) ph[k] -= 1.0; s += sqv (ph[k], dt); }
            b[i] = (float) (s / 6.0);
        }
        return b;
    }
    // metalAmt: cuánto núcleo metálico (808 = 1, 909 = poco: el 909 es casi todo ruido). grit: saturación ligera (crujido digital del 909).
    inline Buf hat (double decay, double scale, double hpf, double bpc, double nm, double metalAmt = 1.0, double grit = 1.0, double lpc = 20000.0)
    {
        const double len = std::min (2.0, 0.02 + decay * 5.0);
        auto b = metal (len, scale); Rng r (31u + 977u * vseed()++);
        for (auto& x : b) x = (float) (metalAmt * x + nm * r.next());
        run (b, 2, bpc, 0.6); run (b, 1, hpf, 0.75); run (b, 1, hpf, 0.75);
        if (grit > 1.0) for (auto& x : b) x = (float) std::tanh (x * grit);
        if (lpc < 19000.0) { run (b, 0, lpc, 0.7); run (b, 0, lpc, 0.7); }
        for (size_t i = 0; i < b.size(); ++i) { const double t = (double) i / FS; b[i] *= (float) (std::min (1.0, t / 0.0003) * (0.55 * ex (t, decay * 0.3) + 0.45 * ex (t, decay))); }
        return b;
    }
    inline Buf hat909 (double decay, double hpf, double bpc, double tone, double metalAmt = 0.28, double grit = 1.8, double lpc = 12500.0) { return hat (decay, tone, hpf, bpc, 1.0, metalAmt, grit, lpc); }
    inline Buf crash (double decay, double scale, double hpf)
    {
        const double len = std::min (4.0, decay * 3.5 + 0.1);
        auto b = metal (len, scale); Rng r (37u + 977u * vseed()++);
        for (auto& x : b) x = x * 0.8f + (float) (0.35 * r.next());
        run (b, 1, hpf, 0.7); run (b, 1, hpf, 0.7);
        for (size_t i = 0; i < b.size(); ++i) { const double t = (double) i / FS; b[i] *= (float) (std::min (1.0, t / 0.001) * (0.4 * ex (t, 0.1) + 0.6 * ex (t, decay))); }
        return b;
    }
    inline Buf ride (double decay, double scale)
    {
        const double len = std::min (3.0, decay * 4.0 + 0.1);
        auto b = metal (len, scale); Rng r (41u + 977u * vseed()++); double ph = 0;
        for (size_t i = 0; i < b.size(); ++i) { const double t = (double) i / FS; ph += TAU * 3200.0 * scale / FS; b[i] = (float) (b[i] * 0.7 + 0.12 * r.next() + 0.35 * std::sin (ph) * ex (t, decay * 0.6)); }
        run (b, 2, 7000, 0.6); run (b, 1, 4500, 0.7);
        for (size_t i = 0; i < b.size(); ++i) { const double t = (double) i / FS; b[i] *= (float) (std::min (1.0, t / 0.0006) * (0.35 * ex (t, 0.05) + 0.65 * ex (t, decay))); }
        return b;
    }
    inline Buf tom (double f0, double f1, double dec)
    {
        auto b = make (dec * 5.0 + 0.1); double ph = 0; Rng r (51u + 977u * vseed()++); Bq cl; cl.set (2, 2500, 0.8);
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / FS, f = f1 + (f0 - f1) * ex (t, 0.035); ph += TAU * f / FS;
            b[i] = (float) (std::tanh (1.4 * (std::sin (ph) + 0.15 * std::sin (2 * ph))) * ex (t, dec) * std::min (1.0, t / 0.0005) + 0.25 * cl.tick (r.next()) * ex (t, 0.003));
        }
        return b;
    }
    inline Buf tomDist (double f0, double f1, double dec, double drive) { auto b = tom (f0, f1, dec); for (auto& x : b) x = (float) std::tanh (x * drive); run (b, 0, 7000, 0.7); return b; }
    inline Buf rim (double f1, double f2)
    {
        auto b = make (0.18); Rng r (61u + 977u * vseed()++); double p1 = 0, p2 = 0; Bq bp; bp.set (2, 4200, 0.9);
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / FS; p1 += TAU * f1 / FS; p2 += TAU * f2 / FS;
            b[i] = (float) (std::tanh (1.5 * (0.6 * std::sin (p1) + 0.6 * std::sin (p2) + 0.5 * bp.tick (r.next()))) * ex (t, 0.011) * std::min (1.0, t / 0.0002));
        }
        run (b, 1, 300, 0.7); return b;
    }
    inline Buf cowbell (double scale, double decay)
    {
        auto b = make (0.8); double p1 = 0, p2 = 0;
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / FS, d1 = 540.0 * scale / FS, d2 = 800.0 * scale / FS; p1 += d1; p2 += d2; p1 -= std::floor (p1); p2 -= std::floor (p2);
            b[i] = (float) ((sqv (p1, d1) + sqv (p2, d2)) * (0.35 * ex (t, 0.012) + 0.65 * ex (t, decay)) * std::min (1.0, t / 0.0005));
        }
        run (b, 2, 2600 * scale, 1.2); run (b, 2, 2600 * scale, 1.2); run (b, 0, 5500, 0.7); return b;
    }
    inline Buf conga (double f0, double dec)
    {
        auto b = make (dec * 5.0 + 0.1); double ph = 0; Rng r (71u + 977u * vseed()++); Bq cl; cl.set (2, 1800, 1.0);
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / FS, f = f0 * (1.0 + 0.25 * ex (t, 0.012)); ph += TAU * f / FS;
            b[i] = (float) (std::sin (ph) * ex (t, dec) * std::min (1.0, t / 0.0004) + 0.3 * cl.tick (r.next()) * ex (t, 0.004));
        }
        return b;
    }
    inline Buf clave (double f)
    {
        auto b = make (0.25); double ph = 0;
        for (size_t i = 0; i < b.size(); ++i) { const double t = (double) i / FS; ph += TAU * f / FS; b[i] = (float) (std::sin (ph) * ex (t, 0.03) * std::min (1.0, t / 0.0003)); }
        return b;
    }
    inline Buf shaker (double decay, double centre, double attack)
    {
        const double len = std::min (1.5, decay * 5.0 + 0.05); auto b = make (len); Rng r (81u + 977u * vseed()++);
        for (auto& x : b) x = r.next();
        run (b, 2, centre, 0.8); run (b, 1, 3000, 0.7);
        for (size_t i = 0; i < b.size(); ++i) { const double t = (double) i / FS; b[i] *= (float) (std::min (1.0, t / attack) * ex (t, decay)); }
        return b;
    }
    inline Buf tambourine (double decay)
    {
        auto b = make (decay * 5.0 + 0.1); Rng r (91u + 977u * vseed()++);
        for (auto& x : b) x = r.next();
        run (b, 1, 5500, 0.7); run (b, 1, 5500, 0.7);
        for (size_t i = 0; i < b.size(); ++i) { const double t = (double) i / FS; b[i] *= (float) (ex (t, decay) + 0.5 * (t >= 0.018 ? ex (t - 0.018, decay * 0.8) : 0.0)) * (float) std::min (1.0, t / 0.0004); }
        return b;
    }

    // ---------- Sintes ----------
    // Voz de sierras con desafine: acorde (semitonos), voces por nota, glide de afinación y filtro con barrido.
    struct VoiceSpec { double f; std::vector<double> chord; int voices; double spread, glide, gtau, a, dtau, floor, len, rel, cutoff, q, sweep, drive; };
    inline Buf synthVoice (const VoiceSpec& s)
    {
        auto b = make (s.len); const int nv = (int) s.chord.size() * s.voices; std::vector<double> ph ((size_t) nv, 0.0); Bq lp; lp.set (0, s.cutoff, s.q);
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / FS, g = std::pow (2.0, s.glide * ex (t, s.gtau) / 12.0); double sum = 0; int k = 0;
            for (double c : s.chord)
                for (int v = 0; v < s.voices; ++v, ++k)
                {
                    const double cents = s.voices == 1 ? 0.0 : s.spread * (2.0 * v / (s.voices - 1) - 1.0);
                    const double dt = s.f * g * std::pow (2.0, c / 12.0 + cents / 1200.0) / FS; ph[(size_t) k] += dt; ph[(size_t) k] -= std::floor (ph[(size_t) k]);
                    sum += sawv (ph[(size_t) k], dt);
                }
            if ((i & 15) == 0) lp.set (0, s.cutoff * (1.0 + s.sweep * ex (t, 0.18)), s.q);
            const double amp = std::min (1.0, t / s.a) * (s.floor + (1.0 - s.floor) * ex (t, s.dtau)) * (t < s.len - s.rel ? 1.0 : std::max (0.0, (s.len - t) / s.rel));
            b[i] = (float) (std::tanh (lp.tick (sum / (double) nv * 1.2) * s.drive) * amp);
        }
        return b;
    }
    inline Buf bass (int type, int midi)
    {
        const double f = mtof (midi), len = type == 3 ? 1.6 : (type == 5 ? 0.6 : 1.1); auto b = make (len); Bq lp; double p[3] { 0, 0, 0 }, ps = 0; const double det[3] { 0.9948, 1.0, 1.0052 };
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / FS, rel = std::min (1.0, std::min (t / 0.004, (len - t) / 0.12)); double s = 0;
            if (type == 0) { ps += TAU * f / FS; s = std::tanh (1.6 * std::sin (ps)) * (0.75 + 0.25 * ex (t, 0.25)); }
            else if (type == 1)
            {
                for (int k = 0; k < 3; ++k) { const double dt = f * det[k] / FS; p[k] += dt; p[k] -= std::floor (p[k]); s += sawv (p[k], dt); }
                ps += TAU * f / FS; if ((i & 31) == 0) lp.set (0, 450, 1.1);
                s = std::tanh (1.6 * (lp.tick (s / 3.0 * 1.4) + 0.5 * std::sin (ps)));
            }
            else if (type == 2)
            {
                const double dt = f / FS; p[0] += dt; p[0] -= std::floor (p[0]); if ((i & 15) == 0) lp.set (0, 180 + 2400 * ex (t, 0.16), 7.0);
                s = std::tanh (1.5 * lp.tick (sawv (p[0], dt))) * ex (t, 0.5);
            }
            else if (type == 3) { ps += TAU * f * (1.0 + 1.2 * ex (t, 0.03)) / FS; s = std::tanh (2.0 * std::sin (ps)) * ex (t, 0.85); }
            else if (type == 5)
            {
                const double fi = f * (1.0 + 0.5 * ex (t, 0.02)), dt = fi / FS; p[0] += dt; p[0] -= std::floor (p[0]); if ((i & 31) == 0) lp.set (0, 1200, 2.0);
                s = std::tanh (1.4 * lp.tick (sqv (p[0], dt))) * ex (t, 0.18);
            }
            else if (type == 6)
            {
                const double dt = f / FS; p[0] += dt; p[0] -= std::floor (p[0]); if ((i & 31) == 0) lp.set (0, 900, 1.0);
                s = lp.tick (std::tanh (6.0 * sawv (p[0], dt))) * (0.6 + 0.4 * ex (t, 0.5));
            }
            else { ps += TAU * f * (1.0 + 0.8 * ex (t, 0.06)) / FS; if ((i & 31) == 0) lp.set (0, 2200, 0.7); s = lp.tick (std::tanh (8.0 * std::sin (ps))) * ex (t, 0.4); }
            b[i] = (float) (s * rel);
        }
        return b;
    }

    // ---------- FX ----------
    inline Buf riser (double len, double f0, double f1, bool up, int seed)
    {
        auto b = make (len); Rng r (seed); Bq bp; double ph = 0;
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double u = (double) i / (double) b.size(), w = up ? u : 1.0 - u, fc = f0 * std::pow (f1 / f0, w);
            if ((i & 15) == 0) bp.set (2, fc, 2.5);
            ph += TAU * (100.0 + 1500.0 * w * w) / FS; const double env = up ? u * u : std::pow (1.0 - u, 1.5);
            b[i] = (float) ((bp.tick (r.next()) * 1.5 + 0.12 * std::sin (ph)) * env);
        }
        return b;
    }
    inline Buf impact (double len, double f0, int seed)
    {
        auto b = make (len); Rng r (seed); double ph = 0; Bq lp; lp.set (0, 900, 0.7);
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / FS; ph += TAU * (f0 * 0.55 + f0 * ex (t, 0.12)) / FS;
            b[i] = (float) (std::sin (ph) * ex (t, len * 0.35) + 0.6 * lp.tick (r.next()) * ex (t, len * 0.18));
        }
        return b;
    }
    inline Buf laser (double f0, double f1, double tau, double fm)
    {
        auto b = make (0.6); double ph = 0, pm = 0;
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / FS, f = f1 + (f0 - f1) * ex (t, tau); pm += TAU * f * 0.5 / FS; ph += TAU * f / FS;
            b[i] = (float) (std::sin (ph + fm * std::sin (pm)) * ex (t, 0.22) * std::min (1.0, t / 0.001));
        }
        return b;
    }
    inline Buf siren (double len, double f0, double f1, double rate)
    {
        auto b = make (len); double ph = 0;
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / FS, m = 0.5 + 0.5 * std::sin (TAU * rate * t - 1.5708); ph += TAU * (f0 + (f1 - f0) * m) / FS;
            b[i] = (float) (std::tanh (1.8 * std::sin (ph)) * std::min (1.0, std::min (t / 0.02, (len - t) / 0.1)));
        }
        return b;
    }
    inline Buf sweepNoise (double len, bool up, int seed, double lo, double hi)
    {
        auto b = make (len); Rng r (seed); Bq f;
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double u = (double) i / (double) b.size(), w = up ? u : 1.0 - u; if ((i & 15) == 0) f.set (1, lo * std::pow (hi / lo, w), 1.2);
            b[i] = f.tick (r.next()) * (float) (std::sin (3.14159265 * std::pow (u, 0.7)));
        }
        return b;
    }

    // ---------- Bucles ----------
    inline void mixInto (Buf& dst, const Buf& src, size_t at, float g) { for (size_t i = 0; i < src.size() && at + i < dst.size(); ++i) dst[at + i] += src[i] * g; }
    inline Buf drumLoop (int kit, int v)
    {
        const double beat = 60.0 / 172.0, step = beat / 4.0; auto out = make (beat * 8.0);
        Buf K, S, C, Hc, Ho, X;
        if (kit == 0) { K = kick (240 + 10 * v, 50, 0.032, 0.34, 1.6, 0.25, 0.7); S = snare (185, 0.07, 0.17, 1.0, 1200, 1.3); C = clap (0.12, 1100, 3, 0.011, 1.1, 1.3); Hc = hat909 (0.04, 7000, 9000, 1.0); Ho = hat909 (0.26, 6500, 9000, 1.0); X = rim (480, 1700); }
        else if (kit == 2) { K = kick (400 + 15 * v, 48, 0.035, 0.28, 6, 0.25, 0.6, 1.5); S = snare (200, 0.08, 0.15, 0.85, 800, 3.0); C = clap (0.09, 1000, 3, 0.01, 1.0, 2.0); Hc = hat (0.03, 1.15, 7500, 10000, 0.2, 0.5, 1.5); Ho = hat (0.18, 1.15, 7500, 10000, 0.2, 0.5, 1.5); X = rim (500, 1800); }
        else          { K = kick (520 + 20 * v, 46, 0.04, 0.38, 9, 0.2, 0.7, 1.8); S = snare (215, 0.09, 0.16, 0.9, 700, 4.0); C = clap (0.1, 900, 4, 0.01, 0.9, 3.0); Hc = hat (0.035, 1.1, 6500, 9000, 0.2); Ho = hat (0.2, 1.1, 6500, 9000, 0.2); X = cowbell (1.0, 0.25); }
        for (int s = 0; s < 32; ++s)
        {
            const size_t at = (size_t) (s * step * FS);
            if (s % 4 == 0) mixInto (out, K, at, 1.0f);
            if (s % 16 == 4 || s % 16 == 12) { mixInto (out, C, at, 0.8f); if (v % 3 == 0) mixInto (out, S, at, 0.5f); }
            if (s % 4 == 2) mixInto (out, Ho, at, 0.5f);
            if (v & 2) { if (s % 4 == 1 || s % 4 == 3) mixInto (out, Hc, at, 0.25f); } else if (s % 2 == 1) mixInto (out, Hc, at, 0.3f);
            if ((v & 4) && s >= 28) mixInto (out, K, at, 0.8f);
            if ((v & 8) && s % 16 == 10) mixInto (out, X, at, 0.4f);
            if ((v & 1) && s % 8 == 7) mixInto (out, K, at, 0.7f);
        }
        float pk = 0; for (auto x : out) pk = std::max (pk, std::abs (x)); if (pk > 1e-6f) for (auto& x : out) x = std::tanh (x / pk * 1.1f);
        return out;
    }


    // ---------- Bucles extra (172 BPM, 2 compases) ----------
    inline Buf gated (const Buf& src, double secs)
    {
        const size_t n = std::min (src.size(), (size_t) (secs * FS)); Buf b (src.begin(), src.begin() + (long) n); const size_t f = std::min (n, (size_t) (0.006 * FS));
        for (size_t i = 0; i < f; ++i) b[n - 1 - i] *= (float) i / (float) f; return b;
    }
    inline void softLimit (Buf& out) { float pk = 0; for (auto x : out) pk = std::max (pk, std::abs (x)); if (pk > 1e-6f) for (auto& x : out) x = std::tanh (x / pk * 1.1f); }
    inline Buf hatLoop (int kit, int v)
    {
        const double beat = 60.0 / 172.0, step = beat / 4.0; auto out = make (beat * 8.0); Buf Hc, Ho;
        if (kit == 0) { Hc = hat909 (mx (0.03, 0.05, V (v, 501)), 7000, 9000, 1.0); Ho = hat909 (mx (0.2, 0.32, V (v, 502)), 6500, 9000, 1.0); }
        else { Hc = hat (0.03, 1.15, 7500, 10000, 0.2, 0.5, 1.5); Ho = hat (0.18, 1.15, 7500, 10000, 0.2, 0.5, 1.5); }
        for (int s = 0; s < 32; ++s)
        {
            const size_t at = (size_t) (s * step * FS); const int m = v % 6; const float acc = (s % 4 == 0) ? 1.0f : (s % 2 == 0 ? 0.75f : 0.5f);
            if (m == 0) { if (s % 2 == 0) mixInto (out, Hc, at, acc * 0.6f); if (s % 4 == 2) mixInto (out, Ho, at, 0.5f); }
            else if (m == 1) { mixInto (out, Hc, at, acc * 0.55f); if (s % 8 == 6) mixInto (out, Ho, at, 0.5f); }
            else if (m == 2) { if (s % 4 == 2) mixInto (out, Ho, at, 0.6f); if (s % 4 == 3) mixInto (out, Hc, at, 0.35f); }
            else if (m == 3) { if (s % 2 == 1) mixInto (out, Hc, at, 0.45f); if (s % 8 == 4) mixInto (out, Ho, at, 0.55f); }
            else if (m == 4) { if (s < 24) { if (s % 2 == 0) mixInto (out, Hc, at, 0.55f); } else mixInto (out, Hc, at, 0.25f + 0.04f * (float) (s - 24)); if (s % 8 == 2) mixInto (out, Ho, at, 0.45f); }
            else { if ((s * 7 + v) % 5 != 0) mixInto (out, Hc, at, acc * 0.5f); if (s % 4 == 2) mixInto (out, Ho, at, 0.5f); }
        }
        softLimit (out); return out;
    }
    inline Buf bassLoop (int type, int v)
    {
        const double beat = 60.0 / 172.0, step = beat / 4.0; auto out = make (beat * 8.0);
        static const int roots[4] { 29, 32, 34, 36 }; const int r = roots[v % 4]; Buf n0 = bass (type, r), n1 = bass (type, r + 12 > 41 ? r : r + 12), n2 = bass (type, r + 7 > 43 ? r : r + 7);
        for (int s = 0; s < 32; ++s)
        {
            const size_t at = (size_t) (s * step * FS); const int pat = (v / 4) % 3;
            if (pat == 0 && s % 4 == 2) mixInto (out, gated (n0, step * 1.8), at, 0.9f);                       // contratiempo
            else if (pat == 1 && s % 2 == 1) mixInto (out, gated (s % 8 == 7 ? n1 : n0, step * 0.9), at, 0.85f);   // semicorcheas sincopadas
            else if (pat == 2 && s % 4 != 0) mixInto (out, gated (s % 8 == 6 ? n2 : n0, step * 0.9), at, 0.8f);    // galope
        }
        softLimit (out); return out;
    }
    inline Buf percLoop (int v)
    {
        const double beat = 60.0 / 172.0, step = beat / 4.0; auto out = make (beat * 8.0);
        Buf a = tom (mx (320, 200, V (v, 511)), mx (180, 110, V (v, 511)), 0.18), b = conga (mx (200, 380, V (v, 512)), 0.12), c = rim (1700, 480), d = shaker (0.05, 6500, 0.006), e = clave (2500), f = cowbell (1.0, 0.12);
        for (int s = 0; s < 32; ++s)
        {
            const size_t at = (size_t) (s * step * FS); const int m = v % 4;
            if (m == 0) { if (s % 8 == 3 || s % 8 == 6) mixInto (out, b, at, 0.8f); if (s % 2 == 1) mixInto (out, d, at, 0.35f); }
            else if (m == 1) { if (s % 16 == 2 || s % 16 == 7 || s % 16 == 10) mixInto (out, c, at, 0.7f); if (s % 4 == 0) mixInto (out, e, at, 0.3f); }
            else if (m == 2) { if (s % 8 == 5) mixInto (out, a, at, 0.8f); if (s % 8 == 7) mixInto (out, a, at, 0.6f); if (s % 4 == 2) mixInto (out, d, at, 0.4f); }
            else { if (s % 8 == 2 || s % 8 == 5) mixInto (out, f, at, 0.45f); if (s % 4 == 1) mixInto (out, c, at, 0.35f); }
        }
        softLimit (out); return out;
    }
    inline Buf rollLoop (int kind, int v)
    {   // redobles de 2 compases que aceleran y suben: 0 snare, 1 clap, 2 kick
        const double beat = 60.0 / 172.0; auto out = make (beat * 8.0); Buf h;
        if (kind == 0) h = snare (mx (180, 215, V (v, 521)), 0.07, 0.16, 1.0, 1200, 1.3, 0.3); else if (kind == 1) h = clap (0.1, mx (1000, 1200, V (v, 522)), 3, 0.011, 1.1, 1.3); else h = kick (mx (260, 520, V (v, 523)), 50, 0.03, 0.2, 3, 0.25, 0.3);
        double t = 0; const double span = beat * (v % 2 ? 8.0 : 4.0); int idx = 0;
        while (t < span) { const double u = t / span, iv = beat * mx (0.5, 0.0625, u * u); const float g = (float) mx (0.35, 1.0, u); mixInto (out, h, (size_t) (t * FS), g); t += iv; ++idx; }
        softLimit (out); return out;
    }

    // ---------- Salida ----------
    // Decima a 44.1 kHz (FIR sinc + Blackman, fc = 19.8 kHz), normaliza a -1 dBFS y aplica un fundido final de 3 ms.
    inline Buf finalize (const Buf& in, float peak = 0.89f)
    {
        static const std::vector<double> h = []
        {
            const int N = 161; std::vector<double> k ((size_t) N); const double fc = 19800.0 / FS; double sum = 0;
            for (int n = 0; n < N; ++n)
            {
                const double m = n - (N - 1) / 2.0, sinc = m == 0 ? 2 * fc : std::sin (TAU * fc * m) / (TAU / 2 * m);
                const double w = 0.42 - 0.5 * std::cos (TAU * n / (N - 1)) + 0.08 * std::cos (2 * TAU * n / (N - 1));
                k[(size_t) n] = sinc * w; sum += k[(size_t) n];
            }
            for (auto& x : k) x /= sum; return k;
        }();
        const int N = (int) h.size(), half = N / 2; Buf out (in.size() / 2, 0.f);
        for (size_t k = 0; k < out.size(); ++k)
        {
            double a = 0; const long c = (long) k * 2;
            for (int j = 0; j < N; ++j) { const long idx = c + j - half; if (idx >= 0 && idx < (long) in.size()) a += h[(size_t) j] * in[(size_t) idx]; }
            out[k] = (float) a;
        }
        {   // quita el desplazamiento de continua (solo si lo hay) y entra con una rampa mínima para no empezar con un chasquido
            double mean = 0; for (auto x : out) mean += x; mean /= (double) std::max<size_t> (1, out.size());
            if (std::abs (mean) > 0.004) { double x1 = 0, y1 = 0; for (auto& v : out) { const double y = (double) v - x1 + 0.9985 * y1; x1 = v; y1 = y; v = (float) y; } }
            for (size_t i = 0; i < 8 && i < out.size(); ++i) out[i] *= (float) i / 8.f;
        }
        float pk = 0; for (auto x : out) pk = std::max (pk, std::abs (x));
        if (pk < 1e-6f) return out;
        const float g = peak / pk; const int n = (int) out.size(), fade = (int) (OUT * 0.003);
        for (int i = 0; i < n; ++i) { float v = out[(size_t) i] * g; if (i > n - fade) v *= (float) (n - i) / (float) fade; out[(size_t) i] = v; }
        return out;
    }

    // Nivel de pico por familia: los hats y platillos no deben sonar tan fuertes como un bombo
    inline float peakFor (const std::string& rel)
    {
        auto has = [&] (const char* k) { return rel.find (k) != std::string::npos; };
        if (has ("Sirena")) return 0.5f;
        if (has ("Hats/") || has ("Shaker") || has ("Tambourine") || has ("Crash") || has ("Ride")) return 0.6f;
        if (has ("Claps/") || has ("FX/") || has ("Rim") || has ("Clave") || has ("Cowbell")) return 0.8f;
        if (has ("Sirena")) return 0.5f;
        if (has ("Snares/")) return 0.85f;
        return 0.89f;
    }

    // ---------- Catálogo ----------
    inline std::string num (int i) { return (i < 10 ? "0" : "") + std::to_string (i); }
    inline std::string noteName (int midi) { static const char* n[12] { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" }; return std::string (n[midi % 12]) + std::to_string (midi / 12 - 1); }

    // Llama a emit(rutaRelativa, buffer a 88.2 kHz) por cada sonido. Devuelve cuántos generó.
    template <class Emit> int catalog (Emit&& emit)
    {
        int count = 0; auto out = [&] (const std::string& p, Buf&& b) { emit (p, std::move (b)); ++count; };
        // Kicks (150)
        for (int i = 0; i < 30; ++i) { const double ad = mx (0.14, 0.45, V (i, 4)); out ("Kicks/Kick 909 " + num (i + 1) + ".wav", kick (mx (170, 290, V (i, 1)), mx (46, 58, V (i, 2)), mx (0.018, 0.045, V (i, 3)), ad, mx (1.2, 2.6, V (i, 5)), mx (0.05, 0.3, V (i, 6)), ad * 3.2 + 0.1, 1.0, i % 3 == 0 ? 3 : 0, mx (5500, 12000, V (i, 7)))); }
        for (int i = 0; i < 18; ++i) { const double ad = mx (0.45, 1.1, V (i, 14)); out ("Kicks/Kick 808 " + num (i + 1) + ".wav", kick (mx (90, 170, V (i, 11)), mx (38, 52, V (i, 12)), mx (0.02, 0.06, V (i, 13)), ad, mx (1.0, 1.8, V (i, 15)), mx (0.0, 0.05, V (i, 16)), std::min (1.6, ad * 3.0 + 0.2), 1.0, 0, mx (2200, 9000, V (i, 17)))); }
        for (int i = 0; i < 40; ++i) { const double ad = mx (0.30, 0.65, V (i, 24)); out ("Kicks/Kick Gabber " + num (i + 1) + ".wav", kick (mx (420, 760, V (i, 21)), mx (38, 58, V (i, 22)), mx (0.022, 0.06, V (i, 23)), ad, mx (5, 14, V (i, 25)), mx (0.1, 0.45, V (i, 26)), ad * 2.2 + 0.15, mx (1.0, 2.4, V (i, 27)), i % 4, mx (3500, 11000, V (i, 28)))); }
        for (int i = 0; i < 26; ++i) out ("Kicks/Kick Hardcore " + num (i + 1) + ".wav", kick (mx (320, 520, V (i, 31)), mx (46, 64, V (i, 32)), mx (0.025, 0.05, V (i, 33)), mx (0.2, 0.36, V (i, 34)), mx (3, 8, V (i, 35)), mx (0.2, 0.45, V (i, 36)), 0.55, mx (1.0, 2.0, V (i, 37)), (i + 1) % 4, mx (4000, 11000, V (i, 38))));
        for (int i = 0; i < 12; ++i) out ("Kicks/Kick Punch " + num (i + 1) + ".wav", kick (mx (260, 420, V (i, 41)), mx (55, 75, V (i, 42)), mx (0.015, 0.03, V (i, 43)), mx (0.08, 0.2, V (i, 44)), mx (2, 5, V (i, 45)), mx (0.25, 0.5, V (i, 46)), 0.35));
        for (int i = 0; i < 10; ++i) { const double ad = mx (0.5, 0.95, V (i, 54)); out ("Kicks/Kick Sub " + num (i + 1) + ".wav", kick (mx (140, 240, V (i, 51)), mx (32, 44, V (i, 52)), mx (0.05, 0.1, V (i, 53)), ad, mx (1.5, 3, V (i, 55)), 0.02, ad * 2 + 0.2)); }
        for (int i = 0; i < 14; ++i) out ("Kicks/Kick Click " + num (i + 1) + ".wav", kick (mx (700, 1100, V (i, 61)), mx (60, 85, V (i, 62)), 0.012, mx (0.08, 0.16, V (i, 63)), mx (1.5, 4.0, V (i, 64)), mx (0.5, 1.2, V (i, 65)), 0.3, 1.0, i % 4, mx (4500, 12000, V (i, 66))));
        // Snares (70)
        for (int i = 0; i < 34; ++i) out ("Snares/Snare 909 " + num (i + 1) + ".wav", snare (mx (170, 215, V (i, 71)), mx (0.05, 0.12, V (i, 72)), mx (0.1, 0.3, V (i, 73)), mx (0.8, 1.3, V (i, 74)), mx (900, 1600, V (i, 75)), 1.3));
        for (int i = 0; i < 22; ++i) out ("Snares/Snare 808 " + num (i + 1) + ".wav", snare (mx (150, 200, V (i, 81)), mx (0.08, 0.16, V (i, 82)), mx (0.1, 0.22, V (i, 83)), mx (0.45, 0.8, V (i, 84)), mx (600, 1100, V (i, 85)), 1.1));
        for (int i = 0; i < 22; ++i) out ("Snares/Snare Gabber " + num (i + 1) + ".wav", snare (mx (200, 280, V (i, 91)), mx (0.06, 0.12, V (i, 92)), mx (0.1, 0.2, V (i, 93)), mx (0.7, 1.1, V (i, 94)), mx (500, 900, V (i, 95)), mx (3, 6, V (i, 96))));
        for (int i = 0; i < 24; ++i) out ("Snares/Snare Hardcore " + num (i + 1) + ".wav", snare (mx (190, 250, V (i, 101)), mx (0.07, 0.14, V (i, 102)), mx (0.12, 0.3, V (i, 103)), mx (0.8, 1.2, V (i, 104)), mx (600, 1200, V (i, 105)), mx (2, 3.5, V (i, 106)), 0.6));
        // Claps (48)
        for (int i = 0; i < 42; ++i) out ("Claps/Clap 909 " + num (i + 1) + ".wav", clap (mx (0.05, 0.3, V (i, 111)), mx (850, 1900, V (i, 112)), 2 + (int) (V (i, 115) * 4.0), mx (0.007, 0.017, V (i, 113)), mx (0.8, 2.4, V (i, 114)), mx (0.9, 2.0, V (i, 116))));
        for (int i = 0; i < 24; ++i) out ("Claps/Clap 808 " + num (i + 1) + ".wav", clap (mx (0.08, 0.45, V (i, 121)), mx (750, 1500, V (i, 122)), 2 + (int) (V (i, 125) * 3.0), mx (0.008, 0.022, V (i, 123)), mx (0.7, 1.8, V (i, 124)), mx (0.7, 1.4, V (i, 126))));
        for (int i = 0; i < 36; ++i) out ("Claps/Clap Gabber " + num (i + 1) + ".wav", clap (mx (0.06, 0.3, V (i, 131)), mx (600, 1500, V (i, 132)), 3 + (int) (V (i, 135) * 3.0), mx (0.006, 0.014, V (i, 133)), mx (0.6, 1.6, V (i, 136)), mx (2.0, 6.0, V (i, 134))));
        // Hats (110): el 909 es casi todo ruido con un poco de metal y crujido; el 808 es metálico puro
        for (int i = 0; i < 28; ++i) out ("Hats/Hat 909 Cerrado " + num (i + 1) + ".wav", hat909 (mx (0.014, 0.08, V (i, 141)), mx (4000, 7500, V (i, 143)), mx (6000, 10500, V (i, 144)), mx (0.85, 1.22, V (i, 142)), mx (0.12, 0.5, V (i, 320)), mx (1.3, 2.6, V (i, 321)), mx (10500, 14500, V (i, 322))));
        for (int i = 0; i < 14; ++i) out ("Hats/Hat 808 Cerrado " + num (i + 1) + ".wav", hat (mx (0.03, 0.09, V (i, 151)), mx (0.8, 0.95, V (i, 152)), mx (4500, 6500, V (i, 153)), mx (6500, 8500, V (i, 154)), mx (0.05, 0.12, V (i, 155))));
        for (int i = 0; i < 14; ++i) out ("Hats/Hat Hardcore Cerrado " + num (i + 1) + ".wav", hat (mx (0.025, 0.055, V (i, 161)), mx (1.05, 1.25, V (i, 162)), mx (7000, 9000, V (i, 163)), mx (9000, 12000, V (i, 164)), mx (0.5, 0.9, V (i, 165)), 0.5, 1.6, 14500.0));
        for (int i = 0; i < 18; ++i) out ("Hats/Hat 909 Abierto " + num (i + 1) + ".wav", hat909 (mx (0.08, 0.42, V (i, 171)), mx (4000, 7000, V (i, 173)), mx (6000, 10000, V (i, 174)), mx (0.85, 1.22, V (i, 172)), mx (0.12, 0.5, V (i, 324)), mx (1.3, 2.6, V (i, 325)), mx (10500, 14500, V (i, 326))));
        for (int i = 0; i < 10; ++i) out ("Hats/Hat 808 Abierto " + num (i + 1) + ".wav", hat (mx (0.15, 0.4, V (i, 181)), mx (0.8, 0.95, V (i, 182)), mx (4500, 6000, V (i, 183)), mx (6500, 8500, V (i, 184)), mx (0.05, 0.12, V (i, 185))));
        for (int i = 0; i < 8; ++i) out ("Hats/Hat Hardcore Abierto " + num (i + 1) + ".wav", hat (mx (0.12, 0.3, V (i, 191)), mx (1.05, 1.25, V (i, 192)), mx (7000, 9000, V (i, 193)), mx (9000, 12000, V (i, 194)), mx (0.5, 0.9, V (i, 195)), 0.5, 1.6, 14500.0));
        for (int i = 0; i < 10; ++i) out ("Hats/Hat Pedal " + num (i + 1) + ".wav", hat909 (mx (0.012, 0.025, V (i, 201)), mx (6000, 8000, V (i, 203)), 9500, mx (0.95, 1.1, V (i, 202)), mx (0.12, 0.5, V (i, 332)), mx (1.3, 2.6, V (i, 333)), mx (10500, 14500, V (i, 334))));
        for (int i = 0; i < 8; ++i) out ("Hats/Hat 909 Semi " + num (i + 1) + ".wav", hat909 (mx (0.07, 0.14, V (i, 205)), mx (5800, 7200, V (i, 206)), mx (7500, 9500, V (i, 207)), mx (0.95, 1.1, V (i, 208)), mx (0.12, 0.5, V (i, 328)), mx (1.3, 2.6, V (i, 329)), mx (10500, 14500, V (i, 330))));
        // Percusiones (80)
        for (int i = 0; i < 16; ++i) { const double f0 = mx (330, 120, i / 15.0); out ("Percusiones/Tom 909 " + num (i + 1) + ".wav", tom (f0, f0 * 0.55, mx (0.14, 0.3, i / 15.0))); }
        for (int i = 0; i < 14; ++i) { const double f0 = mx (260, 100, i / 13.0); out ("Percusiones/Tom 808 " + num (i + 1) + ".wav", tom (f0, f0 * 0.65, mx (0.25, 0.5, i / 13.0))); }
        for (int i = 0; i < 16; ++i) { const double f0 = mx (300, 110, i / 15.0); out ("Percusiones/Tom Gabber " + num (i + 1) + ".wav", tomDist (f0, f0 * 0.5, mx (0.15, 0.35, i / 15.0), mx (3.0, 7.0, V (i, 215)))); }
        for (int i = 0; i < 14; ++i) out ("Percusiones/Rim " + num (i + 1) + ".wav", rim (mx (440, 560, V (i, 211)), mx (1500, 1900, V (i, 212))));
        for (int i = 0; i < 10; ++i) out ("Percusiones/Cowbell " + num (i + 1) + ".wav", cowbell (mx (0.85, 1.2, i / 9.0), mx (0.15, 0.45, V (i, 221))));
        for (int i = 0; i < 12; ++i) out ("Percusiones/Conga " + num (i + 1) + ".wav", conga (mx (180, 420, i / 11.0), mx (0.08, 0.16, V (i, 231))));
        for (int i = 0; i < 10; ++i) out ("Percusiones/Clave " + num (i + 1) + ".wav", clave (mx (2200, 3200, i / 9.0)));
        for (int i = 0; i < 14; ++i) out ("Percusiones/Shaker " + num (i + 1) + ".wav", shaker (mx (0.03, 0.09, V (i, 241)), mx (4500, 8000, V (i, 242)), mx (0.002, 0.012, V (i, 243))));
        for (int i = 0; i < 10; ++i) out ("Percusiones/Tambourine " + num (i + 1) + ".wav", tambourine (mx (0.05, 0.14, i / 9.0)));
        for (int i = 0; i < 10; ++i) out ("Percusiones/Crash " + num (i + 1) + ".wav", crash (mx (0.7, 1.8, i / 9.0), mx (0.9, 1.15, V (i, 251)), mx (3500, 5500, V (i, 252))));
        for (int i = 0; i < 10; ++i) out ("Percusiones/Ride " + num (i + 1) + ".wav", ride (mx (0.5, 1.1, i / 9.0), mx (0.9, 1.15, V (i, 261))));
        // Bajos (56): notas de Fa menor
        static const int bn[15] { 25, 27, 29, 31, 32, 34, 36, 37, 39, 41, 43, 44, 46, 48, 49 }; static const char* bt[7] { "Sub", "Reese", "Acid", "808", "Gabber", "Donk", "Fuzz" };
        for (int t = 0; t < 7; ++t) for (int k = 0; k < 15; ++k) out ("Bajos/Bajo " + std::string (bt[t]) + " " + noteName (bn[k]) + ".wav", bass (t, bn[k]));
        // Leads y stabs (56)
        static const int hn[16] { 41, 44, 46, 48, 53, 56, 58, 60, 65, 68, 70, 72, 77, 80, 82, 84 }, rn[22] { 48, 50, 51, 53, 55, 56, 58, 60, 61, 63, 65, 67, 68, 70, 72, 73, 75, 77, 79, 80, 82, 84 };
        for (int k = 0; k < 16; ++k) out ("Leads/Lead Hoover " + noteName (hn[k]) + ".wav", synthVoice ({ mtof (hn[k]), { 0.0 }, 7, 28, 12, 0.14, 0.005, 0.5, 0.85, 1.2, 0.2, 2600.0 + 300.0 * k, 3.0, 1.5, 2.2 }));
        for (int k = 0; k < 22; ++k) out ("Leads/Stab Rave " + noteName (rn[k]) + ".wav", synthVoice ({ mtof (rn[k]), { 0.0, 3.0, 7.0 }, 3, 12, 0, 0.1, 0.001, 0.12, 0.0, 0.45, 0.12, 4500, 2.5, 1.5, 1.6 }));
        for (int k = 0; k < 22; ++k) out ("Leads/Supersaw " + noteName (rn[k]) + ".wav", synthVoice ({ mtof (rn[k]), { 0.0, 7.0, 12.0 }, 5, 20, 0, 0.1, 0.004, 0.5, 0.6, 1.0, 0.3, 7000, 1.0, 0.5, 1.2 }));
        for (int k = 0; k < 22; ++k) out ("Leads/Pluck " + noteName (rn[k]) + ".wav", synthVoice ({ mtof (rn[k]), { 0.0 }, 2, 8, 0, 0.1, 0.001, 0.14, 0.0, 0.5, 0.08, 1500, 4.0, 5.0, 1.3 }));
        for (int k = 0; k < 22; ++k) out ("Leads/Lead Screech " + noteName (rn[k] + 12) + ".wav", synthVoice ({ mtof (rn[k] + 12), { 0.0 }, 5, 35, 0, 0.1, 0.002, 0.25, 0.6, 0.6, 0.1, 5000, 6.0, 0.8, 4.0 }));
        // FX (50)
        static const double rl[16] { 2, 3, 4, 4, 6, 6, 8, 8, 2.5, 3.5, 4.5, 5, 5.5, 7, 7.5, 8 };
        for (int i = 0; i < 16; ++i) out ("FX/FX Riser " + num (i + 1) + ".wav", riser (rl[i], mx (300, 600, V (i, 271)), mx (6000, 12000, V (i, 272)), true, 300 + i));
        for (int i = 0; i < 12; ++i) out ("FX/FX Downlifter " + num (i + 1) + ".wav", riser (2.0 + 0.6 * (i % 6) + 0.4 * (i / 6), 500, mx (7000, 11000, V (i, 281)), false, 400 + i));
        for (int i = 0; i < 14; ++i) out ("FX/FX Impacto " + num (i + 1) + ".wav", impact (mx (0.8, 2.4, i / 13.0), mx (45, 75, V (i, 291)), 500 + i));
        for (int i = 0; i < 12; ++i) out ("FX/FX Laser " + num (i + 1) + ".wav", laser (mx (3000, 8000, V (i, 301)), mx (80, 300, V (i, 302)), mx (0.03, 0.12, V (i, 303)), mx (0.0, 4.0, V (i, 304))));
        for (int i = 0; i < 12; ++i) out ("FX/FX Barrido " + num (i + 1) + ".wav", sweepNoise (2.5 + 0.5 * (i % 6) + 0.3 * (i / 6), i % 2 == 0, 600 + i, 300, 9000));
        for (int i = 0; i < 10; ++i) { auto b = crash (mx (0.9, 1.6, i / 9.0), 1.0, 4000); std::reverse (b.begin(), b.end()); out ("FX/FX Crash Reverso " + num (i + 1) + ".wav", std::move (b)); }
        for (int i = 0; i < 14; ++i) out ("FX/FX Whoosh " + num (i + 1) + ".wav", sweepNoise (0.7 + 0.25 * (i % 6) + 0.1 * (i / 6), i % 2 == 1, 700 + i, 500, 12000));
        for (int i = 0; i < 12; ++i) out ("FX/FX Sirena " + num (i + 1) + ".wav", siren (1.5 + 0.4 * (i % 6) + 0.3 * (i / 6), mx (400, 700, V (i, 311)), mx (1200, 2200, V (i, 312)), mx (1.0, 3.0, V (i, 313))));
        // Bucles de batería 172 BPM (36)
        for (int i = 0; i < 15; ++i) out ("Loops/Loop 909 172 BPM " + num (i + 1) + ".wav", drumLoop (0, i));
        for (int i = 0; i < 15; ++i) out ("Loops/Loop Gabber 172 BPM " + num (i + 1) + ".wav", drumLoop (1, i));
        for (int i = 0; i < 18; ++i) out ("Loops/Loop Hardcore 172 BPM " + num (i + 1) + ".wav", drumLoop (2, i));
        // Bucles extra (58)
        for (int i = 0; i < 12; ++i) out ("Loops/Hat Loop 909 172 BPM " + num (i + 1) + ".wav", hatLoop (0, i));
        for (int i = 0; i < 6; ++i) out ("Loops/Hat Loop Hardcore 172 BPM " + num (i + 1) + ".wav", hatLoop (1, i));
        { static const char* nm[] { "Sub", "Reese", "Acid", "808", "Gabber", "Donk", "Fuzz" }; static const int ty[] { 1, 2, 4, 5, 6, 0 }; (void) nm;
          for (int i = 0; i < 12; ++i) { const int t = ty[i % 6]; out ("Loops/Bass Loop " + std::string (nm[t]) + " 172 BPM " + num (i + 1) + ".wav", bassLoop (t, i)); } }
        for (int i = 0; i < 8; ++i) out ("Loops/Perc Loop 172 BPM " + num (i + 1) + ".wav", percLoop (i));
        for (int i = 0; i < 8; ++i) out ("Loops/Snare Roll 172 BPM " + num (i + 1) + ".wav", rollLoop (0, i));
        for (int i = 0; i < 6; ++i) out ("Loops/Clap Roll 172 BPM " + num (i + 1) + ".wav", rollLoop (1, i));
        for (int i = 0; i < 6; ++i) out ("Loops/Kick Roll 172 BPM " + num (i + 1) + ".wav", rollLoop (2, i));
        return count;
    }
}
