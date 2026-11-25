#pragma once
#include "FxTape.h"
// Spring Reverb y Tape Echo, con el panel de las imágenes del usuario.
//  - Spring Reverb: dos resortes en paralelo por canal; cada uno es una línea de retardo con una cadena de filtros pasa-todo (la dispersión que hace el "boing" característico)
//    y realimentación filtrada. GAIN empuja el tanque (con saturación suave), TONE abre o cierra el brillo del retorno, MIX mezcla, OUTPUT LEVEL ajusta la salida.
//  - Tape Echo: una cinta con tres cabezas de lectura a 1x, 2x y 3x el tiempo base; MODE SELECTOR elige qué cabezas suenan (1, 2, 3, 1+2, 2+3, 1+2+3),
//    REPEAT RATE cambia la velocidad de la cinta (tiempo base de 340 ms a 50 ms, con el deslizamiento de tono al moverlo), INTENSITY es la realimentación (llega a auto-oscilar)
//    y la cinta oscurece y satura cada vuelta; ECHO VOLUME / REVERB VOLUME mezclan el eco y el resorte, y DIRECT—ECHO reparte entre señal directa y eco.
namespace injfx
{
struct SpringTank
{
    static constexpr int NS = 2, NA = 26, MASK = 8191;
    void init (double sr, int variant)
    {
        static const double ms[2][2] { { 29.7, 41.3 }, { 33.1, 45.7 } };
        for (int s = 0; s < NS; ++s) { len[s] = juce::jlimit (64, MASK - 2, (int) std::lround (ms[variant & 1][s] * sr / 1000.0)); std::fill (std::begin (buf[s]), std::end (buf[s]), 0.f); lp[s] = 0.f; for (int k = 0; k < NA; ++k) ax[s][k] = ay[s][k] = 0.f; }
        w = 0; lpA = (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * 4200.0 / sr)); dcx = dcy = 0.f;
    }
    float process (float x, float fb)
    {
        float out = 0.f; const float c = .62f;
        for (int s = 0; s < NS; ++s)
        {
            float y = buf[s][(w - len[s]) & MASK];
            for (int k = 0; k < NA; ++k) { const float t = c * y + ax[s][k] - c * ay[s][k]; ax[s][k] = y; ay[s][k] = t; y = t; }
            lp[s] += lpA * (y - lp[s]); y = lp[s];
            buf[s][w & MASK] = x * (s == 0 ? 1.f : .8f) + y * fb + 1e-18f; out += y * .6f;
        }
        ++w; const float o = out - dcx + .995f * dcy; dcx = out; dcy = o; return o;
    }
    float buf[NS][MASK + 1] {}, ax[NS][NA] {}, ay[NS][NA] {}, lp[NS] {}; int len[NS] {}, w = 0; float lpA = .4f, dcx = 0.f, dcy = 0.f;
};

