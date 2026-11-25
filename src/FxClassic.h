#pragma once
#include "FxPulso.h"
// Classic EQ: ecualizador de cuatro bandas de consola clásica: cada banda con ganancia (falda azul) y frecuencia por pasos (tapa), pico o shelf en los extremos.
namespace injfx
{
class InjClassic : public InjFx
{
public:
    enum { pHfG, pHfF, pHmG, pHmF, pLmG, pLmF, pLfG, pLfF, pHfShelf, pLfShelf, pIn };
    static inline const char* xmlTypeName = "injclassic";
    static const char* getPluginName() { return "Classic EQ"; }
    InjClassic (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Classic"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 11; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    std::vector<Preset> presets() const override
    {   //                       HF g  HF f HM g HM f LM g LM f LF g LF f HFsh LFsh In
        return { { "Plano", { 0.f, 4.f, 0.f, 3.f, 0.f, 2.f, 0.f, 2.f, 0.f, 0.f, 1.f } }, { "Brillo y aire", { 4.f, 5.f, 2.f, 4.f, 0.f, 2.f, 0.f, 2.f, 1.f, 0.f, 1.f } }, { "Cuerpo", { 0.f, 4.f, 0.f, 3.f, 2.f, 1.f, 4.f, 1.f, 0.f, 1.f, 1.f } },
                 { "Voz presente", { 2.f, 3.f, 4.f, 2.f, -2.f, 3.f, 0.f, 2.f, 0.f, 0.f, 1.f } }, { "Limpiar lodo", { 0.f, 4.f, 0.f, 3.f, -4.f, 1.f, -3.f, 0.f, 0.f, 1.f, 1.f } } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; for (auto& b : f) b.reset(); }
    static float hfHz (int i) { static const float a[] { 3000.f, 4000.f, 6000.f, 10000.f, 12500.f, 15000.f }; return a[juce::jlimit (0, 5, i)]; }
    static float hmHz (int i) { static const float a[] { 600.f, 1000.f, 1500.f, 2000.f, 5000.f, 7000.f }; return a[juce::jlimit (0, 5, i)]; }
    static float lmHz (int i) { static const float a[] { 200.f, 300.f, 400.f, 700.f, 1000.f, 1500.f }; return a[juce::jlimit (0, 5, i)]; }
    static float lfHz (int i) { static const float a[] { 30.f, 50.f, 100.f, 300.f, 400.f, 600.f }; return a[juce::jlimit (0, 5, i)]; }
    void setFilters()
    {
        f[0].set (vi (pHfShelf) ? BQ::HighShelf : BQ::Peak, sr, hfHz (vi (pHfF)), vi (pHfShelf) ? .7 : .9, v (pHfG));
        f[1].set (BQ::Peak, sr, hmHz (vi (pHmF)), 1.0, v (pHmG));
        f[2].set (BQ::Peak, sr, lmHz (vi (pLmF)), 1.0, v (pLmG));
        f[3].set (vi (pLfShelf) ? BQ::LowShelf : BQ::Peak, sr, lfHz (vi (pLfF)), vi (pLfShelf) ? .7 : .9, v (pLfG));
    }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || vi (pIn) == 0) return;
        setFilters();
        for (int c = 0; c < std::min (2, fc.destBuffer->getNumChannels()); ++c)
        {
            float* p = fc.destBuffer->getWritePointer (c, fc.bufferStartSample);
            for (int i = 0; i < fc.bufferNumSamples; ++i) { float x = p[i]; for (auto& b : f) x = b.run (c, x); p[i] = x; }
        }
    }
private:
    static inline const Spec S[] = {
        { "hfg", "HF: Ganancia", -12.f, 12.f, 0.f, 0.f, fDb, "" }, { "hff", "HF: Frecuencia", 0.f, 5.f, 4.f, 0.f, fChoice, "3 kHz|4 kHz|6 kHz|10 kHz|12.5 kHz|15 kHz" },
        { "hmg", "HMF: Ganancia", -12.f, 12.f, 0.f, 0.f, fDb, "" }, { "hmf", "HMF: Frecuencia", 0.f, 5.f, 3.f, 0.f, fChoice, "600 Hz|1 kHz|1.5 kHz|2 kHz|5 kHz|7 kHz" },
        { "lmg", "LMF: Ganancia", -12.f, 12.f, 0.f, 0.f, fDb, "" }, { "lmf", "LMF: Frecuencia", 0.f, 5.f, 2.f, 0.f, fChoice, "200 Hz|300 Hz|400 Hz|700 Hz|1 kHz|1.5 kHz" },
        { "lfg", "LF: Ganancia", -12.f, 12.f, 0.f, 0.f, fDb, "" }, { "lff", "LF: Frecuencia", 0.f, 5.f, 2.f, 0.f, fChoice, "30 Hz|50 Hz|100 Hz|300 Hz|400 Hz|600 Hz" },
        { "hfs", "HF: Forma", 0.f, 1.f, 0.f, 0.f, fChoice, "Pico|Shelf" }, { "lfs", "LF: Forma", 0.f, 1.f, 0.f, 0.f, fChoice, "Pico|Shelf" }, { "in", "Ecualizador", 0.f, 1.f, 1.f, 0.f, fChoice, "Fuera|Dentro" } };
    double sr = 44100.0; BQ f[4];
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjClassic)
};

