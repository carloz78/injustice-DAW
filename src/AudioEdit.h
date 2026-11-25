#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

// Procesos de audio "precalculados" del editor de audio (como los efectos precalculados de FL Studio).
// Trabajan sobre un buffer en memoria y sobre un rango [a, b) de muestras; si el rango está vacío se usa todo el audio.
namespace audioedit
{
    using Buf = juce::AudioBuffer<float>;
    struct Range { int a = 0, b = 0; int size() const { return b - a; } };

    inline Range whole (const Buf& x, int a = 0, int b = 0)
    {
        const int n = x.getNumSamples(); a = juce::jlimit (0, n, a); b = juce::jlimit (0, n, b);
        if (b - a < 1) return { 0, n }; return { a, b };
    }
    inline float peak (const Buf& x, int a = 0, int b = 0)
    {
        const auto r = whole (x, a, b); float p = 0.f; for (int c = 0; c < x.getNumChannels(); ++c) p = std::max (p, x.getMagnitude (c, r.a, r.size())); return p;
    }
    inline float peakDb (const Buf& x, int a = 0, int b = 0) { const float p = peak (x, a, b); return p < 1e-6f ? -120.f : juce::Decibels::gainToDecibels (p); }

    // Quita el nivel continuo (desplazamiento de CC) de cada canal.
    inline void removeDC (Buf& x, int a = 0, int b = 0)
    {
        const auto r = whole (x, a, b);
        for (int c = 0; c < x.getNumChannels(); ++c) { auto* p = x.getWritePointer (c); double s = 0; for (int i = r.a; i < r.b; ++i) s += p[i]; const float m = (float) (s / r.size()); for (int i = r.a; i < r.b; ++i) p[i] -= m; }
    }
    inline void invertPolarity (Buf& x, int a = 0, int b = 0) { const auto r = whole (x, a, b); for (int c = 0; c < x.getNumChannels(); ++c) { auto* p = x.getWritePointer (c); for (int i = r.a; i < r.b; ++i) p[i] = -p[i]; } }
    inline bool swapChannels (Buf& x, int a = 0, int b = 0)
    {
        if (x.getNumChannels() < 2) return false; const auto r = whole (x, a, b); auto *l = x.getWritePointer (0), *rr = x.getWritePointer (1); for (int i = r.a; i < r.b; ++i) std::swap (l[i], rr[i]); return true;
    }
    inline void reverse (Buf& x, int a = 0, int b = 0) { const auto r = whole (x, a, b); for (int c = 0; c < x.getNumChannels(); ++c) { auto* p = x.getWritePointer (c); std::reverse (p + r.a, p + r.b); } }
    // Sube o baja el nivel para que el pico quede en `targetDb`. Devuelve la ganancia aplicada (0 si el audio es silencio).
    inline float normalize (Buf& x, float targetDb = -0.3f, int a = 0, int b = 0)
    {
        const float p = peak (x, a, b); if (p < 1e-6f) return 0.f; const float g = juce::Decibels::decibelsToGain (targetDb) / p; const auto r = whole (x, a, b);
        for (int c = 0; c < x.getNumChannels(); ++c) x.applyGain (c, r.a, r.size(), g); return g;
    }
    inline void silence (Buf& x, int a, int b) { const auto r = whole (x, a, b); x.clear (r.a, r.size()); }
    // Fundido de suave (seno): de 0 a 1 (entrada) o de 1 a 0 (salida) a lo largo del rango.
    inline void fade (Buf& x, bool fadeIn, int a = 0, int b = 0)
    {
        const auto r = whole (x, a, b); const int n = r.size();
        for (int c = 0; c < x.getNumChannels(); ++c) { auto* p = x.getWritePointer (c); for (int i = 0; i < n; ++i) { const float t = n > 1 ? (float) i / (float) (n - 1) : 1.f; const float g = std::sin ((fadeIn ? t : 1.f - t) * juce::MathConstants<float>::halfPi); p[r.a + i] *= g; } }
    }
    // Primera y última muestra por encima del umbral (en dB). Si todo es silencio devuelve un rango vacío {0, 0}.
    inline Range findAudible (const Buf& x, float thresholdDb = -60.f)
    {
        const float th = juce::Decibels::decibelsToGain (thresholdDb); const int n = x.getNumSamples(); int first = -1, last = -1;
        for (int i = 0; i < n && first < 0; ++i) for (int c = 0; c < x.getNumChannels(); ++c) if (std::abs (x.getSample (c, i)) >= th) { first = i; break; }
        if (first < 0) return { 0, 0 };
        for (int i = n - 1; i >= first && last < 0; --i) for (int c = 0; c < x.getNumChannels(); ++c) if (std::abs (x.getSample (c, i)) >= th) { last = i; break; }
        return { first, last + 1 };
    }
    inline Buf crop (const Buf& x, int a, int b)
    {
        const auto r = whole (x, a, b); Buf o (x.getNumChannels(), r.size()); for (int c = 0; c < x.getNumChannels(); ++c) o.copyFrom (c, 0, x, c, r.a, r.size()); return o;
    }
    // Bucle sin clic: mezcla el final con el principio durante `n` muestras y recorta ese tramo, de modo que el final enlaza con el inicio.
    inline Buf loopCrossfade (const Buf& x, int n)
    {
        const int len = x.getNumSamples(); n = juce::jlimit (0, len / 2, n); if (n < 2) return x;
        Buf o (x.getNumChannels(), len - n);
        for (int c = 0; c < x.getNumChannels(); ++c)
        {
            o.copyFrom (c, 0, x, c, 0, len - n); auto* p = o.getWritePointer (c); auto* s = x.getReadPointer (c);
            for (int i = 0; i < n; ++i) { const float t = (float) i / (float) (n - 1); const float w = std::sin (t * juce::MathConstants<float>::halfPi), wo = std::cos (t * juce::MathConstants<float>::halfPi); p[i] = s[i] * w + s[len - n + i] * wo; }
        }
        return o;
    }
    // Quita clics en los extremos del rango con fundidos muy cortos.
    inline void declick (Buf& x, double sampleRate, int a = 0, int b = 0, double ms = 2.0)
    {
        const auto r = whole (x, a, b); const int n = std::min (r.size() / 2, std::max (2, (int) std::lround (sampleRate * ms / 1000.0)));
        if (n < 2) return; fade (x, true, r.a, r.a + n); fade (x, false, r.b - n, r.b);
    }

