#pragma once
#include "FxMaster.h"
// Dual Gate NG-2: puerta de ruido de dos canales (umbral, ataque, retención, liberación, filtro de la cadena lateral OFF/LF/HF con frecuencia, medidor de 7 LEDs),
// con LINK (detección compartida) y selector STEREO / DUAL.
namespace injfx
{
class InjGateDS : public InjFx
{
public:
    static constexpr int per = 7, pLink = 14, pStereo = 15;
    static constexpr int THR (int c) { return c * per; }
    static constexpr int ATK (int c) { return c * per + 1; }
    static constexpr int HLD (int c) { return c * per + 2; }
    static constexpr int REL (int c) { return c * per + 3; }
    static constexpr int KEY (int c) { return c * per + 4; }
    static constexpr int KHZ (int c) { return c * per + 5; }
    static constexpr int INN (int c) { return c * per + 6; }
    static inline const char* xmlTypeName = "injgateds";
    static const char* getPluginName() { return "Dual Gate"; }
    InjGateDS (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "DualGate"; }
    const Spec* specs() const override { return table().data(); }
    int numSpecs() const override { return 16; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    static std::vector<float> defaults() { std::vector<float> d; for (auto& s : table()) d.push_back (s.def); return d; }
    std::vector<Preset> presets() const override
    {
        auto mk = [] (float thr, float atk, float hold, float rel, int key, float khz, int st) { auto d = defaults(); for (int c = 0; c < 2; ++c) { d[(size_t) THR (c)] = thr; d[(size_t) ATK (c)] = atk; d[(size_t) HLD (c)] = hold; d[(size_t) REL (c)] = rel; d[(size_t) KEY (c)] = (float) key; d[(size_t) KHZ (c)] = khz; } d[pStereo] = (float) (1 - st); return d; };
        return { { "Limpieza suave", mk (-45.f, 1.f, 50.f, 200.f, 0, 1000.f, 1) }, { "Batería seca", mk (-30.f, .3f, 30.f, 80.f, 0, 1000.f, 1) }, { "Bombo (solo graves)", mk (-28.f, .5f, 40.f, 120.f, 1, 200.f, 1) },
                 { "Voz (sin siseo)", mk (-38.f, 2.f, 100.f, 300.f, 2, 800.f, 1) }, { "Cortar fugas", mk (-20.f, .1f, 10.f, 30.f, 0, 1000.f, 0) } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; for (auto& k : key) k.reset(); for (int c = 0; c < 2; ++c) { env[c] = 0.f; gain[c] = 0.f; hold[c] = 0; open[c] = false; } }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nullptr }; p[1] = nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : p[0];
        const bool stereo = vi (pStereo) == 0, link = vi (pLink) != 0;
        float thr[2], aA[2], aR[2], aE = coefMs (30.f, sr); int hl[2]; bool on[2];
        for (int c = 0; c < 2; ++c)
        {
            const int s = stereo ? 0 : c;   // en STEREO manda el canal 1
            thr[c] = v (THR (s)); aA[c] = coefMs (v (ATK (s)), sr); aR[c] = coefMs (v (REL (s)) * .35f, sr); hl[c] = (int) (v (HLD (s)) * .001f * sr); on[c] = vi (INN (s)) != 0;
            const int k = vi (KEY (s)); const float hz = juce::jlimit (20.f, (float) sr * .45f, v (KHZ (s)));
            if (k == 1) key[c].set (BQ::LP, sr, hz, .707, 0.0); else if (k == 2) key[c].set (BQ::HP, sr, hz, .707, 0.0); else key[c].set (BQ::Peak, sr, 1000.0, 1.0, 0.0);
            keyOn[c] = k != 0;
        }
        const float floorG = db2lin (-70.f); float pk[2] { 0.f, 0.f }; int openMask = 0;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            float x[2] { p[0][i], p[1][i] }, kd[2];
            for (int c = 0; c < 2; ++c) kd[c] = std::abs (keyOn[c] ? key[c].run (c, x[c]) : x[c]);
            if (stereo || link) { const float m = std::max (kd[0], kd[1]); kd[0] = kd[1] = m; }
            for (int c = 0; c < 2; ++c)
            {
                env[c] = kd[c] > env[c] ? kd[c] : aE * env[c] + (1.f - aE) * kd[c]; const float db = lin2db (env[c] + 1e-9f); pk[c] = std::max (pk[c], db);
                if (db > thr[c]) { open[c] = true; hold[c] = hl[c]; } else if (db < thr[c] - 3.f) { if (hold[c] > 0) --hold[c]; else open[c] = false; } else if (open[c]) hold[c] = hl[c];
                const float tg = (! on[c] || open[c]) ? 1.f : floorG; const float k = tg > gain[c] ? aA[c] : aR[c]; gain[c] = tg + k * (gain[c] - tg);
                if (on[c]) p[c][i] = x[c] * std::max (gain[c], floorG * .5f);
            }
            if (nc < 2) p[1][i] = p[0][i];
        }
        openMask = (open[0] ? 1 : 0) | (open[1] ? 2 : 0);
        meterA.store (pk[0]); meterB.store (pk[1]); meterC.store ((float) openMask);
    }
private:
    static const std::vector<Spec>& table()
    {
        static std::deque<std::string> keep; static std::vector<Spec> t;
        if (! t.empty()) return t;
        auto add = [&] (const std::string& id, const std::string& name, float lo, float hi, float def, float mid, int fmt, const char* ch) { keep.push_back (id); const char* a = keep.back().c_str(); keep.push_back (name); t.push_back ({ a, keep.back().c_str(), lo, hi, def, mid, fmt, ch }); };
        for (int c = 0; c < 2; ++c)
        {
            const std::string s = c == 0 ? "1" : "2", n = "Canal " + s + ": ";
            add ("thr" + s, n + "Umbral", -40.f, 20.f, -20.f, 0.f, fDb, ""); add ("atk" + s, n + "Ataque", .1f, 30.f, 3.f, 3.f, fMs, ""); add ("hld" + s, n + "Retención", 10.f, 3000.f, 100.f, 100.f, fMs, ""); add ("rel" + s, n + "Liberación", 10.f, 3000.f, 170.f, 170.f, fMs, "");
            add ("key" + s, n + "Filtro de cadena lateral", 0.f, 2.f, 0.f, 0.f, fChoice, "Fuera|LF|HF"); add ("khz" + s, n + "Frecuencia del filtro", 20.f, 20000.f, 1000.f, 630.f, fHz, ""); add ("in" + s, n + "Activo", 0.f, 1.f, 1.f, 0.f, fChoice, "No|Sí");
        }
        add ("link", "Enlace", 0.f, 1.f, 0.f, 0.f, fChoice, "No|Sí"); add ("mode", "Modo", 0.f, 1.f, 0.f, 0.f, fChoice, "Stereo|Dual");
        return t;
    }
    double sr = 44100.0; BQ key[2]; bool keyOn[2] { false, false }; float env[2] { 0.f, 0.f }, gain[2] { 0.f, 0.f }; int hold[2] { 0, 0 }; bool open[2] { false, false };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjGateDS)
};

