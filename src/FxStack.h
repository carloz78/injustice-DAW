#pragma once
#include "FxMband.h"
// Stack Comp: compresor vertical de rack (umbral, ratio por pasos 1.5 a infinito, ataque, liberación con AUTO, ganancia de salida, mezcla en paralelo),
// con botones IN, HPF (filtro en la cadena lateral) y F.REL (liberación rápida), y VU de reducción.
namespace injfx
{
class InjStack : public InjFx
{
public:
    enum { pThr, pRatio, pAtk, pRel, pMk, pMix, pIn, pHpf, pFRel };
    static inline const char* xmlTypeName = "injstack";
    static const char* getPluginName() { return "Stack Comp"; }
    InjStack (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "StackComp"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 9; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    std::vector<Preset> presets() const override
    {   //                      Umbral Ratio Ataque Lib  Gan  Mezcla In Hpf FRel
        return { { "Control suave", { -12.f, 1.f, 10.f, .3f, 2.f, 100.f, 1.f, 0.f, 0.f } }, { "Voz presente", { -16.f, 2.f, 3.f, .2f, 4.f, 100.f, 1.f, 0.f, 0.f } }, { "Bombo con pegada", { -18.f, 3.f, 30.f, .1f, 3.f, 100.f, 1.f, 1.f, 1.f } },
                 { "Paralela", { -26.f, 4.f, .3f, .1f, 8.f, 35.f, 1.f, 1.f, 1.f } }, { "Limitar", { -8.f, 5.f, .03f, .05f, 0.f, 100.f, 1.f, 0.f, 0.f } } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; gr = 0.f; hp.reset(); }
    static float ratioOf (int i) { static const float r[] { 1.5f, 2.f, 3.f, 4.f, 6.f, 100.f }; return r[juce::jlimit (0, 5, i)]; }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nullptr }; p[1] = nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : p[0];
        const bool on = vi (pIn) != 0, useHp = vi (pHpf) != 0, fast = vi (pFRel) != 0; const float T = v (pThr), R = ratioOf (vi (pRatio)), mk = db2lin (v (pMk)), mix = v (pMix) * .01f, rel = v (pRel); const bool autoRel = rel >= 1.45f;
        hp.set (BQ::HP, sr, 100.0, .707, 0.0); const float aA = coefMs (v (pAtk), sr); const float relS = std::max (.005f, (fast ? .25f : 1.f) * rel); float maxGr = 0.f;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            float l = p[0][i], r = p[1][i];
            if (on)
            {
                const float s = std::max (std::abs (useHp ? hp.run (0, l) : l), std::abs (useHp ? hp.run (1, r) : r)), x = lin2db (s + 1e-9f), target = std::max (0.f, x - InjComp::curve (x, T, R, 6.f));
                const float rr = autoRel ? coefMs ((600.f - 560.f * juce::jlimit (0.f, 1.f, gr / 10.f)) * (fast ? .25f : 1.f), sr) : coefMs (relS * 1000.f, sr);
                gr = target > gr ? aA * gr + (1.f - aA) * target : rr * gr + (1.f - rr) * target; maxGr = std::max (maxGr, gr);
                const float g = db2lin (-gr) * mk; l = l * (1.f - mix) + l * g * mix; r = r * (1.f - mix) + r * g * mix;
            }
            p[0][i] = l; p[1][i] = nc > 1 ? r : l;
        }
        meterA.store (maxGr);
    }
private:
    static inline const Spec S[] = {
        { "thr", "Umbral", -30.f, 10.f, -12.f, 0.f, fDb, "" }, { "ratio", "Ratio", 0.f, 5.f, 3.f, 0.f, fChoice, "1.5:1|2:1|3:1|4:1|6:1|inf:1" }, { "atk", "Ataque", .03f, 30.f, 3.f, 1.f, fMs, "" }, { "rel", "Liberación", .02f, 1.5f, .3f, .3f, fSec, "" },
        { "mk", "Ganancia de salida", -10.f, 20.f, 0.f, 0.f, fDb, "" }, { "mix", "Mezcla", 0.f, 100.f, 100.f, 0.f, fPct, "" }, { "in", "Compresor activo", 0.f, 1.f, 1.f, 0.f, fChoice, "No|Sí" },
        { "hpf", "Filtro en la cadena lateral", 0.f, 1.f, 0.f, 0.f, fChoice, "No|Sí" }, { "frel", "Liberación rápida", 0.f, 1.f, 0.f, 0.f, fChoice, "No|Sí" } };
    double sr = 44100.0; BQ hp; float gr = 0.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjStack)
};

