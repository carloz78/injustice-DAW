#pragma once
#include "FxCore.h"
// Distorsión y color: clipper, bitcrusher, waveshaper, drive para kick de gabber, EQ de DJ, auto-filtro y filtro de formantes.
namespace injfx
{
// curvas compartidas
inline float triFold (float x) { float t = std::fmod (x + 1.f, 4.f); if (t < 0.f) t += 4.f; return t < 2.f ? t - 1.f : 3.f - t; }
inline float softAlg (float x) { return x / std::sqrt (1.f + x * x); }
inline float cubicClip (float x) { const float u = juce::jlimit (-1.f, 1.f, x); return 1.5f * (u - u * u * u / 3.f); }
inline void drawTransfer (juce::Graphics& g, juce::Rectangle<float> r, const std::function<float (float)>& f, float xScale)
{
    g.setColour (theme::gridLine); for (int k = 1; k < 4; ++k) { g.drawVerticalLine ((int) (r.getX() + r.getWidth() * k / 4.f), r.getY(), r.getBottom()); g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * k / 4.f), r.getX(), r.getRight()); }
    g.setColour (theme::barLine); g.drawLine (r.getX(), r.getBottom(), r.getRight(), r.getY(), 1.f); juce::Path p;
    for (int k = 0; k <= 160; ++k) { const float x = -1.f + k * (2.f / 160.f), y = juce::jlimit (-1.f, 1.f, f (x * xScale)); const float px = r.getCentreX() + x * r.getWidth() * .5f, py = r.getCentreY() - y * (r.getHeight() * .5f - 4.f); if (k == 0) p.startNewSubPath (px, py); else p.lineTo (px, py); }
    g.setColour (theme::accent); g.strokePath (p, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
// Clipper con sobremuestreo: recorta al techo con curva dura, suave o cúbica.
class InjClipper : public InjFx
{
public:
    enum { pType, pDrive, pCeil, pMix };
    INJ_FX_BODY (InjClipper, "injclipper", "Clipper", "Clip", 4)
    Layout layout() const override { Layout L; L.pills = pType; L.visualH = 120; L.knobs = { pDrive, pCeil, pMix }; L.perRow = 3; L.dia = 60.f; L.icon = "M3 18L8 6h8l5 12"; return L; }
    double getLatencySeconds() override { return (double) os.lat / sr; }
    std::vector<Preset> presets() const override { return { { "Kick gabber", { 0.f, 12.f, -1.f, 1.f } }, { "Master seguro", { 1.f, 4.f, -.3f, 1.f } }, { "Saturar suave", { 2.f, 6.f, -2.f, .7f } }, { "Aplastar", { 0.f, 20.f, -3.f, 1.f } } }; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; os.prep(); sDr.prep (sr, 20.f); sCe.prep (sr, 20.f); sMx.prep (sr, 20.f); }
    static float shape (int t, float x) { return t == 0 ? juce::jlimit (-1.f, 1.f, x) : t == 1 ? softAlg (x) : cubicClip (x); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || ! os.ready()) return; const int t = vi (pType); const float dr = db2lin (v (pDrive)), ce = db2lin (v (pCeil)), mix = v (pMix);
        os.run (fc, [&] (int c, float x) { return x * sDr.next (c, dr); }, [&] (int, float x) { return shape (t, x); }, [&] (int c, float y, float d) { return d + (y * sCe.next (c, ce) - d) * sMx.next (c, mix); });
    }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override { const int t = vi (pType); const float dr = db2lin (v (pDrive)), ce = db2lin (v (pCeil)); drawTransfer (g, r, [&] (float x) { return shape (t, x * dr) * ce; }, 1.f); label (g, "Salida", { r.getX() + 8.f, r.getY() + 4.f, 60.f, 14.f }); label (g, "Entrada", { r.getRight() - 68.f, r.getBottom() - 18.f, 60.f, 14.f }, juce::Justification::right); }
private:
    static inline const Spec S[] = { { "type", "Curva", 0.f, 2.f, 0.f, 0.f, fChoice, "Dura|Suave|Cúbica" }, { "drive", "Empuje", 0.f, 24.f, 6.f, 0.f, fDb, "" }, { "ceil", "Techo", -12.f, 0.f, -.3f, 0.f, fDb, "" }, { "mix", "Mezcla", 0.f, 1.f, 1.f, 0.f, fPct, "" } };
    double sr = 44100.0; OS2 os; SmoothSt sDr, sCe, sMx;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjClipper)
};