    // Mínimo y máximo por columna, para dibujar la onda.
    inline std::vector<std::pair<float, float>> peaks (const Buf& x, int cols)
    {
        std::vector<std::pair<float, float>> out ((size_t) std::max (1, cols), { 0.f, 0.f }); const int n = x.getNumSamples(); if (n == 0) return out;
        for (int k = 0; k < cols; ++k)
        {
            const int s = (int) ((juce::int64) n * k / cols), e = std::max (s + 1, (int) ((juce::int64) n * (k + 1) / cols)); float lo = 0.f, hi = 0.f;
            for (int c = 0; c < x.getNumChannels(); ++c) { auto* p = x.getReadPointer (c); for (int i = s; i < std::min (e, n); ++i) { lo = std::min (lo, p[i]); hi = std::max (hi, p[i]); } }
            out[(size_t) k] = { lo, hi };
        }
        return out;
    }

    inline bool readFile (const juce::File& f, Buf& out, double& sr, double maxSeconds = 600.0)
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f)); if (! r || r->lengthInSamples < 1 || r->numChannels < 1) return false;
        sr = r->sampleRate; const int n = (int) std::min<juce::int64> (r->lengthInSamples, (juce::int64) (maxSeconds * sr)); const int nc = (int) std::min (2u, (unsigned) r->numChannels);
        out.setSize (nc, n); return r->read (&out, 0, n, 0, true, nc > 1);
    }
    inline bool writeWav (const juce::File& f, const Buf& b, double sr)
    {
        f.getParentDirectory().createDirectory(); f.deleteFile(); juce::WavAudioFormat wf; std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream()); if (! os) return false;
        std::unique_ptr<juce::AudioFormatWriter> w (wf.createWriterFor (os.get(), sr, (unsigned) b.getNumChannels(), 24, {}, 0)); if (! w) return false; os.release();
        return w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
    inline juce::File editedDir() { return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("INJUSTICE DAW").getChildFile ("Editados"); }
}
