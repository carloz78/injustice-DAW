#pragma once
#include "FxCore.h"
#include <complex>
// Utilidad: ganancia y fase, mid/side, analizador de espectro y medidor de sonoridad (LUFS).
namespace injfx
{
inline void drawToggle (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool on) { g.setColour (on ? theme::accent : theme::bgBar); g.fillRoundedRectangle (r, 8.f); label (g, t, r, juce::Justification::centred, 12.f, true, on ? juce::Colour (0xff1a1a1a) : theme::text); }

//==============================================================================
class InjGainPhase : public InjFx
{
public:
    enum { pGain, pPan, pPolL, pPolR, pSwap, pMono, pDelay };
    INJ_FX_BODY (InjGainPhase, "injgain", "Ganancia y fase", "Ganancia", 7)
    Layout layout() const override { Layout L; L.visualH = 112; L.knobs = { pGain, pPan, pDelay }; L.perRow = 3; L.dia = 62.f; L.icon = "M12 3v18M3 12h18M7 7l10 10"; return L; }
    std::vector<Preset> presets() const override { return { { "Neutro", { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f } }, { "Invertir fase", { 0.f, 0.f, 1.f, 1.f, 0.f, 0.f, 0.f } }, { "Comprobar en mono", { 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f } }, { "Cambiar lados", { 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f } } }; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; for (auto& d : dl) { d.prep ((int) (.02 * sr)); d.clear(); } pk[0] = pk[1] = 0.f; sL.prep (sr, 15.f); sR.prep (sr, 15.f); }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.destBuffer->getNumChannels() < 2 || dl[0].b.empty()) return; float* L = fc.destBuffer->getWritePointer (0, fc.bufferStartSample); float* R = fc.destBuffer->getWritePointer (1, fc.bufferStartSample);
        const float g = db2lin (v (pGain)), pan = v (pPan), gl = std::min (1.f, 1.f - pan) * g, gr = std::min (1.f, 1.f + pan) * g, sl = v (pPolL) > .5f ? -1.f : 1.f, srr = v (pPolR) > .5f ? -1.f : 1.f; const bool sw = v (pSwap) > .5f, mo = v (pMono) > .5f;
        const float dms = v (pDelay), ds = std::abs (dms) * .001f * (float) sr; const bool dR = dms > 0.f; float mL = 0.f, mR = 0.f;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            dl[0].push (L[i]); dl[1].push (R[i]); float a = ds > .5f && ! dR ? dl[0].read (ds) : L[i], b = ds > .5f && dR ? dl[1].read (ds) : R[i];
            if (sw) std::swap (a, b); a *= sl; b *= srr; if (mo) { a = b = .5f * (a + b); } a *= sL.next (gl); b *= sR.next (gr); L[i] = a; R[i] = b; mL = std::max (mL, std::abs (a)); mR = std::max (mR, std::abs (b));
        }
        pk[0] = std::max (mL, pk[0] * .9f); pk[1] = std::max (mR, pk[1] * .9f); meterA.store (pk[0]); meterB.store (pk[1]);
    }
    void visualClickAt (juce::Point<float> pt, juce::Rectangle<float> r) override { const int k = (int) ((pt.x - r.getX() - 8.f) / ((r.getWidth() - 16.f) / 4.f)); if (pt.y - r.getY() > 52.f || k < 0 || k > 3) return; const int ids[4] { pPolL, pPolR, pSwap, pMono }; setValue (ids[k], v (ids[k]) > .5f ? 0.f : 1.f); }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        const char* nm[4] { "Invertir izq.", "Invertir der.", "Cambiar lados", "Mono" }; const int ids[4] { pPolL, pPolR, pSwap, pMono }; const float w = (r.getWidth() - 16.f) / 4.f;
        for (int k = 0; k < 4; ++k) drawToggle (g, { r.getX() + 8.f + k * w + 3.f, r.getY() + 10.f, w - 6.f, 36.f }, juce::String::fromUTF8 (nm[k]), v (ids[k]) > .5f);
        const float m[2] { meterA.load(), meterB.load() }; for (int c = 0; c < 2; ++c) { const auto b = juce::Rectangle<float> (r.getX() + 36.f, r.getY() + 62.f + c * 20.f, r.getWidth() - 56.f, 10.f); barH (g, b, (lin2db (m[c] + 1e-6f) + 60.f) / 60.f, theme::accent); label (g, c == 0 ? "I" : "D", { r.getX() + 12.f, b.getY() - 3.f, 20.f, 14.f }); }
    }
