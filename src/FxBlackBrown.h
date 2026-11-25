#pragma once
#include "FxAirlift.h"
// Black Brown EQ: ecualizador de consola de cuatro bandas con filtros, dos formas de curva (Black: Q que sube con la ganancia; Brown: más ancha y suave) y sus botones de ruta.
namespace injfx
{
class InjBlackBrown : public InjFx
{
public:
    enum { pHpf, pLpf, pHfG, pHfF, pHfBell, pHmG, pHmF, pHmQ, pLmG, pLmF, pLmQ, pLfG, pLfF, pLfBell, pType, pSplit, pBypass, pDyn };
    static inline const char* xmlTypeName = "injblackbrown";
    static const char* getPluginName() { return "Black Brown EQ"; }
    InjBlackBrown (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "BlackBrown"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 18; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    std::vector<Preset> presets() const override
    {   //                       HPF LPF HFg HFf   HFb HMg HMf   HMq LMg LMf  LMq LFg LFf LFb Tipo Spl Byp Dyn
        return { { "Plano", { 0.f, 0.f, 0.f, 8000.f, 0.f, 0.f, 2000.f, 1.5f, 0.f, 300.f, 1.5f, 0.f, 100.f, 0.f, 0.f, 0.f, 0.f, 0.f } },
                 { "Bombo con pegada", { 2.f, 0.f, 2.f, 8000.f, 0.f, -2.f, 2000.f, 1.5f, -4.f, 300.f, 1.2f, 5.f, 60.f, 0.f, 0.f, 0.f, 0.f, 0.f } },
                 { "Voz brillante", { 2.f, 0.f, 3.f, 12000.f, 0.f, 3.f, 3000.f, 1.2f, -2.f, 400.f, 1.5f, 0.f, 100.f, 0.f, 1.f, 0.f, 0.f, 0.f } },
                 { "Limpiar graves", { 3.f, 0.f, 0.f, 8000.f, 0.f, 0.f, 2000.f, 1.5f, -3.f, 300.f, 1.f, 0.f, 100.f, 0.f, 0.f, 0.f, 0.f, 0.f } },
                 { "Bus brillante", { 0.f, 0.f, 4.f, 10000.f, 0.f, 1.f, 4000.f, 1.f, 0.f, 300.f, 1.5f, 2.f, 80.f, 0.f, 1.f, 0.f, 0.f, 0.f } } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; for (auto& b : f) b.reset(); }
    static float hpfHz (int i) { static const float a[] { 0.f, 16.f, 40.f, 80.f, 160.f, 300.f, 600.f }; return a[juce::jlimit (0, 6, i)]; }
    static float lpfHz (int i) { static const float a[] { 0.f, 1000.f, 3000.f, 5000.f, 8000.f, 12000.f, 18000.f }; return a[juce::jlimit (0, 6, i)]; }
    float qOf (float q, float gdb) const { return vi (pType) == 0 ? q * (.8f + .9f * std::abs (gdb) / 15.f) : q * .9f; }   // Black: Q proporcional a la ganancia
    void setFilters()
    {
        const float hp = hpfHz (vi (pHpf)), lp = lpfHz (vi (pLpf));
        f[0].set (BQ::HP, sr, hp > 0.f ? hp : 10.f, .707, 0.0); f[1].set (BQ::LP, sr, lp > 0.f ? lp : sr * .49, .707, 0.0);
        hpOn = hp > 0.f; lpOn = lp > 0.f;
        f[2].set (vi (pHfBell) ? BQ::Peak : BQ::HighShelf, sr, v (pHfF), vi (pHfBell) ? qOf (.9f, v (pHfG)) : .7, v (pHfG));
        f[3].set (BQ::Peak, sr, v (pHmF), qOf (v (pHmQ), v (pHmG)), v (pHmG));
        f[4].set (BQ::Peak, sr, v (pLmF), qOf (v (pLmQ), v (pLmG)), v (pLmG));
        f[5].set (vi (pLfBell) ? BQ::Peak : BQ::LowShelf, sr, v (pLfF), vi (pLfBell) ? qOf (.9f, v (pLfG)) : .7, v (pLfG));
    }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || vi (pBypass) != 0 || vi (pDyn) != 0) return;   // DYN S-C: el EQ alimenta solo al sidechain de la dinámica, el audio pasa sin filtrar
        setFilters(); const bool after = vi (pSplit) != 0;
        for (int c = 0; c < std::min (2, fc.destBuffer->getNumChannels()); ++c)
        {
            float* p = fc.destBuffer->getWritePointer (c, fc.bufferStartSample);
            for (int i = 0; i < fc.bufferNumSamples; ++i)
            {
                float x = p[i];
                if (! after) { if (hpOn) x = f[0].run (c, x); if (lpOn) x = f[1].run (c, x); }
                for (int b = 2; b < 6; ++b) x = f[b].run (c, x);
                if (after) { if (hpOn) x = f[0].run (c, x); if (lpOn) x = f[1].run (c, x); }
                p[i] = x;
            }
        }
    }
private:
    static inline const Spec S[] = {
        { "hpf", "Filtro HPF", 0.f, 6.f, 0.f, 0.f, fChoice, "Fuera|16 Hz|40 Hz|80 Hz|160 Hz|300 Hz|600 Hz" }, { "lpf", "Filtro LPF", 0.f, 6.f, 0.f, 0.f, fChoice, "Fuera|1 kHz|3 kHz|5 kHz|8 kHz|12 kHz|18 kHz" },
        { "hfg", "HF: Ganancia", -15.f, 15.f, 0.f, 0.f, fDb, "" }, { "hff", "HF: Frecuencia", 600.f, 16000.f, 8000.f, 3100.f, fHz, "" }, { "hfb", "HF: Forma", 0.f, 1.f, 0.f, 0.f, fChoice, "Shelf|Campana" },
        { "hmg", "HMF: Ganancia", -15.f, 15.f, 0.f, 0.f, fDb, "" }, { "hmf", "HMF: Frecuencia", 600.f, 7000.f, 2000.f, 2050.f, fHz, "" }, { "hmq", "HMF: Q", .5f, 5.f, 1.5f, 1.6f, fQ, "" },
        { "lmg", "LMF: Ganancia", -15.f, 15.f, 0.f, 0.f, fDb, "" }, { "lmf", "LMF: Frecuencia", 100.f, 1500.f, 300.f, 390.f, fHz, "" }, { "lmq", "LMF: Q", .5f, 5.f, 1.5f, 1.6f, fQ, "" },
        { "lfg", "LF: Ganancia", -15.f, 15.f, 0.f, 0.f, fDb, "" }, { "lff", "LF: Frecuencia", 30.f, 400.f, 100.f, 110.f, fHz, "" }, { "lfb", "LF: Forma", 0.f, 1.f, 0.f, 0.f, fChoice, "Shelf|Campana" },
        { "type", "Tipo de EQ", 0.f, 1.f, 0.f, 0.f, fChoice, "Black|Brown" }, { "split", "Split", 0.f, 1.f, 0.f, 0.f, fChoice, "Filtros antes|Filtros despues" },
        { "byp", "EQ a Bypass", 0.f, 1.f, 0.f, 0.f, fChoice, "No|Si" }, { "dyn", "EQ a Dyn S-C", 0.f, 1.f, 0.f, 0.f, fChoice, "No|Si" } };
    double sr = 44100.0; BQ f[6]; bool hpOn = false, lpOn = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjBlackBrown)
};

