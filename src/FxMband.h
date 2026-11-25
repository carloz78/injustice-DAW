#pragma once
#include "FxCobalt.h"
// Multiband: compresor de tres bandas estéreo (cruces graves/medios/agudos, por banda umbral, ataque, liberación, ganancia y estado MUTE / ON / BYPASS), con modo BIG (ratio más alto),
// mezcla seco/húmedo, ganancia de salida, dos VU de salida (VU o pico, con atenuador de +10 dB), bypass general y encendido.
namespace injfx
{
class InjMband : public InjFx
{
public:
    static constexpr int pX1 = 15, pX2 = 16, pMix = 17, pOut = 18, pBig = 19, pPad = 20, pPeak = 21, pByp = 22, pPow = 23;
    static constexpr int THR (int b) { return b * 5; }
    static constexpr int ATK (int b) { return b * 5 + 1; }
    static constexpr int REL (int b) { return b * 5 + 2; }
    static constexpr int GAIN (int b) { return b * 5 + 3; }
    static constexpr int MODE (int b) { return b * 5 + 4; }
    static inline const char* xmlTypeName = "injmband";
    static const char* getPluginName() { return "Multiband"; }
    InjMband (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Multiband"; }
    const Spec* specs() const override { return table().data(); }
    int numSpecs() const override { return 24; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    static std::vector<float> defaults() { std::vector<float> d; for (auto& s : table()) d.push_back (s.def); return d; }
    std::vector<Preset> presets() const override
    {
        auto mk = [] (float t0, float t1, float t2, float a, float r, float g0, float g2, float mix, int big) { auto d = defaults(); d[(size_t) THR (0)] = t0; d[(size_t) THR (1)] = t1; d[(size_t) THR (2)] = t2;
            for (int b = 0; b < 3; ++b) { d[(size_t) ATK (b)] = a; d[(size_t) REL (b)] = r; } d[(size_t) GAIN (0)] = g0; d[(size_t) GAIN (2)] = g2; d[pMix] = mix; d[pBig] = (float) big; return d; };
        return { { "Master suave", mk (-14.f, -14.f, -16.f, 10.f, .25f, 0.f, 0.f, 100.f, 0) }, { "Graves apretados", mk (-24.f, -12.f, -10.f, 8.f, .2f, 2.f, 0.f, 100.f, 1) }, { "Brillo controlado", mk (-10.f, -12.f, -26.f, 5.f, .15f, 0.f, 1.f, 100.f, 0) },
                 { "Bus de batería", mk (-22.f, -20.f, -20.f, 3.f, .12f, 1.f, 1.f, 70.f, 1) }, { "Paralela agresiva", mk (-30.f, -28.f, -28.f, 1.f, .08f, 3.f, 3.f, 40.f, 1) } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; for (auto* f : fl) f->reset(); for (int b = 0; b < 3; ++b) gr[b] = 0.f; lvl[0] = lvl[1] = 0.f; }
    std::atomic<float> grMeter[3] { 0.f, 0.f, 0.f }, outLevel[2] { -90.f, -90.f };
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nullptr }; p[1] = nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : p[0];
        const float f1 = v (pX1), f2 = juce::jmax (v (pX2), f1 * 1.5f);
        for (auto* f : { &lp1a, &lp1b }) f->set (BQ::LP, sr, f1, .7071, 0.0); for (auto* f : { &hp1a, &hp1b }) f->set (BQ::HP, sr, f1, .7071, 0.0);
        for (auto* f : { &lp2a, &lp2b, &lp2c, &lp2d }) f->set (BQ::LP, sr, f2, .7071, 0.0); for (auto* f : { &hp2a, &hp2b, &hp2c, &hp2d }) f->set (BQ::HP, sr, f2, .7071, 0.0);
        const bool on = vi (pPow) != 0, byp = vi (pByp) != 0, big = vi (pBig) != 0, peak = vi (pPeak) != 0; const float mix = v (pMix) * .01f, outG = db2lin (v (pOut)), ratio = big ? 8.f : 4.f, pad = vi (pPad) ? db2lin (-10.f) : 1.f;
        float thr[3], mk[3], aA[3], aR[3]; int mode[3];
        for (int b = 0; b < 3; ++b) { thr[b] = v (THR (b)); mk[b] = db2lin (v (GAIN (b))); aA[b] = coefMs (v (ATK (b)), sr); aR[b] = coefMs (v (REL (b)) * 1000.f, sr); mode[b] = vi (MODE (b)); }
        const float mA = peak ? coefMs (8.f, sr) : coefMs (150.f, sr), mR = coefMs (300.f, sr); float mxGr[3] { 0.f, 0.f, 0.f };
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            float x[2] { p[0][i], p[1][i] }, band[3][2], out[2];
            for (int c = 0; c < 2; ++c)
            {
                const float lo = lp1b.run (c, lp1a.run (c, x[c])), rest = hp1b.run (c, hp1a.run (c, x[c]));
                band[0][c] = lp2d.run (c, lp2c.run (c, lo)) + hp2d.run (c, hp2c.run (c, lo));   // el grave pasa también por un pasatodo con la fase del cruce agudo
                band[1][c] = lp2b.run (c, lp2a.run (c, rest)); band[2][c] = hp2b.run (c, hp2a.run (c, rest));
            }
            for (int c = 0; c < 2; ++c) out[c] = 0.f;
            for (int b = 0; b < 3; ++b)
            {
                float g = 1.f;
                if (mode[b] == 1) { for (int c = 0; c < 2; ++c) band[b][c] = 0.f; gr[b] = 0.f; }
                else if (mode[b] == 0)
                {
                    const float d = lin2db (std::max (std::abs (band[b][0]), std::abs (band[b][1])) + 1e-9f), tg = std::max (0.f, (d - thr[b]) * (1.f - 1.f / ratio));
                    gr[b] = tg > gr[b] ? aA[b] * gr[b] + (1.f - aA[b]) * tg : aR[b] * gr[b] + (1.f - aR[b]) * tg; g = db2lin (-gr[b]) * mk[b]; mxGr[b] = std::max (mxGr[b], gr[b]);
                }
                else gr[b] = 0.f;
                for (int c = 0; c < 2; ++c) out[c] += band[b][c] * g;
            }
            for (int c = 0; c < 2; ++c)
            {
                const float wet = out[c] * outG, y = byp || ! on ? x[c] : x[c] * (1.f - mix) + wet * mix; p[c][i] = y;
                const float a = std::abs (y) * pad; lvl[c] = a > lvl[c] ? mA * lvl[c] + (1.f - mA) * a : mR * lvl[c] + (1.f - mR) * a;
            }
            if (nc < 2) p[1][i] = p[0][i];
        }
        for (int b = 0; b < 3; ++b) grMeter[b].store (mxGr[b]);
        for (int c = 0; c < 2; ++c) outLevel[c].store (lin2db (lvl[c] * (peak ? 1.f : 1.11f) + 1e-6f));
    }
private:
    static const std::vector<Spec>& table()
    {
        static std::deque<std::string> keep; static std::vector<Spec> t;
        if (! t.empty()) return t;
        auto add = [&] (const std::string& id, const std::string& name, float lo, float hi, float def, float mid, int fmt, const char* ch) { keep.push_back (id); const char* a = keep.back().c_str(); keep.push_back (name); t.push_back ({ a, keep.back().c_str(), lo, hi, def, mid, fmt, ch }); };
        static const char* bn[3] { "Graves", "Medios", "Agudos" };
        for (int b = 0; b < 3; ++b)
        {
            const std::string s = std::to_string (b), n = std::string (bn[b]) + ": ";
            add ("thr" + s, n + "Umbral", -30.f, 20.f, -12.f, 0.f, fDb, ""); add ("atk" + s, n + "Ataque", .1f, 30.f, 5.f, 3.f, fMs, ""); add ("rel" + s, n + "Liberación", .02f, 1.f, .2f, .2f, fSec, "");
            add ("gain" + s, n + "Ganancia", -10.f, 10.f, 0.f, 0.f, fDb, ""); add ("mode" + s, n + "Estado", 0.f, 2.f, 0.f, 0.f, fChoice, "ON|MUTE|BYPASS");
        }
        add ("x1", "Cruce graves", 60.f, 400.f, 150.f, 150.f, fHz, ""); add ("x2", "Cruce agudos", 800.f, 6000.f, 2200.f, 2200.f, fHz, ""); add ("mix", "Mezcla", 0.f, 100.f, 100.f, 0.f, fPct, ""); add ("out", "Ganancia de salida", -10.f, 10.f, 0.f, 0.f, fDb, "");
        add ("big", "BIG", 0.f, 1.f, 0.f, 0.f, fChoice, "No|Sí"); add ("pad", "Atenuador +10 dB de los VU", 0.f, 1.f, 0.f, 0.f, fChoice, "No|Sí"); add ("peak", "VU de pico", 0.f, 1.f, 0.f, 0.f, fChoice, "VU|Pico");
        add ("byp", "Bypass", 0.f, 1.f, 0.f, 0.f, fChoice, "Normal|Bypass"); add ("pow", "Encendido", 0.f, 1.f, 1.f, 0.f, fChoice, "No|Sí");
        return t;
    }
    double sr = 44100.0; BQ lp1a, lp1b, hp1a, hp1b, lp2a, lp2b, lp2c, lp2d, hp2a, hp2b, hp2c, hp2d; BQ* fl[12] { &lp1a, &lp1b, &hp1a, &hp1b, &lp2a, &lp2b, &lp2c, &lp2d, &hp2a, &hp2b, &hp2c, &hp2d }; float gr[3] { 0.f, 0.f, 0.f }, lvl[2] { 0.f, 0.f };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjMband)
};

