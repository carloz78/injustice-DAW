#pragma once
#include "FxDual.h"
// Pulso EQ: ecualizador pasivo de programa con tubos (estilo clásico de rack): graves con boost y atten a la vez, agudos con ancho de banda y atenuación.
namespace injfx
{
class InjPulso : public InjFx
{
public:
    enum { pLfB, pLfA, pCps, pBw, pHfB, pKcs, pHfA, pAsel, pIn, pOn };
    static inline const char* xmlTypeName = "injpulso";
    static const char* getPluginName() { return "Pulso EQ"; }
    InjPulso (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Pulso"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 10; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    std::vector<Preset> presets() const override
    {   //                       LFB LFA CPS BW  HFB KCS HFA ASEL IN ON
        return { { "Plano", { 0.f, 0.f, 2.f, 5.f, 0.f, 3.f, 0.f, 1.f, 1.f, 1.f } }, { "Graves gordos", { 6.f, 3.f, 2.f, 5.f, 0.f, 3.f, 0.f, 1.f, 1.f, 1.f } }, { "Brillo de voz", { 0.f, 0.f, 2.f, 4.f, 4.f, 2.f, 1.f, 1.f, 1.f, 1.f } },
                 { "Aire", { 2.f, 0.f, 1.f, 7.f, 5.f, 6.f, 0.f, 2.f, 1.f, 1.f } }, { "Bombo con cuerpo", { 8.f, 4.f, 1.f, 5.f, 2.f, 1.f, 2.f, 0.f, 1.f, 1.f } } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; for (auto& b : f) b.reset(); }
    static float lfHz (int i) { static const float a[] { 20.f, 30.f, 60.f, 100.f }; return a[juce::jlimit (0, 3, i)]; }
    static float kcsHz (int i) { static const float a[] { 3000.f, 4000.f, 5000.f, 8000.f, 10000.f, 12000.f, 16000.f }; return a[juce::jlimit (0, 6, i)]; }
    static float attHz (int i) { static const float a[] { 5000.f, 10000.f, 20000.f }; return a[juce::jlimit (0, 2, i)]; }
    void setFilters()
    {
        const float fl = lfHz (vi (pCps)), q = 2.2f * std::exp (-(v (pBw) - 1.f) / 9.f * std::log (2.2f / .3f));
        f[0].set (BQ::LowShelf, sr, fl, .55, v (pLfB) * 1.35f);                 // boost de graves
        f[1].set (BQ::LowShelf, sr, fl * 1.5, .5, -v (pLfA) * 1.75f);           // atenuación de graves (un poco más arriba: de ahí el hueco clásico)
        f[2].set (BQ::Peak, sr, kcsHz (vi (pKcs)), q, v (pHfB) * 1.8f);         // boost de agudos con ancho de banda
        f[3].set (BQ::HighShelf, sr, attHz (vi (pAsel)), .5, -v (pHfA) * 1.6f); // atenuación de agudos
    }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || vi (pOn) == 0 || vi (pIn) == 0) return;
        setFilters();
        for (int c = 0; c < std::min (2, fc.destBuffer->getNumChannels()); ++c)
        {
            float* p = fc.destBuffer->getWritePointer (c, fc.bufferStartSample);
            for (int i = 0; i < fc.bufferNumSamples; ++i) { float x = p[i]; for (auto& b : f) x = b.run (c, x); p[i] = x; }
        }
    }
private:
    static inline const Spec S[] = {
        { "lfb", "Graves: Boost", 0.f, 10.f, 0.f, 0.f, fNum, "" }, { "lfa", "Graves: Atten", 0.f, 10.f, 0.f, 0.f, fNum, "" }, { "cps", "Graves: CPS", 0.f, 3.f, 2.f, 0.f, fChoice, "20 Hz|30 Hz|60 Hz|100 Hz" },
        { "bw", "Agudos: Ancho de banda", 1.f, 10.f, 5.f, 0.f, fNum, "" }, { "hfb", "Agudos: Boost", 0.f, 10.f, 0.f, 0.f, fNum, "" }, { "kcs", "Agudos: KCS", 0.f, 6.f, 3.f, 0.f, fChoice, "3 kHz|4 kHz|5 kHz|8 kHz|10 kHz|12 kHz|16 kHz" },
        { "hfa", "Agudos: Atten", 0.f, 10.f, 0.f, 0.f, fNum, "" }, { "asel", "Agudos: Atten Sel", 0.f, 2.f, 1.f, 0.f, fChoice, "5 kHz|10 kHz|20 kHz" },
        { "in", "Ecualizador", 0.f, 1.f, 1.f, 0.f, fChoice, "Fuera|Dentro" }, { "on", "Encendido", 0.f, 1.f, 1.f, 0.f, fChoice, "No|Sí" } };
    double sr = 44100.0; BQ f[4];
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjPulso)
};