//==============================================================================
// Bitcrusher: reduce la resolución (bits) y la frecuencia de muestreo. El aliasing es parte del carácter, por eso no lleva sobremuestreo.
class InjCrusher : public InjFx
{
public:
    enum { pBits, pRate, pJit, pMix, pOut };
    INJ_FX_BODY (InjCrusher, "injcrusher", "Bitcrusher", "Crush", 5)
    Layout layout() const override { Layout L; L.visualH = 90; L.knobs = { pBits, pRate, pJit, pMix, pOut }; L.icon = "M3 18h4v-4h4v-4h4V6h6"; return L; }
    std::vector<Preset> presets() const override { return { { "8 bits clásico", { 8.f, 22050.f, 0.f, 1.f, 0.f } }, { "Lo-fi sucio", { 6.f, 8000.f, .3f, .8f, 0.f } }, { "Destruido", { 3.f, 4000.f, .5f, 1.f, -3.f } }, { "Aire digital", { 12.f, 16000.f, 0.f, .4f, 0.f } } }; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; acc = 1.f; hold[0] = hold[1] = 0.f; rng = 12345u; }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return; const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : nullptr };
        const float L = std::pow (2.f, v (pBits) - 1.f), step = std::min (1.f, v (pRate) / (float) sr), jit = v (pJit), mix = v (pMix), og = db2lin (v (pOut));
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            acc += step; if (acc >= 1.f) { acc -= 1.f; rng = rng * 1664525u + 1013904223u; const float j = ((float) (rng >> 9) / 8388608.f - .5f) * jit; acc += j * step * .5f;
                for (int c = 0; c < nc; ++c) hold[c] = std::round (juce::jlimit (-1.f, 1.f, p[c][i]) * L) / L; }
            for (int c = 0; c < nc; ++c) p[c][i] = (p[c][i] + (hold[c] * og - p[c][i]) * mix);
        }
    }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        const float L = std::pow (2.f, v (pBits) - 1.f), holdN = std::max (1.f, 44100.f / v (pRate)); juce::Path p; float last = 0.f; int cnt = 0;
        for (int x = 0; x <= 568; x += 1) { if (cnt++ % std::max (1, (int) (holdN * 2.f)) == 0) last = std::round (std::sin (x / 568.f * 6.2832f * 2.f) * L) / L; const float y = r.getCentreY() - last * (r.getHeight() * .5f - 8.f); if (x == 0) p.startNewSubPath (r.getX(), y); else p.lineTo (r.getX() + x, y); }
        g.setColour (theme::accent); g.strokePath (p, juce::PathStrokeType (2.f, juce::PathStrokeType::mitered));
    }
private:
    static inline const Spec S[] = { { "bits", "Bits", 1.f, 16.f, 8.f, 8.f, fInt, "" }, { "rate", "Muestreo", 500.f, 44100.f, 44100.f, 8000.f, fHz, "" }, { "jit", "Temblor", 0.f, 1.f, 0.f, 0.f, fPct, "" }, { "mix", "Mezcla", 0.f, 1.f, 1.f, 0.f, fPct, "" }, { "out", "Salida", -12.f, 12.f, 0.f, 0.f, fDb, "" } };
    double sr = 44100.0; float acc = 1.f, hold[2] {}; uint32_t rng = 1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjCrusher)
};

