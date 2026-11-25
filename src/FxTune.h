#pragma once
#include "FxMod3.h"
#include "FxVoice.h"
// INJUSTICE TUNE: corrección de afinación en tiempo real (modo automático). Detecta el tono (YIN), lo lleva a la nota más cercana de la escala
// elegida con velocidad / humanización / tolerancia / vibrato y lo corrige con el cambiador de tono de fase manteniendo los formantes.
namespace injfx
{
inline std::function<void()> openPitchEditor;   // la aplicación lo conecta al editor de afinación manual (Injustice Auto)
class InjTunePro : public InjFx
{
public:
    enum { pRef, pKey, pScale, pSpeed, pHuman, pTol, pVib, pMix, pForm, pBypass, pMask, pSlot };
    static inline const char* xmlTypeName = "injtunepro";
    static const char* getPluginName() { return "Injustice Tune"; }
    InjTunePro (te::PluginCreationInfo i) : InjFx (i) { buildParams(); noteOut.store (-1); }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Tune"; }
    const Spec* specs() const override { return S; }
    int numSpecs() const override { return 12; }
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;
    std::vector<Preset> presets() const override
    {   // ref, key, scale, speed, humanize, tolerance, vibrato, mix, formant
        return { { "Natural", { 440.f, 0.f, 0.f, 45.f, 60.f, 20.f, 0.f, 1.f, 0.f } }, { "Pop vocal (major)", { 440.f, 0.f, 1.f, 20.f, 10.f, 0.f, 0.f, 1.f, 0.f } },
                 { "Hard tune (T-Pain)", { 440.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f } }, { "Subtle fix", { 440.f, 0.f, 0.f, 60.f, 70.f, 35.f, 0.f, 1.f, 0.f } },
                 { "Smooth minor", { 440.f, 0.f, 2.f, 30.f, 30.f, 10.f, 10.f, 1.f, 0.f } }, { "Robot", { 440.f, 0.f, 5.f, 0.f, 0.f, 0.f, 30.f, 1.f, -3.f } }, { "Bright formant", { 440.f, 0.f, 0.f, 20.f, 10.f, 0.f, 0.f, 1.f, 5.f } } };
    }
    double getLatencySeconds() override { return (double) PVShift::LAT / sr; }
    void initialise (const te::PluginInitialisationInfo& i) override
    {
        sr = i.sampleRate; for (auto& c : core) { c.reset (sr); c.keepFormants = true; } for (auto& d : dry) { d.prep (PVShift::LAT + 8); d.clear(); }
        ring.assign (4096, 0.f); rp = 0; sinceHop = 0; semi = 0.f; f0 = 0.f; hold = 0; held = 0.f; lastNote = -999; vibPh = 0.f; ratio = 1.f; noteOut.store (-1); cIn.store (0.f); cOut.store (0.f);
    }
    // escalas (relativas a la tónica): bit = grado cromático
    static int scaleMask (int sc)
    {
        static const int deg[12][8] { { 0, 1, 2, 3, 4, 5, 6, 7 }, { 0, 2, 4, 5, 7, 9, 11, -1 }, { 0, 2, 3, 5, 7, 8, 10, -1 }, { 0, 2, 3, 5, 7, 8, 11, -1 }, { 0, 2, 4, 7, 9, -1, -1, -1 }, { 0, 3, 5, 7, 10, -1, -1, -1 },
                                      { 0, 2, 3, 5, 7, 9, 10, -1 }, { 0, 1, 3, 5, 7, 8, 10, -1 }, { 0, 2, 4, 5, 7, 9, 10, -1 }, { 0, 3, 5, 6, 7, 10, -1, -1 }, { 0, 2, 4, 6, 8, 10, -1, -1 }, { 0, 1, 2, 3, 4, 5, 6, 7 } };
        if (sc <= 0 || sc >= 11) return 0xFFF; int m = 0; for (int k = 0; k < 8; ++k) if (deg[sc][k] >= 0) m |= 1 << deg[sc][k]; return m;
    }
    // notas permitidas (bit = clase de nota absoluta, 0 = Do)
    int noteMask() const
    {
        const int sc = vi (pScale); if (sc >= 11) { const int m = vi (pMask) & 0xFFF; return m != 0 ? m : 0xFFF; }
        const int rel = scaleMask (sc), key = vi (pKey); int m = 0; for (int d = 0; d < 12; ++d) if (rel & (1 << d)) m |= 1 << ((d + key) % 12); return m;
    }
    static float nearestNote (float midi, int mask)
    {
        float best = midi, bd = 1e9f; const int c0 = (int) std::round (midi);
        for (int c = c0 - 7; c <= c0 + 7; ++c) if (mask & (1 << (((c % 12) + 12) % 12))) { const float d = std::abs ((float) c - midi); if (d < bd) { bd = d; best = (float) c; } }
        return best;
    }
    // A/B: instantáneas de los ajustes (sin contar el bypass) que se guardan con el proyecto
    juce::String snapshot() const { juce::String s; for (int i = 0; i <= pMask; ++i) if (i != pBypass) s << juce::String (v (i), 4) << ","; return s; }
    void restore (const juce::String& s) { auto t = juce::StringArray::fromTokens (s, ",", ""); int k = 0; for (int i = 0; i <= pMask; ++i) { if (i == pBypass) continue; if (k < t.size()) setValue (i, t[k].getFloatValue()); ++k; } }
    void switchSlot (int n)
    {
        n = n ? 1 : 0; const int cur = vi (pSlot); if (n == cur) return; state.setProperty (cur ? "tnB" : "tnA", snapshot(), nullptr);
        const auto other = state.getProperty (n ? "tnB" : "tnA").toString(); if (other.isNotEmpty()) restore (other); setValue (pSlot, (float) n);
    }
    void copyToOther() { state.setProperty (vi (pSlot) ? "tnA" : "tnB", snapshot(), nullptr); }
    // YIN sobre la señal mono diezmada a la mitad. Devuelve f0 en Hz o 0 si no hay tono claro.
    float detect()
    {
        const int Wn = 512, tmax = 330, need = (Wn + tmax) * 2; static thread_local std::vector<float> x, dd; x.assign ((size_t) (Wn + tmax), 0.f); dd.assign ((size_t) tmax, 0.f);
        for (int j = 0; j < Wn + tmax; ++j) { const int a = rp - need + j * 2; x[(size_t) j] = .5f * (ring[(size_t) (a & 4095)] + ring[(size_t) ((a + 1) & 4095)]); }
        for (int t = 1; t < tmax; ++t) { float s = 0.f; for (int j = 0; j < Wn; ++j) { const float d = x[(size_t) j] - x[(size_t) (j + t)]; s += d * d; } dd[(size_t) t] = s; }
        float run = 0.f; dd[0] = 1.f; for (int t = 1; t < tmax; ++t) { run += dd[(size_t) t]; dd[(size_t) t] = run > 1e-9f ? dd[(size_t) t] * (float) t / run : 1.f; }
        int tau = -1; for (int t = 24; t < tmax - 1; ++t) if (dd[(size_t) t] < .15f) { while (t + 1 < tmax - 1 && dd[(size_t) (t + 1)] < dd[(size_t) t]) ++t; tau = t; break; }
        if (tau < 0) return 0.f; const float a = dd[(size_t) tau - 1], b = dd[(size_t) tau], c = dd[(size_t) tau + 1]; const float den = a - 2.f * b + c; const float off = std::abs (den) > 1e-9f ? .5f * (a - c) / den : 0.f;
        return (float) (sr * .5) / ((float) tau + off);
    }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || ring.empty()) return; const int nc = std::min (2, fc.destBuffer->getNumChannels()); if (nc < 1) return;
        float* p[2] { fc.destBuffer->getWritePointer (0, fc.bufferStartSample), nc > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : nullptr };
        const float ref = v (pRef), speed = v (pSpeed) / 100.f, hum = v (pHuman) / 100.f, tol = v (pTol) / 100.f * .5f, vibD = v (pVib) / 100.f * .6f, mix = v (pMix), form = v (pForm); const bool byp = vi (pBypass) != 0;
        const int mask = noteMask(), hop = 256; const float hopSec = (float) hop / (float) sr, tauMs = speed * std::sqrt (speed) * 500.f;
        for (auto& c : core) { c.keepFormants = true; c.formantRatio = std::pow (2.f, form / 12.f); }
        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            ring[(size_t) (rp & 4095)] = nc > 1 ? .5f * (p[0][i] + p[1][i]) : p[0][i]; ++rp;
            if (++sinceHop >= hop)
            {
                sinceHop = 0; const float f = detect(); float vib = 0.f;
                if (f > 70.f && f < 900.f)
                {
                    f0 = f; hold = 0; const float m = 69.f + 12.f * std::log2 (f / ref), tg = nearestNote (m, mask), dev = tg - m;
                    if (juce::roundToInt (tg) == lastNote) held += hopSec; else { held = 0.f; lastNote = juce::roundToInt (tg); }
                    const float corr = dev - juce::jlimit (-tol, tol, dev), hf = juce::jmin (1.f, held / .4f), tau = tauMs + hum * hf * 150.f, a = tau <= .5f ? 1.f : 1.f - std::exp (-hopSec * 1000.f / tau);
                    semi += (corr - semi) * a; vib = vibD * juce::jmin (1.f, held / .25f);
                    cIn.store (juce::jlimit (-100.f, 100.f, -dev * 100.f)); cOut.store (juce::jlimit (-100.f, 100.f, (m + semi - tg) * 100.f)); noteOut.store (lastNote);
                }
                else if (++hold > 6) { semi += (0.f - semi) * .2f; f0 = 0.f; held = 0.f; lastNote = -999; noteOut.store (-1); cIn.store (0.f); cOut.store (0.f); }
                vibPh += 6.2831853f * 5.5f * hopSec; if (vibPh > 6.2831853f) vibPh -= 6.2831853f;
                const float tot = semi + vib * std::sin (vibPh); ratio = std::pow (2.f, tot / 12.f);
                meterA.store (f0); meterC.store (tot);
            }
            for (int c = 0; c < nc; ++c) { const float x = p[c][i]; dry[c].push (x); const float d = dry[c].at (PVShift::LAT - 1), y = core[c].run (x, ratio); p[c][i] = byp ? d : d + (y - d) * mix; }
        }
    }
    std::atomic<float> cIn { 0.f }, cOut { 0.f }; std::atomic<int> noteOut { -1 };   // desviación de la entrada y de la salida respecto a la nota destino (centésimas) y nota MIDI destino
