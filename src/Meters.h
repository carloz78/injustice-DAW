#pragma once
#include <tracktion_engine/tracktion_engine.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include <vector>
#include "FloatingPanel.h"
#include "Theme.h"
namespace te = tracktion;

// Toma de audio del Master: copia lo que pasa a un anillo para que la interfaz lo analice. No modifica el sonido.
class InjTap : public te::Plugin
{
public:
    static inline const char* xmlTypeName = "injtap";
    static const char* getPluginName() { return "Medidores"; }
    static constexpr int kSize = 1 << 17;
    InjTap (te::PluginCreationInfo info) : te::Plugin (info) { ring[0].assign (kSize, 0.f); ring[1].assign (kSize, 0.f); }
    ~InjTap() override { notifyListenersOfDeletion(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Med"; }
    juce::String getSelectableDescription() override { return getName(); }
    int getNumOutputChannelsGivenInputs (int n) override { return n; }
    void initialise (const te::PluginInitialisationInfo& i) override { sampleRate.store (i.sampleRate); }
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0) return;
        const int nc = fc.destBuffer->getNumChannels(); if (nc < 1) return;
        const float* a = fc.destBuffer->getReadPointer (0, fc.bufferStartSample); const float* b = fc.destBuffer->getReadPointer (nc > 1 ? 1 : 0, fc.bufferStartSample);
        uint64_t w = writePos.load (std::memory_order_relaxed);
        for (int i = 0; i < fc.bufferNumSamples; ++i, ++w) { ring[0][w & (kSize - 1)] = a[i]; ring[1][w & (kSize - 1)] = b[i]; }
        writePos.store (w, std::memory_order_release);
    }
    // Copia lo nuevo desde 'pos'. Si la interfaz se atrasó mas de lo que cabe, salta al presente.
    int read (uint64_t& pos, std::vector<float>& l, std::vector<float>& r)
    {
        const uint64_t w = writePos.load (std::memory_order_acquire); if (pos > w) pos = w; if (w - pos > (uint64_t) kSize - 4096) pos = w - (kSize - 4096);
        const int n = (int) (w - pos); l.resize ((size_t) n); r.resize ((size_t) n);
        for (int i = 0; i < n; ++i) { l[(size_t) i] = ring[0][(pos + (uint64_t) i) & (kSize - 1)]; r[(size_t) i] = ring[1][(pos + (uint64_t) i) & (kSize - 1)]; }
        pos = w; return n;
    }
    uint64_t written() const { return writePos.load(); }
    std::atomic<double> sampleRate { 44100.0 };
private:
    std::vector<float> ring[2]; std::atomic<uint64_t> writePos { 0 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjTap)
};

// Analisis: volumen (ITU-R BS.1770 / EBU R128), espectro, fase estereo y pico.
class MeterEngine
{
public:
    static constexpr int kFFT = 4096, kScope = 512;
    MeterEngine() : fft (12), window ((size_t) kFFT)
    {
        for (int i = 0; i < kFFT; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) kFFT);   // Hann
        hist.assign (kFFT, 0.f); scope.assign ((size_t) kScope * 2, 0.f); setSampleRate (44100.0);
    }
    void setSampleRate (double fs)
    {
        sr = fs; hop = std::max (1, (int) std::lround (0.1 * fs)); corrDecay = (float) std::exp (-1.0 / (0.4 * fs));
        for (int c = 0; c < 2; ++c) { shelf[c] = makeShelf (fs); hp[c] = makeHP (fs); }
        resetLoudness();
    }
    void resetLoudness() { subAcc = 0; subCount = 0; subs.clear(); blocks.clear(); peak = 0.f; }
    void feed (const float* l, const float* r, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const float a = l[i], b = r[i];
            peak = std::max (peak, std::max (std::abs (a), std::abs (b)));
            const double ya = hp[0].process (shelf[0].process (a)), yb = hp[1].process (shelf[1].process (b));
            subAcc += ya * ya + yb * yb;
            if (++subCount >= hop) { pushSub (subAcc / subCount); subAcc = 0; subCount = 0; }
            sLR = sLR * corrDecay + a * b; sLL = sLL * corrDecay + a * a; sRR = sRR * corrDecay + b * b;
            hist[(size_t) hw] = 0.5f * (a + b); hw = (hw + 1) % kFFT;
            scope[(size_t) sw * 2] = a; scope[(size_t) sw * 2 + 1] = b; sw = (sw + 1) % kScope;
        }
    }
    static float toLUFS (double e) { return e <= 1e-12 ? -200.f : (float) (-0.691 + 10.0 * std::log10 (e)); }
    float momentary() const { return meanLast (4); }
    float shortTerm() const { return meanLast (30); }
    float integrated() const
    {
        double s = 0; int n = 0; for (double e : blocks) if (toLUFS (e) > -70.f) { s += e; ++n; }
        if (n == 0) return -200.f; const float rel = toLUFS (s / n) - 10.f; s = 0; n = 0;
        for (double e : blocks) if (toLUFS (e) > rel) { s += e; ++n; }
        return n ? toLUFS (s / n) : -200.f;
    }
    float peakDb() const { return peak <= 1e-6f ? -120.f : 20.f * std::log10 (peak); }
    float correlation() const { return juce::jlimit (-1.f, 1.f, sLR / std::sqrt (sLL * sRR + 1e-20f)); }
    // Espectro en dB con 0 dB = seno a plena escala. out tiene kFFT/2 valores (bin k = k*sr/kFFT).
    void spectrum (std::vector<float>& out)
    {
        std::vector<float> buf ((size_t) kFFT * 2, 0.f);
        for (int i = 0; i < kFFT; ++i) buf[(size_t) i] = hist[(size_t) ((hw + i) % kFFT)] * window[(size_t) i];
        fft.performFrequencyOnlyForwardTransform (buf.data());
        out.resize ((size_t) kFFT / 2);
        for (int k = 0; k < kFFT / 2; ++k) out[(size_t) k] = 20.f * std::log10 (std::max (1e-7f, buf[(size_t) k] / (kFFT / 4.f)));
    }
    float sampleRate() const { return (float) sr; }
    // puntos del vectorscopio (L,R) del mas viejo al mas nuevo
    void scopePoints (std::vector<std::pair<float, float>>& p) const { p.resize (kScope); for (int i = 0; i < kScope; ++i) { const int k = (sw + i) % kScope; p[(size_t) i] = { scope[(size_t) k * 2], scope[(size_t) k * 2 + 1] }; } }
