#pragma once
#include "FxCore.h"
// Espacio y movimiento: delay de cinta, reverso, panner automático, trémolo y modulación en anillo.
namespace injfx
{
inline float lfoShape (int shape, float ph)   // ph 0..1 → -1..1
{
    switch (shape) { case 0: return std::sin (2.f * kPi * ph); case 1: return ph < .5f ? 4.f * ph - 1.f : 3.f - 4.f * ph; default: { const float s = std::sin (2.f * kPi * ph); return std::tanh (4.f * s) / std::tanh (4.f); } }
}
inline void drawLfo (juce::Graphics& g, juce::Rectangle<float> r, int shape, float depth, float cycles, float phaseNow)
{
    juce::Path p; const float mid = r.getCentreY(), amp = (r.getHeight() * .5f - 10.f) * depth;
    for (int x = 0; x <= 568; x += 2) { const float ph = std::fmod (x / 568.f * cycles, 1.f), y = mid - lfoShape (shape, ph) * amp; if (x == 0) p.startNewSubPath (r.getX(), y); else p.lineTo (r.getX() + x, y); }
    g.setColour (theme::barLine); g.drawHorizontalLine ((int) mid, r.getX(), r.getRight()); g.setColour (theme::accent); g.strokePath (p, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float px = r.getX() + std::fmod (phaseNow * 568.f / cycles * cycles, 568.f) / 1.f; (void) px;
    const float cx = r.getX() + std::fmod (phaseNow, 1.f / cycles) * cycles * 568.f; g.setColour (theme::text); g.fillEllipse (cx - 4.5f, mid - lfoShape (shape, std::fmod (phaseNow * cycles, 1.f)) * amp - 4.5f, 9.f, 9.f);
}
inline double divBeats (int d) { static const double b[] { 0.0, 1.0, .5, .25, 2.0, 4.0 }; return b[juce::jlimit (0, 5, d)]; }   // 0 = libre

//==============================================================================
// Delay de cinta: eco con vaivén de velocidad (wow/flutter), saturación y oscurecimiento en cada repetición.
class InjTape : public InjFx
{
public:
    enum { pTime, pFree, pFb, pWow, pFlut, pSat, pTone, pMix };
    INJ_FX_BODY (InjTape, "injtape", "Delay de cinta", "Cinta", 8)
    Layout layout() const override { Layout L; L.pills = pTime; L.visualH = 70; L.rows = { { pFree, pFb, pWow, pFlut }, { pSat, pTone, pMix } }; L.dia = 54.f; L.icon = "M5 12a3 3 0 106 0 3 3 0 10-6 0M13 12a3 3 0 106 0 3 3 0 10-6 0M8 15h8"; return L; }
    double getTailLength() const override { const float fb = std::min (.95f, v (pFb)); return fb < .02f ? .5 : std::min (12.0, (double) lastSec * std::log (.001) / std::log ((double) fb)); }
    std::vector<Preset> presets() const override { return { { "Cinta vieja", { 2.f, 350.f, .5f, .5f, .3f, .5f, 3500.f, .3f } }, { "Eco de dub", { 1.f, 350.f, .65f, .2f, .1f, .4f, 2800.f, .35f } }, { "Slapback", { 0.f, 120.f, .15f, .3f, .2f, .3f, 5000.f, .3f } }, { "Inestable", { 3.f, 350.f, .55f, .9f, .6f, .6f, 3000.f, .35f } } }; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; for (auto& d : dl) { d.prep ((int) (2.8 * sr)); d.clear(); } sm.prep (sr, 120.f); first = true; lp[0] = lp[1] = hp[0] = hp[1] = 0.f; ph1 = ph2 = 0.f; lastSec = .35f; }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || dl[0].b.empty()) return; const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p0 = fc.destBuffer->getWritePointer (0, fc.bufferStartSample); float* p1 = nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : p0;
        const int m = vi (pTime); const double bpm = bpmAt (fc); const float beats[] { 0.f, 1.f, .5f, .75f }; lastSec = juce::jlimit (.02f, 2.5f, m == 0 ? v (pFree) * .001f : (float) (beats[m] * 60.0 / bpm * (m == 3 ? 1.f : 1.f)));
        if (m == 1) lastSec = (float) (60.0 / bpm); else if (m == 2) lastSec = (float) (30.0 / bpm); else if (m == 3) lastSec = (float) (45.0 / bpm);
        lastSec = juce::jlimit (.02f, 2.5f, lastSec); const float tgt = lastSec * (float) sr, fb = std::min (.95f, v (pFb)), wow = v (pWow) * .0035f * (float) sr * .4f, flu = v (pFlut) * .0006f * (float) sr * .4f, sat = 1.f + v (pSat) * 4.f, mix = v (pMix);
        const float cLp = 1.f - std::exp (-2.f * kPi * v (pTone) / (float) sr), cHp = 1.f - std::exp (-2.f * kPi * 60.f / (float) sr);
        if (first) { sm.snap (tgt); first = false; }
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            ph1 += .6f / (float) sr; if (ph1 > 1.f) ph1 -= 1.f; ph2 += 7.3f / (float) sr; if (ph2 > 1.f) ph2 -= 1.f;
            const float mod = std::sin (2.f * kPi * ph1) * wow + std::sin (2.f * kPi * ph2) * flu, t = std::max (8.f, sm.next (tgt) + mod);
            const float in[2] { p0[i], p1[i] }; float out[2];
            for (int c = 0; c < 2; ++c)
            {
                const float d = dl[c].read (t); out[c] = d; lp[c] += cLp * (d - lp[c]); hp[c] += cHp * (lp[c] - hp[c]);
                float f = std::tanh (sat * (lp[c] - hp[c]) * fb) / sat; dl[c].push (in[c] + f * (sat > 1.f ? 1.f : 1.f));
            }
            p0[i] = in[0] + out[0] * mix; p1[i] = in[1] + out[1] * mix;
        }
    }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        const float fb = std::min (.95f, v (pFb)); const float tmax = 568.f; float t = 0.f, a = 1.f; int k = 0; const float spacing = std::min (140.f, std::max (24.f, lastSec * 220.f));
        while (a > .03f && t < tmax && k < 40) { const float h = (r.getHeight() - 16.f) * a; g.setColour (theme::accent.withAlpha (std::max (.35f, a))); g.fillRoundedRectangle (r.getX() + t, r.getBottom() - 8.f - h, 6.f, h, 2.f); t += spacing; a *= std::max (.05f, fb); ++k; }
        label (g, juce::String (lastSec * 1000.f, 0) + " ms", { r.getRight() - 90.f, r.getY() + 4.f, 82.f, 14.f }, juce::Justification::right, 11.f, true, theme::text);
    }