private:
    static inline const Spec S[] = {
        { "ref", "Reference", 415.f, 466.f, 440.f, 0.f, fNum, "" }, { "key", "Key", 0.f, 11.f, 0.f, 0.f, fNote, "" },
        { "scale", "Scale", 0.f, 11.f, 0.f, 0.f, fChoice, "Chromatic|Major|Minor|Harmonic Minor|Pentatonic Major|Pentatonic Minor|Dorian|Phrygian|Mixolydian|Blues|Whole Tone|Custom" },
        { "speed", "Retune Speed", 0.f, 100.f, 20.f, 0.f, fInt, "" }, { "human", "Humanize", 0.f, 100.f, 10.f, 0.f, fInt, "" }, { "tol", "Note Tolerance", 0.f, 100.f, 0.f, 0.f, fInt, "" },
        { "vib", "Vibrato", 0.f, 100.f, 0.f, 0.f, fInt, "" }, { "mix", "Mix", 0.f, 1.f, 1.f, 0.f, fPct, "" }, { "form", "Formant", -12.f, 12.f, 0.f, 0.f, fSemi, "" },
        { "bypass", "Bypass", 0.f, 1.f, 0.f, 0.f, fChoice, "Off|On" }, { "mask", "Custom notes", 0.f, 4095.f, 1453.f, 0.f, fInt, "" }, { "slot", "A/B", 0.f, 1.f, 0.f, 0.f, fChoice, "A|B" } };
    double sr = 44100.0; PVShift core[2]; Delay dry[2]; std::vector<float> ring; int rp = 0, sinceHop = 0, hold = 0, lastNote = -999; float semi = 0.f, f0 = 0.f, held = 0.f, vibPh = 0.f, ratio = 1.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InjTunePro)
};

