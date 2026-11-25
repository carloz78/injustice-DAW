#pragma once
#include "FxCore.h"
// Tono y color: ecualizador de 4 bandas, filtro (SVF de 12/24 dB) y saturador con sobremuestreo.
namespace injfx
{
//==============================================================================
// Ecualizador de 4 bandas: estante bajo, dos campanas y estante alto. Se arrastran los puntos sobre la curva.
class InjEq : public InjFx
{
public:
    static inline const char* xmlTypeName = "injeq";
    static const char* getPluginName() { return "Ecualizador"; }
    InjEq (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "EQ"; }
    const Spec* specs() const override { return S; }
    enum { pHp = 12, pLp = 13 };
    int numSpecs() const override { return 14; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    std::vector<Preset> presets() const override
    {
        return { { "Limpiar graves", { 40.f, -6.f, .7f, 250.f, -2.f, 1.f, 2400.f, 0.f, 1.f, 9000.f, 0.f, .7f, 30.f, 20000.f } },
                 { "Brillo y aire", { 80.f, 0.f, .7f, 400.f, 0.f, 1.f, 3500.f, 2.f, 1.f, 10000.f, 4.5f, .7f, 20.f, 20000.f } },
                 { "Bombo con cuerpo", { 60.f, 4.f, .8f, 300.f, -3.f, 1.2f, 3000.f, 2.5f, 1.f, 9000.f, 0.f, .7f, 28.f, 20000.f } },
                 { "Voz presente", { 90.f, -4.f, .7f, 300.f, -1.5f, 1.f, 3200.f, 3.f, 1.f, 11000.f, 2.f, .7f, 80.f, 16000.f } },
                 { "Hueco para el bajo", { 100.f, 0.f, .7f, 200.f, -4.f, .8f, 2000.f, 0.f, 1.f, 9000.f, 0.f, .7f, 20.f, 20000.f } },
                 { "Solo para el club (sin sub)", { 80.f, 0.f, .7f, 320.f, 0.f, 1.f, 2400.f, 0.f, 1.f, 9000.f, 0.f, .7f, 35.f, 18000.f } } };
    }
    static BQ band (int b, float f, float g, float q, double sr) { BQ x; x.set (b == 0 ? BQ::LowShelf : b == 3 ? BQ::HighShelf : BQ::Peak, sr, f, q, g); return x; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; for (auto& b : bq) b.reset(); for (auto& b : cut) b.reset(); for (auto& l : last) l = -999.f; lastHp = lastLp = -1.f; for (auto& x : ring) x = 0.f; wp = 0; }
    // espectro de la señal de entrada (para la ventana)
    void spectrum (std::vector<float>& db) const { ringSpectrum (ring, 4095, wp.load(), db); }
    // corte de 24 dB: dos secciones de Butterworth
    static void setCut (BQ* b, bool high, float f, double sr) { b[0].set (high ? BQ::HP : BQ::LP, sr, f, .5412); b[1].set (high ? BQ::HP : BQ::LP, sr, f, 1.3066); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        { const float* a = fc.destBuffer->getReadPointer (0, fc.bufferStartSample); const float* b2 = nc > 1 ? fc.destBuffer->getReadPointer (1, fc.bufferStartSample) : a; int w = wp.load(); for (int i = 0; i < fc.bufferNumSamples; ++i) ring[(w++) & 4095] = .5f * (a[i] + b2[i]); wp.store (w); }
        const float hp = v (pHp), lp = v (pLp); if (hp != lastHp) { setCut (cut, true, hp, sr); lastHp = hp; } if (lp != lastLp) { setCut (cut + 4, false, lp, sr); lastLp = lp; }
        const bool hpOn = hp > 21.f, lpOn = lp < 19900.f;
        for (int b = 0; b < 4; ++b)
        {
            const float f = v (b * 3), g = v (b * 3 + 1), q = v (b * 3 + 2);
            if (f != last[b * 3] || g != last[b * 3 + 1] || q != last[b * 3 + 2]) { auto n = band (b, f, g, q, sr); bq[b].b0 = n.b0; bq[b].b1 = n.b1; bq[b].b2 = n.b2; bq[b].a1 = n.a1; bq[b].a2 = n.a2; last[b * 3] = f; last[b * 3 + 1] = g; last[b * 3 + 2] = q; }
            active[b] = std::abs (g) > .01f;
        }
        for (int c = 0; c < nc; ++c)
        {
            float* d = fc.destBuffer->getWritePointer (c, fc.bufferStartSample);
            for (int i = 0; i < fc.bufferNumSamples; ++i) { float x = d[i]; if (hpOn) x = cut[1].run (c, cut[0].run (c, x)); for (int b = 0; b < 4; ++b) if (active[b]) x = bq[b].run (c, x); if (lpOn) x = cut[5].run (c, cut[4].run (c, x)); d[i] = x; }
        }
    }
private:
    static inline const Spec S[] = {
        { "lf", "Graves Frec", 20.f, 800.f, 80.f, 120.f, fHz, "" }, { "lg", "Graves Gan", -18.f, 18.f, 0.f, 0.f, fDb, "" }, { "lq", "Graves Q", .3f, 4.f, .7f, 1.f, fQ, "" },
        { "m1f", "Medios bajos Frec", 60.f, 3000.f, 320.f, 400.f, fHz, "" }, { "m1g", "Medios bajos Gan", -18.f, 18.f, 0.f, 0.f, fDb, "" }, { "m1q", "Medios bajos Q", .3f, 8.f, 1.f, 1.f, fQ, "" },
        { "m2f", "Medios altos Frec", 300.f, 10000.f, 2400.f, 2000.f, fHz, "" }, { "m2g", "Medios altos Gan", -18.f, 18.f, 0.f, 0.f, fDb, "" }, { "m2q", "Medios altos Q", .3f, 8.f, 1.f, 1.f, fQ, "" },
        { "hf", "Agudos Frec", 1500.f, 20000.f, 9000.f, 7000.f, fHz, "" }, { "hg", "Agudos Gan", -18.f, 18.f, 0.f, 0.f, fDb, "" }, { "hq", "Agudos Q", .3f, 4.f, .7f, 1.f, fQ, "" },
        { "hp", "Corte de graves|Corte graves", 20.f, 500.f, 20.f, 100.f, fCutLo, "" }, { "lp", "Corte de agudos|Corte agudos", 1500.f, 20000.f, 20000.f, 8000.f, fCutHi, "" } };
    double sr = 44100.0; BQ bq[4], cut[6]; float last[12] {}, lastHp = -1.f, lastLp = -1.f; bool active[4] {}; float ring[4096] {}; std::atomic<int> wp { 0 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjEq)
};

class EqEditor : public FxEditor
{
public:
    EqEditor (InjEq& e) : FxEditor (e, 600, 500, "M5 4v16M12 4v16M19 4v16"), eq (e)
    {
        for (auto& q : spSm) q = -120.f;
        knob (InjEq::pHp, 16, 414, 134, 42.f); knob (InjEq::pLp, 16 + 3 * 142, 414, 134, 42.f);
        for (int b = 0; b < 4; ++b)
            for (int k = 0; k < 3; ++k) { auto* kn = knob (b * 3 + k, 16 + b * 142 + 4 + k * 42, 254 + 54, 42, 38.f); kn->labelOverride = k == 0 ? "Frec" : k == 1 ? "Gan" : "Q"; }
    }
protected:
    static juce::Colour colOf (int b) { return theme::trackColours[(b == 0 ? 0 : b == 1 ? 1 : b == 2 ? 2 : 3)]; }
    juce::Rectangle<float> plot() const { return { 16.f, 62.f, 568.f, 178.f }; }
    float fx_ (float f, float w) const { return std::log10 (f / 20.f) / 3.f * w; }
    float yg (float g, float h) const { return h * .5f - g / 18.f * (h * .5f - 8.f); }
    void paintBody (juce::Graphics& g) override
    {
        const auto r = plot(); plotBox (g, r); g.saveState(); juce::Path clip; clip.addRoundedRectangle (r, 3.f); g.reduceClipRegion (clip);
        g.setColour (theme::gridLine); for (int k = 1; k < 8; ++k) g.drawVerticalLine ((int) (r.getX() + r.getWidth() * k / 8.f), r.getY(), r.getBottom());
        for (float db : { -12.f, 12.f }) g.drawHorizontalLine ((int) (r.getY() + yg (db, r.getHeight())), r.getX(), r.getRight());
        g.setColour (theme::barLine); g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * .5f), r.getX(), r.getRight());
        BQ bs[4]; for (int b = 0; b < 4; ++b) bs[b] = InjEq::band (b, fx.v (b * 3), fx.v (b * 3 + 1), fx.v (b * 3 + 2), 48000.0);
        BQ hc[2], lc[2]; const float hpF = fx.v (InjEq::pHp), lpF = fx.v (InjEq::pLp); const bool hpOn = hpF > 21.f, lpOn = lpF < 19900.f; InjEq::setCut (hc, true, hpF, 48000.0); InjEq::setCut (lc, false, lpF, 48000.0);
        {   // espectro en vivo de la señal de entrada, detrás de la curva
            std::vector<float> sp; eq.spectrum (sp); const float binHz = 44100.f / 2048.f; juce::Path q; const int Wd = (int) r.getWidth();
            for (int x = 0; x <= Wd; x += 2)
            {
                const float f0 = 20.f * std::pow (1000.f, (float) x / (float) Wd), f1 = 20.f * std::pow (1000.f, (float) (x + 2) / (float) Wd); const int k0 = std::max (1, (int) (f0 / binHz)), k1 = std::min (1023, std::max (k0, (int) (f1 / binHz)));
                float m = -140.f; for (int k = k0; k <= k1; ++k) m = std::max (m, sp[(size_t) k]); m += 4.5f * std::log2 (f0 / 1000.f); float& sm = spSm[(size_t) std::min (x / 2, 599)]; sm = m > sm ? sm * .5f + m * .5f : sm * .88f + m * .12f;
                const float y = r.getBottom() - juce::jlimit (0.f, 1.f, (sm + 90.f) / 80.f) * r.getHeight() * .85f; if (x == 0) { q.startNewSubPath (r.getX(), r.getBottom()); q.lineTo (r.getX(), y); } else q.lineTo (r.getX() + x, y);
            }
            q.lineTo (r.getRight(), r.getBottom()); q.closeSubPath(); g.setColour (theme::textDim.withAlpha (.20f)); g.fillPath (q);
        }
        juce::Path p, fill; const int W = (int) r.getWidth();
        for (int x = 0; x <= W; x += 2)
        {
            const float f = 20.f * std::pow (10.f, 3.f * x / (float) W); double db = 0; for (int b = 0; b < 4; ++b) if (std::abs (fx.v (b * 3 + 1)) > .01f) db += bs[b].magDb (f, 48000.0); if (hpOn) db += hc[0].magDb (f, 48000.0) + hc[1].magDb (f, 48000.0); if (lpOn) db += lc[0].magDb (f, 48000.0) + lc[1].magDb (f, 48000.0);
            const float y = r.getY() + yg ((float) juce::jlimit (-24.0, 24.0, db), r.getHeight()); if (x == 0) { p.startNewSubPath (r.getX(), y); fill.startNewSubPath (r.getX(), y); } else { p.lineTo (r.getX() + x, y); fill.lineTo (r.getX() + x, y); }
        }
        fill.lineTo (r.getRight(), r.getBottom()); fill.lineTo (r.getX(), r.getBottom()); fill.closeSubPath();
        g.setColour (theme::accent.withAlpha (.12f)); g.fillPath (fill); g.setColour (theme::accent); g.strokePath (p, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.restoreState();
        g.setColour (theme::textDim); g.setFont (uiFont (11.f));
        const std::pair<float, const char*> fl[] { { 20.f, "20" }, { 100.f, "100" }, { 1000.f, "1k" }, { 10000.f, "10k" } };
        for (auto& q : fl) g.drawText (q.second, (int) (r.getX() + fx_ (q.first, r.getWidth())) + 3, (int) r.getBottom() - 16, 40, 12, juce::Justification::left);
        g.drawText ("20k", (int) r.getRight() - 40, (int) r.getBottom() - 16, 34, 12, juce::Justification::right);
        g.drawText ("+12", (int) r.getX() + 6, (int) (r.getY() + yg (12.f, r.getHeight())) - 14, 30, 12, juce::Justification::left); g.drawText ("0 dB", (int) r.getX() + 6, (int) (r.getY() + r.getHeight() * .5f) - 14, 40, 12, juce::Justification::left);
        g.drawText ("-12", (int) r.getX() + 6, (int) (r.getY() + yg (-12.f, r.getHeight())) + 2, 30, 12, juce::Justification::left);
        for (int b = 0; b < 4; ++b) { const auto c = dotPos (b); g.setColour (colOf (b)); g.fillEllipse (c.x - 7.f, c.y - 7.f, 14.f, 14.f); g.setColour (theme::boxFill); g.drawEllipse (c.x - 7.f, c.y - 7.f, 14.f, 14.f, 2.f); }
        const char* nm[] { "Graves", "Medios bajos", "Medios altos", "Agudos" }; const char* ty[] { "Estante bajo", "Campana", "Campana", "Estante alto" };
        for (int b = 0; b < 4; ++b)
        {
            const auto card = juce::Rectangle<float> (16.f + b * 142.f, 254.f, 134.f, 150.f); g.setColour (theme::panel); g.fillRoundedRectangle (card, 3.f); g.setColour (theme::seam); g.drawRoundedRectangle (card.reduced (.5f), 3.f, 1.f);
            g.setColour (colOf (b)); g.fillEllipse (card.getX() + 14.f, card.getY() + 12.f, 10.f, 10.f);
            g.setColour (theme::text); g.setFont (uiFont (13.f, true)); g.drawText (juce::String::fromUTF8 (nm[b]), (int) card.getX() + 30, (int) card.getY() + 6, 100, 17, juce::Justification::left);
            g.setColour (theme::textDim); g.setFont (uiFont (11.f)); g.drawText (ty[b], (int) card.getX() + 30, (int) card.getY() + 22, 100, 14, juce::Justification::left);
        }
    }
    juce::Point<float> dotPos (int b) const { const auto r = plot(); return { r.getX() + fx_ (fx.v (b * 3), r.getWidth()), r.getY() + yg (fx.v (b * 3 + 1), r.getHeight()) }; }
    void mouseDown (const juce::MouseEvent& e) override
    {
        drag = -1; float best = 14.f; for (int b = 0; b < 4; ++b) { const float d = dotPos (b).getDistanceFrom (e.position); if (d < best) { best = d; drag = b; } }
        if (drag >= 0) { fx.param (drag * 3)->parameterChangeGestureBegin(); fx.param (drag * 3 + 1)->parameterChangeGestureBegin(); }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag < 0) return; const auto r = plot();
        const float f = 20.f * std::pow (10.f, 3.f * juce::jlimit (0.f, 1.f, (e.position.x - r.getX()) / r.getWidth())), g = (r.getY() + r.getHeight() * .5f - e.position.y) / (r.getHeight() * .5f - 8.f) * 18.f;
        fx.setValue (drag * 3, f); fx.setValue (drag * 3 + 1, g);
    }
    void mouseUp (const juce::MouseEvent&) override { if (drag >= 0) { fx.param (drag * 3)->parameterChangeGestureEnd(); fx.param (drag * 3 + 1)->parameterChangeGestureEnd(); } drag = -1; }
    void mouseDoubleClick (const juce::MouseEvent& e) override { for (int b = 0; b < 4; ++b) if (dotPos (b).getDistanceFrom (e.position) < 14.f) fx.setValue (b * 3 + 1, 0.f); }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        int hit = -1; float best = 16.f; for (int b = 0; b < 4; ++b) { const float d = dotPos (b).getDistanceFrom (e.position); if (d < best) { best = d; hit = b; } }
        if (hit >= 0) fx.setValue (hit * 3 + 2, fx.v (hit * 3 + 2) * (1.f + w.deltaY * .4f));
    }