private:
    struct Biquad { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        double process (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; } };
    static Biquad makeShelf (double fs)
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196, K = std::tan (juce::MathConstants<double>::pi * f0 / fs);
        const double Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416), a0 = 1.0 + K / Q + K * K; Biquad b;
        b.b0 = (Vh + Vb * K / Q + K * K) / a0; b.b1 = 2.0 * (K * K - Vh) / a0; b.b2 = (Vh - Vb * K / Q + K * K) / a0; b.a1 = 2.0 * (K * K - 1.0) / a0; b.a2 = (1.0 - K / Q + K * K) / a0; return b;
    }
    static Biquad makeHP (double fs)
    {
        const double f0 = 38.13547087602444, Q = 0.5003270373238773, K = std::tan (juce::MathConstants<double>::pi * f0 / fs), a0 = 1.0 + K / Q + K * K; Biquad b;
        b.b0 = 1; b.b1 = -2; b.b2 = 1; b.a1 = 2.0 * (K * K - 1.0) / a0; b.a2 = (1.0 - K / Q + K * K) / a0; return b;
    }
    void pushSub (double z)
    {
        subs.push_back (z); if (subs.size() > 64) subs.erase (subs.begin());
        if (subs.size() >= 4) { double s = 0; for (size_t i = subs.size() - 4; i < subs.size(); ++i) s += subs[i]; blocks.push_back (s / 4.0); if (blocks.size() > 100000) blocks.erase (blocks.begin(), blocks.begin() + 10000); }
    }
    float meanLast (size_t n) const { if (subs.size() < n) { if (subs.empty()) return -200.f; n = subs.size(); } double s = 0; for (size_t i = subs.size() - n; i < subs.size(); ++i) s += subs[i]; return toLUFS (s / (double) n); }

    double sr = 44100.0, subAcc = 0; int hop = 4410, subCount = 0, hw = 0, sw = 0; float corrDecay = 0.99f, sLR = 0, sLL = 0, sRR = 0, peak = 0;
    Biquad shelf[2], hp[2]; std::vector<double> subs, blocks; std::vector<float> hist, scope;
    juce::dsp::FFT fft; std::vector<float> window;
};

