#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <vector>

// Paquete de sonidos propio de INJUSTICE DAW: se genera por código la primera vez (sin derechos de autor).
namespace samplepack
{
    constexpr double SR = 44100.0;
    constexpr double TAU = 6.283185307179586;
    using Buf = std::vector<float>;

    struct Rng { uint32_t s; explicit Rng (uint32_t seed) : s (seed) {} float next() { s = s * 1664525u + 1013904223u; return (float) (s >> 8) * (1.f / 8388608.f) - 1.f; } };
    inline Buf make (double secs) { return Buf ((size_t) (secs * SR), 0.f); }
    inline float lpA (double fc) { return (float) (1.0 - std::exp (-TAU * fc / SR)); }

    inline void lowpass (Buf& b, double fc) { float y = 0, a = lpA (fc); for (auto& x : b) { y += a * (x - y); x = y; } }
    inline void highpass (Buf& b, double fc) { float y = 0, a = lpA (fc); for (auto& x : b) { y += a * (x - y); x -= y; } }

    inline void save (const juce::File& f, Buf b)
    {
        float pk = 0; for (auto x : b) pk = std::max (pk, std::abs (x)); if (pk < 1e-6f) return;
        const float g = 0.89f / pk; const int n = (int) b.size(), fade = (int) (SR * 0.003);
        juce::AudioBuffer<float> buf (1, n); for (int i = 0; i < n; ++i) { float v = b[(size_t) i] * g; if (i > n - fade) v *= (float) (n - i) / (float) fade; buf.setSample (0, i, v); }
        f.getParentDirectory().createDirectory(); f.deleteFile(); juce::WavAudioFormat wav;
        if (auto os = f.createOutputStream()) if (std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), SR, 1, 24, {}, 0)); w) { os.release(); w->writeFromAudioSampleBuffer (buf, 0, n); }
    }

    inline Buf kick (double f0, double f1, double pitchDecay, double ampDecay, double drive, double click, double len)
    {
        auto b = make (len); double ph = 0; Rng r (7);
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / SR, f = f1 + (f0 - f1) * std::exp (-t / pitchDecay); ph += TAU * f / SR;
            const double env = std::exp (-t / ampDecay) * std::min (1.0, t / 0.0006);
            double s = std::sin (ph); s = std::tanh (s * drive) * env;
            s += click * r.next() * std::exp (-t / 0.0018);
            b[i] = (float) s;
        }
        lowpass (b, 9000); for (auto& x : b) x = std::tanh (x * 1.4f); return b;
    }
    inline Buf snare (double tone, double decay, double noiseAmt)
    {
        auto b = make (0.5); Rng r (11); double ph = 0;
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / SR; ph += TAU * (tone + 90.0 * std::exp (-t / 0.02)) / SR;
            b[i] = (float) (0.7 * std::sin (ph) * std::exp (-t / 0.06) + noiseAmt * r.next() * std::exp (-t / decay));
        }
        highpass (b, 180); for (auto& x : b) x = std::tanh (x * 1.6f); return b;
    }
    inline Buf clap (double tail, double lowCut)
    {
        auto b = make (0.6); Rng r (23); const double st[] { 0.0, 0.011, 0.023, 0.036 };
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / SR; double e = 0;
            for (int k = 0; k < 3; ++k) if (t >= st[k] && t < st[k + 1]) e = std::exp (-(t - st[k]) / 0.005);
            if (t >= st[3]) e = std::exp (-(t - st[3]) / tail);
            b[i] = r.next() * (float) e;
        }
        highpass (b, lowCut); lowpass (b, 6500); return b;
    }
    inline Buf hat (double decay, double cut)
    {
        auto b = make (decay * 6 + 0.05); const double fr[] { 205.3, 304.4, 369.6, 522.7, 540.0, 800.0 };
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / SR; double s = 0;
            for (double f : fr) s += std::sin (TAU * f * 7.3 * t) > 0 ? 1.0 : -1.0;
            b[i] = (float) (s / 6.0 * std::exp (-t / decay));
        }
        highpass (b, cut); return b;
    }
    inline Buf tom (double f0, double f1, double decay)
    {
        auto b = make (decay * 5); double ph = 0;
        for (size_t i = 0; i < b.size(); ++i) { const double t = (double) i / SR; ph += TAU * (f1 + (f0 - f1) * std::exp (-t / 0.04)) / SR; b[i] = (float) (std::sin (ph) * std::exp (-t / decay)); }
        return b;
    }
    inline Buf rim()
    {
        auto b = make (0.2); Rng r (5);
        for (size_t i = 0; i < b.size(); ++i) { const double t = (double) i / SR; b[i] = (float) ((std::sin (TAU * 1700 * t) * 0.5 + std::sin (TAU * 480 * t) * 0.6) * std::exp (-t / 0.02) + 0.5 * r.next() * std::exp (-t / 0.004)); }
        return b;
    }
    inline Buf cowbell()
    {
        auto b = make (0.5);
        for (size_t i = 0; i < b.size(); ++i) { const double t = (double) i / SR; const double s = (std::sin (TAU * 540 * t) > 0 ? 1.0 : -1.0) + (std::sin (TAU * 800 * t) > 0 ? 1.0 : -1.0); b[i] = (float) (s * 0.4 * std::exp (-t / 0.12)); }
        highpass (b, 400); lowpass (b, 4500); return b;
    }
    // Bajos de una sola nota (Fa1); 0 sub, 1 reese, 2 acid
    inline Buf bass (int type)
    {
        auto b = make (1.6); const double f = 43.654; double p1 = 0, p2 = 0, p3 = 0; float y = 0, y2 = 0;
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / SR; const double env = std::min (1.0, t / 0.004) * std::exp (-t / (type == 2 ? 0.5 : 1.1));
            p1 += f / SR; p2 += f * 1.004 / SR; p3 += f * 0.996 / SR; auto saw = [] (double& p) { p -= std::floor (p); return 2.0 * p - 1.0; };
            double s;
            if (type == 0) s = std::sin (TAU * p1) + 0.18 * std::sin (TAU * p1 * 2);
            else if (type == 1) { s = 0.5 * (saw (p1) + saw (p2) + saw (p3)); const float a = lpA (420); y += a * ((float) s - y); s = y * 1.6 + 0.5 * std::sin (TAU * p1); }
            else { s = saw (p1); const float a = lpA (180 + 2600 * std::exp (-t / 0.12)); y += a * ((float) s - y); y2 += a * (y - y2); s = y2 * 2.2; }
            b[i] = (float) std::tanh (s * env * (type == 0 ? 1.0 : 1.5));
        }
        return b;
    }
    inline Buf riser (bool up)
    {
        auto b = make (4.0); Rng r (3); float y = 0;
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / SR / 4.0, k = up ? t : 1.0 - t; const float a = lpA (200.0 * std::pow (60.0, k));
            y += a * (r.next() - y); b[i] = y * (float) (0.15 + 0.85 * k * k);
        }
        return b;
    }
    inline Buf impact()
    {
        auto b = make (2.0); Rng r (9); double ph = 0; float y = 0;
        for (size_t i = 0; i < b.size(); ++i)
        {
            const double t = (double) i / SR; ph += TAU * (30.0 + 60.0 * std::exp (-t / 0.25)) / SR; y += lpA (1200) * (r.next() - y);
            b[i] = (float) (std::sin (ph) * std::exp (-t / 0.9) + 0.6 * y * std::exp (-t / 0.5));
        }
        return b;
    }
    inline Buf laser()
    {
        auto b = make (0.35); double ph = 0;
        for (size_t i = 0; i < b.size(); ++i) { const double t = (double) i / SR; ph += TAU * (200.0 + 3300.0 * std::exp (-t / 0.07)) / SR; b[i] = (float) (std::sin (ph) * std::exp (-t / 0.15)); }
        return b;
    }

    inline juce::File packDir (const juce::File& root) { return root.getChildFile ("INJUSTICE Pack"); }

    // Crea el paquete si falta. Devuelve la carpeta.
    inline juce::File ensure (const juce::File& root)
    {
        auto d = packDir (root); auto marker = d.getChildFile (".pack_v1");
        if (marker.existsAsFile()) return d;
        auto K = d.getChildFile ("Kicks"), S = d.getChildFile ("Snares"), C = d.getChildFile ("Claps"), H = d.getChildFile ("Hats"), P = d.getChildFile ("Percusiones"), B = d.getChildFile ("Bajos"), X = d.getChildFile ("FX");
        save (K.getChildFile ("Kick Gabber 01.wav"), kick (480, 48, 0.035, 0.38, 6, 0.25, 0.7));
        save (K.getChildFile ("Kick Gabber 02.wav"), kick (420, 42, 0.05, 0.5, 9, 0.2, 0.9));
        save (K.getChildFile ("Kick Gabber 03 Duro.wav"), kick (600, 52, 0.025, 0.3, 12, 0.4, 0.6));
        save (K.getChildFile ("Kick Hardcore.wav"), kick (350, 55, 0.04, 0.28, 4, 0.3, 0.5));
        save (K.getChildFile ("Kick Punch.wav"), kick (300, 60, 0.02, 0.15, 3, 0.3, 0.3));
        save (K.getChildFile ("Kick Rumble Sub.wav"), kick (200, 38, 0.06, 0.7, 2.5, 0.05, 1.2));
        save (S.getChildFile ("Snare Seco.wav"), snare (190, 0.09, 0.7));
        save (S.getChildFile ("Snare Hardcore.wav"), snare (210, 0.16, 1.0));
        save (S.getChildFile ("Snare Largo.wav"), snare (175, 0.3, 0.8));
        save (C.getChildFile ("Clap Corto.wav"), clap (0.07, 700));
        save (C.getChildFile ("Clap Normal.wav"), clap (0.14, 600));
        save (C.getChildFile ("Clap Sala.wav"), clap (0.3, 500));
        save (H.getChildFile ("Hat Cerrado 01.wav"), hat (0.03, 7000));
        save (H.getChildFile ("Hat Cerrado 02.wav"), hat (0.05, 6000));
        save (H.getChildFile ("Hat Abierto.wav"), hat (0.25, 6500));
        save (P.getChildFile ("Tom Grave.wav"), tom (170, 80, 0.22));
        save (P.getChildFile ("Tom Medio.wav"), tom (240, 130, 0.18));
        save (P.getChildFile ("Perc Rim.wav"), rim());
        save (P.getChildFile ("Perc Cowbell.wav"), cowbell());
        save (B.getChildFile ("Bajo Sub F1.wav"), bass (0));
        save (B.getChildFile ("Bajo Reese F1.wav"), bass (1));
        save (B.getChildFile ("Bajo Acid F1.wav"), bass (2));
        save (X.getChildFile ("FX Riser.wav"), riser (true));
        save (X.getChildFile ("FX Downlifter.wav"), riser (false));
        save (X.getChildFile ("FX Impacto.wav"), impact());
        save (X.getChildFile ("FX Laser.wav"), laser());
        marker.replaceWithText ("1"); return d;
    }
}