//==============================================================================
// Waveshaper: pliegue, seno, Chebyshev y rectificador, con simetría (sesgo) y tono.
class InjShaper : public InjFx
{
public:
    enum { pType, pDrive, pBias, pTone, pOut, pMix };
    INJ_FX_BODY (InjShaper, "injshaper", "Waveshaper", "Shaper", 6)
    Layout layout() const override { Layout L; L.pills = pType; L.visualH = 110; L.knobs = { pDrive, pBias, pTone, pOut, pMix }; L.dia = 56.f; L.icon = "M3 12c3-9 6-9 9 0s6 9 9 0"; return L; }
    double getLatencySeconds() override { return (double) os.lat / sr; }
    std::vector<Preset> presets() const override { return { { "Pliegue metálico", { 0.f, .6f, .2f, .2f, -3.f, .8f } }, { "Seno cálido", { 1.f, .4f, 0.f, 0.f, 0.f, 1.f } }, { "Tercer armónico", { 2.f, .5f, 0.f, 0.f, -2.f, .7f } }, { "Octava arriba", { 3.f, .6f, 0.f, 0.f, -2.f, .6f } } }; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; os.prep(); lo.reset(); hi.reset(); lastTone = -9.f; sDr.prep (sr, 20.f); sOg.prep (sr, 20.f); sMx.prep (sr, 20.f); dcx[0] = dcx[1] = dcy[0] = dcy[1] = 0.f; }
    static float shape (int t, float x)
    {
        switch (t) { case 0: return triFold (x); case 1: return std::sin (x * 1.5707963f * 1.f); case 2: { const float u = juce::jlimit (-1.f, 1.f, x); return .3f * u + .7f * (4.f * u * u * u - 3.f * u); }
                     default: return x * .3f + (std::abs (x) * 2.f - .6f) * .7f * (x > 3.f || x < -3.f ? .3f : 1.f); }
    }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || ! os.ready()) return; const int t = vi (pType); const float dr = db2lin (v (pDrive) * 24.f), bias = v (pBias) * .5f, tone = v (pTone), og = db2lin (v (pOut)), mix = v (pMix);
        if (tone != lastTone) { lo.set (BQ::LowShelf, sr, 300.0, .7, -tone * 6.0); hi.set (BQ::HighShelf, sr, 3000.0, .7, tone * 6.0); lastTone = tone; }
        const float cmp = .5f / std::max (.1f, std::abs (shape (t, .5f * dr + bias)));
        os.run (fc, [&] (int c, float x) { return x * sDr.next (c, dr) + bias; }, [&] (int, float x) { return shape (t, x); },
                [&] (int c, float y, float d) { y *= std::min (cmp, 4.f); const float o = y - dcx[c] + .9985f * dcy[c]; dcx[c] = y; dcy[c] = o; y = o; if (std::abs (tone) > .01f) { y = lo.run (c, y); y = hi.run (c, y); } return d + (y * sOg.next (c, og) - d) * sMx.next (c, mix); });
    }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override { const int t = vi (pType); const float dr = db2lin (v (pDrive) * 24.f) * .4f, b = v (pBias) * .5f; drawTransfer (g, r, [&] (float x) { return shape (t, x * dr + b) * .8f; }, 1.f); }
private:
    static inline const Spec S[] = { { "type", "Tipo", 0.f, 3.f, 0.f, 0.f, fChoice, "Pliegue|Seno|Chebyshev|Rectificador" }, { "drive", "Empuje", 0.f, 1.f, .4f, 0.f, fPct, "" }, { "bias", "Simetría", -1.f, 1.f, 0.f, 0.f, fPct, "" },
        { "tone", "Tono", -1.f, 1.f, 0.f, 0.f, fPct, "" }, { "out", "Salida", -24.f, 12.f, 0.f, 0.f, fDb, "" }, { "mix", "Mezcla", 0.f, 1.f, 1.f, 0.f, fPct, "" } };
    double sr = 44100.0; OS2 os; BQ lo, hi; float lastTone = -9.f, dcx[2] {}, dcy[2] {}; SmoothSt sDr, sOg, sMx;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjShaper)
};

