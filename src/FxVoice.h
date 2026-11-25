#pragma once
#include "FxCore.h"
#include <complex>
#include <cstring>
// Voz y tono: cambio de tono, afinador automático y vocoder.
namespace injfx
{
//---- cambio de tono por vocoder de fase (STFT de 2048 puntos, solape x4). Mantiene la duración y afina con exactitud.
struct PVShift
{
    static constexpr int N = 2048, OS = 4, STEP = N / OS, LAT = N - STEP, H = N / 2;
    float inF[N] {}, outF[N] {}, accum[2 * N] {}, lastPh[H + 1] {}, sumPh[H + 1] {}, anaM[H + 1] {}, anaF[H + 1] {}, synM[H + 1] {}, synF[H + 1] {}, env[H + 2] {}; int rover = LAT; double sr = 44100.0;
    bool keepFormants = false; float formantRatio = 1.f;   // keepFormants: la envolvente espectral se queda en su sitio (o se mueve formantRatio) en lugar de seguir al tono
    std::complex<float> w[N];
    static const std::vector<std::complex<float>>& twid() { static std::vector<std::complex<float>> t = [] { std::vector<std::complex<float>> v ((size_t) N / 2); for (int k = 0; k < N / 2; ++k) v[(size_t) k] = std::polar (1.f, (float) (-2.0 * 3.14159265358979 * k / N)); return v; }(); return t; }
    static const std::vector<float>& win() { static std::vector<float> t = [] { std::vector<float> v ((size_t) N); for (int k = 0; k < N; ++k) v[(size_t) k] = (float) (-.5 * std::cos (2.0 * 3.14159265358979 * k / N) + .5); return v; }(); return t; }
    static void fft (std::complex<float>* x)
    {
        for (int i = 1, j = 0; i < N; ++i) { int bit = N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (x[i], x[j]); }
        const auto& T = twid(); for (int len = 2; len <= N; len <<= 1) { const int st = N / len; for (int i = 0; i < N; i += len) for (int k = 0; k < len / 2; ++k) { const auto u = x[i + k], t = x[i + k + len / 2] * T[(size_t) (k * st)]; x[i + k] = u + t; x[i + k + len / 2] = u - t; } }
    }
    void reset (double rate) { sr = rate; std::fill (std::begin (inF), std::end (inF), 0.f); std::fill (std::begin (outF), std::end (outF), 0.f); std::fill (std::begin (accum), std::end (accum), 0.f); std::fill (std::begin (lastPh), std::end (lastPh), 0.f); std::fill (std::begin (sumPh), std::end (sumPh), 0.f); rover = LAT; }
    inline float run (float x, float ratio)
    {
        inF[rover] = x; const float y = outF[rover - LAT]; ++rover; if (rover >= N) { rover = LAT; frame (ratio); } return y;
    }
    // envolvente espectral por cepstrum: log-magnitud -> cepstrum -> se queda con las primeras quefrencias (~0,9 ms) -> exp
    void spectralEnvelope()
    {
        for (int k = 0; k <= H; ++k) { const float l = std::log (anaM[k] + 1e-6f); w[k] = { l, 0.f }; if (k > 0 && k < H) w[N - k] = { l, 0.f }; }
        fft (w); const int Lq = juce::jlimit (8, 120, (int) (.0009 * sr));
        for (int q = 0; q < N; ++q) { const int d = std::min (q, N - q); const float lift = d <= Lq ? 1.f : (d < Lq + 8 ? .5f + .5f * std::cos (3.14159265f * (float) (d - Lq) / 8.f) : 0.f); w[q] *= lift; }
        fft (w); for (int k = 0; k <= H; ++k) env[k] = std::exp (juce::jlimit (-30.f, 30.f, w[k].real() / (float) N)); env[H + 1] = env[H];
    }
    void frame (float ratio)
    {
        const float pi = 3.14159265f, expct = 2.f * pi * STEP / N, fpb = (float) (sr / N); const auto& W = win();
        for (int k = 0; k < N; ++k) w[k] = { inF[k] * W[(size_t) k], 0.f }; fft (w);
        for (int k = 0; k <= H; ++k)
        {
            const float re = w[k].real(), im = w[k].imag(), mag = 2.f * std::sqrt (re * re + im * im), ph = std::atan2 (im, re); float t = ph - lastPh[k]; lastPh[k] = ph; t -= (float) k * expct;
            long qpd = (long) (t / pi); if (qpd >= 0) qpd += qpd & 1; else qpd -= qpd & 1; t -= pi * (float) qpd; t = OS * t / (2.f * pi); anaM[k] = mag; anaF[k] = (float) k * fpb + t * fpb;
        }
        std::fill (std::begin (synM), std::end (synM), 0.f); std::fill (std::begin (synF), std::end (synF), 0.f);
        const bool useEnv = keepFormants && std::abs (ratio - formantRatio) > .002f;
        if (useEnv) spectralEnvelope();
        for (int k = 0; k <= H; ++k)
        {
            const int idx = (int) ((float) k * ratio); if (idx < 0 || idx > H) continue;
            float g = 1.f; if (useEnv) { const float pos = (float) idx / formantRatio; const int i0 = (int) pos; const float fr = pos - (float) i0; const float e = i0 >= H ? env[H] : env[i0] * (1.f - fr) + env[i0 + 1] * fr; g = juce::jlimit (.1f, 8.f, e / std::max (env[k], 1e-9f)); }
            synM[idx] += anaM[k] * g; synF[idx] = anaF[k] * ratio;
        }
        for (int k = 0; k <= H; ++k)
        {
            float t = synF[k] - (float) k * fpb; t /= fpb; t = 2.f * pi * t / OS; t += (float) k * expct; sumPh[k] += t; const float ph = sumPh[k]; w[k] = { synM[k] * std::cos (ph), synM[k] * std::sin (ph) };
        }
        for (int k = H + 1; k < N; ++k) w[k] = 0.f;
        for (int k = 0; k < N; ++k) w[k] = std::conj (w[k]); fft (w);
        for (int k = 0; k < N; ++k) accum[k] += 2.f * W[(size_t) k] * w[k].real() / ((float) H * OS * 1.5f);
        for (int k = 0; k < STEP; ++k) outF[k] = accum[k]; std::memmove (accum, accum + STEP, sizeof (float) * N);
        for (int k = 0; k < LAT; ++k) inF[k] = inF[k + STEP];
    }
};
using PitchCore = PVShift;

//==============================================================================
class InjPitch : public InjFx
{
public:
    enum { pSt, pFine, pMix };
    INJ_FX_BODY (InjPitch, "injpitch", "Cambio de tono", "Tono", 3)
    Layout layout() const override { Layout L; L.visualH = 80; L.knobs = { pSt, pFine, pMix }; L.perRow = 3; L.dia = 62.f; L.icon = "M12 20V5M6 11l6-6 6 6"; return L; }
    std::vector<Preset> presets() const override { return { { "Octava arriba", { 12.f, 0.f, .5f } }, { "Octava abajo", { -12.f, 0.f, .5f } }, { "Quinta arriba", { 7.f, 0.f, .4f } }, { "Voz de ardilla", { 7.f, 0.f, 1.f } }, { "Engrosar", { 0.f, .15f, .5f } } }; }
    double getLatencySeconds() override { return (double) PVShift::LAT / sr; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; for (auto& c : core) c.reset (sr); for (auto& d : dry) { d.prep (PVShift::LAT + 8); d.clear(); } }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return; const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : nullptr };
        const float ratio = std::pow (2.f, (v (pSt) + v (pFine)) / 12.f), mix = v (pMix);
        for (int i = 0; i < fc.bufferNumSamples; ++i) for (int c = 0; c < nc; ++c) { const float x = p[c][i]; dry[c].push (x); const float d = dry[c].at (PVShift::LAT - 1), y = core[c].run (x, ratio); p[c][i] = d + (y - d) * mix; }
    }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        const float st = v (pSt) + v (pFine); const auto bar = r.reduced (24.f, 26.f); g.setColour (theme::bgBar); g.fillRoundedRectangle (bar, 2.f); g.setColour (theme::barLine); g.drawVerticalLine ((int) bar.getCentreX(), bar.getY(), bar.getBottom());
        const float x = bar.getCentreX() + st / 24.f * bar.getWidth() * .5f; g.setColour (theme::accent); g.fillRoundedRectangle (std::min (x, bar.getCentreX()), bar.getY(), std::max (4.f, std::abs (x - bar.getCentreX())), bar.getHeight(), 2.f);
        label (g, fmtValue ({ "", "", 0, 0, 0, 0, fSemi, "" }, st), r.reduced (10.f, 4.f), juce::Justification::centredTop, 13.f, true, theme::text);
    }
