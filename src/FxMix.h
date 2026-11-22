#pragma once
#include "FxCore.h"
// Dinámica y mezcla: bombeo por tempo (sidechain), multibanda, puerta, de-esser, transient shaper y ensanchador estéreo.
namespace injfx
{
//==============================================================================
// Bombeo por tempo: baja el volumen en cada pulso y lo recupera con la curva elegida (el "sidechain" de la música electrónica sin necesitar otra pista).
class InjDucker : public InjFx
{
public:
    enum { pRate, pDepth, pRel, pCurve, pAtk, pMix };
    INJ_FX_BODY (InjDucker, "injducker", "Bombeo (sidechain)", "Bombeo", 6)
    Layout layout() const override { Layout L; L.pills = pRate; L.visualH = 90; L.knobs = { pDepth, pRel, pCurve, pAtk, pMix }; L.icon = "M3 6c3 0 3 12 6 12s3-12 6-12 3 12 6 12"; return L; }
    std::vector<Preset> presets() const override { return { { "Bombo clásico", { 1.f, .85f, .6f, 0.f, 2.f, 1.f } }, { "Bombeo suave", { 1.f, .5f, .8f, .3f, 5.f, 1.f } }, { "Trance rápido", { 2.f, .9f, .45f, -.2f, 1.f, 1.f } }, { "Medio tiempo", { 0.f, .7f, .7f, .2f, 4.f, 1.f } } }; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; g = 1.f; }
    static float shape (float ph, float rel, float curve)
    {
        if (ph >= rel) return 1.f; const float x = ph / std::max (.01f, rel), e = std::pow (2.f, curve * 2.f); return std::pow (x, e);
    }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return; const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : nullptr };
        static const float per[] { 2.f, 1.f, .5f, .25f }; const float period = per[juce::jlimit (0, 3, vi (pRate))], depth = v (pDepth), rel = v (pRel), cur = v (pCurve), mix = v (pMix);
        const double b0 = beatAt (fc), bpb = bpmAt (fc) / 60.0 / sr, aA = coefMs (std::max (.05f, v (pAtk)), sr);
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            double ph = std::fmod ((b0 + i * bpb) / period, 1.0); if (ph < 0) ph += 1.0; phase = (float) ph;
            const float target = 1.f - depth * (1.f - shape ((float) ph, rel, cur)); g = target < g ? aA * g + (1.f - aA) * target : target;
            const float gm = 1.f - mix + mix * g; for (int c = 0; c < nc; ++c) p[c][i] *= gm;
        }
        meterA.store (phase); meterB.store (g);
    }
    void paintVisual (juce::Graphics& gr, juce::Rectangle<float> r) override
    {
        const float depth = v (pDepth), rel = v (pRel), cur = v (pCurve); juce::Path p;
        for (int x = 0; x <= 568; x += 2) { const float ph = std::fmod (x / 568.f * 2.f, 1.f), y = r.getBottom() - 8.f - (1.f - depth * (1.f - shape (ph, rel, cur))) * (r.getHeight() - 20.f); if (x == 0) p.startNewSubPath (r.getX(), y); else p.lineTo (r.getX() + x, y); }
        gr.setColour (theme::accent); gr.strokePath (p, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float ph = meterA.load(), cx = r.getX() + ph / 2.f * 568.f, cy = r.getBottom() - 8.f - meterB.load() * (r.getHeight() - 20.f); gr.setColour (theme::text); gr.fillEllipse (cx - 4.5f, cy - 4.5f, 9.f, 9.f);
    }
private:
    static inline const Spec S[] = {
        { "rate", "Pulso", 0.f, 3.f, 1.f, 0.f, fChoice, "Cada 2 pulsos|Cada pulso|1/8|1/16" }, { "depth", "Profundidad", 0.f, 1.f, .8f, 0.f, fPct, "" }, { "rel", "Recuperación", .05f, 1.f, .6f, 0.f, fPct, "" },
        { "curve", "Curva", -1.f, 1.f, 0.f, 0.f, fPct, "" }, { "atk", "Ataque", .05f, 30.f, 2.f, 5.f, fMs, "" }, { "mix", "Mezcla", 0.f, 1.f, 1.f, 0.f, fPct, "" } };
    double sr = 44100.0; float g = 1.f, phase = 0.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjDucker)
};