class BlackBrownEditor : public SkinEditor
{
public:
    BlackBrownEditor (InjBlackBrown& p) : SkinEditor (p, skinImage ("bb_bg_png"), .46f, { 146, 0, 735, 1522 }), bb (p)
    {
        const float kx[] { 368.9f, 685.4f, 348.7f, 564.5f, 351.9f, 557.2f, 763.8f, 351.5f, 561.5f, 763.6f, 352.f, 567.1f }, ky[] { 368.6f, 367.7f, 553.6f, 553.5f, 746.f, 748.5f, 751.5f, 955.3f, 954.8f, 957.1f, 1159.4f, 1158.9f };
        const float kr[] { 48.f, 48.f, 47.f, 47.f, 43.f, 43.f, 37.f, 47.f, 45.f, 38.f, 48.f, 47.f }, rot[] { 0.f, -1.f, 2.f, 3.f, 1.f, 1.f, 2.f, 1.f, 0.f, 3.f, 1.f, 0.f };
        const int idx[] { InjBlackBrown::pHpf, InjBlackBrown::pLpf, InjBlackBrown::pHfG, InjBlackBrown::pHfF, InjBlackBrown::pHmG, InjBlackBrown::pHmF, InjBlackBrown::pHmQ, InjBlackBrown::pLmG, InjBlackBrown::pLmF, InjBlackBrown::pLmQ, InjBlackBrown::pLfG, InjBlackBrown::pLfF };
        const std::vector<float> qv { .5f, .7f, 1.f, 1.5f, 2.f, 3.f, 5.f }, qa { -125.5f, -97.f, -41.6f, -4.f, 40.5f, 98.f, 125.f };
        for (int k = 0; k < 12; ++k)
        {
            Knob kn { idx[k], kx[k], ky[k], kr[k], skinImage (("bb_cap" + juce::String (k) + "_png").toRawUTF8()) }; kn.rot0 = rot[k];
            switch (k)
            {
                case 0: kn.stops = { -122.f, -103.f, -63.f, -29.f, 8.f, 42.f, 79.f }; break;
                case 1: kn.stops = { -119.5f, -65.6f, -28.7f, 1.7f, 32.5f, 70.7f, 105.8f }; break;
                case 3: kn.cv = { 600.f, 1500.f, 3000.f, 5000.f, 8000.f, 12000.f, 16000.f }; kn.ca = { -108.f, -53.f, -4.f, 34.f, 73.f, 105.f, 128.f }; break;
                case 5: kn.cv = { 600.f, 800.f, 1500.f, 2000.f, 3000.f, 4000.f, 6000.f, 7000.f }; kn.ca = { -119.f, -101.5f, -43.f, 0.f, 37.f, 70.f, 107.f, 137.f }; break;
                case 6: case 9: kn.cv = qv; kn.ca = qa; break;
                case 8: kn.cv = { 100.f, 150.f, 200.f, 300.f, 600.f, 800.f, 1000.f, 1500.f }; kn.ca = { -108.f, -75.f, -43.f, -1.5f, 40.f, 72.f, 102.5f, 127.f }; break;
                case 11: kn.cv = { 30.f, 40.f, 60.f, 100.f, 200.f, 300.f, 400.f }; kn.ca = { -126.f, -88.5f, -56.f, -18.f, 23.f, 59.f, 95.f }; break;
                default: kn.a0 = -135.f; kn.a1 = 135.f; break;   // perillas de ganancia ±15 dB
            }
            knobs.push_back (kn);
        }
        ledOn = skinImage ("bb_led_on_png"); ledOff = skinImage ("bb_led_off_png"); bkOn = skinImage ("bb_black_on_png"); bkOff = skinImage ("bb_black_off_png"); brOn = skinImage ("bb_brown_on_png"); brOff = skinImage ("bb_brown_off_png");
        hots.push_back ({ { 420.f, 150.f, 120.f, 62.f }, [this] { choose (InjBlackBrown::pType, 0); } });
        hots.push_back ({ { 578.f, 150.f, 108.f, 62.f }, [this] { choose (InjBlackBrown::pType, 1); } });
        auto tg = [this] (float x, float y, float w, float h, int pi) { hots.push_back ({ { x - w * .5f, y - h * .5f, w, h }, [this, pi] { choose (pi, bb.vi (pi) ? 0 : 1); } }); };
        tg (530.f, 370.f, 72.f, 54.f, InjBlackBrown::pSplit); tg (778.f, 551.f, 90.f, 58.f, InjBlackBrown::pHfBell); tg (780.f, 1153.f, 90.f, 58.f, InjBlackBrown::pLfBell);
        tg (393.f, 1363.f, 126.f, 66.f, InjBlackBrown::pBypass); tg (661.f, 1361.f, 134.f, 66.f, InjBlackBrown::pDyn);
        hots.push_back ({ { 470.f, 20.f, 480.f, 100.f }, [this] { presetMenu(); } });
        if (std::getenv ("INJ_PUNCHDEMO")) bb.applyPreset (2);
    }
protected:
    void paintSkin (juce::Graphics& g) override
    {
        const bool black = bb.vi (InjBlackBrown::pType) == 0;
        drawSprite (g, black ? bkOn : bkOff, 480.f, 181.f); drawSprite (g, black ? brOff : brOn, 632.f, 181.f);
        drawSprite (g, black ? ledOn : ledOff, 481.f, 131.f); drawSprite (g, black ? ledOff : ledOn, 629.f, 131.f);
        auto L = [&] (int pi, float x, float y) { drawSprite (g, bb.vi (pi) ? ledOn : ledOff, x, y); };
        L (InjBlackBrown::pSplit, 528.f, 327.f); L (InjBlackBrown::pHfBell, 776.f, 500.f); L (InjBlackBrown::pLfBell, 779.f, 1106.f); L (InjBlackBrown::pBypass, 396.f, 1305.f); L (InjBlackBrown::pDyn, 658.f, 1303.f);
    }
private:
    void presetMenu()
    {
        juce::PopupMenu m; auto ps = bb.presets(); m.addItem (1, "Inicial"); m.addSeparator();
        for (size_t i = 0; i < ps.size(); ++i) m.addItem (100 + (int) i, juce::String::fromUTF8 (ps[i].name), true, bb.presetName() == juce::String::fromUTF8 (ps[i].name));
        juce::Component::SafePointer<BlackBrownEditor> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [sp] (int r) { if (sp == nullptr) return; if (r == 1) sp->bb.resetAll(); else if (r >= 100) sp->bb.applyPreset (r - 100); sp->repaint(); });
    }
    InjBlackBrown& bb; juce::Image ledOn, ledOff, bkOn, bkOff, brOn, brOff;
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjBlackBrown::createEditor() { return std::make_unique<BlackBrownEditor> (*this); }
} // namespace injfx