class InjSpring : public InjFx
{
public:
    enum { pGain, pTone, pMix, pOut, pEffect };
    static inline const char* xmlTypeName = "injspring";
    static const char* getPluginName() { return "Spring Reverb"; }
    InjSpring (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Spring"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 5; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    std::vector<Preset> presets() const override
    {   //                     Gain Tone Mix Out Efecto
        return { { "Surf clásico", { 6.f, 6.f, 4.f, 5.f, 1.f } }, { "Resorte oscuro", { 5.f, 2.f, 3.f, 5.f, 1.f } }, { "Voz con aire", { 4.f, 8.f, 2.f, 5.f, 1.f } },
                 { "Tanque a tope", { 9.f, 5.f, 6.f, 4.f, 1.f } }, { "Golpe de caja", { 7.f, 4.f, 5.f, 5.f, 1.f } } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override
    {
        sr = i.sampleRate; for (int c = 0; c < 2; ++c) tank[c].init (sr, c); for (auto* b : { &toneLP, &hp }) b->reset(); eff = 1.f; env[0] = env[1] = 0.f;
    }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nullptr }; p[1] = nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : p[0];
        const int n = fc.bufferNumSamples; const float mix = v (pMix) * .1f, gDrive = db2lin ((v (pGain) - 5.f) * 2.4f) * .55f, outG = db2lin ((v (pOut) - 5.f) * 2.4f);
        const float dryG = std::cos (mix * 1.5707963f), wetG = std::sin (mix * 1.5707963f) * 2.6f, effTarget = vi (pEffect) ? 1.f : 0.f, ea = coefMs (300.f, sr), fb = .74f, ef = 1.f - std::exp (-1.f / (float) (.02 * sr));
        toneLP.set (BQ::LP, sr, 1400.0 * std::pow (8.0, v (pTone) * .1), .6, 0.0); hp.set (BQ::HP, sr, 130.0, .7, 0.0);
        for (int i = 0; i < n; ++i)
        {
            eff += (effTarget - eff) * ef;
            for (int c = 0; c < 2; ++c)
            {
                if (c == 1 && nc < 2) { p[1][i] = p[0][i]; break; }
                const float in = p[c][i]; const float x = std::tanh (in * gDrive * 1.4f) * .8f; float wet = tank[c].process (x, fb);
                wet = hp.run (c, toneLP.run (c, wet)); const float o = (in * dryG + wet * wetG * eff) * outG; p[c][i] = o;
                const float ao = std::abs (o); env[c] = ea * env[c] + (1.f - ea) * ao;
            }
        }
        meterA.store (lin2db (env[0] * 1.11f + 1e-9f)); meterB.store (lin2db (env[nc > 1 ? 1 : 0] * 1.11f + 1e-9f));
    }
private:
    static inline const Spec S[] = {
        { "gain", "Gain", 0.f, 10.f, 5.f, 0.f, fNum, "" }, { "tone", "Tone", 0.f, 10.f, 5.f, 0.f, fNum, "" }, { "mix", "Mix", 0.f, 10.f, 4.f, 0.f, fNum, "" }, { "out", "Output level", 0.f, 10.f, 5.f, 0.f, fNum, "" },
        { "effect", "Efecto", 0.f, 1.f, 1.f, 0.f, fChoice, "Parado|Activo" } };
    double sr = 44100.0; SpringTank tank[2]; BQ toneLP, hp; float eff = 1.f, env[2] {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjSpring)
};

class InjTapeEcho : public InjFx
{
public:
    enum { pIn, pOut, pMode, pRate, pInt, pEcho, pRev, pBal, pPower };
    static inline const char* xmlTypeName = "injtapeecho";
    static const char* getPluginName() { return "Tape Echo"; }
    InjTapeEcho (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Echo"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 9; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    std::vector<Preset> presets() const override
    {   //                      In  Out Modo Rate Int Eco Rev Bal Power
        return { { "Slapback", { 5.f, 5.f, 0.f, 9.f, 2.f, 5.f, 0.f, .5f, 1.f } }, { "Eco de dub", { 5.f, 5.f, 3.f, 3.f, 6.f, 6.f, 3.f, .5f, 1.f } }, { "Tres cabezas", { 5.f, 5.f, 5.f, 4.f, 4.f, 5.f, 2.f, .5f, 1.f } },
                 { "Nave espacial", { 5.f, 5.f, 4.f, 6.f, 9.f, 6.f, 5.f, .6f, 1.f } }, { "Eco con resorte", { 5.f, 5.f, 1.f, 5.f, 4.f, 5.f, 6.f, .5f, 1.f } }, { "Autooscilación", { 4.f, 5.f, 2.f, 7.f, 10.f, 6.f, 2.f, .6f, 1.f } } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override
    {
        sr = i.sampleRate; for (int c = 0; c < 2; ++c) { std::fill (std::begin (dl[c]), std::end (dl[c]), 0.f); rev[c].init (sr, c); env[c] = 0.f; } w = 0; ph1 = ph2 = 0.0; for (auto* b : { &loopHP, &loopLP, &revHP }) b->reset();
        tSm = (float) timeFor (v (pRate)); pw = 1.f; pkAtt = 0.f;
    }
    double timeFor (float rate) const { return .34 * std::exp (-1.92 * (double) rate * .1); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nullptr }; p[1] = nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : p[0];
        const int n = fc.bufferNumSamples, mode = vi (pMode); static const int masks[6] { 1, 2, 4, 3, 6, 7 }; const int mask = masks[juce::jlimit (0, 5, mode)];
        const float norm = mask == 7 ? .7f : (mask == 3 || mask == 6) ? .85f : 1.f, gIn = db2lin ((v (pIn) - 5.f) * 2.4f), outG = db2lin ((v (pOut) - 5.f) * 2.4f), fbG = v (pInt) * .1f * 1.06f;
        const float echoG = v (pEcho) * .1f * 1.25f, revG = v (pRev) * .1f * 1.8f, bal = v (pBal), dirG = std::min (1.f, 2.f * (1.f - bal)), ecoBal = std::min (1.f, 2.f * bal);
        const double tTarget = std::min (timeFor (v (pRate)), 125000.0 / (3.0 * sr)); const float powTarget = vi (pPower) ? 1.f : 0.f;
        const float sm = 1.f - std::exp (-1.f / (float) (.12 * sr)), pf = 1.f - std::exp (-1.f / (float) (.01 * sr)), ea = coefMs (300.f, sr);
        loopHP.set (BQ::HP, sr, 110.0, .7, 0.0); loopLP.set (BQ::LP, sr, 5200.0, .7, 0.0); revHP.set (BQ::HP, sr, 160.0, .7, 0.0);
        const double dPh1 = 2.0 * juce::MathConstants<double>::pi * .55 / sr, dPh2 = 2.0 * juce::MathConstants<double>::pi * 6.1 / sr; float pk = 0.f;
        for (int i = 0; i < n; ++i)
        {
            tSm += ((float) tTarget - tSm) * sm; pw += (powTarget - pw) * pf;
            const double mod = (std::sin (ph1) * .75 + std::sin (ph2) * .25) * .00022 * sr; ph1 += dPh1; ph2 += dPh2; if (ph1 > 6.2831853) ph1 -= 6.2831853; if (ph2 > 6.2831853) ph2 -= 6.2831853;
            for (int c = 0; c < 2; ++c)
            {
                if (c == 1 && nc < 2) { p[1][i] = p[0][i]; break; }
                const float dry = p[c][i], x = dry * gIn; pk = std::max (pk, std::abs (x));
                float echo = 0.f;
                for (int h = 0; h < 3; ++h)
                {
                    if (! (mask & (1 << h))) continue;
                    const double dd = std::max (4.0, (h + 1) * (double) tSm * sr + mod); const int i0 = (int) dd; const float fr = (float) (dd - i0);
                    const float a = dl_at (c, i0), b = dl_at (c, i0 + 1); echo += (a + (b - a) * fr) * norm;
                }
                float f = loopLP.run (c, loopHP.run (c, echo * fbG)); f = std::tanh (f * 1.25f) / 1.25f;
                dl[c][w & MASKD] = x + f + 1e-18f;
                const float rv = revHP.run (c, rev[c].process ((x + echo * .5f) * .8f, .74f));
                const float o = (dry * dirG + echo * echoG * ecoBal + rv * revG) * outG; const float res = dry + (o - dry) * pw; p[c][i] = res;
                const float ao = std::abs (res); env[c] = ea * env[c] + (1.f - ea) * ao;
            }
            ++w;
        }
        meterA.store (lin2db (env[0] * 1.11f + 1e-9f)); meterB.store (lin2db (env[nc > 1 ? 1 : 0] * 1.11f + 1e-9f));
        pkAtt = std::max (pk, pkAtt * .93f); peak.store (pkAtt);
    }
    std::atomic<float> peak { 0.f };
private:
    static constexpr int MASKD = (1 << 17) - 1;
    float dl_at (int c, int back) const { return dl[c][(w - back) & MASKD]; }
    static inline const Spec S[] = {
        { "in", "Input volume", 0.f, 10.f, 5.f, 0.f, fNum, "" }, { "out", "Output volume", 0.f, 10.f, 5.f, 0.f, fNum, "" }, { "mode", "Mode selector", 0.f, 5.f, 3.f, 0.f, fChoice, "1|2|3|4|5|6" }, { "rate", "Repeat rate", 0.f, 10.f, 5.f, 0.f, fNum, "" },
        { "int", "Intensity", 0.f, 10.f, 4.f, 0.f, fNum, "" }, { "echo", "Echo volume", 0.f, 10.f, 5.f, 0.f, fNum, "" }, { "rev", "Reverb volume", 0.f, 10.f, 2.f, 0.f, fNum, "" }, { "bal", "Direct — Echo", 0.f, 1.f, .5f, 0.f, fPct, "" },
        { "power", "Power", 0.f, 1.f, 1.f, 0.f, fChoice, "Apagado|Encendido" } };
    double sr = 44100.0, ph1 = 0.0, ph2 = 0.0; SpringTank rev[2]; BQ loopHP, loopLP, revHP; float dl[2][1 << 17] {}; float env[2] {}; int w = 0; float tSm = .17f, pw = 1.f, pkAtt = 0.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjTapeEcho)
};

class SpringEditor : public SkinEditor
{
public:
    SpringEditor (InjSpring& p) : SkinEditor (p, skinImage ("sp_bg_png"), .8f), sp (p)
    {
        const float cx[4] { 392.7f, 574.7f, 764.2f, 950.2f }, cy[4] { 167.1f, 167.f, 168.2f, 167.1f }, rot[4] { 0.f, 1.f, 28.f, 5.f }; const int prm[4] { InjSpring::pGain, InjSpring::pTone, InjSpring::pMix, InjSpring::pOut };
        for (int k = 0; k < 4; ++k) { Knob kn { prm[k], cx[k], cy[k], 36.f, skinImage (("sp_cap" + juce::String (k) + "_png").toRawUTF8()) }; kn.a0 = -150.f; kn.a1 = 150.f; kn.rot0 = rot[k]; knobs.push_back (kn); }
        on = skinImage ("sp_btn_on_png"); off = skinImage ("sp_btn_off_png");
        hots.push_back ({ { 1135.f - 30.f, 162.f - 30.f, 60.f, 60.f }, [this] { choose (InjSpring::pEffect, sp.vi (InjSpring::pEffect) ? 0 : 1); } });
        hots.push_back ({ { 560.f, 10.f, 360.f, 90.f }, [this] { presetMenu(); } });
    }
protected:
    void paintSkin (juce::Graphics& g) override { drawSprite (g, sp.vi (InjSpring::pEffect) ? on : off, 1135.f, 162.f); }
private:
    void presetMenu()
    {
        juce::PopupMenu mn; auto ps = sp.presets(); mn.addItem (1, "Inicial"); mn.addSeparator();
        for (size_t i = 0; i < ps.size(); ++i) mn.addItem (100 + (int) i, juce::String::fromUTF8 (ps[i].name), true, sp.presetName() == juce::String::fromUTF8 (ps[i].name));
        juce::Component::SafePointer<SpringEditor> s (this);
        mn.showMenuAsync (juce::PopupMenu::Options(), [s] (int r) { if (s == nullptr) return; if (r == 1) s->sp.resetAll(); else if (r >= 100) s->sp.applyPreset (r - 100); s->repaint(); });
    }
    InjSpring& sp; juce::Image on, off;
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjSpring::createEditor() { return std::make_unique<SpringEditor> (*this); }

class TapeEchoEditor : public SkinEditor
{
public:
    TapeEchoEditor (InjTapeEcho& p) : SkinEditor (p, skinImage ("te_bg_png"), .8f), te (p)
    {
        const float cx[7] { 159.4f, 368.8f, 587.7f, 789.3f, 920.5f, 1056.3f, 1202.1f }, cy[7] { 399.6f, 399.4f, 311.5f, 313.8f, 313.8f, 313.7f, 313.7f }, rot[7] { -90.f, 28.f, 0.f, -107.f, -109.f, -8.f, -9.f };
        const int prm[7] { InjTapeEcho::pIn, InjTapeEcho::pOut, InjTapeEcho::pMode, InjTapeEcho::pRate, InjTapeEcho::pInt, InjTapeEcho::pEcho, InjTapeEcho::pRev };
        for (int k = 0; k < 7; ++k)
        {
            Knob kn { prm[k], cx[k], cy[k], k == 2 ? 52.f : 42.f, skinImage (("te_cap" + juce::String (k) + "_png").toRawUTF8()) }; kn.a0 = -150.f; kn.a1 = 150.f; kn.rot0 = rot[k];
            if (k == 2) kn.stops = { -112.5f, -75.f, -37.5f, 0.f, 37.5f, 75.f };
            knobs.push_back (kn);
        }
        slider = skinImage ("te_slider_png"); led = skinImage ("te_led_png"); pwUp = skinImage ("te_pw_up_png"); pwDn = skinImage ("te_pw_dn_png");
        hots.push_back ({ { 1160.f - 30.f, 457.f - 32.f, 60.f, 64.f }, [this] { choose (InjTapeEcho::pPower, te.vi (InjTapeEcho::pPower) ? 0 : 1); } });
        hots.push_back ({ { 985.f, 70.f, 270.f, 70.f }, [this] { presetMenu(); } });
        if (std::getenv ("INJ_PUNCHDEMO")) { te.setValue (InjTapeEcho::pBal, .75f); vu[0] = vu[1] = -2.f; demo = true; }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto pt = e.position / sc + org; if (slotRect.contains (pt)) { sliding = true; setBal (pt.x); return; } SkinEditor::mouseDown (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (sliding) setBal ((e.position / sc + org).x); else SkinEditor::mouseDrag (e); }
    void mouseUp (const juce::MouseEvent& e) override { if (sliding) { sliding = false; return; } SkinEditor::mouseUp (e); }
    void mouseDoubleClick (const juce::MouseEvent& e) override { if (slotRect.contains (e.position / sc + org)) { te.setValue (InjTapeEcho::pBal, .5f); repaint(); return; } SkinEditor::mouseDoubleClick (e); }
protected:
    void tick() override
    {
        if (! demo) { const float tgt[2] { te.meterA.load() + 18.f, te.meterB.load() + 18.f }; for (int c = 0; c < 2; ++c) vu[c] += (juce::jmax (-25.f, tgt[c]) - vu[c]) * .25f; }
        repaint();
    }
    void paintSkin (juce::Graphics& g) override
    {
        const bool onP = te.vi (InjTapeEcho::pPower) != 0;
        drawSprite (g, onP ? pwUp : pwDn, 1160.f, 457.f);
        drawSprite (g, slider, sliderX (te.v (InjTapeEcho::pBal)), 466.5f);
        if (onP) drawLed (g, led, 1, 1065.f, 462.f);
        if (onP && (te.peak.load() > .9f || demo)) drawLed (g, led, 0, 156.f, 203.f);
        static const float vv[9] { -20.f, -10.f, -5.f, -3.f, -1.f, 0.f, 1.f, 2.f, 3.f }, aa[9] { -33.3f, -21.5f, -10.f, 0.f, 10.f, 18.7f, 22.5f, 27.5f, 32.8f };
        const float x = juce::jlimit (-20.f, 3.f, vu[0] * .5f + vu[1] * .5f); float deg = aa[8]; for (int j = 0; j < 8; ++j) if (x <= vv[j + 1]) { deg = aa[j] + (aa[j + 1] - aa[j]) * (x - vv[j]) / (vv[j + 1] - vv[j]); break; }
        if (vu[0] * .5f + vu[1] * .5f < -20.f) deg = aa[0] - juce::jmin (4.f, -20.f - (vu[0] * .5f + vu[1] * .5f)) * .5f;
        drawNeedle (g, 327.8f, 317.f, deg, 45.f, 122.f, { 234.f, 182.f, 182.f, 90.f }, juce::Colour (0xff17130f), 2.4f);
    }
private:
    static float sliderX (float b) { return 695.5f + 29.f * juce::jlimit (0.f, 1.f, b); }
    void setBal (float x) { te.setValue (InjTapeEcho::pBal, juce::jlimit (0.f, 1.f, (x - 695.5f) / 29.f)); repaint(); }
    void presetMenu()
    {
        juce::PopupMenu mn; auto ps = te.presets(); mn.addItem (1, "Inicial"); mn.addSeparator();
        for (size_t i = 0; i < ps.size(); ++i) mn.addItem (100 + (int) i, juce::String::fromUTF8 (ps[i].name), true, te.presetName() == juce::String::fromUTF8 (ps[i].name));
        juce::Component::SafePointer<TapeEchoEditor> s (this);
        mn.showMenuAsync (juce::PopupMenu::Options(), [s] (int r) { if (s == nullptr) return; if (r == 1) s->te.resetAll(); else if (r >= 100) s->te.applyPreset (r - 100); s->repaint(); });
    }
    InjTapeEcho& te; juce::Image slider, led, pwUp, pwDn; float vu[2] { -25.f, -25.f }; bool demo = false, sliding = false; const juce::Rectangle<float> slotRect { 668.f, 452.f, 84.f, 30.f };
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjTapeEcho::createEditor() { return std::make_unique<TapeEchoEditor> (*this); }
} // namespace injfx
