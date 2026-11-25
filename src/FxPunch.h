#pragma once
#include "FxSkin.h"
// Punch Comp: compresor tipo FET (picos rápidos, ratio hasta NUKE), con filtro del detector, enlace estéreo y saturación. Ventana con el panel de equipo.
namespace injfx
{
class InjPunch : public InjFx
{
public:
    enum { pIn, pAtk, pRel, pOut, pRatio, pDet, pLink, pSat, pByp };
    static inline const char* xmlTypeName = "injpunch";
    static const char* getPluginName() { return "Punch Comp"; }
    InjPunch (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Punch"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 9; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    std::vector<Preset> presets() const override
    {   //                                   Entrada Ataque Lib. Salida Ratio Detector Enlace Satur. Bypass
        return { { "Bombo gabber", { 6.f, 2.f, 3.f, 3.f, 6.f, 1.f, 1.f, 2.f, 0.f } }, { "Bus pegado", { 4.f, 4.f, 5.f, 2.f, 3.f, 0.f, 1.f, 0.f, 0.f } }, { "Voz firme", { 3.f, 3.f, 5.f, 2.f, 2.f, 1.f, 1.f, 0.f, 0.f } },
                 { "Destrucción NUKE", { 8.f, 0.f, 2.f, 4.f, 7.f, 0.f, 1.f, 3.f, 0.f } }, { "Transparente", { 2.f, 5.f, 6.f, 1.f, 1.f, 0.f, 1.f, 0.f, 0.f } } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; gr[0] = gr[1] = 0.f; for (auto& b : flt) b.reset(); lastDet = -1; }
    static float inDb (float k) { return k * 3.f; }                                  // 0..10 -> 0..30 dB de ganancia previa
    static float outDb (float k) { return k * 2.4f; }                                // 0..10 -> 0..24 dB de ganancia de salida
    static float atkMs (float k) { return .05f * std::pow (10.f, k * .27f); }       // 0.05 .. 25 ms
    static float relMs (float k) { return 20.f * std::pow (10.f, k * .17f); }       // 20 .. 1000 ms
    static float ratioOf (int i) { static const float r[] { 1.f, 2.f, 3.f, 4.f, 6.f, 10.f, 20.f, 100.f }; return r[juce::jlimit (0, 7, i)]; }
    static constexpr float kThr = -14.f;                                             // umbral fijo (como en el 1176): la entrada decide cuánto se comprime
    static float shape (float y, int mode)
    {
        if (mode == 1) return std::tanh (1.5f * y) / 1.5f;
        if (mode == 2) return std::tanh (3.f * y) / 3.f * 1.25f;
        if (mode == 3) return std::tanh (7.f * y) / 7.f * 2.2f;
        return y;
    }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p0 = fc.destBuffer->getWritePointer (0, fc.bufferStartSample); float* p1 = nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : p0;
        const float gIn = db2lin (inDb (v (pIn))), gOut = db2lin (outDb (v (pOut))), R = ratioOf (vi (pRatio)); const int det = vi (pDet), sat = vi (pSat); const bool link = vi (pLink) != 0, byp = vi (pByp) != 0;
        if (det != lastDet) { lastDet = det; for (auto& b : flt) b.reset(); flt[0].set (BQ::HP, sr, det == 2 ? 150.0 : 120.0, .7071); flt[1].set (BQ::HP, sr, det == 2 ? 150.0 : 120.0, .7071); flt[2].set (BQ::LP, sr, det == 2 ? 4000.0 : 3000.0, .7071); flt[3].set (BQ::LP, sr, det == 2 ? 4000.0 : 3000.0, .7071); }
        const float aA = std::exp (-1.f / (atkMs (v (pAtk)) * .001f * (float) sr)), aR = std::exp (-1.f / (relMs (v (pRel)) * .001f * (float) sr));
        float peakIn = 0.f, maxGr = 0.f;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            const float l0 = p0[i], r0 = p1[i], l = l0 * gIn, r = r0 * gIn; peakIn = std::max (peakIn, std::max (std::abs (l), std::abs (r)));
            float dl = l, dr = r;
            if (det == 1 || det == 2) { dl = flt[0].run (0, dl); dr = flt[1].run (0, dr); }
            if (det == 2 || det == 3) { dl = flt[2].run (0, dl); dr = flt[3].run (0, dr); }
            float tg[2];
            for (int c = 0; c < 2; ++c)
            {
                const float lv = link ? std::max (std::abs (dl), std::abs (dr)) : std::abs (c == 0 ? dl : dr), x = lin2db (lv);
                tg[c] = std::max (0.f, x - InjComp::curve (x, kThr, R, 6.f));
            }
            for (int c = 0; c < 2; ++c) { const float k = tg[c] > gr[c] ? aA : aR; gr[c] = k * gr[c] + (1.f - k) * tg[c]; }
            if (link) gr[1] = gr[0];
            maxGr = std::max (maxGr, std::max (gr[0], gr[1]));
            if (byp) { p0[i] = l0; p1[i] = r0; continue; }
            p0[i] = shape (l * db2lin (-gr[0]), sat) * gOut; p1[i] = shape (r * db2lin (-gr[1]), sat) * gOut;
        }
        meterA.store (maxGr); meterB.store (lin2db (peakIn));
    }
private:
    static inline const Spec S[] = {
        { "in", "Entrada", 0.f, 10.f, 3.f, 0.f, fNum, "" }, { "atk", "Ataque", 0.f, 10.f, 3.f, 0.f, fNum, "" }, { "rel", "Liberación", 0.f, 10.f, 4.f, 0.f, fNum, "" }, { "out", "Salida", 0.f, 10.f, 3.f, 0.f, fNum, "" },
        { "ratio", "Ratio", 0.f, 7.f, 3.f, 0.f, fChoice, "1:1|2:1|3:1|4:1|6:1|10:1|20:1|NUKE" }, { "det", "Detector", 0.f, 3.f, 0.f, 0.f, fChoice, "Libre|HP|BP|LP" },
        { "link", "Enlace estéreo", 0.f, 1.f, 1.f, 0.f, fChoice, "No|Sí" }, { "sat", "Saturación", 0.f, 3.f, 0.f, 0.f, fChoice, "Apagada|Dist 2|Dist 3|NUKE" }, { "byp", "Bypass", 0.f, 1.f, 0.f, 0.f, fChoice, "No|Sí" } };
    double sr = 44100.0; float gr[2] { 0.f, 0.f }; int lastDet = -1; BQ flt[4];
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjPunch)
};