//==============================================================================
// Multibanda de 3 bandas (cruces Linkwitz-Riley de 24 dB): un compresor por banda, con la fase alineada para sumar plano.
class InjMulti : public InjFx
{
public:
    enum { pX1, pX2, pTL, pRL, pGL, pTM, pRM, pGM, pTH, pRH, pGH, pAtk, pRel };
    INJ_FX_BODY (InjMulti, "injmulti", "Multibanda", "Multi", 13)
    Layout layout() const override
    {
        Layout L; L.visualH = 96; L.dia = 44.f; L.rows = { { pX1, pX2, pAtk, pRel }, { pTL, pRL, pGL, pTM, pRM, pGM, pTH, pRH, pGH } }; L.icon = "M4 4v16M12 4v16M20 4v16M4 14h8M12 8h8"; return L;
    }
    std::vector<Preset> presets() const override
    {
        return { { "Master pegado", { 150.f, 2500.f, -18.f, 2.5f, 0.f, -18.f, 2.f, 0.f, -20.f, 2.f, 0.f, 20.f, 150.f } }, { "Controlar graves", { 120.f, 2500.f, -22.f, 4.f, 0.f, -10.f, 1.5f, 0.f, -10.f, 1.5f, 0.f, 15.f, 120.f } },
                 { "Brillo suave", { 200.f, 3500.f, -14.f, 2.f, 0.f, -16.f, 2.f, 0.f, -24.f, 3.5f, 1.f, 10.f, 100.f } } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; for (auto& b : f) b.reset(); for (auto& x : gr) x = 0.f; lastX1 = lastX2 = -1.f; }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return; const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : fc.destBuffer->getWritePointer (0, fc.bufferStartSample) };
        const float x1 = v (pX1), x2 = std::max (v (pX2), v (pX1) * 1.5f);
        if (x1 != lastX1 || x2 != lastX2) { for (int k = 0; k < 2; ++k) { f[k].set (BQ::LP, sr, x1, .7071); f[2 + k].set (BQ::HP, sr, x1, .7071); f[4 + k].set (BQ::LP, sr, x2, .7071); f[6 + k].set (BQ::HP, sr, x2, .7071); f[8 + k].set (BQ::LP, sr, x2, .7071); f[10 + k].set (BQ::HP, sr, x2, .7071); } lastX1 = x1; lastX2 = x2; }
        const float thr[3] { v (pTL), v (pTM), v (pTH) }, rat[3] { v (pRL), v (pRM), v (pRH) }, gn[3] { db2lin (v (pGL)), db2lin (v (pGM)), db2lin (v (pGH)) };
        const float aA = coefMs (v (pAtk), sr), aR = coefMs (v (pRel), sr); float mx[3] { 0, 0, 0 };
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            float in[2] { p[0][i], p[1][i] }, band[3][2];
            for (int c = 0; c < 2; ++c)
            {
                const float lo = f[1].run (c, f[0].run (c, in[c])), rest = f[3].run (c, f[2].run (c, in[c]));
                const float mid = f[5].run (c, f[4].run (c, rest)), hi = f[7].run (c, f[6].run (c, rest));
                band[0][c] = f[9].run (c, f[8].run (c, lo)) + f[11].run (c, f[10].run (c, lo)); band[1][c] = mid; band[2][c] = hi;   // el grave pasa por un pasa-todo (suma LP+HP del segundo cruce) para alinear la fase
            }
            float out[2] { 0.f, 0.f };
            for (int b = 0; b < 3; ++b)
            {
                const float pk = std::max (std::abs (band[b][0]), std::abs (band[b][1])), db = lin2db (pk), target = std::max (0.f, (db - thr[b]) * (1.f - 1.f / rat[b]));
                const float cc = target > gr[b] ? aA : aR; gr[b] = cc * gr[b] + (1.f - cc) * target; mx[b] = std::max (mx[b], gr[b]);
                const float g = std::exp (-gr[b] * .11512925f) * gn[b]; out[0] += band[b][0] * g; out[1] += band[b][1] * g;
            }
            p[0][i] = out[0]; if (nc > 1) p[1][i] = out[1];
        }
        meterA.store (mx[0]); meterB.store (mx[1]); meterC.store (mx[2]);
    }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        const float x1 = v (pX1), x2 = v (pX2); auto X = [&] (float hz) { return r.getX() + std::log10 (hz / 20.f) / 3.f * r.getWidth(); };
        const float xs[4] { r.getX(), X (x1), X (x2), r.getRight() }; const float m[3] { meterA.load(), meterB.load(), meterC.load() }; const char* nm[3] { "Graves", "Medios", "Agudos" };
        for (int b = 0; b < 3; ++b)
        {
            const auto z = juce::Rectangle<float> (xs[b] + 2.f, r.getY() + 6.f, xs[b + 1] - xs[b] - 4.f, r.getHeight() - 12.f); const auto col = theme::trackColours[b == 0 ? 0 : b == 1 ? 1 : 3];
            g.setColour (col.withAlpha (.12f)); g.fillRoundedRectangle (z, 6.f); g.setColour (col.withAlpha (.8f)); g.fillRoundedRectangle (z.withHeight (juce::jlimit (0.f, 1.f, m[b] / 18.f) * z.getHeight()), 6.f);
            label (g, juce::String::fromUTF8 (nm[b]) + "  " + grText (m[b]) + " dB", z.reduced (8.f, 4.f), juce::Justification::bottomLeft, 11.f, true, theme::text);
        }
    }