private:
    InjEq& eq; int drag = -1; float spSm[600] {};
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjEq::createEditor() { return std::make_unique<EqEditor> (*this); }

//==============================================================================
// Filtro: SVF de integración trapezoidal (estable al automatizar el corte), pendiente de 12 o 24 dB.
class InjFilter : public InjFx
{
public:
    enum { pType, pCut, pRes, pSlope, pMix, pDrive };
    static inline const char* xmlTypeName = "injfilter";
    static const char* getPluginName() { return "Filtro"; }
    InjFilter (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Filtro"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 6; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    std::vector<Preset> presets() const override
    {
        return { { "Barrido de graves", { 0.f, 500.f, .3f, 1.f, 1.f, 0.f } }, { "Quitar graves", { 1.f, 120.f, .1f, 1.f, 1.f, 0.f } }, { "Teléfono", { 2.f, 1500.f, .35f, 0.f, 1.f, .3f } },
                 { "Resonante agresivo", { 0.f, 900.f, .8f, 1.f, 1.f, .5f } }, { "Rechazo de zumbido", { 3.f, 1000.f, .7f, 0.f, 1.f, 0.f } }, { "Acid saturado", { 0.f, 700.f, .85f, 1.f, 1.f, .8f } } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; sm.prep (sr, 12.f); sm.snap (std::log (v (pCut))); for (auto& a : ic1) for (auto& b : a) b = 0.f; for (auto& a : ic2) for (auto& b : a) b = 0.f; }
    static float qOf (float res) { return .7071f * std::pow (30.f, res); }
    // magnitud analógica (dB) para dibujar la curva
    static float magDb (int type, int slope, float res, float f, float fc)
    {
        const float q = qOf (res), q1 = slope ? .5412f : q, q2 = 1.3066f * q / .7071f; const std::complex<float> s (0.f, f / fc);
        auto h = [&] (float Q) { const auto d = s * s + s / Q + 1.f; switch (type) { case 0: return 1.f / d; case 1: return s * s / d; case 2: return (s / Q) / d; default: return (s * s + 1.f) / d; } };
        std::complex<float> r = h (q1); if (slope) r *= h (q2);
        return 20.f * std::log10 (std::max (1e-6f, std::abs (r)));
    }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : nullptr };
        const int type = vi (pType), slope = vi (pSlope); const float tgt = std::log (v (pCut)), res = v (pRes), mix = v (pMix), Q = qOf (res), qs = Q / .7071f, drv = v (pDrive), dg = 1.f + drv * 7.f;
        const float qa = slope ? .5412f : Q, qb = 1.3066f * qs; const int stages = slope ? 2 : 1;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            const float fcut = juce::jlimit (20.f, (float) sr * .45f, std::exp (sm.next (tgt))), g = std::tan (kPi * fcut / (float) sr);
            for (int c = 0; c < nc; ++c)
            {
                const float dry = p[c][i]; float x = drv > .001f ? std::tanh (dg * dry) / dg : dry;
                for (int s = 0; s < stages; ++s)
                {
                    const float k = 1.f / (s == 0 ? qa : qb), a1 = 1.f / (1.f + g * (g + k)), a2 = g * a1, a3 = g * a2;
                    const float v3 = x - ic2[c][s], v1 = a1 * ic1[c][s] + a2 * v3, v2 = ic2[c][s] + a2 * ic1[c][s] + a3 * v3; ic1[c][s] = 2.f * v1 - ic1[c][s]; ic2[c][s] = 2.f * v2 - ic2[c][s];
                    x = type == 0 ? v2 : type == 1 ? x - k * v1 - v2 : type == 2 ? v1 * k * (s == 0 ? 1.f : 1.f) : x - k * v1;
                }
                p[c][i] = dry + (x - dry) * mix;
            }
        }
    }
