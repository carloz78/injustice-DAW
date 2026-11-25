#pragma once
#include "FxCore.h"
// Dinámica: compresor y limitador (con anticipación) de INJUSTICE DAW.
namespace injfx
{
//==============================================================================
// Compresor: detector de picos enlazado, rodilla suave de 6 dB, ataque/liberación sobre la reducción en dB, mezcla en paralelo.
class InjComp : public InjFx
{
public:
    enum { pThr, pRatio, pAtk, pRel, pGain, pMix, pKnee, pHp };
    static inline const char* xmlTypeName = "injcomp";
    static const char* getPluginName() { return "Compresor"; }
    InjComp (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Comp"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 8; }
    std::vector<Preset> presets() const override
    {
        return { { "Bombo pegado", { -20.f, 6.f, 25.f, 70.f, 4.f, 1.f, 3.f, 90.f } }, { "Voz firme", { -22.f, 3.f, 8.f, 120.f, 4.f, 1.f, 8.f, 100.f } }, { "Bus de batería", { -16.f, 2.5f, 30.f, 150.f, 2.f, 1.f, 6.f, 80.f } },
                 { "Paralela agresiva", { -34.f, 10.f, 2.f, 80.f, 8.f, .4f, 2.f, 150.f } }, { "Suave y transparente", { -14.f, 1.8f, 20.f, 200.f, 1.f, 1.f, 12.f, 20.f } } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; gr = 0.f; for (auto& b : hpf) b.reset(); lastHp = -1.f; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    // curva estática (entrada dB -> salida dB) con rodilla de ancho W dB; la usa también la ventana
    static float curve (float x, float T, float R, float W = 6.f)
    {
        const float o = x - T; if (W < .1f) return o <= 0.f ? x : T + o / R;
        if (2.f * o < -W) return x; if (2.f * std::abs (o) <= W) return x + (1.f / R - 1.f) * (o + W * .5f) * (o + W * .5f) / (2.f * W); return T + o / R;
    }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p0 = fc.destBuffer->getWritePointer (0, fc.bufferStartSample); float* p1 = nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : p0;
        const float T = v (pThr), R = v (pRatio), mkDb = v (pGain), mix = v (pMix), knee = v (pKnee), hp = v (pHp); const bool hpOn = hp > 21.f; if (hp != lastHp) { hpf[0].set (BQ::HP, sr, hp, .7071); hpf[1].set (BQ::HP, sr, hp, .7071); lastHp = hp; }
        const float aA = std::exp (-1.f / (std::max (.05f, v (pAtk)) * .001f * (float) sr)), aR = std::exp (-1.f / (std::max (5.f, v (pRel)) * .001f * (float) sr));
        float peakIn = 0.f, maxGr = 0.f;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            const float l = p0[i], r = p1[i]; const float dl = hpOn ? hpf[0].run (0, l) : l, dr = hpOn ? hpf[1].run (0, r) : r, pk = std::max (std::abs (dl), std::abs (dr)); peakIn = std::max (peakIn, std::max (std::abs (l), std::abs (r)));
            const float xdb = lin2db (pk), target = std::max (0.f, xdb - curve (xdb, T, R, knee));
            const float c = target > gr ? aA : aR; gr = c * gr + (1.f - c) * target; maxGr = std::max (maxGr, gr);
            const float g = std::exp ((mkDb - gr) * .11512925f), gm = 1.f - mix + mix * g;
            p0[i] = l * gm; p1[i] = r * gm;
        }
        meterA.store (maxGr); meterB.store (lin2db (peakIn));
    }
private:
    static inline const Spec S[] = {
        { "thr", "Umbral", -60.f, 0.f, -18.f, 0.f, fDb, "" }, { "ratio", "Ratio", 1.f, 20.f, 4.f, 4.f, fRatio, "" },
        { "atk", "Ataque", .1f, 100.f, 12.f, 10.f, fMs, "" }, { "rel", "Liberación", 10.f, 1000.f, 90.f, 120.f, fMs, "" },
        { "gain", "Ganancia", 0.f, 24.f, 3.f, 0.f, fDb, "" }, { "mix", "Mezcla", 0.f, 1.f, 1.f, 0.f, fPct, "" },
        { "knee", "Codo", 0.f, 12.f, 6.f, 0.f, fDb, "" }, { "hp", "Filtro del detector|Filtro det.", 20.f, 300.f, 20.f, 100.f, fCutLo, "" } };
    double sr = 44100.0; float gr = 0.f, lastHp = -1.f; BQ hpf[2];
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjComp)
};

class CompEditor : public FxEditor
{
public:
    CompEditor (InjComp& c) : FxEditor (c, 600, 340, "M4 7l6 3-6 3M20 7l-6 3 6 3M12 4v16"), comp (c)
    {
        for (int k = 0; k < 8; ++k) knob (k, 248 + (k % 4) * 84, 64 + (k / 4) * 96, 84, 48.f);
    }
protected:
    void tick() override { const float a = comp.meterA.load(); gr = std::max (a, gr * .92f); inDb = comp.meterB.load(); }
    void paintBody (juce::Graphics& g) override
    {
        const auto box = juce::Rectangle<float> (16.f, 64.f, 220.f, 190.f); plotBox (g, box);
        auto X = [&] (float db) { return box.getX() + (db + 60.f) / 60.f * box.getWidth(); }; auto Y = [&] (float db) { return box.getBottom() - (db + 60.f) / 60.f * box.getHeight(); };
        g.setColour (theme::gridLine); for (int k = 1; k < 5; ++k) { g.drawVerticalLine ((int) (box.getX() + k * 44), box.getY(), box.getBottom()); g.drawHorizontalLine ((int) (box.getY() + k * 38), box.getX(), box.getRight()); }
        g.setColour (theme::barLine); g.drawLine (box.getX(), box.getBottom(), box.getRight(), box.getY(), 1.f);
        const float T = fx.v (InjComp::pThr), R = fx.v (InjComp::pRatio), KW = fx.v (InjComp::pKnee);
        if (inDb > -60.f) { g.setColour (theme::accent.withAlpha (.25f)); g.drawLine (X (inDb), box.getBottom(), X (inDb), box.getY(), 1.f); }
        juce::Path p; for (int k = 0; k <= 110; ++k) { const float x = -60.f + k * 60.f / 110.f, y = InjComp::curve (x, T, R, KW); if (k == 0) p.startNewSubPath (X (x), Y (y)); else p.lineTo (X (x), Y (y)); }
        g.setColour (theme::accent); g.strokePath (p, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.fillEllipse (X (T) - 6.f, Y (T) - 6.f, 12.f, 12.f); g.setColour (theme::boxFill); g.drawEllipse (X (T) - 6.f, Y (T) - 6.f, 12.f, 12.f, 2.f);
        g.setColour (theme::textDim); g.setFont (uiFont (11.f)); g.drawText ("Salida", (int) box.getX() + 8, (int) box.getY() + 4, 60, 14, juce::Justification::left);
        g.drawText ("Entrada", (int) box.getRight() - 68, (int) box.getBottom() - 18, 60, 14, juce::Justification::right); g.drawText ("Umbral", (int) X (T) + 8, (int) Y (T) + 6, 60, 14, juce::Justification::left);
        // reducción de ganancia
        const float by = 270.f; g.setFont (uiFont (12.f)); g.setColour (theme::textDim); g.drawText (juce::String::fromUTF8 ("Reducción de ganancia"), 16, (int) by, 300, 16, juce::Justification::left);
        g.setColour (theme::text); g.setFont (uiFont (12.f, true)); g.drawText ((gr < .05f ? juce::String ("0.0") : "-" + juce::String (gr, 1)) + " dB", getWidth() - 16 - 120, (int) by, 120, 16, juce::Justification::right);
        const auto bar = juce::Rectangle<float> (16.f, by + 22.f, (float) getWidth() - 32.f, 14.f); g.setColour (theme::boxFill); g.fillRoundedRectangle (bar, 2.f);
        const float w = juce::jlimit (0.f, 1.f, gr / 24.f) * bar.getWidth(); if (w > 1.f) { g.setColour (theme::accent); g.fillRoundedRectangle (bar.withLeft (bar.getRight() - std::max (w, 14.f)), 2.f); }
        g.setColour (theme::textDim); g.setFont (uiFont (11.f)); const char* lb[] { "0", "-6", "-12", "-18", "-24" };
        for (int k = 0; k < 5; ++k) g.drawText (lb[k], (int) (bar.getRight() - k * bar.getWidth() / 4.f) - 20 + (k == 0 ? -10 : (k == 4 ? 10 : 0)), (int) bar.getBottom() + 3, 40, 14, juce::Justification::centred);
    }
private:
    InjComp& comp; float gr = 0.f, inDb = -90.f;
};

inline std::unique_ptr<te::Plugin::EditorComponent> InjComp::createEditor() { return std::make_unique<CompEditor> (*this); }

//==============================================================================
// Limitador con anticipación de 2 ms: la señal no pasa del techo. Tres modos: suave, rápido (liberación corta) y pico real (detecta entre muestras).
class InjLimit : public InjFx
{
public:
    enum { pGain, pCeil, pRel, pMode };
    static inline const char* xmlTypeName = "injlimit";
    static const char* getPluginName() { return "Limitador"; }
    InjLimit (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Lim"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 4; }
    double getLatencySeconds() override { return .002; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    std::vector<Preset> presets() const override
    {
        return { { "Master fuerte", { 6.f, -.3f, 60.f, 2.f } }, { "Master suave", { 3.f, -.5f, 120.f, 0.f } }, { "Bombo gabber", { 9.f, -.2f, 30.f, 1.f } }, { "Seguro para streaming", { 0.f, -1.f, 80.f, 2.f } } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override
    {
        sr = i.sampleRate; L = std::max (8, juce::roundToInt (.002 * sr)); Lw = L + 1;
        for (auto& d : dl) { d.prep (L + 8); d.clear(); }
        cap = 1; while (cap < Lw + 2) cap <<= 1; mv.assign ((size_t) cap, 1.f); mi.assign ((size_t) cap, 0); head = tail = 0; n = 0;
        bc.assign ((size_t) Lw, 1.f); bpos = 0; sum = (double) Lw; env = 1.f; for (auto& h : hist) for (auto& x : h) x = 0.f;
    }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || mv.empty()) return;
        const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p0 = fc.destBuffer->getWritePointer (0, fc.bufferStartSample); float* p1 = nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : p0;
        const float gin = db2lin (v (pGain)), ceil = db2lin (v (pCeil)); const int mode = vi (pMode);
        const float relMs = mode == 1 ? std::max (5.f, v (pRel) * .3f) : v (pRel), relC = 1.f - std::exp (-1.f / (relMs * .001f * (float) sr));
        float pkIn = 0.f, pkOut = 0.f, minG = 1.f;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            const float a = p0[i] * gin, b = p1[i] * gin; pkIn = std::max (pkIn, std::max (std::abs (a), std::abs (b)));
            dl[0].push (a); dl[1].push (b);
            float pk = std::max (std::abs (a), std::abs (b));
            if (mode == 2)
            {   // pico real: interpola entre las dos muestras anteriores (cuatro puntos) y toma el máximo
                const float in2[2] { a, b };
                for (int c = 0; c < 2; ++c)
                {
                    float* h = hist[c]; h[0] = h[1]; h[1] = h[2]; h[2] = h[3]; h[3] = in2[c];
                    for (int k = 1; k < 4; ++k) { const float f = k * .25f, c1 = .5f * (h[2] - h[0]), c2 = h[0] - 2.5f * h[1] + 2.f * h[2] - .5f * h[3], c3 = .5f * (h[3] - h[0]) + 1.5f * (h[1] - h[2]); pk = std::max (pk, std::abs (((c3 * f + c2) * f + c1) * f + h[1])); }
                }
            }
            const float req = pk > ceil ? ceil / pk : 1.f;
            // mínimo móvil de ventana Lw (cola monótona)
            while (tail != head && mv[(size_t) ((tail - 1) & (cap - 1))] >= req) tail = (tail - 1) & (cap - 1);
            mv[(size_t) tail] = req; mi[(size_t) tail] = n; tail = (tail + 1) & (cap - 1);
            while (mi[(size_t) head] <= n - Lw) head = (head + 1) & (cap - 1);
            const float m = mv[(size_t) head]; ++n;
            // promedio móvil de la ganancia mínima: rampa de ataque suave que llega justo cuando sale el pico
            sum += (double) m - (double) bc[(size_t) bpos]; bc[(size_t) bpos] = m; if (++bpos >= Lw) bpos = 0;
            const float avg = (float) (sum / Lw);
            env = avg < env ? avg : env + (avg - env) * relC; minG = std::min (minG, env);
            const float o0 = juce::jlimit (-ceil, ceil, dl[0].at (L) * env), o1 = juce::jlimit (-ceil, ceil, dl[1].at (L) * env);
            p0[i] = o0; p1[i] = o1; pkOut = std::max (pkOut, std::max (std::abs (o0), std::abs (o1)));
        }
        meterA.store (std::max (pkIn, meterA.load() * .9f)); meterB.store (-lin2db (minG)); meterC.store (std::max (pkOut, meterC.load() * .9f));
    }
private:
    static inline const Spec S[] = {
        { "gain", "Ganancia", 0.f, 24.f, 0.f, 0.f, fDb, "" }, { "ceil", "Techo", -12.f, 0.f, -.3f, 0.f, fDb, "" },
        { "rel", "Liberación", 10.f, 500.f, 60.f, 100.f, fMs, "" }, { "mode", "Modo", 0.f, 2.f, 0.f, 0.f, fChoice, "Suave|Rápido|Pico real" } };
    double sr = 44100.0; int L = 96, Lw = 97, cap = 128, head = 0, tail = 0, n = 0, bpos = 0; double sum = 0; float env = 1.f; float hist[2][4] {};
    Delay dl[2]; std::vector<float> mv, bc; std::vector<int> mi;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjLimit)
};

class LimitEditor : public FxEditor
{
public:
    LimitEditor (InjLimit& l) : FxEditor (l, 600, 300, "M3 8h18M7 8v8M12 8v11M17 8v6"), lim (l)
    {
        const int x0 = 176, cw = (getWidth() - 16 - x0) / 3;
        for (int k = 0; k < 3; ++k) knob (k, x0 + k * cw, 66, cw, 62.f);
        pills (InjLimit::pMode, { x0, 182, getWidth() - 16 - x0, 34 });
    }
protected:
    void tick() override
    {
        inDb = std::max (lin2db (lim.meterA.load()), inDb - 1.2f); gr = std::max (lim.meterB.load(), gr - .4f); outDb = std::max (lin2db (lim.meterC.load()), outDb - 1.2f);
    }
    void paintBody (juce::Graphics& g) override
    {
        const char* nm[] { "Entrada", "Reducción", "Salida" };
        for (int k = 0; k < 3; ++k)
        {
            const auto r = juce::Rectangle<float> (16.f + k * 50.f, 66.f, 22.f, 150.f); g.setColour (theme::boxFill); g.fillRoundedRectangle (r, 2.f);
            juce::Graphics::ScopedSaveState ss (g); juce::Path clip; clip.addRoundedRectangle (r, 2.f); g.reduceClipRegion (clip);
            if (k == 1) { const float h = juce::jlimit (0.f, 1.f, gr / 12.f) * r.getHeight(); g.setColour (theme::record); g.fillRect (r.withHeight (h)); }
            else { const float db = k == 0 ? inDb : outDb, h = juce::jlimit (0.f, 1.f, (db + 48.f) / 48.f) * r.getHeight(); g.setColour (theme::accent); g.fillRect (r.withTop (r.getBottom() - h)); }
        }
        g.setColour (theme::textDim); g.setFont (uiFont (11.f));
        for (int k = 0; k < 3; ++k) g.drawText (juce::String::fromUTF8 (nm[k]), (int) (16 + k * 50 - 14), 222, 50, 14, juce::Justification::centred);
        g.setFont (uiFont (12.f, true)); g.setColour (theme::text);
        g.drawText ((gr < .05f ? juce::String ("0.0") : "-" + juce::String (gr, 1)) + " dB", 16, 244, 120, 16, juce::Justification::left);
    }
private:
    InjLimit& lim; float inDb = -90.f, outDb = -90.f, gr = 0.f;
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjLimit::createEditor() { return std::make_unique<LimitEditor> (*this); }
} // namespace injfx