class ClassicEditor : public SkinEditor
{
public:
    ClassicEditor (InjClassic& p) : SkinEditor (p, skinImage ("classic_bg_png"), .5f, { 195, 0, 635, 1520 }), cl (p)
    {
        const float cx[] { 457.2f, 458.f, 458.1f, 458.1f }, cy[] { 351.4f, 612.8f, 874.1f, 1138.7f };
        const int gi[] { InjClassic::pHfG, InjClassic::pHmG, InjClassic::pLmG, InjClassic::pLfG }, fi[] { InjClassic::pHfF, InjClassic::pHmF, InjClassic::pLmF, InjClassic::pLfF };
        for (int k = 0; k < 4; ++k)
        {
            Knob sk { gi[k], cx[k], cy[k], 70.f, skinImage (("classic_sk" + juce::String (k) + "_png").toRawUTF8()) }; sk.a0 = -60.f; sk.a1 = 60.f; knobs.push_back (sk);       // falda: ganancia
            Knob cp { fi[k], cx[k], cy[k], 44.f, skinImage (("classic_cap" + juce::String (k) + "_png").toRawUTF8()) }; cp.rot0 = 1.5f; cp.stops = { -139.f, -113.f, -87.f, -62.f, -36.f, -10.f }; knobs.push_back (cp);   // tapa: frecuencia
        }
        for (int k = 0; k < 2; ++k) { tup[k] = skinImage (("classic_t" + juce::String (k) + "_up_png").toRawUTF8()); tdn[k] = skinImage (("classic_t" + juce::String (k) + "_dn_png").toRawUTF8()); }
        ledOn = skinImage ("classic_led_on_png"); ledOff = skinImage ("classic_led_off_png"); btOn = skinImage ("classic_btn_on_png"); btOff = skinImage ("classic_btn_off_png");
        hots.push_back ({ { 741.f - 44.f, 336.f - 52.f, 88.f, 104.f }, [this] { choose (InjClassic::pHfShelf, cl.vi (InjClassic::pHfShelf) ? 0 : 1); } });
        hots.push_back ({ { 741.f - 44.f, 1128.f - 52.f, 88.f, 104.f }, [this] { choose (InjClassic::pLfShelf, cl.vi (InjClassic::pLfShelf) ? 0 : 1); } });
        hots.push_back ({ { 466.f, 1288.f, 112.f, 110.f }, [this] { choose (InjClassic::pIn, cl.vi (InjClassic::pIn) ? 0 : 1); } });
        hots.push_back ({ { 370.f, 140.f, 320.f, 70.f }, [this] { presetMenu(); } });
        if (std::getenv ("INJ_PUNCHDEMO")) cl.applyPreset (3);
    }
protected:
    void paintSkin (juce::Graphics& g) override
    {
        drawSprite (g, cl.vi (InjClassic::pHfShelf) ? tdn[0] : tup[0], 741.f, 336.f); drawSprite (g, cl.vi (InjClassic::pLfShelf) ? tdn[1] : tup[1], 741.f, 1128.f);
        const bool on = cl.vi (InjClassic::pIn) != 0; drawSprite (g, on ? ledOn : ledOff, 406.f, 1342.f); drawSprite (g, on ? btOn : btOff, 522.f, on ? 1343.f : 1346.f);
        // la ganancia se ve también en los puntos de la derecha: el punto elegido se enciende
        static const float px[] { 108.8f, 127.7f, 138.3f, 143.6f, 144.7f, 141.2f, 133.8f, 121.4f, 97.9f }, py[] { -116.6f, -88.2f, -62.4f, -37.3f, -12.6f, 16.f, 41.2f, 62.9f, 87.9f };
        static const float vals[4][9] { { 12, 9, 6, 4, 2, -2, -4, -6, -12 }, { 12, 9, 6, 4, 2, -2, -4, -9, -12 }, { 12, 9, 6, 4, 2, -2, -6, -9, -12 }, { 12, 9, 6, 4, 2, -2, -6, -9, -12 } };
        const float cx[] { 457.2f, 458.f, 458.1f, 458.1f }, cy[] { 351.4f, 612.8f, 874.1f, 1138.7f }; const int gi[] { InjClassic::pHfG, InjClassic::pHmG, InjClassic::pLmG, InjClassic::pLfG };
        for (int b = 0; b < 4; ++b)
        {
            const float gv = juce::jlimit (-12.f, 12.f, cl.v (gi[b])); float x = px[8], y = py[8];
            for (int i = 0; i < 8; ++i) if (gv <= vals[b][i] && gv >= vals[b][i + 1]) { const float t = (vals[b][i] - gv) / (vals[b][i] - vals[b][i + 1]); x = px[i] + (px[i + 1] - px[i]) * t; y = py[i] + (py[i + 1] - py[i]) * t; break; }
            const float mx = cx[b] + x, my = cy[b] + y; juce::ColourGradient gr (juce::Colour (0xffffe9a8).withAlpha (.95f), mx * sc, my * sc, juce::Colour (0xffffb347).withAlpha (0.f), (mx + 15.f) * sc, my * sc, true);
            g.setGradientFill (gr); g.fillEllipse ((mx - 15.f) * sc, (my - 15.f) * sc, 30.f * sc, 30.f * sc); g.setColour (juce::Colour (0xfffff6dc)); g.fillEllipse ((mx - 4.5f) * sc, (my - 4.5f) * sc, 9.f * sc, 9.f * sc);
        }
    }
private:
    void presetMenu()
    {
        juce::PopupMenu m; auto ps = cl.presets(); m.addItem (1, "Inicial"); m.addSeparator();
        for (size_t i = 0; i < ps.size(); ++i) m.addItem (100 + (int) i, juce::String::fromUTF8 (ps[i].name), true, cl.presetName() == juce::String::fromUTF8 (ps[i].name));
        juce::Component::SafePointer<ClassicEditor> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [sp] (int r) { if (sp == nullptr) return; if (r == 1) sp->cl.resetAll(); else if (r >= 100) sp->cl.applyPreset (r - 100); sp->repaint(); });
    }
    InjClassic& cl; juce::Image tup[2], tdn[2], ledOn, ledOff, btOn, btOff;
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjClassic::createEditor() { return std::make_unique<ClassicEditor> (*this); }
} // namespace injfx