private:
    static inline const Spec S[] = {
        { "x1", "Cruce grave|Cruce 1", 60.f, 800.f, 150.f, 250.f, fHz, "" }, { "x2", "Cruce agudo|Cruce 2", 800.f, 8000.f, 2500.f, 2500.f, fHz, "" },
        { "tl", "Umbral graves|Umb. grv", -60.f, 0.f, -20.f, 0.f, fDb, "" }, { "rl", "Ratio graves|Rat. grv", 1.f, 20.f, 3.f, 4.f, fRatio, "" }, { "gl", "Ganancia graves|Gan. grv", -12.f, 12.f, 0.f, 0.f, fDb, "" },
        { "tm", "Umbral medios|Umb. med", -60.f, 0.f, -20.f, 0.f, fDb, "" }, { "rm", "Ratio medios|Rat. med", 1.f, 20.f, 2.f, 4.f, fRatio, "" }, { "gm", "Ganancia medios|Gan. med", -12.f, 12.f, 0.f, 0.f, fDb, "" },
        { "th", "Umbral agudos|Umb. agu", -60.f, 0.f, -20.f, 0.f, fDb, "" }, { "rh", "Ratio agudos|Rat. agu", 1.f, 20.f, 2.f, 4.f, fRatio, "" }, { "gh", "Ganancia agudos|Gan. agu", -12.f, 12.f, 0.f, 0.f, fDb, "" },
        { "atk", "Ataque", 1.f, 100.f, 15.f, 15.f, fMs, "" }, { "rel", "Liberación", 20.f, 500.f, 150.f, 150.f, fMs, "" } };
    double sr = 44100.0; BQ f[12]; float gr[3] {}, lastX1 = -1.f, lastX2 = -1.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjMulti)
};