private:
    static inline const Spec S[] = { { "st", "Semitonos", -24.f, 24.f, 0.f, 0.f, fSemi, "" }, { "fine", "Ajuste fino", -1.f, 1.f, 0.f, 0.f, fSemi, "" }, { "mix", "Mezcla", 0.f, 1.f, 1.f, 0.f, fPct, "" } };
    double sr = 44100.0; PitchCore core[2]; Delay dry[2];
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjPitch)
};

//==============================================================================
// Afinador automático: detecta el tono (YIN), lo lleva a la nota más cercana de la escala y lo corrige con el cambiador de tono.
class InjTune : public InjFx
{
public:
    enum { pKey, pScale, pSpeed, pAmount, pMix };
    INJ_FX_BODY (InjTune, "injtune", "Afinador", "Afinar", 5)
    Layout layout() const override { Layout L; L.pills = pScale; L.visualH = 100; L.knobs = { pKey, pSpeed, pAmount, pMix }; L.perRow = 4; L.dia = 60.f; L.icon = "M12 3v14M7 8l5-5 5 5M5 21h14"; return L; }
    std::vector<Preset> presets() const override { return { { "Natural", { 0.f, 0.f, 120.f, .8f, 1.f } }, { "Efecto duro (T-Pain)", { 0.f, 0.f, 0.f, 1.f, 1.f } }, { "Menor suave", { 9.f, 2.f, 60.f, .9f, 1.f } }, { "Pentatónica", { 0.f, 3.f, 30.f, 1.f, 1.f } } }; }
    double getLatencySeconds() override { return (double) PVShift::LAT / sr; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; for (auto& c : core) c.reset (sr); for (auto& d : dry) { d.prep (PVShift::LAT + 8); d.clear(); } ring.assign (4096, 0.f); rp = 0; sinceHop = 0; semi = 0.f; target = 0.f; f0 = 0.f; hold = 0; }
    static bool inScale (int scale, int deg) { static const bool M[4][12] { { 1,1,1,1,1,1,1,1,1,1,1,1 }, { 1,0,1,0,1,1,0,1,0,1,0,1 }, { 1,0,1,1,0,1,0,1,1,0,1,0 }, { 1,0,0,1,0,1,0,1,0,0,1,0 } }; return M[juce::jlimit (0, 3, scale)][((deg % 12) + 12) % 12]; }
    float nearestNote (float midi) const { const int key = vi (pKey), sc = vi (pScale); float best = midi; float bd = 1e9f; const int c0 = (int) std::round (midi); for (int c = c0 - 3; c <= c0 + 3; ++c) if (inScale (sc, c - key)) { const float d = std::abs ((float) c - midi); if (d < bd) { bd = d; best = (float) c; } } return best; }
    // YIN sobre la señal mono diezmada a la mitad. Devuelve f0 en Hz o 0 si no hay tono claro.
    float detect()
    {
        const int Wn = 512, tmax = 330, need = (Wn + tmax) * 2; static thread_local std::vector<float> x, dd; x.assign ((size_t) (Wn + tmax), 0.f); dd.assign ((size_t) tmax, 0.f);
        for (int j = 0; j < Wn + tmax; ++j) { const int a = rp - need + j * 2; x[(size_t) j] = .5f * (ring[(size_t) ((a & 4095))] + ring[(size_t) ((a + 1) & 4095)]); }
        for (int t = 1; t < tmax; ++t) { float s = 0.f; for (int j = 0; j < Wn; ++j) { const float d = x[(size_t) j] - x[(size_t) (j + t)]; s += d * d; } dd[(size_t) t] = s; }
        float run = 0.f; dd[0] = 1.f; for (int t = 1; t < tmax; ++t) { run += dd[(size_t) t]; dd[(size_t) t] = run > 1e-9f ? dd[(size_t) t] * (float) t / run : 1.f; }
        int tau = -1; for (int t = 24; t < tmax - 1; ++t) if (dd[(size_t) t] < .15f) { while (t + 1 < tmax - 1 && dd[(size_t) (t + 1)] < dd[(size_t) t]) ++t; tau = t; break; }
        if (tau < 0) return 0.f; const float a = dd[(size_t) tau - 1], b = dd[(size_t) tau], c = dd[(size_t) tau + 1]; const float den = a - 2.f * b + c; const float off = std::abs (den) > 1e-9f ? .5f * (a - c) / den : 0.f;
        return (float) (sr * .5) / ((float) tau + off);
    }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || ring.empty()) return; const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : nullptr };
        const float amt = v (pAmount), mix = v (pMix), speed = v (pSpeed); const int hop = 256; const float perHop = speed < .5f ? 1.f : 1.f - std::exp (-(float) hop / ((float) sr * speed * .001f));
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            ring[(size_t) (rp & 4095)] = nc > 1 ? .5f * (p[0][i] + p[1][i]) : p[0][i]; ++rp;
            if (++sinceHop >= hop)
            {
                sinceHop = 0; const float f = detect();
                if (f > 70.f && f < 900.f) { f0 = f; hold = 0; const float m = 69.f + 12.f * std::log2 (f / 440.f); target = nearestNote (m); semi += ((target - m) * amt - semi) * perHop; }
                else if (++hold > 6) { semi += (0.f - semi) * .2f; f0 = 0.f; }
                meterA.store (f0); meterB.store (f0 > 0.f ? 440.f * std::pow (2.f, (target - 69.f) / 12.f) : 0.f); meterC.store (semi);
            }
            const float ratio = std::pow (2.f, semi / 12.f); for (int c = 0; c < nc; ++c) { const float x = p[c][i]; dry[c].push (x); const float d = dry[c].at (PVShift::LAT - 1), y = core[c].run (x, ratio); p[c][i] = d + (y - d) * mix; }
        }
    }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        static const char* nn[] { "Do", "Do#", "Re", "Re#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" }; const float f = meterA.load(), t = meterB.load(), s = meterC.load();
        const auto name = [&] (float hz) { const int m = juce::roundToInt (69.f + 12.f * std::log2 (hz / 440.f)); return juce::String (nn[((m % 12) + 12) % 12]) + juce::String (m / 12 - 1); };
        label (g, f > 0.f ? name (f) + juce::String::fromUTF8 ("  →  ") + name (t) : juce::String::fromUTF8 ("Sin tono"), { r.getX(), r.getY() + 6.f, r.getWidth(), 30.f }, juce::Justification::centred, 24.f, true, f > 0.f ? theme::text : theme::textDim);
        const auto bar = juce::Rectangle<float> (r.getX() + 40.f, r.getBottom() - 30.f, r.getWidth() - 80.f, 10.f); g.setColour (theme::bgBar); g.fillRoundedRectangle (bar, 2.f); g.setColour (theme::barLine); g.drawVerticalLine ((int) bar.getCentreX(), bar.getY() - 3.f, bar.getBottom() + 3.f);
        const float x = bar.getCentreX() + juce::jlimit (-1.f, 1.f, s / 1.f) * bar.getWidth() * .5f; g.setColour (theme::accent); g.fillEllipse (x - 7.f, bar.getCentreY() - 7.f, 14.f, 14.f);
        label (g, juce::String::fromUTF8 ("Corrección"), { bar.getX(), bar.getBottom() + 2.f, 100.f, 12.f }, juce::Justification::left, 10.f);
    }