private:
    static inline const Spec S[] = { { "gain", "Ganancia", -24.f, 24.f, 0.f, 0.f, fDb, "" }, { "pan", "Balance", -1.f, 1.f, 0.f, 0.f, fPct, "" }, { "pl", "Fase izq.", 0.f, 1.f, 0.f, 0.f, fChoice, "Normal|Invertida" }, { "pr", "Fase der.", 0.f, 1.f, 0.f, 0.f, fChoice, "Normal|Invertida" },
        { "swap", "Cambiar lados", 0.f, 1.f, 0.f, 0.f, fChoice, "No|Sí" }, { "mono", "Mono", 0.f, 1.f, 0.f, 0.f, fChoice, "No|Sí" }, { "delay", "Retardo der./izq.|Retardo", -5.f, 5.f, 0.f, 0.f, fMs, "" } };
    double sr = 44100.0; Delay dl[2]; float pk[2] {}; Smooth sL, sR;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjGainPhase)
};

//==============================================================================
class InjMidSide : public InjFx
{
public:
    enum { pMid, pSide, pHp, pMidTone, pSideTone };
    INJ_FX_BODY (InjMidSide, "injmidside", "Mid/Side", "M/S", 5)
    Layout layout() const override { Layout L; L.visualH = 84; L.knobs = { pMid, pSide, pHp, pMidTone, pSideTone }; L.dia = 54.f; L.icon = "M12 3v18M3 8h6M15 16h6"; return L; }
    std::vector<Preset> presets() const override { return { { "Neutro", { 0.f, 0.f, 20.f, 0.f, 0.f } }, { "Ensanchar con aire", { 0.f, 3.f, 150.f, 0.f, 3.f } }, { "Graves en el centro", { 0.f, 0.f, 200.f, 0.f, 0.f } }, { "Más estrecho", { 1.f, -4.f, 120.f, 0.f, 0.f } } }; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; for (auto& b : f) b.reset(); lastHp = -1.f; lastMt = lastSt = -99.f; pk[0] = pk[1] = 0.f; }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.destBuffer->getNumChannels() < 2) return; float* L = fc.destBuffer->getWritePointer (0, fc.bufferStartSample); float* R = fc.destBuffer->getWritePointer (1, fc.bufferStartSample);
        const float gm = db2lin (v (pMid)), gs = db2lin (v (pSide)), hp = v (pHp), mt = v (pMidTone), st = v (pSideTone);
        if (hp != lastHp) { f[0].set (BQ::HP, sr, hp, .7071); f[1].set (BQ::HP, sr, hp, .7071); lastHp = hp; } if (mt != lastMt) { f[2].set (BQ::HighShelf, sr, 5000.0, .7, mt); lastMt = mt; } if (st != lastSt) { f[3].set (BQ::HighShelf, sr, 5000.0, .7, st); lastSt = st; }
        const bool useHp = hp > 21.f; float mM = 0.f, mS = 0.f;
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            float m = .5f * (L[i] + R[i]), s = .5f * (L[i] - R[i]); if (useHp) s = f[1].run (0, f[0].run (0, s)); if (std::abs (mt) > .01f) m = f[2].run (0, m); if (std::abs (st) > .01f) s = f[3].run (0, s); m *= gm; s *= gs;
            L[i] = m + s; R[i] = m - s; mM = std::max (mM, std::abs (m)); mS = std::max (mS, std::abs (s));
        }
        pk[0] = std::max (mM, pk[0] * .9f); pk[1] = std::max (mS, pk[1] * .9f); meterA.store (pk[0]); meterB.store (pk[1]);
    }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        const char* nm[2] { "Centro (mid)", "Lados (side)" }; const float m[2] { meterA.load(), meterB.load() };
        for (int c = 0; c < 2; ++c) { const auto b = juce::Rectangle<float> (r.getX() + 110.f, r.getY() + 18.f + c * 28.f, r.getWidth() - 130.f, 14.f); barH (g, b, (lin2db (m[c] + 1e-6f) + 60.f) / 60.f, c == 0 ? theme::trackColours[0] : theme::accent); label (g, juce::String::fromUTF8 (nm[c]), { r.getX() + 12.f, b.getY() - 1.f, 96.f, 16.f }, juce::Justification::left, 12.f, true, theme::text); }
    }
