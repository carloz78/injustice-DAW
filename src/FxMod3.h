#pragma once
#include "FxEcho.h"
// Phaser (doble fasor con dos generadores de barrido), Flanger (modo flanger / doubler) y Chorus (cuatro modos de dimension con medidor), con los paneles del usuario.
namespace injfx
{
// ------------------------------------------------------------------ Phaser
class InjPhaserDual : public InjFx
{
public:
    enum { pRate1, pMan1, pShape1, pRate2, pMan2, pShape2, pDepthA, pFbA, pSwA, pDepthB, pFbB, pSwB, pSync, pIn, pPower, pPed };
    static inline const char* xmlTypeName = "injphaserdual";
    static const char* getPluginName() { return "Phaser"; }
    InjPhaserDual (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Phaser"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 16; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    std::vector<Preset> presets() const override
    {   //                  R1  M1 Sh1 R2  M2 Sh2 DA  FA  SwA DB  FB  SwB Syn In  Pw Ped
        return { { "Clásico lento", { .4f, 0.f, 0.f, .7f, 0.f, 0.f, 6.f, 3.f, 0.f, 6.f, 3.f, 1.f, 0.f, 2.f, 1.f, .5f } }, { "Jet", { 1.f, 0.f, 0.f, .3f, 0.f, 0.f, 9.f, 8.f, 0.f, 8.f, 7.f, 0.f, 0.f, 0.f, 1.f, .5f } },
                 { "Doble barrido", { .5f, 0.f, 0.f, 1.3f, 0.f, 0.f, 7.f, 5.f, 0.f, 7.f, 5.f, 1.f, 1.f, 1.f, 1.f, .5f } }, { "Pulso cuadrado", { 2.f, 0.f, 1.f, 2.f, 0.f, 0.f, 8.f, 4.f, 0.f, 5.f, 4.f, 1.f, 0.f, 2.f, 1.f, .5f } },
                 { "Pedal fijo", { 1.f, 0.f, 0.f, 1.f, 0.f, 0.f, 7.f, 6.f, 2.f, 7.f, 6.f, 2.f, 0.f, 0.f, 1.f, .35f } }, { "Sutil", { .2f, 0.f, 0.f, .2f, 0.f, 0.f, 4.f, 1.f, 0.f, 3.f, 1.f, 1.f, 0.f, 2.f, 1.f, .5f } } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; ph1 = 0.0; ph2 = 0.0; for (auto& c : z) for (auto& a : c) for (auto& b : a) b = 0.f; last[0][0] = last[0][1] = last[1][0] = last[1][1] = 0.f; }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nullptr }; p[1] = nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : p[0];
        const int n = fc.bufferNumSamples; const bool on = vi (pPower) != 0, sq1 = vi (pShape1) != 0, sq2 = vi (pShape2) != 0, ped1 = vi (pMan1) != 0, ped2 = vi (pMan2) != 0, rev = vi (pSync) != 0; const int swA = vi (pSwA), swB = vi (pSwB), inB = vi (pIn);
        const double d1 = 2.0 * juce::MathConstants<double>::pi * v (pRate1) / sr, d2 = 2.0 * juce::MathConstants<double>::pi * v (pRate2) / sr;
        const float spanA = v (pDepthA) * .1f * 6.f, spanB = v (pDepthB) * .1f * 6.f, fbA = v (pFbA) * .1f * .88f, fbB = v (pFbB) * .1f * .88f, pedV = v (pPed);
        const float pw = on ? 1.f : 0.f; float l1 = 0.f, l2 = 0.f;
        for (int i = 0; i < n; ++i)
        {
            auto shape = [] (double ph, bool sqr) { const float s = (float) std::sin (ph); return sqr ? .5f + .5f * std::tanh (4.f * s) : .5f + .5f * s; };
            float g1 = ped1 ? pedV : shape (ph1, sq1), g2 = ped2 ? pedV : shape (ph2, sq2); if (rev) g2 = 1.f - g2; ph1 += d1; ph2 += d2; if (ph1 > 6.2831853) ph1 -= 6.2831853; if (ph2 > 6.2831853) ph2 -= 6.2831853; l1 = g1; l2 = g2;
            const float sA = swA == 0 ? g1 : swA == 1 ? g2 : pedV, sB = swB == 0 ? g1 : swB == 1 ? g2 : pedV;
            const float aA = coef (800.f * std::exp2 ((sA - .5f) * spanA)), aB = coef (800.f * std::exp2 ((sB - .5f) * spanB));
            for (int c = 0; c < 2; ++c)
            {
                if (c == 1 && nc < 2) { p[1][i] = p[0][i]; break; }
                const float x = p[c][i]; const float yA = stage (c, 0, x, aA, fbA); const float inb = inB == 0 ? yA : inB == 1 ? x : .5f * (x + yA); const float yB = stage (c, 1, inb, aB, fbB);
                const float oA = .5f * (x + yA), oB = .5f * (inb + yB); p[c][i] = x + (.5f * (oA + oB) - x) * pw;
            }
        }
        led1.store (l1); led2.store (l2);
    }
    std::atomic<float> led1 { 0.f }, led2 { 0.f };
private:
    float coef (float f) const { const float t = std::tan (juce::MathConstants<float>::pi * juce::jlimit (20.f, (float) sr * .45f, f) / (float) sr); return (t - 1.f) / (t + 1.f); }
    float stage (int c, int ph, float x, float a, float fb)
    {
        float y = x + std::tanh (fb * last[ph][c]);
        for (int k = 0; k < 6; ++k) { const float o = a * y + z[ph][c][k]; z[ph][c][k] = y - a * o; y = o; }
        last[ph][c] = y; return y;
    }
    static inline const Spec S[] = {
        { "rate1", "Gen 1 · velocidad", .1f, 10.f, 1.f, 1.f, fNum, "" }, { "man1", "Gen 1 · modo", 0.f, 1.f, 0.f, 0.f, fChoice, "Man|Ped" }, { "shape1", "Gen 1 · forma", 0.f, 1.f, 0.f, 0.f, fChoice, "Senoidal|Cuadrada" },
        { "rate2", "Gen 2 · velocidad", .1f, 10.f, 1.f, 1.f, fNum, "" }, { "man2", "Gen 2 · modo", 0.f, 1.f, 0.f, 0.f, fChoice, "Man|Ped" }, { "shape2", "Gen 2 · forma", 0.f, 1.f, 0.f, 0.f, fChoice, "Senoidal|Cuadrada" },
        { "depthA", "A · profundidad", 0.f, 10.f, 6.f, 0.f, fNum, "" }, { "fbA", "A · realimentación", 0.f, 10.f, 3.f, 0.f, fNum, "" }, { "swA", "A · barrido", 0.f, 2.f, 0.f, 0.f, fChoice, "Gen 1|Gen 2|Ped" },
        { "depthB", "B · profundidad", 0.f, 10.f, 6.f, 0.f, fNum, "" }, { "fbB", "B · realimentación", 0.f, 10.f, 3.f, 0.f, fNum, "" }, { "swB", "B · barrido", 0.f, 2.f, 1.f, 0.f, fChoice, "Gen 1|Gen 2|Ped" },
        { "sync", "Sweep sync", 0.f, 1.f, 0.f, 0.f, fChoice, "Norm|Rev" }, { "in", "B · entrada", 0.f, 2.f, 2.f, 0.f, fChoice, "A|B|A+B" }, { "power", "Power", 0.f, 1.f, 1.f, 0.f, fChoice, "Apagado|Encendido" },
        { "ped", "Posición del pedal", 0.f, 1.f, .5f, 0.f, fPct, "" } };
    double sr = 44100.0, ph1 = 0.0, ph2 = 0.0; float z[2][2][6] {}, last[2][2] {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjPhaserDual)
};

// ------------------------------------------------------------------ Flanger
class InjFlangerPro : public InjFx
{
public:
    enum { pManual, pWidth, pSpeed, pMix, pRegen, pInvert, pMode };
    static inline const char* xmlTypeName = "injflangerpro";
    static const char* getPluginName() { return "Flanger"; }
    InjFlangerPro (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Flanger"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 7; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    std::vector<Preset> presets() const override
    {   //                 Man Wid Spd Mix Reg Inv Modo
        return { { "Jet clásico", { 4.f, 7.f, 2.f, 5.f, 6.f, 0.f, 0.f } }, { "Flanger suave", { 5.f, 4.f, 1.f, 4.f, 3.f, 0.f, 0.f } }, { "Metálico", { 2.f, 5.f, 3.f, 5.f, 9.f, 0.f, 0.f } },
                 { "Negativo", { 3.f, 6.f, 2.f, 5.f, 7.f, 1.f, 0.f } }, { "Doblaje", { 4.f, 3.f, 3.f, 5.f, 0.f, 0.f, 1.f } }, { "Barrido lento", { 6.f, 9.f, .5f, 5.f, 5.f, 0.f, 0.f } } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; for (auto& b : buf) std::fill (std::begin (b), std::end (b), 0.f); w = 0; dSm = 1.f; ph = 0.0; fbs[0] = fbs[1] = 0.f; }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nullptr }; p[1] = nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : p[0];
        const int n = fc.bufferNumSamples; const bool dbl = vi (pMode) != 0, inv = vi (pInvert) != 0; const float m = v (pMix) * .1f, dry = std::min (1.f, 2.f * (1.f - m)), wet = std::min (1.f, 2.f * m) * (inv ? -1.f : 1.f);
        const float wid = v (pWidth) * .1f, man = v (pManual);
        const double speed = (dbl ? .3 : 1.0) * .05 * std::pow (100.0, v (pSpeed) * .1); const double dPh = 2.0 * juce::MathConstants<double>::pi * speed / sr;
        const float d0ms = dbl ? 12.f + 28.f * man * .1f : .25f * std::pow (32.f, man * .1f), fb = dbl ? 0.f : v (pRegen) * .1f * .92f * (inv ? -1.f : 1.f), sm = 1.f - std::exp (-1.f / (float) (.03 * sr));
        for (int i = 0; i < n; ++i)
        {
            const float lfo = (float) std::sin (ph); ph += dPh; if (ph > 6.2831853) ph -= 6.2831853;
            const float depth = dbl ? wid * .12f : wid * .9f; const float target = std::max (.04f, d0ms * (1.f + depth * lfo)); dSm += (target - dSm) * sm;
            const float dl = std::min (dSm * (float) sr * .001f, 8000.f); const int i0 = (int) dl; const float fr = dl - (float) i0;
            for (int c = 0; c < 2; ++c)
            {
                if (c == 1 && nc < 2) { p[1][i] = p[0][i]; break; }
                const float x = p[c][i]; const float a = buf[c][(w - i0) & MASK], b = buf[c][(w - i0 - 1) & MASK], d = a + (b - a) * fr;
                buf[c][w & MASK] = x + std::tanh (d * fb) + 1e-18f; p[c][i] = x * dry + d * wet;
            }
            ++w;
        }
    }
private:
    static constexpr int MASK = 16383;
    static inline const Spec S[] = {
        { "manual", "Manual", 0.f, 10.f, 4.f, 0.f, fNum, "" }, { "width", "Width", 0.f, 10.f, 6.f, 0.f, fNum, "" }, { "speed", "Speed", 0.f, 10.f, 2.f, 0.f, fNum, "" }, { "mix", "Mix", 0.f, 10.f, 5.f, 0.f, fNum, "" },
        { "regen", "Regen", 0.f, 10.f, 5.f, 0.f, fNum, "" }, { "invert", "Invert", 0.f, 1.f, 0.f, 0.f, fChoice, "Normal|Invertido" }, { "mode", "Modo", 0.f, 1.f, 0.f, 0.f, fChoice, "Flanger|Doubler" } };
    double sr = 44100.0, ph = 0.0; float buf[2][MASK + 1] {}; int w = 0; float dSm = 1.f, fbs[2] {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjFlangerPro)
};

// ------------------------------------------------------------------ Chorus
class InjChorusDim : public InjFx
{
public:
    enum { pMode, pBypass, pPower };
    static inline const char* xmlTypeName = "injchorusdim";
    static const char* getPluginName() { return "Chorus"; }
    InjChorusDim (te::PluginCreationInfo i) : InjFx (i) { buildParams(); meterA.store (-90.f); meterB.store (-90.f); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Chorus"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 3; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    std::vector<Preset> presets() const override { return { { "Dimensión 1", { 1.f, 0.f, 1.f } }, { "Dimensión 2", { 2.f, 0.f, 1.f } }, { "Dimensión 3", { 3.f, 0.f, 1.f } }, { "Dimensión 4", { 4.f, 0.f, 1.f } } }; }
    void initialise (const te::PluginInitialisationInfo& i) override
    {
        sr = i.sampleRate; for (auto& b : buf) std::fill (std::begin (b), std::end (b), 0.f); w = 0; ph = 0.0; lp[0] = lp[1] = 0.f; act = 0.f; env = 0.f; meterA.store (-90.f); meterB.store (-90.f); lpA = 1.f - std::exp (-2.f * juce::MathConstants<float>::pi * 8000.f / (float) sr);
    }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nullptr }; p[1] = nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : p[0];
        static const double rate[5] { 0.0, .25, .38, .6, .95 }, depthMs[5] { 0.0, .3, .5, .8, 1.3 }; static const float wetG[5] { 0.f, .45f, .55f, .65f, .75f };
        const int mode = juce::jlimit (0, 4, vi (pMode)); const bool active = vi (pPower) != 0 && vi (pBypass) == 0 && mode > 0; const float tgt = active ? 1.f : 0.f, sm = 1.f - std::exp (-1.f / (float) (.015 * sr));
        const double dPh = 2.0 * juce::MathConstants<double>::pi * rate[mode] / sr; const float dm = (float) (depthMs[mode] * sr * .001), base = (float) (5.0 * sr * .001), ea = coefMs (350.f, sr);
        for (int i = 0; i < n_(fc); ++i)
        {
            act += (tgt - act) * sm; const float lfo = (float) std::sin (ph); if (mode > 0) { ph += dPh; if (ph > 6.2831853) ph -= 6.2831853; }
            const float mono = nc > 1 ? .5f * (p[0][i] + p[1][i]) : p[0][i]; buf[0][w & MASK] = mono; const float dA = base + dm * lfo, dB = base - dm * lfo; float wets[2];
            for (int c = 0; c < 2; ++c) { const float d = c == 0 ? dA : dB; const int i0 = (int) d; const float fr = d - (float) i0; const float a = buf[0][(w - i0) & MASK], b = buf[0][(w - i0 - 1) & MASK]; lp[c] += lpA * ((a + (b - a) * fr) - lp[c]); wets[c] = lp[c]; }
            for (int c = 0; c < nc; ++c) { const float x = p[c][i]; p[c][i] = x + wets[c] * wetG[mode] * act; }
            ++w;
            const float ao = std::abs (p[0][i]); env = ea * env + (1.f - ea) * ao;
        }
        const float db = lin2db (env * 1.11f + 1e-9f); meterA.store (db); meterB.store (db);
    }
private:
    static int n_ (const te::PluginRenderContext& fc) { return fc.bufferNumSamples; }
    static constexpr int MASK = 8191;
    static inline const Spec S[] = { { "mode", "Dimension mode", 0.f, 4.f, 2.f, 0.f, fChoice, "Off|1|2|3|4" }, { "bypass", "Bypass", 0.f, 1.f, 0.f, 0.f, fChoice, "Off|On" }, { "power", "Power", 0.f, 1.f, 1.f, 0.f, fChoice, "Apagado|Encendido" } };
    double sr = 44100.0, ph = 0.0; float buf[1][MASK + 1] {}; int w = 0; float lp[2] {}, act = 0.f, env = 0.f, lpA = .5f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjChorusDim)
};
// ------------------------------------------------------------------ editores
inline void presetPopup (InjFx& fx, juce::Component* owner)
{
    juce::PopupMenu mn; auto ps = fx.presets(); mn.addItem (1, "Inicial"); mn.addSeparator();
    for (size_t i = 0; i < ps.size(); ++i) mn.addItem (100 + (int) i, juce::String::fromUTF8 (ps[i].name), true, fx.presetName() == juce::String::fromUTF8 (ps[i].name));
    juce::Component::SafePointer<juce::Component> o (owner); InjFx* f = &fx;
    mn.showMenuAsync (juce::PopupMenu::Options(), [o, f] (int r) { if (o == nullptr) return; if (r == 1) f->resetAll(); else if (r >= 100) f->applyPreset (r - 100); o->repaint(); });
}
class PhaserEditor : public SkinEditor
{
public:
    PhaserEditor (InjPhaserDual& p) : SkinEditor (p, skinImage ("ph_bg_png"), .55f), ph (p)
    {
        using P = InjPhaserDual; const float cx[6] { 611.f, 1286.7f, 267.f, 508.3f, 1232.3f, 1459.7f }, cy[6] { 217.f, 216.7f, 448.3f, 448.7f, 449.f, 447.7f }, rot[6] { 24.f, 22.f, 27.f, 24.f, 18.f, 17.f };
        const int prm[6] { P::pRate1, P::pRate2, P::pDepthA, P::pFbA, P::pDepthB, P::pFbB };
        for (int k = 0; k < 6; ++k)
        {
            Knob kn { prm[k], cx[k], cy[k], 46.f, skinImage (("ph_cap" + juce::String (k) + "_png").toRawUTF8()) }; kn.a0 = -150.f; kn.a1 = 150.f; kn.rot0 = rot[k];
            if (k < 2) { kn.cv = { .1f, .2f, .5f, 1.f, 2.f, 5.f, 10.f }; kn.ca = { -150.f, -100.f, -50.f, 0.f, 50.f, 100.f, 150.f }; }
            knobs.push_back (kn);
        }
        for (auto* n : { "ph_man1", "ph_man2", "ph_sync" }) { tg.push_back ({ skinImage ((juce::String (n) + "_up_png").toRawUTF8()), skinImage ((juce::String (n) + "_dn_png").toRawUTF8()) }); }
        sh[0] = skinImage ("ph_sh1_a_png"); sh[1] = skinImage ("ph_sh1_b_png"); sh[2] = skinImage ("ph_sh2_a_png"); sh[3] = skinImage ("ph_sh2_b_png");
        pwOn = skinImage ("ph_pw_on_png"); pwOff = skinImage ("ph_pw_off_png"); led = skinImage ("ph_led_png");
        const char* rk[3] { "ph_rkA", "ph_rkB", "ph_rkI" }; for (int k = 0; k < 3; ++k) for (const char* sfx : { "_t", "_m", "_b" }) rkI[k].push_back (skinImage ((juce::String (rk[k]) + sfx + "_png").toRawUTF8()));
        auto tgl = [this] (float x, float y, int prm) { hots.push_back ({ { x - 30.f, y - 42.f, 60.f, 84.f }, [this, prm] { choose (prm, ph.vi (prm) ? 0 : 1); } }); };
        tgl (816.5f, 210.f, P::pMan1); tgl (1496.5f, 208.f, P::pMan2); tgl (983.f, 445.f, P::pSync);
        hots.push_back ({ { 1012.f - 26.f, 215.f - 18.f, 52.f, 36.f }, [this] { choose (P::pShape1, ph.vi (P::pShape1) ? 0 : 1); } }); hots.push_back ({ { 1687.5f - 26.f, 214.f - 18.f, 52.f, 36.f }, [this] { choose (P::pShape2, ph.vi (P::pShape2) ? 0 : 1); } });
        hots.push_back ({ { 1902.f - 40.f, 193.f - 50.f, 80.f, 100.f }, [this] { choose (P::pPower, ph.vi (P::pPower) ? 0 : 1); } });
        const float rx[3] { 704.5f, 1636.5f, 1838.5f }; const int rp[3] { P::pSwA, P::pSwB, P::pIn };
        for (int k = 0; k < 3; ++k) for (int j = 0; j < 3; ++j) hots.push_back ({ { rx[k] - 24.f, 444.5f - 42.f + 28.f * (float) j, 48.f, 28.f }, [this, k, j, rp] { choose (rp[k], j); } });
        hots.push_back ({ { 135.f, 195.f, 290.f, 80.f }, [this] { presetPopup (ph, this); } });
    }
protected:
    void paintSkin (juce::Graphics& g) override
    {
        using P = InjPhaserDual; const bool on = ph.vi (P::pPower) != 0;
        const float tx[3] { 816.5f, 1496.5f, 983.f }, ty[3] { 210.f, 208.f, 445.f }; const int tp[3] { P::pMan1, P::pMan2, P::pSync };
        for (int k = 0; k < 3; ++k) drawSprite (g, ph.vi (tp[k]) ? tg[(size_t) k].second : tg[(size_t) k].first, tx[k], ty[k]);
        drawSprite (g, sh[ph.vi (P::pShape1) ? 1 : 0], 1012.f, 215.f); drawSprite (g, sh[ph.vi (P::pShape2) ? 3 : 2], 1687.5f, 214.f);
        drawSprite (g, on ? pwOn : pwOff, 1902.f, 193.f);
        const float rx[3] { 704.5f, 1636.5f, 1838.5f }; const int rp[3] { P::pSwA, P::pSwB, P::pIn };
        for (int k = 0; k < 3; ++k) drawSprite (g, rkI[k][(size_t) juce::jlimit (0, 2, ph.vi (rp[k]))], rx[k], 444.5f);
        if (on) { if (ph.led1.load() > .5f) drawLed (g, led, 0, 1082.f, 98.f); if (ph.led2.load() > .5f) drawLed (g, led, 1, 1752.f, 98.f); }
    }
private:
    InjPhaserDual& ph; std::vector<std::pair<juce::Image, juce::Image>> tg; juce::Image sh[4], pwOn, pwOff, led; std::vector<juce::Image> rkI[3];
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjPhaserDual::createEditor() { return std::make_unique<PhaserEditor> (*this); }

class FlangerEditor : public SkinEditor
{
public:
    FlangerEditor (InjFlangerPro& p) : SkinEditor (p, skinImage ("fl_bg_png"), .55f), fl (p)
    {
        using P = InjFlangerPro; const float cx[5] { 618.3f, 829.f, 1017.7f, 1232.3f, 1449.3f }, cy[5] { 201.5f, 202.f, 202.f, 201.f, 201.f }, rot[5] { 3.f, 27.f, 27.f, -2.f, 16.f }; const int prm[5] { P::pManual, P::pWidth, P::pSpeed, P::pMix, P::pRegen };
        for (int k = 0; k < 5; ++k) { Knob kn { prm[k], cx[k], cy[k], 52.f, skinImage (("fl_cap" + juce::String (k) + "_png").toRawUTF8()) }; kn.a0 = -150.f; kn.a1 = 150.f; kn.rot0 = rot[k]; knobs.push_back (kn); }
        cream = skinImage ("fl_cream_png"); grey = skinImage ("fl_grey_png"); invUp = skinImage ("fl_inv_up_png"); invDn = skinImage ("fl_inv_dn_png"); led = skinImage ("fl_led_png");
        hots.push_back ({ { 314.f - 38.f, 237.5f - 33.f, 76.f, 66.f }, [this] { choose (P::pMode, 0); } }); hots.push_back ({ { 447.f - 38.f, 237.5f - 33.f, 76.f, 66.f }, [this] { choose (P::pMode, 1); } });
        hots.push_back ({ { 1618.f - 28.f, 209.f - 42.f, 56.f, 84.f }, [this] { choose (P::pInvert, fl.vi (P::pInvert) ? 0 : 1); } });
        hots.push_back ({ { 1720.f, 60.f, 260.f, 80.f }, [this] { presetPopup (fl, this); } });
    }
protected:
    void paintSkin (juce::Graphics& g) override
    {
        using P = InjFlangerPro; const bool d = fl.vi (P::pMode) != 0;
        drawSprite (g, d ? grey : cream, 314.f, 237.5f); drawSprite (g, d ? cream : grey, 447.f, 237.5f); drawSprite (g, fl.vi (P::pInvert) ? invDn : invUp, 1618.f, 209.f); drawLed (g, led, 0, 171.f, 218.f);
    }
private:
    InjFlangerPro& fl; juce::Image cream, grey, invUp, invDn, led;
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjFlangerPro::createEditor() { return std::make_unique<FlangerEditor> (*this); }

class ChorusEditor : public SkinEditor
{
public:
    ChorusEditor (InjChorusDim& p) : SkinEditor (p, skinImage ("ch_bg_png"), .55f), ch (p)
    {
        using P = InjChorusDim; red = skinImage ("ch_red_png"); cream = skinImage ("ch_cream_png"); bpUp = skinImage ("ch_bp_up_png"); bpDn = skinImage ("ch_bp_dn_png"); pwUp = skinImage ("ch_pw_up_png"); pwDn = skinImage ("ch_pw_dn_png"); led = skinImage ("ch_led_png"); seg = skinImage ("ch_seg_png");
        for (int k = 0; k < 5; ++k) hots.push_back ({ { bx[k] - 40.f, 143.5f - 25.f, 80.f, 50.f }, [this, k] { choose (P::pMode, k); } });
        hots.push_back ({ { 165.5f - 40.f, 161.5f - 60.f, 80.f, 120.f }, [this] { choose (P::pBypass, ch.vi (P::pBypass) ? 0 : 1); } });
        hots.push_back ({ { 1911.f - 41.f, 206.5f - 59.f, 82.f, 118.f }, [this] { choose (P::pPower, ch.vi (P::pPower) ? 0 : 1); } });
        hots.push_back ({ { 1510.f, 130.f, 300.f, 90.f }, [this] { presetPopup (ch, this); } });
    }
protected:
    void tick() override
    {
        const float db = ch.vi (InjChorusDim::pPower) ? ch.meterA.load() : -90.f; lvl = db > lvl ? db : lvl - .8f; repaint();
    }
    void paintSkin (juce::Graphics& g) override
    {
        using P = InjChorusDim; const bool on = ch.vi (P::pPower) != 0; const int m = ch.vi (P::pMode);
        for (int k = 0; k < 5; ++k) drawSprite (g, k == m ? red : cream, bx[k], 143.5f);
        drawSprite (g, ch.vi (P::pBypass) ? bpUp : bpDn, 165.5f, 161.5f); drawSprite (g, on ? pwUp : pwDn, 1911.f, 206.5f);
        if (on) { drawLed (g, led, 0, 1912.f, 108.f); if (m > 0 && ! ch.vi (P::pBypass)) drawLed (g, led, 1, 739.f, 260.f); }
        static const float sy[7] { 93.f, 120.5f, 144.f, 168.f, 201.5f, 237.f, 266.5f }, th[7] { -4.f, -9.f, -12.f, -14.f, -19.f, -29.f, -39.f };
        if (on) for (int k = 0; k < 7; ++k) if (lvl >= th[k]) drawLed (g, seg, k, 1286.f, sy[k]);
    }
private:
    const float bx[5] { 516.5f, 605.f, 694.5f, 783.5f, 871.5f };
    InjChorusDim& ch; juce::Image red, cream, bpUp, bpDn, pwUp, pwDn, led, seg; float lvl = -90.f;
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjChorusDim::createEditor() { return std::make_unique<ChorusEditor> (*this); }
} // namespace injfx
