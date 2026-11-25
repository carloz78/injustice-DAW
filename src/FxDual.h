#pragma once
#include "FxOptical.h"
// Dual Comp: compresor de dos canales con válvulas (estilo Fairchild): izquierda-derecha o lateral-vertical (M/S), seis constantes de tiempo, dos medidores.
namespace injfx
{
class InjDual : public InjFx
{
public:
    enum { pIn0, pThr0, pTc0, pIn1, pThr1, pTc1, pMode, pMeter, pOn };
    static inline const char* xmlTypeName = "injdual";
    static const char* getPluginName() { return "Dual Comp"; }
    InjDual (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Dual"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 9; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    std::vector<Preset> presets() const override
    {   //                       In0  Thr0 Tc0 In1 Thr1 Tc1 Modo Meter On
        return { { "Bus estéreo", { 6.f, -14.f, 2.f, 6.f, -14.f, 2.f, 0.f, 1.f, 1.f } }, { "Mid/Side ancho", { 6.f, -12.f, 3.f, 4.f, -20.f, 1.f, 1.f, 1.f, 1.f } }, { "Bus rápido", { 8.f, -18.f, 0.f, 8.f, -18.f, 0.f, 0.f, 1.f, 1.f } },
                 { "Master lento", { 4.f, -10.f, 3.f, 4.f, -10.f, 3.f, 0.f, 1.f, 1.f } }, { "Aplastar", { 12.f, -28.f, 4.f, 12.f, -28.f, 4.f, 0.f, 1.f, 1.f } } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; gr[0] = gr[1] = 0.f; }
    static float atkMs (int t) { static const float a[] { .2f, .2f, .4f, .8f, 2.f, 4.f }; return a[juce::jlimit (0, 5, t)]; }
    static float relMs (int t) { static const float r[] { 300.f, 800.f, 2000.f, 5000.f, 2500.f, 6000.f }; return r[juce::jlimit (0, 5, t)]; }
    std::atomic<float> inA { -90.f }, inB { -90.f };
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p0 = fc.destBuffer->getWritePointer (0, fc.bufferStartSample); float* p1 = nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : p0;
        if (vi (pOn) == 0) { meterA.store (0.f); meterB.store (0.f); inA.store (-90.f); inB.store (-90.f); return; }
        const bool ms = vi (pMode) != 0; const float gi[2] { v (pIn0), v (pIn1) }, th[2] { v (pThr0), v (pThr1) }; const int tc[2] { vi (pTc0), vi (pTc1) };
        float aA[2], aR[2], gl[2], go[2]; for (int c = 0; c < 2; ++c) { aA[c] = coefMs (atkMs (tc[c]), sr); aR[c] = coefMs (relMs (tc[c]), sr); gl[c] = db2lin (gi[c]); go[c] = db2lin (-.5f * gi[c]); }
        float mx[2] { 0.f, 0.f }, pk[2] { 0.f, 0.f };
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            float x[2] { p0[i], p1[i] }; if (ms) { const float m = (x[0] + x[1]) * .5f, s = (x[0] - x[1]) * .5f; x[0] = m; x[1] = s; }
            for (int c = 0; c < 2; ++c)
            {
                const float y = x[c] * gl[c], d = lin2db (std::abs (y)); pk[c] = std::max (pk[c], std::abs (x[c]));
                const float target = std::max (0.f, d - InjComp::curve (d, th[c], 4.f, 18.f)), k = target > gr[c] ? aA[c] : aR[c]; gr[c] = k * gr[c] + (1.f - k) * target; mx[c] = std::max (mx[c], gr[c]);
                x[c] = y * db2lin (-gr[c]) * go[c];
            }
            if (ms) { const float l = x[0] + x[1], r = x[0] - x[1]; x[0] = l; x[1] = r; }
            p0[i] = x[0]; p1[i] = x[1];
        }
        meterA.store (mx[0]); meterB.store (mx[1]); inA.store (lin2db (pk[0])); inB.store (lin2db (pk[1]));
    }
private:
    static inline const Spec S[] = {
        { "in0", "Entrada 1", 0.f, 20.f, 6.f, 0.f, fDb, "" }, { "thr0", "Umbral 1", -40.f, 0.f, -14.f, 0.f, fDb, "" }, { "tc0", "Constante de tiempo 1", 0.f, 5.f, 2.f, 0.f, fChoice, "1|2|3|4|5|6" },
        { "in1", "Entrada 2", 0.f, 20.f, 6.f, 0.f, fDb, "" }, { "thr1", "Umbral 2", -40.f, 0.f, -14.f, 0.f, fDb, "" }, { "tc1", "Constante de tiempo 2", 0.f, 5.f, 2.f, 0.f, fChoice, "1|2|3|4|5|6" },
        { "mode", "Modo", 0.f, 1.f, 0.f, 0.f, fChoice, "Izquierda - Derecha|Lateral - Vertical" }, { "meter", "Medidor", 0.f, 1.f, 1.f, 0.f, fChoice, "Entrada|Reducción" }, { "on", "Encendido", 0.f, 1.f, 1.f, 0.f, fChoice, "No|Sí" } };
    double sr = 44100.0; float gr[2] { 0.f, 0.f };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjDual)
};