//==============================================================================
// Drive para kick de gabber: cuatro caracteres, graves limpios por debajo del cruce y recorte pesado por arriba.
class InjKickDrive : public InjFx
{
public:
    enum { pChar, pDrive, pXo, pKeep, pTone, pLevel, pMix };
    INJ_FX_BODY (InjKickDrive, "injkickdrive", "Drive de kick gabber", "Kick", 7)
    Layout layout() const override { Layout L; L.pills = pChar; L.visualH = 100; L.knobs = { pDrive, pXo, pKeep, pTone, pLevel, pMix }; L.perRow = 6; L.dia = 50.f; L.icon = "M3 16c2-8 4-8 6 0s4 8 6 0 4-8 6 0"; return L; }
    double getLatencySeconds() override { return (double) os.lat / sr; }
    std::vector<Preset> presets() const override
    {
        return { { "Rotterdam pesado", { 0.f, .75f, 90.f, .9f, -.1f, -2.f, 1.f } }, { "Hardcore agresivo", { 1.f, .65f, 100.f, .8f, .1f, -2.f, 1.f } }, { "Uptempo redondo", { 2.f, .6f, 80.f, 1.f, 0.f, -1.f, 1.f } }, { "Chillido", { 3.f, .55f, 110.f, .8f, .3f, -3.f, .9f } } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; os.prep(); for (auto& b : bq) b.reset(); lastXo = -1.f; sDr.prep (sr, 20.f); sLv.prep (sr, 20.f); sMx.prep (sr, 20.f); lastTone = -9.f; dcx[0] = dcx[1] = dcy[0] = dcy[1] = 0.f; }
    static float shape (int t, float x)
    {
        switch (t) { case 0: return juce::jlimit (-1.f, 1.f, std::tanh (2.f * x) * 1.3f); case 1: { const float y = std::tanh (x + .25f) - std::tanh (.25f); return juce::jlimit (-1.f, 1.f, 1.6f * y); }
                     case 2: return std::tanh (2.5f * std::tanh (x)); default: return triFold (x * 1.4f); }
    }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || ! os.ready()) return; const int t = vi (pChar); const float dr = db2lin (v (pDrive) * 36.f), keep = v (pKeep), lv = db2lin (v (pLevel)), mix = v (pMix), tone = v (pTone), xo = v (pXo);
        if (xo != lastXo) { bq[0].set (BQ::HP, sr, xo, .7071); bq[1].set (BQ::HP, sr, xo, .7071); bq[2].set (BQ::LP, sr, xo, .7071); bq[3].set (BQ::LP, sr, xo, .7071); lastXo = xo; }
        if (tone != lastTone) { bq[4].set (BQ::LowShelf, sr, 300.0, .7, -tone * 6.0); bq[5].set (BQ::HighShelf, sr, 3000.0, .7, tone * 6.0); lastTone = tone; }
        os.run (fc, [&] (int c, float x) { return x * sDr.next (c, dr); }, [&] (int, float x) { return shape (t, x); },
                [&] (int c, float y, float d)
                {
                    const float o = y - dcx[c] + .9985f * dcy[c]; dcx[c] = y; dcy[c] = o; y = bq[1].run (c, bq[0].run (c, o)); const float low = bq[3].run (c, bq[2].run (c, d)) * keep;
                    float w = y * .9f + low; if (std::abs (tone) > .01f) { w = bq[4].run (c, w); w = bq[5].run (c, w); } return d + (w * sLv.next (c, lv) - d) * sMx.next (c, mix);
                });
    }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override { const int t = vi (pChar); const float dr = db2lin (v (pDrive) * 36.f) * .22f; drawTransfer (g, r, [&] (float x) { return shape (t, x * dr); }, 1.f); }