// Panel flotante: espectro del Master, vectorscopio, fase y volumen (LUFS).
class MetersPanel : public FloatingPanel, private juce::Timer
{
public:
    MetersPanel (te::Edit& e) : edit (e)
    {
        title = "Medidores"; icon = icons::Sliders; setDesign (780, 470); startTimerHz (30);
    }
    ~MetersPanel() override { stopTimer(); }
    void attach()    // busca o crea la toma de audio al final de la cadena del Master
    {
        auto& ml = edit.getMasterPluginList(); tap = nullptr;
        for (auto* p : ml.getPlugins()) if (auto* t = dynamic_cast<InjTap*> (p)) { tap = t; break; }
        if (tap == nullptr) { auto p = edit.getPluginCache().createNewPlugin (InjTap::xmlTypeName, {}); ml.insertPlugin (p, -1, nullptr); tap = dynamic_cast<InjTap*> (p.get()); }
        keep = tap; readPos = tap ? tap->written() : 0;
    }
    InjTap* getTap() const { return tap; }
    MeterEngine& engineRef() { return engine; }
    void pump()
    {
        if (tap == nullptr) return;
        const double fs = tap->sampleRate.load(); if (std::abs (fs - engine.sampleRate()) > 1.0 && fs > 8000.0) engine.setSampleRate (fs);
        if (tap->read (readPos, bl, br) > 0) engine.feed (bl.data(), br.data(), (int) bl.size());
    }
    void setSampleRateForTest (double fs) { engine.setSampleRate (fs); }
protected:
    void bodyDown (const juce::MouseEvent& e) override { if (resetR().contains (e.position)) { engine.resetLoudness(); repaint(); } }
    juce::String tipAt (juce::Point<float> p) override { return resetR().contains (p) ? "Reiniciar" : juce::String(); }
    void paintBody (juce::Graphics& g) override
    {
        using namespace theme;
        const auto sp = specR(); g.setColour (boxFill); g.fillRoundedRectangle (sp, 6.f); g.setColour (boxLine); g.drawRoundedRectangle (sp, 6.f, 1.f);
        // rejilla
        g.setFont (juce::FontOptions (11.f)); const float fmin = 20.f, fmax = 20000.f;
        auto fx = [&] (float f) { return sp.getX() + sp.getWidth() * std::log (f / fmin) / std::log (fmax / fmin); };
        auto dy = [&] (float db) { return sp.getBottom() - sp.getHeight() * (db - kMinDb) / (kMaxDb - kMinDb); };
        for (float f : { 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f })
        {
            g.setColour (gridLine); g.drawVerticalLine ((int) fx (f), sp.getY() + 1, sp.getBottom() - 1);
            g.setColour (textDim); g.drawText (f >= 1000.f ? juce::String ((int) (f / 1000.f)) + "k" : juce::String ((int) f), juce::Rectangle<float> (fx (f) - 18, sp.getBottom() - 16, 36, 14), juce::Justification::centred);
        }
        for (float db = -80.f; db <= -10.f; db += 20.f)
        {
            g.setColour (gridLine); g.drawHorizontalLine ((int) dy (db), sp.getX() + 1, sp.getRight() - 1);
            g.setColour (textDim); g.drawText (juce::String ((int) db), juce::Rectangle<float> (sp.getX() + 4, dy (db) - 14, 30, 13), juce::Justification::centredLeft);
        }
        // espectro: maximo por columna de pixeles, con inclinacion de +3 dB/octava para que la musica se vea pareja
        if (! spec.empty())
        {
            const int w = (int) sp.getWidth(); juce::Path p; const float binHz = engine.sampleRate() / (float) MeterEngine::kFFT; bool started = false;
            for (int x = 0; x < w; ++x)
            {
                const float f0 = fmin * std::exp (std::log (fmax / fmin) * (float) x / (float) w), f1 = fmin * std::exp (std::log (fmax / fmin) * (float) (x + 1) / (float) w);
                const float k0 = f0 / binHz, k1 = std::max (k0 + 1.f, f1 / binHz); float v = -200.f;
                if (k1 - k0 <= 1.01f) { const int k = (int) k0; const float t = k0 - (float) k; v = spec[(size_t) std::min (k, (int) spec.size() - 2)] * (1.f - t) + spec[(size_t) std::min (k + 1, (int) spec.size() - 1)] * t; }
                else for (int k = (int) k0; k <= (int) k1 && k < (int) spec.size(); ++k) v = std::max (v, spec[(size_t) k]);
                v += 3.f * std::log2 (std::sqrt (f0 * f1) / 1000.f);
                const float y = juce::jlimit (sp.getY() + 1.f, sp.getBottom() - 1.f, dy (v));
                if (! started) { p.startNewSubPath (sp.getX() + (float) x, y); started = true; } else p.lineTo (sp.getX() + (float) x, y);
            }
            juce::Path fill (p); fill.lineTo (sp.getRight() - 1, sp.getBottom() - 1); fill.lineTo (sp.getX(), sp.getBottom() - 1); fill.closeSubPath();
            g.setGradientFill (juce::ColourGradient (accent.withAlpha (.55f), 0, sp.getY(), accent.withAlpha (.05f), 0, sp.getBottom(), false)); g.fillPath (fill);
            g.setColour (accentHi); g.strokePath (p, juce::PathStrokeType (1.4f));
        }
        // vectorscopio
        const auto vs = scopeR(); g.setColour (boxFill); g.fillRoundedRectangle (vs, 6.f); g.setColour (boxLine); g.drawRoundedRectangle (vs, 6.f, 1.f);
        const auto c = vs.getCentre(); const float rad = vs.getWidth() * .5f - 10.f;
        g.setColour (gridLine); g.drawLine (c.x, vs.getY() + 6, c.x, vs.getBottom() - 6); g.drawLine (vs.getX() + 6, c.y, vs.getRight() - 6, c.y);
        g.drawEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2, 1.f);
        g.setColour (textDim); g.setFont (juce::FontOptions (11.f)); g.drawText ("L", juce::Rectangle<float> (vs.getX() + 4, vs.getY() + 3, 14, 12), juce::Justification::centred); g.drawText ("R", juce::Rectangle<float> (vs.getRight() - 18, vs.getY() + 3, 14, 12), juce::Justification::centred);
        for (size_t i = 0; i < pts.size(); ++i)
        {
            const float l = pts[i].first, r = pts[i].second, x = (l - r) * .7071f, y = (l + r) * .7071f;     // M arriba, S a los lados
            g.setColour (pink.withAlpha (.12f + .5f * (float) i / (float) pts.size())); g.fillRect (c.x + x * rad * 1.1f - 1.f, c.y - y * rad * 1.1f - 1.f, 2.f, 2.f);
        }
        // fase
        const auto cr = corrR(); g.setColour (boxFill); g.fillRoundedRectangle (cr, 5.f); g.setColour (boxLine); g.drawRoundedRectangle (cr, 5.f, 1.f);
        const float cv = engine.correlation(), cx = cr.getCentreX() + cv * (cr.getWidth() * .5f - 4.f);
        g.setColour (gridLine); g.drawVerticalLine ((int) cr.getCentreX(), cr.getY() + 2, cr.getBottom() - 2);
        g.setColour (cv < 0.f ? record : accentHi); g.fillRoundedRectangle (cx - 3.f, cr.getY() + 2.f, 6.f, cr.getHeight() - 4.f, 2.f);
        g.setColour (textDim); g.drawText ("-1", juce::Rectangle<float> (cr.getX() + 4, cr.getBottom() + 1, 24, 12), juce::Justification::centredLeft); g.drawText ("+1", juce::Rectangle<float> (cr.getRight() - 28, cr.getBottom() + 1, 24, 12), juce::Justification::centredRight);
        // volumen
        auto num = [] (float v, int d = 1) { return v < -150.f ? juce::String::fromUTF8 ("–∞") : juce::String (v, d); };
        const char* names[] { "M", "S", "I", "Pico" }; const float vals[] { engine.momentary(), engine.shortTerm(), engine.integrated(), engine.peakDb() };
        for (int i = 0; i < 4; ++i)
        {
            auto row = rowR (i); g.setColour (i == 2 ? panelHi : boxFill); g.fillRoundedRectangle (row, 5.f);
            g.setColour (textDim); g.setFont (juce::FontOptions (13.f)); g.drawText (names[i], row.withTrimmedLeft (10), juce::Justification::centredLeft);
            const bool hot = i == 3 && vals[i] > -0.1f;
            g.setColour (hot ? record : text); g.setFont (juce::FontOptions (20.f, juce::Font::bold)); g.drawText (num (vals[i]), row.withTrimmedRight (58), juce::Justification::centredRight);
            g.setColour (textDim); g.setFont (juce::FontOptions (12.f)); g.drawText (i == 3 ? "dBFS" : "LUFS", row.withTrimmedRight (10), juce::Justification::centredRight);
        }
    }
