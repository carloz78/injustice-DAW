#pragma once
#include "FxDeEsser.h"
// Attack Shaper: modelador de transitorios de cuatro canales (ataque y sustain de -6 a +6 dB por canal), con LINK 1-2 y 3-4. Cada canal procesa un canal de audio:
// en una pista estéreo trabajan los canales 1 y 2 (izquierda y derecha); el 3 y el 4 actúan cuando el bus tiene cuatro canales.
namespace injfx
{
class InjAttackShaper : public InjFx
{
public:
    static constexpr int pLink12 = 12, pLink34 = 13, pPower = 14;
    static constexpr int ATK (int c) { return c * 3; }
    static constexpr int SUS (int c) { return c * 3 + 1; }
    static constexpr int ON (int c) { return c * 3 + 2; }
    static inline const char* xmlTypeName = "injattackshaper";
    static const char* getPluginName() { return "Attack Shaper"; }
    InjAttackShaper (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "AtkShaper"; }
    const Spec* specs() const override { return table().data(); }
    int numSpecs() const override { return 15; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    static std::vector<float> defaults() { std::vector<float> d; for (auto& s : table()) d.push_back (s.def); return d; }
    std::vector<Preset> presets() const override
    {
        auto mk = [] (float a, float s, bool link) { auto d = defaults(); for (int c = 0; c < 4; ++c) { d[(size_t) ATK (c)] = a; d[(size_t) SUS (c)] = s; } d[pLink12] = d[pLink34] = link ? 1.f : 0.f; return d; };
        return { { "Plano", defaults() }, { "Más pegada", mk (5.f, -2.f, true) }, { "Más cuerpo", mk (-1.f, 5.f, true) }, { "Seco y corto", mk (3.f, -5.f, true) }, { "Sala viva", mk (-2.f, 6.f, true) } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; for (int c = 0; c < 4; ++c) { fast[c] = slow[c] = 0.f; g[c] = 1.f; } }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || vi (pPower) == 0) return;
        const int nc = std::min (4, fc.destBuffer->getNumChannels()); float* p[4]; for (int c = 0; c < nc; ++c) p[c] = fc.destBuffer->getWritePointer (c, fc.bufferStartSample);
        const float fA = coefMs (.8f, sr), fR = coefMs (25.f, sr), sA = coefMs (25.f, sr), sR = coefMs (120.f, sr), gA = coefMs (1.2f, sr);
        float atk[4], sus[4]; bool on[4]; int sig = 0;
        for (int c = 0; c < nc; ++c) { const int s = (c == 1 && vi (pLink12)) ? 0 : (c == 3 && vi (pLink34)) ? 2 : c; atk[c] = v (ATK (s)); sus[c] = v (SUS (s)); on[c] = vi (ON (c)) != 0; }
        for (int i = 0; i < fc.bufferNumSamples; ++i)
            for (int c = 0; c < nc; ++c)
            {
                const float x = p[c][i], a = std::abs (x);
                fast[c] = a > fast[c] ? fA * fast[c] + (1.f - fA) * a : fR * fast[c] + (1.f - fR) * a; slow[c] = a > slow[c] ? sA * slow[c] + (1.f - sA) * a : sR * slow[c] + (1.f - sR) * a;
                const float d = lin2db (fast[c] + 1e-7f) - lin2db (slow[c] + 1e-7f);
                const float gdb = d > 0.f ? atk[c] * juce::jmin (d, 9.f) / 9.f : sus[c] * juce::jmin (-d, 9.f) / 9.f;
                const float tg = on[c] ? db2lin (gdb) : 1.f; g[c] = tg + gA * (g[c] - tg); p[c][i] = x * g[c];
                if (slow[c] > .003f) sig |= 1 << c;
            }
        meterA.store ((float) sig);
    }
private:
    static const std::vector<Spec>& table()
    {
        static std::deque<std::string> keep; static std::vector<Spec> t;
        if (! t.empty()) return t;
        auto add = [&] (const std::string& id, const std::string& name, float lo, float hi, float def, int fmt, const char* ch) { keep.push_back (id); const char* a = keep.back().c_str(); keep.push_back (name); t.push_back ({ a, keep.back().c_str(), lo, hi, def, 0.f, fmt, ch }); };
        for (int c = 0; c < 4; ++c) { const std::string s = std::to_string (c + 1), n = "Canal " + s + ": "; add ("atk" + s, n + "Ataque", -6.f, 6.f, 0.f, fDb, ""); add ("sus" + s, n + "Sustain", -6.f, 6.f, 0.f, fDb, ""); add ("on" + s, n + "Activo", 0.f, 1.f, 1.f, fChoice, "No|Sí"); }
        add ("l12", "Enlace 1-2", 0.f, 1.f, 0.f, fChoice, "No|Sí"); add ("l34", "Enlace 3-4", 0.f, 1.f, 0.f, fChoice, "No|Sí"); add ("pw", "Encendido", 0.f, 1.f, 1.f, fChoice, "No|Sí");
        return t;
    }
    double sr = 44100.0; float fast[4] {}, slow[4] {}, g[4] { 1.f, 1.f, 1.f, 1.f };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjAttackShaper)
};