private:
    static inline const Spec S[] = { { "char", "Carácter", 0.f, 3.f, 0.f, 0.f, fChoice, "Rotterdam|Hardcore|Uptempo|Chillido" }, { "drive", "Empuje", 0.f, 1.f, .65f, 0.f, fPct, "" }, { "xo", "Cruce", 40.f, 300.f, 90.f, 100.f, fHz, "" },
        { "keep", "Sub limpio", 0.f, 1.f, .9f, 0.f, fPct, "" }, { "tone", "Tono", -1.f, 1.f, 0.f, 0.f, fPct, "" }, { "level", "Nivel", -12.f, 12.f, -2.f, 0.f, fDb, "" }, { "mix", "Mezcla", 0.f, 1.f, 1.f, 0.f, fPct, "" } };
    double sr = 44100.0; OS2 os; BQ bq[6]; float lastXo = -1.f, lastTone = -9.f, dcx[2] {}, dcy[2] {}; SmoothSt sDr, sLv, sMx;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjKickDrive)
};

//==============================================================================
// EQ de DJ: aislador de 3 bandas (cruces de 24 dB) que llega a apagar cada banda.
class InjEqDj : public InjFx
{
public:
    enum { pLow, pMid, pHigh, pX1, pX2 };
    INJ_FX_BODY (InjEqDj, "injeqdj", "EQ de DJ", "DJ", 5)
    Layout layout() const override { Layout L; L.visualH = 110; L.knobs = { pLow, pMid, pHigh, pX1, pX2 }; L.icon = "M6 4v16M12 4v16M18 4v16M4 14h4M10 8h4M16 12h4"; return L; }
    std::vector<Preset> presets() const override { return { { "Sin graves", { -60.f, 0.f, 0.f, 250.f, 3000.f } }, { "Solo medios", { -60.f, 0.f, -60.f, 250.f, 3000.f } }, { "Teléfono", { -60.f, 3.f, -60.f, 400.f, 2500.f } }, { "Más graves", { 4.f, 0.f, -2.f, 250.f, 3000.f } } }; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; for (auto& b : f) b.reset(); lx1 = lx2 = -1.f; for (auto& q : sg) q.prep (sr, 15.f); }
    float gainOf (int i) const { const float x = v (i); return x <= specs()[i].lo + .5f ? 0.f : db2lin (x); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return; const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : fc.destBuffer->getWritePointer (0, fc.bufferStartSample) };
        const float x1 = v (pX1), x2 = std::max (v (pX2), x1 * 1.5f);
        if (x1 != lx1 || x2 != lx2) { for (int k = 0; k < 2; ++k) { f[k].set (BQ::LP, sr, x1, .7071); f[2 + k].set (BQ::HP, sr, x1, .7071); f[4 + k].set (BQ::LP, sr, x2, .7071); f[6 + k].set (BQ::HP, sr, x2, .7071); f[8 + k].set (BQ::LP, sr, x2, .7071); f[10 + k].set (BQ::HP, sr, x2, .7071); } lx1 = x1; lx2 = x2; }
        const float gl = gainOf (pLow), gm = gainOf (pMid), gh = gainOf (pHigh);
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            float in[2] { p[0][i], p[1][i] }, out[2];
            for (int c = 0; c < 2; ++c)
            {
                const float lo = f[1].run (c, f[0].run (c, in[c])), rest = f[3].run (c, f[2].run (c, in[c])), mid = f[5].run (c, f[4].run (c, rest)), hi = f[7].run (c, f[6].run (c, rest));
                out[c] = (f[9].run (c, f[8].run (c, lo)) + f[11].run (c, f[10].run (c, lo))) * sg[0].next (c, gl) + mid * sg[1].next (c, gm) + hi * sg[2].next (c, gh);
            }
            p[0][i] = out[0]; if (nc > 1) p[1][i] = out[1];
        }
    }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        const float gs[3] { v (pLow), v (pMid), v (pHigh) }; const char* nm[3] { "Graves", "Medios", "Agudos" }; const float cw = r.getWidth() / 3.f;
        for (int b = 0; b < 3; ++b)
        {
            const auto z = juce::Rectangle<float> (r.getX() + b * cw + 14.f, r.getY() + 10.f, cw - 28.f, r.getHeight() - 36.f); const bool kill = gs[b] <= -59.5f; const auto col = theme::trackColours[b == 0 ? 0 : b == 1 ? 1 : 3];
            barV (g, z, kill ? 0.f : (gs[b] + 60.f) / 66.f, col.darker (.35f)); label (g, kill ? juce::String ("Apagado") : juce::String (gs[b], 1) + " dB", { z.getX(), z.getBottom() + 4.f, z.getWidth(), 14.f }, juce::Justification::centred, 11.f, true, kill ? theme::record : theme::text);
            label (g, nm[b], z.reduced (6.f, 6.f), juce::Justification::topLeft, 11.f, true, theme::text.withAlpha (kill ? .45f : 1.f));
        }
    }
