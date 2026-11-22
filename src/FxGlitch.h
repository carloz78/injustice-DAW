#pragma once
#include "FxCore.h"
// Creativos: cortador rítmico, repetidor de ritmo, parada de cinta, granular y glitch.
namespace injfx
{
inline uint32_t hash32 (uint32_t x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
inline float hash01 (uint32_t x) { return (float) (hash32 (x) & 0xffffff) / 16777216.f; }

struct StereoRing
{
    std::vector<float> b[2]; int mask = 0; long long w = 0;
    void prep (int n) { int s = 16; while (s < n) s <<= 1; for (auto& v : b) v.assign ((size_t) s, 0.f); mask = s - 1; w = 0; }
    bool ok() const { return mask > 0; }
    void push (float l, float r) { b[0][(size_t) (w & mask)] = l; b[1][(size_t) (w & mask)] = r; ++w; }
    float get (int c, long long i) const { return b[c][(size_t) (i & mask)]; }
    float getf (int c, double i) const { const long long k = (long long) std::floor (i); const float f = (float) (i - (double) k); return get (c, k) * (1.f - f) + get (c, k + 1) * f; }
};
inline double gridBeats3 (int g) { static const double d[] { .5, .25, .125 }; return d[juce::jlimit (0, 2, g)]; }

//==============================================================================
#define INJ_STEP(n) { "s" #n, "Paso " #n, 0.f, 1.f, 1.f, 0.f, fChoice, "Apagado|Encendido" }
class InjStutter : public InjFx
{
public:
    enum { pGrid, pSmooth, pMix, pStep0 };
    INJ_FX_BODY (InjStutter, "injstutter", "Cortador rítmico", "Cortar", 19)
    Layout layout() const override { Layout L; L.pills = pGrid; L.visualH = 96; L.knobs = { pSmooth, pMix }; L.perRow = 2; L.dia = 58.f; L.icon = "M3 6h4v12H3zM10 6h4v12h-4zM17 6h4v12h-4z"; return L; }
    std::vector<Preset> presets() const override
    {
        return { { "Corcheas", { 1.f, 3.f, 1.f, 1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0 } }, { "Galope", { 1.f, 3.f, 1.f, 1,1,0,1, 1,1,0,1, 1,1,0,1, 1,1,0,1 } }, { "Trance gate", { 1.f, 2.f, 1.f, 1,0,1,1, 0,1,1,0, 1,0,1,1, 0,1,0,1 } },
                 { "Roto", { 1.f, 2.f, 1.f, 1,1,1,0, 1,1,0,0, 1,0,1,1, 0,1,0,0 } }, { "Todo abierto", { 1.f, 3.f, 1.f, 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1 } } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; g = 1.f; }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return; const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : nullptr };
        const double db = gridBeats3 (vi (pGrid)), b0 = beatAt (fc), bps = bpmAt (fc) / 60.0 / sr; const float a = coefMs (std::max (.2f, v (pSmooth)), sr), mix = v (pMix); int step = 0;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            const long long k = (long long) std::floor ((b0 + i * bps) / db); step = (int) (((k % 16) + 16) % 16); const float tg = v (pStep0 + step) > .5f ? 1.f : 0.f; g = a * g + (1.f - a) * tg;
            const float gm = 1.f - mix + mix * g; for (int c = 0; c < nc; ++c) p[c][i] *= gm;
        }
        meterA.store ((float) step);
    }
    void visualClickAt (juce::Point<float> pt, juce::Rectangle<float> r) override { const float w = (r.getWidth() - 16.f) / 16.f; const int k = (int) ((pt.x - r.getX() - 8.f) / w); if (k >= 0 && k < 16) setValue (pStep0 + k, v (pStep0 + k) > .5f ? 0.f : 1.f); }
    void paintVisual (juce::Graphics& gr, juce::Rectangle<float> r) override
    {
        const float w = (r.getWidth() - 16.f) / 16.f; const int cur = (int) meterA.load();
        for (int k = 0; k < 16; ++k) { const bool on = v (pStep0 + k) > .5f; const auto c = juce::Rectangle<float> (r.getX() + 8.f + k * w + 2.f, r.getY() + 14.f, w - 4.f, r.getHeight() - 28.f); gr.setColour (on ? (k % 4 == 0 ? theme::accent : theme::accent.withAlpha (.8f)) : theme::bgBar); gr.fillRoundedRectangle (c, 5.f); if (k == cur) { gr.setColour (theme::text); gr.drawRoundedRectangle (c.expanded (1.f), 5.f, 2.f); } }
    }
private:
    static inline const Spec S[] = { { "grid", "Rejilla", 0.f, 2.f, 1.f, 0.f, fChoice, "1/8|1/16|1/32" }, { "smooth", "Suavizado", .2f, 30.f, 3.f, 5.f, fMs, "" }, { "mix", "Mezcla", 0.f, 1.f, 1.f, 0.f, fPct, "" },
        INJ_STEP (1), INJ_STEP (2), INJ_STEP (3), INJ_STEP (4), INJ_STEP (5), INJ_STEP (6), INJ_STEP (7), INJ_STEP (8), INJ_STEP (9), INJ_STEP (10), INJ_STEP (11), INJ_STEP (12), INJ_STEP (13), INJ_STEP (14), INJ_STEP (15), INJ_STEP (16) };
    double sr = 44100.0; float g = 1.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjStutter)
};