private:
    static inline const Spec S[] = { { "time", "Tiempo", 0.f, 3.f, 0.f, 0.f, fChoice, "Libre|1/4|1/8|1/8 con punto" }, { "free", "Tiempo libre|Tiempo", 30.f, 2000.f, 350.f, 300.f, fMs, "" }, { "fb", "Repeticiones", 0.f, .95f, .5f, 0.f, fPct, "" },
        { "wow", "Vaivén lento|Vaivén", 0.f, 1.f, .3f, 0.f, fPct, "" }, { "flut", "Temblor rápido|Temblor", 0.f, 1.f, .2f, 0.f, fPct, "" }, { "sat", "Saturación|Satur.", 0.f, 1.f, .4f, 0.f, fPct, "" },
        { "tone", "Oscurecer", 800.f, 12000.f, 3500.f, 3000.f, fHz, "" }, { "mix", "Mezcla", 0.f, 1.f, .3f, 0.f, fPct, "" } };
    double sr = 44100.0; Delay dl[2]; Smooth sm; bool first = true; float lp[2] {}, hp[2] {}, ph1 = 0.f, ph2 = 0.f, lastSec = .35f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjTape)
};

//==============================================================================
// Reverso: graba un trozo del compás y lo reproduce al revés con fundidos (efecto "antes del golpe").
class InjReverse : public InjFx
{
public:
    enum { pLen, pFade, pFb, pMix, pDry };
    INJ_FX_BODY (InjReverse, "injreverse", "Reverso", "Reverso", 5)
    Layout layout() const override { Layout L; L.pills = pLen; L.visualH = 80; L.knobs = { pFade, pFb, pMix, pDry }; L.perRow = 4; L.dia = 60.f; L.icon = "M20 12H8M12 6l-6 6 6 6"; return L; }
    double getTailLength() const override { return (double) sliceSec * 2.0; }
    std::vector<Preset> presets() const override { return { { "Succión de un pulso", { 1.f, .3f, 0.f, .6f, 1.f } }, { "Subida de compás", { 3.f, .4f, .2f, .7f, 1.f } }, { "Reverso corto", { 0.f, .25f, 0.f, .5f, 1.f } }, { "Solo reverso", { 1.f, .3f, 0.f, 1.f, 0.f } } }; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; const int mx = (int) (4.5 * 60.0 / 30.0 * sr) + 16; for (int k = 0; k < 2; ++k) for (int c = 0; c < 2; ++c) buf[k][c].assign ((size_t) mx, 0.f); pos = 0; rec = 0; lenS = 0; }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || buf[0][0].empty()) return; const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : nullptr };
        static const float lb[] { .5f, 1.f, 2.f, 4.f }; const int want = juce::jlimit (16, (int) buf[0][0].size() - 8, (int) (lb[juce::jlimit (0, 3, vi (pLen))] * 60.0 / bpmAt (fc) * sr)); const float fade = v (pFade), fb = v (pFb), mix = v (pMix), dry = v (pDry);
        sliceSec = (float) want / (float) sr;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            if (lenS == 0 || pos >= lenS) { if (lenS != 0) rec ^= 1; lenS = want; pos = 0; }
            const int fl = std::max (1, (int) (fade * .5f * (float) lenS)); float env = 1.f; if (pos < fl) env = .5f - .5f * std::cos (kPi * (float) pos / (float) fl); else if (pos > lenS - fl) env = .5f - .5f * std::cos (kPi * (float) (lenS - pos) / (float) fl);
            for (int c = 0; c < nc; ++c)
            {
                const int ridx = std::min (lenS - 1, (int) buf[rec ^ 1][c].size() - 1) - pos; const float w = ridx >= 0 ? buf[rec ^ 1][c][(size_t) ridx] * env : 0.f; const float x = p[c][i];
                buf[rec][c][(size_t) pos] = x + w * fb; p[c][i] = x * dry + w * mix;
            }
            ++pos;
        }
        meterA.store (lenS > 0 ? (float) pos / (float) lenS : 0.f);
    }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        const float f = meterA.load(), mid = r.getCentreY(); const auto bar = r.reduced (16.f, 14.f); g.setColour (theme::bgBar); g.fillRoundedRectangle (bar, 8.f);
        juce::Path tri; tri.startNewSubPath (bar.getX() + 6.f, bar.getBottom() - 6.f); tri.lineTo (bar.getRight() - 6.f, bar.getY() + 6.f); tri.lineTo (bar.getRight() - 6.f, bar.getBottom() - 6.f); tri.closeSubPath(); g.setColour (theme::barLine); g.fillPath (tri);
        const float x = bar.getX() + 6.f + f * (bar.getWidth() - 12.f); g.setColour (theme::accent); g.fillRoundedRectangle (x - 2.f, bar.getY() + 4.f, 4.f, bar.getHeight() - 8.f, 2.f); (void) mid;
    }
