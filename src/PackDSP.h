#pragma once
// Síntesis del paquete de sonidos INJUSTICE (C++ puro, sin JUCE).
// Todo se genera a 88.2 kHz (sobremuestreo x2) con osciladores polyBLEP y filtros biquad RBJ,
// y al final se decima a 44.1 kHz con un FIR de sinc ventaneada (Blackman) para que no haya aliasing.
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include <map>

namespace packdsp
{
    constexpr double OUT = 44100.0, OS = 2.0, FS = OUT * OS, TAU = 6.283185307179586;
    using Buf = std::vector<float>;

    struct Rng { uint32_t s; explicit Rng (uint32_t seed) : s (seed * 2654435761u + 12345u) {} float next() { s = s * 1664525u + 1013904223u; return (float) (s >> 8) * (1.f / 8388608.f) - 1.f; } };
    inline uint32_t& vseed() { static uint32_t v = 0; return v; }   // cada generador de ruido usa una semilla distinta: dos claps o hats nunca comparten la misma forma de onda
    inline Buf make (double secs) { return Buf ((size_t) (secs * FS), 0.f); }
    inline double ex (double t, double tau) { return std::exp (-t / tau); }
    inline double V (int i, double salt) { const double x = i * 0.6180339887 + salt * 0.3819660113; return x - std::floor (x); }
    inline double VH (int i, double salt) { uint32_t h = (uint32_t) i * 2654435761u ^ (uint32_t) (salt * 40503.0) * 2246822519u; h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16; return (double) (h & 0xffffff) / 16777216.0; }   // aleatorio por parametro (independiente)
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
    inline Buf snare (double tone, double td, double nd, double noiseAmt, double hpf, double drive, double len = 0.5, double toneAmt = 1.0)
    {
        auto b = make (len); Rng r (11u + 977u * vseed()++); double p1 = 0, p2 = 0; Buf n = make (len);
        for (auto& x : n) x = r.next();
        run (n, 1, hpf, 0.7); run (n, 0, 10500, 0.7);
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / FS, bend = 1.0 + 0.35 * ex (t, 0.012);
            p1 += TAU * tone * bend / FS; p2 += TAU * tone * 1.83 * bend / FS;
            const double tones = toneAmt * (0.7 * std::sin (p1) + 0.4 * std::sin (p2)) * ex (t, td);
            const double nz = n[i] * ex (t, nd) * std::min (1.0, t / 0.0003);
            b[i] = (float) std::tanh ((tones + noiseAmt * nz) * drive);
        }
        return b;
    }
    inline Buf clap (double tail, double centre, int bursts, double spacing, double q, double drive)
    {   // Clap SECO: rafagas de ruido filtrado + un cuerpo corto (12 a 35 ms) y nada de cola ni reverb.
        const double body = std::clamp (tail * 0.28, 0.012, 0.035), burstTau = 0.0032;
        const double len = bursts * spacing + body * 7.0 + 0.01;
        auto b = make (len); Rng r (23u + 977u * vseed()++); Buf n2 = make (len);
        for (size_t i = 0; i < b.size(); ++i) { const float v = r.next(); b[i] = v; n2[i] = v; }
        run (b, 2, centre, q); run (n2, 2, centre * 1.6, 1.0);
        const double dr = std::min (drive, 2.6);
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / FS; double e = 0;
            for (int k = 0; k < bursts; ++k) { const double st = k * spacing; if (t >= st && t < st + spacing) e = (0.8 + 0.04 * k) * ex (t - st, burstTau); }
            if (t >= bursts * spacing) e = 0.9 * ex (t - bursts * spacing, body);
            b[i] = (float) std::tanh ((b[i] + 0.3 * n2[i]) * e * 2.0 * dr);
        }
        const size_t fo = (size_t) (0.004 * FS); for (size_t i = 0; i < fo && i < b.size(); ++i) b[b.size() - 1 - i] *= (float) i / (float) fo;
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
    // Hat "medido": se parametriza por el largo real (t40 = ms hasta caer 40 dB) como los samples de referencia.
    struct RefHat
    {
        double t40 = 0.2, fastMix = 0.4, scale = 1.0, metalAmt = 0.6, noise = 0.8, hpf = 5500, lp = 15000, bpc = 8000, bpq = 0.5, grit = 1.0;
        double bounceHz = 0, bounceDepth = 0, attack = 0.0003, len = 0;
        double layerAmt = 0, layerHpf = 9000;   // segunda capa de ruido mas aguda (hat "layered")
    };
    inline Buf hatRef (const RefHat& h)
    {
        const double tau = h.t40 / std::pow (4.6, 1.0 / 1.35) * 1.2, L = h.len > 0 ? h.len : std::min (2.0, h.t40 * 2.6 + 0.03);
        Rng r (41u + 811u * vseed()++); const double rv1 = 0.5 + 0.5 * r.next(), rv2 = 0.5 + 0.5 * r.next(), rv3 = 0.5 + 0.5 * r.next(), rv4 = 0.5 + 0.5 * r.next(), rv5 = 0.5 + 0.5 * r.next();   // caracter propio de cada hat
        const double hpfMul = 0.55 + 0.85 * rv1, bpcMul = 0.65 + 0.8 * rv2, bpqv = std::min (0.9, h.bpq * (0.6 + 0.8 * rv3)), ring = 0.0; (void) rv4;
        double rf[3], rp[3] = { 0, 0, 0 }; for (auto& f : rf) f = 3500.0 + 9500.0 * (0.5 + 0.5 * r.next());
        auto b = metal (L, h.scale * (0.85 + 0.3 * rv5)); Buf n2 = make (L);
        for (size_t i = 0; i < b.size(); ++i) { double x = h.metalAmt * b[i] + h.noise * r.next(); if (ring > 0) { double rs = 0; for (int k = 0; k < 3; ++k) { rp[k] += TAU * rf[k] / FS; rs += std::sin (rp[k]); } x += ring * 0.35 * rs; } b[i] = (float) x; }
        if (h.layerAmt > 0) { for (auto& x : n2) x = (float) r.next(); run (n2, 1, h.layerHpf, 0.75); run (n2, 1, h.layerHpf, 0.75); }
        run (b, 2, h.bpc * 0.8 * bpcMul, bpqv); run (b, 1, h.hpf * 0.72 * hpfMul, 0.75); run (b, 1, h.hpf * 0.72 * hpfMul, 0.75);
        if (h.layerAmt > 0) for (size_t i = 0; i < b.size(); ++i) b[i] += (float) (h.layerAmt * n2[i]);
        if (h.grit > 1.0) for (auto& x : b) x = (float) std::tanh (x * h.grit);
        if (h.lp < 19000.0) { run (b, 0, h.lp, 0.7); run (b, 0, h.lp, 0.7); }
        Buf hi = b; run (hi, 1, 8500.0 * (0.8 + 0.4 * rv5), 0.75); Bq ck; ck.set (1, 6000, 0.7);
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / FS, atk = std::min (1.0, t / h.attack);
            double e = atk * std::exp (-std::pow (t / tau, 1.35)), eh = atk * std::exp (-std::pow (t / (tau * 0.5), 1.35));
            if (h.bounceDepth > 0) { const double bz = 1.0 - h.bounceDepth * (0.5 - 0.5 * std::cos (TAU * h.bounceHz * t)) * std::min (1.0, t / 0.02); e *= bz; eh *= bz; }
            (void) h.fastMix;
            b[i] = (float) ((double) b[i] * e * 0.6 + (double) hi[i] * eh * 0.7 + 0.5 * ck.tick (r.next()) * std::exp (-t / 0.0015));   // cuerpo + brillo que cae mas rapido + golpe inicial
        }
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
    inline Buf gated (const Buf& src, double secs)
    {
        const size_t n = std::min (src.size(), (size_t) (secs * FS)); Buf b (src.begin(), src.begin() + (long) n); const size_t f = std::min (n, (size_t) (0.006 * FS));
        for (size_t i = 0; i < f; ++i) b[n - 1 - i] *= (float) i / (float) f; return b;
    }
    // Banco con los kicks, snares y hats nuevos (los primeros 12 de cada familia): los bucles usan los mismos sonidos que el paquete.
    using Bank = std::map<std::string, Buf>;
    inline Bank*& bankPtr() { static Bank* p = nullptr; return p; }
    inline bool bankHas() { return bankPtr() != nullptr && ! bankPtr()->empty(); }
    inline Buf bk (char kind, const char* fam, int idx, int count)
    {   // idx cualquiera: se reduce al rango guardado (1 a 12 como maximo)
        const int n = std::min (12, count), i = ((idx % n) + n) % n + 1;
        const std::string pre = kind == 'K' ? "Kicks/Kick " : (kind == 'S' ? "Snares/Snare " : "Hats/Hat ");
        const auto it = bankPtr()->find (pre + fam + " " + (i < 10 ? "0" : "") + std::to_string (i) + ".wav");
        return it != bankPtr()->end() ? it->second : Buf (64, 0.f);
    }
    inline void mixInto (Buf& dst, const Buf& src, size_t at, float g) { for (size_t i = 0; i < src.size() && at + i < dst.size(); ++i) dst[at + i] += src[i] * g; }
    inline Buf drumLoop (int kit, int v)
    {   // 2 compases a 172 BPM. Cada bucle cambia de patrón (6 arquetipos), de golpes (2-3 variantes por instrumento) y de relleno; los golpes llevan fuerza distinta
        const double beat = 60.0 / 172.0, step = beat / 4.0; const size_t N = (size_t) (beat * 8.0 * FS); Rng r (5000u + 131u * kit + 17u * v + 977u * vseed()++);
        const int arch = v % 6, vv = v / 6;
        std::vector<Buf> K, S, C, Hc; Buf Ho, X;
        for (int j = 0; j < 3; ++j)
        {
            const double d = (double) j;
            if (kit == 0) { K.push_back (kick (235 + 9 * v + 6 * d, 50 + d, 0.032 + 0.003 * d, 0.30 + 0.025 * v / 3.0 + 0.02 * d, 1.6 + 0.3 * d, 0.25, 0.7)); S.push_back (snare (180 + 6 * d, 0.06 + 0.01 * d, 0.16 + 0.01 * d, 1.0, 1100, 1.3 + 0.2 * d)); C.push_back (clap (0.12 - 0.01 * d, 1100 + 60 * d, 3, 0.011, 1.1, 1.3)); Hc.push_back (hat909 (0.035 + 0.008 * d, 7000 + 300 * d, 9000, 1.0)); }
            else if (kit == 2) { K.push_back (kick (380 + 14 * v + 12 * d, 48, 0.035, 0.27 + 0.02 * d, 6 + 1.5 * d, 0.25, 0.6, 1.5, (int) d % 3)); S.push_back (snare (195 + 6 * d, 0.08, 0.15, 0.85, 800, 3.0 + 0.5 * d)); C.push_back (clap (0.09 + 0.01 * d, 1000 + 80 * d, 3, 0.01, 1.0, 2.0)); Hc.push_back (hat (0.03 + 0.006 * d, 1.15, 7500, 10000, 0.2, 0.5, 1.5)); }
            else { K.push_back (kick (500 + 18 * v + 14 * d, 46, 0.04, 0.34 + 0.025 * d, 9 + 2 * d, 0.2, 0.7, 1.8, (int) d % 4)); S.push_back (snare (210 + 5 * d, 0.09, 0.16, 0.9, 700, 4.0 + 0.5 * d)); C.push_back (clap (0.1 + 0.01 * d, 900 + 70 * d, 4, 0.01, 0.9, 3.0)); Hc.push_back (hat (0.034 + 0.006 * d, 1.1, 6500, 9000, 0.2)); }
        }
        if (bankHas())
        {   // sonidos nuevos del paquete en vez de los de la version vieja (cada bucle toma variantes distintas)
            K.clear(); S.clear(); Hc.clear();
            static const char* kf[3][3] { { "Basic", "Dash", "Sub" }, { "Gabber", "Gabber", "Burst" }, { "Buzz", "Burst", "Gabber" } };
            static const char* sf[3][3] { { "Clasico", "Generico", "Brillante" }, { "Crowd", "Hype", "Seco" }, { "Metalico", "Oscuro", "Seco" } };
            static const char* hf[3][3] { { "Corto", "Limpio", "Corto" }, { "Tick", "Corto", "Corto" }, { "Core", "Corto", "Tick" } };
            static const int kc[3][3] { { 20, 12, 16 }, { 20, 20, 14 }, { 16, 14, 20 } }, sc[3][3] { { 12, 12, 14 }, { 10, 10, 10 }, { 10, 10, 10 } }, hc[3][3] { { 14, 12, 14 }, { 8, 14, 14 }, { 12, 14, 8 } };
            const int kt = kit == 0 ? 0 : (kit == 2 ? 2 : 1);
            for (int j = 0; j < 3; ++j) { K.push_back (bk ('K', kf[kt][j], v * 3 + j * 5, kc[kt][j])); S.push_back (bk ('S', sf[kt][j], v * 2 + j * 3, sc[kt][j])); Hc.push_back (bk ('H', hf[kt][j], v + j * 2, hc[kt][j])); }
        }
        const double oh = 0.16 + 0.04 * (vv % 3);
        if (kit == 0) { Ho = hat909 (0.26, 6500, 9000, 1.0); X = rim (480, 1700); } else if (kit == 2) { Ho = hat (oh, 1.15, 7500, 10000, 0.2, 0.5, 1.5); X = rim (500, 1800); } else { Ho = hat (oh + 0.02, 1.1, 6500, 9000, 0.2); X = cowbell (1.0, 0.25); }
        if (bankHas()) Ho = gated (bk ('H', kit == 0 ? "Abierto" : (kit == 1 ? "Layered" : "Abierto"), v + vv, kit == 1 ? 8 : 12), beat * 0.5);   // el hat abierto se corta con el siguiente golpe
        Buf kk (N, 0.f), rest (N, 0.f); std::vector<size_t> kickAt;
        auto at = [&] (int s) { return (size_t) (s * step * FS); };
        auto hitK = [&] (int s, float g)
        {   // el bombo nuevo corta la cola del anterior (fade de 12 ms): los bombos largos ya no se encima unos con otros
            const size_t a = at (s), f = (size_t) (0.012 * FS);
            for (size_t i = a; i < N; ++i) { const size_t d = i - a; kk[i] *= d < f ? 1.f - (float) d / (float) f : 0.f; }
            mixInto (kk, K[(size_t) (r.next() * 0.5 + 0.5) * 3 % 3], a, g); kickAt.push_back (a);
        };
        auto vel = [&] (float base, float spread) { return base * (1.f + spread * (float) r.next()); };
        for (int s = 0; s < 32; ++s)
        {
            const int sb = s % 16, bar = s / 16; const bool fillBar = bar == 1;
            // bombo según el arquetipo
            bool kk4 = sb % 4 == 0;
            if (arch == 4) kk4 = sb == 0 || sb == 6 || sb == 10;   // medio tiempo con caja en 8
            if (kk4 && ! (fillBar && sb >= 12 && (arch == 5 || (v & 1)))) hitK (s, sb == 0 ? 1.0f : vel (0.92f, 0.06f));
            if (arch == 1 && sb == 14 && ! fillBar) hitK (s, 0.6f);
            if (arch == 2 && (sb == 6 || sb == 14)) hitK (s, 0.62f);
            if (arch == 3 && sb == 10) hitK (s, 0.75f);
            if (arch == 5 && fillBar && sb >= 12) hitK (s, 0.7f + 0.08f * (sb - 12));
            if (arch == 0 && (v & 8) && sb == 7) hitK (s, 0.6f);
            // caja / clap
            const bool backbeat = arch == 4 ? sb == 8 : (sb == 4 || sb == 12);
            if (backbeat) { mixInto (rest, C[(size_t) (s / 4) % 3], at (s), vel (0.78f, 0.1f)); if (v % 3 == 0 || kit == 2) mixInto (rest, S[(size_t) (s / 4) % 3], at (s), vel (0.5f, 0.1f)); }
            if (fillBar && sb >= 12 && (v % 2 == 1) && (sb % 2 == 0 || vv % 2)) mixInto (rest, S[(size_t) sb % 3], at (s), 0.35f + 0.1f * (sb - 12));
            if (v % 4 == 1 && sb == 15 && bar == 0) mixInto (rest, S[1], at (s), 0.3f);   // nota fantasma
            // hats
            if (sb % 4 == 2) mixInto (rest, Ho, at (s), vel (0.5f, 0.1f));
            static const float acc[4] { 0.0f, 0.32f, 0.0f, 0.24f };
            if (vv % 2 == 0) { if (sb % 2 == 1) mixInto (rest, Hc[(size_t) s % 3], at (s), vel (0.3f, 0.2f)); }
            else if (sb % 4 != 2) { const float g = sb % 4 == 0 ? 0.36f : acc[sb % 4]; if (g > 0 && (r.next() * 0.5 + 0.5) > 0.12) mixInto (rest, Hc[(size_t) s % 3], at (s), vel (g, 0.25f)); }
            if ((v & 8) == 0 && (v % 5 == 2) && sb == 10) mixInto (rest, X, at (s), 0.4f);
            if ((v & 8) && sb == 10) mixInto (rest, X, at (s), 0.4f);
        }
        // bombeo: todo lo que no es bombo baja un poco cuando entra el bombo (se oye más limpio y con más empuje)
        std::vector<float> duck (N, 1.f);
        for (auto a : kickAt) for (size_t i = 0; i < (size_t) (0.14 * FS) && a + i < N; ++i) { const float d = 1.f - 0.45f * (float) std::exp (-(double) i / (0.045 * FS)); duck[a + i] = std::min (duck[a + i], d); }
        Buf out (N); for (size_t i = 0; i < N; ++i) out[i] = kk[i] + rest[i] * duck[i];
        float pk = 0; for (auto x : out) pk = std::max (pk, std::abs (x)); if (pk > 1e-6f) for (auto& x : out) x = std::tanh (x / pk * 1.15f);
        return out;
    }

    // ---------- Bucles extra (172 BPM, 2 compases) ----------
    inline void softLimit (Buf& out) { float pk = 0; for (auto x : out) pk = std::max (pk, std::abs (x)); if (pk > 1e-6f) for (auto& x : out) x = std::tanh (x / pk * 1.1f); }
    inline Buf hatLoop (int kit, int v)
    {
        const double beat = 60.0 / 172.0, step = beat / 4.0; auto out = make (beat * 8.0); Buf Hc, Ho;
        if (kit == 0) { Hc = hat909 (mx (0.03, 0.05, V (v, 501)), 7000, 9000, 1.0); Ho = hat909 (mx (0.2, 0.32, V (v, 502)), 6500, 9000, 1.0); }
        else { Hc = hat (0.03, 1.15, 7500, 10000, 0.2, 0.5, 1.5); Ho = hat (0.18, 1.15, 7500, 10000, 0.2, 0.5, 1.5); }
        if (bankHas()) { Hc = bk ('H', kit == 0 ? "Corto" : "Core", v, kit == 0 ? 14 : 12); Ho = gated (bk ('H', kit == 0 ? "Abierto" : "Layered", v, kit == 0 ? 12 : 8), beat * 0.5); }
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
    {   // 2 compases a 172 BPM; 6 patrones distintos y un hueco por golpe de bombo (el bajo respira para dejar pasar el kick)
        const double beat = 60.0 / 172.0, step = beat / 4.0; auto out = make (beat * 8.0);
        static const int roots[4] { 29, 32, 34, 36 }; const int r = roots[v % 4]; Buf n0 = bass (type, r), n1 = bass (type, r + 12 > 41 ? r : r + 12), n2 = bass (type, r + 7 > 43 ? r : r + 7), n3 = bass (type, r + 3 > 40 ? r : r + 3);
        const int pat = (v / 4) % 6;
        for (int s = 0; s < 32; ++s)
        {
            const size_t at = (size_t) (s * step * FS); const int m = s % 16;
            switch (pat)
            {
                case 0: if (s % 4 == 2) mixInto (out, gated (n0, step * 1.8), at, 0.9f); break;                                              // contratiempo
                case 1: if (s % 2 == 1) mixInto (out, gated (s % 8 == 7 ? n1 : n0, step * 0.9), at, 0.85f); break;                          // semicorcheas sincopadas
                case 2: if (s % 4 != 0) mixInto (out, gated (s % 8 == 6 ? n2 : n0, step * 0.9), at, 0.8f); break;                            // galope
                case 3: if (s % 4 != 0) mixInto (out, gated (m == 7 || m == 15 ? n1 : (m == 11 ? n3 : n0), step * 0.85), at, 0.82f); break;  // rodante con saltos
                case 4: if (s % 4 == 2) mixInto (out, gated (s % 8 == 6 ? n1 : n0, step * 1.9), at, 0.9f); else if (s % 8 == 3) mixInto (out, gated (n2, step * 0.8), at, 0.7f); break;   // contratiempo con octava y quinta
                default: if (m == 3 || m == 6 || m == 10 || m == 13) mixInto (out, gated (m == 13 ? n3 : n0, step * 1.4), at, 0.88f); break;   // 3-3-2 sincopado
            }
        }
        for (int k = 0; k < 8; ++k)   // hueco para el kick en cada tiempo
        {
            const size_t a = (size_t) (k * beat * FS), n = (size_t) (0.085 * FS);
            for (size_t i = 0; i < n && a + i < out.size(); ++i) { const double u = (double) i / (double) n; out[a + i] *= (float) (0.2 + 0.8 * u * u); }
        }
        softLimit (out); return out;
    }
    inline Buf percLoop (int v)
    {   // 2 compases a 172 BPM; 8 patrones (congas, rim, toms, cowbell, tribal, hats, clave, rebote) y golpes con volumen humano
        const double beat = 60.0 / 172.0, step = beat / 4.0; auto out = make (beat * 8.0);
        const Buf a = tom (mx (320, 200, V (v, 511)), mx (180, 110, V (v, 511)), 0.18), a2 = tom (mx (230, 150, V (v, 531)), mx (120, 80, V (v, 531)), 0.22), b = conga (mx (200, 380, V (v, 512)), 0.12), b2 = conga (mx (300, 520, V (v, 532)), 0.1),
            c = rim (1700, 480), d = shaker (0.05, 6500, 0.006), e = clave (2500), f = cowbell (1.0, 0.12);
        Buf hc = d, ho = d; if (bankHas()) { hc = bk ('H', v % 2 ? "Tick" : "Corto", v, 8); ho = gated (bk ('H', "Abierto", v, 12), beat * 0.45); }
        const int m = v % 8; int n = 0;
        auto put = [&] (const Buf& x, int s, float g) { const float h = (float) (0.8 + 0.2 * VH (n++, 7000 + v)); mixInto (out, x, (size_t) (s * step * FS), g * h); };
        for (int s = 0; s < 32; ++s)
        {
            const int q = s % 16, o = s % 8;
            switch (m)
            {
                case 0: if (o == 3 || o == 6) put (b, s, 0.8f); if (s % 2 == 1) put (d, s, 0.35f); if (q == 14) put (b2, s, 0.6f); break;
                case 1: if (q == 2 || q == 7 || q == 10) put (c, s, 0.7f); if (s % 4 == 0) put (e, s, 0.3f); if (s % 2 == 1) put (hc, s, 0.4f); break;
                case 2: if (o == 5) put (a, s, 0.8f); if (o == 7) put (a, s, 0.6f); if (s % 4 == 2) put (d, s, 0.4f); if (q >= 12) put (q % 2 ? a2 : a, s, 0.55f); break;
                case 3: if (o == 2 || o == 5) put (f, s, 0.45f); if (s % 4 == 1) put (c, s, 0.35f); if (s % 4 == 2) put (hc, s, 0.35f); break;
                case 4: if (o == 0) put (a2, s, 0.85f); if (o == 3 || o == 6) put (b, s, 0.7f); if (q == 10) put (a, s, 0.6f); if (s % 2 == 1) put (d, s, 0.3f); break;
                case 5: if (s % 2 == 0) put (hc, s, q % 4 == 0 ? 0.5f : 0.3f); if (o == 6) put (ho, s, 0.45f); if (s % 4 == 3) put (hc, s, 0.4f); break;
                case 6: if (q == 0 || q == 3 || q == 6 || q == 10 || q == 12) put (e, s, 0.5f); if (o == 4) put (b, s, 0.75f); if (s % 2 == 1) put (d, s, 0.3f); break;
                default: if (q == 3 || q == 6 || q == 10 || q == 13) put (q % 2 ? a : a2, s, 0.75f); if (s % 4 == 0) put (hc, s, 0.45f); if (q == 15) put (b2, s, 0.5f); break;
            }
        }
        softLimit (out); return out;
    }
    inline Buf rollLoop (int kind, int v)
    {   // redobles de 2 compases que aceleran y suben: 0 snare, 1 clap, 2 kick
        const double beat = 60.0 / 172.0; auto out = make (beat * 8.0); Buf h;
        if (kind == 0) h = snare (mx (180, 215, V (v, 521)), 0.07, 0.16, 1.0, 1200, 1.3, 0.3); else if (kind == 1) h = clap (0.1, mx (1000, 1200, V (v, 522)), 3, 0.011, 1.1, 1.3); else h = kick (mx (260, 520, V (v, 523)), 50, 0.03, 0.2, 3, 0.25, 0.3);
        if (bankHas()) { if (kind == 0) h = bk ('S', v % 2 ? "Clasico" : "Generico", v, 12); else if (kind == 2) { h = bk ('K', v % 2 ? "Dash" : "Basic", v, 12); h = gated (h, 0.09); } if (kind == 0) h = gated (h, 0.16); }
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
        if (has ("Hats/Hat Layered")) return 0.97f;
        if (has ("Hats/Hat Tick")) return 0.5f;
        if (has ("Hats/Hat ") && ! has ("Hat Pedal")) return 0.8f;   // los hats de referencia llegan a -0 a -3 dBFS
        if (has ("Hats/") || has ("Shaker") || has ("Tambourine") || has ("Crash") || has ("Ride")) return 0.6f;
        if (has ("Claps/") || has ("FX/") || has ("Rim") || has ("Clave") || has ("Cowbell")) return 0.8f;
        if (has ("Sirena")) return 0.5f;
        if (has ("Snares/")) return 0.97f;   // los snares de referencia llegan a ~0 dBFS
        if (has ("Kicks/")) return 0.97f;   // los bombos reales de referencia llegan a ~0 dBFS
        return 0.89f;
    }

    // ---------- Bombos de varios tipos (cada familia se construye distinto, no solo cambia el tono) ----------
    struct KickX
    {
        double f0 = 200, f1 = 50, pd = 0.03, ad = 0.15, len = 0.6;   // cuerpo: tono inicial, final, caída del tono, caída del volumen, largo
        double pre = 3, post = 1.2, lp = 9000, hp = 28; int shape = 0;   // saturación: antes, después, filtros; forma 0 tanh, 1 recorte, 2 pliegue, 3 asimétrica, 4 bits, 5 doble etapa
        double click = 0.3, clickF = 3200;                              // golpe de ruido al inicio
        double tail = 0, tailF = 50, tailTau = 0.3;                     // cola sostenida grave (la "campana" del gabber, el rumble del hard techno)
        double knock = 0, knockF = 180, knockTau = 0.03;                // golpe de medios
        double noise = 0, noiseTau = 0.03, noiseF = 1800;              // capa de ruido (aire / suciedad)
    };
    inline double shaper (int shape, double x, double crush)
    {
        switch (shape)
        {
            case 1: return std::max (-1.0, std::min (1.0, x));
            case 2: return std::sin (x * 1.15);
            case 3: return std::tanh (x + 0.35 * std::abs (x)) - 0.1;
            case 4: { const double q = std::floor (std::tanh (x) * crush + 0.5) / crush; return q; }
            case 5: return std::tanh (std::sin (std::tanh (x) * 2.2) * 1.6);
            default: return std::tanh (x);
        }
    }
    inline Buf kickX (const KickX& k)
    {
        auto b = make (k.len); Rng r (7u + 977u * vseed()++); Bq cl, nz; cl.set (2, k.clickF, 0.8); nz.set (2, k.noiseF, 0.7);
        double ph = 0, pt = 0, pk = 0; const double norm = std::tanh (std::max (1.0, k.pre));
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / FS, f = k.f1 + (k.f0 - k.f1) * (0.75 * ex (t, k.pd) + 0.25 * ex (t, k.pd * 4.0));
            ph += TAU * f / FS; pt += TAU * k.tailF * (1.0 + 0.04 * ex (t, 0.08)) / FS; pk += TAU * k.knockF * (1.0 + 0.5 * ex (t, 0.01)) / FS;
            double x = std::sin (ph) * ex (t, k.ad) * std::min (1.0, t / 0.0005);
            x += k.tail * std::sin (pt) * ex (t, k.tailTau) * std::min (1.0, t / 0.004);
            x += k.knock * std::sin (pk) * ex (t, k.knockTau);
            double y = shaper (k.shape, x * k.pre, 6.0 + 18.0 * std::fmod (k.pre, 1.0)) / norm;
            y += k.click * cl.tick (r.next()) * ex (t, 0.0025) * 2.0;
            y += k.noise * nz.tick (r.next()) * ex (t, k.noiseTau) * 1.5;
            b[i] = (float) y;
        }
        if (k.hp > 0) run (b, 1, k.hp, 0.707);
        run (b, 0, k.lp, 0.707);
        if (k.post > 1.0) for (auto& x : b) x = (float) std::tanh (x * k.post);
        return b;
    }
    // Bombo "afinado" medido de samples reales de hard techno / gabber: casi toda la energia en 20-60 Hz, un clic de tono que cae en unos 80 ms
    // hasta una nota grave estable, poca energia por encima de 1 kHz (salvo la capa "buzz") y pico cerca de 0 dBFS.
    struct RefKick
    {
        double f0 = 300, fm = 110, f1 = 48, pd1 = 0.012, pd2 = 0.05, ad = 0.25, len = 0.45;   // tono inicial, intermedio, final; caidas; volumen; largo
        double pre = 2.0, post = 1.0, lp = 2500, hp = 22; int shape = 0;                        // saturacion
        double subAmp = 0.0, subTau = 0.3;                                                       // sub extra a la nota final (cola)
        double topF = 0, topAmp = 0, topDur = 0.04, topFm = 0;                                   // capa aguda tipo "buzz" (seno a 1-2 kHz con FM leve)
        double click = 0.0;                                                                      // golpe de ruido
    };
    inline Buf kickRef (const RefKick& k)
    {
        auto b = make (k.len); Rng r (11u + 733u * vseed()++); Bq cl; cl.set (2, 2800, 0.8);
        double ph = 0, ps = 0, pt = 0; const double norm = std::tanh (std::max (1.0, k.pre));
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / FS, f = k.f1 + (k.fm - k.f1) * ex (t, k.pd2) + (k.f0 - k.fm) * ex (t, k.pd1);
            ph += TAU * f / FS; ps += TAU * k.f1 / FS;
            double x = std::sin (ph) * ex (t, k.ad) * std::min (1.0, t / 0.0006);
            if (k.subAmp > 0) x += k.subAmp * std::sin (ps) * ex (t, k.subTau) * std::min (1.0, t / 0.006);
            double y = shaper (k.shape, x * k.pre, 8.0) / norm;
            if (k.topAmp > 0 && t < k.topDur * 2.5)
            {
                pt += TAU * k.topF * (1.0 + k.topFm * std::sin (TAU * 90.0 * t)) / FS;
                const double g = t < k.topDur ? 1.0 : ex (t - k.topDur, k.topDur * 0.3);
                y += k.topAmp * std::sin (pt) * g * ex (t, k.topDur * 1.5) * std::min (1.0, t / 0.0004);
            }
            if (k.click > 0) y += k.click * cl.tick (r.next()) * ex (t, 0.002);
            b[i] = (float) y;
        }
        if (k.hp > 0) run (b, 1, k.hp, 0.707);
        run (b, 0, k.lp, 0.707);
        if (k.post > 1.0) { const double nn = std::tanh (k.post); for (auto& x : b) x = (float) (std::tanh (x * k.post) / nn); }
        return b;
    }
    // Snare "medido": cuerpo tonal con caida de tono + capa de ruido + golpe inicial + rafaga (tipo clap) + cola difusa + parte metalica opcional.
    struct RefSnare
    {
        double f0 = 260, f1 = 170, pd = 0.02, bodyTau = 0.08, bodyAmp = 1.0, ratio2 = 1.8, amp2 = 0.0;        // cuerpo
        double nAmt = 1.0, nTau = 0.08, nHpf = 1500, nLp = 14000, nBpc = 0, nBpq = 0.6;                         // ruido principal
        double click = 0.3, burstN = 0, burstGap = 0.009, burstAmt = 0.0;                                        // golpe inicial y rafaga
        double tailAmt = 0.0, tailTau = 0.2, tailLp = 5000;                                                      // cola difusa (sala)
        double metalAmt = 0.0, metalScale = 1.0, metalTau = 0.1;                                                 // parte metalica
        double drive = 1.0, lp = 16000, hp = 60, len = 0.4; int shape = 0;
    };
    inline Buf snareRef (const RefSnare& k)
    {
        auto b = make (k.len); const size_t n = b.size(); Rng r (53u + 617u * vseed()++);
        Buf nz = make (k.len), tail = make (k.len), mt;
        for (auto& x : nz) x = (float) r.next(); for (auto& x : tail) x = (float) r.next();
        if (k.nBpc > 0) run (nz, 2, k.nBpc, k.nBpq); run (nz, 1, k.nHpf, 0.75); run (nz, 0, k.nLp, 0.7);
        if (k.tailAmt > 0) { run (tail, 0, k.tailLp, 0.7); run (tail, 1, 250.0, 0.7); }
        if (k.metalAmt > 0) { mt = metal (k.len, k.metalScale); run (mt, 2, 2600.0 * k.metalScale, 0.5); }
        Buf ck = make (k.len); { Rng r2 (91u + 41u * vseed()++); Bq c; c.set (1, 3500, 0.7); for (size_t i = 0; i < n; ++i) ck[i] = c.tick (r2.next()); }
        double ph = 0, ph2 = 0;
        for (size_t i = 0; i < n; ++i)
        {
            const double t = (double) i / FS, f = k.f1 + (k.f0 - k.f1) * ex (t, k.pd), atk = std::min (1.0, t / 0.0004);
            ph += TAU * f / FS; ph2 += TAU * f * k.ratio2 / FS;
            double y = k.bodyAmp * std::sin (ph) * ex (t, k.bodyTau) + k.amp2 * std::sin (ph2) * ex (t, k.bodyTau * 0.6);
            double ne = ex (t, k.nTau);
            if (k.burstN > 0) { for (int j = 0; j < (int) k.burstN; ++j) { const double tj = t - j * k.burstGap; if (tj >= 0) ne += k.burstAmt * ex (tj, 0.0035) * (j + 1 == (int) k.burstN ? 0.0 : 1.0); } }
            y += 2.8 * k.nAmt * nz[i] * ne;   // el ruido filtrado pierde mucha energia: se compensa para que la proporcion cuerpo/ruido sea la de los samples reales
            y += 2.0 * k.click * ck[i] * ex (t, 0.0018);
            if (k.tailAmt > 0) y += 2.8 * k.tailAmt * tail[i] * ex (t, k.tailTau) * std::min (1.0, t / 0.01);
            if (k.metalAmt > 0) y += 2.5 * k.metalAmt * mt[i] * ex (t, k.metalTau);
            b[i] = (float) (y * atk);
        }
        if (k.drive > 1.0) for (auto& x : b) x = (float) shaper (k.shape, (double) x * k.drive, 8.0) / (float) std::tanh (k.drive);
        if (k.hp > 0) run (b, 1, k.hp, 0.707);
        run (b, 0, k.lp, 0.707);
        return b;
    }
    // Rumble de hard techno: bajo corto con saw + seno saturados, para las contra-notas entre bombos
    inline Buf rumbleBass (int midi, double len)
    {
        const double f = mtof (midi); auto b = make (len); double p1 = 0, p2 = 0, ps = 0;
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / FS, dt = f / FS; p1 += dt; p1 -= std::floor (p1); p2 += dt * 1.006; p2 -= std::floor (p2); ps += TAU * f / FS;
            const double env = ex (t, len * 0.35) * std::min (1.0, t / 0.003);
            b[i] = (float) (std::tanh ((0.8 * std::sin (ps) + 0.35 * sawv (p1, dt) + 0.3 * sawv (p2, dt * 1.006)) * 3.0) * env);
        }
        run (b, 0, 260, 0.9); run (b, 1, 30, 0.707); return b;
    }
    inline Buf techLoop (int v)
    {   // 150 BPM, 2 compases: bombo duro + rumble en contratiempo + hats + clap
        const double beat = 60.0 / 150.0, step = beat / 4.0; auto out = make (beat * 8.0);
        KickX k; k.f0 = mx (170, 230, V (v, 601)); k.f1 = 50; k.pd = 0.025; k.ad = mx (0.08, 0.13, V (v, 602)); k.len = 0.4; k.pre = mx (3.5, 7, V (v, 603)); k.shape = v % 3 == 1 ? 3 : 0; k.knock = 0.7; k.knockF = 170; k.click = 0.4; k.tail = 0.25; k.tailF = 50; k.tailTau = 0.1; k.lp = 8000;
        Buf K = kickX (k), Hc = hat (0.025, 1.0, 7000, 9500, 1.0, 0.2, 1.5, 15000), Ho = hat (0.14, 1.0, 6500, 9000, 1.0, 0.2, 1.5, 15000); const Buf Cp = clap (0.16, 1400, 3, 0.011, 1.3, 1.6);
        if (bankHas()) { K = gated (bk ('K', v % 2 ? "Basic" : "Dash", v * 2, 12), beat * 0.9); Hc = bk ('H', "Tick", v, 8); Ho = gated (bk ('H', "Abierto", v, 12), beat * 0.5); }
        const Buf Rb = rumbleBass (29 + (v % 3) * 2, 0.34);
        for (int s = 0; s < 32; ++s)
        {
            const size_t at = (size_t) (s * step * FS);
            if (s % 4 == 0) mixInto (out, K, at, 1.0f);
            if (s % 4 == 2) { mixInto (out, Rb, at, 0.85f); mixInto (out, Ho, at, 0.45f); }
            if (s % 8 == 4) mixInto (out, Cp, at, 0.7f);
            if (s % 2 == 1 && (v & 1 ? s % 4 != 3 : true)) mixInto (out, Hc, at, 0.28f);
        }
        float pk = 0; for (auto x : out) pk = std::max (pk, std::abs (x)); if (pk > 1e-6f) for (auto& x : out) x = std::tanh (x / pk * 1.1f);
        return out;
    }


    // ---------- Instrumentos (metales, cuerdas, piano, organo, coro, flauta, campanas, pads, guitarra) ----------
    inline Buf ksString (double f, double len, double decay, uint32_t seed)
    {   // Karplus-Strong con retardo fraccionario
        auto b = make (len); const double N = FS / f - 0.5; const size_t M = (size_t) N + 3; std::vector<float> d (M, 0.f); Rng r (seed); Bq lp; lp.set (0, std::min (9000.0, f * 6.0), 0.7);
        for (size_t i = 0; i < M; ++i) d[i] = lp.tick (r.next());
        size_t w = 0; float prev = 0;
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double rp = (double) w - N + (double) M * 4; const size_t i0 = (size_t) rp % M, i1 = (i0 + 1) % M; const double fr = rp - std::floor (rp);
            const float y = (float) (d[i0] * (1.0 - fr) + d[i1] * fr); const float o = (float) (0.5 * (y + prev) * decay); prev = y; d[w] = o; w = (w + 1) % M; b[i] = y;
        }
        return b;
    }
    enum InstKind { Trumpet, Trombone, BrassStab, Strings, StringsStac, Pizz, Piano, EPiano, Organ, Choir, Flute, Bell, Marimba, PadWarm, PadDark, Guitar, OrchHit, NumInst };
    inline Buf instNote (int kind, int midi)
    {
        const double f = mtof (midi); const double lens[NumInst] { 1.1, 1.3, 0.55, 2.0, 0.4, 0.9, 2.4, 1.9, 1.5, 2.0, 1.3, 2.6, 1.0, 3.2, 3.2, 1.3, 0.9 };
        const double len = lens[kind]; auto b = make (len); Rng r ((uint32_t) (kind * 977 + midi * 31 + 5)); Bq lp, f1, f2, f3, nz; double p[16]; for (auto& x : p) x = 0; double ph2 = 0, ph3 = 0;
        for (int k = 0; k < 16; ++k) p[k] = 0.137 * k;
        if (kind == Guitar)
        {
            const Buf a = ksString (f, len, 0.9985, 11), c = ksString (f * 1.4983, len, 0.9985, 12), d = ksString (f * 2.0, len, 0.9985, 13); Bq hp; hp.set (1, 90, 0.7); lp.set (0, 4800, 0.7);
            for (size_t i = 0; i < b.size(); ++i) b[i] = (float) std::tanh (9.0 * hp.tick (lp.tick (a[i] + 0.8 * c[i] + 0.6 * d[i])) * 0.6) * (float) std::min (1.0, (len - (double) i / FS) / 0.1);
            return b;
        }
        if (kind == Pizz)
        {
            const Buf a = ksString (f, len, 0.9972, 21);
            for (size_t i = 0; i < b.size(); ++i) b[i] = a[i] * (float) std::min (1.0, (len - (double) i / FS) / 0.08);
            return b;
        }
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / FS, tl = len - t; double s = 0;
            const double vibD = std::min (1.0, std::max (0.0, (t - 0.18) / 0.3)), vib = std::sin (TAU * 5.4 * t) * vibD;
            double amp = 1.0;
            switch (kind)
            {
                case Trumpet: case Trombone:
                {
                    const bool tb = kind == Trombone; const double ff = f * (1.0 + 0.0035 * vib * (tb ? 0.6 : 1.0)), at = tb ? 0.08 : 0.045;
                    for (int k = 0; k < 2; ++k) { const double dt = ff * (k ? 1.003 : 1.0) / FS; p[k] += dt; p[k] -= std::floor (p[k]); s += sawv (p[k], dt); }
                    const double env = 1.0 - ex (t, tb ? 0.12 : 0.07), fc = std::max (tb ? 500.0 : 800.0, f * (tb ? 2.2 : 3.0)) * (1.0 + (tb ? 2.5 : 3.5) * env);
                    if ((i & 15) == 0) { lp.set (0, fc, 1.3); f1.set (2, tb ? 650 : 1250, 2.5); }
                    s = lp.tick (s * 0.5) + 0.5 * f1.tick (s * 0.5) + (double) r.next() * 0.06 * ex (t, 0.04);
                    amp = std::min (1.0, t / at) * (0.82 + 0.18 * ex (t, 0.3)); s = std::tanh (s * 1.5);
                    break;
                }
                case BrassStab: case OrchHit:
                {
                    static const double ch[4] { 0, 7, 12, 19 }; const bool oh = kind == OrchHit;
                    for (int k = 0; k < (oh ? 4 : 3); ++k) { const double dt = f * std::pow (2.0, (oh ? ch[k] : (k == 1 ? 7 : (k == 2 ? 12 : 0))) / 12.0) / FS; p[k] += dt; p[k] -= std::floor (p[k]); s += sawv (p[k], dt) + 0.5 * sawv (std::fmod (p[k] + 0.31, 1.0), dt); }
                    if ((i & 15) == 0) lp.set (0, (oh ? 600 : 700) + (oh ? 6500 : 4800) * ex (t, oh ? 0.06 : 0.09), 1.1);
                    s = std::tanh (lp.tick (s * 0.25) * 1.8 + (oh ? (double) r.next() * 0.25 * ex (t, 0.03) : 0.0)); amp = std::min (1.0, t / 0.006) * (0.15 + 0.85 * ex (t, oh ? 0.2 : 0.26));
                    break;
                }
                case Strings: case StringsStac:
                {
                    const bool st = kind == StringsStac;
                    for (int k = 0; k < 6; ++k) { const double cents = (k - 2.5) * 5.6 + 2.0 * std::sin (TAU * (0.3 + 0.11 * k) * t + k), dt = f * std::pow (2.0, cents / 1200.0 * (1.0 + 0.0 * vib)) * (1.0 + 0.0025 * vib * vibD) / FS; p[k] += dt; p[k] -= std::floor (p[k]); s += sawv (p[k], dt); }
                    if ((i & 15) == 0) lp.set (0, std::max (1800.0, f * 5.0) * (st ? 1.4 : 1.0), 0.8);
                    s = lp.tick (s / 6.0 * 1.6); amp = st ? std::min (1.0, t / 0.012) * ex (t, 0.13) : std::min (1.0, t / 0.22);
                    break;
                }
                case Piano:
                {
                    const double B = 0.00006 * std::pow (f / 130.0, 1.6), tau0 = std::clamp (2.6 * std::pow (130.0 / f, 0.45), 0.5, 3.0);
                    for (int k = 1; k <= 10; ++k)
                    {
                        const double fk = f * k * std::sqrt (1.0 + B * k * k); if (fk > 16000) break;
                        const double ak = std::pow (1.0 / k, 1.05) * (k == 1 ? 1.0 : 1.0 + 0.7 * ex ((double) k - 3.0, 2.0)) * std::exp (-t * (k - 1) * 0.0), dk = ex (t, tau0 / (1.0 + 0.55 * (k - 1)));
                        const double beat = 0.35 * k * (0.5 + 0.5 * std::sin (7.0 * midi + k));
                        s += ak * dk * (std::sin (TAU * fk * t) + 0.7 * std::sin (TAU * (fk + beat) * t + 1.1)) * (k > 3 ? std::min (1.0, 1.0 - 0.6 * ex (t, 0.004)) : 1.0);
                    }
                    s = s * 0.4 + (double) r.next() * 0.12 * ex (t, 0.003); amp = std::min (1.0, t / 0.0015);
                    break;
                }
                case EPiano:
                {
                    const double idx = 1.8 * ex (t, 0.45) + 0.25, idx2 = 0.9 * ex (t, 0.025);
                    s = std::sin (TAU * f * t + idx * std::sin (TAU * f * t)) + 0.35 * std::sin (TAU * f * 3.0 * t + idx2 * std::sin (TAU * f * 14.0 * t)) * ex (t, 0.2);
                    amp = std::min (1.0, t / 0.002) * (0.3 + 0.7 * ex (t, 0.9)); s *= 0.7;
                    break;
                }
                case Organ:
                {
                    static const double rt[6] { 0.5, 1.0, 1.5, 2.0, 3.0, 4.0 }, gn[6] { 0.5, 1.0, 0.5, 0.7, 0.35, 0.45 }; const double lv = 1.0 + 0.0025 * std::sin (TAU * 6.4 * t);
                    for (int k = 0; k < 6; ++k) s += gn[k] * std::sin (TAU * f * rt[k] * lv * t);
                    s = s * 0.3 + (double) r.next() * 0.08 * ex (t, 0.006); amp = std::min (1.0, t / 0.01);
                    break;
                }
                case Choir:
                {
                    for (int k = 0; k < 3; ++k) { const double dt = f * (1.0 + (k - 1) * 0.0045) * (1.0 + 0.004 * vib) / FS; p[k] += dt; p[k] -= std::floor (p[k]); s += sawv (p[k], dt); }
                    if ((i & 63) == 0) { f1.set (2, 780, 6.0); f2.set (2, 1180, 7.0); f3.set (2, 2850, 9.0); }
                    const double x = s / 3.0 + (double) r.next() * 0.05; s = (f1.tick (x) + 0.55 * f2.tick (x) + 0.3 * f3.tick (x)) * 3.2; amp = std::min (1.0, t / 0.28);
                    break;
                }
                case Flute:
                {
                    const double ff = f * (1.0 + 0.003 * vib); ph2 += TAU * ff / FS;
                    if ((i & 31) == 0) nz.set (2, f * 2.0, 1.5);
                    s = std::sin (ph2) + 0.22 * std::sin (2 * ph2) + 0.07 * std::sin (3 * ph2) + 1.0 * nz.tick (r.next()) * (0.18 + 0.5 * ex (t, 0.05)); amp = std::min (1.0, t / 0.06) * (0.9 + 0.1 * ex (t, 0.4)); s *= 0.8;
                    break;
                }
                case Bell:
                {
                    const double i1 = 3.5 * ex (t, 0.5) + 0.4;
                    s = std::sin (TAU * f * t + i1 * std::sin (TAU * f * 3.5 * t)) * ex (t, 1.5) + 0.35 * std::sin (TAU * f * 2.76 * t) * ex (t, 0.7) + 0.2 * std::sin (TAU * f * 5.4 * t) * ex (t, 0.25);
                    amp = std::min (1.0, t / 0.001); s *= 0.7;
                    break;
                }
                case Marimba:
                {
                    s = std::sin (TAU * f * t) * ex (t, 0.4) + 0.35 * std::sin (TAU * f * 3.93 * t) * ex (t, 0.05) + (double) r.next() * 0.1 * ex (t, 0.002); amp = std::min (1.0, t / 0.001);
                    break;
                }
                case PadWarm: case PadDark:
                {
                    const bool dk = kind == PadDark;
                    for (int k = 0; k < 5; ++k) { const double cents = (k - 2) * (dk ? 11.0 : 15.0) + 4.0 * std::sin (TAU * (0.2 + 0.07 * k) * t), dt = f * std::pow (2.0, cents / 1200.0) / FS; p[k] += dt; p[k] -= std::floor (p[k]); s += sawv (p[k], dt); }
                    if (dk) { const double dt = f * 0.5 / FS; p[6] += dt; p[6] -= std::floor (p[6]); s += 1.5 * sqv (p[6], dt); }
                    if ((i & 31) == 0) lp.set (0, (dk ? 650.0 : 1300.0) * (1.0 + 0.6 * std::sin (TAU * 0.13 * t) * 0.5 + 0.3 * (1.0 - ex (t, 0.8))), dk ? 1.4 : 0.9);
                    s = lp.tick (s / 5.0 * 1.5); if (dk) s = std::tanh (s * 1.8); amp = std::min (1.0, t / (dk ? 0.5 : 0.7));
                    break;
                }
                default: break;
            }
            const double relT = kind == PadWarm || kind == PadDark ? 0.9 : (kind == Strings || kind == Choir ? 0.35 : (kind == Bell || kind == Piano || kind == EPiano ? 0.25 : 0.08));
            b[i] = (float) (s * amp * std::min (1.0, tl / relT));
        }
        return b;
    }


    // ---------- Leads agresivos (hakken, mentasm, eurodance, gated, gabber, laser, acid, scream, donk, stab) ----------
    struct AggVar { double drv = 1.0, cut = 1.0, det = 1.0, gl = 1.0; };
    enum AggKind { Hakken, Mentasm, EuroDance, Gated, GabberLead, Laser, AcidLead, Scream, Donk, StabHard, NumAgg };
    inline Buf aggroLead (int kind, int midi, const AggVar& av = AggVar())
    {
        const double f = mtof (midi), pf = std::sqrt (f / 261.6) * av.cut;
        const double lens[NumAgg] { 0.45, 0.9, 0.6, 0.76, 0.5, 0.35, 0.4, 0.6, 0.3, 0.32 }; const double len = lens[kind];
        auto b = make (len); Rng r ((uint32_t) (kind * 733 + midi * 17 + 3)); double p[12]; for (int k = 0; k < 12; ++k) p[k] = 0.113 * k + 0.05;
        Bq lp, lp2, bp, hp; hp.set (1, 120, 0.7); double ps = 0, pm = 0;
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / FS, tl = len - t; double s = 0, amp = 1.0;
            if (kind == Hakken)
            {   // sierra + cuadrada con salto de tono hacia abajo y filtro resonante: el "hakken" clasico
                const double g = std::pow (2.0, 12.0 * av.gl * ex (t, 0.03) / 12.0);
                for (int k = 0; k < 3; ++k) { const double dt = f * g * std::pow (2.0, (k - 1) * 22.0 * av.det / 1200.0) / FS; p[k] += dt; p[k] -= std::floor (p[k]); s += sawv (p[k], dt); }
                { const double dt = f * g / FS; p[3] += dt; p[3] -= std::floor (p[3]); s += 0.9 * sqv (p[3], dt); const double d2 = 0.5 * f * g / FS; p[4] += d2; p[4] -= std::floor (p[4]); s += 0.7 * sawv (p[4], d2); }
                if ((i & 15) == 0) lp.set (0, (900 + 6500 * ex (t, 0.12)) * pf, 3.8);
                s = std::tanh (lp.tick (s * 0.3) * 5.5); amp = std::min (1.0, t / 0.001) * (0.55 + 0.45 * ex (t, 0.2));
            }
            else if (kind == Mentasm)
            {   // hoover: PWM de 5 sierras muy abiertas que se hunden desde arriba
                const double g = std::pow (2.0, 7.0 * av.gl * ex (t, 0.09) / 12.0);
                for (int k = 0; k < 5; ++k)
                {
                    const double dt = f * g * std::pow (2.0, ((k - 2) * 24.0 + 6.0 * std::sin (TAU * (0.5 + 0.13 * k) * t)) * av.det / 1200.0) / FS; p[k] += dt; p[k] -= std::floor (p[k]);
                    double q = p[k] + 0.5 + 0.32 * std::sin (TAU * (0.9 + 0.2 * k) * t + k); q -= std::floor (q); s += sawv (p[k], dt) - sawv (q, dt);
                }
                if ((i & 15) == 0) { lp.set (0, (2600 + 3800 * ex (t, 0.35)) * pf, 2.6); bp.set (2, 1500 * pf, 1.2); }
                const double x = s * 0.22; s = std::tanh ((lp.tick (x) + 0.5 * bp.tick (x)) * 4.5); amp = std::min (1.0, t / 0.004) * (0.8 + 0.2 * ex (t, 0.4));
            }
            else if (kind == EuroDance)
            {   // supersaw brillante + pulso en octava, ataque seco, bailable
                for (int k = 0; k < 7; ++k) { const double dt = f * std::pow (2.0, (k - 3) * 16.0 * av.det / 1200.0) / FS; p[k] += dt; p[k] -= std::floor (p[k]); s += sawv (p[k], dt); }
                { const double dt = f * 2.0 / FS; p[8] += dt; p[8] -= std::floor (p[8]); s += 1.6 * sqv (p[8], dt); const double d2 = 0.5 * f / FS; p[9] += d2; p[9] -= std::floor (p[9]); s += 1.5 * sawv (p[9], d2); }
                if ((i & 15) == 0) lp.set (0, (3000 + 7000 * ex (t, 0.18)) * pf, 1.5);
                s = std::tanh (lp.tick (s * 0.16) * 3.2); amp = std::min (1.0, t / 0.002) * (0.45 + 0.55 * ex (t, 0.32));
            }
            else if (kind == Gated)
            {   // supersaw de 9 voces cortado en semicorcheas (a 160 BPM): puro movimiento
                for (int k = 0; k < 9; ++k) { const double dt = f * std::pow (2.0, (k - 4) * 14.0 * av.det / 1200.0) / FS; p[k] += dt; p[k] -= std::floor (p[k]); s += sawv (p[k], dt); }
                { const double dt = f * 2.0 / FS; p[9] += dt; p[9] -= std::floor (p[9]); s += sawv (p[9], dt); }
                const double step = 60.0 / 160.0 / 4.0, ph = std::fmod (t, step) / step, gate = std::min (1.0, ph / 0.03) * (ph < 0.62 ? 1.0 : std::max (0.0, 1.0 - (ph - 0.62) / 0.05));
                if ((i & 15) == 0) lp.set (0, (4200 + 2000 * (1.0 - gate)) * pf, 1.2);
                s = std::tanh (lp.tick (s * 0.13) * 3.0); amp = gate * (0.8 + 0.2 * ex (t, 0.3));
            }
            else if (kind == GabberLead)
            {   // sierras desafinadas + cuadrada, saturacion brutal y filtro: el lead de gabber de siempre
                for (int k = 0; k < 4; ++k) { const double dt = f * std::pow (2.0, (k - 1.5) * 13.0 * av.det / 1200.0) / FS; p[k] += dt; p[k] -= std::floor (p[k]); s += sawv (p[k], dt); }
                { const double dt = f / FS; p[5] += dt; p[5] -= std::floor (p[5]); s += 1.2 * sqv (p[5], dt); }
                s = std::tanh (s * 0.25 * 14.0); if ((i & 15) == 0) lp.set (0, 5200 * pf, 0.9); s = lp.tick (s) * 0.9; s = std::tanh (s * 2.0);
                amp = std::min (1.0, t / 0.002) * (0.8 + 0.2 * ex (t, 0.25));
            }
            else if (kind == Laser)
            {   // FM con caida de tono muy rapida: laser de rave
                const double g = std::pow (2.0, 24.0 * av.gl * ex (t, 0.05) / 12.0); ps += TAU * f * g / FS; pm += TAU * f * g * 2.0 / FS;
                s = std::sin (ps + (5.0 * ex (t, 0.09) + 0.8) * std::sin (pm)); s = std::tanh (s * 2.2); amp = std::min (1.0, t / 0.001) * (0.25 + 0.75 * ex (t, 0.14));
            }
            else if (kind == AcidLead)
            {   // 303 agresivo: sierra con resonancia alta y acento
                const double dt = f / FS; p[0] += dt; p[0] -= std::floor (p[0]); if ((i & 7) == 0) lp.set (0, (300 + 4200 * ex (t, 0.12)) * pf, 11.0);
                s = std::tanh (lp.tick (sawv (p[0], dt)) * 3.2); amp = std::min (1.0, t / 0.001) * (0.4 + 0.6 * ex (t, 0.22));
            }
            else if (kind == Scream)
            {   // chillido: sierras con tono que sube, formante y plegado de onda
                const double g = std::pow (2.0, -3.0 * av.gl * ex (t, 0.06) / 12.0);
                for (int k = 0; k < 4; ++k) { const double dt = f * g * std::pow (2.0, (k - 1.5) * 10.0 * av.det / 1200.0) / FS; p[k] += dt; p[k] -= std::floor (p[k]); s += sawv (p[k], dt); }
                if ((i & 15) == 0) { bp.set (2, 1900 * pf, 4.5); lp.set (0, 6500 * pf, 1.0); }
                const double x = s * 0.25; s = std::sin (2.6 * (x + 1.4 * bp.tick (x))); s = std::tanh (lp.tick (s) * 2.2); amp = std::min (1.0, t / 0.002) * (0.7 + 0.3 * ex (t, 0.3));
            }
            else if (kind == Donk)
            {   // donk: golpe de tono que cae, cuerpo corto saturado
                ps += TAU * f * (1.0 + 1.6 * ex (t, 0.018)) / FS; const double dt = f / FS; p[0] += dt; p[0] -= std::floor (p[0]);
                s = std::tanh (3.5 * (std::sin (ps) + 0.4 * sqv (p[0], dt))); if ((i & 15) == 0) lp.set (0, 3500 * pf, 1.2); s = lp.tick (s); amp = std::min (1.0, t / 0.0008) * ex (t, 0.14);
            }
            else
            {   // stab duro: acorde menor de sierras y cuadradas, cerrado y saturado
                static const double ch[4] { 0, 3, 7, 12 };
                for (int k = 0; k < 4; ++k) { const double dt = f * std::pow (2.0, ch[k] / 12.0) / FS; p[k] += dt; p[k] -= std::floor (p[k]); s += sawv (p[k], dt) + 0.6 * sqv (std::fmod (p[k] + 0.2, 1.0), dt); }
                if ((i & 15) == 0) lp.set (0, (1800 + 7000 * ex (t, 0.09)) * pf, 1.8);
                s = std::tanh (lp.tick (s * 0.18) * 4.0); amp = std::min (1.0, t / 0.001) * (0.25 + 0.75 * ex (t, 0.13));
            }
            if (av.drv != 1.0) s = std::tanh (s * av.drv) / std::tanh (av.drv);
            s = hp.tick (s);
            b[i] = (float) (s * amp * std::min (1.0, tl / 0.03));
        }
        return b;
    }


    // ---------- Catálogo ----------
    inline std::string num (int i) { return (i < 10 ? "0" : "") + std::to_string (i); }
    inline std::string noteName (int midi) { static const char* n[12] { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" }; return std::string (n[midi % 12]) + std::to_string (midi / 12 - 1); }

    // Llama a emit(rutaRelativa, buffer a 88.2 kHz) por cada sonido. Devuelve cuántos generó.
    template <class Emit> int catalog (Emit&& emit)
    {
        int count = 0; Bank bank; bankPtr() = &bank;
        auto out = [&] (const std::string& p, Buf&& b)
        {   // los kicks, snares y hats nuevos (hasta el 12 de cada familia) se guardan para armar los bucles
            if (p.size() > 7 && (p.rfind ("Kicks/Kick ", 0) == 0 || p.rfind ("Snares/Snare ", 0) == 0 || p.rfind ("Hats/Hat ", 0) == 0))
            {
                const int n = std::atoi (p.substr (p.size() - 6, 2).c_str());
                if (n >= 1 && n <= 12) { Buf c = b; float pk = 0; for (auto x : c) pk = std::max (pk, std::abs (x)); const float tgt = p[0] == 'K' ? 1.0f : (p[0] == 'S' ? 0.9f : 0.55f); if (pk > 1e-6f) for (auto& x : c) x *= tgt / pk; bank[p] = std::move (c); }
            }
            emit (p, std::move (b)); ++count;
        };
        // Kicks (150)
        // Kicks: familias medidas de samples reales (ver RefKick). Afinados entre 40 y 55 Hz (E1 a A1).
        auto tune = [&] (double v) { return 38.0 * std::pow (1.5, v); };   // 38 a 57 Hz, continuo
        for (int i = 0; i < 20; ++i) { RefKick k; k.f0 = mx (300, 380, VH (i, 101)); k.fm = mx (85, 130, VH (i, 102)); k.f1 = tune (VH (i, 103)); k.pd1 = mx (0.016, 0.028, VH (i, 104)); k.pd2 = mx (0.03, 0.05, VH (i, 105)); k.ad = mx (0.2, 0.3, VH (i, 106)); k.len = mx (0.38, 0.5, VH (i, 107));
            k.pre = mx (1.3, 3.5, VH (i, 108)); k.shape = (int) (VH (i, 191) * 4.99) % 4 == 3 ? 5 : (int) (VH (i, 191) * 4.99) % 4; k.lp = mx (700, 4200, VH (i, 109)); k.click = VH (i, 192) > 0.6 ? mx (0.04, 0.2, VH (i, 193)) : 0.0; k.subAmp = mx (0.15, 0.5, VH (i, 110)); k.subTau = 0.2; out ("Kicks/Kick Basic " + num (i + 1) + ".wav", kickRef (k)); }
        for (int i = 0; i < 16; ++i) { RefKick k; k.f0 = mx (85, 130, VH (i, 111)); k.fm = mx (62, 80, VH (i, 112)); k.f1 = tune (VH (i, 113)); k.pd1 = mx (0.006, 0.012, VH (i, 114)); k.pd2 = mx (0.05, 0.09, VH (i, 115)); k.ad = mx (0.22, 0.34, VH (i, 116)); k.len = mx (0.42, 0.55, VH (i, 117));
            k.pre = mx (1.0, 4.5, VH (i, 118)); k.shape = VH (i, 194) > 0.7 ? 2 : 0; k.lp = mx (400, 2600, VH (i, 119)); k.subAmp = mx (0.2, 0.6, VH (i, 120)); k.subTau = 0.25; out ("Kicks/Kick Sub " + num (i + 1) + ".wav", kickRef (k)); }
        for (int i = 0; i < 12; ++i) { RefKick k; k.f0 = mx (190, 240, VH (i, 121)); k.fm = mx (90, 110, VH (i, 122)); k.f1 = tune (VH (i, 123)); k.pd1 = mx (0.004, 0.008, VH (i, 124)); k.pd2 = mx (0.07, 0.12, VH (i, 125)); k.ad = mx (0.16, 0.22, VH (i, 126)); k.len = mx (0.34, 0.4, VH (i, 127));
            k.pre = mx (1.6, 3.0, VH (i, 128)); k.lp = mx (1200, 3500, VH (i, 129)); k.subAmp = mx (0.0, 0.7, VH (i, 195)); k.subTau = mx (0.1, 0.3, VH (i, 196)); k.shape = (int) (VH (i, 197) * 3.99); k.click = mx (0.0, 0.25, VH (i, 130)); out ("Kicks/Kick Dash " + num (i + 1) + ".wav", kickRef (k)); }
        for (int i = 0; i < 16; ++i) { RefKick k; k.f0 = mx (120, 220, VH (i, 131)); k.fm = mx (80, 140, VH (i, 132)); k.f1 = mx (50, 60, VH (i, 133)); k.pd1 = 0.01; k.pd2 = mx (0.03, 0.06, VH (i, 134)); k.ad = mx (0.24, 0.34, VH (i, 135)); k.len = mx (0.5, 0.6, VH (i, 136));
            k.pre = mx (3, 20, VH (i, 137)); k.lp = mx (3000, 7000, VH (i, 138)); k.post = mx (1.5, 3.0, VH (i, 139)); k.subAmp = mx (0.4, 0.9, VH (i, 140)); k.subTau = 0.3;
            k.topF = mx (1100, 2000, VH (i, 141)); k.topAmp = mx (0.25, 1.0, VH (i, 142)); k.topDur = mx (0.025, 0.07, VH (i, 143)); k.topFm = mx (0.0, 0.4, VH (i, 144)); k.shape = (int) (VH (i, 211) * 3.99); out ("Kicks/Kick Buzz " + num (i + 1) + ".wav", kickRef (k)); }
        for (int i = 0; i < 14; ++i) { RefKick k; k.f0 = mx (45, 420, VH (i, 151)); k.fm = mx (50, 160, VH (i, 152)); k.f1 = mx (41, 50, VH (i, 153)); k.pd1 = 0.01; k.pd2 = mx (0.04, 0.08, VH (i, 154)); k.ad = mx (0.28, 0.4, VH (i, 155)); k.len = mx (0.6, 0.7, VH (i, 156));
            k.pre = mx (7, 40, VH (i, 157)); { static const int sh[] { 1, 2, 3, 5, 1 }; k.shape = sh[(int) (VH (i, 198) * 4.99)]; } k.lp = mx (1500, 6500, VH (i, 158)); k.post = mx (1.0, 5.0, VH (i, 159)); k.click = mx (0.0, 0.3, VH (i, 199));
            if (VH (i, 200) > 0.6) { k.topF = mx (900, 3200, VH (i, 201)); k.topAmp = mx (0.2, 0.6, VH (i, 202)); k.topDur = mx (0.015, 0.04, VH (i, 203)); } k.subAmp = mx (0.6, 1.6, VH (i, 160)); k.subTau = mx (0.2, 0.6, VH (i, 204)); out ("Kicks/Kick Burst " + num (i + 1) + ".wav", kickRef (k)); }
        for (int i = 0; i < 10; ++i) { RefKick k; k.f0 = mx (300, 700, VH (i, 161)); k.fm = k.f0 * mx (0.5, 0.95, VH (i, 162)); k.f1 = mx (36, 44, VH (i, 163)); k.pd1 = 0.02; k.pd2 = mx (0.018, 0.03, VH (i, 164)); k.ad = mx (0.5, 0.9, VH (i, 165)); k.len = mx (1.0, 1.5, VH (i, 166));
            k.pre = mx (1.4, 3.0, VH (i, 167)); k.lp = mx (1500, 4000, VH (i, 168)); k.subAmp = 0.8; k.subTau = mx (0.5, 0.9, VH (i, 169)); out ("Kicks/Kick Cola " + num (i + 1) + ".wav", kickRef (k)); }
        for (int i = 0; i < 20; ++i) { RefKick k; k.f0 = mx (200, 600, VH (i, 171)); k.fm = mx (70, 220, VH (i, 172)); k.f1 = mx (44, 58, VH (i, 173)); k.pd1 = mx (0.008, 0.014, VH (i, 174)); k.pd2 = mx (0.05, 0.08, VH (i, 175)); k.ad = mx (0.35, 0.6, VH (i, 176)); k.len = mx (0.7, 1.1, VH (i, 177));
            k.pre = mx (3, 22, VH (i, 178)); { static const int sh[] { 0, 1, 2, 3, 5 }; k.shape = sh[(int) (VH (i, 205) * 4.99)]; } k.lp = mx (1800, 6500, VH (i, 179)); k.post = mx (1.0, 4.0, VH (i, 180));
            if (VH (i, 206) > 0.65) { k.topF = mx (700, 2200, VH (i, 207)); k.topAmp = mx (0.15, 0.5, VH (i, 208)); k.topDur = mx (0.02, 0.05, VH (i, 209)); k.topFm = mx (0.0, 0.3, VH (i, 210)); } k.subAmp = mx (0.3, 0.8, VH (i, 181)); k.subTau = mx (0.3, 0.5, VH (i, 182)); k.click = mx (0.05, 0.2, VH (i, 183)); out ("Kicks/Kick Gabber " + num (i + 1) + ".wav", kickRef (k)); }
        // Hats: familias medidas de samples reales (ver RefHat). Cada parametro varia por su cuenta.
        for (int i = 0; i < 14; ++i) { RefHat h; h.t40 = mx (0.032, 0.065, VH (i, 301)); h.fastMix = mx (0.2, 0.6, VH (i, 302)); h.scale = mx (0.8, 1.7, VH (i, 303)); h.metalAmt = mx (0.2, 1.0, VH (i, 304)); h.noise = mx (0.7, 1.5, VH (i, 305)); h.hpf = mx (4500, 8200, VH (i, 306)); h.lp = mx (11000, 17000, VH (i, 307)); h.bpc = mx (6500, 10500, VH (i, 308)); h.grit = VH (i, 309) > 0.5 ? mx (1.2, 2.5, VH (i, 310)) : 1.0; h.attack = mx (0.0001, 0.0006, VH (i, 311)); out ("Hats/Hat Corto " + num (i + 1) + ".wav", hatRef (h)); }
        for (int i = 0; i < 8; ++i) { RefHat h; h.t40 = mx (0.028, 0.045, VH (i, 321)); h.fastMix = 0.3; h.scale = mx (0.7, 1.3, VH (i, 322)); h.metalAmt = mx (0.7, 1.4, VH (i, 323)); h.noise = mx (0.2, 0.7, VH (i, 324)); h.hpf = mx (3000, 5500, VH (i, 325)); h.lp = mx (9000, 14000, VH (i, 326)); h.bpc = mx (5000, 7500, VH (i, 327)); h.bpq = mx (0.5, 1.2, VH (i, 328)); out ("Hats/Hat Tick " + num (i + 1) + ".wav", hatRef (h)); }
        for (int i = 0; i < 12; ++i) { RefHat h; h.t40 = mx (0.13, 0.4, VH (i, 331)); h.fastMix = mx (0.2, 0.5, VH (i, 332)); h.scale = mx (0.9, 1.5, VH (i, 333)); h.metalAmt = mx (0.8, 1.3, VH (i, 334)); h.noise = mx (0.1, 0.3, VH (i, 335)); h.hpf = mx (4500, 7000, VH (i, 336)); h.lp = mx (12000, 16000, VH (i, 337)); h.bpc = mx (6500, 9500, VH (i, 338)); out ("Hats/Hat Limpio " + num (i + 1) + ".wav", hatRef (h)); }
        for (int i = 0; i < 12; ++i) { RefHat h; h.t40 = mx (0.2, 0.3, VH (i, 341)); h.fastMix = mx (0.3, 0.6, VH (i, 342)); h.scale = mx (0.9, 1.6, VH (i, 343)); h.metalAmt = mx (0.9, 1.6, VH (i, 344)); h.noise = mx (0.15, 0.45, VH (i, 345)); h.hpf = mx (3800, 6500, VH (i, 346)); h.lp = mx (11000, 16000, VH (i, 347)); h.bpc = mx (7000, 10000, VH (i, 348)); h.grit = VH (i, 349) > 0.6 ? 1.6 : 1.0; out ("Hats/Hat Core " + num (i + 1) + ".wav", hatRef (h)); }
        for (int i = 0; i < 8; ++i) { RefHat h; h.t40 = mx (0.1, 0.34, VH (i, 351)); h.fastMix = mx (0.2, 0.5, VH (i, 352)); h.scale = mx (0.9, 1.5, VH (i, 353)); h.metalAmt = mx (0.9, 1.6, VH (i, 354)); h.noise = mx (0.08, 0.3, VH (i, 355)); h.hpf = mx (4500, 7000, VH (i, 356)); h.bounceHz = mx (14, 42, VH (i, 357)); h.bounceDepth = mx (0.35, 0.75, VH (i, 358)); out ("Hats/Hat Bouncy " + num (i + 1) + ".wav", hatRef (h)); }
        for (int i = 0; i < 8; ++i) { RefHat h; h.t40 = mx (0.18, 0.55, VH (i, 361)); h.fastMix = mx (0.2, 0.45, VH (i, 362)); h.scale = mx (1.3, 2.1, VH (i, 363)); h.metalAmt = mx (0.9, 1.6, VH (i, 364)); h.noise = mx (0.06, 0.25, VH (i, 365)); h.hpf = mx (6500, 11500, VH (i, 366)); h.lp = mx (14000, 19000, VH (i, 367)); h.bpc = mx (10000, 13000, VH (i, 368)); out ("Hats/Hat Agudo " + num (i + 1) + ".wav", hatRef (h)); }
        for (int i = 0; i < 12; ++i) { RefHat h; h.t40 = mx (0.36, 0.5, VH (i, 371)); h.fastMix = mx (0.15, 0.4, VH (i, 372)); h.scale = mx (0.8, 1.5, VH (i, 373)); h.metalAmt = mx (0.8, 1.3, VH (i, 374)); h.noise = mx (0.16, 0.42, VH (i, 375)); h.hpf = mx (4200, 7200, VH (i, 376)); h.lp = mx (11000, 16000, VH (i, 377)); h.bpc = mx (6500, 9500, VH (i, 378)); h.grit = VH (i, 379) > 0.5 ? mx (1.2, 2.2, VH (i, 380)) : 1.0; out ("Hats/Hat Abierto " + num (i + 1) + ".wav", hatRef (h)); }
        for (int i = 0; i < 8; ++i) { RefHat h; h.t40 = mx (0.2, 0.55, VH (i, 381)); h.fastMix = mx (0.2, 0.5, VH (i, 382)); h.scale = mx (0.9, 1.6, VH (i, 383)); h.metalAmt = mx (0.9, 1.6, VH (i, 384)); h.noise = mx (0.1, 0.35, VH (i, 385)); h.hpf = mx (4000, 6500, VH (i, 386)); h.layerAmt = mx (0.3, 0.9, VH (i, 387)); h.layerHpf = mx (6000, 13000, VH (i, 388)); h.grit = mx (1.3, 2.6, VH (i, 389)); out ("Hats/Hat Layered " + num (i + 1) + ".wav", hatRef (h)); }
        for (int i = 0; i < 6; ++i) { RefHat h; h.t40 = mx (0.45, 0.62, VH (i, 391)); h.len = 1.7; h.fastMix = mx (0.05, 0.2, VH (i, 392)); h.scale = mx (1.0, 2.0, VH (i, 393)); h.metalAmt = mx (0.8, 1.4, VH (i, 394)); h.noise = mx (0.12, 0.35, VH (i, 395)); h.hpf = mx (5500, 8500, VH (i, 396)); h.bpc = mx (7500, 10500, VH (i, 397)); h.lp = mx (13000, 17000, VH (i, 398)); out ("Hats/Hat Largo " + num (i + 1) + ".wav", hatRef (h)); }
        // Snares: familias medidas de samples reales (ver RefSnare). Cada parametro varia por su cuenta.
        auto sn = [&] (const char* nm, int count, auto&& setup) { for (int i = 0; i < count; ++i) { RefSnare k; setup (k, i); out (std::string ("Snares/Snare ") + nm + " " + num (i + 1) + ".wav", snareRef (k)); } };
        sn ("Clasico", 12, [&] (RefSnare& k, int i) { k.f0 = mx (150, 230, VH (i, 401)); k.f1 = mx (118, 170, VH (i, 402)); k.pd = mx (0.012, 0.03, VH (i, 403)); k.bodyTau = mx (0.06, 0.14, VH (i, 404)); k.amp2 = mx (0.0, 0.5, VH (i, 405)); k.ratio2 = mx (1.5, 2.4, VH (i, 406));
            k.nAmt = mx (0.59, 1.10, VH (i, 407)); k.nTau = mx (0.07, 0.14, VH (i, 408)); k.nHpf = mx (900, 2400, VH (i, 409)); k.nLp = mx (9000, 15000, VH (i, 410)); k.click = mx (0.1, 0.5, VH (i, 411)); k.drive = mx (1.0, 2.2, VH (i, 412)); k.shape = (int) (VH (i, 413) * 2.99); k.len = mx (0.3, 0.45, VH (i, 414)); });
        sn ("Oscuro", 10, [&] (RefSnare& k, int i) { k.f0 = mx (170, 260, VH (i, 421)); k.f1 = mx (150, 200, VH (i, 422)); k.pd = mx (0.01, 0.03, VH (i, 423)); k.bodyTau = mx (0.05, 0.11, VH (i, 424)); k.bodyAmp = mx (0.9, 1.4, VH (i, 425)); k.amp2 = mx (0.0, 0.4, VH (i, 426));
            k.nAmt = mx (0.88, 1.98, VH (i, 427)); k.nTau = mx (0.05, 0.1, VH (i, 428)); k.nHpf = mx (250, 2000, VH (i, 429)); k.nLp = mx (1800, 9000, VH (i, 430)); k.click = mx (0.0, 0.3, VH (i, 431)); k.drive = mx (1.2, 3.0, VH (i, 432)); k.shape = (int) (VH (i, 433) * 3.99); k.len = mx (0.18, 0.28, VH (i, 434)); });
        sn ("Crowd", 10, [&] (RefSnare& k, int i) { k.f0 = mx (300, 420, VH (i, 441)); k.f1 = mx (250, 340, VH (i, 442)); k.pd = mx (0.008, 0.02, VH (i, 443)); k.bodyTau = mx (0.03, 0.08, VH (i, 444)); k.bodyAmp = mx (0.5, 1.0, VH (i, 445));
            k.nAmt = mx (0.44, 0.77, VH (i, 446)); k.nTau = mx (0.08, 0.16, VH (i, 447)); k.nHpf = mx (840, 2100, VH (i, 448)); k.nLp = mx (6750, 11250, VH (i, 449)); k.burstN = (int) (3 + VH (i, 450) * 4.99); k.burstGap = mx (0.005, 0.014, VH (i, 451)); k.burstAmt = mx (0.6, 1.2, VH (i, 452)); k.click = mx (0.0, 0.3, VH (i, 453)); k.tailAmt = mx (0.1, 0.5, VH (i, 454)); k.tailTau = mx (0.05, 0.14, VH (i, 455)); k.len = mx (0.22, 0.4, VH (i, 456)); });
        sn ("Brillante", 14, [&] (RefSnare& k, int i) { k.f0 = mx (170, 250, VH (i, 461)); k.f1 = mx (130, 200, VH (i, 462)); k.pd = mx (0.01, 0.025, VH (i, 463)); k.bodyTau = mx (0.04, 0.1, VH (i, 464)); k.bodyAmp = mx (0.4, 0.9, VH (i, 465));
            k.nAmt = mx (0.66, 1.21, VH (i, 466)); k.nTau = mx (0.1, 0.22, VH (i, 467)); k.nHpf = mx (1750, 4550, VH (i, 468)); k.nLp = mx (9000, 14250, VH (i, 469)); k.click = mx (0.2, 0.8, VH (i, 470)); k.drive = mx (1.0, 2.5, VH (i, 471)); k.shape = (int) (VH (i, 472) * 2.99); k.len = mx (0.26, 0.48, VH (i, 473)); });
        sn ("Hype", 10, [&] (RefSnare& k, int i) { k.f0 = mx (150, 380, VH (i, 481)); k.f1 = mx (130, 300, VH (i, 482)); k.pd = mx (0.01, 0.03, VH (i, 483)); k.bodyTau = mx (0.04, 0.2, VH (i, 484)); k.amp2 = mx (0.0, 0.4, VH (i, 485));
            k.nAmt = mx (0.42, 0.84, VH (i, 486)); k.nTau = mx (0.08, 0.15, VH (i, 487)); k.nHpf = mx (800, 2600, VH (i, 488)); k.nLp = mx (7000, 14000, VH (i, 489)); k.tailAmt = mx (0.5, 1.2, VH (i, 490)); k.tailTau = mx (0.07, 0.22, VH (i, 491)); k.tailLp = mx (1500, 9000, VH (i, 492)); k.click = mx (0.1, 0.5, VH (i, 493)); k.drive = mx (1.0, 2.4, VH (i, 494)); k.len = mx (0.4, 0.65, VH (i, 495)); });
        sn ("Seco", 10, [&] (RefSnare& k, int i) { k.f0 = mx (210, 330, VH (i, 501)); k.f1 = mx (190, 260, VH (i, 502)); k.pd = mx (0.004, 0.012, VH (i, 503)); k.bodyTau = mx (0.012, 0.03, VH (i, 504)); k.bodyAmp = mx (0.7, 1.3, VH (i, 505));
            k.nAmt = mx (0.60, 1.12, VH (i, 506)); k.nTau = mx (0.02, 0.045, VH (i, 507)); k.nHpf = mx (700, 2450, VH (i, 508)); k.nLp = mx (6000, 11250, VH (i, 509)); k.click = mx (0.2, 0.8, VH (i, 510)); k.drive = mx (1.2, 3.0, VH (i, 511)); k.shape = (int) (VH (i, 512) * 3.99); k.len = mx (0.1, 0.2, VH (i, 513)); });
        sn ("Melodico", 10, [&] (RefSnare& k, int i) { k.f0 = mx (260, 420, VH (i, 521)); k.f1 = mx (180, 290, VH (i, 522)); k.pd = mx (0.03, 0.08, VH (i, 523)); k.bodyTau = mx (0.07, 0.15, VH (i, 524)); k.bodyAmp = mx (1.0, 1.6, VH (i, 525)); k.amp2 = mx (0.2, 0.7, VH (i, 526)); k.ratio2 = mx (1.5, 3.0, VH (i, 527));
            k.nAmt = mx (0.75, 2.00, VH (i, 528)); k.nTau = mx (0.04, 0.09, VH (i, 529)); k.nHpf = mx (1500, 4000, VH (i, 530)); k.nLp = mx (8000, 14000, VH (i, 531)); k.click = mx (0.05, 0.3, VH (i, 532)); k.drive = mx (1.0, 2.0, VH (i, 533)); k.len = mx (0.25, 0.4, VH (i, 534)); });
        sn ("Metalico", 10, [&] (RefSnare& k, int i) { k.f0 = mx (200, 300, VH (i, 541)); k.f1 = mx (180, 260, VH (i, 542)); k.pd = mx (0.01, 0.03, VH (i, 543)); k.bodyTau = mx (0.04, 0.1, VH (i, 544)); k.bodyAmp = mx (0.5, 1.0, VH (i, 545));
            k.nAmt = mx (0.21, 0.56, VH (i, 546)); k.nTau = mx (0.06, 0.14, VH (i, 547)); k.nHpf = mx (1500, 3750, VH (i, 548)); k.metalAmt = mx (0.35, 1.0, VH (i, 549)); k.metalScale = mx (0.5, 1.4, VH (i, 550)); k.metalTau = mx (0.07, 0.18, VH (i, 551)); k.click = mx (0.1, 0.4, VH (i, 552)); k.drive = mx (1.0, 2.2, VH (i, 553)); k.len = mx (0.25, 0.42, VH (i, 554)); });
        sn ("Generico", 12, [&] (RefSnare& k, int i) { k.f0 = mx (210, 330, VH (i, 561)); k.f1 = mx (170, 260, VH (i, 562)); k.pd = mx (0.01, 0.03, VH (i, 563)); k.bodyTau = mx (0.04, 0.1, VH (i, 564)); k.bodyAmp = mx (0.6, 1.2, VH (i, 565)); k.amp2 = mx (0.0, 0.5, VH (i, 566));
            k.nAmt = mx (0.56, 1.05, VH (i, 567)); k.nTau = mx (0.06, 0.12, VH (i, 568)); k.nHpf = mx (900, 3000, VH (i, 569)); k.nLp = mx (7200, 12800, VH (i, 570)); k.click = mx (0.1, 0.5, VH (i, 571)); k.drive = mx (1.0, 2.5, VH (i, 572)); k.shape = (int) (VH (i, 573) * 2.99); k.len = mx (0.25, 0.42, VH (i, 574)); });
        // Claps (48)
        for (int i = 0; i < 42; ++i) out ("Claps/Clap 909 " + num (i + 1) + ".wav", clap (mx (0.035, 0.09, V (i, 111)), mx (700, 1250, V (i, 112)), 2 + (int) (V (i, 115) * 3.0), mx (0.008, 0.014, V (i, 113)), mx (0.9, 2.0, V (i, 114)), mx (0.9, 1.8, V (i, 116))));
        for (int i = 0; i < 24; ++i) out ("Claps/Clap 808 " + num (i + 1) + ".wav", clap (mx (0.05, 0.13, V (i, 121)), mx (700, 1200, V (i, 122)), 2 + (int) (V (i, 125) * 3.0), mx (0.009, 0.02, V (i, 123)), mx (0.8, 1.6, V (i, 124)), mx (0.8, 1.4, V (i, 126))));
        for (int i = 0; i < 36; ++i) out ("Claps/Clap Gabber " + num (i + 1) + ".wav", clap (mx (0.06, 0.3, V (i, 131)), mx (600, 1500, V (i, 132)), 3 + (int) (V (i, 135) * 3.0), mx (0.006, 0.014, V (i, 133)), mx (0.6, 1.6, V (i, 136)), mx (2.0, 6.0, V (i, 134))));
        // Hats (110): el 909 es casi todo ruido con un poco de metal y crujido; el 808 es metálico puro
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
        // Instrumentos nuevos: metales, cuerdas, piano, organo, coro, flauta, campanas, pads y guitarra (una nota cada 2 semitonos)
        {
            struct Ins { const char* name; int kind; int lo, hi; };
            static const Ins ins[] { { "Trompeta", Trumpet, 58, 82 }, { "Trombon", Trombone, 41, 65 }, { "Metales Stab", BrassStab, 46, 70 }, { "Cuerdas", Strings, 46, 82 }, { "Cuerdas Staccato", StringsStac, 46, 82 },
                { "Pizzicato", Pizz, 48, 84 }, { "Piano", Piano, 36, 84 }, { "Piano Electrico", EPiano, 40, 80 }, { "Organo", Organ, 36, 76 }, { "Coro Ah", Choir, 48, 78 }, { "Flauta", Flute, 60, 90 },
                { "Campanas", Bell, 60, 96 }, { "Marimba", Marimba, 48, 84 }, { "Pad Calido", PadWarm, 36, 72 }, { "Pad Oscuro", PadDark, 29, 65 }, { "Guitarra Distorsionada", Guitar, 33, 57 }, { "Golpe Orquestal", OrchHit, 36, 60 } };
            for (const auto& n : ins) for (int m = n.lo; m <= n.hi; m += 2) out (std::string ("Instrumentos/") + n.name + " " + noteName (m) + ".wav", instNote (n.kind, m));
            for (int k = 0; k < 22; ++k) out ("Leads/Lead Trance " + noteName (rn[k]) + ".wav", synthVoice ({ mtof (rn[k]), { 0.0, -12.0 }, 7, 18, 0, 0.1, 0.006, 0.9, 0.75, 1.2, 0.18, 4200, 1.2, 0.6, 1.1 }));
            for (int k = 0; k < 22; ++k) out ("Leads/Lead Hardstyle " + noteName (rn[k]) + ".wav", synthVoice ({ mtof (rn[k]), { 0.0, 12.0 }, 7, 25, 12, 0.025, 0.002, 0.5, 0.7, 0.55, 0.1, 5200, 1.5, 0.8, 1.8 }));
            for (int k = 0; k < 22; ++k) out ("Leads/Lead Euro " + noteName (rn[k]) + ".wav", synthVoice ({ mtof (rn[k]), { 0.0 }, 2, 6, 0, 0.1, 0.01, 0.8, 0.8, 1.0, 0.12, 2800, 2.0, 1.2, 1.5 }));
        {   // variantes de los leads agresivos (mas saturadas, mas oscuras, mas abiertas): 11 notas cada una
            static const AggVar vars[3] { { 1.8, 1.0, 1.5, 1.0 }, { 1.0, 0.65, 0.6, 1.5 }, { 2.5, 1.3, 2.0, 0.5 } }; static const char* vn[3] { "B ", "C ", "D " };
            static const char* an2[NumAgg] { "Lead Hakken ", "Lead Mentasm ", "Lead Eurodance ", "Lead Gated ", "Lead Gabber ", "Lead Laser ", "Lead Acid ", "Lead Scream ", "Lead Donk ", "Stab Hard " };
            for (int a = 0; a < NumAgg; ++a) for (int v = 0; v < 3; ++v) for (int k = 0; k < 22; k += 2) out (std::string ("Leads/") + an2[a] + vn[v] + noteName (rn[k]) + ".wav", aggroLead (a, rn[k], vars[v]));
        }
        {   // Leads agresivos nuevos (hard dance, eurodance, hakken, gabber): 22 notas cada uno
            static const char* an[NumAgg] { "Lead Hakken ", "Lead Mentasm ", "Lead Eurodance ", "Lead Gated ", "Lead Gabber ", "Lead Laser ", "Lead Acid ", "Lead Scream ", "Lead Donk ", "Stab Hard " };
            for (int a = 0; a < NumAgg; ++a) for (int k = 0; k < 22; ++k) out (std::string ("Leads/") + an[a] + noteName (rn[k]) + ".wav", aggroLead (a, rn[k]));
        }
        }
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
        for (int i = 0; i < 18; ++i) out ("Loops/Loop 909 172 BPM " + num (i + 1) + ".wav", drumLoop (0, i));
        for (int i = 0; i < 18; ++i) out ("Loops/Loop Gabber 172 BPM " + num (i + 1) + ".wav", drumLoop (1, i));
        for (int i = 0; i < 24; ++i) out ("Loops/Loop Hardcore 172 BPM " + num (i + 1) + ".wav", drumLoop (2, i));
        // Bucles extra (58)
        for (int i = 0; i < 12; ++i) out ("Loops/Hat Loop 909 172 BPM " + num (i + 1) + ".wav", hatLoop (0, i));
        for (int i = 0; i < 6; ++i) out ("Loops/Hat Loop Hardcore 172 BPM " + num (i + 1) + ".wav", hatLoop (1, i));
        { static const char* nm[] { "Sub", "Reese", "Acid", "808", "Gabber", "Donk", "Fuzz" }; static const int ty[] { 1, 2, 4, 5, 6, 0 }; (void) nm;
          for (int i = 0; i < 36; ++i) { const int t = ty[i % 6]; out ("Loops/Bass Loop " + std::string (nm[t]) + " 172 BPM " + num (i + 1) + ".wav", bassLoop (t, i)); } }
        for (int i = 0; i < 24; ++i) out ("Loops/Perc Loop 172 BPM " + num (i + 1) + ".wav", percLoop (i));
        for (int i = 0; i < 8; ++i) out ("Loops/Snare Roll 172 BPM " + num (i + 1) + ".wav", rollLoop (0, i));
        for (int i = 0; i < 6; ++i) out ("Loops/Clap Roll 172 BPM " + num (i + 1) + ".wav", rollLoop (1, i));
        for (int i = 0; i < 6; ++i) out ("Loops/Kick Roll 172 BPM " + num (i + 1) + ".wav", rollLoop (2, i));
        // Familias de bombos de estilo (Rotterdam, Frenchcore, Old School, Industrial, Screech, Hard Techno, Rumble, Saturado): mismo motor medido que los demas (84 bombos)
        {
            static const int shp[5] { 0, 1, 2, 3, 5 };
            for (int i = 0; i < 12; ++i) { RefKick k; k.f0 = mx (320, 620, VH (i, 301)); k.fm = mx (90, 125, VH (i, 302)); k.f1 = mx (41, 58, VH (i, 303)); k.pd1 = mx (0.010, 0.016, VH (i, 304)); k.pd2 = mx (0.035, 0.055, VH (i, 305)); k.ad = mx (0.2, 0.42, VH (i, 306)); k.len = mx (0.7, 0.9, VH (i, 307));
                k.pre = mx (5, 24, VH (i, 308)); k.shape = shp[(int) (VH (i, 309) * 2.99) % 3 == 0 ? 1 : ((int) (VH (i, 309) * 2.99) % 3 == 1 ? 3 : 0)]; k.lp = mx (2800, 8500, VH (i, 310)); k.post = mx (1.5, 2.5, VH (i, 311)); k.subAmp = mx (0.5, 1.0, VH (i, 312)); k.subTau = mx (0.3, 0.5, VH (i, 313)); k.click = mx (0.1, 0.3, VH (i, 314)); out ("Kicks/Kick Rotterdam " + num (i + 1) + ".wav", kickRef (k)); }
            for (int i = 0; i < 10; ++i) { RefKick k; k.f0 = mx (600, 1000, VH (i, 321)); k.fm = mx (120, 200, VH (i, 322)); k.f1 = mx (40, 48, VH (i, 323)); k.pd1 = mx (0.012, 0.02, VH (i, 324)); k.pd2 = mx (0.04, 0.06, VH (i, 325)); k.ad = mx (0.5, 0.8, VH (i, 326)); k.len = mx (1.0, 1.3, VH (i, 327));
                k.pre = mx (15, 40, VH (i, 328)); k.shape = i % 2 ? 2 : 5; k.lp = mx (2500, 6000, VH (i, 329)); k.post = mx (2.0, 3.5, VH (i, 330)); k.subAmp = mx (0.6, 1.2, VH (i, 331)); k.subTau = mx (0.5, 0.9, VH (i, 332)); k.click = 0.15; out ("Kicks/Kick Frenchcore " + num (i + 1) + ".wav", kickRef (k)); }
            for (int i = 0; i < 10; ++i) { RefKick k; k.f0 = mx (250, 450, VH (i, 341)); k.fm = mx (90, 130, VH (i, 342)); k.f1 = mx (55, 80, VH (i, 343)); k.pd1 = 0.010; k.pd2 = mx (0.025, 0.04, VH (i, 344)); k.ad = mx (0.10, 0.18, VH (i, 345)); k.len = mx (0.35, 0.5, VH (i, 346));
                k.pre = mx (1.5, 4.0, VH (i, 347)); k.shape = i % 2 ? 3 : 0; k.lp = mx (6000, 12000, VH (i, 348)); k.click = mx (0.3, 0.7, VH (i, 349)); out ("Kicks/Kick Old School " + num (i + 1) + ".wav", kickRef (k)); }
            for (int i = 0; i < 10; ++i) { RefKick k; k.f0 = mx (300, 600, VH (i, 351)); k.fm = mx (100, 160, VH (i, 352)); k.f1 = mx (48, 60, VH (i, 353)); k.pd1 = 0.010; k.pd2 = mx (0.035, 0.05, VH (i, 354)); k.ad = mx (0.15, 0.25, VH (i, 355)); k.len = mx (0.5, 0.6, VH (i, 356));
                k.pre = mx (3, 8, VH (i, 357)); k.shape = i % 2 ? 1 : 2; k.lp = mx (5000, 10000, VH (i, 358)); k.post = mx (1.2, 2.0, VH (i, 359)); k.topF = mx (1500, 3500, VH (i, 360)); k.topAmp = mx (0.5, 1.0, VH (i, 361)); k.topDur = mx (0.02, 0.05, VH (i, 362)); k.topFm = mx (0.1, 0.4, VH (i, 363)); k.click = mx (0.6, 1.0, VH (i, 364)); k.subAmp = 0.4; k.subTau = 0.15; out ("Kicks/Kick Industrial " + num (i + 1) + ".wav", kickRef (k)); }
            for (int i = 0; i < 8; ++i) { RefKick k; k.f0 = mx (900, 1600, VH (i, 371)); k.fm = mx (150, 250, VH (i, 372)); k.f1 = mx (50, 70, VH (i, 373)); k.pd1 = 0.008; k.pd2 = mx (0.025, 0.04, VH (i, 374)); k.ad = mx (0.2, 0.3, VH (i, 375)); k.len = 0.5;
                k.pre = mx (6, 14, VH (i, 376)); k.shape = 2; k.lp = mx (3000, 5000, VH (i, 377)); k.post = 1.5; k.topF = mx (2000, 3500, VH (i, 378)); k.topAmp = 0.6; k.topDur = mx (0.02, 0.04, VH (i, 379)); k.topFm = 0.3; k.click = 0.2; out ("Kicks/Kick Screech " + num (i + 1) + ".wav", kickRef (k)); }
            for (int i = 0; i < 14; ++i) { RefKick k; k.f0 = mx (130, 320, VH (i, 381)); k.fm = mx (80, 110, VH (i, 382)); k.f1 = mx (43, 60, VH (i, 383)); k.pd1 = 0.012; k.pd2 = mx (0.03, 0.045, VH (i, 384)); k.ad = mx (0.14, 0.36, VH (i, 385)); k.len = mx (0.5, 0.6, VH (i, 386));
                k.pre = mx (2, 11, VH (i, 387)); k.shape = i % 3 == 0 ? 3 : (i % 3 == 1 ? 0 : 1); k.lp = mx (6000, 10000, VH (i, 388)); k.post = mx (1.3, 2.0, VH (i, 389)); k.subAmp = mx (0.0, 0.9, VH (i, 390)); k.subTau = 0.15; k.click = mx (0.3, 0.6, VH (i, 391)); out ("Kicks/Kick Hard Techno " + num (i + 1) + ".wav", kickRef (k)); }
            for (int i = 0; i < 10; ++i) { RefKick k; k.f0 = mx (160, 240, VH (i, 401)); k.fm = mx (80, 100, VH (i, 402)); k.f1 = mx (40, 52, VH (i, 403)); k.pd1 = 0.012; k.pd2 = mx (0.035, 0.05, VH (i, 404)); k.ad = mx (0.35, 0.5, VH (i, 405)); k.len = mx (1.0, 1.4, VH (i, 406));
                k.pre = mx (3, 6, VH (i, 407)); k.shape = i % 2 ? 3 : 0; k.lp = mx (2500, 6000, VH (i, 408)); k.post = 1.3; k.hp = 30; k.subAmp = mx (1.0, 1.6, VH (i, 409)); k.subTau = mx (0.5, 0.9, VH (i, 410)); k.click = 0.3; out ("Kicks/Kick Rumble " + num (i + 1) + ".wav", kickRef (k)); }
            for (int i = 0; i < 10; ++i) { RefKick k; k.f0 = mx (200, 350, VH (i, 421)); k.fm = mx (80, 140, VH (i, 422)); k.f1 = 50; k.pd1 = 0.012; k.pd2 = 0.05; k.ad = mx (0.25, 0.35, VH (i, 423)); k.len = 0.7;
                k.pre = mx (2, 30, VH (i, 424)); k.shape = shp[i % 5]; k.lp = mx (3000, 11000, VH (i, 425)); k.post = mx (1.0, 2.5, VH (i, 426)); k.subAmp = 0.5; k.subTau = 0.3; k.click = mx (0.1, 0.4, VH (i, 427)); out ("Kicks/Kick Saturado " + num (i + 1) + ".wav", kickRef (k)); }
        }
        // Hard techno: hats, claps, percusión, bajos y bucles
        for (int i = 0; i < 6; ++i) out ("Claps/Clap Techno " + num (i + 1) + ".wav", clap (mx (0.1, 0.28, V (i, 821)), mx (1100, 1800, V (i, 822)), 2 + (int) (V (i, 823) * 3.0), mx (0.009, 0.015, V (i, 824)), mx (1.0, 1.8, V (i, 825)), mx (1.3, 2.2, V (i, 826))));
        for (int i = 0; i < 4; ++i) { const double f0 = mx (360, 220, i / 3.0); out ("Percusiones/Perc Techno Tom " + num (i + 1) + ".wav", tomDist (f0, f0 * 0.6, mx (0.08, 0.16, V (i, 831)), mx (2.5, 5.0, V (i, 832)))); }
        for (int i = 0; i < 4; ++i) out ("Percusiones/Perc Techno Rim " + num (i + 1) + ".wav", rim (mx (520, 760, V (i, 841)), mx (1900, 2600, V (i, 842))));
        { static const int rn[8] { 24, 26, 27, 29, 31, 32, 34, 36 }; for (int k = 0; k < 8; ++k) out ("Bajos/Bajo Rumble " + noteName (rn[k]) + ".wav", rumbleBass (rn[k], 0.9)); }
        for (int i = 0; i < 6; ++i) out ("Loops/Loop Hard Techno 150 BPM " + num (i + 1) + ".wav", techLoop (i));
        bankPtr() = nullptr;
        return count;
    }
}
