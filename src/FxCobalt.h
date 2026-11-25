#pragma once
#include "FxAttackShaper.h"
// Cobalt Limiter: limitador/compresor estéreo de dos lados (entrada, salida, ataque, release, filtro de cadena lateral, modos COMP 1 / COMP 2 / LIMIT, THD, rango de entrada,
// IN por lado, selectores L / R y LINK) con un VU de reducción por lado.
namespace injfx
{
class InjCobalt : public InjFx
{
public:
    static constexpr int per = 9, pLink = 18, pSelL = 19, pSelR = 20;
    static constexpr int INP (int c) { return c * per; }
    static constexpr int OUT (int c) { return c * per + 1; }
    static constexpr int ATK (int c) { return c * per + 2; }
    static constexpr int REL (int c) { return c * per + 3; }
    static constexpr int SC (int c) { return c * per + 4; }
    static constexpr int MODE (int c) { return c * per + 5; }
    static constexpr int THD (int c) { return c * per + 6; }
    static constexpr int HL (int c) { return c * per + 7; }
    static constexpr int INN (int c) { return c * per + 8; }
    static inline const char* xmlTypeName = "injcobalt";
    static const char* getPluginName() { return "Cobalt Limiter"; }
    InjCobalt (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Cobalt"; }
    const Spec* specs() const override { return table().data(); }
    int numSpecs() const override { return 21; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    static std::vector<float> defaults() { std::vector<float> d; for (auto& s : table()) d.push_back (s.def); return d; }
    std::vector<Preset> presets() const override
    {
        auto mk = [] (float in, float out, float atk, float rel, int sc, int mode, int thd) { auto d = defaults(); for (int c = 0; c < 2; ++c) { d[(size_t) INP (c)] = in; d[(size_t) OUT (c)] = out; d[(size_t) ATK (c)] = atk; d[(size_t) REL (c)] = rel; d[(size_t) SC (c)] = (float) sc; d[(size_t) MODE (c)] = (float) mode; d[(size_t) THD (c)] = (float) thd; } return d; };
        return { { "Bombo contundente", mk (8.f, -3.f, .05f, .2f, 2, 1, 1) }, { "Voz firme", mk (6.f, -2.f, .15f, .5f, 1, 0, 0) }, { "Bus fuerte", mk (5.f, -2.f, .2f, .3f, 2, 0, 1) },
                 { "Aplastar", mk (14.f, -8.f, .03f, .1f, 0, 2, 1) }, { "Limitar picos", mk (4.f, -1.f, .02f, .2f, 0, 2, 0) } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; gr[0] = gr[1] = 0.f; hp[0].reset(); hp[1].reset(); }
    static float ratioOf (int m) { return m == 0 ? 4.f : m == 1 ? 8.f : 20.f; }
    static float scHz (int i) { static const float h[] { 0.f, 60.f, 90.f, 150.f, 300.f }; return h[juce::jlimit (0, 4, i)]; }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nullptr }; p[1] = nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : p[0];
        const bool link = vi (pLink) != 0, sel[2] { vi (pSelL) != 0, vi (pSelR) != 0 };
        float inG[2], outG[2], aA[2], aR[2], rat[2]; bool on[2], thd[2], scOn[2];
        for (int c = 0; c < 2; ++c)
        {
            inG[c] = db2lin (v (INP (c)) - (vi (HL (c)) ? 0.f : 10.f)); outG[c] = db2lin (v (OUT (c))); aA[c] = coefMs (v (ATK (c)), sr); aR[c] = coefMs (v (REL (c)) * 1000.f * .25f, sr);
            rat[c] = ratioOf (vi (MODE (c))); on[c] = vi (INN (c)) != 0 && sel[c]; thd[c] = vi (THD (c)) != 0; const float h = scHz (vi (SC (c))); scOn[c] = h > 0.f; hp[c].set (BQ::HP, sr, h > 0.f ? h : 20.f, .707, 0.0);
        }
        const float T = -20.f; float mx[2] { 0.f, 0.f };
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            float x[2] { p[0][i], p[1][i] }, d[2], tg[2];
            for (int c = 0; c < 2; ++c) { const float s = x[c] * inG[c]; d[c] = std::abs (scOn[c] ? hp[c].run (c, s) : s); }
            for (int c = 0; c < 2; ++c) { const float db = lin2db (d[c] + 1e-9f); tg[c] = on[c] ? std::max (0.f, (db - T) * (1.f - 1.f / rat[c])) : 0.f; }
            if (link) tg[0] = tg[1] = std::max (tg[0], tg[1]);
            for (int c = 0; c < 2; ++c)
            {
                gr[c] = tg[c] > gr[c] ? aA[c] * gr[c] + (1.f - aA[c]) * tg[c] : aR[c] * gr[c] + (1.f - aR[c]) * tg[c]; mx[c] = std::max (mx[c], gr[c]);
                if (! sel[c] || vi (INN (c)) == 0) { continue; }
                float y = x[c] * inG[c] * db2lin (-gr[c]); if (thd[c]) y = std::tanh (y * 1.6f) / 1.6f * 1.15f; p[c][i] = y * outG[c];
            }
            if (nc < 2) p[1][i] = p[0][i];
        }
        meterA.store (mx[0]); meterB.store (mx[1]);
    }
private:
    static const std::vector<Spec>& table()
    {
        static std::deque<std::string> keep; static std::vector<Spec> t;
        if (! t.empty()) return t;
        auto add = [&] (const std::string& id, const std::string& name, float lo, float hi, float def, float mid, int fmt, const char* ch) { keep.push_back (id); const char* a = keep.back().c_str(); keep.push_back (name); t.push_back ({ a, keep.back().c_str(), lo, hi, def, mid, fmt, ch }); };
        for (int c = 0; c < 2; ++c)
        {
            const std::string s = c == 0 ? "L" : "R", n = c == 0 ? "Izq: " : "Der: ";
            add ("in" + s, n + "Entrada", -20.f, 20.f, 0.f, 0.f, fDb, ""); add ("out" + s, n + "Salida", -20.f, 20.f, 0.f, 0.f, fDb, ""); add ("atk" + s, n + "Ataque", .02f, .8f, .15f, .15f, fMs, ""); add ("rel" + s, n + "Release", .05f, 1.5f, .5f, .5f, fSec, "");
            add ("sc" + s, n + "Cadena lateral", 0.f, 4.f, 0.f, 0.f, fChoice, "Fuera|60 Hz|90 Hz|150 Hz|300 Hz"); add ("mode" + s, n + "Modo", 0.f, 2.f, 0.f, 0.f, fChoice, "COMP 1|COMP 2|LIMIT"); add ("thd" + s, n + "THD", 0.f, 1.f, 0.f, 0.f, fChoice, "Limpio|THD");
            add ("hl" + s, n + "Rango de entrada", 0.f, 1.f, 1.f, 0.f, fChoice, "Low|High"); add ("on" + s, n + "IN", 0.f, 1.f, 1.f, 0.f, fChoice, "No|Sí");
        }
        add ("link", "Enlace", 0.f, 1.f, 1.f, 0.f, fChoice, "No|Sí"); add ("selL", "Canal L", 0.f, 1.f, 1.f, 0.f, fChoice, "No|Sí"); add ("selR", "Canal R", 0.f, 1.f, 1.f, 0.f, fChoice, "No|Sí");
        return t;
    }
    double sr = 44100.0; BQ hp[2]; float gr[2] { 0.f, 0.f };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjCobalt)
};

