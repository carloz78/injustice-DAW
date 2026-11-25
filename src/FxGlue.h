#pragma once
#include "FxFet.h"
// Glue Comp: compresor de bus estéreo enlazado (umbral, ataque, liberación con modo AUTO, ratios 2/4/10, ganancia de salida), con saturación analógica y fundido de salida.
namespace injfx
{
class InjGlue : public InjFx
{
public:
    enum { pThr, pMk, pAtk, pRel, pRatio, pIn, pAnalog, pFade };
    static inline const char* xmlTypeName = "injglue";
    static const char* getPluginName() { return "Glue Comp"; }
    InjGlue (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Glue"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 8; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    std::vector<Preset> presets() const override
    {   //                            Umbral Gan. Ataque Lib. Ratio In Analog Fade
        return { { "Pegamento suave", { -12.f, 2.f, 10.f, 1.2f, 0.f, 1.f, 0.f, 0.f } }, { "Bus de batería", { -16.f, 3.f, 3.f, .3f, 1.f, 1.f, 1.f, 0.f } }, { "Master", { -10.f, 1.f, 30.f, 1.5f, 0.f, 1.f, 1.f, 0.f } },
                 { "Aplastar", { -24.f, 6.f, 1.f, .1f, 2.f, 1.f, 1.f, 0.f } }, { "Automático", { -14.f, 2.f, 3.f, 1.5f, 1.f, 1.f, 0.f, 0.f } } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; gr = 0.f; fade = 1.f; }
    static float ratioOf (int i) { static const float r[] { 2.f, 4.f, 10.f }; return r[juce::jlimit (0, 2, i)]; }
    static bool isAuto (float rel) { return rel >= 1.45f; }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p0 = fc.destBuffer->getWritePointer (0, fc.bufferStartSample); float* p1 = nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : p0;
        const float T = v (pThr), R = ratioOf (vi (pRatio)), mk = db2lin (v (pMk)), atk = v (pAtk), rel = v (pRel); const bool on = vi (pIn) != 0, an = vi (pAnalog) != 0, autoRel = isAuto (rel), fd = vi (pFade) != 0;
        const float aA = std::exp (-1.f / (atk * .001f * (float) sr)); const float aR = std::exp (-1.f / (rel * (float) sr));
        const float stDown = 1.f / (5.f * (float) sr), stUp = 1.f / (.5f * (float) sr);
        float maxGr = 0.f;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            float l = p0[i], r = p1[i];
            if (on)
            {
                const float x = lin2db (std::max (std::abs (l), std::abs (r))), target = std::max (0.f, x - InjComp::curve (x, T, R, 3.f));
                const float ar = autoRel ? std::exp (-1.f / ((1.2f - .95f * juce::jlimit (0.f, 1.f, gr / 8.f)) * (float) sr)) : aR;
                const float k = target > gr ? aA : ar; gr = k * gr + (1.f - k) * target; maxGr = std::max (maxGr, gr);
                const float g = db2lin (-gr) * mk; l *= g; r *= g;
                if (an) { l = std::tanh (l * 1.2f) / 1.2f; r = std::tanh (r * 1.2f) / 1.2f; }
            }
            fade = fd ? std::max (0.f, fade - stDown) : std::min (1.f, fade + stUp); l *= fade; r *= fade;
            p0[i] = l; p1[i] = r;
        }
        meterA.store (maxGr);
    }
private:
    static inline const Spec S[] = {
        { "thr", "Umbral", -30.f, 0.f, -15.f, 0.f, fDb, "" }, { "mk", "Ganancia de salida", 0.f, 15.f, 0.f, 0.f, fDb, "" }, { "atk", "Ataque", .1f, 10.f, 3.f, 1.f, fMs, "" }, { "rel", "Liberación", .1f, 1.5f, .6f, .6f, fSec, "" },
        { "ratio", "Ratio", 0.f, 2.f, 1.f, 0.f, fChoice, "2:1|4:1|10:1" }, { "in", "Compresor activo", 0.f, 1.f, 1.f, 0.f, fChoice, "No|Sí" }, { "analog", "Analógico", 0.f, 1.f, 0.f, 0.f, fChoice, "No|Sí" }, { "fade", "Fundido de salida", 0.f, 1.f, 0.f, 0.f, fChoice, "No|Sí" } };
    double sr = 44100.0; float gr = 0.f, fade = 1.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjGlue)
};

