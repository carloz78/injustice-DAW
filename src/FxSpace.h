#pragma once
#include "FxCore.h"
// Espacio y movimiento: reverb (red de retardos con realimentación), delay estéreo con rebote y modulación (chorus, flanger, phaser).
namespace injfx
{
//==============================================================================
// Reverb: red de 8 líneas con matriz de Hadamard, difusión de entrada, amortiguación de agudos en el lazo y modulación lenta.
class InjReverb : public InjFx
{
public:
    enum { pType, pSize, pTime, pPre, pDark, pMix, pWidth, pLow };
    static inline const char* xmlTypeName = "injreverb";
    static const char* getPluginName() { return "Reverb"; }
    InjReverb (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Reverb"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 8; }
    double getTailLength() const override { return (double) v (pTime) + (double) v (pPre) * .001 + .2; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    std::vector<Preset> presets() const override
    {
        return { { "Sala oscura", { 0.f, .45f, 1.8f, 20.f, .6f, .22f, 1.f, 60.f } }, { "Plato brillante", { 1.f, .4f, 2.2f, 5.f, .15f, .25f, 1.15f, 40.f } }, { "Cámara cálida", { 2.f, .6f, 2.6f, 15.f, .45f, .2f, 1.f, 40.f } },
                 { "Catedral enorme", { 3.f, .9f, 7.f, 35.f, .5f, .3f, 1.25f, 90.f } }, { "Cola larga de kick", { 3.f, .8f, 4.5f, 0.f, .75f, .18f, .8f, 160.f } }, { "Ambiente corto", { 0.f, .25f, .5f, 0.f, .5f, .15f, .9f, 40.f } } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override
    {
        sr = i.sampleRate; for (auto& l : line) { l.prep ((int) (.34 * sr)); l.clear(); } for (auto& s : lenSm) s.prep (sr, 350.f); for (int k = 0; k < N; ++k) { lp[k] = 0.f; lenSm[k].snap (lenTarget (k)); ph[k] = .13f * k; }
        pd[0].prep ((int) (.25 * sr)); pd[1].prep ((int) (.25 * sr)); pd[0].clear(); pd[1].clear(); lowcut[0].reset(); lowcut[1].reset(); lastLow = -1.f;
        for (int c = 0; c < 2; ++c) for (int s = 0; s < 3; ++s) { ap[c][s].prep ((int) (.03 * sr)); ap[c][s].clear(); }
    }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || line[0].b.empty()) return;
        const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p0 = fc.destBuffer->getWritePointer (0, fc.bufferStartSample); float* p1 = nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : p0;
        const float T = std::max (.1f, v (pTime)), dark = v (pDark), mix = v (pMix), preS = v (pPre) * .001f * (float) sr;
        const float fcut = 18000.f * std::pow (.08f, dark), cLp = 1.f - std::exp (-2.f * kPi * fcut / (float) sr), dry = std::cos (mix * kPi * .5f), wet = std::sin (mix * kPi * .5f);
        const float wdt = v (pWidth), lowF = v (pLow); const bool lowOn = lowF > 21.f; if (lowF != lastLow) { lowcut[0].set (BQ::HP, sr, lowF, .7071); lowcut[1].set (BQ::HP, sr, lowF, .7071); lastLow = lowF; }
        float gl[N], tgt[N]; for (int k = 0; k < N; ++k) { tgt[k] = lenTarget (k); gl[k] = std::exp (-6.9078f * (tgt[k] / (float) sr) / T); }
        const float modD = .00035f * (float) sr, apG = .62f; const int apLen[3] { (int) (.0051 * sr), (int) (.0077 * sr), (int) (.0113 * sr) };
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            const float inL = p0[i], inR = p1[i]; pd[0].push (inL); pd[1].push (inR);
            float x[2] { pd[0].read (std::max (1.f, preS)), pd[1].read (std::max (1.f, preS)) };
            for (int c = 0; c < 2; ++c) for (int s = 0; s < 2; ++s) { auto& a = ap[c][s]; const float z = a.at (apLen[s] - 1), w = x[c] + apG * z; x[c] = -apG * w + z; a.push (w); }
            float y[N];
            for (int k = 0; k < N; ++k)
            {
                ph[k] += (.21f + .067f * k) / (float) sr; if (ph[k] > 1.f) ph[k] -= 1.f;
                const float d = lenSm[k].next (tgt[k]) + modD * std::sin (2.f * kPi * ph[k]);
                const float o = line[k].read (std::max (4.f, d)); lp[k] += cLp * (o - lp[k]); y[k] = lp[k] * gl[k];
            }
            // matriz de Hadamard de 8 (suma y resta en tres etapas), normalizada
            float a[N]; for (int k = 0; k < N; k += 2) { a[k] = y[k] + y[k + 1]; a[k + 1] = y[k] - y[k + 1]; }
            float b[N]; for (int k = 0; k < N; k += 4) { b[k] = a[k] + a[k + 2]; b[k + 1] = a[k + 1] + a[k + 3]; b[k + 2] = a[k] - a[k + 2]; b[k + 3] = a[k + 1] - a[k + 3]; }
            float z[N]; for (int k = 0; k < 4; ++k) { z[k] = b[k] + b[k + 4]; z[k + 4] = b[k] - b[k + 4]; }
            const float nrm = .35355339f; for (int k = 0; k < N; ++k) line[k].push (z[k] * nrm + ((k & 1) ? x[1] : x[0]) * .5f);
            float oL = (y[0] - y[1] + y[2] - y[3] + y[4] - y[5] + y[6] - y[7]) * kWet, oR = (y[0] + y[1] - y[2] - y[3] + y[4] + y[5] - y[6] - y[7]) * kWet;
            { const float m = .5f * (oL + oR), sd = .5f * (oL - oR) * wdt; oL = m + sd; oR = m - sd; } if (lowOn) { oL = lowcut[0].run (0, oL); oR = lowcut[1].run (0, oR); }
            p0[i] = inL * dry + oL * wet; p1[i] = inR * dry + oR * wet;
        }
    }
    static constexpr float kWet = .62f;
private:
    static constexpr int N = 8;
    float lenTarget (int k) const
    {
        static const float tab[4][N] { { 29.7f, 37.1f, 41.1f, 43.7f, 47.9f, 53.3f, 59.9f, 67.7f }, { 14.3f, 17.9f, 21.1f, 25.7f, 29.3f, 33.7f, 37.9f, 41.3f },
                                       { 41.3f, 47.9f, 53.9f, 61.3f, 71.9f, 79.3f, 89.3f, 101.3f }, { 61.7f, 73.3f, 83.9f, 97.1f, 109.7f, 121.3f, 137.9f, 151.1f } };
        return tab[juce::jlimit (0, 3, vi (pType))][k] * (.4f + 1.2f * v (pSize)) * .001f * (float) sr;
    }
    static inline const Spec S[] = {
        { "type", "Tipo", 0.f, 3.f, 0.f, 0.f, fChoice, "Sala|Plato|Cámara|Catedral" }, { "size", "Tamaño", 0.f, 1.f, .45f, 0.f, fPct, "" }, { "time", "Tiempo", .2f, 10.f, 1.8f, 2.f, fSec, "" },
        { "pre", "Pre-retardo", 0.f, 200.f, 20.f, 0.f, fMs, "" }, { "dark", "Oscuridad", 0.f, 1.f, .6f, 0.f, fPct, "" }, { "mix", "Mezcla", 0.f, 1.f, .22f, 0.f, fPct, "" },
        { "width", "Ancho", 0.f, 1.5f, 1.f, 0.f, fPct, "" }, { "low", "Corte de graves|Corte graves", 20.f, 500.f, 20.f, 100.f, fCutLo, "" } };
    double sr = 44100.0; Delay line[N], pd[2], ap[2][3]; Smooth lenSm[N]; float lp[N] {}, ph[N] {}, lastLow = -1.f; BQ lowcut[2];
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjReverb)
};

class ReverbEditor : public FxEditor
{
public:
    ReverbEditor (InjReverb& r) : FxEditor (r, 600, 320, "M3 12h2l2-6 3 12 3-9 2 5 2-2h4")
    {
        pills (InjReverb::pType, { 16, 64, 568, 34 }); knobRow ({ InjReverb::pSize, InjReverb::pTime, InjReverb::pPre, InjReverb::pDark, InjReverb::pWidth, InjReverb::pLow, InjReverb::pMix }, 184);
    }
protected:
    void paintBody (juce::Graphics& g) override
    {
        const auto r = juce::Rectangle<float> (16.f, 112.f, 568.f, 56.f); plotBox (g, r); g.saveState(); juce::Path clip; clip.addRoundedRectangle (r, 3.f); g.reduceClipRegion (clip);
        const float T = fx.v (InjReverb::pTime), pre = fx.v (InjReverb::pPre) * .001f, tmax = std::max (3.f, T * 1.25f + pre); juce::Path p, fill; const float base = r.getBottom() - 4.f, hgt = r.getHeight() - 10.f;
        p.startNewSubPath (r.getX(), base);
        for (int x = 0; x <= 284; ++x)
        {
            const float t = x / 284.f * tmax - pre; const float env = t < 0.f ? 0.f : std::max (0.f, 1.f - t / T);   // cae en línea recta en dB: llega a -60 dB en el tiempo pedido
            const float h = std::sin (x * 12.9898f) * 43758.5453f, jit = h - std::floor (h); const float a = env * (.7f + .3f * jit);
            p.lineTo (r.getX() + x * 2.f, base - a * hgt);
        }
        fill = p; fill.lineTo (r.getRight(), r.getBottom()); fill.lineTo (r.getX(), r.getBottom()); fill.closeSubPath(); g.setColour (theme::accent.withAlpha (.12f)); g.fillPath (fill);
        g.setColour (theme::accent); g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::mitered)); g.restoreState();
    }
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjReverb::createEditor() { return std::make_unique<ReverbEditor> (*this); }

