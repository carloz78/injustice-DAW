#pragma once
#include "FxGlue.h"
// Optical Comp: compresor óptico con tubos (estilo LA-2A): Peak Reduction, Gain, Output, Compress/Limit y liberación que depende del programa.
namespace injfx
{
class InjOptical : public InjFx
{
public:
    enum { pGain, pPeak, pOut, pLimit };
    static inline const char* xmlTypeName = "injoptical";
    static const char* getPluginName() { return "Optical Comp"; }
    InjOptical (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Optical"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 4; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    std::vector<Preset> presets() const override
    {   //                       Gain Peak Salida Limit
        return { { "Voz suave", { 30.f, 40.f, 10.f, 0.f } }, { "Bajo estable", { 35.f, 50.f, 10.f, 1.f } }, { "Bus cálido", { 20.f, 30.f, 10.f, 0.f } }, { "Limitar fuerte", { 45.f, 70.f, 10.f, 1.f } }, { "Casi nada", { 10.f, 15.f, 10.f, 0.f } } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; gF = gS = 0.f; }
    static float thrOf (float peak) { return -peak * .5f; }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p0 = fc.destBuffer->getWritePointer (0, fc.bufferStartSample); float* p1 = nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : p0;
        const float T = thrOf (v (pPeak)), R = vi (pLimit) ? 10.f : 3.f, mk = db2lin (v (pGain) * .25f), out = db2lin ((v (pOut) - 10.f) * 2.4f);
        const float aAf = coefMs (10.f, sr), aRf = coefMs (80.f, sr), aAs = coefMs (400.f, sr), aRs = coefMs (2000.f, sr);
        float maxGr = 0.f;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            float l = p0[i], r = p1[i];
            const float x = lin2db (std::max (std::abs (l), std::abs (r))), target = std::max (0.f, x - InjComp::curve (x, T, R, 12.f));
            const float kf = target > gF ? aAf : aRf, ks = target > gS ? aAs : aRs; gF = kf * gF + (1.f - kf) * target; gS = ks * gS + (1.f - ks) * target;
            const float gr = std::max (gF, gS); maxGr = std::max (maxGr, gr);
            const float g = db2lin (-gr) * mk * out; l *= g; r *= g;
            p0[i] = l + .02f * l * std::abs (l); p1[i] = r + .02f * r * std::abs (r);   // un toque de armónicos pares, como el tubo
        }
        meterA.store (maxGr);
    }
private:
    static inline const Spec S[] = {
        { "gain", "Gain", 0.f, 100.f, 20.f, 0.f, fNum, "" }, { "peak", "Peak Reduction", 0.f, 100.f, 30.f, 0.f, fNum, "" }, { "out", "Output", 0.f, 10.f, 10.f, 0.f, fNum, "" }, { "limit", "Modo", 0.f, 1.f, 0.f, 0.f, fChoice, "Compress|Limit" } };
    double sr = 44100.0; float gF = 0.f, gS = 0.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjOptical)
};

class OpticalEditor : public SkinEditor
{
public:
    OpticalEditor (InjOptical& p) : SkinEditor (p, skinImage ("opt_bg_png"), .46f), op (p)
    {
        const float kx[] { 551.6f, 1605.8f, 1932.8f }, ky[] { 413.2f, 413.4f, 334.3f }, kr[] { 58.5f, 58.f, 36.8f };
        const int idx[] { InjOptical::pGain, InjOptical::pPeak, InjOptical::pOut }; const char* cn[] { "opt_cap0_png", "opt_cap1_png", "opt_cap2_png" };
        for (int k = 0; k < 3; ++k) knobs.push_back ({ idx[k], kx[k], ky[k], kr[k], skinImage (cn[k]) });
        up = skinImage ("opt_tg_up_png"); dn = skinImage ("opt_tg_dn_png");
        hots.push_back ({ { 160.f, 350.f, 150.f, 170.f }, [this] { choose (InjOptical::pLimit, op.vi (InjOptical::pLimit) ? 0 : 1); } });
        hots.push_back ({ { 700.f, 30.f, 420.f, 80.f }, [this] { presetMenu(); } });
        hots.push_back ({ { 700.f, 112.f, 460.f, 40.f }, [this] { op.setEnabled (! op.isEnabled()); } });
        if (std::getenv ("INJ_PUNCHDEMO")) { op.applyPreset (3); grS = 9.f; demo = true; }
    }
protected:
    void tick() override { if (! demo) grS = std::max (op.meterA.load(), grS * .9f); repaint(); }
    void paintSkin (juce::Graphics& g) override
    {
        drawSprite (g, op.vi (InjOptical::pLimit) ? dn : up, 238.f, 432.f);
        static const float vdb[] { -20.f, -10.f, -7.f, -5.f, -3.f, -2.f, -1.f, 0.f }, ang[] { -26.5f, -18.6f, -13.1f, -7.8f, -2.4f, 1.1f, 4.8f, 10.3f };
        const float v = -juce::jlimit (0.f, 20.f, grS); float a = ang[7];
        for (int i = 0; i < 7; ++i) if (v >= vdb[i] && v <= vdb[i + 1]) a = ang[i] + (ang[i + 1] - ang[i]) * (v - vdb[i]) / (vdb[i + 1] - vdb[i]);
        drawNeedle (g, 1089.5f, 738.3f, a, 250.f, 384.f, { 856.f, 268.f, 460.f, 223.f }, juce::Colour (0xff16120e), 3.4f);
    }
private:
    void presetMenu()
    {
        juce::PopupMenu m; auto ps = op.presets(); m.addItem (1, "Inicial"); m.addSeparator();
        for (size_t i = 0; i < ps.size(); ++i) m.addItem (100 + (int) i, juce::String::fromUTF8 (ps[i].name), true, op.presetName() == juce::String::fromUTF8 (ps[i].name));
        juce::Component::SafePointer<OpticalEditor> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [sp] (int r) { if (sp == nullptr) return; if (r == 1) sp->op.resetAll(); else if (r >= 100) sp->op.applyPreset (r - 100); sp->repaint(); });
    }
    InjOptical& op; juce::Image up, dn; float grS = 0.f; bool demo = false;
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjOptical::createEditor() { return std::make_unique<OpticalEditor> (*this); }
} // namespace injfx