private:
    static inline const Spec S[] = { { "len", "Duración", 0.f, 3.f, 1.f, 0.f, fChoice, "1/2 pulso|1 pulso|2 pulsos|1 compás" }, { "fade", "Fundido", 0.f, 1.f, .3f, 0.f, fPct, "" }, { "fb", "Realimentación|Realim.", 0.f, .8f, 0.f, 0.f, fPct, "" },
        { "mix", "Reverso", 0.f, 1.f, .6f, 0.f, fPct, "" }, { "dry", "Original", 0.f, 1.f, 1.f, 0.f, fPct, "" } };
    double sr = 44100.0; std::vector<float> buf[2][2]; int pos = 0, rec = 0, lenS = 0; float sliceSec = .4f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjReverse)
};

//==============================================================================
// Panner automático: mueve el sonido de lado a lado, sincronizado al tempo o libre.
class InjAutoPan : public InjFx
{
public:
    enum { pDiv, pHz, pDepth, pShape, pPhase };
    INJ_FX_BODY (InjAutoPan, "injautopan", "Panner automático", "AutoPan", 5)
    Layout layout() const override { Layout L; L.pills = pDiv; L.visualH = 100; L.knobs = { pHz, pDepth, pShape, pPhase }; L.perRow = 4; L.dia = 60.f; L.icon = "M3 12h4l2-6 4 12 2-6h6"; return L; }
    std::vector<Preset> presets() const override { return { { "Vaivén por pulso", { 1.f, 1.f, .7f, 0.f, 0.f } }, { "Rápido en corcheas", { 2.f, 1.f, .8f, 1.f, 0.f } }, { "Barrido lento", { 5.f, 1.f, 1.f, 0.f, 0.f } }, { "Salto cuadrado", { 3.f, 1.f, .6f, 2.f, 0.f } } }; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; ph = 0.f; }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.destBuffer->getNumChannels() < 2) return; float* L = fc.destBuffer->getWritePointer (0, fc.bufferStartSample); float* R = fc.destBuffer->getWritePointer (1, fc.bufferStartSample);
        const int d = vi (pDiv); const double db = divBeats (d), b0 = beatAt (fc), bps = bpmAt (fc) / 60.0 / sr; const float depth = v (pDepth), hz = v (pHz), off = v (pPhase); const int sh = vi (pShape);
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            if (d == 0) { ph += hz / (float) sr; if (ph > 1.f) ph -= 1.f; } else { double q = std::fmod ((b0 + i * bps) / db, 1.0); if (q < 0) q += 1.0; ph = (float) q; }
            const float pan = lfoShape (sh, std::fmod (ph + off * .5f, 1.f)) * depth, a = (pan * .5f + .5f) * (kPi * .5f), gl = std::cos (a) * 1.4142f, gr = std::sin (a) * 1.4142f; L[i] *= gl; R[i] *= gr;
        }
        meterA.store (ph);
    }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override { const float cyc = vi (pDiv) == 0 ? 2.f : 2.f; drawLfo (g, r, vi (pShape), v (pDepth), cyc, std::fmod (meterA.load(), 1.f) / cyc * 1.f); label (g, "Izq", { r.getX() + 6.f, r.getY() + 2.f, 40.f, 14.f }); label (g, "Der", { r.getX() + 6.f, r.getBottom() - 16.f, 40.f, 14.f }); }