class PunchEditor : public SkinEditor
{
public:
    PunchEditor (InjPunch& p) : SkinEditor (p, skinImage ("punch_bg_png"), .46f), pu (p)
    {
        leds = skinImage ("punch_leds_png");
        const float kx[] { 554.9f, 961.9f, 1370.7f, 1774.4f }, ky[] { 522.9f, 524.4f, 524.2f, 523.8f }, kr[] { 73.f, 72.3f, 72.3f, 72.5f }; const char* cn[] { "punch_cap0_png", "punch_cap1_png", "punch_cap2_png", "punch_cap3_png" };
        for (int k = 0; k < 4; ++k) knobs.push_back ({ k, kx[k], ky[k], kr[k], skinImage (cn[k]) });
        for (int k = 0; k < 8; ++k) hots.push_back ({ { ratioX[k] - 34.f, 190.f, 68.f, 56.f }, [this, k] { choose (InjPunch::pRatio, k); } });
        const float dx[] { 1336.f, 1415.f, 1494.f }; for (int k = 0; k < 3; ++k) hots.push_back ({ { dx[k] - 34.f, 138.f, 68.f, 52.f }, [this, k] { choose (InjPunch::pDet, pu.vi (InjPunch::pDet) == k + 1 ? 0 : k + 1); } });
        hots.push_back ({ { 1541.f, 138.f, 68.f, 52.f }, [this] { choose (InjPunch::pLink, pu.vi (InjPunch::pLink) ? 0 : 1); } });
        const float ax[] { 1697.f, 1788.f, 1882.f }; for (int k = 0; k < 3; ++k) hots.push_back ({ { ax[k] - 38.f, 138.f, 76.f, 52.f }, [this, k] { choose (InjPunch::pSat, pu.vi (InjPunch::pSat) == k + 1 ? 0 : k + 1); } });
        hots.push_back ({ { 235.f, 524.f, 60.f, 60.f }, [this] { choose (InjPunch::pByp, pu.vi (InjPunch::pByp) ? 0 : 1); } });
        hots.push_back ({ { 2018.f, 181.f, 80.f, 120.f }, [this] { pu.setEnabled (! pu.isEnabled()); } });
        hots.push_back ({ { 163.f, 190.f, 380.f, 100.f }, [this] { presetMenu(); } });   // el nombre "Punch Comp" abre los preajustes
        if (std::getenv ("INJ_PUNCHDEMO")) { pu.applyPreset (3); grS = 13.f; demo = true; }   // solo capturas
    }
protected:
    void tick() override { if (! demo) grS = std::max (pu.meterA.load(), grS * .9f); repaint(); }
    void paintSkin (juce::Graphics& g) override
    {
        static const float th[] { 30.f, 26.f, 23.f, 20.f, 17.f, 14.f, 12.f, 10.f, 8.f, 6.f, 4.f, 3.f, 2.f, 1.f };
        for (int j = 0; j < 14; ++j) if (grS >= th[j]) drawLed (g, leds, j, 620.f + j * 48.4f, 104.f);
        drawLed (g, leds, 14 + pu.vi (InjPunch::pRatio), ratioX[pu.vi (InjPunch::pRatio)], 214.f);
        const int d = pu.vi (InjPunch::pDet); if (d > 0) drawLed (g, leds, 22 + d - 1, 1336.f + (d - 1) * 79.f, 110.f);
        if (pu.vi (InjPunch::pLink)) drawLed (g, leds, 25, 1574.f, 110.f);
        const int s = pu.vi (InjPunch::pSat); if (s > 0) drawLed (g, leds, 26 + s - 1, s == 1 ? 1696.f : (s == 2 ? 1786.f : 1882.f), s == 3 ? 108.f : 109.f);
        if (pu.vi (InjPunch::pByp)) drawLed (g, leds, 29, 264.f, 452.f);
    }
private:
    void presetMenu()
    {
        juce::PopupMenu m; auto ps = pu.presets(); m.addItem (1, "Inicial"); m.addSeparator();
        for (size_t i = 0; i < ps.size(); ++i) m.addItem (100 + (int) i, juce::String::fromUTF8 (ps[i].name), true, pu.presetName() == juce::String::fromUTF8 (ps[i].name));
        juce::Component::SafePointer<PunchEditor> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [sp] (int r) { if (sp == nullptr) return; if (r == 1) sp->pu.resetAll(); else if (r >= 100) sp->pu.applyPreset (r - 100); sp->repaint(); });
    }
    InjPunch& pu; juce::Image leds; float grS = 0.f; bool demo = false;
    static constexpr float ratioX[8] { 756.f, 816.f, 876.f, 934.f, 998.f, 1060.f, 1132.f, 1214.f };
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjPunch::createEditor() { return std::make_unique<PunchEditor> (*this); }
} // namespace injfx