class DualEditor : public SkinEditor
{
public:
    DualEditor (InjDual& p) : SkinEditor (p, skinImage ("dual_bg_png"), .46f), du (p)
    {
        const float kx[] { 684.2f, 969.6f, 686.f, 975.1f }, ky[] { 320.6f, 306.5f, 641.1f, 629.4f }, kr[] { 53.4f, 68.3f, 55.9f, 65.9f };
        const int idx[] { InjDual::pIn0, InjDual::pThr0, InjDual::pIn1, InjDual::pThr1 }; const char* cn[] { "dual_cap0_png", "dual_cap1_png", "dual_cap2_png", "dual_cap3_png" };
        for (int k = 0; k < 4; ++k) knobs.push_back ({ idx[k], kx[k], ky[k], kr[k], skinImage (cn[k]) });
        knobs.push_back ({ InjDual::pTc0, 1267.f, 319.f, 58.f, skinImage ("dual_sel0_png"), -91.f, 91.f }); knobs.push_back ({ InjDual::pTc1, 1267.f, 649.f, 58.f, skinImage ("dual_sel1_png"), -91.f, 91.f });
        const char* tn[] { "dual_mode", "dual_meter", "dual_on" }; for (int k = 0; k < 3; ++k) { tup[k] = skinImage ((juce::String (tn[k]) + "_up_png").toRawUTF8()); tdn[k] = skinImage ((juce::String (tn[k]) + "_dn_png").toRawUTF8()); }
        lampOn = skinImage ("dual_lamp_on_png"); lampOff = skinImage ("dual_lamp_off_png");
        const int pi[] { InjDual::pMode, InjDual::pMeter, InjDual::pOn };
        for (int k = 0; k < 3; ++k) hots.push_back ({ { 1543.f - 60.f, ty[k] - 70.f, 120.f, 140.f }, [this, k, pi] { choose (pi[k], du.vi (pi[k]) ? 0 : 1); } });
        hots.push_back ({ { 560.f, 30.f, 700.f, 60.f }, [this] { presetMenu(); } });
        if (std::getenv ("INJ_PUNCHDEMO")) { du.applyPreset (4); gs[0] = 9.f; gs[1] = 6.f; demo = true; }
    }
protected:
    void tick() override
    {
        if (! demo) { const bool gr = du.vi (InjDual::pMeter) != 0; const float a = gr ? du.meterA.load() : du.inA.load(), b = gr ? du.meterB.load() : du.inB.load(); gs[0] = gr ? std::max (a, gs[0] * .9f) : a; gs[1] = gr ? std::max (b, gs[1] * .9f) : b; }
        repaint();
    }
    void paintSkin (juce::Graphics& g) override
    {
        const int pi[] { InjDual::pMode, InjDual::pMeter, InjDual::pOn };
        for (int k = 0; k < 3; ++k) drawSprite (g, du.vi (pi[k]) == (k == 0 ? 0 : 1) ? tup[k] : tdn[k], 1543.f, ty[k]);
        drawSprite (g, du.vi (InjDual::pOn) ? lampOn : lampOff, 1543.f, 576.f);
        static const float vdb[] { -20.f, -10.f, -7.f, -5.f, -3.f, -2.f, -1.f, 0.f }, ang[] { -35.3f, -23.7f, -12.9f, -1.8f, 10.1f, 20.1f, 28.8f, 37.9f };
        for (int c = 0; c < 2; ++c)
        {
            const bool gr = du.vi (InjDual::pMeter) != 0; const float v = gr ? -juce::jlimit (0.f, 20.f, gs[c]) : juce::jlimit (-20.f, 0.f, gs[c]); float a = ang[7];
            for (int i = 0; i < 7; ++i) if (v >= vdb[i] && v <= vdb[i + 1]) a = ang[i] + (ang[i + 1] - ang[i]) * (v - vdb[i]) / (vdb[i + 1] - vdb[i]);
            drawNeedle (g, 332.2f, 430.7f + 330.f * (float) c, a, 60.f, 167.f, { 183.f, 206.f + 330.f * (float) c, 302.f, 170.f }, juce::Colour (0xff120e0a), 3.2f);
        }
    }
private:
    void presetMenu()
    {
        juce::PopupMenu m; auto ps = du.presets(); m.addItem (1, "Inicial"); m.addSeparator();
        for (size_t i = 0; i < ps.size(); ++i) m.addItem (100 + (int) i, juce::String::fromUTF8 (ps[i].name), true, du.presetName() == juce::String::fromUTF8 (ps[i].name));
        juce::Component::SafePointer<DualEditor> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [sp] (int r) { if (sp == nullptr) return; if (r == 1) sp->du.resetAll(); else if (r >= 100) sp->du.applyPreset (r - 100); sp->repaint(); });
    }
    InjDual& du; juce::Image tup[3], tdn[3], lampOn, lampOff; float gs[2] { 0.f, 0.f }; bool demo = false;
    static constexpr float ty[3] { 258.f, 450.f, 723.f };
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjDual::createEditor() { return std::make_unique<DualEditor> (*this); }
} // namespace injfx