class GlueEditor : public SkinEditor
{
public:
    GlueEditor (InjGlue& p) : SkinEditor (p, skinImage ("glue_bg_png"), .46f), gl (p)
    {
        const float kx[] { 397.5f, 691.5f, 322.f, 537.9f, 755.5f }, ky[] { 688.9f, 689.5f, 963.2f, 959.8f, 961.8f }, kr[] { 49.3f, 49.4f, 43.4f, 43.2f, 43.6f };
        const int idx[] { InjGlue::pThr, InjGlue::pMk, InjGlue::pAtk, InjGlue::pRel, InjGlue::pRatio }; const char* cn[] { "glue_cap0_png", "glue_cap1_png", "glue_cap2_png", "glue_cap3_png", "glue_cap4_png" };
        for (int k = 0; k < 5; ++k) knobs.push_back ({ idx[k], kx[k], ky[k], kr[k], skinImage (cn[k]) });
        const char* ln[] { "glue_litIN_png", "glue_litANALOG_png", "glue_litFADEOFF_png" }; for (int k = 0; k < 3; ++k) lit[k] = skinImage (ln[k]);
        const int pi[] { InjGlue::pIn, InjGlue::pAnalog, InjGlue::pFade };
        for (int k = 0; k < 3; ++k) hots.push_back ({ { bx[k] - 88.f, 1196.f, 176.f, 90.f }, [this, k, pi] { choose (pi[k], gl.vi (pi[k]) ? 0 : 1); } });
        hots.push_back ({ { 300.f, 140.f, 490.f, 120.f }, [this] { presetMenu(); } });                                                    // "GLUE COMP" abre los preajustes
        hots.push_back ({ { 340.f, 235.f, 410.f, 40.f }, [this] { gl.setEnabled (! gl.isEnabled()); } });                              // "BUS COMPRESSOR" enciende y apaga
        if (std::getenv ("INJ_PUNCHDEMO")) { gl.applyPreset (3); grS = 9.f; demo = true; }
    }
protected:
    void tick() override { if (! demo) grS = std::max (gl.meterA.load(), grS * .9f); repaint(); }
    void paintSkin (juce::Graphics& g) override
    {
        for (int k = 0; k < 3; ++k) { static const int pi[] { InjGlue::pIn, InjGlue::pAnalog, InjGlue::pFade }; if (gl.vi (pi[k])) drawSprite (g, lit[k], bx[k], 1241.f); }
        const float a = -24.9f + juce::jlimit (0.f, 20.f, grS) / 20.f * 50.f;
        drawNeedle (g, 542.7f, 788.5f, a, 290.f, 394.f, { 322.f, 321.f, 443.f, 169.f }, juce::Colour (0xffeef1f4), 3.2f);
    }
private:
    void presetMenu()
    {
        juce::PopupMenu m; auto ps = gl.presets(); m.addItem (1, "Inicial"); m.addSeparator();
        for (size_t i = 0; i < ps.size(); ++i) m.addItem (100 + (int) i, juce::String::fromUTF8 (ps[i].name), true, gl.presetName() == juce::String::fromUTF8 (ps[i].name));
        juce::Component::SafePointer<GlueEditor> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [sp] (int r) { if (sp == nullptr) return; if (r == 1) sp->gl.resetAll(); else if (r >= 100) sp->gl.applyPreset (r - 100); sp->repaint(); });
    }
    InjGlue& gl; juce::Image lit[3]; float grS = 0.f; bool demo = false;
    static constexpr float bx[3] { 337.f, 542.f, 750.f };
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjGlue::createEditor() { return std::make_unique<GlueEditor> (*this); }
} // namespace injfx