private:
    static inline const Spec S[] = { { "div", "Velocidad", 0.f, 5.f, 1.f, 0.f, fChoice, "Libre|1 pulso|1/8|1/16|2 pulsos|1 compás" }, { "hz", "Frecuencia libre|Libre", .05f, 20.f, 1.f, 1.f, fRate, "" }, { "depth", "Profundidad", 0.f, 1.f, .7f, 0.f, fPct, "" },
        { "shape", "Forma", 0.f, 2.f, 0.f, 0.f, fChoice, "Seno|Triángulo|Cuadrada suave" }, { "phase", "Fase", 0.f, 1.f, 0.f, 0.f, fPct, "" } };
    double sr = 44100.0; float ph = 0.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjAutoPan)
};

//==============================================================================
// Trémolo: modula el volumen (sincronizado o libre) con desfase entre canales para efecto estéreo.
class InjTremolo : public InjFx
{
public:
    enum { pDiv, pHz, pDepth, pShape, pStereo, pMix };
    INJ_FX_BODY (InjTremolo, "injtremolo", "Trémolo", "Trémolo", 6)
    Layout layout() const override { Layout L; L.pills = pDiv; L.visualH = 100; L.knobs = { pHz, pDepth, pShape, pStereo, pMix }; L.dia = 58.f; L.icon = "M3 12c2-8 4-8 6 0s4 8 6 0 4-8 6 0"; return L; }
    std::vector<Preset> presets() const override { return { { "Trémolo clásico", { 0.f, 5.f, .6f, 0.f, 0.f, 1.f } }, { "Corte rítmico", { 3.f, 1.f, 1.f, 2.f, 0.f, 1.f } }, { "Vibración estéreo", { 0.f, 4.f, .5f, 0.f, 1.f, 1.f } }, { "Pulso por corcheas", { 2.f, 1.f, .8f, 1.f, 0.f, 1.f } } }; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; ph = 0.f; }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return; const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : nullptr };
        const int d = vi (pDiv), sh = vi (pShape); const double db = divBeats (d), b0 = beatAt (fc), bps = bpmAt (fc) / 60.0 / sr; const float depth = v (pDepth), hz = v (pHz), st = v (pStereo) * .5f, mix = v (pMix); float gl = 1.f;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            if (d == 0) { ph += hz / (float) sr; if (ph > 1.f) ph -= 1.f; } else { double q = std::fmod ((b0 + i * bps) / db, 1.0); if (q < 0) q += 1.0; ph = (float) q; }
            for (int c = 0; c < nc; ++c) { const float l = lfoShape (sh, std::fmod (ph + (c == 1 ? st : 0.f), 1.f)), g = 1.f - depth * (.5f - .5f * l); if (c == 0) gl = g; p[c][i] *= 1.f - mix + mix * g; }
        }
        meterA.store (ph); meterB.store (gl);
    }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override { drawLfo (g, r, vi (pShape), v (pDepth), 3.f, meterA.load() / 3.f); }