//==============================================================================
class InjBeatRepeat : public InjFx
{
public:
    enum { pSlice, pChance, pDur, pDecay, pMix };
    INJ_FX_BODY (InjBeatRepeat, "injrepeat", "Repetidor de ritmo", "Repetir", 5)
    Layout layout() const override { Layout L; L.pills = pSlice; L.visualH = 80; L.knobs = { pChance, pDur, pDecay, pMix }; L.perRow = 4; L.dia = 60.f; L.icon = "M17 2l4 4-4 4M3 11V9a3 3 0 013-3h15M7 22l-4-4 4-4M21 13v2a3 3 0 01-3 3H3"; return L; }
    std::vector<Preset> presets() const override { return { { "Tartamudeo", { 1.f, .5f, 2.f, 0.f, 1.f } }, { "Rodillo", { 2.f, .6f, 4.f, .3f, 1.f } }, { "Repetir siempre", { 0.f, 1.f, 2.f, 0.f, 1.f } }, { "Eco que cae", { 1.f, .7f, 8.f, .9f, 1.f } } }; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; ring.prep ((int) (4.5 * sr)); lastG = -999999999LL; active = false; start = 0; rpos = 0; }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || ! ring.ok()) return; const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : fc.destBuffer->getWritePointer (0, fc.bufferStartSample) };
        static const double sb[] { .5, .25, .125, 1. }; const double sliceB = sb[juce::jlimit (0, 3, vi (pSlice))]; const int dur = juce::jlimit (1, 8, vi (pDur)); const double b0 = beatAt (fc), bps = bpmAt (fc) / 60.0 / sr; const int len = std::max (32, (int) (sliceB / bps));
        const float chance = v (pChance), decay = v (pDecay), mix = v (pMix);
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            const long long gi = (long long) std::floor ((b0 + i * bps) / (sliceB * dur));
            if (gi != lastG) { lastG = gi; active = hash01 ((uint32_t) gi * 2654435761u + 17u) < chance; if (active) { start = ring.w - len; rpos = 0; } }
            const float x0 = p[0][i], x1 = p[1][i]; float o0 = x0, o1 = x1;
            if (active)
            {
                const int pos = rpos % len, n = rpos / len; const float fade = std::min ({ 1.f, (float) pos / 32.f, (float) (len - pos) / 32.f }), gn = fade * (1.f - decay * (float) n / (float) dur);
                o0 = x0 + (ring.get (0, start + pos) * gn - x0) * mix; o1 = x1 + (ring.get (1, start + pos) * gn - x1) * mix; ++rpos;
            }
            ring.push (x0, x1); p[0][i] = o0; if (nc > 1) p[1][i] = o1;
        }
        meterA.store (active ? 1.f : 0.f);
    }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        const int dur = juce::jlimit (1, 8, vi (pDur)); const float w = (r.getWidth() - 24.f) / (float) dur, dec = v (pDecay);
        for (int k = 0; k < dur; ++k) { const float h = (r.getHeight() - 24.f) * (1.f - dec * (float) k / (float) dur); g.setColour (theme::accent.withAlpha (meterA.load() > .5f ? 1.f : .6f)); g.fillRoundedRectangle (r.getX() + 12.f + k * w + 2.f, r.getBottom() - 10.f - h, w - 4.f, h, 4.f); }
    }