private:
    static inline const Spec S[] = { { "low", "Graves", -60.f, 6.f, 0.f, 0.f, fKill, "" }, { "mid", "Medios", -60.f, 6.f, 0.f, 0.f, fKill, "" }, { "high", "Agudos", -60.f, 6.f, 0.f, 0.f, fKill, "" },
        { "x1", "Cruce grave|Cruce 1", 80.f, 800.f, 250.f, 300.f, fHz, "" }, { "x2", "Cruce agudo|Cruce 2", 1000.f, 8000.f, 3000.f, 3000.f, fHz, "" } };
    double sr = 44100.0; BQ f[12]; float lx1 = -1.f, lx2 = -1.f; SmoothSt sg[3];
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjEqDj)
};

//==============================================================================
// Auto-filtro: el corte sigue la fuerza de la señal (como un "wah") y/o un oscilador lento.
class InjAutoFilter : public InjFx
{
public:
    enum { pMode, pCut, pRes, pEnv, pLfo, pRate, pAtk, pRel, pMix };
    INJ_FX_BODY (InjAutoFilter, "injautofilter", "Auto-filtro", "AutoF", 9)
    Layout layout() const override { Layout L; L.pills = pMode; L.visualH = 60; L.rows = { { pCut, pRes, pEnv, pLfo }, { pRate, pAtk, pRel, pMix } }; L.dia = 52.f; L.icon = "M3 8h6c3 0 3 9 6 9h6"; return L; }
    std::vector<Preset> presets() const override { return { { "Wah automático", { 2.f, 500.f, .6f, .8f, 0.f, 1.f, 3.f, 120.f, 1.f } }, { "Barrido lento", { 0.f, 400.f, .5f, 0.f, .7f, .2f, 10.f, 200.f, 1.f } }, { "Pluck de bajo", { 0.f, 120.f, .55f, .9f, 0.f, 1.f, 1.f, 90.f, 1.f } }, { "Pulso rítmico", { 1.f, 800.f, .4f, 0.f, .6f, 4.f, 5.f, 80.f, 1.f } } }; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; env = 0.f; for (auto& a : ic1) for (auto& b : a) b = 0.f; for (auto& a : ic2) for (auto& b : a) b = 0.f; ph = 0.f; }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return; const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : nullptr };
        const int mode = vi (pMode); const float base = v (pCut), Q = .7071f * std::pow (30.f, v (pRes)), k = 1.f / Q, envAmt = v (pEnv), lfoAmt = v (pLfo), rate = v (pRate), mix = v (pMix); const float aA = coefMs (v (pAtk), sr), aR = coefMs (v (pRel), sr); float fcLast = base;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            float pk = std::abs (p[0][i]); if (nc > 1) pk = std::max (pk, std::abs (p[1][i])); env = pk > env ? aA * env + (1.f - aA) * pk : aR * env + (1.f - aR) * pk;
            ph += rate / (float) sr; if (ph > 1.f) ph -= 1.f; const float e01 = juce::jlimit (0.f, 1.f, (lin2db (env + 1e-6f) + 50.f) / 50.f), oct = 5.f * envAmt * e01 + 3.f * lfoAmt * std::sin (2.f * kPi * ph);
            fcLast = juce::jlimit (25.f, (float) sr * .45f, base * std::pow (2.f, oct)); const float g = std::tan (kPi * fcLast / (float) sr), a1 = 1.f / (1.f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            for (int c = 0; c < nc; ++c)
            {
                const float x = p[c][i], v3 = x - ic2[c][0], v1 = a1 * ic1[c][0] + a2 * v3, v2 = ic2[c][0] + a2 * ic1[c][0] + a3 * v3; ic1[c][0] = 2.f * v1 - ic1[c][0]; ic2[c][0] = 2.f * v2 - ic2[c][0];
                const float y = mode == 0 ? v2 : mode == 1 ? x - k * v1 - v2 : v1 * k; p[c][i] = x + (y - x) * mix;
            }
        }
        meterA.store (fcLast);
    }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        const float f = meterA.load() > 1.f ? meterA.load() : v (pCut), x = r.getX() + 16.f + std::log10 (std::max (20.f, f) / 20.f) / 3.f * (r.getWidth() - 32.f); const auto bar = r.reduced (16.f, 24.f); g.setColour (theme::bgBar); g.fillRoundedRectangle (bar, 2.f);
        g.setColour (theme::accent); g.fillRoundedRectangle (bar.withWidth (std::max (6.f, x - bar.getX())), 2.f); label (g, juce::String::fromUTF8 ("Corte actual  ") + fmtValue ({ "", "", 0, 0, 0, 0, fHz, "" }, f), r.reduced (24.f, 4.f), juce::Justification::topLeft, 11.f, true, theme::text);
    }