//==============================================================================
// Puerta de ruido con histéresis, retención y piso ajustable.
class InjGate : public InjFx
{
public:
    enum { pThr, pRange, pAtk, pHold, pRel, pHyst };
    INJ_FX_BODY (InjGate, "injgate", "Puerta de ruido", "Puerta", 6)
    Layout layout() const override { Layout L; L.visualH = 40; L.knobs = { pThr, pRange, pAtk, pHold, pRel, pHyst }; L.perRow = 6; L.dia = 50.f; L.icon = "M4 20V4h16v16M4 12h16"; return L; }
    std::vector<Preset> presets() const override { return { { "Limpiar ruido", { -48.f, -60.f, 1.f, 40.f, 150.f, 4.f } }, { "Bombo seco", { -30.f, -80.f, .3f, 20.f, 60.f, 3.f } }, { "Suave", { -55.f, -18.f, 5.f, 60.f, 250.f, 6.f } } }; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; env = 0.f; g = 0.f; open = false; hold = 0; }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return; const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : nullptr };
        const float thr = v (pThr), hy = v (pHyst), floorG = db2lin (v (pRange)); const int holdS = (int) (v (pHold) * .001f * (float) sr);
        const float aA = coefMs (v (pAtk), sr), aR = coefMs (v (pRel), sr), eR = coefMs (8.f, sr); float mn = 1.f;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            float pk = std::abs (p[0][i]); if (nc > 1) pk = std::max (pk, std::abs (p[1][i])); env = pk > env ? pk : eR * env + (1.f - eR) * pk;
            const float db = lin2db (env); if (db > thr) { open = true; hold = holdS; } else if (db < thr - hy) { if (hold > 0) --hold; else open = false; } else if (hold > 0) --hold;
            const float target = open ? 1.f : floorG; g = target > g ? aA * g + (1.f - aA) * target : aR * g + (1.f - aR) * target; if (g < floorG) g = floorG; mn = std::min (mn, g);
            for (int c = 0; c < nc; ++c) p[c][i] *= g;
        }
        meterA.store (mn);
    }
    void paintVisual (juce::Graphics& gr, juce::Rectangle<float> r) override
    {
        const float g = meterA.load(); barH (gr, r.reduced (16.f, 14.f), g, g > .7f ? theme::accent : juce::Colour (0xff8a8a90));
        label (gr, g > .7f ? "Abierta" : "Cerrada", r.reduced (24.f, 0.f), juce::Justification::centredLeft, 12.f, true, juce::Colour (0xff1a1a1a).withAlpha (g > .7f ? 1.f : 0.f)); label (gr, dbText (lin2db (g)) + " dB", r.reduced (24.f, 0.f), juce::Justification::centredRight, 12.f, true, theme::text);
    }
private:
    static inline const Spec S[] = {
        { "thr", "Umbral", -80.f, 0.f, -45.f, 0.f, fDb, "" }, { "range", "Reducción", -80.f, 0.f, -80.f, 0.f, fDb, "" }, { "atk", "Ataque", .1f, 50.f, 1.f, 5.f, fMs, "" },
        { "hold", "Retención", 0.f, 500.f, 30.f, 80.f, fMs, "" }, { "rel", "Liberación", 5.f, 1000.f, 120.f, 120.f, fMs, "" }, { "hyst", "Histéresis", 0.f, 12.f, 3.f, 0.f, fDb, "" } };
    double sr = 44100.0; float env = 0.f, g = 0.f; bool open = false; int hold = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjGate)
};

//==============================================================================
// De-esser de banda dividida: solo baja la zona de las eses cuando se pasan del umbral.
class InjDeEss : public InjFx
{
public:
    enum { pFreq, pThr, pAmt, pQ, pListen };
    INJ_FX_BODY (InjDeEss, "injdeess", "De-esser", "DeEss", 5)
    Layout layout() const override { Layout L; L.visualH = 40; L.knobs = { pFreq, pThr, pAmt, pQ, pListen }; L.icon = "M5 8c0-3 6-3 6 0s6 3 6 6-6 3-6 0"; return L; }
    std::vector<Preset> presets() const override { return { { "Voz masculina", { 5500.f, -30.f, 10.f, 1.5f, 0.f } }, { "Voz femenina", { 7500.f, -30.f, 10.f, 1.5f, 0.f } }, { "Hi-hats duros", { 9500.f, -26.f, 8.f, 1.2f, 0.f } } }; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; bp.reset(); gr = 0.f; last = -1.f; }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return; const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : nullptr };
        const float fq = v (pFreq) + v (pQ) * 1000.f; if (fq != last) { bp.set (BQ::BP, sr, v (pFreq), v (pQ)); last = fq; }
        const float thr = v (pThr), maxRed = v (pAmt); const bool listen = vi (pListen) == 1; const float aA = coefMs (.5f, sr), aR = coefMs (45.f, sr); float mg = 0.f;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            float band[2] { bp.run (0, p[0][i]), nc > 1 ? bp.run (1, p[1][i]) : 0.f }; const float pk = std::max (std::abs (band[0]), std::abs (band[1])), db = lin2db (pk);
            const float target = juce::jlimit (0.f, maxRed, (db - thr) * .83f); const float cc = target > gr ? aA : aR; gr = cc * gr + (1.f - cc) * target; mg = std::max (mg, gr);
            const float cut = 1.f - std::exp (-gr * .11512925f);   // fracción de la banda que se quita
            for (int c = 0; c < nc; ++c) p[c][i] = listen ? band[c] : p[c][i] - band[c] * cut;
        }
        meterA.store (mg);
    }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override { barH (g, r.reduced (16.f, 14.f), meterA.load() / 12.f, juce::Colour (0xffff7a70)); label (g, juce::String::fromUTF8 ("Reducción  ") + grText (meterA.load()) + " dB", r.reduced (24.f, 0.f), juce::Justification::centredLeft, 12.f, true, theme::text); }
