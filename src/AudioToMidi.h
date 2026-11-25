#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>
#include <vector>

// Audio a MIDI para melodías de una sola voz (voz tarareada, un lead, un bajo): detecta el tono con el método YIN y agrupa los
// tramos estables en notas. No sirve para acordes ni para batería.
namespace audio2midi
{
    struct Note { int pitch = 60; double start = 0, len = 0; int vel = 90; };   // segundos

    // Frecuencia (Hz) de un trozo de audio, o 0 si no hay un tono claro.
    inline double yin (const float* x, int w, double sr, double fmin = 60.0, double fmax = 1500.0, float threshold = 0.15f)
    {
        const int half = w / 2, tauMin = std::max (2, (int) (sr / fmax)), tauMax = std::min (half - 1, (int) (sr / fmin)); if (tauMax <= tauMin + 2) return 0.0;
        std::vector<float> d ((size_t) tauMax + 2, 0.f);
        for (int t = 1; t <= tauMax; ++t) { double s = 0; for (int j = 0; j < half; ++j) { const double v = (double) x[j] - (double) x[j + t]; s += v * v; } d[(size_t) t] = (float) s; }
        std::vector<float> c ((size_t) tauMax + 2, 1.f); double run = 0; for (int t = 1; t <= tauMax; ++t) { run += d[(size_t) t]; c[(size_t) t] = run > 0 ? (float) (d[(size_t) t] * t / run) : 1.f; }
        int best = -1; for (int t = tauMin; t <= tauMax; ++t) if (c[(size_t) t] < threshold) { while (t + 1 <= tauMax && c[(size_t) t + 1] < c[(size_t) t]) ++t; best = t; break; }
        if (best < 0) return 0.0;
        double tau = best; if (best > 1 && best < tauMax) { const double a = c[(size_t) best - 1], b = c[(size_t) best], cc = c[(size_t) best + 1], den = a - 2 * b + cc; if (std::abs (den) > 1e-12) tau = best + 0.5 * (a - cc) / den; }
        return tau > 0 ? sr / tau : 0.0;
    }
    inline int midiOf (double hz) { return hz <= 0 ? -1 : (int) std::lround (69.0 + 12.0 * std::log2 (hz / 440.0)); }

    // Convierte un audio mono en notas. Máximo 60 s de audio.
    inline std::vector<Note> convert (const std::vector<float>& mono, double sr, double gateDb = -45.0, double minNoteSec = 0.05)
    {
        std::vector<Note> out; const int W = 2048, hop = 512; const int n = (int) std::min<size_t> (mono.size(), (size_t) (60.0 * sr)); if (n < W + hop || sr < 8000.0) return out;
        const int frames = (n - W) / hop + 1; std::vector<int> m ((size_t) frames, -1); std::vector<float> lev ((size_t) frames, 0.f); const float gate = (float) juce::Decibels::decibelsToGain (gateDb);
        for (int f = 0; f < frames; ++f)
        {
            const float* p = mono.data() + (size_t) f * hop; double e = 0; for (int i = 0; i < W; ++i) e += (double) p[i] * p[i]; const float rms = (float) std::sqrt (e / W); lev[(size_t) f] = rms;
            if (rms >= gate) m[(size_t) f] = midiOf (yin (p, W, sr));
        }
        { auto s = m; for (int f = 1; f + 1 < frames; ++f) { int a = m[(size_t) f - 1], b = m[(size_t) f], c = m[(size_t) f + 1]; if (a == c && b != a) s[(size_t) f] = a; } m = s; }   // un fotograma suelto distinto no cuenta
        int f = 0;
        while (f < frames)
        {
            if (m[(size_t) f] < 0) { ++f; continue; } int g = f; float peakRms = 0.f; while (g < frames && m[(size_t) g] == m[(size_t) f]) { peakRms = std::max (peakRms, lev[(size_t) g]); ++g; }
            const double st = ((double) f * hop + W * 0.5 - hop * 0.5) / sr, en = ((double) (g - 1) * hop + W * 0.5 + hop * 0.5) / sr;
            if (en - st >= minNoteSec) { Note nt; nt.pitch = juce::jlimit (0, 127, m[(size_t) f]); nt.start = std::max (0.0, st); nt.len = en - nt.start; const double db = juce::Decibels::gainToDecibels (peakRms); nt.vel = juce::jlimit (30, 120, (int) std::lround (40.0 + (db + 40.0) * (80.0 / 34.0))); out.push_back (nt); }
            f = g;
        }
        return out;
    }
}