class CobaltEditor : public SkinEditor
{
public:
    CobaltEditor (InjCobalt& p) : SkinEditor (p, skinImage ("lm_bg_png"), .46f, { 10, 82, 2152, 585 }), cb (p)
    {
        const char* rn[4] { "L_in", "L_out", "R_in", "R_out" }; const float rx[4] { 344.5f, 858.f, 1397.f, 1911.5f }, ry[4] { 253.f, 257.f, 256.f, 251.f };
        for (int k = 0; k < 4; ++k) { const int c = k / 2; Knob kn { (k % 2) ? InjCobalt::OUT (c) : InjCobalt::INP (c), rx[k], ry[k], 44.f, skinImage (("lm_red_" + juce::String (rn[k]) + "_png").toRawUTF8()) }; kn.cv = { -20.f, -10.f, 0.f, 10.f, 20.f }; kn.ca = { -102.f, -43.f, 0.f, 43.f, 102.f }; knobs.push_back (kn); }
        const char* ln[8] { "L_att", "L_rel", "L_sc", "L_mode", "R_att", "R_rel", "R_sc", "R_mode" }; const float lx[8] { 310.f, 469.f, 624.6f, 777.4f, 1374.9f, 1530.4f, 1684.3f, 1838.8f }, ly[8] { 482.1f, 481.8f, 481.1f, 498.5f, 480.9f, 480.3f, 480.2f, 497.9f };
        for (int k = 0; k < 8; ++k)
        {
            const int c = k / 4, t = k % 4; const int idx = t == 0 ? InjCobalt::ATK (c) : t == 1 ? InjCobalt::REL (c) : t == 2 ? InjCobalt::SC (c) : InjCobalt::MODE (c);
            Knob kn { idx, lx[k], ly[k], 38.f, skinImage (("lm_" + juce::String (ln[k]) + "_png").toRawUTF8()) };
            if (t == 0 || t == 1) { kn.a0 = -129.f; kn.a1 = 129.f; } else if (t == 2) kn.stops = { -125.f, -42.f, 0.f, 42.f, 125.f }; else kn.stops = { -13.f, 25.f, 57.f };
            knobs.push_back (kn);
        }
        const char* sd[2] { "L", "R" };
        for (int c = 0; c < 2; ++c)
        {
            auto ld = [&] (const juce::String& n, juce::Image& a, juce::Image& b, const char* s1, const char* s2) { a = skinImage ((n + s1).toRawUTF8()); b = skinImage ((n + s2).toRawUTF8()); };
            ld ("lm_hl" + juce::String (sd[c]), hlA[c], hlB[c], "_up_png", "_dn_png"); ld ("lm_thd" + juce::String (sd[c]), thA[c], thB[c], "_up_png", "_dn_png");
            ld ("lm_in" + juce::String (sd[c]), inA[c], inB[c], "_on_png", "_off_png"); ld ("lm_sel" + juce::String (sd[c]), seA[c], seB[c], "_on_png", "_off_png");
            vuF[c] = juce::Rectangle<float> (480.f + c * 1045.f, 184.f, 244.f, 147.f);
            hots.push_back ({ { hlP[c].x - 30.f, hlP[c].y - 46.f, 60.f, 92.f }, [this, c] { choose (InjCobalt::HL (c), cb.vi (InjCobalt::HL (c)) ? 0 : 1); } });
            hots.push_back ({ { thP[c].x - 30.f, thP[c].y - 46.f, 60.f, 92.f }, [this, c] { choose (InjCobalt::THD (c), cb.vi (InjCobalt::THD (c)) ? 0 : 1); } });
            hots.push_back ({ { inP[c].x - 40.f, inP[c].y - 40.f, 80.f, 80.f }, [this, c] { choose (InjCobalt::INN (c), cb.vi (InjCobalt::INN (c)) ? 0 : 1); } });
            hots.push_back ({ { seP[c].x - 40.f, seP[c].y - 40.f, 80.f, 80.f }, [this, c] { const int i = c ? InjCobalt::pSelR : InjCobalt::pSelL; choose (i, cb.vi (i) ? 0 : 1); } });
        }
        lkA = skinImage ("lm_link_up_png"); lkB = skinImage ("lm_link_dn_png");
        hots.push_back ({ { 1088.f - 32.f, 565.f - 48.f, 64.f, 96.f }, [this] { choose (InjCobalt::pLink, cb.vi (InjCobalt::pLink) ? 0 : 1); } });
        hots.push_back ({ { 1021.f, 277.f, 141.f, 76.f }, [this] { presetMenu(); } });
        if (std::getenv ("INJ_PUNCHDEMO")) { cb.applyPreset (0); gs[0] = 9.f; gs[1] = 6.f; demo = true; }
    }
protected:
    void tick() override { if (! demo) { gs[0] = std::max (cb.meterA.load(), gs[0] * .9f); gs[1] = std::max (cb.meterB.load(), gs[1] * .9f); } repaint(); }
    void paintSkin (juce::Graphics& g) override
    {
        for (int c = 0; c < 2; ++c)
        {
            drawSprite (g, cb.vi (InjCobalt::HL (c)) ? hlA[c] : hlB[c], hlP[c].x, hlP[c].y); drawSprite (g, cb.vi (InjCobalt::THD (c)) ? thA[c] : thB[c], thP[c].x, thP[c].y);
            drawSprite (g, cb.vi (InjCobalt::INN (c)) ? inA[c] : inB[c], inP[c].x, inP[c].y); drawSprite (g, cb.vi (c ? InjCobalt::pSelR : InjCobalt::pSelL) ? seA[c] : seB[c], seP[c].x, seP[c].y);
            const float deg = -41.f + juce::jlimit (0.f, 20.f, gs[c]) / 20.f * 81.7f; drawNeedle (g, 604.f + c * 1045.f, 354.8f, deg, 69.f, 126.f, vuF[c], juce::Colour (0xff1e1812), 3.2f);
        }
        drawSprite (g, cb.vi (InjCobalt::pLink) ? lkA : lkB, 1088.f, 565.f);
    }
private:
    void presetMenu()
    {
        juce::PopupMenu m; auto ps = cb.presets(); m.addItem (1, "Inicial"); m.addSeparator();
        for (size_t i = 0; i < ps.size(); ++i) m.addItem (100 + (int) i, juce::String::fromUTF8 (ps[i].name), true, cb.presetName() == juce::String::fromUTF8 (ps[i].name));
        juce::Component::SafePointer<CobaltEditor> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [sp] (int r) { if (sp == nullptr) return; if (r == 1) sp->cb.resetAll(); else if (r >= 100) sp->cb.applyPreset (r - 100); sp->repaint(); });
    }
    InjCobalt& cb; juce::Image hlA[2], hlB[2], thA[2], thB[2], inA[2], inB[2], seA[2], seB[2], lkA, lkB; juce::Rectangle<float> vuF[2]; float gs[2] { 0.f, 0.f }; bool demo = false;
    const juce::Point<float> hlP[2] { { 199.f, 281.f }, { 1270.f, 280.f } }, thP[2] { { 181.f, 483.f }, { 1257.f, 481.f } }, inP[2] { { 920.f, 488.f }, { 1987.f, 488.f } }, seP[2] { { 1039.f, 411.f }, { 1138.f, 411.f } };
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjCobalt::createEditor() { return std::make_unique<CobaltEditor> (*this); }
} // namespace injfx
