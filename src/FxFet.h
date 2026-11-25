#pragma once
#include "FxPunch.h"
// FET Compressor: compresor de picos tipo 1176 (entrada que empuja contra un umbral fijo, 4 ratios, VU de reducción). Ventana con el panel de equipo.
namespace injfx
{
class InjFet : public InjFx
{
public:
    enum { pIn, pOut, pAtk, pRel, pRatio };
    static inline const char* xmlTypeName = "injfet";
    static const char* getPluginName() { return "FET Compressor"; }
    InjFet (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "FET"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 5; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    std::vector<Preset> presets() const override
    {   //                       Entrada Salida Ataque Lib. Ratio
        return { { "Bombo con punch", { 26.f, 20.f, 3.f, 5.f, 2.f } }, { "Voz clásica", { 22.f, 24.f, 4.f, 4.f, 1.f } }, { "Bus de batería", { 20.f, 25.f, 5.f, 3.f, 1.f } },
                 { "Aplastar todo", { 36.f, 16.f, 7.f, 6.f, 3.f } }, { "Suave", { 14.f, 26.f, 2.f, 2.f, 0.f } } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; gr = 0.f; }
    static float atkMs (float k) { return .8f * std::pow (.025f, (k - 1.f) / 6.f); }            // 1 = lento (0.8 ms), 7 = rápido (0.02 ms)
    static float relMs (float k) { return 1100.f * std::pow (50.f / 1100.f, (k - 1.f) / 6.f); } // 1 = lento (1.1 s), 7 = rápido (50 ms)
    static float ratioOf (int i) { static const float r[] { 4.f, 8.f, 12.f, 20.f }; return r[juce::jlimit (0, 3, i)]; }
    static constexpr float kThr = -18.f;
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p0 = fc.destBuffer->getWritePointer (0, fc.bufferStartSample); float* p1 = nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : p0;
        const float gIn = db2lin (v (pIn)), gOut = db2lin (v (pOut) - 24.f), R = ratioOf (vi (pRatio));
        const float aA = std::exp (-1.f / (atkMs (v (pAtk)) * .001f * (float) sr)), aR = std::exp (-1.f / (relMs (v (pRel)) * .001f * (float) sr));
        float maxGr = 0.f, peakIn = 0.f;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            const float l = p0[i] * gIn, r = p1[i] * gIn, pk = std::max (std::abs (l), std::abs (r)); peakIn = std::max (peakIn, pk);
            const float x = lin2db (pk), target = std::max (0.f, x - InjComp::curve (x, kThr, R, 3.f)), k = target > gr ? aA : aR; gr = k * gr + (1.f - k) * target; maxGr = std::max (maxGr, gr);
            const float g = db2lin (-gr) * gOut; p0[i] = l * g; p1[i] = r * g;
        }
        meterA.store (maxGr); meterB.store (lin2db (peakIn));
    }
private:
    static inline const Spec S[] = {
        { "in", "Entrada", 0.f, 48.f, 24.f, 0.f, fDb, "" }, { "out", "Salida", 0.f, 48.f, 24.f, 0.f, fNum, "" }, { "atk", "Ataque", 1.f, 7.f, 3.f, 0.f, fNum, "" }, { "rel", "Liberación", 1.f, 7.f, 4.f, 0.f, fNum, "" },
        { "ratio", "Ratio", 0.f, 3.f, 1.f, 0.f, fChoice, "4:1|8:1|12:1|20:1" } };
    double sr = 44100.0; float gr = 0.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjFet)
};

class FetEditor : public SkinEditor
{
public:
    FetEditor (InjFet& p) : SkinEditor (p, skinImage ("fet_bg_png"), .46f), fe (p)
    {
        const float kx[] { 334.1f, 758.8f, 1108.7f, 1110.4f }, ky[] { 369.8f, 369.7f, 249.4f, 493.f }, kr[] { 89.9f, 89.7f, 38.5f, 41.1f }; const char* cn[] { "fet_cap0_png", "fet_cap1_png", "fet_cap2_png", "fet_cap3_png" };
        for (int k = 0; k < 4; ++k) knobs.push_back ({ k, kx[k], ky[k], kr[k], skinImage (cn[k]) });
        const char* ln[] { "fet_lit4_png", "fet_lit8_png", "fet_lit12_png", "fet_lit20_png" }; for (int k = 0; k < 4; ++k) lit[k] = skinImage (ln[k]);
        for (int k = 0; k < 4; ++k) hots.push_back ({ { 1317.f, slotY[k] - 48.f, 110.f, 96.f }, [this, k] { choose (InjFet::pRatio, 3 - k); } });   // arriba 20:1 ... abajo 4:1
        hots.push_back ({ { 215.f, 20.f, 520.f, 130.f }, [this] { presetMenu(); } });                                                           // el logo abre los preajustes
        hots.push_back ({ { 1483.f, 543.f, 540.f, 40.f }, [this] { fe.setEnabled (! fe.isEnabled()); } });                                     // "FET COMPRESSOR" enciende y apaga
        if (std::getenv ("INJ_PUNCHDEMO")) { fe.applyPreset (3); grS = 9.f; demo = true; }
    }
protected:
    void tick() override { if (! demo) grS = std::max (fe.meterA.load(), grS * .9f); repaint(); }
    void paintSkin (juce::Graphics& g) override
    {
        const int sel = fe.vi (InjFet::pRatio); drawSprite (g, lit[sel], 1372.f, slotY[3 - sel]);
        // aguja: 0 dB de reducción = cero de la escala; ángulos medidos de la escala de la imagen
        static const float vdb[] { -20.f, -10.f, -7.f, -5.f, -3.f, -2.f, -1.f, 0.f }, ang[] { -27.7f, -20.8f, -15.2f, -9.2f, -3.7f, .3f, 4.2f, 9.6f };
        const float v = -juce::jlimit (0.f, 20.f, grS); float a = ang[7];
        for (int i = 0; i < 7; ++i) if (v >= vdb[i] && v <= vdb[i + 1]) a = ang[i] + (ang[i + 1] - ang[i]) * (v - vdb[i]) / (vdb[i + 1] - vdb[i]);
        drawNeedle (g, 1793.2f, 678.7f, a, 320.f, 368.f, { 1563.f, 236.f, 456.f, 208.f }, juce::Colour (0xff16120e), 3.4f);
    }
private:
    void presetMenu()
    {
        juce::PopupMenu m; auto ps = fe.presets(); m.addItem (1, "Inicial"); m.addSeparator();
        for (size_t i = 0; i < ps.size(); ++i) m.addItem (100 + (int) i, juce::String::fromUTF8 (ps[i].name), true, fe.presetName() == juce::String::fromUTF8 (ps[i].name));
        juce::Component::SafePointer<FetEditor> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [sp] (int r) { if (sp == nullptr) return; if (r == 1) sp->fe.resetAll(); else if (r >= 100) sp->fe.applyPreset (r - 100); sp->repaint(); });
    }
    InjFet& fe; juce::Image lit[4]; float grS = 0.f; bool demo = false;
    static constexpr float slotY[4] { 240.f, 336.f, 431.f, 526.f };
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjFet::createEditor() { return std::make_unique<FetEditor> (*this); }
} // namespace injfx