//==============================================================================
// Delay estéreo sincronizado al tempo del proyecto (o libre), con rebote izquierda/derecha, filtro de agudos en la realimentación y ancho.
class InjDelay : public InjFx
{
public:
    enum { pTime, pFb, pTone, pPing, pWidth, pMix, pFree, pLow, pDuck };
    static inline const char* xmlTypeName = "injdelay";
    static const char* getPluginName() { return "Delay"; }
    InjDelay (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Delay"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 9; }
    double getTailLength() const override { const float fb = std::min (.95f, v (pFb)); return fb < .02f ? .5 : std::min (12.0, (double) lastSec * std::log (.001) / std::log ((double) fb)); }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    std::vector<Preset> presets() const override
    {
        return { { "Rebote ancho", { 1.f, .38f, 5000.f, .6f, .8f, .25f, 350.f, 60.f, 0.f } }, { "Eco de negra", { 0.f, .45f, 7000.f, 0.f, 1.f, .22f, 350.f, 60.f, .25f } }, { "Corto y pegado", { 3.f, .2f, 4000.f, .3f, .6f, .18f, 350.f, 80.f, 0.f } },
                 { "Oscuro y largo", { 2.f, .65f, 2500.f, .7f, 1.f, .3f, 350.f, 100.f, .4f } }, { "Slapback", { 4.f, .1f, 6000.f, 0.f, .5f, .25f, 110.f, 80.f, 0.f } }, { "Dub con ducking", { 1.f, .55f, 3500.f, .5f, 1.f, .3f, 350.f, 140.f, .6f } } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override
    {
        sr = i.sampleRate; for (auto& d : dl) { d.prep ((int) (2.6 * sr)); d.clear(); } sm.prep (sr, 90.f); sm.snap (.375f * (float) sr); lpS[0] = lpS[1] = hpS[0] = hpS[1] = 0.f; lastSec = .375f; first = true; env = 0.f; dk = 1.f;
    }
    float delaySeconds (const te::PluginRenderContext& fc)
    {
        const int m = vi (pTime); if (m >= 4) return v (pFree) * .001f;
        const double bpm = std::max (30.0, edit.tempoSequence.getBpmAt (fc.editTime.getStart())); const float beats[] { 1.f, .5f, .75f, .25f }; return (float) (beats[m] * 60.0 / bpm);
    }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || dl[0].b.empty()) return;
        const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p0 = fc.destBuffer->getWritePointer (0, fc.bufferStartSample); float* p1 = nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : p0;
        lastSec = std::min (2.5f, std::max (.01f, delaySeconds (fc))); const float tgt = lastSec * (float) sr, fb = std::min (.95f, v (pFb)), pp = v (pPing), wd = v (pWidth), mix = v (pMix);
        const float cLp = 1.f - std::exp (-2.f * kPi * v (pTone) / (float) sr), cHp = 1.f - std::exp (-2.f * kPi * v (pLow) / (float) sr), duck = v (pDuck), rEnv = coefMs (180.f, sr);
        if (first) { sm.snap (tgt); first = false; }
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            const float t = sm.next (tgt), inL = p0[i], inR = p1[i], mono = .5f * (inL + inR); { const float a = std::max (std::abs (inL), std::abs (inR)); env = a > env ? a : rEnv * env + (1.f - rEnv) * a; const float tg = 1.f - duck * juce::jlimit (0.f, 1.f, env * 4.f); dk += (tg - dk) * (tg < dk ? .02f : .0015f); }
            const float d[2] { dl[0].read (t), dl[1].read (t) };
            const float src[2] { (1.f - pp) * d[0] + pp * d[1], (1.f - pp) * d[1] + pp * d[0] }; float w[2];
            for (int c = 0; c < 2; ++c)
            {
                lpS[c] += cLp * (src[c] - lpS[c]); hpS[c] += cHp * (lpS[c] - hpS[c]); float f = (lpS[c] - hpS[c]) * fb;
                if (std::abs (f) > .9f) f = (f < 0.f ? -1.f : 1.f) * (.9f + std::tanh ((std::abs (f) - .9f) * 10.f) * .1f);   // limita la realimentación para que nunca se dispare
                w[c] = f;
            }
            dl[0].push (w[0] + inL * (1.f - pp) + mono * pp); dl[1].push (w[1] + inR * (1.f - pp));
            const float m = .5f * (d[0] + d[1]), s = .5f * (d[0] - d[1]) * wd;
            p0[i] = inL + (m + s) * mix * dk; p1[i] = inR + (m - s) * mix * dk;
        }
    }
private:
    static inline const Spec S[] = {
        { "time", "Tiempo", 0.f, 4.f, 1.f, 0.f, fChoice, "1/4|1/8|1/8 con punto|1/16|Libre" }, { "fb", "Repeticiones", 0.f, .95f, .38f, 0.f, fPct, "" },
        { "tone", "Filtro agudos", 1000.f, 20000.f, 5000.f, 5000.f, fHz, "" }, { "ping", "Rebote L/R", 0.f, 1.f, .6f, 0.f, fPct, "" }, { "width", "Ancho", 0.f, 1.f, .8f, 0.f, fPct, "" },
        { "mix", "Mezcla", 0.f, 1.f, .25f, 0.f, fPct, "" }, { "free", "Tiempo libre", 20.f, 2000.f, 350.f, 300.f, fMs, "" },
        { "low", "Corte de graves|Corte graves", 20.f, 600.f, 40.f, 100.f, fHz, "" }, { "duck", "Reducir al sonar|Ducking", 0.f, 1.f, 0.f, 0.f, fPct, "" } };
    double sr = 44100.0; Delay dl[2]; Smooth sm; float lpS[2] {}, hpS[2] {}, lastSec = .375f, env = 0.f, dk = 1.f; bool first = true;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjDelay)
};