private:
    static inline const Spec S[] = { { "slice", "Trozo", 0.f, 3.f, 1.f, 0.f, fChoice, "1/8|1/16|1/32|1/4" }, { "chance", "Probabilidad", 0.f, 1.f, .5f, 0.f, fPct, "" }, { "dur", "Repeticiones", 1.f, 8.f, 2.f, 0.f, fInt, "" },
        { "decay", "Caída", 0.f, 1.f, 0.f, 0.f, fPct, "" }, { "mix", "Mezcla", 0.f, 1.f, 1.f, 0.f, fPct, "" } };
    double sr = 44100.0; StereoRing ring; long long lastG = 0, start = 0; bool active = false; int rpos = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjBeatRepeat)
};

//==============================================================================
class InjTapeStop : public InjFx
{
public:
    enum { pStop, pTime, pSpin, pCurve };
    INJ_FX_BODY (InjTapeStop, "injtapestop", "Parada de cinta", "Parada", 4)
    Layout layout() const override { Layout L; L.pills = pStop; L.visualH = 90; L.knobs = { pTime, pSpin, pCurve }; L.perRow = 3; L.dia = 62.f; L.icon = "M12 3a9 9 0 100 18 9 9 0 000-18zM12 9a3 3 0 100 6 3 3 0 000-6z"; return L; }
    std::vector<Preset> presets() const override { return { { "Parada rápida", { 0.f, .35f, .25f, .5f } }, { "Parada lenta", { 0.f, 1.5f, .6f, .3f } }, { "Apagón de disco", { 0.f, .8f, 1.f, .8f } } }; }
    double getTailLength() const override { return (double) v (pTime) + .1; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; for (auto& d : dl) { d.prep ((int) (4.5 * sr)); d.clear(); } u = 1.f; lag = 0.f; xf = 0.f; ang = 0.f; }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || dl[0].b.empty()) return; const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : nullptr };
        const bool stop = vi (pStop) == 1; const float dT = 1.f / (std::max (.03f, v (pTime)) * (float) sr), dS = 1.f / (std::max (.03f, v (pSpin)) * (float) sr), ex = 1.f + 2.f * v (pCurve); const float maxLag = (float) (4.2 * sr); float sp = 1.f;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            for (int c = 0; c < nc; ++c) dl[c].push (p[c][i]);
            u = stop ? std::max (0.f, u - dT) : std::min (1.f, u + dS); sp = std::pow (u, ex);
            if (u < 1.f || lag > 0.f)
            {
                lag = std::min (maxLag, lag + (1.f - sp)); float g = 1.f;
                if (u >= 1.f && ! stop) { xf += 1.f / (.03f * (float) sr); if (xf >= 1.f) { lag = 0.f; xf = 0.f; g = 0.f; } else g = 1.f - xf; } else xf = 0.f;
                for (int c = 0; c < nc; ++c) { const float x = p[c][i]; const float w = (lag > 0.f ? dl[c].read (lag) : x) * std::min (1.f, sp * 2.5f); p[c][i] = w * g + x * (1.f - g) * (lag > 0.f || g < 1.f ? 1.f : 0.f); }
            }
            ang += sp * .12f;
        }
        meterA.store (sp); meterB.store (std::fmod (ang, 6.2832f));
    }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        const auto c = r.getCentre(); const float rad = r.getHeight() * .38f, a = meterB.load(); g.setColour (theme::bgBar); g.fillEllipse (c.x - rad, c.y - rad, rad * 2.f, rad * 2.f); g.setColour (theme::accent);
        for (int k = 0; k < 3; ++k) { const float t = a + k * 2.0944f; g.drawLine (c.x, c.y, c.x + std::cos (t) * rad * .9f, c.y + std::sin (t) * rad * .9f, 3.f); } g.fillEllipse (c.x - 5.f, c.y - 5.f, 10.f, 10.f);
        label (g, juce::String (juce::roundToInt (meterA.load() * 100.f)) + " %", { r.getRight() - 90.f, r.getY() + 6.f, 82.f, 16.f }, juce::Justification::right, 13.f, true, theme::text);
    }