class GateDSEditor : public SkinEditor
{
public:
    GateDSEditor (InjGateDS& p) : SkinEditor (p, skinImage ("gt_bg_png"), .46f), gt (p)
    {
        // centro, radio de la tapa y ángulo del puntero en la imagen, por canal: umbral, ataque, retención, liberación, frecuencia del filtro
        const float cx[2][5] { { 215.6f, 451.f, 661.2f, 884.8f, 391.5f }, { 1301.7f, 1537.3f, 1747.5f, 1972.4f, 1477.1f } }, cy[2][5] { { 376.9f, 376.3f, 375.1f, 376.3f, 566.1f }, { 377.2f, 376.5f, 376.6f, 376.4f, 567.1f } },
                    cr[5] { 44.5f, 36.9f, 36.9f, 36.9f, 33.7f }, rot[2][5] { { 25.f, -4.f, -6.f, -2.f, -10.f }, { 33.f, -1.f, 1.f, 4.f, -5.f } };
        for (int c = 0; c < 2; ++c) for (int k = 0; k < 5; ++k)
        {
            const int idx = k < 4 ? InjGateDS::per * c + k : InjGateDS::KHZ (c);
            Knob kn { idx, cx[c][k], cy[c][k], cr[k], skinImage (("gt_cap" + juce::String (c * 5 + k) + "_png").toRawUTF8()) }; kn.a0 = -131.f; kn.a1 = 131.f; kn.rot0 = rot[c][k]; knobs.push_back (kn);
        }
        for (int c = 0; c < 2; ++c) { led[c] = skinImage (("gt_led" + juce::String (c) + "_png").toRawUTF8()); inOn[c] = skinImage (("gt_in" + juce::String (c) + "_on_png").toRawUTF8()); inOff[c] = skinImage (("gt_in" + juce::String (c) + "_off_png").toRawUTF8());
            hots.push_back ({ { inX[c] - 40.f, 145.f, 80.f, 80.f }, [this, c] { choose (InjGateDS::INN (c), gt.vi (InjGateDS::INN (c)) ? 0 : 1); } });
            hots.push_back ({ { keyX[c] - 40.f, 535.f, 80.f, 90.f }, [this, c] { choose (InjGateDS::KEY (c), (gt.vi (InjGateDS::KEY (c)) + 1) % 3); } }); }
        linkOn = skinImage ("gt_link_on_png"); linkOff = skinImage ("gt_link_off_png"); sdUp = skinImage ("gt_sd_up_png"); sdDn = skinImage ("gt_sd_dn_png");
        hots.push_back ({ { 1046.f, 255.f, 80.f, 80.f }, [this] { choose (InjGateDS::pLink, gt.vi (InjGateDS::pLink) ? 0 : 1); } });
        hots.push_back ({ { 1050.f, 425.f, 72.f, 100.f }, [this] { choose (InjGateDS::pStereo, gt.vi (InjGateDS::pStereo) ? 0 : 1); } });
        hots.push_back ({ { 1300.f, 20.f, 500.f, 90.f }, [this] { presetMenu(); } });
        if (std::getenv ("INJ_PUNCHDEMO")) { gt.applyPreset (1); lv[0] = -12.f; lv[1] = 4.f; demo = true; }
    }
protected:
    void tick() override { if (! demo) { lv[0] = std::max (gt.meterA.load(), lv[0] - 1.5f); lv[1] = std::max (gt.meterB.load(), lv[1] - 1.5f); } repaint(); }
    void paintSkin (juce::Graphics& g) override
    {
        for (int c = 0; c < 2; ++c)
        {
            drawSprite (g, gt.vi (InjGateDS::INN (c)) ? inOn[c] : inOff[c], inX[c], 185.f);
            const int n = juce::jlimit (0, 7, (int) std::floor ((lv[c] + 50.f) / 10.f));   // LED k se enciende cuando la señal pasa de -40 + 10k dB
            for (int k = 0; k < 7; ++k) if (lv[c] >= -40.f + 10.f * k - 5.f) drawLed (g, led[c], k, ledX[k] + c * 1086.f, 552.f);
            (void) n;
            const int kk = gt.vi (InjGateDS::KEY (c)); const float x0 = keyX[c];
            const juce::Line<float> tk[3] { { x0 - 28.f, 560.f, x0 - 19.f, 568.f }, { x0, 553.f, x0, 561.f }, { x0 + 21.f, 568.f, x0 + 30.f, 560.f } };
            g.setColour (juce::Colour (0xffffd24a).withAlpha (.35f)); g.drawLine (tk[kk].getStart().x * sc, tk[kk].getStart().y * sc, tk[kk].getEnd().x * sc, tk[kk].getEnd().y * sc, 7.f * sc);
            g.setColour (juce::Colour (0xfffff2c0)); g.drawLine (tk[kk].getStart().x * sc, tk[kk].getStart().y * sc, tk[kk].getEnd().x * sc, tk[kk].getEnd().y * sc, 3.f * sc);
        }
        drawSprite (g, gt.vi (InjGateDS::pLink) ? linkOn : linkOff, 1086.f, 295.f); drawSprite (g, gt.vi (InjGateDS::pStereo) ? sdDn : sdUp, 1086.f, 475.f);
    }
private:
    void presetMenu()
    {
        juce::PopupMenu m; auto ps = gt.presets(); m.addItem (1, "Inicial"); m.addSeparator();
        for (size_t i = 0; i < ps.size(); ++i) m.addItem (100 + (int) i, juce::String::fromUTF8 (ps[i].name), true, gt.presetName() == juce::String::fromUTF8 (ps[i].name));
        juce::Component::SafePointer<GateDSEditor> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [sp] (int r) { if (sp == nullptr) return; if (r == 1) sp->gt.resetAll(); else if (r >= 100) sp->gt.applyPreset (r - 100); sp->repaint(); });
    }
    InjGateDS& gt; juce::Image led[2], inOn[2], inOff[2], linkOn, linkOff, sdUp, sdDn; float lv[2] { -80.f, -80.f }; bool demo = false;
    static constexpr float inX[2] { 185.f, 2044.f }, keyX[2] { 203.f, 1289.f }, ledX[7] { 612.5f, 660.f, 707.5f, 754.7f, 802.f, 849.2f, 899.1f };
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjGateDS::createEditor() { return std::make_unique<GateDSEditor> (*this); }
} // namespace injfx