class MbandEditor : public SkinEditor
{
public:
    MbandEditor (InjMband& p) : SkinEditor (p, skinImage ("mb_bg_png"), .5f, { 0, 95, 2172, 540 }), mb (p)
    {
        // nombre: parámetro, índice de la tapa, centro, ángulo del puntero en la imagen, recorrido
        struct K { int idx, cap; float x, y, rot, span; };
        const K ks[] { { InjMband::THR (0), 0, 262.5f, 258.5f, 20.f, 130.f }, { InjMband::THR (1), 1, 688.5f, 244.4f, -53.f, 130.f }, { InjMband::THR (2), 2, 1149.f, 259.2f, 27.f, 130.f },
            { InjMband::ATK (0), 3, 166.5f, 488.9f, -15.f, 130.f }, { InjMband::REL (0), 4, 292.f, 489.2f, -12.f, 130.f }, { InjMband::GAIN (0), 5, 417.3f, 488.f, 34.f, 130.f },
            { InjMband::ATK (1), 6, 605.1f, 488.9f, -11.f, 130.f }, { InjMband::REL (1), 7, 728.9f, 488.8f, -8.f, 130.f }, { InjMband::GAIN (1), 8, 853.f, 486.8f, 40.f, 130.f },
            { InjMband::ATK (2), 9, 1035.7f, 488.6f, -11.f, 130.f }, { InjMband::REL (2), 10, 1157.5f, 488.4f, 37.f, 130.f }, { InjMband::GAIN (2), 11, 1283.1f, 486.1f, -7.f, 130.f },
            { InjMband::pMix, 12, 1576.8f, 486.f, -5.f, 135.f }, { InjMband::pOut, 13, 1713.3f, 486.3f, 43.f, 135.f }, { InjMband::pX1, 14, 517.8f, 253.8f, 2.f, 135.f }, { InjMband::pX2, 15, 949.f, 248.5f, 2.f, 135.f } };
        for (auto& k : ks) { Knob kn { k.idx, k.x, k.y, k.cap < 3 ? 40.f : (k.cap >= 14 ? 28.f : 33.f), skinImage (("mb_cap" + juce::String (k.cap) + "_png").toRawUTF8()) }; kn.a0 = -k.span; kn.a1 = k.span; kn.rot0 = k.rot; knobs.push_back (kn); }
        for (int b = 0; b < 3; ++b) { gr_[b] = skinImage (("mb_gr" + juce::String (b) + "_png").toRawUTF8()); st_[b] = skinImage (("mb_st" + juce::String (b) + "_png").toRawUTF8()); }
        const char* tn[4] { "pad", "peak", "byp", "big" }; for (int k = 0; k < 4; ++k) { tA[k] = skinImage (("mb_" + juce::String (tn[k]) + "_up_png").toRawUTF8()); tB[k] = skinImage (("mb_" + juce::String (tn[k]) + "_dn_png").toRawUTF8()); }
        const int tp[4] { InjMband::pPad, InjMband::pPeak, InjMband::pByp, InjMband::pBig };
        for (int k = 0; k < 4; ++k) hots.push_back ({ { tx[k] - 32.f, ty[k] - 52.f, 64.f, 104.f }, [this, k, tp] { choose (tp[k], mb.vi (tp[k]) ? 0 : 1); } });
        pwOn = skinImage ("mb_pw_on_png"); pwOff = skinImage ("mb_pw_off_png"); pwLed = skinImage ("mb_pwled_png");
        hots.push_back ({ { 1895.f - 40.f, 494.f - 44.f, 80.f, 88.f }, [this] { choose (InjMband::pPow, mb.vi (InjMband::pPow) ? 0 : 1); } });
        for (int b = 0; b < 3; ++b) for (int s = 0; s < 3; ++s) hots.push_back ({ { sx[b] - 16.f, sy[s] - 16.f, 32.f, 32.f }, [this, b, s] { static const int m[3] { 1, 0, 2 }; choose (InjMband::MODE (b), m[s]); } });
        hots.push_back ({ { 1960.f, 215.f, 230.f, 120.f }, [this] { presetMenu(); } });
        if (std::getenv ("INJ_PUNCHDEMO")) { mb.applyPreset (4); demo = true; g3[0] = 12.f; g3[1] = 6.f; g3[2] = 3.f; vu[0] = -4.f; vu[1] = -8.f; }
    }
protected:
    void tick() override
    {
        if (! demo) { for (int b = 0; b < 3; ++b) g3[b] = std::max (mb.grMeter[b].load(), g3[b] * .9f); for (int c = 0; c < 2; ++c) vu[c] += (juce::jlimit (-40.f, 12.f, mb.outLevel[c].load() + 18.f) - vu[c]) * .5f; }
        repaint();
    }
    static float needleDeg (float db)
    {
        static const float cv[8] { -20.f, -10.f, -7.f, -5.f, -3.f, 0.f, 3.f, 5.f }, ca[8] { -27.4f, -12.4f, -5.7f, 0.f, 10.f, 24.6f, 37.f, 49.6f };
        db = juce::jlimit (-20.f, 5.f, db); for (int i = 0; i < 7; ++i) if (db <= cv[i + 1]) return ca[i] + (ca[i + 1] - ca[i]) * (db - cv[i]) / (cv[i + 1] - cv[i]); return ca[7];
    }
    void paintSkin (juce::Graphics& g) override
    {
        static const float th[8] { 30.f, 20.f, 15.f, 10.f, 6.f, 4.f, 2.f, 1.f };
        for (int b = 0; b < 3; ++b)
        {
            const int nl = gr_[b].getWidth() / gr_[b].getHeight();
            for (int k = 0; k < nl; ++k) { const float t = nl == 8 ? th[k] : (k < 4 ? th[k] : (k == 4 ? 6.f : th[k + 1])); if (g3[b] >= t * .8f) drawLed (g, gr_[b], k, ledX[b][k], 351.2f); }
            const int m = mb.vi (InjMband::MODE (b)), lit[3] { m == 1, m == 0, m == 2 }; for (int s = 0; s < 3; ++s) if (lit[s]) drawLed (g, st_[b], s, sx[b], sy[s]);
        }
        const int tp[4] { InjMband::pPad, InjMband::pPeak, InjMband::pByp, InjMband::pBig };
        for (int k = 0; k < 4; ++k) drawSprite (g, mb.vi (tp[k]) == (k == 3 ? 1 : 0) ? tA[k] : tB[k], tx[k], ty[k]);
        const bool on = mb.vi (InjMband::pPow) != 0; drawSprite (g, on ? pwOn : pwOff, 1895.f, 494.f); if (on) drawLed (g, pwLed, 0, 1895.f, 531.f);
        if (mb.vi (InjMband::pByp)) { juce::ColourGradient gd (juce::Colour (0xffffa030), 1827.f * sc, 530.f * sc, juce::Colour (0x00ffa030), 1840.f * sc, 530.f * sc, true); g.setGradientFill (gd); g.fillEllipse (1814.f * sc, 517.f * sc, 26.f * sc, 26.f * sc); }
        const float offs[2] { 0.f, 280.f };
        for (int c = 0; c < 2; ++c) drawNeedle (g, 1488.8f + offs[c], 324.2f, needleDeg (vu[c]), 4.f, 121.f, { 1384.f + offs[c], 165.f, 243.f, 162.f }, juce::Colour (0xff17120d), 2.8f);
    }
private:
    void presetMenu()
    {
        juce::PopupMenu m; auto ps = mb.presets(); m.addItem (1, "Inicial"); m.addSeparator();
        for (size_t i = 0; i < ps.size(); ++i) m.addItem (100 + (int) i, juce::String::fromUTF8 (ps[i].name), true, mb.presetName() == juce::String::fromUTF8 (ps[i].name));
        juce::Component::SafePointer<MbandEditor> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [sp] (int r) { if (sp == nullptr) return; if (r == 1) sp->mb.resetAll(); else if (r >= 100) sp->mb.applyPreset (r - 100); sp->repaint(); });
    }
    InjMband& mb; juce::Image gr_[3], st_[3], tA[4], tB[4], pwOn, pwOff, pwLed; float g3[3] { 0.f, 0.f, 0.f }, vu[2] { -30.f, -30.f }; bool demo = false;
    static constexpr float tx[4] { 1395.f, 1463.f, 1824.f, 139.f }, ty[4] { 488.f, 488.f, 488.f, 206.f }, sx[3] { 370.f, 802.5f, 1260.7f }, sy[3] { 214.8f, 254.2f, 293.4f };
    static constexpr float ledX[3][8] { { 226.7f, 254.6f, 281.9f, 309.9f, 336.9f, 363.6f, 390.6f, 418.6f }, { 668.9f, 697.2f, 725.1f, 752.7f, 794.1f, 822.7f, 850.6f, 0.f }, { 1102.7f, 1130.6f, 1158.3f, 1185.6f, 1213.1f, 1241.f, 1268.5f, 1296.7f } };
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjMband::createEditor() { return std::make_unique<MbandEditor> (*this); }
} // namespace injfx