private:
    static inline const Spec S[] = { { "stop", "Estado", 0.f, 1.f, 0.f, 0.f, fChoice, "Reproducir|Parar" }, { "time", "Tiempo de parada|Parada", .05f, 3.f, .6f, .5f, fSec, "" }, { "spin", "Tiempo de arranque|Arranque", .05f, 2.f, .3f, .3f, fSec, "" }, { "curve", "Curva", 0.f, 1.f, .5f, 0.f, fPct, "" } };
    double sr = 44100.0; Delay dl[2]; float u = 1.f, lag = 0.f, xf = 0.f, ang = 0.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjTapeStop)
};

//==============================================================================
class InjGranular : public InjFx
{
public:
    enum { pSize, pDens, pSpray, pPitch, pFreeze, pMix };
    INJ_FX_BODY (InjGranular, "injgranular", "Granular", "Granos", 6)
    Layout layout() const override { Layout L; L.pills = pFreeze; L.visualH = 84; L.rows = { { pSize, pDens, pSpray }, { pPitch, pMix } }; L.dia = 54.f; L.icon = "M5 5h2v2H5zM11 3h2v2h-2zM17 6h2v2h-2zM8 11h2v2H8zM14 12h2v2h-2zM4 17h2v2H4zM11 18h2v2h-2zM18 17h2v2h-2z"; return L; }
    std::vector<Preset> presets() const override { return { { "Nube", { 120.f, 30.f, 250.f, 0.f, 0.f, .6f } }, { "Congelar", { 200.f, 40.f, 80.f, 0.f, 1.f, 1.f } }, { "Octava brillante", { 90.f, 45.f, 180.f, 12.f, 0.f, .5f } }, { "Polvo", { 30.f, 80.f, 500.f, -5.f, 0.f, .7f } } }; }
    double getTailLength() const override { return v (pFreeze) > .5f ? 4.0 : .6; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; ring.prep ((int) (5.5 * sr)); for (auto& g : gr) g.on = false; acc = 0.f; rng = 2463534242u; frozenW = 0; wasFrozen = false; }
    float rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (float) (rng & 0xffffff) / 16777216.f; }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || ! ring.ok()) return; const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : fc.destBuffer->getWritePointer (0, fc.bufferStartSample) };
        const int len = std::max (64, (int) (v (pSize) * .001f * (float) sr)); const float dens = v (pDens), spray = v (pSpray) * .001f * (float) sr, inc = std::pow (2.f, v (pPitch) / 12.f), mix = v (pMix); const bool fz = v (pFreeze) > .5f;
        const float scale = 1.3f / std::sqrt (std::max (1.f, dens * v (pSize) * .001f));
        if (fz && ! wasFrozen) frozenW = ring.w; wasFrozen = fz;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            const float x0 = p[0][i], x1 = p[1][i]; if (! fz) ring.push (x0, x1);
            const long long wr = fz ? frozenW : ring.w;
            acc += dens / (float) sr; while (acc >= 1.f) { acc -= 1.f; for (auto& g : gr) if (! g.on) { float lag = 2205.f + rnd() * spray; if (! fz) lag = std::max (lag, (float) len * std::max (0.f, inc - 1.f) + 96.f); lag = std::min (lag, (float) (ring.mask - len * 2 - 8)); g.on = true; g.idx = (double) (wr - (long long) lag); g.inc = inc * (1.0 + (rnd() - .5) * .004); g.age = 0; g.len = len; g.pan = rnd(); break; } }
            float o0 = 0.f, o1 = 0.f;
            for (auto& g : gr) if (g.on)
            {
                const float w = .5f - .5f * std::cos (2.f * kPi * (float) g.age / (float) g.len); const float s0 = ring.getf (0, g.idx) * w, s1 = ring.getf (1, g.idx) * w; o0 += s0 * (1.f - g.pan * .6f); o1 += s1 * (.4f + g.pan * .6f);
                g.idx += g.inc; if (++g.age >= g.len) g.on = false;
            }
            p[0][i] = x0 * (1.f - mix) + o0 * scale * mix; if (nc > 1) p[1][i] = x1 * (1.f - mix) + o1 * scale * mix;
        }
    }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        const float sz = v (pSize), dn = v (pDens); const float tspan = 400.f; const float step = 1000.f / std::max (1.f, dn); const bool fz = v (pFreeze) > .5f; int k = 0;
        for (float t = 0.f; t < tspan && k < 80; t += step, ++k) { const float w = sz / tspan * r.getWidth(), x = r.getX() + 10.f + t / tspan * (r.getWidth() - 20.f - w * .5f), h = (r.getHeight() - 24.f) * (.5f + .5f * hash01 ((uint32_t) k * 77u + 5u)); juce::Path p; p.startNewSubPath (x, r.getBottom() - 10.f);
            for (int j = 0; j <= 16; ++j) p.lineTo (x + w * j / 16.f, r.getBottom() - 10.f - h * (.5f - .5f * std::cos (2.f * kPi * j / 16.f))); g.setColour (theme::accent.withAlpha (fz ? .9f : .5f)); g.strokePath (p, juce::PathStrokeType (1.5f)); }
    }
