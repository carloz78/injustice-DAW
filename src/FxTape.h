#pragma once
#include "FxTubeSat.h"
// Tape Saturation: saturación de cinta con nivel de grabación y reproducción por canal, DRIVE, BIAS, recortes de agudos y graves, mezcla y salida,
// tres velocidades (7.5, 15, 30 ips: más o menos agudos, "golpe" de graves y vaivén), tres tipos de cinta (A cálida, B caliente, C limpia),
// curva NAB / CCIR y zumbido opcional. Los carretes giran mientras entra señal.
namespace injfx
{
class InjTapeSat : public InjFx
{
public:
    enum { pRecL, pRepL, pRecR, pRepR, pDrive, pBias, pHF, pLF, pMix, pOut, pSpeed, pType, pHum, pEq };
    static inline const char* xmlTypeName = "injtapesat";
    static const char* getPluginName() { return "Tape Saturation"; }
    InjTapeSat (te::PluginCreationInfo i) : InjFx (i) { buildParams(); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Tape"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 14; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    std::vector<Preset> presets() const override
    {   //                      RecL RepL RecR RepR Drive Bias HF  LF  Mix  Out Vel Tipo Hum Eq
        return { { "Pegamento suave", { 5.f, 5.f, 5.f, 5.f, 3.f, 0.f, 1.f, 1.f, 1.f, 0.f, 1.f, 0.f, 0.f, 0.f } }, { "Cinta caliente", { 5.f, 5.f, 5.f, 5.f, 6.f, 0.f, -1.f, 2.f, 1.f, -2.f, 0.f, 0.f, 0.f, 0.f } },
                 { "Golpe en el master", { 5.f, 5.f, 5.f, 5.f, 4.f, 1.f, 2.f, 2.f, 0.7f, -1.f, 1.f, 1.f, 0.f, 1.f } }, { "Lo-fi de casete", { 5.f, 5.f, 5.f, 5.f, 8.f, -2.f, -4.f, 3.f, 1.f, -3.f, 0.f, 1.f, 1.f, 0.f } },
                 { "Brillo limpio", { 5.f, 5.f, 5.f, 5.f, 1.f, 0.f, 3.f, 0.f, 0.8f, 0.f, 2.f, 2.f, 0.f, 1.f } }, { "Saturación pesada (gabber)", { 6.f, 4.f, 6.f, 4.f, 9.f, 1.f, -1.f, 3.f, 0.85f, -4.f, 1.f, 1.f, 0.f, 0.f } } };
    }
    void initialise (const te::PluginInitialisationInfo& i) override
    {
        sr = i.sampleRate; for (int c = 0; c < 2; ++c) { std::fill (std::begin (dly[c]), std::end (dly[c]), 0.f); std::fill (std::begin (dry[c]), std::end (dry[c]), 0.f); env[c] = 0.f; dcs[c] = 0.f; }
        w = 0; ph1 = ph2 = hum = 0.0; for (auto* b : { &bump, &lsT, &hsT, &lpS, &eqL, &eqH }) b->reset();
    }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nullptr }; p[1] = nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : p[0];
        const int n = fc.bufferNumSamples, sp = vi (pSpeed), ty = vi (pType); const bool nab = vi (pEq) == 0, humOn = vi (pHum) != 0;
        static const double bumpHz[3] { 60.0, 45.0, 32.0 }, bumpDb[3] { 2.6, 1.6, .6 }, lpHz[3] { 11000.0, 17000.0, 21000.0 }, wowMs[3] { .30, .12, .04 };
        bump.set (BQ::Peak, sr, bumpHz[sp], .8, bumpDb[sp] + (ty == 0 ? .6 : ty == 2 ? -.4 : 0.0)); lpS.set (BQ::LP, sr, std::min (lpHz[sp], sr * .45), .6, 0.0);
        lsT.set (BQ::LowShelf, sr, 110.0, .7, v (pLF)); hsT.set (BQ::HighShelf, sr, 8000.0, .7, v (pHF));
        eqL.set (BQ::LowShelf, sr, 90.0, .7, nab ? 1.2 : -.6); eqH.set (BQ::HighShelf, sr, 10000.0, .7, nab ? -.8 : 1.0);
        const float gDrive = db2lin (v (pDrive) * (ty == 1 ? 2.0f : ty == 2 ? 1.0f : 1.6f)), comp = 1.f / std::pow (gDrive, .7f), bias = v (pBias) * .035f, mix = v (pMix), outG = db2lin (v (pOut));
        const float recG[2] { db2lin ((v (pRecL) - 5.f) * 2.4f), db2lin ((v (pRecR) - 5.f) * 2.4f) }, repG[2] { db2lin ((v (pRepL) - 5.f) * 2.4f), db2lin ((v (pRepR) - 5.f) * 2.4f) };
        const double depth = wowMs[sp] * sr / 1000.0, base = depth + 3.0, dPh1 = 2.0 * juce::MathConstants<double>::pi * .7 / sr, dPh2 = 2.0 * juce::MathConstants<double>::pi * 6.5 / sr, dHum = 2.0 * juce::MathConstants<double>::pi * 60.0 / sr;
        const float ea = coefMs (300.f, sr); float pk[2] { 0.f, 0.f };
        auto shape = [ty] (float x) { return ty == 1 ? x / std::sqrt (1.f + x * x) * 1.08f : std::tanh (x); };
        const float off = shape (bias);
        for (int i = 0; i < n; ++i)
        {
            const double mod = std::sin (ph1) * .7 + std::sin (ph2) * .3; ph1 += dPh1; ph2 += dPh2; const float hz = humOn ? (float) (.0007 * (std::sin (hum) + .5 * std::sin (2 * hum) + .25 * std::sin (3 * hum))) : 0.f; hum += dHum;
            if (ph1 > 6.2831853) ph1 -= 6.2831853; if (ph2 > 6.2831853) ph2 -= 6.2831853; if (hum > 6.2831853) hum -= 6.2831853;
            const double dl = base + depth * mod; const int i0 = (int) dl; const float fr = (float) (dl - i0);
            for (int c = 0; c < 2; ++c)
            {
                if (c == 1 && nc < 2) { p[1][i] = p[0][i]; break; }
                const float in = p[c][i]; const float x = in * recG[c] * gDrive; float y = (shape (x + bias) - off) * comp;
                dcs[c] = .9995f * dcs[c] + .0005f * y; y -= dcs[c];
                y = bump.run (c, y); y = lsT.run (c, y); y = hsT.run (c, y); y = lpS.run (c, y); y = eqL.run (c, y); y = eqH.run (c, y); y *= repG[c];
                dly[c][w & 4095] = y; const float a = dly[c][(w - i0) & 4095], b = dly[c][(w - i0 - 1) & 4095]; const float wet = a + (b - a) * fr;
                dry[c][w & 255] = in; const float dd = dry[c][(w - (int) base - 1) & 255];
                float o = (dd * (1.f - mix) + wet * mix) * outG + hz; p[c][i] = o;
                const float ao = std::abs (o); env[c] = ao > env[c] ? ea * env[c] + (1.f - ea) * ao : ea * env[c] + (1.f - ea) * ao; pk[c] = std::max (pk[c], ao);
            }
            ++w;
        }
        meterA.store (lin2db (env[0] * 1.11f + 1e-9f)); meterB.store (lin2db (env[nc > 1 ? 1 : 0] * 1.11f + 1e-9f));
        pkL.store (std::max (pk[0], pkL.load() * .93f)); pkR.store (std::max (pk[nc > 1 ? 1 : 0], pkR.load() * .93f));
    }
    std::atomic<float> pkL { 0.f }, pkR { 0.f };
private:
    static inline const Spec S[] = {
        { "recl", "Grabación izquierda", 0.f, 10.f, 5.f, 0.f, fNum, "" }, { "repl", "Reproducción izquierda", 0.f, 10.f, 5.f, 0.f, fNum, "" }, { "recr", "Grabación derecha", 0.f, 10.f, 5.f, 0.f, fNum, "" }, { "repr", "Reproducción derecha", 0.f, 10.f, 5.f, 0.f, fNum, "" },
        { "drive", "Drive", 0.f, 10.f, 5.f, 0.f, fNum, "" }, { "bias", "Bias", -5.f, 5.f, 0.f, 0.f, fNum, "" }, { "hf", "Recorte de agudos", -6.f, 6.f, 0.f, 0.f, fDb, "" }, { "lf", "Recorte de graves", -6.f, 6.f, 0.f, 0.f, fDb, "" },
        { "mix", "Mezcla", 0.f, 1.f, 1.f, 0.f, fPct, "" }, { "out", "Salida", -12.f, 12.f, 0.f, 0.f, fDb, "" }, { "speed", "Velocidad", 0.f, 2.f, 1.f, 0.f, fChoice, "7.5 ips|15 ips|30 ips" }, { "type", "Tipo de cinta", 0.f, 2.f, 0.f, 0.f, fChoice, "A|B|C" },
        { "hum", "Zumbido", 0.f, 1.f, 0.f, 0.f, fChoice, "No|Sí" }, { "eq", "Curva", 0.f, 1.f, 0.f, 0.f, fChoice, "NAB|CCIR" } };
    double sr = 44100.0, ph1 = 0.0, ph2 = 0.0, hum = 0.0; BQ bump, lsT, hsT, lpS, eqL, eqH; float dly[2][4096] {}, dry[2][256] {}, env[2] {}, dcs[2] {}; int w = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjTapeSat)
};