class StackEditor : public SkinEditor
{
public:
    StackEditor (InjStack& p) : SkinEditor (p, skinImage ("vc_bg_png"), .55f, { 335, 0, 356, 1536 }), sk (p)
    {
        struct K { int idx, cap; float x, y, rot, span; };
        const K ks[] { { InjStack::pThr, 0, 528.2f, 460.3f, 2.f, 112.f }, { InjStack::pRatio, 1, 528.2f, 603.4f, 2.f, 0.f }, { InjStack::pAtk, 2, 528.3f, 741.9f, -139.f, 112.f }, { InjStack::pRel, 3, 528.3f, 884.2f, 36.f, 112.f },
            { InjStack::pMk, 4, 528.4f, 1031.1f, 1.f, 115.f }, { InjStack::pMix, 5, 511.9f, 1369.2f, 0.f, 130.f } };
        for (auto& k : ks) { Knob kn { k.idx, k.x, k.y, 36.f, skinImage (("vc_cap" + juce::String (k.cap) + "_png").toRawUTF8()) }; kn.a0 = -k.span; kn.a1 = k.span; kn.rot0 = k.rot; if (k.cap == 1) kn.stops = { -95.f, -51.f, 0.f, 50.f, 95.f, 119.f }; knobs.push_back (kn); }
        const char* bn[3] { "in", "hpf", "frel" }; const int bi[3] { InjStack::pIn, InjStack::pHpf, InjStack::pFRel };
        for (int k = 0; k < 3; ++k) { bOn[k] = skinImage (("vc_" + juce::String (bn[k]) + "_on_png").toRawUTF8()); bOff[k] = skinImage (("vc_" + juce::String (bn[k]) + "_off_png").toRawUTF8()); hots.push_back ({ { bx[k] - 40.f, 1135.f, 80.f, 80.f }, [this, k, bi] { choose (bi[k], sk.vi (bi[k]) ? 0 : 1); } }); }
        hots.push_back ({ { 440.f, 60.f, 230.f, 90.f }, [this] { presetMenu(); } });
        if (std::getenv ("INJ_PUNCHDEMO")) { sk.applyPreset (2); gr = 8.f; demo = true; }
    }
protected:
    void tick() override { if (! demo) gr = std::max (sk.meterA.load(), gr * .9f); repaint(); }
    void paintSkin (juce::Graphics& g) override
    {
        const int bi[3] { InjStack::pIn, InjStack::pHpf, InjStack::pFRel }; for (int k = 0; k < 3; ++k) drawSprite (g, sk.vi (bi[k]) ? bOn[k] : bOff[k], bx[k], 1175.f);
        const float deg = -44.f + juce::jlimit (0.f, 20.f, gr) / 20.f * 86.f; drawNeedle (g, 510.f, 338.f, deg, 57.f, 132.f, { 396.f, 176.f, 234.f, 126.f }, juce::Colour (0xfff4f4f2), 3.2f);
    }
private:
    void presetMenu()
    {
        juce::PopupMenu m; auto ps = sk.presets(); m.addItem (1, "Inicial"); m.addSeparator();
        for (size_t i = 0; i < ps.size(); ++i) m.addItem (100 + (int) i, juce::String::fromUTF8 (ps[i].name), true, sk.presetName() == juce::String::fromUTF8 (ps[i].name));
        juce::Component::SafePointer<StackEditor> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [sp] (int r) { if (sp == nullptr) return; if (r == 1) sp->sk.resetAll(); else if (r >= 100) sp->sk.applyPreset (r - 100); sp->repaint(); });
    }
    InjStack& sk; juce::Image bOn[3], bOff[3]; float gr = 0.f; bool demo = false;
    static constexpr float bx[3] { 418.f, 518.f, 608.f };
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjStack::createEditor() { return std::make_unique<StackEditor> (*this); }
} // namespace injfx
