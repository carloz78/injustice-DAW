#pragma once
#include "FxStack.h"
// Tube Saturation: dos canales (izquierdo y derecho) de saturación por válvulas con DRIVE, BIAS, OUTPUT, tipo de distorsión (T, P1, P2),
// filtro (apagado, graves 60 Hz fuera, agudos 6 kHz fuera), OVERDRIVE, bypass por canal y dos medidores de corriente (mA).
namespace injfx
{
class InjTubeSat : public InjFx
{
public:
    static constexpr int perCh = 7, pPower = 14;
    enum { oDrive, oBias, oOut, oDist, oFilt, oOver, oByp };
    static constexpr int P (int ch, int o) { return ch * perCh + o; }
    static inline const char* xmlTypeName = "injtubesat";
    static const char* getPluginName() { return "Tube Saturation"; }
    InjTubeSat (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "TubeSat"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 15; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    std::vector<Preset> presets() const override
    {   //  Ch1: Drive Bias Out Dist Filt Over Byp | Ch2 igual | Power
        auto both = [] (float d, float b, float o, float di, float fi, float ov) { return std::vector<float> { d, b, o, di, fi, ov, 0.f, d, b, o, di, fi, ov, 0.f, 1.f }; };
        return { { "Calidez suave", both (3.f, 5.f, 5.f, 0.f, 0.f, 0.f) }, { "Válvula caliente", both (6.f, 6.f, 4.f, 0.f, 0.f, 0.f) }, { "Pentodo con mordida", both (6.f, 5.f, 4.f, 1.f, 1.f, 0.f) },
                 { "Crunch para guitarras y leads", both (7.f, 5.f, 3.f, 2.f, 2.f, 0.f) }, { "Overdrive sucio", both (8.f, 4.f, 3.f, 2.f, 1.f, 1.f) }, { "Brillo de master", both (2.f, 5.f, 5.f, 0.f, 0.f, 0.f) } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; for (int c = 0; c < 2; ++c) { hp[c].reset(); lp[c].reset(); dc[c] = 0.f; env[c] = 0.f; } }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || vi (pPower) == 0) return;
        const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nullptr }; p[1] = nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : p[0];
        const int n = fc.bufferNumSamples; const float rel = coefMs (300.f, sr), dcA = coefMs (60.f, sr);
        for (int c = 0; c < (nc > 1 ? 2 : 1); ++c)
        {
            if (vi (P (c, oByp)) != 0) { env[c] *= .9f; continue; }
            const bool od = vi (P (c, oOver)) != 0; const int type = vi (P (c, oDist)), filt = vi (P (c, oFilt));
            const float drv = db2lin (v (P (c, oDrive)) * 3.f + (od ? 10.f : 0.f)), bias = (v (P (c, oBias)) - 5.f) / 5.f * .35f;
            const float outG = db2lin ((v (P (c, oOut)) - 5.f) * 3.f - v (P (c, oDrive)) * .8f - (od ? 3.f : 0.f));
            hp[c].set (BQ::HP, sr, filt == 1 ? 60.0 : 10.0, .707, 0.0); lp[c].set (BQ::LP, sr, filt == 2 ? 6000.0 : (double) sr * .45, .707, 0.0);
            auto shape = [type] (float x) { switch (type) { case 0: return std::tanh (x); case 1: return x / (1.f + std::abs (x)) * 1.2f; default: { const float a = juce::jlimit (-1.118f, 1.118f, x); return (a - a * a * a * .8f / 3.f) * 1.3f; } } };
            const float dcOff = shape (bias); float m = env[c];
            for (int i = 0; i < n; ++i)
            {
                float x = hp[c].run (c, p[c][i]) * drv; const float ax = std::abs (x); m = ax > m ? ax : rel * m + (1.f - rel) * ax;
                float y = shape (x + bias) - dcOff; dc[c] = dcA * dc[c] + (1.f - dcA) * y; y -= dc[c]; y = lp[c].run (c, y);
                p[c][i] = y * outG;
            }
            env[c] = m; (c == 0 ? meterA : meterB).store (10.f * std::tanh (m * .55f));
        }
    }
private:
    static inline const Spec S[] = {
        { "d1", "Canal 1: Drive", 0.f, 10.f, 5.f, 0.f, fNum, "" }, { "b1", "Canal 1: Bias", 0.f, 10.f, 5.f, 0.f, fNum, "" }, { "o1", "Canal 1: Output", 0.f, 10.f, 5.f, 0.f, fNum, "" }, { "t1", "Canal 1: Distorsión", 0.f, 2.f, 0.f, 0.f, fChoice, "T|P1|P2" },
        { "f1", "Canal 1: Filtro", 0.f, 2.f, 0.f, 0.f, fChoice, "Apagado|60 Hz|6 kHz" }, { "v1", "Canal 1: Overdrive", 0.f, 1.f, 0.f, 0.f, fChoice, "No|Sí" }, { "y1", "Canal 1: Bypass", 0.f, 1.f, 0.f, 0.f, fChoice, "No|Sí" },
        { "d2", "Canal 2: Drive", 0.f, 10.f, 5.f, 0.f, fNum, "" }, { "b2", "Canal 2: Bias", 0.f, 10.f, 5.f, 0.f, fNum, "" }, { "o2", "Canal 2: Output", 0.f, 10.f, 5.f, 0.f, fNum, "" }, { "t2", "Canal 2: Distorsión", 0.f, 2.f, 0.f, 0.f, fChoice, "T|P1|P2" },
        { "f2", "Canal 2: Filtro", 0.f, 2.f, 0.f, 0.f, fChoice, "Apagado|60 Hz|6 kHz" }, { "v2", "Canal 2: Overdrive", 0.f, 1.f, 0.f, 0.f, fChoice, "No|Sí" }, { "y2", "Canal 2: Bypass", 0.f, 1.f, 0.f, 0.f, fChoice, "No|Sí" },
        { "pw", "Encendido", 0.f, 1.f, 1.f, 0.f, fChoice, "Apagado|Encendido" } };
    double sr = 44100.0; BQ hp[2], lp[2]; float dc[2] {}, env[2] {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjTubeSat)
};