class DelayEditor : public FxEditor
{
public:
    DelayEditor (InjDelay& d) : FxEditor (d, 600, 392, "M4 5v14M10 8v8M16 10.5v3M21 12h0")
    {
        pills (InjDelay::pTime, { 16, 64, 568, 34 });
        const int ids[8] { InjDelay::pFree, InjDelay::pFb, InjDelay::pTone, InjDelay::pLow, InjDelay::pPing, InjDelay::pWidth, InjDelay::pDuck, InjDelay::pMix };
        for (int k = 0; k < 8; ++k) ks[k] = knob (ids[k], 16, 184, 100);
        layoutKnobs();
    }
protected:
    void layoutKnobs()
    {
        const bool libre = fx.vi (InjDelay::pTime) >= 4; if (laid && libre == lastLibre) return; laid = true; lastLibre = libre;
        const int cw = 568 / 4; ks[0]->setVisible (libre); int c = 0;
        for (int k = libre ? 0 : 1; k < 8; ++k, ++c) ks[k]->setBounds (16 + (c % 4) * cw, 184 + (c / 4) * 100, cw, 94);
    }
    void tick() override { layoutKnobs(); }
    void paintBody (juce::Graphics& g) override
    {
        const auto r = juce::Rectangle<float> (16.f, 112.f, 568.f, 56.f); plotBox (g, r); const float fb = fx.v (InjDelay::pFb);
        g.setColour (theme::text); g.fillRoundedRectangle (r.getX() + 20.f, r.getY() + 8.f, 8.f, 40.f, 2.f); const float step = 110.f;
        for (int k = 1; k <= 4; ++k) { const float h = std::max (3.f, 34.f * std::pow (fb / .95f * .95f + .02f, (float) k - 1.f)); g.setColour (theme::accent.withAlpha (1.f - .17f * (float) k)); g.fillRoundedRectangle (r.getX() + 20.f + step * k, r.getBottom() - 8.f - h, 8.f, h, 2.f); }
    }
private:
    FxKnob* ks[8] {}; bool lastLibre = false, laid = false;
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjDelay::createEditor() { return std::make_unique<DelayEditor> (*this); }

//==============================================================================
// Modulación: chorus (dos voces), flanger (retardo corto con realimentación) y phaser (seis pasa-todo).
class InjMod : public InjFx
{
public:
    enum { pMode, pRate, pDepth, pFb, pWidth, pMix };
    static inline const char* xmlTypeName = "injmod";
    static const char* getPluginName() { return "Modulacion"; }
    InjMod (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return juce::String::fromUTF8 ("Modulación"); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Mod"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 6; }
    double getTailLength() const override { return .1; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    std::vector<Preset> presets() const override
    {
        return { { "Coro ancho", { 0.f, .8f, .55f, .2f, .9f, .4f } }, { "Chorus lento", { 0.f, .25f, .7f, 0.f, 1.f, .5f } }, { "Flanger de jet", { 1.f, .15f, .8f, .7f, .6f, .6f } },
                 { "Phaser clásico", { 2.f, .5f, .7f, .45f, .8f, .6f } }, { "Vibrato", { 0.f, 5.f, .35f, 0.f, 0.f, 1.f } } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override
    {
        sr = i.sampleRate; for (auto& d : dl) { d.prep ((int) (.06 * sr)); d.clear(); } phase = 0.f; for (auto& a : ap) for (auto& s : a) s.z = 0.f; fbS[0] = fbS[1] = 0.f;
    }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || dl[0].b.empty()) return;
        const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* io[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : fc.destBuffer->getWritePointer (0, fc.bufferStartSample) };
        const int mode = vi (pMode); const float rate = v (pRate), dep = v (pDepth), fb = v (pFb), wd = v (pWidth), mix = v (pMix), msS = (float) sr * .001f;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            phase += rate / (float) sr; if (phase > 1.f) phase -= 1.f;
            float in[2] { io[0][i], io[1][i] }, out[2];
            for (int c = 0; c < 2; ++c)
            {
                const float ph = phase + (c ? wd * .5f : 0.f), lfo = std::sin (2.f * kPi * ph), x = in[c]; float wet = 0.f;
                if (mode == 0)
                {
                    const float d1 = (14.f + dep * 5.f * lfo) * msS, d2 = (21.f + dep * 5.f * std::sin (2.f * kPi * (ph + .33f))) * msS;
                    dl[c].push (x + fbS[c] * fb * .5f); const float a = dl[c].read (d1), b = dl[c].read (d2); fbS[c] = a; wet = (a + b) * .5f; out[c] = x * (1.f - mix) + wet * mix;
                }
                else if (mode == 1)
                {
                    const float d = (.6f + (1.f + lfo) * .5f * dep * 5.5f) * msS; dl[c].push (x + fbS[c] * fb); wet = dl[c].read (d); fbS[c] = wet; out[c] = (x + wet * mix) / (1.f + mix * .4f);
                }
                else
                {
                    const float lo = 250.f, hi = lo * std::pow (16.f, .3f + .7f * dep), f = lo * std::pow (hi / lo, (1.f + lfo) * .5f), t = std::tan (kPi * std::min (f, (float) sr * .45f) / (float) sr), a = (t - 1.f) / (t + 1.f);
                    float y = x + fbS[c] * fb; for (int s = 0; s < 6; ++s) y = ap[c][s].run (y, a); fbS[c] = y; wet = y; out[c] = x * (1.f - mix * .5f) + wet * mix * .5f;
                }
            }
            io[0][i] = out[0]; if (nc > 1) io[1][i] = out[1];
        }
        meterA.store (phase);
    }
private:
    static inline const Spec S[] = {
        { "mode", "Tipo", 0.f, 2.f, 0.f, 0.f, fChoice, "Chorus|Flanger|Phaser" }, { "rate", "Velocidad", .05f, 10.f, .8f, 1.f, fNum, "" }, { "depth", "Profundidad", 0.f, 1.f, .55f, 0.f, fPct, "" },
        { "fb", "Realimentación", 0.f, .9f, .2f, 0.f, fPct, "" }, { "width", "Ancho", 0.f, 1.f, .9f, 0.f, fPct, "" }, { "mix", "Mezcla", 0.f, 1.f, .4f, 0.f, fPct, "" } };
    double sr = 44100.0; Delay dl[2]; Allpass1 ap[2][6]; float phase = 0.f, fbS[2] {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjMod)
};

class ModEditor : public FxEditor
{
public:
    ModEditor (InjMod& m) : FxEditor (m, 600, 300, "M3 12c3-8 6-8 9 0s6 8 9 0"), mod (m)
    {
        pills (InjMod::pMode, { 16, 64, 568, 34 }); knobRow ({ InjMod::pRate, InjMod::pDepth, InjMod::pFb, InjMod::pWidth, InjMod::pMix }, 170);
    }
protected:
    void tick() override { ph = mod.meterA.load(); }
    void paintBody (juce::Graphics& g) override
    {
        const auto r = juce::Rectangle<float> (16.f, 110.f, 568.f, 48.f); plotBox (g, r); const float cyc = std::max (.5f, std::min (6.f, fx.v (InjMod::pRate) * 1.6f)); juce::Path p;
        for (int x = 0; x <= 568; x += 2) { const float y = r.getCentreY() - std::sin (2.f * kPi * (x / 568.f * cyc)) * r.getHeight() * .36f; if (x == 0) p.startNewSubPath (r.getX(), y); else p.lineTo (r.getX() + x, y); }
        g.setColour (theme::accent); g.strokePath (p, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float px = r.getX() + std::fmod (ph * cyc, 1.f) / cyc * 568.f; const float py = r.getCentreY() - std::sin (2.f * kPi * (std::fmod (ph * cyc, 1.f) / cyc * cyc)) * r.getHeight() * .36f;
        g.setColour (theme::text); g.fillEllipse (px - 4.5f, py - 4.5f, 9.f, 9.f);
    }
private:
    InjMod& mod; float ph = 0.f;
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjMod::createEditor() { return std::make_unique<ModEditor> (*this); }
} // namespace injfx
