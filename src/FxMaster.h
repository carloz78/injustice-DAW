#pragma once
#include "FxBlackBrown.h"
#include <deque>
#include <string>
// Master EQ: ecualizador estéreo de masterización: cada canal con cuatro bandas (frecuencia por pasos, ganancia, campana o shelf, ×1 / fuera / ×1.5), pasaaltos, pasabajos y salida.
namespace injfx
{
class InjMaster : public InjFx
{
public:
    static constexpr int per = 21, pOn = 42;
    static constexpr int F (int c, int b) { return c * per + b; }          // frecuencia (selector)
    static constexpr int G (int c, int b) { return c * per + 4 + b; }      // ganancia ±15 dB
    static constexpr int SH (int c, int b) { return c * per + 8 + b; }     // campana / shelf
    static constexpr int MU (int c, int b) { return c * per + 12 + b; }    // ×1 / fuera / ×1.5
    static constexpr int HP (int c) { return c * per + 16; }
    static constexpr int LP (int c) { return c * per + 17; }
    static constexpr int HPI (int c) { return c * per + 18; }
    static constexpr int LPI (int c) { return c * per + 19; }
    static constexpr int OUT (int c) { return c * per + 20; }
    static inline const char* xmlTypeName = "injmaster";
    static const char* getPluginName() { return "Master EQ"; }
    InjMaster (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "MasterEQ"; }
    const Spec* specs() const override { return table().data(); }
    int numSpecs() const override { return 43; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    static std::vector<float> defaults() { std::vector<float> d; for (auto& s : table()) d.push_back (s.def); return d; }
    std::vector<Preset> presets() const override
    {
        auto mk = [] (std::initializer_list<std::pair<int, float>> ch) { auto d = defaults(); for (int c = 0; c < 2; ++c) for (auto& q : ch) d[(size_t) (c * per + q.first)] = q.second; return d; };
        return { { "Plano", defaults() },
                 { "Master brillante", mk ({ { 4 + 3, 2.f }, { 4 + 2, 1.5f }, { 3, 2.f }, { 2, 2.f } }) },
                 { "Master con cuerpo", mk ({ { 4 + 0, 2.5f }, { 4 + 1, -1.f }, { 0, 1.f } }) },
                 { "Limpieza", mk ({ { 16, 1.f }, { 4 + 1, -2.f }, { 1, 1.f } }) },
                 { "Pegada de bombo", mk ({ { 4 + 0, 4.f }, { 4 + 1, -2.f }, { 0, 1.f }, { 16, 1.f } }) } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; for (auto& c : f) for (auto& b : c) b.reset(); }
    static float bandHz (int b, int i) { static const float t[4][4] { { 30.f, 60.f, 100.f, 200.f }, { 700.f, 1500.f, 3000.f, 5000.f }, { 1500.f, 3000.f, 6000.f, 10000.f }, { 8000.f, 12000.f, 16000.f, 20000.f } }; return t[juce::jlimit (0, 3, b)][juce::jlimit (0, 3, i)]; }
    static float hpHz (int i) { static const float a[] { 0.f, 30.f, 60.f, 100.f, 200.f, 300.f }; return a[juce::jlimit (0, 5, i)]; }
    static float lpHz (int i) { static const float a[] { 0.f, 3000.f, 5000.f, 7000.f, 10000.f, 20000.f }; return a[juce::jlimit (0, 5, i)]; }
    void setFilters (int c)
    {
        for (int b = 0; b < 4; ++b)
        {
            const int mu = vi (MU (c, b)); const float gdb = mu == 1 ? 0.f : v (G (c, b)) * (mu == 2 ? 1.5f : 1.f), hz = bandHz (b, vi (F (c, b)));
            const bool shelf = vi (SH (c, b)) != 0;
            f[c][(size_t) b].set (shelf ? (b == 0 ? BQ::LowShelf : BQ::HighShelf) : BQ::Peak, sr, hz, shelf ? .7 : .8, gdb);
        }
        const float hp = hpHz (vi (HP (c))), lp = lpHz (vi (LP (c)));
        hpOn[c] = hp > 0.f && vi (HPI (c)) != 0; lpOn[c] = lp > 0.f && vi (LPI (c)) != 0;
        f[c][4].set (BQ::HP, sr, hp > 0.f ? hp : 10.f, .707, 0.0); f[c][5].set (BQ::LP, sr, lp > 0.f ? lp : sr * .49, .707, 0.0);
        outG[c] = db2lin (v (OUT (c)));
    }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || vi (pOn) == 0) return;
        for (int ch = 0; ch < std::min (2, fc.destBuffer->getNumChannels()); ++ch)
        {
            setFilters (ch); float* p = fc.destBuffer->getWritePointer (ch, fc.bufferStartSample);
            for (int i = 0; i < fc.bufferNumSamples; ++i)
            {
                float x = p[i]; for (int b = 0; b < 4; ++b) x = f[ch][(size_t) b].run (ch, x);
                if (hpOn[ch]) x = f[ch][4].run (ch, x); if (lpOn[ch]) x = f[ch][5].run (ch, x);
                p[i] = x * outG[ch];
            }
        }
    }
private:
    static const std::vector<Spec>& table()
    {
        static std::deque<std::string> keep; static std::vector<Spec> t;
        if (! t.empty()) return t;
        auto add = [&] (const std::string& id, const std::string& name, float lo, float hi, float def, int fmt, const char* choices) { keep.push_back (id); const char* a = keep.back().c_str(); keep.push_back (name); t.push_back ({ a, keep.back().c_str(), lo, hi, def, 0.f, fmt, choices }); };
        static const char* fr[4] { "30 Hz|60 Hz|100 Hz|200 Hz", "700 Hz|1.5 kHz|3 kHz|5 kHz", "1.5 kHz|3 kHz|6 kHz|10 kHz", "8 kHz|12 kHz|16 kHz|20 kHz" }; static const char* bn[4] { "Bass", "Presence 2", "Presence 1", "Treble" };
        for (int c = 0; c < 2; ++c)
        {
            const std::string ch = c == 0 ? "L" : "R", chn = c == 0 ? "Izq: " : "Der: ";
            for (int b = 0; b < 4; ++b) add ("f" + ch + std::to_string (b), chn + bn[b] + " frecuencia", 0.f, 3.f, 2.f, fChoice, fr[b]);
            for (int b = 0; b < 4; ++b) add ("g" + ch + std::to_string (b), chn + bn[b] + " ganancia", -15.f, 15.f, 0.f, fDb, "");
            for (int b = 0; b < 4; ++b) add ("s" + ch + std::to_string (b), chn + bn[b] + " forma", 0.f, 1.f, (b == 0 || b == 3) ? 1.f : 0.f, fChoice, "Campana|Shelf");
            for (int b = 0; b < 4; ++b) add ("m" + ch + std::to_string (b), chn + bn[b] + " rango", 0.f, 2.f, 0.f, fChoice, "x1|Fuera|x1.5");
            add ("hp" + ch, chn + "Pasaaltos", 0.f, 5.f, 0.f, fChoice, "Fuera|30 Hz|60 Hz|100 Hz|200 Hz|300 Hz"); add ("lp" + ch, chn + "Pasabajos", 0.f, 5.f, 0.f, fChoice, "Fuera|3 kHz|5 kHz|7 kHz|10 kHz|20 kHz");
            add ("hi" + ch, chn + "Pasaaltos activo", 0.f, 1.f, 1.f, fChoice, "No|Si"); add ("li" + ch, chn + "Pasabajos activo", 0.f, 1.f, 1.f, fChoice, "No|Si");
            add ("o" + ch, chn + "Salida", -6.f, 6.f, 0.f, fDb, "");
        }
        add ("on", "Encendido", 0.f, 1.f, 1.f, fChoice, "No|Si");
        return t;
    }
    double sr = 44100.0; BQ f[2][6]; bool hpOn[2] { false, false }, lpOn[2] { false, false }; float outG[2] { 1.f, 1.f };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjMaster)
};