private:
    static inline const Spec S[] = {
        { "freq", "Frecuencia", 3000.f, 12000.f, 6500.f, 6000.f, fHz, "" }, { "thr", "Umbral", -50.f, 0.f, -28.f, 0.f, fDb, "" }, { "amt", "Reducción máx.|Máx.", 0.f, 24.f, 10.f, 0.f, fDb, "" },
        { "q", "Ancho|Q", .5f, 4.f, 1.5f, 1.5f, fQ, "" }, { "listen", "Escuchar", 0.f, 1.f, 0.f, 0.f, fChoice, "No|Banda" } };
    double sr = 44100.0; BQ bp; float gr = 0.f, last = -1.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjDeEss)
};

//==============================================================================
// Transient shaper: compara una envolvente rápida con una lenta para realzar o suavizar el golpe y la cola.
class InjTransient : public InjFx
{
public:
    enum { pAtk, pSus, pOut };
    INJ_FX_BODY (InjTransient, "injtransient", "Transientes", "Trans", 3)
    Layout layout() const override { Layout L; L.visualH = 70; L.knobs = { pAtk, pSus, pOut }; L.perRow = 3; L.dia = 62.f; L.icon = "M3 18L8 4l3 14 3-10 2 8h5"; return L; }
    std::vector<Preset> presets() const override { return { { "Más golpe", { .6f, 0.f, 0.f } }, { "Bombo corto", { .4f, -.6f, 0.f } }, { "Más cuerpo", { 0.f, .6f, 0.f } }, { "Suavizar", { -.5f, .2f, 0.f } } }; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; ef = es = 0.f; g = 1.f; }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return; const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : nullptr };
        const float atk = v (pAtk), sus = v (pSus), outG = db2lin (v (pOut));
        const float fA = coefMs (.4f, sr), fR = coefMs (18.f, sr), sA = coefMs (28.f, sr), sR = coefMs (110.f, sr), gA = coefMs (.6f, sr); float mxg = 0.f, mng = 0.f;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            float pk = std::abs (p[0][i]); if (nc > 1) pk = std::max (pk, std::abs (p[1][i]));
            ef = pk > ef ? fA * ef + (1.f - fA) * pk : fR * ef + (1.f - fR) * pk; es = pk > es ? sA * es + (1.f - sA) * pk : sR * es + (1.f - sR) * pk;
            const float d = juce::jlimit (-18.f, 18.f, lin2db (ef + 1e-6f) - lin2db (es + 1e-6f)); float gdb = d > 0.f ? atk * d * .9f : sus * (-d) * .9f; gdb = juce::jlimit (-15.f, 15.f, gdb);
            const float target = std::exp (gdb * .11512925f); g = gA * g + (1.f - gA) * target; mxg = std::max (mxg, lin2db (g)); mng = std::min (mng, lin2db (g));
            for (int c = 0; c < nc; ++c) p[c][i] *= g * outG;
        }
        meterA.store (mxg); meterB.store (mng);
    }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        const float a = v (pAtk), s = v (pSus); juce::Path p; const float base = r.getBottom() - 8.f, h = r.getHeight() - 16.f;
        for (int x = 0; x <= 568; x += 2) { const float t = x / 568.f, env = std::exp (-t * 5.f), shaped = env * (1.f + (t < .08f ? a * .8f : 0.f) + (t >= .08f ? s * .6f * (1.f - t) : 0.f)); const float y = base - juce::jlimit (0.f, 1.f, shaped * .75f) * h; if (x == 0) p.startNewSubPath (r.getX(), y); else p.lineTo (r.getX() + x, y); }
        g.setColour (theme::accent); g.strokePath (p, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        juce::Path q; for (int x = 0; x <= 568; x += 2) { const float y = base - std::exp (-(x / 568.f) * 5.f) * .75f * h; if (x == 0) q.startNewSubPath (r.getX(), y); else q.lineTo (r.getX() + x, y); }
        g.setColour (theme::barLine); g.strokePath (q, juce::PathStrokeType (1.2f)); label (g, "Original", { r.getX() + 8.f, r.getY() + 4.f, 80.f, 14.f });
    }
