#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

// Detector de tempo y tono de un archivo de audio.
//  - Bucles (más de ~1.5 s): estima el BPM por autocorrelación de los golpes y lo ajusta al largo exacto del archivo si éste cuadra con un número entero de tiempos;
//    el tono (tonalidad) sale de los perfiles de Krumhansl sobre el espectro agrupado en 12 notas.
//  - Sonidos sueltos (bombos, bajos, tonos): da la nota fundamental más fuerte (útil para afinar un kick con el bajo).
namespace sampleanalysis
{
    struct Result
    {
        bool ok = false, oneShot = false;
        double seconds = 0, bpm = 0, bpmConf = 0, hz = 0;
        int keyPc = -1; bool minor = false; int midi = -1;   // keyPc: 0 = Do ... 11 = Si
        float keyConf = 0.f;
    };
    inline juce::String pcName (int pc) { static const char* n[] { "Do", "Do#", "Re", "Re#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" }; return n[((pc % 12) + 12) % 12]; }
    inline juce::String noteName (int midi) { return pcName (midi) + juce::String (midi / 12 - 1); }

    inline bool readMono (const juce::File& f, std::vector<float>& out, double& sr, double maxSec = 60.0)
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f)); if (! r || r->lengthInSamples < 256) return false;
        sr = r->sampleRate; const int n = (int) std::min<juce::int64> (r->lengthInSamples, (juce::int64) (maxSec * sr));
        juce::AudioBuffer<float> b ((int) std::max<unsigned> (1u, r->numChannels), n); r->read (&b, 0, n, 0, true, true);
        out.assign ((size_t) n, 0.f); const int nc = b.getNumChannels();
        for (int c = 0; c < nc; ++c) { auto* p = b.getReadPointer (c); for (int i = 0; i < n; ++i) out[(size_t) i] += p[i] / (float) nc; }
        return true;
    }

    // Tempo por autocorrelación de la envolvente de golpes. Devuelve BPM entre 70 y 210 (0 si no hay pulso claro).
    inline double estimateBpm (const std::vector<float>& x, double sr, double& conf, double totalSeconds)
    {
        const int hop = 256; const int nf = (int) (x.size() / (size_t) hop); if (nf < 64) { conf = 0; return 0; }
        std::vector<double> env ((size_t) nf);
        for (int i = 0; i < nf; ++i) { double e = 0; for (int k = 0; k < hop; ++k) { const double v = x[(size_t) (i * hop + k)]; e += v * v; } env[(size_t) i] = std::log1p (400.0 * std::sqrt (e / hop)); }
        std::vector<double> flux ((size_t) nf, 0.0); double mean = 0;
        for (int i = 1; i < nf; ++i) { flux[(size_t) i] = std::max (0.0, env[(size_t) i] - env[(size_t) i - 1]); mean += flux[(size_t) i]; }
        mean /= nf; for (auto& v : flux) v = std::max (0.0, v - mean);
        const double fps = sr / hop; double best = 0, bestScore = -1, sum = 0; int cnt = 0;
        for (double bpm = 70.0; bpm <= 210.0; bpm += 0.5)
        {
            const double lag = 60.0 / bpm * fps; double s = 0; int m = 0;
            for (int i = 0; i + (int) lag + 1 < nf; ++i) { const double a = flux[(size_t) i]; if (a <= 0) continue; const int l0 = (int) lag; const double fr = lag - l0; s += a * (flux[(size_t) (i + l0)] * (1 - fr) + flux[(size_t) (i + l0 + 1)] * fr); ++m; }
            if (m == 0) continue; s /= (double) m; sum += s; ++cnt;
            const double w = 1.0 + 0.15 * std::exp (-0.5 * std::pow ((bpm - 160.0) / 45.0, 2.0));   // preferencia suave por la zona de gabber / hardcore
            if (s * w > bestScore) { bestScore = s * w; best = bpm; }
        }
        if (cnt == 0 || best <= 0) { conf = 0; return 0; }
        auto scoreAt = [&] (double bpm)
        {
            const double lag = 60.0 / bpm * fps; double sc = 0; int m = 0;
            for (int i = 0; i + (int) lag + 1 < nf; ++i) { const double a = flux[(size_t) i]; if (a <= 0) continue; const int l0 = (int) lag; const double fr = lag - l0; sc += a * (flux[(size_t) (i + l0)] * (1 - fr) + flux[(size_t) (i + l0 + 1)] * fr); ++m; }
            return m ? sc / m : 0.0;
        };
        conf = bestScore / (sum / cnt + 1e-12);
        // si el archivo dura un número entero de tiempos (bucles), solo valen los tempos que cuadran con ese largo; se elige el que mejor late
        {
            auto prior = [] (double b) { return 1.0 + 0.15 * std::exp (-0.5 * std::pow ((b - 160.0) / 45.0, 2.0)); };
            double pick = 0, pickScore = -1;
            for (int beats : { 4, 8, 12, 16, 24, 32, 48, 64 })
            {
                const double c = 60.0 * beats / totalSeconds; if (c < 70.0 || c > 210.0) continue;
                bool near = false; for (double f : { 1.0, 2.0, 0.5, 1.5, 1.0 / 1.5, 2.0 / 3.0, 3.0 / 2.0 }) if (std::abs (c - best * f) / (best * f) < 0.04) near = true;
                if (! near) continue; const double sc = scoreAt (c) * prior (c); if (sc > pickScore) { pickScore = sc; pick = c; }
            }
            if (pick > 0) best = pick;
        }
        if (best < 105.0 && best * 2.0 <= 210.0 && scoreAt (best * 2.0) >= 0.2 * scoreAt (best)) best *= 2.0;   // evita quedarse en la mitad del tempo (muy común en bucles de bombo)
        return best;
    }

    // Tonalidad: perfiles de Krumhansl sobre el cromagrama
    inline void estimateKey (const std::vector<float>& x, double sr, int& pc, bool& minor, float& conf)
    {
        constexpr int order = 13, N = 1 << order; juce::dsp::FFT fft (order); std::vector<float> win ((size_t) N), buf ((size_t) N * 2);
        for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) N);
        double chroma[12] {}; const size_t hopS = (size_t) N / 2; int frames = 0;
        const size_t maxStart = x.size() > (size_t) N ? x.size() - (size_t) N : 0; const size_t stride = std::max<size_t> (hopS, maxStart / 80);   // como mucho ~80 ventanas
        for (size_t s = 0; s <= maxStart && frames < 80; s += stride, ++frames)
        {
            std::fill (buf.begin(), buf.end(), 0.f); for (int i = 0; i < N; ++i) buf[(size_t) i] = x[s + (size_t) i] * win[(size_t) i];
            fft.performFrequencyOnlyForwardTransform (buf.data());
            for (int k = 1; k < N / 2; ++k)
            {
                const double f = k * sr / N; if (f < 60.0 || f > 2200.0) continue;
                const double m = 12.0 * std::log2 (f / 440.0) + 69.0; const int p = (int) std::lround (m); if (std::abs (m - p) > 0.35) continue;
                chroma[((p % 12) + 12) % 12] += std::sqrt ((double) buf[(size_t) k]);
            }
        }
        double tot = 0; for (double c : chroma) tot += c; if (tot <= 0) { pc = -1; conf = 0; return; }
        static const double maj[12] { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 }, mnr[12] { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
        auto corr = [&] (const double* prof, int rot)
        {
            double mc = tot / 12.0, mp = 0; for (int i = 0; i < 12; ++i) mp += prof[i]; mp /= 12; double num = 0, dc = 0, dp = 0;
            for (int i = 0; i < 12; ++i) { const double a = chroma[(i + rot) % 12] - mc, b = prof[i] - mp; num += a * b; dc += a * a; dp += b * b; }
            return num / std::sqrt (dc * dp + 1e-12);
        };
        double best = -2, second = -2; for (int r = 0; r < 12; ++r) for (int m = 0; m < 2; ++m) { const double c = corr (m ? mnr : maj, r); if (c > best) { second = best; best = c; pc = r; minor = m == 1; } else if (c > second) second = c; }
        conf = (float) juce::jlimit (0.0, 1.0, (best - second) * 4.0 + best * 0.3);
    }

    // Nota fundamental de un sonido suelto: pico más fuerte entre 25 y 500 Hz en la parte sostenida (tras el golpe)
    inline double estimateFundamental (const std::vector<float>& x, double sr)
    {
        size_t pk = 0; float m = 0; for (size_t i = 0; i < x.size(); ++i) if (std::abs (x[i]) > m) { m = std::abs (x[i]); pk = i; }
        const size_t start = std::min (x.size() - 1, pk + (size_t) (0.03 * sr)); constexpr int order = 15, N = 1 << order; juce::dsp::FFT fft (order); std::vector<float> buf ((size_t) N * 2, 0.f);
        const size_t len = std::min<size_t> ((size_t) N, x.size() - start); if (len < 512) return 0.0;
        for (size_t i = 0; i < len; ++i) buf[i] = x[start + i] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) len));
        fft.performFrequencyOnlyForwardTransform (buf.data());
        int bestK = 0; float bestV = 0; const int k0 = (int) (25.0 * N / sr), k1 = (int) (500.0 * N / sr);
        for (int k = std::max (2, k0); k < k1; ++k) if (buf[(size_t) k] > bestV && buf[(size_t) k] >= buf[(size_t) k - 1] && buf[(size_t) k] >= buf[(size_t) k + 1]) { bestV = buf[(size_t) k]; bestK = k; }
        if (bestK == 0) return 0.0;
        const double a = buf[(size_t) bestK - 1], b = buf[(size_t) bestK], c = buf[(size_t) bestK + 1], d = a - 2 * b + c, off = std::abs (d) > 1e-9 ? 0.5 * (a - c) / d : 0.0;   // interpolación parabólica
        return (bestK + off) * sr / N;
    }

    inline Result analyse (const juce::File& f)
    {
        Result r; std::vector<float> x; double sr = 0; if (! readMono (f, x, sr)) return r;
        r.ok = true; r.seconds = (double) x.size() / sr;
        if (r.seconds < 1.5)
        {
            r.oneShot = true; r.hz = estimateFundamental (x, sr); if (r.hz > 0) r.midi = (int) std::lround (12.0 * std::log2 (r.hz / 440.0) + 69.0);
            return r;
        }
        r.bpm = estimateBpm (x, sr, r.bpmConf, r.seconds); estimateKey (x, sr, r.keyPc, r.minor, r.keyConf);
        return r;
    }
    inline juce::String describe (const Result& r)
    {
        if (! r.ok) return juce::String::fromUTF8 ("No se pudo leer el archivo");
        if (r.oneShot) return r.midi >= 0 ? juce::String::fromUTF8 ("Nota: ") + noteName (r.midi) + " (" + juce::String (r.hz, 1) + " Hz)" : juce::String::fromUTF8 ("No se encontró una nota clara");
        juce::String s = r.bpm > 0 ? juce::String::fromUTF8 ("Tempo: ") + juce::String (r.bpm, 1) + " BPM" : juce::String::fromUTF8 ("Tempo: no se distingue un pulso claro");
        if (r.keyPc >= 0) s += juce::String::fromUTF8 ("  ·  Tono: ") + pcName (r.keyPc) + (r.minor ? " menor" : " mayor") + (r.keyConf < 0.35f ? " (poco seguro)" : "");
        return s;
    }
}