private:
    static inline const Spec S[] = { { "mid", "Centro", -12.f, 12.f, 0.f, 0.f, fDb, "" }, { "side", "Lados", -12.f, 12.f, 0.f, 0.f, fDb, "" }, { "hp", "Lados sin graves|Corte lados", 20.f, 600.f, 20.f, 120.f, fHz, "" },
        { "mt", "Brillo del centro|Brillo C", -6.f, 6.f, 0.f, 0.f, fDb, "" }, { "st", "Brillo de los lados|Brillo L", -6.f, 6.f, 0.f, 0.f, fDb, "" } };
    double sr = 44100.0; BQ f[4]; float lastHp = -1.f, lastMt = -99.f, lastSt = -99.f, pk[2] {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjMidSide)
};

//==============================================================================
// Analizador de espectro: no cambia el sonido, solo lo muestra (FFT de 2048 puntos, escala logarítmica).
class InjSpectrum : public InjFx
{
public:
    enum { pSlope, pSmooth, pFloor };
    static constexpr int N = 2048;
    INJ_FX_BODY (InjSpectrum, "injspectrum", "Analizador de espectro", "Espectro", 3)
    Layout layout() const override { Layout L; L.visualH = 250; L.knobs = { pSlope, pSmooth, pFloor }; L.perRow = 3; L.dia = 54.f; L.icon = "M3 20V12M8 20V6M13 20V10M18 20V4"; return L; }
    std::vector<Preset> presets() const override { return { { "Como un analizador pro", { 4.5f, .6f, -90.f } }, { "Plano", { 0.f, .5f, -90.f } }, { "Rápido", { 3.f, .2f, -80.f } }, { "Detalle de graves", { 4.5f, .8f, -110.f } } }; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; for (auto& x : ring) x = 0.f; wp = 0; for (auto& x : sm) x = -120.f; }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return; const int nc = fc.destBuffer->getNumChannels(); const float* a = fc.destBuffer->getReadPointer (0, fc.bufferStartSample); const float* b = nc > 1 ? fc.destBuffer->getReadPointer (1, fc.bufferStartSample) : a; int w = wp.load();
        for (int i = 0; i < fc.bufferNumSamples; ++i) { ring[(size_t) (w & (2 * N - 1))] = .5f * (a[i] + b[i]); ++w; } wp.store (w);
    }
    void analyse (std::vector<float>& outDb) const { ringSpectrum (ring, 2 * N - 1, wp.load(), outDb); }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        std::vector<float> db; analyse (db); const float slope = v (pSlope), flo = v (pFloor), smo = juce::jlimit (0.f, .97f, v (pSmooth) * .95f); const float binHz = (float) sr / (float) N;
        const auto plot = r.reduced (12.f, 14.f).withTrimmedBottom (14.f); for (float f : { 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f }) { const float x = plot.getX() + std::log10 (f / 20.f) / 3.f * plot.getWidth(); g.setColour (juce::Colour (0xff2a2b30)); g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom()); label (g, f >= 1000.f ? juce::String (f / 1000.f, 0) + "k" : juce::String ((int) f), { x - 16.f, plot.getBottom() + 1.f, 32.f, 12.f }, juce::Justification::centred, 9.f); }
        for (float d = 0.f; d > flo + 1.f; d -= 20.f) { const float y = plot.getY() + (d / flo) * plot.getHeight(); g.setColour (juce::Colour (0xff2a2b30)); g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight()); label (g, juce::String ((int) d), { plot.getX() + 2.f, y + 1.f, 30.f, 12.f }, juce::Justification::left, 9.f); }
        juce::Path p; float peakF = 0.f, peakV = -999.f; const int W = (int) plot.getWidth();
        for (int x = 0; x <= W; ++x)
        {
            const float f0 = 20.f * std::pow (1000.f, (float) x / (float) W), f1 = 20.f * std::pow (1000.f, (float) (x + 1) / (float) W); const int k0 = std::max (1, (int) (f0 / binHz)), k1 = std::min (N / 2 - 1, std::max (k0, (int) (f1 / binHz)));
            float m = -140.f; for (int k = k0; k <= k1; ++k) m = std::max (m, db[(size_t) k]); m += slope * std::log2 (std::max (20.f, (f0 + f1) * .5f) / 1000.f);
            float& s = sm[(size_t) std::min (x, 1023)]; s = m > s ? s * smo * .5f + m * (1.f - smo * .5f) : s * smo + m * (1.f - smo); if (s > peakV) { peakV = s; peakF = f0; }
            const float y = plot.getY() + juce::jlimit (0.f, 1.f, s / flo) * plot.getHeight(); if (x == 0) { p.startNewSubPath (plot.getX(), plot.getBottom()); p.lineTo (plot.getX(), y); } else p.lineTo (plot.getX() + x, y);
        }
        p.lineTo (plot.getRight(), plot.getBottom()); p.closeSubPath(); g.setColour (theme::accent.withAlpha (.28f)); g.fillPath (p); g.setColour (theme::accent); g.strokePath (p, juce::PathStrokeType (1.6f));
        if (peakV > flo + 6.f) label (g, "Pico  " + fmtValue ({ "", "", 0, 0, 0, 0, fHz, "" }, peakF), { plot.getRight() - 130.f, plot.getY() + 2.f, 126.f, 14.f }, juce::Justification::right, 11.f, true, theme::text);
    }