class PulsoEditor : public SkinEditor
{
public:
    PulsoEditor (InjPulso& p) : SkinEditor (p, skinImage ("pulso_bg_png"), .46f, { 0, 40, 2172, 640 }), pu (p)
    {
        const float kx[] { 469.6f, 769.9f, 1217.8f, 1683.2f, 632.f, 1049.5f, 1381.1f, 1686.6f }, ky[] { 220.3f, 222.2f, 220.3f, 220.4f, 481.9f, 483.6f, 476.8f, 479.4f }, kr[] { 75.f, 75.f, 75.f, 75.f, 50.f, 70.f, 51.f, 39.f };
        const int idx[] { InjPulso::pLfB, InjPulso::pLfA, InjPulso::pHfB, InjPulso::pHfA, InjPulso::pCps, InjPulso::pBw, InjPulso::pKcs, InjPulso::pAsel }; const float rot[] { -6.f, -1.f, 0.f, 3.f, 20.f, 2.f, 36.f, 27.f };
        for (int k = 0; k < 8; ++k) { Knob kn { idx[k], kx[k], ky[k], kr[k], skinImage (("pulso_cap" + juce::String (k) + "_png").toRawUTF8()) }; kn.a0 = -135.f; kn.a1 = 135.f; kn.rot0 = rot[k];
            if (k == 4) kn.stops = { -45.f, -15.5f, 22.f, 52.5f }; if (k == 5) { kn.a0 = -113.f; kn.a1 = 140.f; } if (k == 6) kn.stops = { -61.f, -33.f, 1.f, 35.f, 65.f, 93.f, 121.f }; if (k == 7) kn.stops = { -40.f, 0.f, 43.f };
            knobs.push_back (kn); }
        for (int k = 0; k < 2; ++k) { const char* n = k == 0 ? "pulso_in" : "pulso_on"; tup[k] = skinImage ((juce::String (n) + "_up_png").toRawUTF8()); tdn[k] = skinImage ((juce::String (n) + "_dn_png").toRawUTF8()); }
        lampOn = skinImage ("pulso_lamp_on_png"); lampOff = skinImage ("pulso_lamp_off_png");
        hots.push_back ({ { 414.f - 46.f, 484.f - 58.f, 92.f, 116.f }, [this] { choose (InjPulso::pIn, pu.vi (InjPulso::pIn) ? 0 : 1); } });
        hots.push_back ({ { 1975.f - 48.f, 428.f - 54.f, 96.f, 108.f }, [this] { choose (InjPulso::pOn, pu.vi (InjPulso::pOn) ? 0 : 1); } });
        hots.push_back ({ { 100.f, 380.f, 230.f, 50.f }, [this] { presetMenu(); } });
        if (std::getenv ("INJ_PUNCHDEMO")) { pu.applyPreset (4); }
    }
protected:
    void paintSkin (juce::Graphics& g) override
    {
        drawSprite (g, pu.vi (InjPulso::pIn) ? tup[0] : tdn[0], 414.f, 484.f); drawSprite (g, pu.vi (InjPulso::pOn) ? tup[1] : tdn[1], 1975.f, 428.f);
        drawSprite (g, pu.vi (InjPulso::pOn) ? lampOn : lampOff, 1975.f, 247.f);
    }
private:
    void presetMenu()
    {
        juce::PopupMenu m; auto ps = pu.presets(); m.addItem (1, "Inicial"); m.addSeparator();
        for (size_t i = 0; i < ps.size(); ++i) m.addItem (100 + (int) i, juce::String::fromUTF8 (ps[i].name), true, pu.presetName() == juce::String::fromUTF8 (ps[i].name));
        juce::Component::SafePointer<PulsoEditor> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [sp] (int r) { if (sp == nullptr) return; if (r == 1) sp->pu.resetAll(); else if (r >= 100) sp->pu.applyPreset (r - 100); sp->repaint(); });
    }
    InjPulso& pu; juce::Image tup[2], tdn[2], lampOn, lampOff;
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjPulso::createEditor() { return std::make_unique<PulsoEditor> (*this); }
} // namespace injfx