class AttackShaperEditor : public SkinEditor
{
public:
    AttackShaperEditor (InjAttackShaper& p) : SkinEditor (p, skinImage ("tr_bg_png"), .5f, { 0, 205, 2172, 315 }), at (p)
    {
        const float cx[8] { 415.7f, 583.f, 866.6f, 1032.5f, 1250.6f, 1415.8f, 1697.5f, 1868.7f }, cy[8] { 365.1f, 364.6f, 364.4f, 364.6f, 364.6f, 364.5f, 364.6f, 364.6f }, rot[8] { -4.f, -2.f, -2.f, -1.f, -1.f, 1.f, 1.f, 1.f };
        for (int k = 0; k < 8; ++k) { Knob kn { (k / 2) * 3 + (k % 2), cx[k], cy[k], 44.f, skinImage (("tr_cap" + juce::String (k) + "_png").toRawUTF8()) }; kn.a0 = -130.f; kn.a1 = 130.f; kn.rot0 = rot[k]; knobs.push_back (kn); }
        sig = skinImage ("tr_sig_png");
        for (int k = 0; k < 4; ++k)
        {
            onOn[k] = skinImage (("tr_on" + juce::String (k) + "_on_png").toRawUTF8()); onOff[k] = skinImage (("tr_on" + juce::String (k) + "_off_png").toRawUTF8());
            hots.push_back ({ { onX[k] - 34.f, oy - 34.f, 68.f, 68.f }, [this, k] { choose (InjAttackShaper::ON (k), at.vi (InjAttackShaper::ON (k)) ? 0 : 1); } });
        }
        for (int k = 0; k < 2; ++k)
        {
            lkOn[k] = skinImage (("tr_lk" + juce::String (k) + "_on_png").toRawUTF8()); lkOff[k] = skinImage (("tr_lk" + juce::String (k) + "_off_png").toRawUTF8());
            hots.push_back ({ { lkX[k] - 30.f, ly - 30.f, 60.f, 60.f }, [this, k] { const int i = k ? InjAttackShaper::pLink34 : InjAttackShaper::pLink12; choose (i, at.vi (i) ? 0 : 1); } });
        }
        pwOn = skinImage ("tr_pw_on_png"); pwOff = skinImage ("tr_pw_off_png");
        hots.push_back ({ { 2030.f - 58.f, 366.f - 68.f, 116.f, 136.f }, [this] { choose (InjAttackShaper::pPower, at.vi (InjAttackShaper::pPower) ? 0 : 1); } });
        hots.push_back ({ { 120.f, 390.f, 195.f, 95.f }, [this] { presetMenu(); } });
        if (std::getenv ("INJ_PUNCHDEMO")) { at.applyPreset (1); demo = true; }
    }
protected:
    void tick() override { if (! demo) mask = (int) at.meterA.load(); else mask = 0xf; repaint(); }
    void paintSkin (juce::Graphics& g) override
    {
        for (int k = 0; k < 4; ++k) { drawSprite (g, at.vi (InjAttackShaper::ON (k)) ? onOn[k] : onOff[k], onX[k], oy); if (mask & (1 << k)) drawLed (g, sig, k, sigX[k], oy); }
        drawSprite (g, at.vi (InjAttackShaper::pLink12) ? lkOn[0] : lkOff[0], lkX[0], ly); drawSprite (g, at.vi (InjAttackShaper::pLink34) ? lkOn[1] : lkOff[1], lkX[1], ly);
        drawSprite (g, at.vi (InjAttackShaper::pPower) ? pwOn : pwOff, 2030.f, 366.f);
    }
private:
    void presetMenu()
    {
        juce::PopupMenu m; auto ps = at.presets(); m.addItem (1, "Inicial"); m.addSeparator();
        for (size_t i = 0; i < ps.size(); ++i) m.addItem (100 + (int) i, juce::String::fromUTF8 (ps[i].name), true, at.presetName() == juce::String::fromUTF8 (ps[i].name));
        juce::Component::SafePointer<AttackShaperEditor> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [sp] (int r) { if (sp == nullptr) return; if (r == 1) sp->at.resetAll(); else if (r >= 100) sp->at.applyPreset (r - 100); sp->repaint(); });
    }
    InjAttackShaper& at; juce::Image sig, onOn[4], onOff[4], lkOn[2], lkOff[2], pwOn, pwOff; int mask = 0; bool demo = false;
    static constexpr float oy = 457.f, ly = 356.f, onX[4] { 404.f, 855.f, 1238.f, 1685.f }, sigX[4] { 554.9f, 1004.6f, 1386.8f, 1838.6f }, lkX[2] { 711.f, 1540.f };
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjAttackShaper::createEditor() { return std::make_unique<AttackShaperEditor> (*this); }
} // namespace injfx