class MasterEditor : public SkinEditor
{
public:
    MasterEditor (InjMaster& p) : SkinEditor (p, skinImage ("ms_bg_png"), .46f), ma (p)
    {
        const float rx[2][7] { { 407.3f, 681.3f, 960.8f, 1243.5f, 1526.4f, 1721.8f, 1907.8f }, { 408.1f, 683.8f, 962.7f, 1244.1f, 1526.4f, 1721.1f, 1907.2f } }, ry[2][7] { { 142.1f, 142.6f, 143.f, 142.8f, 153.5f, 154.8f, 183.1f }, { 482.3f, 481.8f, 482.1f, 482.3f, 496.f, 496.7f, 525.6f } };
        const float rrot[2][7] { { 2.f, -1.f, 1.f, 1.f, 1.f, 2.f, 2.f }, { 2.f, 0.f, 1.f, 3.f, 1.f, 3.f, 3.f } };
        const float sx[2][4] { { 365.5f, 651.4f, 929.5f, 1210.9f }, { 366.3f, 650.7f, 930.5f, 1212.2f } }, sy[2][4] { { 270.9f, 271.2f, 270.8f, 270.5f }, { 607.8f, 607.7f, 607.f, 606.6f } }, srot[2][4] { { 10.f, 10.f, 14.f, 19.f }, { 6.f, 8.f, 13.f, 18.f } };
        for (int c = 0; c < 2; ++c)
        {
            for (int i = 0; i < 7; ++i)
            {
                const int idx = i < 4 ? InjMaster::F (c, i) : i == 4 ? InjMaster::HP (c) : i == 5 ? InjMaster::LP (c) : InjMaster::OUT (c);
                Knob kn { idx, rx[c][i], ry[c][i], 38.f, skinImage (("ms_red" + juce::String (c) + "_" + juce::String (i) + "_png").toRawUTF8()) }; kn.rot0 = rrot[c][i];
                if (i < 4) kn.stops = { -55.f, -18.f, 18.f, 55.f }; else if (i == 4) kn.stops = { -97.f, -60.f, -31.f, 2.f, 38.f, 73.f }; else if (i == 5) kn.stops = { -103.f, -36.f, 0.f, 31.5f, 62.f, 86.f }; else { kn.a0 = -117.f; kn.a1 = 117.f; }
                knobs.push_back (kn);
            }
            for (int b = 0; b < 4; ++b) { Knob kn { InjMaster::G (c, b), sx[c][b], sy[c][b], 38.f, skinImage (("ms_st" + juce::String (c) + "_" + juce::String (b) + "_png").toRawUTF8()) }; kn.a0 = -135.f; kn.a1 = 135.f; kn.rot0 = srot[c][b]; knobs.push_back (kn); }
        }
        // posiciones de los interruptores (halladas sobre la imagen)
        const float bx[2][4] { { 507.f, 779.f, 1058.f, 1342.f }, { 506.f, 780.f, 1059.f, 1343.f } }, by[2] { 148.5f, 487.f }, mx[2][4] { { 491.f, 768.f, 1050.f, 1336.f }, { 491.f, 768.f, 1051.f, 1336.f } }, my[2] { 270.5f, 607.5f };
        for (int c = 0; c < 2; ++c) for (int b = 0; b < 4; ++b)
        {
            bellUp[c][b] = skinImage (("ms_bell" + juce::String (c) + juce::String (b) + "_up_png").toRawUTF8()); bellDn[c][b] = skinImage (("ms_bell" + juce::String (c) + juce::String (b) + "_dn_png").toRawUTF8());
            for (int s = 0; s < 3; ++s) mul[c][b][s] = skinImage (("ms_mul" + juce::String (c) + juce::String (b) + (s == 0 ? "_up_png" : s == 1 ? "_mid_png" : "_dn_png")).toRawUTF8());
            bxy[c][b] = { bx[c][b], by[c] }; mxy[c][b] = { mx[c][b], my[c] };
            hots.push_back ({ { bx[c][b] - 18.f, by[c] - 30.f, 36.f, 60.f }, [this, c, b] { choose (InjMaster::SH (c, b), ma.vi (InjMaster::SH (c, b)) ? 0 : 1); } });
            hots.push_back ({ { mx[c][b] - 20.f, my[c] - 40.f, 40.f, 80.f }, [this, c, b] { choose (InjMaster::MU (c, b), (ma.vi (InjMaster::MU (c, b)) + 1) % 3); } });   // cada clic pasa a la siguiente posición
        }
        for (int c = 0; c < 2; ++c) { hpOn[c] = skinImage (("ms_hp" + juce::String (c) + "_on_png").toRawUTF8()); hpOff[c] = skinImage (("ms_hp" + juce::String (c) + "_off_png").toRawUTF8()); lpOn[c] = skinImage (("ms_lp" + juce::String (c) + "_on_png").toRawUTF8()); lpOff[c] = skinImage (("ms_lp" + juce::String (c) + "_off_png").toRawUTF8());
            const float dy = c == 0 ? 0.f : 341.f; hots.push_back ({ { 1488.f, 247.f + dy, 84.f, 72.f }, [this, c] { choose (InjMaster::HPI (c), ma.vi (InjMaster::HPI (c)) ? 0 : 1); } }); hots.push_back ({ { 1682.f, 247.f + dy, 84.f, 72.f }, [this, c] { choose (InjMaster::LPI (c), ma.vi (InjMaster::LPI (c)) ? 0 : 1); } }); }
        lampOn = skinImage ("ms_lamp_on_png"); lampOff = skinImage ("ms_lamp_off_png"); pwUp = skinImage ("ms_pw_up_png"); pwDn = skinImage ("ms_pw_dn_png");
        hots.push_back ({ { 2088.f - 42.f, 435.f - 56.f, 84.f, 112.f }, [this] { choose (InjMaster::pOn, ma.vi (InjMaster::pOn) ? 0 : 1); } });
        hots.push_back ({ { 40.f, 365.f, 220.f, 60.f }, [this] { presetMenu(); } });
        if (std::getenv ("INJ_PUNCHDEMO")) ma.applyPreset (1);
    }
protected:
    void paintSkin (juce::Graphics& g) override
    {
        for (int c = 0; c < 2; ++c)
        {
            for (int b = 0; b < 4; ++b) { drawSprite (g, ma.vi (InjMaster::SH (c, b)) ? bellDn[c][b] : bellUp[c][b], bxy[c][b].x, bxy[c][b].y); drawSprite (g, mul[c][b][juce::jlimit (0, 2, ma.vi (InjMaster::MU (c, b)))], mxy[c][b].x, mxy[c][b].y); }
            const float dy = c == 0 ? 0.f : 341.f; drawSprite (g, ma.vi (InjMaster::HPI (c)) ? hpOn[c] : hpOff[c], 1530.f, 283.f + dy); drawSprite (g, ma.vi (InjMaster::LPI (c)) ? lpOn[c] : lpOff[c], 1724.f, 283.f + dy);
        }
        const bool on = ma.vi (InjMaster::pOn) != 0; drawSprite (g, on ? lampOn : lampOff, 2085.f, 215.f); drawSprite (g, on ? pwUp : pwDn, 2088.f, 435.f);
    }
private:
    void presetMenu()
    {
        juce::PopupMenu m; auto ps = ma.presets(); m.addItem (1, "Inicial"); m.addSeparator();
        for (size_t i = 0; i < ps.size(); ++i) m.addItem (100 + (int) i, juce::String::fromUTF8 (ps[i].name), true, ma.presetName() == juce::String::fromUTF8 (ps[i].name));
        juce::Component::SafePointer<MasterEditor> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [sp] (int r) { if (sp == nullptr) return; if (r == 1) sp->ma.resetAll(); else if (r >= 100) sp->ma.applyPreset (r - 100); sp->repaint(); });
    }
    InjMaster& ma; juce::Image bellUp[2][4], bellDn[2][4], mul[2][4][3], hpOn[2], hpOff[2], lpOn[2], lpOff[2], lampOn, lampOff, pwUp, pwDn; juce::Point<float> bxy[2][4], mxy[2][4];
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjMaster::createEditor() { return std::make_unique<MasterEditor> (*this); }
} // namespace injfx
