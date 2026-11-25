#pragma once
#include "FxGateDS.h"
// De-Esser: reductor de sibilancias con entrada, frecuencia, rango (reducción máxima), liberación, salida, modo DE-ESS (solo la banda) o BROADBAND (toda la señal),
// medidor de reducción de 7 LEDs y escucha de la cadena lateral.
namespace injfx
{
class InjDeEsser : public InjFx
{
public:
    enum { pIn, pFreq, pRange, pRel, pOut, pMode, pListen };
    static inline const char* xmlTypeName = "injdeesser";
    static const char* getPluginName() { return "De-Esser"; }
    InjDeEsser (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "DeEsser"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 7; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    std::vector<Preset> presets() const override
    {   //                       In  Freq    Rango Lib  Out Modo Escucha
        return { { "Voz suave", { 0.f, 6000.f, 6.f, 150.f, 0.f, 0.f, 0.f } }, { "Voz agresiva", { 4.f, 5500.f, 15.f, 120.f, 0.f, 0.f, 0.f } }, { "Hi-hat dominante", { 2.f, 9000.f, 12.f, 80.f, 0.f, 0.f, 0.f } },
                 { "Bus de voces", { 0.f, 4500.f, 8.f, 250.f, 0.f, 1.f, 0.f } }, { "Buscar la sibilancia", { 0.f, 6500.f, 10.f, 150.f, 0.f, 0.f, 1.f } } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; env = 0.f; gr = 0.f; bp[0].reset(); bp[1].reset(); sc.reset(); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nullptr }; p[1] = nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : p[0];
        const float inG = db2lin (v (pIn)), outG = db2lin (v (pOut)), range = v (pRange); const bool broad = vi (pMode) == 1, listen = vi (pListen) != 0;
        const float hz = juce::jlimit (200.f, (float) sr * .45f, v (pFreq)); bp[0].set (BQ::BP, sr, hz, 1.2, 0.0); bp[1].set (BQ::BP, sr, hz, 1.2, 0.0); sc.set (BQ::BP, sr, hz, 1.2, 0.0);
        const float aE = coefMs (1.5f, sr), aR = coefMs (v (pRel), sr); float maxGr = 0.f;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            float x[2] { p[0][i] * inG, p[1][i] * inG }, b[2] { bp[0].run (0, x[0]), bp[1].run (1, x[1]) };
            const float lvl = std::max (std::abs (b[0]), std::abs (b[1])); env = lvl > env ? aE * env + (1.f - aE) * lvl : aR * env + (1.f - aR) * lvl;
            const float db = lin2db (env + 1e-9f), target = juce::jlimit (0.f, range, (db + 26.f) * .9f); gr = target > gr ? aE * gr + (1.f - aE) * target : aR * gr + (1.f - aR) * target;
            const float g = db2lin (-gr); maxGr = std::max (maxGr, gr);
            for (int c = 0; c < 2; ++c) { float y = broad ? x[c] * g : x[c] - b[c] * (1.f - g); if (listen) y = b[c]; p[c][i] = y * outG; }
            if (nc < 2) p[1][i] = p[0][i];
        }
        meterA.store (maxGr);
    }
private:
    static inline const Spec S[] = {
        { "in", "Entrada", -30.f, 20.f, 0.f, 0.f, fDb, "" }, { "freq", "Frecuencia", 800.f, 12000.f, 6000.f, 4000.f, fHz, "" }, { "range", "Rango", 0.f, 25.f, 10.f, 0.f, fDb, "" }, { "rel", "Liberación", 25.f, 1500.f, 200.f, 200.f, fMs, "" },
        { "out", "Salida", -30.f, 20.f, 0.f, 0.f, fDb, "" }, { "mode", "Modo", 0.f, 1.f, 0.f, 0.f, fChoice, "De-Ess|Broadband" }, { "listen", "Escuchar cadena lateral", 0.f, 1.f, 0.f, 0.f, fChoice, "No|Sí" } };
    double sr = 44100.0; BQ bp[2], sc; float env = 0.f, gr = 0.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjDeEsser)
};

