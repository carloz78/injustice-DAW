#pragma once
#include "FxClassic.h"
// Airlift EQ: ecualizador de realce (estilo clásico de estudio): seis bandas que solo suben (sub, 40, 160, 650, 2.5k) y una banda de aire con frecuencia elegible.
namespace injfx
{
class InjAirlift : public InjFx
{
public:
    enum { pAirF, pAir, p25, p650, p160, p40, pSub, pIn };
    static inline const char* xmlTypeName = "injairlift";
    static const char* getPluginName() { return "Airlift EQ"; }
    InjAirlift (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Airlift"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 8; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    std::vector<Preset> presets() const override
    {   //                       AirF Air 2.5k 650 160  40  Sub In
        return { { "Plano", { 3.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f } }, { "Aire de voz", { 3.f, 5.f, 2.f, 0.f, 0.f, 0.f, 0.f, 1.f } }, { "Graves pesados", { 3.f, 0.f, 0.f, 0.f, 4.f, 6.f, 5.f, 1.f } },
                 { "Brillo de mezcla", { 4.f, 4.f, 3.f, 0.f, 1.f, 2.f, 1.f, 1.f } }, { "Cuerpo de bombo", { 2.f, 1.f, 1.f, 2.f, 5.f, 6.f, 7.f, 1.f } } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; for (auto& b : f) b.reset(); }
    static float airHz (int i) { static const float a[] { 10000.f, 2500.f, 5000.f, 10000.f, 20000.f, 40000.f }; return a[juce::jlimit (0, 5, i)]; }
    void setFilters()
    {
        f[0].set (BQ::LowShelf, sr, 45.0, .6, v (pSub) * 1.2f);
        f[1].set (BQ::Peak, sr, 40.0, .7, v (p40) * 1.5f);
        f[2].set (BQ::Peak, sr, 160.0, .7, v (p160) * 1.5f);
        f[3].set (BQ::Peak, sr, 650.0, .7, v (p650) * 1.5f);
        f[4].set (BQ::Peak, sr, 2500.0, .7, v (p25) * 1.5f);
        f[5].set (BQ::HighShelf, sr, airHz (vi (pAirF)), .45, vi (pAirF) == 0 ? 0.f : v (pAir) * 1.6f);
    }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        float pk = 0.f;
        if (vi (pIn) != 0) setFilters();
        for (int c = 0; c < std::min (2, fc.destBuffer->getNumChannels()); ++c)
        {
            float* p = fc.destBuffer->getWritePointer (c, fc.bufferStartSample);
            for (int i = 0; i < fc.bufferNumSamples; ++i) { float x = p[i]; if (vi (pIn) != 0) for (auto& b : f) x = b.run (c, x); p[i] = x; pk = std::max (pk, std::abs (x)); }
        }
        meterA.store (lin2db (pk));   // nivel de pico de salida: enciende los LED PEAK y SIGNAL
    }
private:
    static inline const Spec S[] = {
        { "airf", "Aire: Frecuencia", 0.f, 5.f, 3.f, 0.f, fChoice, "Apagado|2.5 kHz|5 kHz|10 kHz|20 kHz|40 kHz" }, { "air", "Aire: Ganancia", 0.f, 10.f, 0.f, 0.f, fNum, "" },
        { "g25", "2.5 kHz", 0.f, 10.f, 0.f, 0.f, fNum, "" }, { "g650", "650 Hz", 0.f, 10.f, 0.f, 0.f, fNum, "" }, { "g160", "160 Hz", 0.f, 10.f, 0.f, 0.f, fNum, "" }, { "g40", "40 Hz", 0.f, 10.f, 0.f, 0.f, fNum, "" },
        { "sub", "Sub (10 Hz)", 0.f, 10.f, 0.f, 0.f, fNum, "" }, { "in", "Ecualizador", 0.f, 1.f, 1.f, 0.f, fChoice, "Fuera|Dentro" } };
    double sr = 44100.0; BQ f[6];
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjAirlift)
};

class AirliftEditor : public SkinEditor
{
public:
    AirliftEditor (InjAirlift& p) : SkinEditor (p, skinImage ("air_bg_png"), .5f, { 220, 0, 590, 1490 }), ai (p)
    {
        const float kx[] { 432.f, 650.f, 416.4f, 683.3f, 416.5f, 684.f, 425.f }, ky[] { 384.4f, 551.3f, 679.9f, 808.8f, 954.5f, 1068.9f, 1226.f }, rot[] { 2.f, 4.f, 4.f, -1.f, 4.f, -2.f, 14.f };
        const int idx[] { InjAirlift::pAirF, InjAirlift::pAir, InjAirlift::p25, InjAirlift::p650, InjAirlift::p160, InjAirlift::p40, InjAirlift::pSub };
        for (int k = 0; k < 7; ++k) { Knob kn { idx[k], kx[k], ky[k], 50.f, skinImage (("air_cap" + juce::String (k) + "_png").toRawUTF8()) }; kn.a0 = -132.f; kn.a1 = 132.f; kn.rot0 = rot[k]; if (k == 0) kn.stops = { -101.f, -61.f, -24.f, 21.5f, 62.f, 100.f }; knobs.push_back (kn); }
        pkOn = skinImage ("air_peak_on_png"); pkOff = skinImage ("air_peak_off_png"); sgOn = skinImage ("air_sig_on_png"); sgOff = skinImage ("air_sig_off_png"); bUp = skinImage ("air_btn_up_png"); bDn = skinImage ("air_btn_dn_png");
        hots.push_back ({ { 655.f - 48.f, 1245.f - 48.f, 96.f, 96.f }, [this] { choose (InjAirlift::pIn, ai.vi (InjAirlift::pIn) ? 0 : 1); } });
        hots.push_back ({ { 260.f, 160.f, 520.f, 80.f }, [this] { presetMenu(); } });
        if (std::getenv ("INJ_PUNCHDEMO")) { ai.applyPreset (4); demo = true; }
    }
protected:
    void tick() override { const float lv = ai.meterA.load(); if (lv > -1.5f) hold = 12; else if (hold > 0) --hold; sig = lv > -55.f; repaint(); }
    void paintSkin (juce::Graphics& g) override
    {
        const bool on = ai.vi (InjAirlift::pIn) != 0;
        drawSprite (g, (demo || hold > 0) && on ? pkOn : pkOff, 651.f, 274.f); drawSprite (g, (demo || sig) ? sgOn : sgOff, 651.f, 334.f); drawSprite (g, on ? bDn : bUp, 655.f, 1245.f);
    }
private:
    void presetMenu()
    {
        juce::PopupMenu m; auto ps = ai.presets(); m.addItem (1, "Inicial"); m.addSeparator();
        for (size_t i = 0; i < ps.size(); ++i) m.addItem (100 + (int) i, juce::String::fromUTF8 (ps[i].name), true, ai.presetName() == juce::String::fromUTF8 (ps[i].name));
        juce::Component::SafePointer<AirliftEditor> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [sp] (int r) { if (sp == nullptr) return; if (r == 1) sp->ai.resetAll(); else if (r >= 100) sp->ai.applyPreset (r - 100); sp->repaint(); });
    }
    InjAirlift& ai; juce::Image pkOn, pkOff, sgOn, sgOff, bUp, bDn; int hold = 0; bool sig = false, demo = false;
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjAirlift::createEditor() { return std::make_unique<AirliftEditor> (*this); }
} // namespace injfx