private:
    static inline const Spec S[] = { { "div", "Velocidad", 0.f, 5.f, 0.f, 0.f, fChoice, "Libre|1 pulso|1/8|1/16|2 pulsos|1 compás" }, { "hz", "Frecuencia libre|Libre", .1f, 30.f, 5.f, 4.f, fRate, "" }, { "depth", "Profundidad", 0.f, 1.f, .6f, 0.f, fPct, "" },
        { "shape", "Forma", 0.f, 2.f, 0.f, 0.f, fChoice, "Seno|Triángulo|Cuadrada suave" }, { "stereo", "Estéreo", 0.f, 1.f, 0.f, 0.f, fPct, "" }, { "mix", "Mezcla", 0.f, 1.f, 1.f, 0.f, fPct, "" } };
    double sr = 44100.0; float ph = 0.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjTremolo)
};

//==============================================================================
// Modulación en anillo: multiplica la señal por un oscilador; suena metálico y de robot.
class InjRing : public InjFx
{
public:
    enum { pFreq, pShape, pLfo, pRate, pMix, pOut };
    INJ_FX_BODY (InjRing, "injring", "Modulación en anillo", "Anillo", 6)
    Layout layout() const override { Layout L; L.pills = pShape; L.visualH = 80; L.knobs = { pFreq, pLfo, pRate, pMix, pOut }; L.dia = 58.f; L.icon = "M8 12a4 4 0 108 0 4 4 0 10-8 0M12 12a4 4 0 108 0 4 4 0 10-8 0"; return L; }
    double getLatencySeconds() override { return (double) os.lat / sr; }
    std::vector<Preset> presets() const override { return { { "Robot", { 150.f, 0.f, 0.f, 1.f, 1.f, 0.f } }, { "Metálico", { 780.f, 0.f, 0.f, 1.f, .7f, -2.f } }, { "Dalek", { 30.f, 1.f, 0.f, 1.f, 1.f, -1.f } }, { "Campana que cae", { 400.f, 0.f, .8f, .4f, .8f, -2.f } } }; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; os.prep(); ph = 0.f; lph = 0.f; }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return; const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : nullptr };
        const int sh = vi (pShape); const float f0 = v (pFreq), la = v (pLfo), lr = v (pRate), mix = v (pMix), og = db2lin (v (pOut)); float cur = f0;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            lph += lr / (float) sr; if (lph > 1.f) lph -= 1.f; cur = f0 * std::pow (2.f, la * 2.f * std::sin (2.f * kPi * lph)); ph += cur / (float) sr; if (ph > 1.f) ph -= 1.f;
            const float car = sh == 0 ? std::sin (2.f * kPi * ph) : (ph < .5f ? 1.f : -1.f) * .9f; for (int c = 0; c < nc; ++c) { const float x = p[c][i]; p[c][i] = x + (x * car * og - x) * mix; }
        }
        meterA.store (cur);
    }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        const float f = meterA.load() > 1.f ? meterA.load() : v (pFreq); const float cycles = juce::jlimit (1.f, 24.f, f / 40.f); juce::Path p; const float mid = r.getCentreY(), amp = r.getHeight() * .5f - 10.f;
        for (int x = 0; x <= 568; ++x) { const float t = x / 568.f * cycles, s = vi (pShape) == 0 ? std::sin (2.f * kPi * t) : (std::fmod (t, 1.f) < .5f ? 1.f : -1.f), env = std::sin (2.f * kPi * x / 568.f * 1.5f); const float y = mid - s * env * amp; if (x == 0) p.startNewSubPath (r.getX(), y); else p.lineTo (r.getX() + x, y); }
        g.setColour (theme::accent); g.strokePath (p, juce::PathStrokeType (2.f)); label (g, fmtValue ({ "", "", 0, 0, 0, 0, fHz, "" }, f), { r.getRight() - 90.f, r.getY() + 4.f, 82.f, 14.f }, juce::Justification::right, 11.f, true, theme::text);
    }
private:
    static inline const Spec S[] = { { "freq", "Frecuencia", 20.f, 5000.f, 150.f, 400.f, fHz, "" }, { "shape", "Onda", 0.f, 1.f, 0.f, 0.f, fChoice, "Seno|Cuadrada" }, { "lfo", "Variación|Variac.", 0.f, 1.f, 0.f, 0.f, fPct, "" },
        { "rate", "Velocidad|Veloc.", .05f, 20.f, 1.f, 1.f, fRate, "" }, { "mix", "Mezcla", 0.f, 1.f, 1.f, 0.f, fPct, "" }, { "out", "Salida", -12.f, 12.f, 0.f, 0.f, fDb, "" } };
    double sr = 44100.0; OS2 os; float ph = 0.f, lph = 0.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjRing)
};
} // namespace injfx