private:
    static inline const Spec S[] = { { "slope", "Pendiente", 0.f, 6.f, 4.5f, 0.f, fOct, "" }, { "smooth", "Suavizado", 0.f, 1.f, .6f, 0.f, fPct, "" }, { "floor", "Rango", -120.f, -60.f, -90.f, 0.f, fDb, "" } };
    double sr = 44100.0; float ring[2 * N] {}; std::atomic<int> wp { 0 }; float sm[1024] {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjSpectrum)
};

//==============================================================================
// Medidor de sonoridad (LUFS, ITU BS.1770 / EBU R128): momentáneo, de 3 s e integrado con puertas. Haz clic en la zona visual para reiniciar.
class InjLufs : public InjFx
{
public:
    enum { pTarget };
    INJ_FX_BODY (InjLufs, "injlufs", "Medidor LUFS", "LUFS", 1)
    Layout layout() const override { Layout L; L.visualH = 170; L.knobs = { pTarget }; L.perRow = 1; L.dia = 58.f; L.icon = "M4 18V6M9 18V10M14 18V4M19 18V12"; return L; }
    std::vector<Preset> presets() const override { return { { "Streaming (-14)", { -14.f } }, { "Radio / TV (-23)", { -23.f } }, { "Club (-9)", { -9.f } }, { "Gabber a tope (-7)", { -7.f } } }; }
    void initialise (const te::PluginInitialisationInfo& i) override { sr = i.sampleRate; setup(); doReset(); }
    void setup() { for (int c = 0; c < 2; ++c) { kb[c][0].set (BQ::HighShelf, sr, 1681.97, .7072, 3.9998); kb[c][1].set (BQ::HP, sr, 38.1355, .5003); } blockN = std::max (1, (int) (.1 * sr)); gated.reserve (40001); blocks.reserve (32); }
    void doReset() { for (auto& c : kb) for (auto& b : c) b.reset(); acc = 0.0; cnt = 0; blocks.clear(); gated.clear(); mom = sho = integ = -120.f; peak = 0.f; resetReq = false; }
    void visualClick() override { resetReq = true; }
    float integrated() const { return iA.load(); }
    float momentary() const { return mA.load(); }
    static float toL (double ms) { return ms > 1e-12 ? (float) (-.691 + 10.0 * std::log10 (ms)) : -120.f; }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return; if (resetReq.load()) doReset(); const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        const float* p[2] { fc.destBuffer->getReadPointer (0, fc.bufferStartSample), nc > 1 ? fc.destBuffer->getReadPointer (1, fc.bufferStartSample) : fc.destBuffer->getReadPointer (0, fc.bufferStartSample) };
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            for (int c = 0; c < 2; ++c) { const float x = p[c][i]; peak = std::max (peak, std::abs (x)); const float y = kb[c][1].run (0, kb[c][0].run (0, x)); acc += (double) y * y; }
            if (++cnt >= blockN)
            {
                blocks.push_back (acc / (double) cnt); if (blocks.size() > 30) blocks.erase (blocks.begin()); acc = 0.0; cnt = 0;
                auto meanLast = [&] (size_t n) { n = std::min (n, blocks.size()); double s = 0; for (size_t k = blocks.size() - n; k < blocks.size(); ++k) s += blocks[k]; return s / (double) n; };
                mom = toL (meanLast (4)); sho = toL (meanLast (30));
                if (blocks.size() >= 4) { const double m4 = meanLast (4); if (toL (m4) > -70.f && gated.size() < 40000) gated.push_back (m4); }
                if (! gated.empty()) { double s = 0; for (double q : gated) s += q; const float gam = toL (s / (double) gated.size()) - 10.f; double s2 = 0; int n2 = 0; for (double q : gated) if (toL (q) > gam) { s2 += q; ++n2; } if (n2 > 0) integ = toL (s2 / n2); }
                mA.store (mom); sA.store (sho); iA.store (integ); pA.store (lin2db (peak + 1e-9f));
            }
        }
    }
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> r) override
    {
        const float im = iA.load(), mo = mA.load(), sh = sA.load(), tg = v (pTarget), pk = pA.load();
        label (g, im > -100.f ? juce::String (im, 1) : juce::String::fromUTF8 ("\xe2\x80\x94"), { r.getX() + 14.f, r.getY() + 8.f, 200.f, 56.f }, juce::Justification::left, 44.f, true, im > -100.f && im > tg + 1.f ? juce::Colour (0xffff7a70) : theme::text);
        label (g, juce::String::fromUTF8 ("LUFS integrado"), { r.getX() + 16.f, r.getY() + 62.f, 200.f, 14.f }, juce::Justification::left, 11.f);
        if (im > -100.f) label (g, juce::String (im - tg > 0 ? "+" : "") + juce::String (im - tg, 1) + " LU vs objetivo", { r.getX() + 16.f, r.getY() + 78.f, 220.f, 14.f }, juce::Justification::left, 11.f, true, theme::text);
        label (g, pk > -100.f ? "Pico " + juce::String (pk, 1) + " dB" : juce::String ("Pico  ") + juce::String::fromUTF8 ("\xe2\x80\x94"), { r.getX() + 16.f, r.getY() + 96.f, 200.f, 14.f }, juce::Justification::left, 11.f);
        const float bx = r.getX() + 250.f, bw = r.getWidth() - 270.f; const float vals[2] { mo, sh }; const char* nm[2] { "Moment\xc3\xa1neo", "3 segundos" };
        for (int k = 0; k < 2; ++k) { const auto b = juce::Rectangle<float> (bx, r.getY() + 30.f + k * 44.f, bw, 14.f); barH (g, b, (vals[k] + 40.f) / 40.f, theme::accent); label (g, juce::String::fromUTF8 (nm[k]) + "  " + (vals[k] > -100.f ? juce::String (vals[k], 1) : juce::String::fromUTF8 ("\xe2\x80\x94")), { b.getX(), b.getY() - 16.f, bw, 14.f }, juce::Justification::left, 11.f, true, theme::text); }
        const float tx = bx + (tg + 40.f) / 40.f * bw; g.setColour (theme::text); g.drawVerticalLine ((int) tx, r.getY() + 20.f, r.getY() + 100.f);
        label (g, "Clic para reiniciar", { r.getX(), r.getBottom() - 20.f, r.getWidth() - 12.f, 14.f }, juce::Justification::right, 10.f);
    }
private:
    static inline const Spec S[] = { { "target", "Objetivo", -30.f, -6.f, -14.f, 0.f, fLufs, "" } };
    double sr = 44100.0; BQ kb[2][2]; double acc = 0.0; int cnt = 0, blockN = 4410; std::vector<double> blocks, gated; float mom = -120.f, sho = -120.f, integ = -120.f, peak = 0.f; std::atomic<bool> resetReq { false }; std::atomic<float> mA { -120.f }, sA { -120.f }, iA { -120.f }, pA { -120.f };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjLufs)
};
} // namespace injfx