private:
    static constexpr float kMinDb = -90.f, kMaxDb = 0.f;
    juce::Rectangle<float> specR() const { return { 24.f, 64.f, 520.f, 382.f }; }
    juce::Rectangle<float> scopeR() const { return { 560.f, 64.f, 196.f, 196.f }; }
    juce::Rectangle<float> corrR() const { return { 560.f, 270.f, 196.f, 18.f }; }
    juce::Rectangle<float> rowR (int i) const { return { 560.f, 306.f + (float) i * 36.f, 196.f, 32.f }; }
    juce::Rectangle<float> resetR() const { return { 560.f, 306.f + 2 * 36.f, 196.f, 68.f }; }
    void timerCallback() override
    {
        if (! isVisible() || tap == nullptr) return;
        pump(); engine.spectrum (raw); if (spec.size() != raw.size()) spec = raw; else for (size_t k = 0; k < raw.size(); ++k) spec[k] = raw[k] > spec[k] ? raw[k] : std::max (raw[k], spec[k] - 2.5f);
        engine.scopePoints (pts); repaint();
    }
    te::Edit& edit; InjTap* tap = nullptr; te::Plugin::Ptr keep; MeterEngine engine; uint64_t readPos = 0; std::vector<float> bl, br, raw, spec; std::vector<std::pair<float, float>> pts;
};