private:
    static inline const Spec S[] = { { "key", "Tonalidad", 0.f, 11.f, 0.f, 0.f, fNote, "" }, { "scale", "Escala", 0.f, 3.f, 0.f, 0.f, fChoice, "Cromática|Mayor|Menor|Pentatónica" }, { "speed", "Velocidad", 0.f, 400.f, 100.f, 80.f, fMs, "" },
        { "amt", "Cantidad", 0.f, 1.f, .8f, 0.f, fPct, "" }, { "mix", "Mezcla", 0.f, 1.f, 1.f, 0.f, fPct, "" } };
    double sr = 44100.0; PitchCore core[2]; Delay dry[2]; std::vector<float> ring; int rp = 0, sinceHop = 0, hold = 0; float semi = 0.f, target = 0.f, f0 = 0.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjTune)
};

//==============================================================================
// Vocoder de 16 bandas: la voz entra y mueve un sintetizador interno de acordes (más ruido para las eses).
class InjVocoder : public InjFx
{
public:
    enum { pRoot, pChord, pOct, pAtk, pRel, pSib, pMix, pOut };
    static constexpr int NB = 16;
    INJ_FX_BODY (InjVocoder, "injvocoder", "Vocoder", "Vocoder", 8)
    Layout layout() const override { Layout L; L.pills = pChord; L.visualH = 100; L.rows = { { pRoot, pOct, pAtk, pRel }, { pSib, pMix, pOut } }; L.dia = 54.f; L.icon = "M4 10v4M8 6v12M12 8v8M16 4v16M20 9v6"; return L; }
    std::vector<Preset> presets() const override { return { { "Robot de acorde", { 0.f, 1.f, 1.f, 3.f, 60.f, .5f, 1.f, 0.f } }, { "Coro de quintas", { 4.f, 2.f, 1.f, 8.f, 100.f, .4f, 1.f, 0.f } }, { "Monofónico", { 9.f, 0.f, 0.f, 2.f, 40.f, .6f, 1.f, 0.f } }, { "Susurro", { 0.f, 1.f, 2.f, 5.f, 80.f, 1.f, .8f, 0.f } } }; }
    void initialise (const te::PluginInitialisationInfo& i) override
    {
        sr = i.sampleRate; for (int k = 0; k < NB; ++k) { const float fc = 120.f * std::pow (7500.f / 120.f, (float) k / (NB - 1)); fcs[k] = fc; for (int j = 0; j < 2; ++j) { ma[k][j].set (BQ::BP, sr, fc, 6.0); ma[k][j].reset(); ca[k][j].set (BQ::BP, sr, fc, 6.0); ca[k][j].reset(); } env[k] = 0.f; }
        for (auto& o : ph) o = 0.f; rng = 99u;
    }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return; const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : nullptr };
        static const int ch[4][4] { { 0, 7, 12, 19 }, { 0, 3, 7, 12 }, { 0, 4, 7, 12 }, { 0, 7, 12, 16 } };   // quinta/menor/mayor/sus
        const int chord = juce::jlimit (0, 3, vi (pChord)); const float base = 36.f + (float) vi (pRoot) + 12.f * (v (pOct) - 1.f); const float aA = coefMs (v (pAtk), sr), aR = coefMs (v (pRel), sr), sib = v (pSib), mix = v (pMix), og = db2lin (v (pOut)) * 5.f;
        float inc[4]; for (int k = 0; k < 4; ++k) inc[k] = 440.f * std::pow (2.f, (base + (float) ch[chord][k] - 69.f) / 12.f) / (float) sr;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            const float x = nc > 1 ? .5f * (p[0][i] + p[1][i]) : p[0][i]; float saw = 0.f; for (int k = 0; k < 4; ++k) { ph[k] += inc[k]; if (ph[k] > 1.f) ph[k] -= 1.f; saw += (ph[k] * 2.f - 1.f) * .25f; }
            rng = rng * 1664525u + 1013904223u; const float nz = (float) (rng >> 8) / 8388608.f - 1.f; float out = 0.f;
            for (int k = 0; k < NB; ++k)
            {
                const float m = ma[k][1].run (0, ma[k][0].run (0, x)), a = std::abs (m); env[k] = a > env[k] ? aA * env[k] + (1.f - aA) * a : aR * env[k] + (1.f - aR) * a;
                const float hi = k >= 11 ? sib * (float) (k - 10) / 5.f : 0.f; const float carr = saw * (1.f - hi) + nz * hi * 1.5f; out += ca[k][1].run (0, ca[k][0].run (0, carr)) * env[k] * 8.f;
            }
            for (int c = 0; c < nc; ++c) p[c][i] = p[c][i] * (1.f - mix) + out * og * mix;
        }
        for (int k = 0; k < NB; ++k) bars[k].store (env[k]);
    }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        const float w = (r.getWidth() - 16.f) / NB; for (int k = 0; k < NB; ++k) { const float e = juce::jlimit (0.f, 1.f, (lin2db (bars[k].load() * 4.f + 1e-6f) + 60.f) / 60.f); const float h = e * (r.getHeight() - 14.f); g.setColour (theme::accent.withAlpha (.35f + .65f * e)); g.fillRoundedRectangle (r.getX() + 8.f + k * w + 2.f, r.getBottom() - 6.f - std::max (3.f, h), w - 4.f, std::max (3.f, h), 3.f); }
    }
private:
    static inline const Spec S[] = { { "root", "Nota base", 0.f, 11.f, 0.f, 0.f, fNote, "" }, { "chord", "Acorde", 0.f, 3.f, 1.f, 0.f, fChoice, "Quintas|Menor|Mayor|Suspendido" }, { "oct", "Octava", 0.f, 2.f, 1.f, 0.f, fInt, "" },
        { "atk", "Ataque", 1.f, 40.f, 3.f, 0.f, fMs, "" }, { "rel", "Liberación", 10.f, 300.f, 60.f, 80.f, fMs, "" }, { "sib", "Sibilantes", 0.f, 1.f, .5f, 0.f, fPct, "" }, { "mix", "Mezcla", 0.f, 1.f, 1.f, 0.f, fPct, "" }, { "out", "Salida", -12.f, 12.f, 0.f, 0.f, fDb, "" } };
    double sr = 44100.0; BQ ma[NB][2], ca[NB][2]; float fcs[NB] {}, env[NB] {}, ph[4] {}; uint32_t rng = 1; std::atomic<float> bars[NB];
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjVocoder)
};
} // namespace injfx
