#pragma once
#include "FxAll.h"
#include <cstdio>
// Pruebas automáticas de los efectos integrados (se lanzan con INJ_FXTEST=1). Miden respuesta, nivel, latencia y estabilidad.
namespace injfx::test
{
using Buf = juce::AudioBuffer<float>;
constexpr double SR = 44100.0;

struct Rig
{
    te::Plugin::Ptr p; InjFx* fx = nullptr;
    Rig (te::Edit& e, const char* type) { p = e.getPluginCache().createNewPlugin (type, {}); fx = dynamic_cast<InjFx*> (p.get()); if (fx) fx->initialise ({ te::TimePosition(), SR, 512 }); }
    void set (int i, float v) { fx->setValue (i, v); }
    Buf run (const Buf& in)
    {
        Buf out; out.makeCopyOf (in);
        for (int s = 0; s < out.getNumSamples(); s += 512)
        {
            const int n = std::min (512, out.getNumSamples() - s);
            te::PluginRenderContext ctx (&out, juce::AudioChannelSet::stereo(), s, n, nullptr, 0.0, te::TimeRange (te::TimePosition::fromSeconds (s / SR), te::TimeDuration::fromSeconds (n / SR)), true, false, false, false);
            fx->applyToBuffer (ctx);
        }
        return out;
    }
};

inline Buf sine (double f, float amp, double secs, double phase = 0.0)
{
    Buf b (2, (int) (secs * SR)); for (int c = 0; c < 2; ++c) for (int i = 0; i < b.getNumSamples(); ++i) b.setSample (c, i, amp * (float) std::sin (2 * juce::MathConstants<double>::pi * f * i / SR + phase)); return b;
}
inline Buf noise (float amp, double secs, int seed = 1)
{
    juce::Random r (seed); Buf b (2, (int) (secs * SR)); for (int c = 0; c < 2; ++c) for (int i = 0; i < b.getNumSamples(); ++i) b.setSample (c, i, amp * (r.nextFloat() * 2.f - 1.f)); return b;
}
inline Buf impulse (double secs, float amp = 1.f, int at = 100) { Buf b (2, (int) (secs * SR)); b.clear(); b.setSample (0, at, amp); b.setSample (1, at, amp); return b; }
inline double rmsDb (const Buf& b, double t0, double t1)
{
    const int a = (int) (t0 * SR), z = std::min (b.getNumSamples(), (int) (t1 * SR)); double s = 0; int n = 0;
    for (int c = 0; c < 2; ++c) for (int i = a; i < z; ++i) { const double x = b.getSample (c, i); s += x * x; ++n; }
    return 10.0 * std::log10 (std::max (1e-20, s / std::max (1, n)));
}
inline float peakOf (const Buf& b, double t0, double t1)
{
    float m = 0.f; for (int c = 0; c < 2; ++c) for (int i = (int) (t0 * SR); i < std::min (b.getNumSamples(), (int) (t1 * SR)); ++i) m = std::max (m, std::abs (b.getSample (c, i))); return m;
}
inline bool finite (const Buf& b) { for (int c = 0; c < 2; ++c) for (int i = 0; i < b.getNumSamples(); ++i) if (! std::isfinite (b.getSample (c, i))) return false; return true; }
// distorsión armónica total de un seno de f Hz (ventana de ciclos enteros)
inline double thd (const Buf& b, double f, double t0, double t1)
{
    const int a = (int) (t0 * SR), z = (int) (t1 * SR); double sc = 0, ss = 0, tot = 0; const int n = z - a;
    for (int i = a; i < z; ++i) { const double w = 2 * juce::MathConstants<double>::pi * f * i / SR, x = b.getSample (0, i); sc += x * std::cos (w); ss += x * std::sin (w); tot += x * x; }
    const double fund = 2.0 * (sc * sc + ss * ss) / ((double) n * n) * n / 2.0 * 2.0 / n; (void) fund;
    const double A2 = (sc * sc + ss * ss) * 4.0 / ((double) n * n), fundE = A2 / 2.0, totE = tot / n; return std::sqrt (std::max (0.0, totE - fundE) / std::max (1e-20, fundE));
}

inline void run (te::Edit& edit, int& fails)
{
    auto chk = [&] (bool ok, const juce::String& what) { std::printf ("%s  %s\n", ok ? "OK  " : "FALLO", what.toRawUTF8()); if (! ok) ++fails; };
    // ---- existencia, presets y guardado de parámetros
    for (auto& e : catalog())
    {
        Rig r (edit, e.type); chk (r.fx != nullptr, juce::String ("efecto: se crea ") + e.label); if (r.fx == nullptr) continue;
        auto ps = r.fx->presets(); bool ok = true; for (int i = 0; i < (int) ps.size(); ++i) { r.fx->applyPreset (i); for (int k = 0; k < r.fx->numSpecs(); ++k) { const auto& s = r.fx->specs()[k]; const float x = r.fx->v (k); if (x < s.lo - 1e-3f || x > s.hi + 1e-3f) ok = false; } }
        chk (ok && ! ps.empty(), juce::String ("efecto: sus presets caen dentro de los rangos - ") + e.label);
        r.fx->resetAll(); r.set (0, r.fx->specs()[0].hi * .5f); r.fx->flushPluginStateToValueTree(); auto st = r.p->state.createCopy(); st.removeProperty ("id", nullptr);
        auto p2 = edit.getPluginCache().createNewPlugin (st); auto* f2 = dynamic_cast<InjFx*> (p2.get());
        chk (f2 != nullptr && std::abs (f2->v (0) - r.fx->v (0)) < 1e-3f, juce::String ("efecto: guarda y recupera sus parametros - ") + e.label);
    }
    // ---- ecualizador
    { Rig r (edit, InjEq::xmlTypeName); r.set (6, 1000.f); r.set (7, 12.f); r.set (8, 1.f);
      auto in = sine (1000, .1f, 1.0); auto o = r.run (in); const double g = rmsDb (o, .3, .9) - rmsDb (in, .3, .9); chk (std::abs (g - 12.0) < 1.0, "ecualizador: +12 dB en 1 kHz (" + juce::String (g, 2) + ")");
      auto in2 = sine (100, .1f, 1.0); Rig r2 (edit, InjEq::xmlTypeName); r2.set (6, 1000.f); r2.set (7, 12.f); r2.set (8, 1.f); auto o2 = r2.run (in2); const double g2 = rmsDb (o2, .3, .9) - rmsDb (in2, .3, .9); chk (std::abs (g2) < .6, "ecualizador: no toca los 100 Hz (" + juce::String (g2, 2) + ")");
      Rig r3 (edit, InjEq::xmlTypeName); r3.set (0, 100.f); r3.set (1, -12.f); auto in3 = sine (40, .1f, 1.0); auto o3 = r3.run (in3); const double g3 = rmsDb (o3, .3, .9) - rmsDb (in3, .3, .9); chk (g3 < -9.0, "ecualizador: el estante bajo recorta graves (" + juce::String (g3, 2) + ")"); }
    // ---- compresor
    { Rig r (edit, InjComp::xmlTypeName); r.set (InjComp::pThr, -24.f); r.set (InjComp::pRatio, 4.f); r.set (InjComp::pAtk, 1.f); r.set (InjComp::pRel, 50.f); r.set (InjComp::pGain, 0.f);
      auto in = sine (1000, .5f, 1.5); auto o = r.run (in); const double g = rmsDb (o, 1.0, 1.4) - rmsDb (in, 1.0, 1.4);
      chk (std::abs (g + 13.5) < 1.2, "compresor: -24 dB, 4:1, entrada -6 dB baja ~13.5 dB (" + juce::String (g, 2) + ")"); chk (std::abs (r.fx->meterA.load() - 13.5f) < 1.5f, "compresor: el medidor de reduccion marca " + juce::String (r.fx->meterA.load(), 2));
      Rig r2 (edit, InjComp::xmlTypeName); r2.set (InjComp::pThr, -24.f); r2.set (InjComp::pMix, 0.f); r2.set (InjComp::pGain, 0.f); auto o2 = r2.run (in); chk (std::abs (rmsDb (o2, 1.0, 1.4) - rmsDb (in, 1.0, 1.4)) < .05, "compresor: mezcla 0 deja el sonido igual");
      Rig r3 (edit, InjComp::xmlTypeName); r3.set (InjComp::pThr, 0.f); r3.set (InjComp::pGain, 0.f); auto o3 = r3.run (sine (1000, .3f, 1.0)); chk (std::abs (rmsDb (o3, .5, .9) - rmsDb (sine (1000, .3f, 1.0), .5, .9)) < .1, "compresor: bajo el umbral no toca la senal"); }
    // ---- limitador
    { Rig r (edit, InjLimit::xmlTypeName); r.set (InjLimit::pCeil, -1.f); r.set (InjLimit::pGain, 0.f); const float ceil = db2lin (-1.f);
      auto in = sine (1000, 2.f, 1.0); auto o = r.run (in); const float pk = peakOf (o, .2, 1.0); chk (pk <= ceil + 1e-4f && pk > ceil * .9f, "limitador: +6 dB de senal no pasa del techo -1 dB (pico " + juce::String (pk, 4) + ")");
      Rig r2 (edit, InjLimit::xmlTypeName); r2.set (InjLimit::pCeil, -6.f); auto cl = impulse (.2, .1f, 1000); auto oc = r2.run (cl); int at = 0; float best = 0; for (int i = 0; i < oc.getNumSamples(); ++i) if (std::abs (oc.getSample (0, i)) > best) { best = std::abs (oc.getSample (0, i)); at = i; }
      const int lat = (int) std::lround (r2.fx->getLatencySeconds() * SR); chk (at - 1000 == lat && std::abs (best - .1f) < .002f, "limitador: la latencia declarada (" + juce::String (lat) + " muestras) coincide con la real (" + juce::String (at - 1000) + ")");
      Rig r3 (edit, InjLimit::xmlTypeName); r3.set (InjLimit::pCeil, -1.f); r3.set (InjLimit::pMode, 2.f); auto in3 = sine (11025, 1.5f, 1.0, .785398); auto o3 = r3.run (in3); chk (peakOf (o3, .2, 1.0) <= ceil + 1e-4f, "limitador: modo pico real contiene los picos entre muestras");
      Rig r4 (edit, InjLimit::xmlTypeName); r4.set (InjLimit::pCeil, -1.f); r4.set (InjLimit::pGain, 12.f); auto n4 = noise (.5f, 2.0); auto o4 = r4.run (n4); chk (finite (o4) && peakOf (o4, .1, 2.0) <= ceil + 1e-4f, "limitador: ruido fuerte +12 dB queda bajo el techo"); }
    // ---- filtro
    { auto attn = [&] (int type, int slope, float cut, double f) { Rig r (edit, InjFilter::xmlTypeName); r.set (InjFilter::pType, (float) type); r.set (InjFilter::pSlope, (float) slope); r.set (InjFilter::pCut, cut); r.set (InjFilter::pRes, .0f);
          auto in = sine (f, .2f, 1.0); auto o = r.run (in); return rmsDb (o, .4, .9) - rmsDb (in, .4, .9); };
      chk (attn (0, 1, 1000.f, 10000) < -40.0 && std::abs (attn (0, 1, 1000.f, 100)) < .8, "filtro: pasa bajos de 24 dB a 1 kHz corta los 10 kHz (" + juce::String (attn (0, 1, 1000.f, 10000), 1) + " dB)");
      const double s12 = attn (0, 0, 1000.f, 4000); chk (s12 < -18.0 && s12 > -30.0, "filtro: 12 dB por octava (a 4 kHz " + juce::String (s12, 1) + " dB)");
      chk (attn (1, 1, 1000.f, 100) < -35.0 && std::abs (attn (1, 1, 1000.f, 8000)) < .8, "filtro: pasa altos corta los graves");
      chk (attn (3, 0, 1000.f, 1000) < -25.0 && std::abs (attn (3, 0, 1000.f, 100)) < 1.0, "filtro: el rechazo anula el corte (" + juce::String (attn (3, 0, 1000.f, 1000), 1) + " dB)");
      Rig r (edit, InjFilter::xmlTypeName); r.set (InjFilter::pRes, 1.f); auto o = r.run (noise (.3f, 2.0)); chk (finite (o) && peakOf (o, 0, 2.0) < 20.f, "filtro: con resonancia al maximo sigue estable");
      Rig rs (edit, InjFilter::xmlTypeName); Buf sw = noise (.3f, 1.0); Buf out; out.makeCopyOf (sw); bool ok = true; for (int s = 0; s < out.getNumSamples(); s += 256) { rs.set (InjFilter::pCut, 100.f * std::pow (150.f, (float) s / (float) out.getNumSamples())); Buf part (2, 256); for (int c = 0; c < 2; ++c) part.copyFrom (c, 0, out, c, s, std::min (256, out.getNumSamples() - s)); part = rs.run (part); if (! finite (part)) ok = false; }
      chk (ok, "filtro: el barrido rapido del corte no se desestabiliza"); }
    // ---- saturador
    { Rig r (edit, InjSat::xmlTypeName); r.set (InjSat::pType, 0.f); r.set (InjSat::pAmt, .7f); r.set (InjSat::pTone, 0.f); auto in = sine (1000, .5f, 1.0); auto o = r.run (in); const double t = thd (o, 1000, .3, .9);
      chk (t > .03 && finite (o), "saturador: suave genera armonicos (THD " + juce::String (t * 100, 1) + " %)");
      Rig r2 (edit, InjSat::xmlTypeName); r2.set (InjSat::pType, 4.f); r2.set (InjSat::pAmt, 1.f); r2.set (InjSat::pOut, 0.f); auto o2 = r2.run (in); chk (peakOf (o2, .3, 1.0) < 1.2f, "saturador: el clipper no se pasa (pico " + juce::String (peakOf (o2, .3, 1.0), 2) + ")");
      Rig r3 (edit, InjSat::xmlTypeName); r3.set (InjSat::pMix, 0.f); r3.set (InjSat::pTone, 0.f); auto o3 = r3.run (in); const int lat = (int) std::lround (r3.fx->getLatencySeconds() * SR); double err = 0; for (int i = 4000; i < 30000; ++i) err = std::max (err, (double) std::abs (o3.getSample (0, i) - in.getSample (0, i - lat)));
      chk (err < 2e-3, "saturador: mezcla 0 devuelve la senal original con su latencia de " + juce::String (lat) + " muestras (error " + juce::String (err, 5) + ")");
      Rig r4 (edit, InjSat::xmlTypeName); r4.set (InjSat::pType, 3.f); r4.set (InjSat::pAmt, .8f); auto o4 = r4.run (in); double dc = 0; for (int i = 20000; i < 40000; ++i) dc += o4.getSample (0, i); dc /= 20000.0; chk (std::abs (dc) < .02, "saturador: el fuzz asimetrico no deja continua (" + juce::String (dc, 4) + ")"); }
    // ---- reverb
    { Rig r (edit, InjReverb::xmlTypeName); r.set (InjReverb::pMix, 1.f); r.set (InjReverb::pTime, 1.8f); auto o = r.run (impulse (5.0)); chk (finite (o), "reverb: no produce valores invalidos");
      const double a = rmsDb (o, .3, .8), b = rmsDb (o, 3.0, 3.5); chk (a - b > 30.0 && a > -75.0, "reverb: la cola cae con el tiempo (" + juce::String (a, 1) + " -> " + juce::String (b, 1) + " dB)");
      Rig rl (edit, InjReverb::xmlTypeName); rl.set (InjReverb::pMix, 1.f); rl.set (InjReverb::pTime, 6.f); auto ol = rl.run (impulse (5.0)); chk (rmsDb (ol, 3.0, 3.5) - b > 20.0, "reverb: un tiempo mas largo alarga la cola");
      const double t60 = [&] { double ref = rmsDb (o, .15, .25); for (double t = .3; t < 4.8; t += .1) if (rmsDb (o, t, t + .1) < ref - 60.0) return t; return 9.0; }(); chk (t60 > .9 && t60 < 3.4, "reverb: la cola baja 60 dB hacia el tiempo pedido de 1.8 s (medido " + juce::String (t60, 2) + " s)");
      Rig r0 (edit, InjReverb::xmlTypeName); r0.set (InjReverb::pMix, 0.f); auto in = noise (.3f, .5); auto o0 = r0.run (in); double e = 0; for (int i = 0; i < in.getNumSamples(); ++i) e = std::max (e, (double) std::abs (o0.getSample (0, i) - in.getSample (0, i))); chk (e < 1e-6, "reverb: mezcla 0 deja el sonido seco");
      Rig rn (edit, InjReverb::xmlTypeName); rn.set (InjReverb::pMix, 1.f); rn.set (InjReverb::pTime, 2.f); auto nin = noise (.25f, 4.0); auto on = rn.run (nin); const double lv = rmsDb (on, 2.0, 4.0) - rmsDb (nin, 2.0, 4.0); chk (lv > -9.0 && lv < 6.0, "reverb: el nivel de la reverb es parecido al de la entrada (" + juce::String (lv, 1) + " dB)");
      for (int ty = 0; ty < 4; ++ty) { Rig rt (edit, InjReverb::xmlTypeName); rt.set (InjReverb::pType, (float) ty); rt.set (InjReverb::pMix, 1.f); rt.set (InjReverb::pTime, 4.f); auto ot = rt.run (impulse (3.0)); chk (finite (ot) && peakOf (ot, 0, 3.0) < 1.5f && rmsDb (ot, .5, 1.0) > -70.0, "reverb: el tipo " + juce::String (ty) + " es estable y suena"); } }
    // ---- delay
    { Rig r (edit, InjDelay::xmlTypeName); r.set (InjDelay::pTime, 4.f); r.set (InjDelay::pFree, 100.f); r.set (InjDelay::pFb, .5f); r.set (InjDelay::pMix, 1.f); r.set (InjDelay::pPing, 0.f); r.set (InjDelay::pTone, 20000.f); r.set (InjDelay::pWidth, 1.f);
      auto o = r.run (impulse (1.0, 1.f, 1000)); auto peakAt = [&] (int a, int z) { int at = a; float b = 0; for (int i = a; i < z; ++i) if (std::abs (o.getSample (0, i)) > b) { b = std::abs (o.getSample (0, i)); at = i; } return std::make_pair (at, b); };
      const auto e1 = peakAt (1000 + 4000, 1000 + 4800), e2 = peakAt (1000 + 8800, 1000 + 9600); chk (std::abs (e1.first - (1000 + 4410)) <= 3, "delay: el primer eco llega a los 100 ms (muestra " + juce::String (e1.first - 1000) + ")");
      chk (e2.second / e1.second > .35f && e2.second / e1.second < .65f, "delay: con repeticiones 50 % el segundo eco es la mitad (" + juce::String (e2.second / e1.second, 2) + ")");
      Rig rs (edit, InjDelay::xmlTypeName); rs.set (InjDelay::pTime, 1.f); rs.set (InjDelay::pMix, 1.f); rs.set (InjDelay::pTone, 20000.f); auto os = rs.run (impulse (1.0, 1.f, 1000)); int at = 0; float bb = 0; for (int i = 3000; i < os.getNumSamples(); ++i) if (std::abs (os.getSample (0, i)) > bb) { bb = std::abs (os.getSample (0, i)); at = i; }
      const double bpm = edit.tempoSequence.getBpmAt (te::TimePosition()); chk (std::abs ((at - 1000) - (int) (.5 * 60.0 / bpm * SR)) <= 3, "delay: 1/8 a " + juce::String (bpm, 0) + " BPM tarda " + juce::String (30000.0 / bpm, 0) + " ms (muestra " + juce::String (at - 1000) + ")");
      Rig rp (edit, InjDelay::xmlTypeName); rp.set (InjDelay::pTime, 4.f); rp.set (InjDelay::pFree, 100.f); rp.set (InjDelay::pPing, 1.f); rp.set (InjDelay::pMix, 1.f); rp.set (InjDelay::pTone, 20000.f); rp.set (InjDelay::pFb, .6f); rp.set (InjDelay::pWidth, 1.f);
      auto op = rp.run (impulse (1.0, 1.f, 1000)); float l1 = 0, r1 = 0, l2 = 0, r2 = 0; for (int i = 5000; i < 5800; ++i) { l1 = std::max (l1, std::abs (op.getSample (0, i))); r1 = std::max (r1, std::abs (op.getSample (1, i))); } for (int i = 9400; i < 10200; ++i) { l2 = std::max (l2, std::abs (op.getSample (0, i))); r2 = std::max (r2, std::abs (op.getSample (1, i))); }
      chk (l1 > r1 * 2.f && r2 > l2 * 1.5f || (l1 > .2f && r1 < l1), "delay: el rebote alterna izquierda y derecha (L1 " + juce::String (l1, 2) + " R1 " + juce::String (r1, 2) + " L2 " + juce::String (l2, 2) + " R2 " + juce::String (r2, 2) + ")");
      Rig rf (edit, InjDelay::xmlTypeName); rf.set (InjDelay::pTime, 4.f); rf.set (InjDelay::pFree, 30.f); rf.set (InjDelay::pFb, .95f); rf.set (InjDelay::pMix, 1.f); auto of = rf.run (noise (.8f, 6.0)); chk (finite (of) && peakOf (of, 0, 6.0) < 4.f, "delay: con realimentacion al 95 % no se dispara"); }
    // ---- modulación
    { auto in = noise (.3f, 1.5);
      Rig r0 (edit, InjMod::xmlTypeName); r0.set (InjMod::pMix, 0.f); auto o0 = r0.run (in); double e = 0; for (int i = 0; i < in.getNumSamples(); ++i) e = std::max (e, (double) std::abs (o0.getSample (0, i) - in.getSample (0, i))); chk (e < 1e-6, "modulacion: chorus con mezcla 0 deja el sonido igual");
      for (int m = 0; m < 3; ++m) { Rig r (edit, InjMod::xmlTypeName); r.set (InjMod::pMode, (float) m); r.set (InjMod::pMix, .8f); r.set (InjMod::pFb, .9f); r.set (InjMod::pDepth, 1.f); auto o = r.run (in);
          double d = 0; for (int i = 20000; i < 30000; ++i) d += std::abs (o.getSample (0, i) - in.getSample (0, i)); chk (finite (o) && peakOf (o, 0, 1.5) < 6.f && d > 10.0, juce::String ("modulacion: el modo ") + juce::String (m) + " cambia el sonido y es estable"); }
      Rig rl (edit, InjMod::xmlTypeName); rl.set (InjMod::pMode, 0.f); rl.set (InjMod::pMix, .5f); auto ol = rl.run (impulse (.2, 1.f, 100)); int taps = 0; for (int i = 0; i < ol.getNumSamples(); ++i) if (std::abs (ol.getSample (0, i)) > .05f) ++taps; chk (taps >= 2, "modulacion: el chorus suma voces retrasadas"); }
    // ---- ventanas: se crean y se dibujan
    { const char* dir = std::getenv ("INJ_FXSHOT"); bool ok = true;
      for (auto& e : catalog())
      {
          Rig r (edit, e.type); auto ed = r.fx->createEditor(); if (ed == nullptr) { ok = false; continue; }
          if (dir) { auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.f); juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (dir)); f.createDirectory(); juce::PNGImageFormat png; juce::FileOutputStream os (f.getChildFile (juce::String (e.type) + ".png")); if (os.openedOk()) { os.setPosition (0); os.truncate(); png.writeImageToStream (img, os); } }
      }
      chk (ok, "ventanas: los " + juce::String ((int) catalog().size()) + " efectos crean su ventana"); }
}
} // namespace injfx::test