private:
    static inline const Spec S[] = { { "atk", "Ataque", -1.f, 1.f, 0.f, 0.f, fPct, "" }, { "sus", "Sostén", -1.f, 1.f, 0.f, 0.f, fPct, "" }, { "out", "Salida", -12.f, 12.f, 0.f, 0.f, fDb, "" } };
    double sr = 44100.0; float ef = 0.f, es = 0.f, g = 1.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjTransient)
};

//==============================================================================
// Ensanchador estéreo: ancho de los lados, graves en mono, ganancia del centro y medidor de correlación.
class InjWidener : public InjFx
{
public:
    enum { pWidth, pMono, pMid, pOut };
    INJ_FX_BODY (InjWidener, "injwidener", "Ensanchador estéreo", "Ancho", 4)
    Layout layout() const override { Layout L; L.visualH = 50; L.knobs = { pWidth, pMono, pMid, pOut }; L.perRow = 4; L.icon = "M12 4v16M8 8c-3 2-3 6 0 8M16 8c3 2 3 6 0 8"; return L; }
    std::vector<Preset> presets() const override { return { { "Ancho moderado", { 1.4f, 120.f, 0.f, 0.f } }, { "Súper ancho", { 1.8f, 150.f, -1.f, -1.f } }, { "Casi mono", { .3f, 100.f, 0.f, 0.f } }, { "Graves firmes", { 1.2f, 200.f, 0.f, 0.f } } }; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; hp.reset(); last = -1.f; corr = 0.f; sLL = sRR = sLR = 0.f; }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.destBuffer->getNumChannels() < 2) return;
        float* l = fc.destBuffer->getWritePointer (0, fc.bufferStartSample); float* r = fc.destBuffer->getWritePointer (1, fc.bufferStartSample);
        const float w = v (pWidth), mg = db2lin (v (pMid)), og = db2lin (v (pOut)), fm = v (pMono); if (fm != last) { hp.set (BQ::HP, sr, fm, .7071); last = fm; } const bool useMono = fm > 21.f;
        const float k = coefMs (150.f, sr);
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            const float m = (l[i] + r[i]) * .5f * mg; float s = (l[i] - r[i]) * .5f * w; if (useMono) s = hp.run (0, s);
            const float oL = (m + s) * og, oR = (m - s) * og; sLL = k * sLL + (1.f - k) * oL * oL; sRR = k * sRR + (1.f - k) * oR * oR; sLR = k * sLR + (1.f - k) * oL * oR; l[i] = oL; r[i] = oR;
        }
        meterA.store (sLR / std::sqrt (std::max (1e-12f, sLL * sRR)));
    }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        const auto b = r.reduced (24.f, 18.f); g.setColour (theme::bgBar); g.fillRoundedRectangle (b.withHeight (14.f).withY (r.getCentreY() - 7.f), 7.f); const float c = juce::jlimit (-1.f, 1.f, meterA.load()), cx = b.getCentreX() + c * b.getWidth() * .5f;
        g.setColour (c < 0.f ? juce::Colour (0xffff7a70) : theme::accent); g.fillEllipse (cx - 8.f, r.getCentreY() - 8.f, 16.f, 16.f);
        label (g, juce::String::fromUTF8 ("-1  fase invertida"), { b.getX(), r.getY() + 4.f, 200.f, 14.f }); label (g, "+1  mono", { b.getRight() - 200.f, r.getY() + 4.f, 200.f, 14.f }, juce::Justification::centredRight); label (g, juce::String::fromUTF8 ("Correlación"), { b.getX(), r.getBottom() - 18.f, 120.f, 14.f });
    }
private:
    static inline const Spec S[] = { { "width", "Ancho", 0.f, 2.f, 1.f, 0.f, fPct, "" }, { "mono", "Graves en mono", 20.f, 500.f, 20.f, 120.f, fHz, "" }, { "mid", "Centro", -12.f, 12.f, 0.f, 0.f, fDb, "" }, { "out", "Salida", -12.f, 12.f, 0.f, 0.f, fDb, "" } };
    double sr = 44100.0; BQ hp; float last = -1.f, corr = 0.f, sLL = 0.f, sRR = 0.f, sLR = 0.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjWidener)
};
} // namespace injfx