class TubeSatEditor : public SkinEditor
{
public:
    TubeSatEditor (InjTubeSat& p) : SkinEditor (p, skinImage ("ts_bg_png"), .62f), ts (p)
    {
        // perillas: Drive, Bias, Output de cada canal (centros medidos sobre la imagen)
        const float cx[6] { 304.9f, 719.2f, 713.9f, 304.9f, 718.f, 712.5f }, cy[6] { 301.2f, 301.3f, 521.6f, 1088.7f, 1087.8f, 1309.4f }, rot[6] { 9.f, -11.f, -3.f, 11.f, -11.f, 0.f }, r[6] { 57.f, 57.f, 36.f, 57.f, 57.f, 36.f }, span[6] { 157.f, 157.f, 145.f, 157.f, 157.f, 145.f };
        for (int k = 0; k < 6; ++k) { const int ch = k / 3, o = k % 3; Knob kn { InjTubeSat::P (ch, o), cx[k], cy[k], r[k], skinImage (("ts_cap" + juce::String (k) + "_png").toRawUTF8()) }; kn.a0 = -span[k]; kn.a1 = span[k]; kn.rot0 = rot[k]; knobs.push_back (kn); }
        tg[0] = skinImage ("ts_tgL_png"); tg[1] = skinImage ("ts_tgC_png"); tg[2] = skinImage ("ts_tgR_png"); ob[0] = skinImage ("ts_obL_png"); ob[1] = skinImage ("ts_obR_png"); pwUp = skinImage ("ts_pw_up_png"); pwDn = skinImage ("ts_pw_dn_png"); led = skinImage ("ts_led_png");
        for (int ch = 0; ch < 2; ++ch)
        {
            const float dy = ch * 786.5f;
            // distorsión: T / P1 / P2 (tres posiciones: pulsar a la izquierda, centro o derecha de la palanca); filtro igual
            for (int k = 0; k < 3; ++k) { hots.push_back ({ { 511.7f - 52.f + 36.f * (float) k, 324.3f + dy - 34.f, 36.f, 68.f }, [this, ch, k] { choose (InjTubeSat::P (ch, InjTubeSat::oDist), k); } });
                                          hots.push_back ({ { 511.f - 52.f + 36.f * (float) k, 479.3f + dy - 34.f, 36.f, 68.f }, [this, ch, k] { choose (InjTubeSat::P (ch, InjTubeSat::oFilt), k); } }); }
            hots.push_back ({ { 314.3f - 24.f, 523.7f + dy - 30.f, 48.f, 60.f }, [this, ch] { toggle (InjTubeSat::P (ch, InjTubeSat::oOver)); } });
            hots.push_back ({ { 312.f + 399.f * (float) ch - 24.f, 882.3f - 30.f, 48.f, 60.f }, [this, ch] { toggle (InjTubeSat::P (ch, InjTubeSat::oByp)); } });
        }
        hots.push_back ({ { 512.3f - 30.f, 818.f - 42.f, 60.f, 84.f }, [this] { toggle (InjTubeSat::pPower); } });
        hots.push_back ({ { 340.f, 20.f, 400.f, 100.f }, [this] { presetMenu(); } });
        if (std::getenv ("INJ_PUNCHDEMO")) { ts.applyPreset (3); ts.setValue (InjTubeSat::P (1, InjTubeSat::oOver), 1.f); m[0] = 5.5f; m[1] = 8.f; demo = true; }
    }
protected:
    void tick() override { if (! demo) { m[0] = std::max (ts.meterA.load(), m[0] * .9f); m[1] = std::max (ts.meterB.load(), m[1] * .9f); } repaint(); }
    void paintSkin (juce::Graphics& g) override
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            const float dy = ch * 786.5f; auto st = [&] (int o) { return ts.vi (InjTubeSat::P (ch, o)); };
            drawSprite (g, tg[st (InjTubeSat::oDist)], 511.7f, 320.3f + dy); drawSprite (g, tg[st (InjTubeSat::oFilt)], 511.f, 475.3f + dy);
            drawSprite (g, ob[st (InjTubeSat::oOver) ? 1 : 0], 314.3f, 519.7f + dy); drawSprite (g, ob[st (InjTubeSat::oByp) ? 1 : 0], 312.f + 399.f * (float) ch, 878.3f);
            // aguja del medidor de corriente: 0 mA a la izquierda, 10 mA a la derecha (pivote bajo la ventana del medidor)
            const float px = 311.7f + 400.f * (float) ch, deg = (juce::jlimit (0.f, 10.f, m[ch]) - 5.f) * 8.12f;
            drawNeedle (g, px, 833.f, deg, 62.f, 131.f, { 205.f + 400.f * (float) ch, 679.f, 213.f, 116.f }, juce::Colour (0xff141414), 3.2f);
        }
        drawSprite (g, ts.vi (InjTubeSat::pPower) ? pwUp : pwDn, 512.3f, ts.vi (InjTubeSat::pPower) ? 813.f : 823.f); if (ts.vi (InjTubeSat::pPower)) drawLed (g, led, 0, 511.5f, 696.5f);
    }
private:
    void toggle (int idx) { choose (idx, ts.vi (idx) ? 0 : 1); }
    void presetMenu()
    {
        juce::PopupMenu mn; auto ps = ts.presets(); mn.addItem (1, "Inicial"); mn.addSeparator();
        for (size_t i = 0; i < ps.size(); ++i) mn.addItem (100 + (int) i, juce::String::fromUTF8 (ps[i].name), true, ts.presetName() == juce::String::fromUTF8 (ps[i].name));
        juce::Component::SafePointer<TubeSatEditor> sp (this);
        mn.showMenuAsync (juce::PopupMenu::Options(), [sp] (int r) { if (sp == nullptr) return; if (r == 1) sp->ts.resetAll(); else if (r >= 100) sp->ts.applyPreset (r - 100); sp->repaint(); });
    }
    InjTubeSat& ts; juce::Image tg[3], ob[2], pwUp, pwDn, led; float m[2] { 0.f, 0.f }; bool demo = false;
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjTubeSat::createEditor() { return std::make_unique<TubeSatEditor> (*this); }
} // namespace injfx