// ------------------------------------------------------------------ ventana
class TuneEditor : public SkinEditor
{
public:
    TuneEditor (InjTunePro& p) : SkinEditor (p, skinImage ("tn_bg_png"), .55f), t (p)
    {
        using P = InjTunePro; cap = skinImage ("tn_cap_png"); btnOn = skinImage ("tn_btn_on_png"); bypOn = skinImage ("tn_byp_on_png");
        struct K { int idx; float cx, cy, r; };
        for (const K& k : { K { P::pSpeed, 231.f, 572.f, 62.f }, K { P::pHuman, 558.f, 573.f, 62.f }, K { P::pTol, 882.f, 571.f, 62.f }, K { P::pVib, 1208.f, 572.f, 62.f }, K { P::pMix, 1542.f, 572.f, 62.f }, K { P::pForm, 1542.f, 350.f, 56.f } })
        { Knob kn { k.idx, k.cx, k.cy, k.r, cap }; kn.a0 = -135.f; kn.a1 = 135.f; knobs.push_back (kn); }
        hots.push_back ({ { 338.f, 160.f, 36.f, 26.f }, [this] { t.setValue (P::pRef, std::round ((t.v (P::pRef) + .1f) * 10.f) / 10.f); } });
        hots.push_back ({ { 338.f, 186.f, 36.f, 26.f }, [this] { t.setValue (P::pRef, std::round ((t.v (P::pRef) - .1f) * 10.f) / 10.f); } });
        hots.push_back ({ refBox, [this] { refMenu(); } }); hots.push_back ({ { 92.f, 262.f, 288.f, 50.f }, [this] { keyMenu(); } }); hots.push_back ({ { 92.f, 364.f, 288.f, 50.f }, [this] { scaleMenu(); } });
        hots.push_back ({ { 1339.f, 27.f, 69.f, 49.f }, [this] { t.switchSlot (0); } }); hots.push_back ({ { 1413.f, 27.f, 66.f, 49.f }, [this] { t.switchSlot (1); } });
        hots.push_back ({ { 1486.f, 27.f, 90.f, 49.f }, [this] { t.copyToOther(); flash = 8; } }); hots.push_back ({ { 1610.f, 27.f, 103.f, 49.f }, [this] { choose (P::pBypass, t.vi (P::pBypass) ? 0 : 1); } });
        hots.push_back ({ { 640.f, 4.f, 500.f, 62.f }, [this] { presetPopup (t, this); } });
        hots.push_back ({ { 1548.f, 170.f, 168.f, 50.f }, [] { if (openPitchEditor) openPitchEditor(); } });   // GRAPH: abre el editor manual
        hots.push_back ({ { 40.f, 742.f, 66.f, 124.f }, [this] { base = juce::jmax (12, base - 12); } }); hots.push_back ({ { 1676.f, 742.f, 62.f, 124.f }, [this] { base = juce::jmin (72, base + 12); } });
        for (int j = 0; j < 22; ++j) { const int pc = bpc[j % 5]; hots.push_back ({ { BC[j] - 15.f, 741.f, 30.f, 82.f }, [this, pc] { toggleNote (pc); } }); }
        for (int i = 0; i < 31; ++i) { const int pc = wpc[i % 7]; hots.push_back ({ { WE[i], 741.f, WE[i + 1] - WE[i], 127.f }, [this, pc] { toggleNote (pc); } }); }
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const auto pt = e.position / sc + org;
        if (refBox.contains (pt)) { t.setValue (InjTunePro::pRef, std::round ((t.v (InjTunePro::pRef) + (w.deltaY > 0 ? .1f : -.1f) * (e.mods.isShiftDown() ? 1.f : 1.f)) * 10.f) / 10.f); repaint(); return; }
        SkinEditor::mouseWheelMove (e, w);
    }
protected:
    void tick() override
    {
        const int n = t.noteOut.load(); if (n >= 0) { dIn += (t.cIn.load() - dIn) * .35f; dOut += (t.cOut.load() - dOut) * .35f; lastNoteShown = n; } else { dIn *= .8f; dOut *= .8f; }
        if (flash > 0) --flash; repaint();
    }
    void paintSkin (juce::Graphics& g) override
    {
        using P = InjTunePro; const auto col = [] (int r, int gg, int b, float a = 1.f) { return juce::Colour ((juce::uint8) r, (juce::uint8) gg, (juce::uint8) b, a); };
        const auto ink = col (28, 26, 36), purple = col (168, 44, 245);
        const auto txt = [&] (const juce::String& s, float cx, float cy, float px, juce::Colour c, juce::Justification j = juce::Justification::centred) { g.setFont (uiFont (px * sc, true)); g.setColour (c); const bool left = j == juce::Justification::left; g.drawText (s, juce::Rectangle<float> ((left ? cx : cx - 300.f) * sc, (cy - px) * sc, 600.f * sc, 2.f * px * sc), j, false); };
        // ---- teclado: notas de la escala encendidas, la nota destino más fuerte
        const int mask = t.noteMask(), nOut = t.noteOut.load();
        const bool tint = mask != 0xFFF;   // cromática: todas las notas valen, así que no se pinta la escala
        for (int i = 0; i < 31; ++i)
        {
            const int pc = wpc[i % 7], midi = base + 12 * (i / 7) + pc; if (! (mask & (1 << pc)) || (! tint && midi != nOut)) continue;
            juce::RectangleList<float> rl (juce::Rectangle<float> (WE[i] + 1.f, 741.f, WE[i + 1] - WE[i] - 2.f, 126.f)); for (int j = 0; j < 22; ++j) rl.subtract (juce::Rectangle<float> (BC[j] - 15.5f, 740.f, 31.f, 83.5f));
            juce::Path pth; for (auto& r : rl) pth.addRectangle (r * sc); g.setColour (midi == nOut ? col (222, 137, 252) : col (221, 184, 251)); g.fillPath (pth);
        }
        for (int j = 0; j < 22; ++j)
        {
            const int pc = bpc[j % 5], midi = base + 12 * (j / 5) + pc; if (! (mask & (1 << pc)) || (! tint && midi != nOut)) continue;
            g.setColour (midi == nOut ? col (160, 50, 240, .92f) : col (116, 15, 192, .8f)); g.fillRoundedRectangle (juce::Rectangle<float> (BC[j] - 14.f, 741.f, 28.f, 80.f) * sc, 3.f * sc);
        }
        for (int i = 0; i < 31; i += 7) txt ("C" + juce::String ((base + 12 * (i / 7)) / 12 - 1), (WE[i] + WE[i + 1]) * .5f, 852.f, 13.f, col (110, 90, 150, .75f));
        // ---- medidor
        {
            const float cx = 887.5f * sc, cy = 653.1f * sc, k = juce::MathConstants<float>::pi / 180.f * .415f; juce::Graphics::ScopedSaveState ss (g); g.reduceClipRegion ((juce::Rectangle<float> (428.f, 122.f, 902.f, 312.f) * sc).toNearestInt());
            if (nOut >= 0 || std::abs (dIn) > .5f)
            {
                const float a0 = juce::jmin (0.f, dIn) * k, a1 = juce::jmax (0.f, dIn) * k; juce::Path band; band.addPieSegment (cx - 469.f * sc, cy - 469.f * sc, 938.f * sc, 938.f * sc, a0, a1, 425.f / 469.f); g.setColour (col (223, 143, 253, .9f)); g.fillPath (band);
            }
            if (nOut >= 0 || std::abs (dOut) > .5f)
            {
                const float a = dOut * k, ux = std::sin (a), uy = -std::cos (a); juce::Line<float> ln (cx + ux * 385.f * sc, cy + uy * 385.f * sc, cx + ux * 491.f * sc, cy + uy * 491.f * sc);
                g.setColour (col (172, 17, 253, .2f)); g.drawLine (ln, 17.f * sc); g.setColour (col (172, 17, 253)); juce::Path np; np.startNewSubPath (ln.getStart()); np.lineTo (ln.getEnd()); g.strokePath (np, juce::PathStrokeType (9.f * sc, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            const juce::String nm = nOut >= 0 ? juce::String (nn[((nOut % 12) + 12) % 12]) : juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94"));
            txt (nm, 887.5f, 334.f, 120.f, nOut >= 0 ? col (106, 18, 170) : col (106, 18, 170, .3f));
        }
        // ---- perillas: arco y marca
        for (auto& kn : knobs)
        {
            const bool bi = kn.idx == P::pForm; const float n = juce::jlimit (0.f, 1.f, t.param (kn.idx)->getCurrentNormalisedValue()), deg = -135.f + 270.f * n, rad = (bi ? 61.f : 67.f) * sc, ra = juce::MathConstants<float>::pi / 180.f;
            const float from = bi ? 0.f : -135.f, to = deg; juce::Path arc; if (std::abs (to - from) > .5f) arc.addCentredArc (kn.cx * sc, kn.cy * sc, rad, rad, 0.f, juce::jmin (from, to) * ra, juce::jmax (from, to) * ra, true);
            g.setColour (col (172, 50, 243)); g.strokePath (arc, juce::PathStrokeType (8.5f * sc, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
            const float ux = std::sin (deg * ra), uy = -std::cos (deg * ra), r0 = bi ? 22.f : 28.f, r1 = bi ? 48.f : 58.f; juce::Path np; np.startNewSubPath ((kn.cx + ux * r0) * sc, (kn.cy + uy * r0) * sc); np.lineTo ((kn.cx + ux * r1) * sc, (kn.cy + uy * r1) * sc);
            g.setColour (purple); g.strokePath (np, juce::PathStrokeType (6.f * sc, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        const float cx5[5] { 235.f, 562.f, 887.f, 1211.f, 1546.f }; const int pi5[5] { P::pSpeed, P::pHuman, P::pTol, P::pVib, P::pMix };
        for (int k = 0; k < 5; ++k) txt (k == 4 ? juce::String (juce::roundToInt (t.v (P::pMix) * 100.f)) + "%" : juce::String (t.vi (pi5[k])), cx5[k], 677.f, 31.f, ink);
        { const float fv = t.v (P::pForm); txt ((fv > .04f ? "+" : "") + juce::String (fv, 1), 1542.f, 352.f, 20.f, col (120, 40, 190, .85f)); }
        // ---- cuadros de texto
        txt (juce::String (t.v (P::pRef), 1) + " Hz", 220.f, 188.f, 32.f, ink); txt (juce::String (nn[t.vi (P::pKey) % 12]), 115.f, 288.f, 32.f, ink, juce::Justification::left); txt (scaleName (t.vi (P::pScale)), 115.f, 390.f, 32.f, ink, juce::Justification::left);
        // ---- botones superiores
        const int slot = t.vi (P::pSlot); const bool byp = t.vi (P::pBypass) != 0;
        if (slot == 0) drawSprite (g, btnOn, 1373.f, 52.f); else drawSprite (g, btnOn, 1446.f, 52.f);
        if (byp) drawSprite (g, bypOn, 1661.f, 52.f);
        if (flash > 0) { g.setColour (purple.withAlpha (.12f * (float) flash)); g.fillRoundedRectangle (juce::Rectangle<float> (1486.f, 27.f, 89.f, 49.f) * sc, 9.f * sc); }
        txt ("A", 1373.f, 52.f, 28.f, slot == 0 ? juce::Colours::white : ink); txt ("B", 1446.f, 52.f, 28.f, slot == 1 ? juce::Colours::white : ink); txt ("BYPASS", 1661.f, 52.f, 24.f, byp ? juce::Colours::white : ink);
    }
private:
    void toggleNote (int pc)
    {
        using P = InjTunePro; int m = t.noteMask() ^ (1 << pc); if ((m & 0xFFF) == 0) return; t.setValue (P::pMask, (float) m); t.setValue (P::pScale, 11.f); repaint();
    }
    static juce::String scaleName (int i) { static const char* n[] { "Chromatic", "Major", "Minor", "Harmonic Minor", "Pentatonic Major", "Pentatonic Minor", "Dorian", "Phrygian", "Mixolydian", "Blues", "Whole Tone", "Custom" }; return n[juce::jlimit (0, 11, i)]; }
    void refMenu()
    {
        juce::PopupMenu mn; const float vals[] { 432.f, 435.f, 438.f, 440.f, 442.f, 444.f, 446.f }; for (int i = 0; i < 7; ++i) mn.addItem (1 + i, juce::String (vals[i], 1) + " Hz", true, std::abs (t.v (InjTunePro::pRef) - vals[i]) < .05f);
        juce::Component::SafePointer<TuneEditor> sp (this); mn.showMenuAsync (juce::PopupMenu::Options(), [sp, vals] (int r) { if (sp != nullptr && r > 0) { sp->t.setValue (InjTunePro::pRef, vals[r - 1]); sp->repaint(); } });
    }
    void keyMenu()
    {
        juce::PopupMenu mn; for (int i = 0; i < 12; ++i) mn.addItem (1 + i, nn[i], true, t.vi (InjTunePro::pKey) == i);
        juce::Component::SafePointer<TuneEditor> sp (this); mn.showMenuAsync (juce::PopupMenu::Options(), [sp] (int r) { if (sp != nullptr && r > 0) { sp->t.setValue (InjTunePro::pKey, (float) (r - 1)); sp->repaint(); } });
    }
    void scaleMenu()
    {
        juce::PopupMenu mn; for (int i = 0; i < 12; ++i) mn.addItem (1 + i, scaleName (i), true, t.vi (InjTunePro::pScale) == i);
        juce::Component::SafePointer<TuneEditor> sp (this);
        mn.showMenuAsync (juce::PopupMenu::Options(), [sp] (int r) { if (sp == nullptr || r <= 0) return; if (r == 12) sp->t.setValue (InjTunePro::pMask, (float) sp->t.noteMask()); sp->t.setValue (InjTunePro::pScale, (float) (r - 1)); sp->repaint(); });
    }
    InjTunePro& t; juce::Image cap, btnOn, bypOn; float dIn = 0.f, dOut = 0.f; int lastNoteShown = -1, flash = 0, base = 36; const juce::Rectangle<float> refBox { 92.f, 160.f, 246.f, 52.f };
    static inline const char* nn[12] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static inline const int wpc[7] { 0, 2, 4, 5, 7, 9, 11 }, bpc[5] { 1, 3, 6, 8, 10 };
    static inline const float WE[32] { 107.5f, 161.f, 213.f, 265.f, 316.5f, 368.f, 419.5f, 470.f, 521.5f, 572.5f, 623.3f, 674.5f, 726.f, 776.5f, 827.f, 878.f, 929.5f, 981.f, 1031.5f, 1082.f, 1133.5f, 1183.f, 1233.5f, 1284.f, 1334.f, 1385.5f, 1436.f, 1486.f, 1535.f, 1585.f, 1635.5f, 1672.f };
    static inline const float BC[22] { 158.5f, 216.f, 315.5f, 369.5f, 422.f, 521.f, 575.f, 674.f, 727.f, 779.f, 879.f, 932.5f, 1032.f, 1084.5f, 1137.5f, 1233.f, 1287.f, 1385.f, 1438.f, 1490.f, 1585.f, 1637.f };
};
inline std::unique_ptr<te::Plugin::EditorComponent> InjTunePro::createEditor() { return std::make_unique<TuneEditor> (*this); }
} // namespace injfx