class DeEsserEditor : public SkinEditor
{
public:
    DeEsserEditor (InjDeEsser& p) : SkinEditor (p, skinImage ("de_bg_png"), .46f), de (p)
    {
        const float cx[5] { 257.4f, 565.6f, 879.5f, 1186.9f, 1948.5f }, cy[5] { 357.4f, 357.8f, 357.8f, 357.2f, 355.9f }, rot[5] { -7.f, -3.f, -2.f, -1.f, 7.f }, span[5] { 102.f, 135.f, 135.f, 130.f, 102.f };
        for (int k = 0; k < 5; ++k) { Knob kn { k, cx[k], cy[k], 47.f, skinImage (("de_cap" + juce::String (k) + "_png").toRawUTF8()) }; kn.a0 = -span[k]; kn.a1 = span[k]; kn.rot0 = rot[k]; knobs.push_back (kn); }
        led = skinImage ("de_led_png"); for (int k = 0; k < 2; ++k) { mOn[k] = skinImage (("de_m" + juce::String (k) + "_on_png").toRawUTF8()); mOff[k] = skinImage (("de_m" + juce::String (k) + "_off_png").toRawUTF8()); }
        hots.push_back ({ { 233.f - 45.f, 570.f - 45.f, 90.f, 90.f }, [this] { choose (InjDeEsser::pMode, 0); } });
        hots.push_back ({ { 426.f - 45.f, 570.f - 45.f, 90.f, 90.f }, [this] { choose (InjDeEsser::pMode, 1); } });
        hots.push_back ({ { 1176.f - 30.f, 595.f - 30.f, 60.f, 60.f }, [this] { choose (InjDeEsser::pListen, de.vi (InjDeEsser::pListen) ? 0 : 1); } });
        hots.push_back ({ { 560.f, 40.f, 1450.f, 120.f }, [this] { presetMenu(); } });
        if (std::getenv ("INJ_PUNCHDEMO")) { de.applyPreset (1); gr = 12.f; demo = true; }
    }
protected:
    void tick() override { if (! demo) gr = std::max (de.meterA.load(), gr * .88f); repaint(); }
    void paintSkin (juce::Graphics& g) override
    {
        const int m = de.vi (InjDeEsser::pMode); drawSprite (g, m == 0 ? mOn[0] : mOff[0], 233.f, 570.f); drawSprite (g, m == 1 ? mOn[1] : mOff[1], 426.f, 570.f);
        static const float th[7] { .5f, 3.f, 6.f, 10.f, 15.f, 20.f, 25.f }; static const float lx[7] { 1416.1f, 1469.f, 1521.6f, 1574.7f, 1627.8f, 1682.3f, 1735.2f };
        for (int k = 0; k < 7; ++k) if (gr >= th[k]) drawLed (g, led, k, lx[k], 330.2f);
        if (de.vi (InjDeEsser::pListen))
        {
            juce::ColourGradient gd (juce::Colour (0xffffb43a), 1176.f * sc, 595.f * sc, juce::Colour (0x00ffb43a), (1176.f + 30.f) * sc, 595.f * sc, true); g.setGradientFill (gd); g.fillEllipse ((1176.f - 30.f) * sc, (595.f - 30.f) * sc, 60.f * sc, 60.f * sc);
        }
    }
private:
    void presetMenu()
    {
        juce::PopupMenu m; auto ps = de.presets(); m.addItem (1, "Inicial"); m.addSeparator();
        for (size_t i = 0; i < ps.size(); ++i) m.addItem (100 + (int) i, juce::String::fromUTF8 (ps[i].name), true, de.presetName() == juce::String::fromUTF8 (ps[i].name));
        juce::Component::SafePointer<DeEsserEditor> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [sp] (int r) { if (sp == nullptr) return; if (r == 1) sp->de.resetAll(); else if (r >= 100) sp->de.applyPreset (r - 100); sp->repaint(); });
    }
    InjDeEsser& de; juce::Image led, mOn[2], mOff[2]; float gr = 0.f; bool demo = false;
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjDeEsser::createEditor() { return std::make_unique<DeEsserEditor> (*this); }
} // namespace injfx