private:
    static inline const Spec S[] = {
        { "type", "Tipo", 0.f, 3.f, 0.f, 0.f, fChoice, "Pasa bajos|Pasa altos|Pasa banda|Rechazo" }, { "cut", "Corte", 20.f, 20000.f, 2000.f, 1000.f, fHz, "" },
        { "res", "Resonancia", 0.f, 1.f, .25f, 0.f, fPct, "" }, { "slope", "Pendiente", 0.f, 1.f, 0.f, 0.f, fChoice, "12 dB|24 dB" }, { "mix", "Mezcla", 0.f, 1.f, 1.f, 0.f, fPct, "" },
        { "drive", "Saturación", 0.f, 1.f, 0.f, 0.f, fPct, "" } };
    double sr = 44100.0; Smooth sm; float ic1[2][2] {}, ic2[2][2] {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjFilter)
};

class FilterEditor : public FxEditor
{
public:
    FilterEditor (InjFilter& f) : FxEditor (f, 600, 340, "M3 8h8c4 0 4 9 10 9")
    {
        pills (InjFilter::pType, { 16, 64, 568, 34 }); knobRow ({ InjFilter::pCut, InjFilter::pRes, InjFilter::pDrive, InjFilter::pSlope, InjFilter::pMix }, 238);
    }
protected:
    void paintBody (juce::Graphics& g) override
    {
        const auto r = juce::Rectangle<float> (16.f, 110.f, 568.f, 116.f); plotBox (g, r); g.saveState(); juce::Path clip; clip.addRoundedRectangle (r, 3.f); g.reduceClipRegion (clip);
        g.setColour (theme::gridLine); for (int k = 1; k < 8; ++k) g.drawVerticalLine ((int) (r.getX() + r.getWidth() * k / 8.f), r.getY(), r.getBottom());
        for (int k = 1; k < 4; ++k) g.drawHorizontalLine ((int) (r.getY() + k * r.getHeight() / 4.f), r.getX(), r.getRight());
        const int type = fx.vi (InjFilter::pType), slope = fx.vi (InjFilter::pSlope); const float res = fx.v (InjFilter::pRes), fc = fx.v (InjFilter::pCut);
        auto Y = [&] (float db) { return r.getY() + 30.f - db * 2.2f; }; juce::Path p, fill;
        for (int x = 0; x <= (int) r.getWidth(); x += 2)
        {
            const float f = 20.f * std::pow (10.f, 3.f * x / r.getWidth()), y = juce::jlimit (r.getY() + 2.f, r.getBottom() - 1.f, Y (InjFilter::magDb (type, slope, res, f, fc)));
            if (x == 0) { p.startNewSubPath (r.getX(), y); fill.startNewSubPath (r.getX(), y); } else { p.lineTo (r.getX() + x, y); fill.lineTo (r.getX() + x, y); }
        }
        fill.lineTo (r.getRight(), r.getBottom()); fill.lineTo (r.getX(), r.getBottom()); fill.closeSubPath();
        g.setColour (theme::accent.withAlpha (.12f)); g.fillPath (fill); g.setColour (theme::accent); g.strokePath (p, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float cx = r.getX() + std::log10 (fc / 20.f) / 3.f * r.getWidth(); g.fillEllipse (cx - 7.f, Y (InjFilter::magDb (type, slope, res, fc, fc)) - 7.f, 14.f, 14.f);
        g.restoreState(); g.setColour (theme::textDim); g.setFont (uiFont (11.f));
        const std::pair<float, const char*> fl[] { { 20.f, "20" }, { 100.f, "100" }, { 1000.f, "1k" }, { 10000.f, "10k" } };
        for (auto& q : fl) g.drawText (q.second, (int) (r.getX() + std::log10 (q.first / 20.f) / 3.f * r.getWidth()) + 3, (int) r.getBottom() - 14, 40, 12, juce::Justification::left);
        g.drawText ("20k", (int) r.getRight() - 40, (int) r.getBottom() - 14, 34, 12, juce::Justification::right);
    }
    void mouseDown (const juce::MouseEvent& e) override { dragging = e.y > 110 && e.y < 226 && e.x > 16 && e.x < 584; if (dragging) { fx.param (InjFilter::pCut)->parameterChangeGestureBegin(); mouseDrag (e); } }
    void mouseDrag (const juce::MouseEvent& e) override { if (dragging) fx.setValue (InjFilter::pCut, 20.f * std::pow (10.f, 3.f * juce::jlimit (0.f, 1.f, ((float) e.x - 16.f) / 568.f))); }
    void mouseUp (const juce::MouseEvent&) override { if (dragging) fx.param (InjFilter::pCut)->parameterChangeGestureEnd(); dragging = false; }
private:
    bool dragging = false;
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjFilter::createEditor() { return std::make_unique<FilterEditor> (*this); }

//==============================================================================
// Saturador: cinco curvas con sobremuestreo x2 (sin aliasing), compensación de nivel, tono y mezcla.
class InjSat : public InjFx
{
public:
    enum { pType, pAmt, pTone, pOut, pMix };
    static inline const char* xmlTypeName = "injsat";
    static const char* getPluginName() { return "Saturador"; }
    InjSat (te::PluginCreationInfo i) : InjFx (i), os (2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true)
    {
        buildParams(); os.initProcessing (kMax); latSamples = os.getLatencyInSamples();
    }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Sat"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 5; }
    double getLatencySeconds() override { return (double) latSamples / sr; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    std::vector<Preset> presets() const override
    {
        return { { "Cinta cálida", { 2.f, .42f, -.2f, 0.f, .7f } }, { "Calentar suave", { 0.f, .25f, 0.f, 0.f, .6f } }, { "Bombo gabber duro", { 1.f, .75f, -.1f, -2.f, 1.f } },
                 { "Fuzz sucio", { 3.f, .7f, .2f, -3.f, 1.f } }, { "Clipper de master", { 4.f, .3f, 0.f, 0.f, 1.f } } };
    }
    // las cinco curvas (también las dibuja la ventana)
    static float shape (int t, float x)
    {
        switch (t)
        {
            case 0: return std::tanh (x);
            case 1: { const float a = std::abs (x), w = .4f; const float y = a < 1.f - w ? a : a < 1.f + w ? a - (a - (1.f - w)) * (a - (1.f - w)) / (4.f * w) : 1.f; return x < 0 ? -y : y; }
            case 2: return std::tanh (x + .18f) - std::tanh (.18f);
            case 3: return x >= 0.f ? 1.f - std::exp (-2.f * x) : -.8f * (1.f - std::exp (1.2f * x));
            default: return juce::jlimit (-1.f, 1.f, x);
        }
    }
    void initialise (const te::PluginInitialisationInfo& i) override
    {
        sr = i.sampleRate; os.reset(); latSamples = os.getLatencyInSamples(); latInt = (int) std::lround (latSamples);
        for (auto& d : dry) { d.prep (latInt + kMax + 8); d.clear(); } sc.setSize (2, kMax); for (auto& b : lo) b.reset (); for (auto& b : hi) b.reset(); dcx[0] = dcx[1] = dcy[0] = dcy[1] = 0.f; lastTone = -9.f;
    }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || sc.getNumSamples() < kMax) return;
        const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        const int type = vi (pType); const float amt = v (pAmt), tone = v (pTone), outG = db2lin (v (pOut)), mix = v (pMix), drive = db2lin (amt * 30.f);
        const float cmp = .5f / std::max (.05f, std::abs (shape (type, .5f * drive)));
        if (tone != lastTone) { lo[0].set (BQ::LowShelf, sr, 300.0, .7, -tone * 6.0); hi[0].set (BQ::HighShelf, sr, 3000.0, .7, tone * 6.0); lastTone = tone; for (int c = 1; c < 2; ++c) {} }
        float pk = 0.f;
        forChunks (fc.bufferNumSamples, kMax, [&] (int s0, int n)
        {
            float* io[2]; for (int c = 0; c < 2; ++c) io[c] = fc.destBuffer->getWritePointer (juce::jmin (c, nc - 1), fc.bufferStartSample + s0);
            for (int c = 0; c < 2; ++c) { float* w = sc.getWritePointer (c); for (int i = 0; i < n; ++i) { dry[c].push (io[c][i]); w[i] = io[c][i] * drive; } }
            juce::dsp::AudioBlock<float> blk (sc); auto sub = blk.getSubBlock (0, (size_t) n); auto up = os.processSamplesUp (sub);
            for (size_t c = 0; c < 2; ++c) { float* u = up.getChannelPointer (c); for (size_t i = 0; i < up.getNumSamples(); ++i) u[i] = shape (type, u[i]); }
            os.processSamplesDown (sub);
            for (int c = 0; c < 2; ++c)
            {
                float* w = sc.getWritePointer (c);
                for (int i = 0; i < n; ++i)
                {
                    float y = w[i] * cmp; const float yo = y - dcx[c] + .9985f * dcy[c]; dcx[c] = y; dcy[c] = yo; y = yo;   // quita la continua que dejan las curvas asimétricas
                    if (std::abs (tone) > .01f) { y = lo[0].run (c, y); y = hi[0].run (c, y); }
                    const float d = dry[c].at (latInt + n - 1 - i); const float o = (d + (y * outG - d) * mix); pk = std::max (pk, std::abs (o));
                    if (c < nc) io[c][i] = o;
                }
            }
        });
        meterA.store (pk);
    }
private:
    static constexpr int kMax = 2048;
    static inline const Spec S[] = {
        { "type", "Tipo", 0.f, 4.f, 0.f, 0.f, fChoice, "Suave|Duro|Cinta|Fuzz|Clipper" }, { "amt", "Cantidad", 0.f, 1.f, .42f, 0.f, fPct, "" }, { "tone", "Tono", -1.f, 1.f, 0.f, 0.f, fPct, "" },
        { "out", "Salida", -24.f, 12.f, 0.f, 0.f, fDb, "" }, { "mix", "Mezcla", 0.f, 1.f, 1.f, 0.f, fPct, "" } };
    juce::dsp::Oversampling<float> os; double sr = 44100.0; float latSamples = 0.f; int latInt = 0; Delay dry[2]; juce::AudioBuffer<float> sc;
    BQ lo[1], hi[1]; float dcx[2] {}, dcy[2] {}, lastTone = -9.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjSat)
};