private:
    struct Grain { bool on = false; double idx = 0.0, inc = 1.0; int age = 0, len = 1; float pan = .5f; };
    static inline const Spec S[] = { { "size", "Tamaño", 20.f, 400.f, 120.f, 120.f, fMs, "" }, { "dens", "Granos por segundo|Granos/s", 2.f, 100.f, 30.f, 25.f, fInt, "" }, { "spray", "Dispersión", 0.f, 800.f, 250.f, 150.f, fMs, "" }, { "pitch", "Tono", -24.f, 24.f, 0.f, 0.f, fSemi, "" },
        { "freeze", "Entrada", 0.f, 1.f, 0.f, 0.f, fChoice, "En vivo|Congelar" }, { "mix", "Mezcla", 0.f, 1.f, .6f, 0.f, fPct, "" } };
    double sr = 44100.0; StereoRing ring; Grain gr[48]; float acc = 0.f; uint32_t rng = 1; long long frozenW = 0; bool wasFrozen = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjGranular)
};

//==============================================================================
class InjGlitch : public InjFx
{
public:
    enum { pType, pGrid, pChance, pSeed, pMix };
    INJ_FX_BODY (InjGlitch, "injglitch", "Glitch", "Glitch", 5)
    Layout layout() const override { Layout L; L.pills = pType; L.visualH = 76; L.knobs = { pGrid, pChance, pSeed, pMix }; L.perRow = 4; L.dia = 60.f; L.icon = "M3 8h6v3H3zM11 13h10v3H11zM15 4h6v3h-6z"; return L; }
    std::vector<Preset> presets() const override { return { { "Mixto suave", { 0.f, 1.f, .25f, 7.f, 1.f } }, { "Caos", { 0.f, 2.f, .6f, 21.f, 1.f } }, { "Solo reversos", { 2.f, 0.f, .35f, 3.f, 1.f } }, { "Cortes", { 4.f, 1.f, .3f, 12.f, 1.f } } }; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; ring.prep ((int) (4.5 * sr)); }
    // 0 = nada, 1 = repetir cuarto, 2 = reverso, 3 = lento, 4 = corte
    int actionFor (long long k) const
    {
        const int ty = vi (pType); if (hash01 ((uint32_t) k * 2654435761u + (uint32_t) vi (pSeed) * 7919u) >= v (pChance)) return 0;
        if (ty > 0) return ty; return 1 + (int) (hash01 ((uint32_t) k * 40503u + (uint32_t) vi (pSeed) * 104729u + 3u) * 4.f) % 4;
    }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || ! ring.ok()) return; const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : fc.destBuffer->getWritePointer (0, fc.bufferStartSample) };
        const double db = gridBeats3 (vi (pGrid)), b0 = beatAt (fc), bps = bpmAt (fc) / 60.0 / sr; const int len = std::max (64, (int) (db / bps)); const float mix = v (pMix); int act = 0;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            const double bb = (b0 + i * bps) / db; const long long k = (long long) std::floor (bb); const int j = juce::jlimit (0, len - 1, (int) ((bb - (double) k) * len)); act = actionFor (k); const float x0 = p[0][i], x1 = p[1][i];
            float o0 = x0, o1 = x1;
            if (act > 0)
            {
                const float fade = std::min ({ 1.f, (float) j / 48.f, (float) (len - j) / 48.f }); const long long prevStart = ring.w - j - len; int jj = j; float wet0 = 0.f, wet1 = 0.f;
                if (act == 1) { const int q = std::max (16, len / 4); jj = (len - q) + (j % q); } else if (act == 2) jj = len - 1 - j; else if (act == 3) jj = j / 2;
                if (act != 4) { wet0 = ring.get (0, prevStart + jj) * fade; wet1 = ring.get (1, prevStart + jj) * fade; }
                o0 = x0 + (wet0 - x0) * mix; o1 = x1 + (wet1 - x1) * mix;
            }
            ring.push (x0, x1); p[0][i] = o0; if (nc > 1) p[1][i] = o1;
        }
        meterA.store ((float) act);
    }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        const juce::Colour cols[] { theme::bgBar, juce::Colour (0xffffb020), theme::accent, juce::Colour (0xff4fc3f7), juce::Colour (0xff6b6e78) };
        const float w = (r.getWidth() - 16.f) / 16.f; for (int k = 0; k < 16; ++k) { const int a = actionFor (k); g.setColour (cols[a]); g.fillRoundedRectangle (r.getX() + 8.f + k * w + 2.f, r.getY() + 12.f, w - 4.f, r.getHeight() - 24.f, 5.f); }
    }
private:
    static inline const Spec S[] = { { "type", "Tipo", 0.f, 4.f, 0.f, 0.f, fChoice, "Mixto|Repetir|Reverso|Lento|Corte" }, { "grid", "Rejilla", 0.f, 2.f, 1.f, 0.f, fChoice, "1/8|1/16|1/32" }, { "chance", "Probabilidad", 0.f, 1.f, .3f, 0.f, fPct, "" },
        { "seed", "Semilla", 0.f, 99.f, 7.f, 0.f, fInt, "" }, { "mix", "Mezcla", 0.f, 1.f, 1.f, 0.f, fPct, "" } };
    double sr = 44100.0; StereoRing ring;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjGlitch)
};
} // namespace injfx