class TapeEditor : public SkinEditor
{
public:
    TapeEditor (InjTapeSat& p) : SkinEditor (p, skinImage ("tp_bg_png"), .62f), tp (p)
    {
        const float cx[10] { 276.f, 391.f, 763.f, 879.5f, 118.2f, 288.6f, 447.f, 596.5f, 748.5f, 912.3f }, cy[10] { 181.6f, 181.8f, 182.f, 182.f, 1276.5f, 1276.6f, 1275.f, 1275.f, 1275.f, 1276.1f };
        const int prm[10] { InjTapeSat::pRepL, InjTapeSat::pRecL, InjTapeSat::pRepR, InjTapeSat::pRecR, InjTapeSat::pDrive, InjTapeSat::pBias, InjTapeSat::pHF, InjTapeSat::pLF, InjTapeSat::pMix, InjTapeSat::pOut };
        for (int k = 0; k < 10; ++k) { Knob kn { prm[k], cx[k], cy[k], k < 4 ? 34.f : 43.f, skinImage (("tp_cap" + juce::String (k) + "_png").toRawUTF8()) }; kn.a0 = -135.f; kn.a1 = 135.f; kn.rot0 = 0.f; knobs.push_back (kn); }
        reel[0] = skinImage ("tp_reelL_png"); reel[1] = skinImage ("tp_reelR_png"); lever = skinImage ("tp_lever_png"); led = skinImage ("tp_led_png");
        for (int k = 0; k < 3; ++k) { sOn[k] = skinImage (("tp_sp" + juce::String (k) + "_on_png").toRawUTF8()); sOff[k] = skinImage (("tp_sp" + juce::String (k) + "_off_png").toRawUTF8()); tOn[k] = skinImage (("tp_ty" + juce::String (k) + "_on_png").toRawUTF8()); tOff[k] = skinImage (("tp_ty" + juce::String (k) + "_off_png").toRawUTF8());
            hots.push_back ({ { 77.f + 58.5f * (float) k - 29.f, 935.f - 31.f, 58.f, 62.f }, [this, k] { choose (InjTapeSat::pSpeed, k); } });
            hots.push_back ({ { 909.f - 44.f, 882.f + 62.5f * (float) k - 26.f, 88.f, 52.f }, [this, k] { choose (InjTapeSat::pType, k); } }); }
        hots.push_back ({ { 380.f, 784.f, 66.f, 32.f }, [this] { choose (InjTapeSat::pHum, tp.vi (InjTapeSat::pHum) ? 0 : 1); } });
        hots.push_back ({ { 578.f, 784.f, 66.f, 32.f }, [this] { choose (InjTapeSat::pEq, tp.vi (InjTapeSat::pEq) ? 0 : 1); } });
        hots.push_back ({ { 400.f, 330.f, 240.f, 200.f }, [this] { presetMenu(); } });
        if (std::getenv ("INJ_PUNCHDEMO")) { tp.applyPreset (5); vu[0] = 1.5f; vu[1] = -4.f; demo = true; angle = .7f; }
        last = juce::Time::getMillisecondCounterHiRes();
    }
protected:
    void tick() override
    {
        const double now = juce::Time::getMillisecondCounterHiRes(), dt = juce::jlimit (0.0, .2, (now - last) / 1000.0); last = now;
        if (! demo)
        {
            const float tgt[2] { tp.meterA.load() + 18.f, tp.meterB.load() + 18.f };
            for (int c = 0; c < 2; ++c) vu[c] += (juce::jmax (-25.f, tgt[c]) - vu[c]) * (float) (1.0 - std::exp (-dt / .12));
            const bool run = tp.meterA.load() > -70.f || tp.meterB.load() > -70.f; static const float rate[3] { 1.4f, 2.8f, 5.6f };
            if (run) angle -= rate[tp.vi (InjTapeSat::pSpeed)] * (float) dt; hot[0] = tp.pkL.load() > .9f; hot[1] = tp.pkR.load() > .9f;
        }
        else { hot[0] = true; }
        repaint();
    }
    void paintSkin (juce::Graphics& g) override
    {
        const float cx[2] { 227.3f, 794.3f }, cyy[2] { 540.5f, 540.7f };
        for (int k = 0; k < 2; ++k) { const float h = reel[k].getWidth() * .5f; g.drawImageTransformed (reel[k], juce::AffineTransform::translation (-h, -h).rotated (angle).scaled (sc).translated (cx[k] * sc, cyy[k] * sc), false); }
        const int sp = tp.vi (InjTapeSat::pSpeed), ty = tp.vi (InjTapeSat::pType);
        for (int k = 0; k < 3; ++k) { drawSprite (g, sp == k ? sOn[k] : sOff[k], 77.f + 58.5f * (float) k, 935.f); drawSprite (g, ty == k ? tOn[k] : tOff[k], 909.f, 882.f + 62.5f * (float) k); }
        drawSprite (g, lever, tp.vi (InjTapeSat::pHum) ? 431.5f : 398.f, 802.f); drawSprite (g, lever, tp.vi (InjTapeSat::pEq) ? 627.5f : 595.f, 802.f); if (tp.vi (InjTapeSat::pHum)) drawLed (g, led, 0, 471.5f, 799.5f);
        // agujas VU (0 VU = -18 dBFS) y luces de sobrecarga
        static const float vv[10] { -20.f, -10.f, -7.f, -5.f, -3.f, -1.f, 0.f, 1.f, 2.f, 3.f }, aa[10] { -49.3f, -38.2f, -29.3f, -22.f, -12.2f, -.8f, 8.8f, 17.8f, 29.9f, 40.2f };
        for (int c = 0; c < 2; ++c)
        {
            const float x = juce::jlimit (-20.f, 3.f, vu[c]); float deg = aa[9]; for (int j = 0; j < 9; ++j) if (x <= vv[j + 1]) { deg = aa[j] + (aa[j + 1] - aa[j]) * (x - vv[j]) / (vv[j + 1] - vv[j]); break; }
            if (vu[c] < -20.f) deg = aa[0] - (juce::jmin (5.f, -20.f - vu[c]) * .6f);
            drawNeedle (g, 146.f + 481.f * (float) c, 226.5f, deg, 6.f, 68.f, { 58.f + 481.f * (float) c, 131.f, 152.f, 91.f }, juce::Colour (0xff17130f), 2.6f);
            if (hot[c]) { const float lx = 135.f + 482.5f * (float) c, ly = 258.5f; juce::ColourGradient gd (juce::Colour (0xffff4a28), lx * sc, ly * sc, juce::Colour (0x00ff4a28), (lx + 17.f) * sc, ly * sc, true); g.setGradientFill (gd); g.fillEllipse ((lx - 17.f) * sc, (ly - 17.f) * sc, 34.f * sc, 34.f * sc); }
        }
    }
private:
    void presetMenu()
    {
        juce::PopupMenu mn; auto ps = tp.presets(); mn.addItem (1, "Inicial"); mn.addSeparator();
        for (size_t i = 0; i < ps.size(); ++i) mn.addItem (100 + (int) i, juce::String::fromUTF8 (ps[i].name), true, tp.presetName() == juce::String::fromUTF8 (ps[i].name));
        juce::Component::SafePointer<TapeEditor> sp (this);
        mn.showMenuAsync (juce::PopupMenu::Options(), [sp] (int r) { if (sp == nullptr) return; if (r == 1) sp->tp.resetAll(); else if (r >= 100) sp->tp.applyPreset (r - 100); sp->repaint(); });
    }
    InjTapeSat& tp; juce::Image reel[2], lever, led, sOn[3], sOff[3], tOn[3], tOff[3]; float vu[2] { -25.f, -25.f }, angle = 0.f; bool hot[2] { false, false }, demo = false; double last = 0.0;
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjTapeSat::createEditor() { return std::make_unique<TapeEditor> (*this); }
} // namespace injfx