class SatEditor : public FxEditor
{
public:
    SatEditor (InjSat& s) : FxEditor (s, 600, 340, "M3 17c4 0 4-10 9-10s5 10 9 10")
    {
        pills (InjSat::pType, { 16, 64, 568, 34 });
        const int ks[4] { InjSat::pAmt, InjSat::pTone, InjSat::pOut, InjSat::pMix };
        for (int k = 0; k < 4; ++k) knob (ks[k], 232 + (k % 2) * 176, 114 + (k / 2) * 108, 176, 62.f);
    }
protected:
    void paintBody (juce::Graphics& g) override
    {
        const auto r = juce::Rectangle<float> (16.f, 112.f, 200.f, 200.f); plotBox (g, r); g.saveState(); juce::Path clip; clip.addRoundedRectangle (r, 3.f); g.reduceClipRegion (clip);
        g.setColour (theme::gridLine); for (int k = 1; k < 4; ++k) { g.drawVerticalLine ((int) (r.getX() + k * 50), r.getY(), r.getBottom()); g.drawHorizontalLine ((int) (r.getY() + k * 50), r.getX(), r.getRight()); }
        g.setColour (theme::barLine); g.drawLine (r.getX(), r.getBottom(), r.getRight(), r.getY(), 1.f);
        const int type = fx.vi (InjSat::pType); const float drive = db2lin (fx.v (InjSat::pAmt) * 30.f) * .35f; juce::Path p;
        for (int k = 0; k <= 100; ++k) { const float x = -1.f + k * .02f, y = juce::jlimit (-1.f, 1.f, InjSat::shape (type, x * drive)); const float px = r.getCentreX() + x * r.getWidth() * .5f, py = r.getCentreY() - y * r.getHeight() * .5f; if (k == 0) p.startNewSubPath (px, py); else p.lineTo (px, py); }
        g.setColour (theme::accent); g.strokePath (p, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.restoreState(); g.setColour (theme::textDim); g.setFont (uiFont (11.f)); g.drawText ("Salida", (int) r.getX() + 8, (int) r.getY() + 4, 60, 14, juce::Justification::left); g.drawText ("Entrada", (int) r.getRight() - 68, (int) r.getBottom() - 18, 60, 14, juce::Justification::right);
    }
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjSat::createEditor() { return std::make_unique<SatEditor> (*this); }
} // namespace injfx