private:
    static inline const Spec S[] = { { "mode", "Tipo", 0.f, 2.f, 0.f, 0.f, fChoice, "Pasa bajos|Pasa altos|Pasa banda" }, { "cut", "Corte base|Corte", 30.f, 16000.f, 400.f, 1000.f, fHz, "" }, { "res", "Resonancia", 0.f, 1.f, .4f, 0.f, fPct, "" },
        { "env", "Envolvente", -1.f, 1.f, .6f, 0.f, fPct, "" }, { "lfo", "Oscilador|Osc.", 0.f, 1.f, 0.f, 0.f, fPct, "" }, { "rate", "Velocidad|Veloc.", .05f, 15.f, 1.f, 1.f, fRate, "" }, { "atk", "Ataque", 1.f, 100.f, 5.f, 10.f, fMs, "" },
        { "rel", "Liberación", 20.f, 800.f, 150.f, 200.f, fMs, "" }, { "mix", "Mezcla", 0.f, 1.f, 1.f, 0.f, fPct, "" } };
    double sr = 44100.0; float env = 0.f, ph = 0.f, ic1[2][1] {}, ic2[2][1] {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjAutoFilter)
};

//==============================================================================
// Filtro de formantes: tres resonancias de vocal (A, E, I, O, U) con paso suave entre ellas, desplazamiento y oscilador.
class InjFormant : public InjFx
{
public:
    enum { pVowel, pShift, pRes, pRate, pDepth, pMix, pOut };
    INJ_FX_BODY (InjFormant, "injformant", "Filtro de formantes", "Vocal", 7)
    Layout layout() const override { Layout L; L.visualH = 110; L.knobs = { pVowel, pShift, pRes, pRate, pDepth, pMix, pOut }; L.perRow = 7; L.dia = 46.f; L.icon = "M5 12c0-5 4-8 7-8s7 3 7 8-4 8-7 8"; return L; }
    std::vector<Preset> presets() const override { return { { "Vocal A", { 0.f, 0.f, .5f, 0.f, 0.f, 1.f, 0.f } }, { "Hablando (A-E-I-O-U)", { 0.f, 0.f, .5f, .6f, 1.f, 1.f, 0.f } }, { "Voz de robot", { 3.f, -.3f, .8f, 0.f, 0.f, 1.f, 0.f } }, { "Grito agudo", { 2.f, .5f, .7f, 0.f, 0.f, .8f, -2.f } } }; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; for (auto& b : bq) b.reset(); ph = 0.f; cnt = 0; }
    static void formants (float vowel, float shift, float out[3])
    {
        static const float T[5][3] { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 350, 1700, 2700 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
        const float vv = juce::jlimit (0.f, 4.f, vowel); const int a = (int) vv, b = std::min (4, a + 1); const float t = vv - (float) a, sh = std::pow (2.f, shift * .6f);
        for (int k = 0; k < 3; ++k) out[k] = (T[a][k] * (1.f - t) + T[b][k] * t) * sh;
    }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return; const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : nullptr };
        const float mix = v (pMix), og = db2lin (v (pOut)), rate = v (pRate), dep = v (pDepth), Q = 4.f + v (pRes) * 16.f; const float gains[3] { 1.4f, .8f, .5f };
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            if ((cnt++ & 31) == 0)
            {
                ph += rate * 32.f / (float) sr; if (ph > 1.f) ph -= 1.f; const float vow = v (pVowel) + (rate > .01f ? dep * 2.f * (.5f + .5f * std::sin (2.f * kPi * ph)) * 2.f - dep * 2.f : 0.f), shift = v (pShift);
                formants (juce::jlimit (0.f, 4.f, vow), shift, cur); for (int k = 0; k < 3; ++k) { bq[k].set (BQ::BP, sr, cur[k], Q); }
            }
            for (int c = 0; c < nc; ++c) { const float x = p[c][i]; float y = 0.f; for (int k = 0; k < 3; ++k) { const float b = bq[k].run (c, x); y += b * gains[k]; } p[c][i] = x + (y * 2.2f * og - x) * mix; }
        }
        meterA.store (cur[0]); meterB.store (cur[1]); meterC.store (cur[2]);
    }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        float fm[3]; formants (v (pVowel), v (pShift), fm); juce::Path p; const float Q = 4.f + v (pRes) * 16.f; const float gains[3] { 1.4f, .8f, .5f };
        for (int x = 0; x <= 568; x += 2)
        {
            const float f = 20.f * std::pow (10.f, 3.f * x / 568.f); float a = 0.f; for (int k = 0; k < 3; ++k) { BQ b; b.set (BQ::BP, 48000.0, fm[k], Q); a += (float) std::pow (10.0, b.magDb (f, 48000.0) / 20.0) * gains[k]; }
            const float y = r.getBottom() - 8.f - juce::jlimit (0.f, 1.f, a / 2.2f) * (r.getHeight() - 20.f); if (x == 0) p.startNewSubPath (r.getX(), y); else p.lineTo (r.getX() + x, y);
        }
        g.setColour (theme::accent); g.strokePath (p, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        label (g, juce::String::fromUTF8 ("Vocal  ") + fmtValue ({ "", "", 0, 0, 0, 0, fVowel, "" }, v (pVowel)), { r.getX() + 10.f, r.getY() + 4.f, 120.f, 16.f }, juce::Justification::left, 13.f, true, theme::text);
    }
private:
    static inline const Spec S[] = { { "vowel", "Vocal", 0.f, 4.f, 0.f, 0.f, fVowel, "" }, { "shift", "Desplazar", -1.f, 1.f, 0.f, 0.f, fPct, "" }, { "res", "Resonancia", 0.f, 1.f, .5f, 0.f, fPct, "" }, { "rate", "Velocidad|Veloc.", 0.f, 8.f, 0.f, 1.f, fRate, "" },
        { "depth", "Movimiento|Mov.", 0.f, 1.f, 0.f, 0.f, fPct, "" }, { "mix", "Mezcla", 0.f, 1.f, 1.f, 0.f, fPct, "" }, { "out", "Salida", -12.f, 12.f, 0.f, 0.f, fDb, "" } };
    double sr = 44100.0; BQ bq[3]; float ph = 0.f, cur[3] { 800.f, 1150.f, 2900.f }; int cnt = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjFormant)
};
} // namespace injfx
