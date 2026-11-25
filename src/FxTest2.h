#pragma once
#include "FxTest.h"
// Pruebas de los efectos de la segunda tanda.
namespace injfx::test
{
inline double amp (const Buf& b, double f, double t0, double t1, int ch = 0)   // amplitud de una frecuencia (Goertzel)
{
    const int a = (int) (t0 * SR), z = std::min (b.getNumSamples(), (int) (t1 * SR)); double sc = 0, ss = 0; for (int i = a; i < z; ++i) { const double w = 2 * juce::MathConstants<double>::pi * f * i / SR, x = b.getSample (ch, i); sc += x * std::cos (w); ss += x * std::sin (w); }
    return 2.0 * std::sqrt (sc * sc + ss * ss) / std::max (1, z - a);
}
inline double ampDb (const Buf& b, double f, double t0, double t1, int ch = 0) { return 20.0 * std::log10 (std::max (1e-12, amp (b, f, t0, t1, ch))); }
inline double rmsCh (const Buf& b, int ch, double t0, double t1) { double s = 0; int n = 0; for (int i = (int) (t0 * SR); i < std::min (b.getNumSamples(), (int) (t1 * SR)); ++i) { const double x = b.getSample (ch, i); s += x * x; ++n; } return 10.0 * std::log10 (std::max (1e-20, s / std::max (1, n))); }
inline Buf stereoOf (const Buf& l, const Buf& r) { Buf b (2, l.getNumSamples()); b.copyFrom (0, 0, l, 0, 0, l.getNumSamples()); b.copyFrom (1, 0, r, 0, 0, l.getNumSamples()); return b; }
inline double maxDiff (const Buf& a, const Buf& b, int off = 0, int from = 0, int to = -1) { double e = 0; if (to < 0) to = a.getNumSamples(); for (int c = 0; c < 2; ++c) for (int i = std::max (from, off); i < to && i - off < b.getNumSamples(); ++i) e = std::max (e, (double) std::abs (a.getSample (c, i) - b.getSample (c, i - off))); return e; }

inline Buf zeros (double secs) { Buf b (2, (int) (secs * SR)); b.clear(); return b; }
inline void runIntegration (te::Edit& edit, int& fails)
{
    auto chk = [&] (bool ok, const juce::String& what) { std::printf ("%s  %s\n", ok ? "OK  " : "FALLO", what.toRawUTF8()); if (! ok) ++fails; };
    // ---- integracion: los 38 efectos juntos en la cadena de una pista real y un render offline completo
    { auto tracks = te::getAudioTracks (edit); chk (! tracks.isEmpty(), "integracion: el proyecto tiene pistas");
      if (! tracks.isEmpty())
      {
          auto* t = tracks[0]; auto* vol = t->getVolumePlugin(); juce::Array<te::Plugin::Ptr> added; for (auto& e : catalog()) { auto np = edit.getPluginCache().createNewPlugin (e.type, {}); if (np == nullptr) continue; t->pluginList.insertPlugin (np, vol ? t->pluginList.indexOf (vol) : -1, nullptr); np->setEnabled (true); added.add (np); }
          chk (added.size() == (int) catalog().size(), "integracion: se insertan los " + juce::String (added.size()) + " efectos en la cadena de la pista");
          auto out = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_fxchain.wav"); out.deleteFile(); const bool ok = te::Renderer::renderToFile (edit, out, false);
          bool good = ok && out.existsAsFile() && out.getSize() > 1000; if (good) { juce::AudioFormatManager fm; fm.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (out)); if (rd != nullptr) { juce::AudioBuffer<float> b ((int) rd->numChannels, (int) std::min<juce::int64> (rd->lengthInSamples, 400000)); rd->read (&b, 0, b.getNumSamples(), 0, true, true); for (int c = 0; c < b.getNumChannels() && good; ++c) for (int i = 0; i < b.getNumSamples(); ++i) if (! std::isfinite (b.getSample (c, i))) { good = false; break; } } else good = false; }
          std::printf ("  [diag] render ok=%d existe=%d bytes=%lld good=%d\n", (int) ok, (int) out.existsAsFile(), (long long) out.getSize(), (int) good); chk (good, "integracion: el render offline con todos los efectos termina y no tiene valores invalidos"); out.deleteFile();
          for (auto& p : added) p->deleteFromParent(); chk (t->pluginList.size() <= 1 + 8, "integracion: los efectos se quitan de la cadena"); }
    }
}
inline void runNew (te::Edit& edit, int& fails)
{
    const double bpm = edit.tempoSequence.getBpmAt (te::TimePosition()), beatS = 60.0 / bpm;
    auto chk = [&] (bool ok, const juce::String& what) { std::printf ("%s  %s\n", ok ? "OK  " : "FALLO", what.toRawUTF8()); if (! ok) ++fails; };
    // ---- bombeo
    { Rig r (edit, InjDucker::xmlTypeName); r.set (InjDucker::pRate, 1.f); r.set (InjDucker::pDepth, 1.f); r.set (InjDucker::pRel, .6f); r.set (InjDucker::pAtk, .5f); auto o = r.run (sine (200, .5f, 3.0));
      const double lo = rmsDb (o, 2 * beatS + .004, 2 * beatS + .02), hi = rmsDb (o, 2 * beatS + .3, 2 * beatS + .36); chk (hi - lo > 20.0, "bombeo: el volumen se hunde al golpe de cada pulso y se recupera (" + juce::String (hi - lo, 1) + " dB)"); }
    // ---- multibanda
    { auto in = noise (.2f, 2.0); Rig r (edit, InjMulti::xmlTypeName); r.set (InjMulti::pRL, 1.f); r.set (InjMulti::pRM, 1.f); r.set (InjMulti::pRH, 1.f); auto o = r.run (in); chk (std::abs (rmsDb (o, .5, 1.9) - rmsDb (in, .5, 1.9)) < .6, "multibanda: con ratio 1 suma plana (" + juce::String (rmsDb (o, .5, 1.9) - rmsDb (in, .5, 1.9), 2) + " dB)");
      Rig r2 (edit, InjMulti::xmlTypeName); r2.set (InjMulti::pTL, -60.f); r2.set (InjMulti::pRL, 20.f); r2.set (InjMulti::pRM, 1.f); r2.set (InjMulti::pRH, 1.f); auto lo = sine (60, .5f, 2.0); auto ol = r2.run (lo); chk (rmsDb (ol, 1.0, 1.9) - rmsDb (lo, 1.0, 1.9) < -8.0, "multibanda: comprime la banda grave (" + juce::String (rmsDb (ol, 1.0, 1.9) - rmsDb (lo, 1.0, 1.9), 1) + " dB)");
      auto mi = sine (1000, .5f, 2.0); auto om = r2.run (mi); chk (std::abs (rmsDb (om, 1.0, 1.9) - rmsDb (mi, 1.0, 1.9)) < 1.0, "multibanda: no toca la banda media"); }
    // ---- puerta
    { Rig r (edit, InjGate::xmlTypeName); r.set (InjGate::pThr, -40.f); r.set (InjGate::pRange, -80.f); auto q = sine (1000, .001f, 1.5); auto oq = r.run (q); chk (rmsDb (oq, .8, 1.4) - rmsDb (q, .8, 1.4) < -30.0, "puerta: cierra con senal bajo el umbral");
      Rig r2 (edit, InjGate::xmlTypeName); r2.set (InjGate::pThr, -40.f); auto l = sine (1000, .3f, 1.5); auto ol = r2.run (l); chk (std::abs (rmsDb (ol, .8, 1.4) - rmsDb (l, .8, 1.4)) < .5, "puerta: deja pasar la senal fuerte"); }
    // ---- de-esser
    { Rig r (edit, InjDeEss::xmlTypeName); r.set (InjDeEss::pFreq, 7000.f); r.set (InjDeEss::pThr, -30.f); r.set (InjDeEss::pAmt, 12.f); auto s = sine (7000, .5f, 1.5); auto os = r.run (s); Rig r2 (edit, InjDeEss::xmlTypeName); r2.set (InjDeEss::pFreq, 7000.f); r2.set (InjDeEss::pThr, -30.f); auto b = sine (500, .5f, 1.5); auto ob = r2.run (b);
      chk (rmsDb (os, .8, 1.4) - rmsDb (s, .8, 1.4) < -3.0 && std::abs (rmsDb (ob, .8, 1.4) - rmsDb (b, .8, 1.4)) < 1.0, "de-esser: baja 7 kHz (" + juce::String (rmsDb (os, .8, 1.4) - rmsDb (s, .8, 1.4), 1) + " dB) y deja los 500 Hz"); }
    // ---- transientes
    { auto kick = sine (100, .6f, 1.0); for (int i = 0; i < kick.getNumSamples(); ++i) { const float e = std::exp (-(float) i / (float) (.12 * SR)); for (int c = 0; c < 2; ++c) kick.setSample (c, i, kick.getSample (c, i) * e); }
      Rig a (edit, InjTransient::xmlTypeName); a.set (InjTransient::pAtk, 1.f); Rig b (edit, InjTransient::xmlTypeName); b.set (InjTransient::pAtk, -1.f); const float pa = peakOf (a.run (kick), 0, .03), pb = peakOf (b.run (kick), 0, .03); chk (pa > pb * 1.15f, "transientes: el ataque sube y baja el golpe (" + juce::String (pa, 2) + " vs " + juce::String (pb, 2) + ")");
      Rig c (edit, InjTransient::xmlTypeName); c.set (InjTransient::pSus, 1.f); Rig d (edit, InjTransient::xmlTypeName); d.set (InjTransient::pSus, -1.f); chk (rmsDb (c.run (kick), .2, .4) > rmsDb (d.run (kick), .2, .4) + 3.0, "transientes: el sosten alarga o corta la cola"); }
    // ---- ensanchador
    { auto in = stereoOf (sine (1000, .4f, 1.0), noise (.3f, 1.0)); Rig r (edit, InjWidener::xmlTypeName); r.set (InjWidener::pWidth, 0.f); r.set (InjWidener::pMono, 20.f); auto o = r.run (in); double e = 0; for (int i = 22000; i < 40000; ++i) e = std::max (e, (double) std::abs (o.getSample (0, i) - o.getSample (1, i))); chk (e < 2e-3, "ensanchador: ancho 0 deja izquierda = derecha (" + juce::String (e, 5) + ")");
      Buf side (2, (int) SR); for (int i = 0; i < side.getNumSamples(); ++i) { const float x = .4f * std::sin (2.f * kPi * 50.f * i / (float) SR); side.setSample (0, i, x); side.setSample (1, i, -x); } Rig r2 (edit, InjWidener::xmlTypeName); r2.set (InjWidener::pMono, 250.f); auto o2 = r2.run (side);
      chk (rmsDb (o2, .5, .95) - rmsDb (side, .5, .95) < -6.0, "ensanchador: graves en mono quita los graves de los lados (" + juce::String (rmsDb (o2, .5, .95) - rmsDb (side, .5, .95), 1) + " dB)"); }
    // ---- clipper
    { Rig r (edit, InjClipper::xmlTypeName); r.set (InjClipper::pType, 0.f); r.set (InjClipper::pDrive, 12.f); r.set (InjClipper::pCeil, -1.f); auto o = r.run (sine (1000, .8f, 1.0)); const float pk = peakOf (o, .2, 1.0); chk (pk < db2lin (-1.f) * 1.12f && pk > db2lin (-1.f) * .9f, "clipper: no pasa del techo (" + juce::String (pk, 3) + ")");
      Rig r2 (edit, InjClipper::xmlTypeName); r2.set (InjClipper::pMix, 0.f); r2.set (InjClipper::pCeil, 0.f); auto in = sine (800, .5f, 1.0); auto o2 = r2.run (in); const int lat = (int) std::lround (r2.fx->getLatencySeconds() * SR); chk (maxDiff (o2, in, lat, 4000, 30000) < 2e-3, "clipper: mezcla 0 devuelve la senal con su latencia de " + juce::String (lat) + " muestras"); }
    // ---- bitcrusher
    { Rig r (edit, InjCrusher::xmlTypeName); r.set (InjCrusher::pBits, 4.f); auto o = r.run (sine (440, .9f, .5)); std::set<int> vals; for (int i = 0; i < o.getNumSamples(); ++i) vals.insert (juce::roundToInt (o.getSample (0, i) * 1000.f)); chk ((int) vals.size() <= 17, "bitcrusher: 4 bits dejan como mucho 17 niveles (" + juce::String ((int) vals.size()) + ")");
      Rig r2 (edit, InjCrusher::xmlTypeName); r2.set (InjCrusher::pBits, 16.f); r2.set (InjCrusher::pRate, 4410.f); auto o2 = r2.run (noise (.5f, .5)); int same = 0; for (int i = 1; i < o2.getNumSamples(); ++i) if (o2.getSample (0, i) == o2.getSample (0, i - 1)) ++same; chk (same > o2.getNumSamples() * .8, "bitcrusher: baja el muestreo y mantiene los valores"); }
    // ---- waveshaper y drive de kick
    { Rig r (edit, InjShaper::xmlTypeName); r.set (InjShaper::pType, 1.f); r.set (InjShaper::pDrive, .8f); auto o = r.run (sine (500, .5f, 1.0)); chk (finite (o) && thd (o, 500, .3, .9) > .05, "waveshaper: el seno agrega armonicos (THD " + juce::String (thd (o, 500, .3, .9) * 100, 1) + " %)");
      for (int t = 0; t < 4; ++t) { Rig k (edit, InjKickDrive::xmlTypeName); k.set (InjKickDrive::pChar, (float) t); k.set (InjKickDrive::pDrive, 1.f); auto ok = k.run (sine (60, .6f, 1.0)); chk (finite (ok) && peakOf (ok, .2, 1.0) < 2.5f && rmsDb (ok, .3, .9) > -25.0, "drive de kick: el caracter " + juce::String (t) + " es estable y suena"); }
      Rig k (edit, InjKickDrive::xmlTypeName); k.set (InjKickDrive::pKeep, 1.f); k.set (InjKickDrive::pDrive, 1.f); auto sub = sine (50, .5f, 1.0); auto os = k.run (sub); chk (std::abs (ampDb (os, 50, .3, .9) - ampDb (sub, 50, .3, .9)) < 4.0, "drive de kick: el sub de 50 Hz se conserva (" + juce::String (ampDb (os, 50, .3, .9) - ampDb (sub, 50, .3, .9), 1) + " dB)");
      Rig k2 (edit, InjKickDrive::xmlTypeName); k2.set (InjKickDrive::pDrive, 1.f); auto hi = sine (1500, .5f, 1.0); chk (thd (k2.run (hi), 1500, .3, .9) > .2, "drive de kick: distorsiona fuerte por encima del cruce"); }
    // ---- EQ de DJ
    { auto in = noise (.2f, 2.0); Rig r (edit, InjEqDj::xmlTypeName); auto o = r.run (in); chk (std::abs (rmsDb (o, .5, 1.9) - rmsDb (in, .5, 1.9)) < .8, "EQ de DJ: con todo a 0 dB suena plano (" + juce::String (rmsDb (o, .5, 1.9) - rmsDb (in, .5, 1.9), 2) + " dB)");
      Rig k (edit, InjEqDj::xmlTypeName); k.set (InjEqDj::pLow, -60.f); auto l = sine (60, .4f, 1.0); chk (rmsDb (k.run (l), .5, .95) - rmsDb (l, .5, .95) < -40.0, "EQ de DJ: apagar graves los elimina");
      Rig m (edit, InjEqDj::xmlTypeName); m.set (InjEqDj::pMid, -60.f); auto mm = sine (1000, .4f, 1.0); chk (rmsDb (m.run (mm), .5, .95) - rmsDb (mm, .5, .95) < -30.0, "EQ de DJ: apagar medios los elimina"); }
    // ---- auto-filtro y formantes
    { Rig a (edit, InjAutoFilter::xmlTypeName); a.set (InjAutoFilter::pEnv, 1.f); a.run (noise (.5f, .6)); const float fl = a.fx->meterA.load(); Rig b (edit, InjAutoFilter::xmlTypeName); b.set (InjAutoFilter::pEnv, 1.f); auto ob = b.run (noise (.005f, .6)); chk (fl > b.fx->meterA.load() * 2.f && finite (ob), "auto-filtro: el corte sube con la fuerza de la senal (" + juce::String (fl, 0) + " vs " + juce::String (b.fx->meterA.load(), 0) + " Hz)");
      Rig f1 (edit, InjFormant::xmlTypeName); f1.set (InjFormant::pVowel, 0.f); Rig f2 (edit, InjFormant::xmlTypeName); f2.set (InjFormant::pVowel, 4.f); auto s = sine (800, .3f, 1.0); chk (rmsDb (f1.run (s), .4, .9) > rmsDb (f2.run (s), .4, .9) + 6.0, "formantes: la A deja pasar los 800 Hz mas que la U"); }
    // ---- delay de cinta, reverso
    { Rig r (edit, InjTape::xmlTypeName); r.set (InjTape::pTime, 0.f); r.set (InjTape::pFree, 100.f); r.set (InjTape::pFb, .5f); r.set (InjTape::pWow, 0.f); r.set (InjTape::pFlut, 0.f); r.set (InjTape::pSat, 0.f); r.set (InjTape::pTone, 12000.f); r.set (InjTape::pMix, 1.f);
      auto o = r.run (impulse (1.0, .5f, 1000)); int at = 0; float bb = 0; for (int i = 3000; i < 6000; ++i) if (std::abs (o.getSample (0, i)) > bb) { bb = std::abs (o.getSample (0, i)); at = i; } chk (std::abs (at - 1000 - 4410) <= 12 && bb > .05f, "delay de cinta: el eco llega a los 100 ms (muestra " + juce::String (at - 1000) + ")");
      Rig f (edit, InjTape::xmlTypeName); f.set (InjTape::pFb, .95f); f.set (InjTape::pSat, 1.f); auto of = f.run (noise (.8f, 5.0)); chk (finite (of) && peakOf (of, 0, 5.0) < 6.f, "delay de cinta: con realimentacion al 95 % no se dispara"); }
    { Rig r (edit, InjReverse::xmlTypeName); r.set (InjReverse::pLen, 1.f); r.set (InjReverse::pMix, 1.f); r.set (InjReverse::pDry, 0.f); const int len = (int) (beatS * SR); auto o = r.run (impulse (1.5, 1.f, len / 2)); int at = 0; float bb = 0; for (int i = len; i < 2 * len; ++i) if (std::abs (o.getSample (0, i)) > bb) { bb = std::abs (o.getSample (0, i)); at = i; }
      chk (std::abs (at - (len + len / 2)) <= 6 && bb > .8f, "reverso: el golpe vuelve invertido en el siguiente pulso (muestra " + juce::String (at) + ", esperada " + juce::String (len + len / 2) + ")"); }
    // ---- autopan, tremolo, anillo
    { Rig r (edit, InjAutoPan::xmlTypeName); r.set (InjAutoPan::pDiv, 1.f); r.set (InjAutoPan::pDepth, 1.f); auto o = r.run (sine (1000, .5f, 1.5)); const double q = beatS * .25, q3 = beatS * .75;
      chk (rmsCh (o, 1, q - .004, q + .004) - rmsCh (o, 0, q - .004, q + .004) > 25.0 && rmsCh (o, 0, q3 - .004, q3 + .004) - rmsCh (o, 1, q3 - .004, q3 + .004) > 25.0, "panner automatico: viaja de un lado al otro dentro del pulso"); }
    { Rig r (edit, InjTremolo::xmlTypeName); r.set (InjTremolo::pDiv, 0.f); r.set (InjTremolo::pHz, 5.f); r.set (InjTremolo::pDepth, 1.f); auto o = r.run (sine (1000, .5f, 1.0)); double mx = -200, mn = 200; for (double t = .1; t < .8; t += .01) { const double x = rmsDb (o, t, t + .01); mx = std::max (mx, x); mn = std::min (mn, x); } chk (mx - mn > 20.0, "tremolo: el volumen oscila (" + juce::String (mx - mn, 1) + " dB de variacion)");
      Rig r0 (edit, InjTremolo::xmlTypeName); r0.set (InjTremolo::pMix, 0.f); auto in = sine (1000, .5f, .5); chk (maxDiff (r0.run (in), in) < 1e-6, "tremolo: mezcla 0 deja la senal igual"); }
    { Rig r (edit, InjRing::xmlTypeName); r.set (InjRing::pFreq, 100.f); r.set (InjRing::pShape, 0.f); r.set (InjRing::pMix, 1.f); r.set (InjRing::pOut, 0.f); auto o = r.run (sine (1000, .5f, 1.0)); chk (ampDb (o, 1100, .3, .9) > ampDb (o, 1000, .3, .9) + 15.0, "anillo: aparecen las bandas laterales 900 y 1100 Hz y se va la original"); }
    // ---- voz
    { Rig r (edit, InjPitch::xmlTypeName); r.set (InjPitch::pSt, 12.f); r.set (InjPitch::pMix, 1.f); auto o = r.run (sine (440, .4f, 1.5)); chk (amp (o, 880, .5, 1.4) > amp (o, 440, .5, 1.4) * 3.0 && amp (o, 880, .5, 1.4) > .15, "cambio de tono: +12 semitonos lleva 440 a 880 Hz (" + juce::String (amp (o, 880, .5, 1.4), 2) + ")");
      Rig r0 (edit, InjPitch::xmlTypeName); auto in = sine (300, .4f, 1.0); auto o0 = r0.run (in); const int plat = (int) std::lround (r0.fx->getLatencySeconds() * SR); chk (std::abs (rmsDb (o0, .3, .9) - rmsDb (in, .3, .9)) < 1.5 && finite (o0), "cambio de tono: a 0 semitonos conserva el nivel (" + juce::String (rmsDb (o0, .3, .9) - rmsDb (in, .3, .9), 2) + " dB, latencia " + juce::String (plat) + " muestras)"); }
    { Rig r (edit, InjTune::xmlTypeName); r.set (InjTune::pScale, 0.f); r.set (InjTune::pSpeed, 0.f); r.set (InjTune::pAmount, 1.f); auto o = r.run (sine (450, .4f, 2.0)); chk (std::abs (r.fx->meterA.load() - 450.f) < 6.f, "afinador: detecta 450 Hz (" + juce::String (r.fx->meterA.load(), 1) + ")");
      chk (amp (o, 440, 1.0, 1.9) > amp (o, 450, 1.0, 1.9) * 1.5, "afinador: lleva 450 Hz a La 440");
      Rig n (edit, InjTune::xmlTypeName); n.run (noise (.3f, 1.0)); chk (n.fx->meterA.load() == 0.f, "afinador: con ruido no detecta tono y no corrige"); }
    { Rig r (edit, InjVocoder::xmlTypeName); auto o = r.run (noise (.3f, 1.0)); chk (finite (o) && rmsDb (o, .3, .9) > -45.0, "vocoder: la voz mueve el sintetizador (" + juce::String (rmsDb (o, .3, .9), 1) + " dB)");
      Rig q (edit, InjVocoder::xmlTypeName); auto oq = q.run (zeros (1.0)); chk (rmsDb (oq, .3, .9) < -80.0, "vocoder: sin voz no suena"); }
    // ---- creativos
    { Rig r (edit, InjStutter::xmlTypeName); r.fx->applyPreset (0); auto o = r.run (sine (500, .5f, 1.0)); const double step = beatS * .25; chk (rmsDb (o, .01, step - .005) - rmsDb (o, step + .01, 2 * step - .005) > 20.0, "cortador: paso encendido suena y paso apagado calla"); }
    { Rig r (edit, InjBeatRepeat::xmlTypeName); r.set (InjBeatRepeat::pSlice, 0.f); r.set (InjBeatRepeat::pChance, 1.f); r.set (InjBeatRepeat::pDur, 2.f); r.set (InjBeatRepeat::pDecay, 0.f); r.set (InjBeatRepeat::pMix, 1.f); auto in = noise (.5f, 1.5, 4); auto o = r.run (in); const int len = (int) (.5 * beatS * SR); double best = 1e9; const int N0 = (int) std::lround (beatS * SR); for (int N = N0 - 3; N <= N0 + 3; ++N) best = std::min (best, (double) std::abs (o.getSample (0, N + 1000) - in.getSample (0, N + 1000 - len)));
      chk (best < 1e-4, "repetidor: repite el trozo anterior sin cambiarlo (error " + juce::String (best, 6) + ")"); }
    { Rig r (edit, InjTapeStop::xmlTypeName); r.set (InjTapeStop::pStop, 1.f); r.set (InjTapeStop::pTime, .3f); auto in = sine (1000, .5f, 1.0); auto o = r.run (in); chk (rmsDb (o, 0, .05) > -12.0 && rmsDb (o, .6, .95) < -60.0, "parada de cinta: se frena hasta el silencio (" + juce::String (rmsDb (o, .6, .95), 0) + " dB)");
      r.set (InjTapeStop::pStop, 0.f); auto o2 = r.run (sine (1000, .5f, 2.0)); chk (std::abs (rmsDb (o2, 1.5, 1.95) - rmsDb (in, .2, .6)) < 1.0 && finite (o2), "parada de cinta: al soltar vuelve a sonar normal"); }
    { Rig r (edit, InjGranular::xmlTypeName); r.set (InjGranular::pMix, 1.f); auto o = r.run (sine (440, .4f, 1.5)); chk (finite (o) && rmsDb (o, .6, 1.4) > -30.0, "granular: nube de granos audible (" + juce::String (rmsDb (o, .6, 1.4), 1) + " dB)");
      r.set (InjGranular::pFreeze, 1.f); auto o2 = r.run (zeros (1.0)); chk (rmsDb (o2, .3, .9) > -40.0, "granular: congelar mantiene el sonido sin entrada (" + juce::String (rmsDb (o2, .3, .9), 1) + " dB)"); }
    { Rig r (edit, InjGlitch::xmlTypeName); r.set (InjGlitch::pType, 4.f); r.set (InjGlitch::pChance, 1.f); auto o = r.run (sine (500, .5f, 1.0)); chk (rmsDb (o, .3, .9) < -70.0, "glitch: corte con probabilidad 100 % deja silencio");
      Rig r0 (edit, InjGlitch::xmlTypeName); r0.set (InjGlitch::pChance, 0.f); auto in = noise (.3f, .8); chk (maxDiff (r0.run (in), in) < 1e-6, "glitch: probabilidad 0 deja el sonido igual");
      Rig r1 (edit, InjGlitch::xmlTypeName); r1.set (InjGlitch::pChance, 1.f); r1.set (InjGlitch::pType, 2.f); auto o1 = r1.run (noise (.3f, 1.5)); chk (finite (o1) && peakOf (o1, 0, 1.5) < 1.f, "glitch: el reverso es estable"); }
    // ---- utilidad
    { auto in = stereoOf (noise (.2f, .6, 1), noise (.2f, .6, 2)); Rig g (edit, InjGainPhase::xmlTypeName); g.set (InjGainPhase::pGain, 6.f); chk (std::abs (rmsDb (g.run (in), .1, .5) - rmsDb (in, .1, .5) - 6.02) < .05, "ganancia: +6 dB sube 6 dB");
      Rig p (edit, InjGainPhase::xmlTypeName); p.set (InjGainPhase::pPolL, 1.f); auto op = p.run (in); chk (std::abs (op.getSample (0, 1000) + in.getSample (0, 1000)) < 1e-6 && std::abs (op.getSample (1, 1000) - in.getSample (1, 1000)) < 1e-6, "ganancia: invertir fase izquierda");
      Rig s (edit, InjGainPhase::xmlTypeName); s.set (InjGainPhase::pSwap, 1.f); auto os = s.run (in); chk (std::abs (os.getSample (0, 1000) - in.getSample (1, 1000)) < 1e-6, "ganancia: cambiar lados");
      Rig m (edit, InjGainPhase::xmlTypeName); m.set (InjGainPhase::pMono, 1.f); auto om = m.run (in); chk (std::abs (om.getSample (0, 1000) - om.getSample (1, 1000)) < 1e-6, "ganancia: mono iguala los dos lados"); }
    { auto mono = stereoOf (sine (1000, .3f, 1.0), sine (1000, .3f, 1.0)); Buf side (2, (int) SR); for (int i = 0; i < side.getNumSamples(); ++i) { const float x = .3f * std::sin (2.f * kPi * 1000.f * i / (float) SR); side.setSample (0, i, x); side.setSample (1, i, -x); }
      Rig a (edit, InjMidSide::xmlTypeName); a.set (InjMidSide::pMid, -12.f); chk (std::abs (rmsDb (a.run (mono), .3, .9) - rmsDb (mono, .3, .9) + 12.0) < .1 && std::abs (rmsDb (a.run (side), .3, .9) - rmsDb (side, .3, .9)) < .1, "mid/side: bajar el centro solo toca lo central");
      Rig b (edit, InjMidSide::xmlTypeName); b.set (InjMidSide::pSide, 12.f); chk (std::abs (rmsDb (b.run (side), .3, .9) - rmsDb (side, .3, .9) - 12.0) < .1 && std::abs (rmsDb (b.run (mono), .3, .9) - rmsDb (mono, .3, .9)) < .1, "mid/side: subir los lados solo toca lo lateral"); }
    { Rig r (edit, InjSpectrum::xmlTypeName); auto in = sine (1000, 1.f, 1.0); auto o = r.run (in); chk (maxDiff (o, in) < 1e-7, "espectro: no cambia el sonido");
      auto* sp = dynamic_cast<InjSpectrum*> (r.fx); std::vector<float> db; sp->analyse (db); int kmax = 1; for (int k = 1; k < (int) db.size(); ++k) if (db[(size_t) k] > db[(size_t) kmax]) kmax = k; const float hz = kmax * (float) SR / InjSpectrum::N;
      chk (std::abs (hz - 1000.f) < 25.f && db[(size_t) kmax] > -3.0f && db[(size_t) kmax] < 1.0f, "espectro: un seno de 1 kHz a 0 dB marca " + juce::String (hz, 0) + " Hz y " + juce::String (db[(size_t) kmax], 1) + " dB"); }
    { Rig r (edit, InjLufs::xmlTypeName); auto* lf = dynamic_cast<InjLufs*> (r.fx); r.run (stereoOf (sine (1000, db2lin (-23.f), 5.0), sine (1000, db2lin (-23.f), 5.0))); chk (std::abs (lf->integrated() + 23.f) < .4f, "LUFS: un seno de 1 kHz a -23 dBFS en estereo mide " + juce::String (lf->integrated(), 2) + " LUFS");
      r.run (stereoOf (sine (1000, db2lin (-80.f), 3.0), sine (1000, db2lin (-80.f), 3.0))); chk (std::abs (lf->integrated() + 23.f) < .4f && lf->momentary() < -60.f, "LUFS: el silencio no baja la medida integrada (puerta)");
      Rig q (edit, InjLufs::xmlTypeName); auto* lq = dynamic_cast<InjLufs*> (q.fx); q.run (stereoOf (sine (1000, db2lin (-10.f), 2.0), sine (1000, db2lin (-10.f), 2.0))); chk (lq->momentary() > r.fx->specs()[0].lo + 0.f && std::abs (lq->momentary() + 10.f) < .5f, "LUFS: el momentaneo marca " + juce::String (lq->momentary(), 2) + " con -10 dBFS"); }
    // ---- Punch Comp (compresor FET con panel de equipo)
    { auto in = sine (1000, .5f, 1.5);
      Rig a (edit, InjPunch::xmlTypeName); a.set (InjPunch::pIn, 6.f); a.set (InjPunch::pAtk, 0.f); a.set (InjPunch::pRel, 3.f); a.set (InjPunch::pOut, 0.f); a.set (InjPunch::pRatio, 3.f); auto oa = a.run (in);
      const float ga = a.fx->meterA.load(); chk (ga > 6.f, "punch comp: con entrada alta y 4:1 comprime (" + juce::String (ga, 1) + " dB)");
      Rig b (edit, InjPunch::xmlTypeName); b.set (InjPunch::pIn, 6.f); b.set (InjPunch::pAtk, 0.f); b.set (InjPunch::pRel, 3.f); b.set (InjPunch::pOut, 0.f); b.set (InjPunch::pRatio, 6.f); b.run (in);
      chk (b.fx->meterA.load() > ga + 1.f, "punch comp: 20:1 comprime mas que 4:1 (" + juce::String (b.fx->meterA.load(), 1) + " vs " + juce::String (ga, 1) + " dB)");
      Rig c (edit, InjPunch::xmlTypeName); c.set (InjPunch::pIn, 6.f); c.set (InjPunch::pRatio, 0.f); c.run (in); chk (c.fx->meterA.load() < .3f, "punch comp: 1:1 no comprime");
      Rig d (edit, InjPunch::xmlTypeName); d.set (InjPunch::pIn, 8.f); d.set (InjPunch::pOut, 0.f); d.set (InjPunch::pByp, 1.f); auto od = d.run (in); chk (std::abs (rmsDb (od, 1.0, 1.4) - rmsDb (in, 1.0, 1.4)) < .05, "punch comp: bypass deja el sonido igual");
      auto lo = sine (60, .5f, 1.5); Rig e0 (edit, InjPunch::xmlTypeName); e0.set (InjPunch::pIn, 6.f); e0.set (InjPunch::pRatio, 3.f); e0.run (lo); Rig e1 (edit, InjPunch::xmlTypeName); e1.set (InjPunch::pIn, 6.f); e1.set (InjPunch::pRatio, 3.f); e1.set (InjPunch::pDet, 1.f); e1.run (lo);
      chk (e0.fx->meterA.load() > 6.f && e1.fx->meterA.load() < e0.fx->meterA.load() - 4.f, "punch comp: el filtro HP del detector ignora los graves (" + juce::String (e0.fx->meterA.load(), 1) + " dB libre, " + juce::String (e1.fx->meterA.load(), 1) + " dB con HP)");
      Rig f0 (edit, InjPunch::xmlTypeName); f0.set (InjPunch::pIn, 6.f); f0.set (InjPunch::pOut, 4.f); auto of0 = f0.run (in); Rig f1 (edit, InjPunch::xmlTypeName); f1.set (InjPunch::pIn, 6.f); f1.set (InjPunch::pOut, 4.f); f1.set (InjPunch::pSat, 3.f); auto of1 = f1.run (in);
      float dif = 0.f; for (int i = 20000; i < 40000; ++i) dif = std::max (dif, std::abs (of0.getSample (0, i) - of1.getSample (0, i))); chk (dif > .02f, "punch comp: la saturacion NUKE cambia la onda (" + juce::String (dif, 3) + ")");
      auto ed = a.fx->createEditor(); chk (ed != nullptr && ed->getWidth() > 800 && ed->getHeight() > 250, "punch comp: la ventana con el panel se crea (" + juce::String (ed != nullptr ? ed->getWidth() : 0) + "x" + juce::String (ed != nullptr ? ed->getHeight() : 0) + ")"); }
    // ---- FET Compressor (panel de equipo)
    { auto in = sine (1000, .5f, 1.5); auto qin = sine (1000, .01f, 1.5);
      Rig a (edit, InjFet::xmlTypeName); a.set (InjFet::pIn, 30.f); a.set (InjFet::pAtk, 5.f); a.set (InjFet::pRel, 4.f); a.set (InjFet::pRatio, 0.f); a.run (in); const float g4 = a.fx->meterA.load();
      Rig b (edit, InjFet::xmlTypeName); b.set (InjFet::pIn, 30.f); b.set (InjFet::pAtk, 5.f); b.set (InjFet::pRel, 4.f); b.set (InjFet::pRatio, 3.f); b.run (in); const float g20 = b.fx->meterA.load();
      chk (g4 > 8.f && g20 > g4 + 1.f, "fet: 20:1 comprime mas que 4:1 (" + juce::String (g20, 1) + " vs " + juce::String (g4, 1) + " dB)");
      Rig c (edit, InjFet::xmlTypeName); c.set (InjFet::pIn, 6.f); c.set (InjFet::pOut, 24.f); c.run (qin); chk (c.fx->meterA.load() < .3f, "fet: con poca entrada no comprime");
      Rig d (edit, InjFet::xmlTypeName); d.set (InjFet::pIn, 0.f); d.set (InjFet::pOut, 24.f); auto od = d.run (qin); chk (std::abs (rmsDb (od, 1.0, 1.4) - rmsDb (qin, 1.0, 1.4)) < .1, "fet: entrada 0 y salida 24 dejan el nivel igual");
      Rig e (edit, InjFet::xmlTypeName); e.set (InjFet::pIn, 30.f); e.set (InjFet::pOut, 12.f); auto oe = e.run (in); Rig f (edit, InjFet::xmlTypeName); f.set (InjFet::pIn, 30.f); f.set (InjFet::pOut, 36.f); auto of = f.run (in);
      chk (std::abs ((rmsDb (of, 1.0, 1.4) - rmsDb (oe, 1.0, 1.4)) - 24.0) < 1.0, "fet: la salida sube 24 dB al pasar de 12 a 36 (" + juce::String (rmsDb (of, 1.0, 1.4) - rmsDb (oe, 1.0, 1.4), 1) + ")");
      auto ed = a.fx->createEditor(); chk (ed != nullptr && ed->getWidth() > 800 && ed->getHeight() > 250, "fet: la ventana con el panel se crea"); }
    // ---- Glue Comp
    { auto in = sine (1000, .5f, 1.5);
      Rig a (edit, InjGlue::xmlTypeName); a.set (InjGlue::pThr, -24.f); a.set (InjGlue::pRatio, 1.f); a.set (InjGlue::pAtk, 1.f); a.set (InjGlue::pRel, .3f); a.set (InjGlue::pMk, 0.f); auto oa = a.run (in);
      const double g = rmsDb (oa, 1.0, 1.4) - rmsDb (in, 1.0, 1.4); chk (std::abs (g + 13.5) < 1.5, "glue: -24 dB, 4:1, entrada -6 dB baja ~13.5 dB (" + juce::String (g, 2) + ")");
      Rig b (edit, InjGlue::xmlTypeName); b.set (InjGlue::pThr, -24.f); b.set (InjGlue::pRatio, 2.f); b.set (InjGlue::pAtk, 1.f); b.set (InjGlue::pRel, .3f); b.run (in); chk (b.fx->meterA.load() > a.fx->meterA.load() + 1.f, "glue: 10:1 comprime mas que 4:1");
      Rig c (edit, InjGlue::xmlTypeName); c.set (InjGlue::pIn, 0.f); c.set (InjGlue::pThr, -30.f); auto oc = c.run (in); chk (std::abs (rmsDb (oc, 1.0, 1.4) - rmsDb (in, 1.0, 1.4)) < .05, "glue: con IN apagado no toca la senal");
      Rig d (edit, InjGlue::xmlTypeName); d.set (InjGlue::pThr, 0.f); d.set (InjGlue::pMk, 6.f); auto od = d.run (in); chk (std::abs ((rmsDb (od, 1.0, 1.4) - rmsDb (in, 1.0, 1.4)) - 6.0) < .3, "glue: la ganancia de salida suma 6 dB");
      Rig e (edit, InjGlue::xmlTypeName); e.set (InjGlue::pThr, 0.f); e.set (InjGlue::pFade, 1.f); auto oe = e.run (sine (1000, .5f, 6.0)); chk (rmsDb (oe, 5.5, 5.9) < rmsDb (oe, 0.1, .5) - 30.0, "glue: el fundido baja la senal a silencio en 5 s");
      auto ed = a.fx->createEditor(); chk (ed != nullptr && ed->getHeight() > 600 && ed->getWidth() > 400, "glue: la ventana con el panel se crea"); }
    // ---- Optical Comp
    { auto in = sine (1000, .5f, 1.5);
      Rig a (edit, InjOptical::xmlTypeName); a.set (InjOptical::pPeak, 50.f); a.set (InjOptical::pGain, 0.f); a.set (InjOptical::pOut, 10.f); a.run (in); const float gc = a.fx->meterA.load();
      Rig b (edit, InjOptical::xmlTypeName); b.set (InjOptical::pPeak, 50.f); b.set (InjOptical::pGain, 0.f); b.set (InjOptical::pLimit, 1.f); b.run (in); const float gl = b.fx->meterA.load();
      chk (gc > 6.f && gl > gc + 2.f, "optical: Limit comprime mas que Compress (" + juce::String (gl, 1) + " vs " + juce::String (gc, 1) + " dB)");
      Rig c (edit, InjOptical::xmlTypeName); c.set (InjOptical::pPeak, 0.f); c.set (InjOptical::pGain, 0.f); c.set (InjOptical::pOut, 10.f); auto oc = c.run (in); chk (std::abs (rmsDb (oc, 1.0, 1.4) - rmsDb (in, 1.0, 1.4)) < .4, "optical: sin Peak Reduction casi no toca la senal (" + juce::String (rmsDb (oc, 1.0, 1.4) - rmsDb (in, 1.0, 1.4), 2) + ")");
      Rig d (edit, InjOptical::xmlTypeName); d.set (InjOptical::pPeak, 0.f); d.set (InjOptical::pGain, 40.f); auto od = d.run (in); chk (std::abs ((rmsDb (od, 1.0, 1.4) - rmsDb (oc, 1.0, 1.4)) - 10.0) < .6, "optical: Gain 40 suma 10 dB");
      Rig e (edit, InjOptical::xmlTypeName); e.set (InjOptical::pPeak, 0.f); e.set (InjOptical::pGain, 0.f); e.set (InjOptical::pOut, 5.f); auto oe = e.run (in); chk (std::abs ((rmsDb (oc, 1.0, 1.4) - rmsDb (oe, 1.0, 1.4)) - 12.0) < .6, "optical: Output 5 baja 12 dB");
      auto ed = a.fx->createEditor(); chk (ed != nullptr && ed->getWidth() > 800, "optical: la ventana con el panel se crea"); }
    // ---- Dual Comp
    { auto in = sine (1000, .5f, 1.5); auto qin = sine (1000, .01f, 1.5);
      Rig a (edit, InjDual::xmlTypeName); a.set (InjDual::pIn0, 12.f); a.set (InjDual::pIn1, 12.f); a.set (InjDual::pThr0, -30.f); a.set (InjDual::pThr1, -30.f); a.set (InjDual::pTc0, 0.f); a.set (InjDual::pTc1, 0.f); a.run (in);
      chk (a.fx->meterA.load() > 6.f && a.fx->meterB.load() > 6.f, "dual: izquierda-derecha comprime los dos canales (" + juce::String (a.fx->meterA.load(), 1) + " y " + juce::String (a.fx->meterB.load(), 1) + " dB)");
      Rig b (edit, InjDual::xmlTypeName); b.set (InjDual::pIn0, 12.f); b.set (InjDual::pIn1, 12.f); b.set (InjDual::pThr0, -30.f); b.set (InjDual::pThr1, -30.f); b.set (InjDual::pTc0, 0.f); b.set (InjDual::pTc1, 0.f); b.set (InjDual::pMode, 1.f); b.run (in);
      chk (b.fx->meterA.load() > 6.f && b.fx->meterB.load() < .3f, "dual: lateral-vertical con senal mono solo comprime el centro (" + juce::String (b.fx->meterA.load(), 1) + " y " + juce::String (b.fx->meterB.load(), 1) + " dB)");
      Rig c (edit, InjDual::xmlTypeName); c.set (InjDual::pIn0, 0.f); c.set (InjDual::pIn1, 0.f); c.set (InjDual::pThr0, 0.f); c.set (InjDual::pThr1, 0.f); auto oc = c.run (qin); chk (std::abs (rmsDb (oc, 1.0, 1.4) - rmsDb (qin, 1.0, 1.4)) < .1, "dual: entrada 0 y umbral 0 dejan el nivel igual");
      Rig d (edit, InjDual::xmlTypeName); d.set (InjDual::pIn0, 12.f); d.set (InjDual::pThr0, -30.f); d.set (InjDual::pOn, 0.f); auto od = d.run (in); chk (std::abs (rmsDb (od, 1.0, 1.4) - rmsDb (in, 1.0, 1.4)) < .01, "dual: apagado deja la senal intacta");
      Rig e (edit, InjDual::xmlTypeName); e.set (InjDual::pIn0, 12.f); e.set (InjDual::pThr0, -30.f); e.set (InjDual::pTc0, 0.f); e.run (in); const float r1 = e.fx->meterA.load();
      Rig f (edit, InjDual::xmlTypeName); f.set (InjDual::pIn0, 12.f); f.set (InjDual::pThr0, -10.f); f.set (InjDual::pTc0, 0.f); f.run (in); chk (r1 > f.fx->meterA.load() + 3.f, "dual: un umbral mas bajo comprime mas");
      auto ed = a.fx->createEditor(); chk (ed != nullptr && ed->getWidth() > 700 && ed->getHeight() > 350, "dual: la ventana con el panel se crea"); }
    // ---- Pulso EQ
    { auto gainAt = [&] (std::initializer_list<std::pair<int, float>> ps, float hz) { auto in = sine (hz, .1f, 1.5); Rig r (edit, InjPulso::xmlTypeName); for (auto& q : ps) r.set (q.first, q.second); auto o = r.run (in); return rmsDb (o, 1.0, 1.4) - rmsDb (in, 1.0, 1.4); };
      chk (gainAt ({ { InjPulso::pHfB, 10.f }, { InjPulso::pKcs, 3.f } }, 8000.f) > 10.f, "pulso: boost de agudos a 8 kHz sube mas de 10 dB");
      chk (gainAt ({ { InjPulso::pHfA, 10.f }, { InjPulso::pAsel, 1.f } }, 14000.f) < -8.f, "pulso: atten de agudos a 10 kHz baja mas de 8 dB");
      chk (gainAt ({ { InjPulso::pLfB, 10.f }, { InjPulso::pCps, 2.f } }, 50.f) > 8.f && std::abs (gainAt ({ { InjPulso::pLfB, 10.f }, { InjPulso::pCps, 2.f } }, 1000.f)) < .6f, "pulso: boost de graves a 60 Hz sube y no toca los 1000 Hz");
      chk (gainAt ({ { InjPulso::pHfB, 10.f }, { InjPulso::pKcs, 2.f }, { InjPulso::pBw, 10.f } }, 3500.f) > gainAt ({ { InjPulso::pHfB, 10.f }, { InjPulso::pKcs, 2.f }, { InjPulso::pBw, 1.f } }, 3500.f) + 3.f, "pulso: ancho de banda amplio abre mas que el estrecho");
      chk (std::abs (gainAt ({ { InjPulso::pHfB, 10.f }, { InjPulso::pIn, 0.f } }, 5000.f)) < .01f && std::abs (gainAt ({ { InjPulso::pHfB, 10.f }, { InjPulso::pOn, 0.f } }, 5000.f)) < .01f, "pulso: fuera y apagado dejan la senal intacta");
      Rig w (edit, InjPulso::xmlTypeName); auto ed = w.fx->createEditor(); chk (ed != nullptr && ed->getWidth() > 900 && ed->getHeight() > 250, "pulso: la ventana con el panel se crea"); }
    // ---- Classic EQ
    { auto gainAt = [&] (std::initializer_list<std::pair<int, float>> ps, float hz) { auto in = sine (hz, .1f, 1.5); Rig r (edit, InjClassic::xmlTypeName); for (auto& q : ps) r.set (q.first, q.second); auto o = r.run (in); return rmsDb (o, 1.0, 1.4) - rmsDb (in, 1.0, 1.4); };
      chk (std::abs (gainAt ({ { InjClassic::pHmG, 9.f }, { InjClassic::pHmF, 1.f } }, 1000.f) - 9.f) < .8f, "classic: HMF +9 dB a 1 kHz sube 9 dB (" + juce::String (gainAt ({ { InjClassic::pHmG, 9.f }, { InjClassic::pHmF, 1.f } }, 1000.f), 1) + ")");
      chk (gainAt ({ { InjClassic::pLfG, -9.f }, { InjClassic::pLfF, 2.f }, { InjClassic::pLfShelf, 1.f } }, 40.f) < -6.f, "classic: LF shelf -9 dB baja los graves");
      chk (gainAt ({ { InjClassic::pHfG, 9.f }, { InjClassic::pHfF, 3.f }, { InjClassic::pHfShelf, 1.f } }, 16000.f) > 6.f && gainAt ({ { InjClassic::pHfG, 9.f }, { InjClassic::pHfF, 3.f }, { InjClassic::pHfShelf, 0.f } }, 16000.f) < gainAt ({ { InjClassic::pHfG, 9.f }, { InjClassic::pHfF, 3.f }, { InjClassic::pHfShelf, 1.f } }, 16000.f) - 3.f, "classic: HF shelf sube los agudos mas que el pico");
      chk (std::abs (gainAt ({ { InjClassic::pHmG, 9.f }, { InjClassic::pIn, 0.f } }, 1000.f)) < .01f, "classic: fuera deja la senal intacta");
      Rig w (edit, InjClassic::xmlTypeName); auto ed = w.fx->createEditor(); chk (ed != nullptr && ed->getWidth() > 250 && ed->getHeight() > 650, "classic: la ventana con el panel se crea (" + juce::String (ed != nullptr ? ed->getWidth() : 0) + "x" + juce::String (ed != nullptr ? ed->getHeight() : 0) + ")"); }
    // ---- Airlift EQ
    { auto gainAt = [&] (std::initializer_list<std::pair<int, float>> ps, float hz) { auto in = sine (hz, .1f, 1.5); Rig r (edit, InjAirlift::xmlTypeName); for (auto& q : ps) r.set (q.first, q.second); auto o = r.run (in); return rmsDb (o, 1.0, 1.4) - rmsDb (in, 1.0, 1.4); };
      chk (gainAt ({ { InjAirlift::p25, 10.f } }, 2500.f) > 12.f, "airlift: 2.5 kHz al maximo sube mas de 12 dB");
      chk (gainAt ({ { InjAirlift::pAir, 10.f }, { InjAirlift::pAirF, 3.f } }, 16000.f) > 10.f && gainAt ({ { InjAirlift::pAir, 10.f }, { InjAirlift::pAirF, 0.f } }, 16000.f) < .3f, "airlift: la banda de aire sube los agudos y apagada no hace nada");
      chk (gainAt ({ { InjAirlift::pSub, 10.f } }, 25.f) > 8.f && std::abs (gainAt ({ { InjAirlift::pSub, 10.f } }, 3000.f)) < .5f, "airlift: sub sube los graves y deja los medios (" + juce::String (gainAt ({ { InjAirlift::pSub, 10.f } }, 25.f), 1) + " y " + juce::String (gainAt ({ { InjAirlift::pSub, 10.f } }, 3000.f), 1) + ")");
      chk (std::abs (gainAt ({ { InjAirlift::p160, 10.f }, { InjAirlift::pIn, 0.f } }, 160.f)) < .01f, "airlift: fuera deja la senal intacta");
      { auto in = sine (1000, .9f, 1.0); Rig r (edit, InjAirlift::xmlTypeName); r.run (in); chk (r.fx->meterA.load() > -2.f, "airlift: el medidor de pico sigue la salida (" + juce::String (r.fx->meterA.load(), 1) + " dB)"); }
      Rig w (edit, InjAirlift::xmlTypeName); auto ed = w.fx->createEditor(); chk (ed != nullptr && ed->getWidth() > 250 && ed->getHeight() > 650, "airlift: la ventana con el panel se crea"); }
    // ---- Black Brown EQ
    { auto gainAt = [&] (std::initializer_list<std::pair<int, float>> ps, float hz) { auto in = sine (hz, .1f, 1.5); Rig r (edit, InjBlackBrown::xmlTypeName); for (auto& q : ps) r.set (q.first, q.second); auto o = r.run (in); return rmsDb (o, 1.0, 1.4) - rmsDb (in, 1.0, 1.4); };
      chk (gainAt ({ { InjBlackBrown::pHmG, 9.f }, { InjBlackBrown::pHmF, 2000.f } }, 2000.f) > 8.f, "blackbrown: HMF +9 dB sube en su frecuencia");
      chk (gainAt ({ { InjBlackBrown::pLfG, 12.f }, { InjBlackBrown::pLfF, 100.f } }, 30.f) > 8.f && std::abs (gainAt ({ { InjBlackBrown::pLfG, 12.f }, { InjBlackBrown::pLfF, 100.f } }, 3000.f)) < .5f, "blackbrown: LF shelf sube los graves y deja los medios");
      chk (gainAt ({ { InjBlackBrown::pHpf, 4.f } }, 40.f) < -10.f && std::abs (gainAt ({ { InjBlackBrown::pHpf, 4.f } }, 2000.f)) < .3f, "blackbrown: HPF 160 Hz corta los graves");
      chk (gainAt ({ { InjBlackBrown::pLpf, 2.f } }, 10000.f) < -10.f && std::abs (gainAt ({ { InjBlackBrown::pLpf, 2.f } }, 500.f)) < .3f, "blackbrown: LPF 3 kHz corta los agudos");
      chk (gainAt ({ { InjBlackBrown::pHmG, 12.f }, { InjBlackBrown::pHmF, 2000.f }, { InjBlackBrown::pType, 1.f } }, 3000.f) > gainAt ({ { InjBlackBrown::pHmG, 12.f }, { InjBlackBrown::pHmF, 2000.f }, { InjBlackBrown::pType, 0.f } }, 3000.f) + .3f, "blackbrown: Brown es mas ancho que Black");
      chk (std::abs (gainAt ({ { InjBlackBrown::pHmG, 9.f }, { InjBlackBrown::pBypass, 1.f } }, 2000.f)) < .01f && std::abs (gainAt ({ { InjBlackBrown::pHmG, 9.f }, { InjBlackBrown::pDyn, 1.f } }, 2000.f)) < .01f, "blackbrown: bypass y Dyn S-C dejan el audio intacto");
      Rig w (edit, InjBlackBrown::xmlTypeName); auto ed = w.fx->createEditor(); chk (ed != nullptr && ed->getWidth() > 300 && ed->getHeight() > 650, "blackbrown: la ventana con el panel se crea"); }
    // ---- Master EQ
    { auto chGain = [&] (std::initializer_list<std::pair<int, float>> ps, float hz, int ch) { auto in = sine (hz, .1f, 1.5); Rig r (edit, InjMaster::xmlTypeName); for (auto& q : ps) r.set (q.first, q.second); auto o = r.run (in);
          auto rms = [&] (const Buf& b) { double s = 0; int n = 0; for (int i = (int) (1.0 * SR); i < (int) (1.4 * SR); ++i) { const double x = b.getSample (ch, i); s += x * x; ++n; } return 10.0 * std::log10 (s / n + 1e-20); }; return rms (o) - rms (in); };
      chk (chGain ({ { InjMaster::G (0, 0), 12.f }, { InjMaster::SH (0, 0), 0.f } }, 100.f, 0) > 8.f && std::abs (chGain ({ { InjMaster::G (0, 0), 12.f }, { InjMaster::SH (0, 0), 0.f } }, 100.f, 1)) < .1f, "master: el canal izquierdo se ecualiza sin tocar el derecho");
      chk (chGain ({ { InjMaster::G (1, 2), 9.f }, { InjMaster::F (1, 2), 1.f } }, 3000.f, 1) > 7.f && std::abs (chGain ({ { InjMaster::G (1, 2), 9.f }, { InjMaster::F (1, 2), 1.f } }, 3000.f, 0)) < .1f, "master: el canal derecho se ecualiza sin tocar el izquierdo");
      chk (chGain ({ { InjMaster::G (0, 1), 10.f }, { InjMaster::MU (0, 1), 1.f } }, 1500.f, 0) < .1f, "master: rango 'Fuera' anula la banda");
      { const float a = (float) chGain ({ { InjMaster::G (0, 1), 6.f }, { InjMaster::F (0, 1), 1.f } }, 1500.f, 0), b2 = (float) chGain ({ { InjMaster::G (0, 1), 6.f }, { InjMaster::F (0, 1), 1.f }, { InjMaster::MU (0, 1), 2.f } }, 1500.f, 0); chk (std::abs (b2 - a * 1.5f) < .8f, "master: rango x1.5 multiplica la ganancia (" + juce::String (a, 1) + " -> " + juce::String (b2, 1) + ")"); }
      chk (chGain ({ { InjMaster::HP (0), 3.f } }, 40.f, 0) < -8.f && std::abs (chGain ({ { InjMaster::HP (0), 3.f } }, 40.f, 1)) < .1f, "master: pasaaltos 100 Hz corta solo el canal izquierdo");
      chk (chGain ({ { InjMaster::HP (0), 3.f }, { InjMaster::HPI (0), 0.f } }, 40.f, 0) > -.1f, "master: boton IN apagado deja pasar el pasaaltos");
      chk (chGain ({ { InjMaster::LP (1), 1.f } }, 10000.f, 1) < -10.f, "master: pasabajos 3 kHz corta los agudos");
      chk (std::abs (chGain ({ { InjMaster::OUT (0), 6.f } }, 1000.f, 0) - 6.f) < .2f, "master: salida +6 dB");
      chk (std::abs (chGain ({ { InjMaster::G (0, 0), 12.f }, { InjMaster::pOn, 0.f } }, 100.f, 0)) < .01f, "master: apagado deja la senal intacta");
      Rig w (edit, InjMaster::xmlTypeName); auto ed = w.fx->createEditor(); chk (ed != nullptr && ed->getWidth() > 900 && ed->getHeight() > 300, "master: la ventana con el panel se crea"); }
    // ---- Dual Gate
    { auto loud = sine (1000, .5f, 1.5); auto quiet = sine (1000, .01f, 1.5);
      Rig a (edit, InjGateDS::xmlTypeName); a.set (InjGateDS::THR (0), -20.f); a.set (InjGateDS::THR (1), -20.f); auto oa = a.run (loud); chk (std::abs (rmsDb (oa, 1.0, 1.4) - rmsDb (loud, 1.0, 1.4)) < .6, "gate: la senal fuerte pasa");
      Rig b (edit, InjGateDS::xmlTypeName); b.set (InjGateDS::THR (0), -20.f); b.set (InjGateDS::THR (1), -20.f); auto ob = b.run (quiet); chk (rmsDb (ob, 1.0, 1.4) < rmsDb (quiet, 1.0, 1.4) - 20.0, "gate: la senal por debajo del umbral se corta mas de 20 dB (" + juce::String (rmsDb (ob, 1.0, 1.4) - rmsDb (quiet, 1.0, 1.4), 1) + ")");
      Rig c (edit, InjGateDS::xmlTypeName); c.set (InjGateDS::THR (0), -20.f); c.set (InjGateDS::INN (0), 0.f); c.set (InjGateDS::INN (1), 0.f); auto oc = c.run (quiet); chk (std::abs (rmsDb (oc, 1.0, 1.4) - rmsDb (quiet, 1.0, 1.4)) < .05, "gate: con IN apagado no toca la senal");
      Rig d (edit, InjGateDS::xmlTypeName); d.set (InjGateDS::THR (0), -40.f); auto od = d.run (quiet); chk (d.fx->meterA.load() > -50.f, "gate: el medidor de la cadena lateral se mueve");
      auto ed = a.fx->createEditor(); chk (ed != nullptr && ed->getWidth() > 800, "gate: la ventana con el panel se crea"); }
    // ---- De-Esser
    { auto hi = sine (8000, .3f, 1.5); auto lo = sine (200, .3f, 1.5);
      Rig a (edit, InjDeEsser::xmlTypeName); a.set (InjDeEsser::pFreq, 8000.f); a.set (InjDeEsser::pRange, 15.f); auto oa = a.run (hi); chk (rmsDb (oa, 1.0, 1.4) < rmsDb (hi, 1.0, 1.4) - 5.0, "de-esser: baja la sibilancia de 8 kHz (" + juce::String (rmsDb (oa, 1.0, 1.4) - rmsDb (hi, 1.0, 1.4), 1) + ")");
      Rig b (edit, InjDeEsser::xmlTypeName); b.set (InjDeEsser::pFreq, 8000.f); b.set (InjDeEsser::pRange, 15.f); auto ob = b.run (lo); chk (std::abs (rmsDb (ob, 1.0, 1.4) - rmsDb (lo, 1.0, 1.4)) < .8, "de-esser: no toca los graves");
      Rig c (edit, InjDeEsser::xmlTypeName); c.set (InjDeEsser::pFreq, 8000.f); c.set (InjDeEsser::pListen, 1.f); auto oc = c.run (lo); chk (rmsDb (oc, 1.0, 1.4) < rmsDb (lo, 1.0, 1.4) - 20.0, "de-esser: escuchar la cadena lateral solo deja la banda");
      Rig d (edit, InjDeEsser::xmlTypeName); d.set (InjDeEsser::pOut, 6.f); d.set (InjDeEsser::pRange, 0.f); auto od = d.run (lo); chk (std::abs ((rmsDb (od, 1.0, 1.4) - rmsDb (lo, 1.0, 1.4)) - 6.0) < .4, "de-esser: la salida suma 6 dB");
      auto ed = a.fx->createEditor(); chk (ed != nullptr && ed->getWidth() > 800, "de-esser: la ventana con el panel se crea"); }
    // ---- Tube Saturation
    { using T = InjTubeSat; auto lo = sine (1000, .05f, 1.5), hi = sine (1000, .1f, 1.5), big = sine (1000, .4f, 1.5), tone10 = sine (10000, .05f, 1.5);
      Rig a (edit, T::xmlTypeName); a.set (T::P (0, T::oDrive), 0.f); a.set (T::P (0, T::oOut), 5.f); auto oa = a.run (lo), ob = a.run (hi); chk (std::abs ((rmsDb (ob, 1.0, 1.4) - rmsDb (oa, 1.0, 1.4)) - 6.0) < .6, "tube sat: con drive 0 y senal baja es casi lineal");
      Rig b (edit, T::xmlTypeName); b.set (T::P (0, T::oDrive), 10.f); auto oc = b.run (lo), od = b.run (big); chk (rmsDb (od, 1.0, 1.4) - rmsDb (oc, 1.0, 1.4) < 12.0, "tube sat: con drive 10 comprime la senal fuerte (" + juce::String (rmsDb (od, 1.0, 1.4) - rmsDb (oc, 1.0, 1.4), 1) + " dB de 18)");
      Rig c (edit, T::xmlTypeName); c.set (T::P (0, T::oByp), 1.f); c.set (T::P (1, T::oByp), 1.f); auto oe = c.run (big); chk (std::abs (rmsDb (oe, 1.0, 1.4) - rmsDb (big, 1.0, 1.4)) < .05, "tube sat: bypass del canal deja pasar la senal");
      Rig d (edit, T::xmlTypeName); d.set (T::pPower, 0.f); auto of = d.run (big); chk (std::abs (rmsDb (of, 1.0, 1.4) - rmsDb (big, 1.0, 1.4)) < .05, "tube sat: apagado no toca la senal");
      Rig e1 (edit, T::xmlTypeName); for (int q = 0; q < 2; ++q) { e1.set (T::P (q, T::oDrive), 1.f); e1.set (T::P (q, T::oFilt), 0.f); } Rig e2 (edit, T::xmlTypeName); for (int q = 0; q < 2; ++q) { e2.set (T::P (q, T::oDrive), 1.f); e2.set (T::P (q, T::oFilt), 2.f); }
      chk (rmsDb (e2.run (tone10), 1.0, 1.4) < rmsDb (e1.run (tone10), 1.0, 1.4) - 5.0, "tube sat: el filtro de 6 kHz baja un tono de 10 kHz");
      Rig f1 (edit, T::xmlTypeName); Rig f2 (edit, T::xmlTypeName); for (int q = 0; q < 2; ++q) { f1.set (T::P (q, T::oOut), 0.f); f2.set (T::P (q, T::oOut), 10.f); } chk (rmsDb (f2.run (lo), 1.0, 1.4) - rmsDb (f1.run (lo), 1.0, 1.4) > 25.0, "tube sat: output de 0 a 10 cambia unos 30 dB");
      Rig g1 (edit, T::xmlTypeName); g1.set (T::P (0, T::oDrive), 6.f); auto og1 = g1.run (big); Rig g2 (edit, T::xmlTypeName); g2.set (T::P (0, T::oDrive), 6.f); g2.set (T::P (0, T::oOver), 1.f); auto og2 = g2.run (big);
      chk (rmsDb (og2, 1.0, 1.4) != rmsDb (og1, 1.0, 1.4), "tube sat: overdrive cambia el sonido"); float mx = 0; for (int i = 0; i < og1.getNumSamples(); ++i) mx = std::max (mx, std::abs (og1.getSample (0, i))); chk (std::isfinite (mx) && mx < 4.f, "tube sat: la salida es finita y acotada");
      auto ed = a.fx->createEditor(); chk (ed != nullptr && ed->getWidth() > 500, "tube sat: la ventana con el panel se crea"); }
    // ---- Tape Saturation
    { using T = InjTapeSat; auto lo = sine (1000, .05f, 1.5), big = sine (1000, .4f, 1.5), t14 = sine (14000, .05f, 1.5), t10 = sine (10000, .02f, 1.5), sil = Buf (2, (int) (1.5 * SR));
      sil.clear();
      Rig a (edit, T::xmlTypeName); a.set (T::pDrive, 0.f); auto oa = a.run (lo); chk (std::abs (rmsDb (oa, 1.0, 1.4) - rmsDb (lo, 1.0, 1.4)) < .8, "tape: con drive 0 y senal baja no cambia el nivel (" + juce::String (rmsDb (oa, 1.0, 1.4) - rmsDb (lo, 1.0, 1.4), 2) + " dB)");
      Rig b (edit, T::xmlTypeName); b.set (T::pDrive, 0.f); b.set (T::pOut, 12.f); chk (std::abs ((rmsDb (b.run (lo), 1.0, 1.4) - rmsDb (oa, 1.0, 1.4)) - 12.0) < .5, "tape: la salida suma 12 dB");
      Rig c (edit, T::xmlTypeName); c.set (T::pDrive, 10.f); chk (rmsDb (c.run (big), 1.0, 1.4) - rmsDb (c.run (lo), 1.0, 1.4) < 14.0, "tape: con drive 10 comprime la senal fuerte");
      Rig d (edit, T::xmlTypeName); d.set (T::pMix, 0.f); d.set (T::pDrive, 10.f); chk (std::abs (rmsDb (d.run (big), 1.0, 1.4) - rmsDb (big, 1.0, 1.4)) < .1, "tape: con mezcla 0 deja pasar la senal seca");
      Rig e1 (edit, T::xmlTypeName); e1.set (T::pDrive, 0.f); e1.set (T::pSpeed, 0.f); Rig e2 (edit, T::xmlTypeName); e2.set (T::pDrive, 0.f); e2.set (T::pSpeed, 2.f); chk (rmsDb (e1.run (t14), 1.0, 1.4) < rmsDb (e2.run (t14), 1.0, 1.4) - 3.0, "tape: a 7.5 ips caen los agudos frente a 30 ips");
      Rig f1 (edit, T::xmlTypeName); f1.set (T::pDrive, 0.f); Rig f2 (edit, T::xmlTypeName); f2.set (T::pDrive, 0.f); f2.set (T::pHF, 6.f); const double hfd = rmsDb (f2.run (t10), 1.0, 1.4) - rmsDb (f1.run (t10), 1.0, 1.4); chk (hfd > 4.0 && hfd < 7.5, "tape: recorte de agudos +6 sube un tono de 10 kHz (" + juce::String (hfd, 1) + " dB)");
      Rig h1 (edit, T::xmlTypeName); Rig h2 (edit, T::xmlTypeName); h2.set (T::pHum, 1.f); chk (rmsDb (h1.run (sil), 0.5, 1.4) < -100.0 && rmsDb (h2.run (sil), 0.5, 1.4) > -75.0, "tape: el zumbido solo suena si se enciende");
      Rig i1 (edit, T::xmlTypeName); i1.set (T::pType, 1.f); i1.set (T::pDrive, 8.f); auto oi = i1.run (big); float mx = 0; for (int q = 0; q < oi.getNumSamples(); ++q) mx = std::max (mx, std::abs (oi.getSample (0, q))); chk (std::isfinite (mx) && mx < 3.f, "tape: la salida es finita y acotada");
      auto ed = a.fx->createEditor(); chk (ed != nullptr && ed->getWidth() > 500, "tape: la ventana con el panel se crea"); }
    // ---- Spring Reverb
    { using T = InjSpring; auto tone = sine (1000, .2f, 1.5), burst = noise (.4f, .3, 3); burst.setSize (2, (int) (2.0 * SR), true); for (int c = 0; c < 2; ++c) burst.clear (c, (int) (.3 * SR), burst.getNumSamples() - (int) (.3 * SR));
      Rig a (edit, T::xmlTypeName); a.set (T::pMix, 0.f); chk (std::abs (rmsDb (a.run (tone), 1.0, 1.4) - rmsDb (tone, 1.0, 1.4)) < .1, "spring: con mix 0 deja pasar la senal seca");
      Rig b (edit, T::xmlTypeName); b.set (T::pMix, 6.f); auto ob = b.run (burst); const double tail = rmsDb (ob, .45, .9); chk (tail > -60.0 && tail < rmsDb (burst, 0.0, .3) - 6.0, "spring: deja una cola tras el golpe (" + juce::String (tail, 1) + " dB)");
      Rig c (edit, T::xmlTypeName); c.set (T::pMix, 0.f); auto oc = c.run (burst); chk (rmsDb (oc, .45, .9) < -100.0, "spring: sin mezcla no hay cola");
      Rig d (edit, T::xmlTypeName); d.set (T::pMix, 8.f); d.set (T::pEffect, 0.f); auto od = d.run (burst); chk (rmsDb (od, .6, .9) < -90.0, "spring: con el efecto parado no hay cola (" + juce::String (rmsDb (od, .6, .9), 1) + " dB)");
      Rig e1 (edit, T::xmlTypeName); e1.set (T::pMix, 0.f); e1.set (T::pOut, 0.f); Rig e2 (edit, T::xmlTypeName); e2.set (T::pMix, 0.f); e2.set (T::pOut, 10.f); chk (std::abs ((rmsDb (e2.run (tone), 1.0, 1.4) - rmsDb (e1.run (tone), 1.0, 1.4)) - 24.0) < .5, "spring: output de 0 a 10 cambia 24 dB");
      auto hiB = sine (6000, .4f, .3); hiB.setSize (2, (int) (2.0 * SR), true); for (int c = 0; c < 2; ++c) hiB.clear (c, (int) (.3 * SR), hiB.getNumSamples() - (int) (.3 * SR));
      Rig f1 (edit, T::xmlTypeName); f1.set (T::pMix, 8.f); f1.set (T::pTone, 0.f); Rig f2 (edit, T::xmlTypeName); f2.set (T::pMix, 8.f); f2.set (T::pTone, 10.f); const double tl = rmsDb (f1.run (hiB), .4, .9), th = rmsDb (f2.run (hiB), .4, .9); chk (th > tl + 6.0, "spring: tone abierto deja pasar mas agudos en la cola (" + juce::String (th - tl, 1) + " dB)");
      Rig g (edit, T::xmlTypeName); g.set (T::pGain, 10.f); g.set (T::pMix, 10.f); auto og = g.run (noise (.9f, 2.0, 5)); float mx = 0; for (int i = 0; i < og.getNumSamples(); ++i) mx = std::max (mx, std::abs (og.getSample (0, i))); chk (std::isfinite (mx) && mx < 8.f, "spring: la salida es finita y acotada (" + juce::String (mx, 2) + ")");
      auto ed = a.fx->createEditor(); chk (ed != nullptr && ed->getWidth() > 800, "spring: la ventana con el panel se crea"); }
    // ---- Tape Echo
    { using T = InjTapeEcho; auto setTime = [] (Rig& r, int mode, float rate) { r.set (T::pMode, (float) mode); r.set (T::pRate, rate); r.set (T::pInt, 0.f); r.set (T::pRev, 0.f); r.set (T::pBal, .5f); r.set (T::pEcho, 10.f); };
      auto firstEcho = [&] (Rig& r) { auto o = r.run (impulse (1.5, .5f, 1000)); std::vector<int> at; for (int i = 4000; i < o.getNumSamples() - 2; ++i) { const float a = std::abs (o.getSample (0, i)); if (a > .03f && a >= std::abs (o.getSample (0, i - 1)) && a >= std::abs (o.getSample (0, i + 1)) && (at.empty() || i - at.back() > 200)) at.push_back (i - 1000); } return at; };
      Rig a (edit, T::xmlTypeName); setTime (a, 0, 5.f); auto ea = firstEcho (a); const int exp1 = (int) std::lround (.34 * std::exp (-1.92 * .5) * SR);
      chk (! ea.empty() && std::abs (ea[0] - exp1) < 80, "tape echo: modo 1 repite a un tiempo base (" + juce::String (ea.empty() ? -1 : ea[0]) + " de " + juce::String (exp1) + " muestras)");
      Rig b (edit, T::xmlTypeName); setTime (b, 5, 5.f); auto eb = firstEcho (b); chk (eb.size() >= 3 && std::abs (eb[1] - 2 * exp1) < 120 && std::abs (eb[2] - 3 * exp1) < 160, "tape echo: modo 6 suena las tres cabezas a 1x, 2x y 3x (" + juce::String ((int) eb.size()) + " ecos)");
      Rig c (edit, T::xmlTypeName); setTime (c, 3, 5.f); auto ec = firstEcho (c); chk (ec.size() == 2, "tape echo: modo 4 suena las cabezas 1 y 2 (" + juce::String ((int) ec.size()) + " ecos)");
      Rig d (edit, T::xmlTypeName); setTime (d, 0, 9.f); d.set (T::pInt, 0.f); auto od = d.run (impulse (1.5, .5f, 1000)); Rig e (edit, T::xmlTypeName); setTime (e, 0, 9.f); e.set (T::pInt, 7.f); auto oe = e.run (impulse (1.5, .5f, 1000));
      chk (rmsDb (oe, .6, 1.4) > rmsDb (od, .6, 1.4) + 15.0, "tape echo: intensity alarga las repeticiones");
      Rig f (edit, T::xmlTypeName); setTime (f, 5, 9.f); f.set (T::pInt, 10.f); f.set (T::pRev, 10.f); auto of = f.run (noise (.8f, 4.0, 7)); float mx = 0; for (int i = 0; i < of.getNumSamples(); ++i) mx = std::max (mx, std::abs (of.getSample (0, i))); chk (std::isfinite (mx) && mx < 12.f, "tape echo: con intensity 10 la salida sigue acotada (" + juce::String (mx, 2) + ")");
      auto tone = sine (1000, .2f, 1.5); Rig g (edit, T::xmlTypeName); g.set (T::pEcho, 0.f); g.set (T::pRev, 0.f); g.set (T::pBal, 0.f); chk (std::abs (rmsDb (g.run (tone), 1.0, 1.4) - rmsDb (tone, 1.0, 1.4)) < .1, "tape echo: con el deslizador en DIRECT solo pasa la senal directa");
      Rig h (edit, T::xmlTypeName); h.set (T::pPower, 0.f); h.set (T::pEcho, 10.f); chk (std::abs (rmsDb (h.run (tone), 1.0, 1.4) - rmsDb (tone, 1.0, 1.4)) < .1, "tape echo: apagado no toca la senal");
      Rig i1 (edit, T::xmlTypeName); i1.set (T::pBal, 0.f); i1.set (T::pEcho, 0.f); i1.set (T::pRev, 0.f); i1.set (T::pOut, 10.f); chk (std::abs ((rmsDb (i1.run (tone), 1.0, 1.4) - rmsDb (tone, 1.0, 1.4)) - 12.0) < .5, "tape echo: output volume 10 suma 12 dB");
      auto ed = a.fx->createEditor(); chk (ed != nullptr && ed->getWidth() > 800, "tape echo: la ventana con el panel se crea"); }
    // ---- Phaser doble, Flanger, Chorus
    { using T = InjPhaserDual; auto s = sine (1000, .3f, 2.0);
      Rig a (edit, T::xmlTypeName); a.set (T::pPower, 0.f); chk (std::abs (rmsDb (a.run (s), .5, 1.8) - rmsDb (s, .5, 1.8)) < .05, "phaser: apagado no toca la senal");
      Rig b (edit, T::xmlTypeName); b.set (T::pRate1, 4.f); b.set (T::pDepthA, 10.f); b.set (T::pSwA, 0.f); b.set (T::pFbA, 6.f); b.set (T::pIn, 1.f); auto ob = b.run (noise (.3f, 3.0, 11)); double lo = 1e9, hi = -1e9;
      for (double t0 = .5; t0 < 2.8; t0 += .05) { const double r = rmsDb (ob, t0, t0 + .05); lo = std::min (lo, r); hi = std::max (hi, r); }
      chk (hi - lo > .6 && finite (ob), "phaser: el barrido modula el nivel (" + juce::String (hi - lo, 1) + " dB)");
      Rig c (edit, T::xmlTypeName); c.set (T::pMan1, 1.f); c.set (T::pSwA, 2.f); c.set (T::pSwB, 2.f); c.set (T::pDepthA, 10.f); c.set (T::pDepthB, 10.f); c.set (T::pPed, .1f); auto nz = noise (.3f, 1.5, 13); const double p1 = rmsDb (c.run (nz), .3, 1.4);
      Rig d (edit, T::xmlTypeName); d.set (T::pMan1, 1.f); d.set (T::pSwA, 2.f); d.set (T::pSwB, 2.f); d.set (T::pDepthA, 10.f); d.set (T::pDepthB, 10.f); d.set (T::pPed, .9f); auto od = d.run (nz); const double p2 = rmsDb (od, .3, 1.4);
      chk (std::abs (rmsDb (nz, .3, 1.4) - p1) > .1 && std::isfinite (p2), "phaser: el modo pedal fija el barrido y cambia el sonido");
      Rig e (edit, T::xmlTypeName); e.set (T::pFbA, 10.f); e.set (T::pFbB, 10.f); e.set (T::pRate1, 10.f); auto oe = e.run (noise (.9f, 2.0, 5)); float mx = 0; for (int i = 0; i < oe.getNumSamples(); ++i) mx = std::max (mx, std::abs (oe.getSample (0, i))); chk (std::isfinite (mx) && mx < 10.f, "phaser: con realimentacion a tope la salida es acotada (" + juce::String (mx, 2) + ")");
      auto ed = a.fx->createEditor(); chk (ed != nullptr && ed->getWidth() > 900, "phaser: la ventana con el panel se crea"); }
    { using T = InjFlangerPro; auto s5 = sine (500, .3f, 1.5), s10 = sine (1000, .3f, 1.5);
      auto setup = [] (Rig& r) { r.set (T::pManual, 4.f); r.set (T::pWidth, 0.f); r.set (T::pRegen, 0.f); r.set (T::pMix, 5.f); };
      Rig a (edit, T::xmlTypeName); setup (a); const double n5 = rmsDb (a.run (s5), .6, 1.4); Rig b (edit, T::xmlTypeName); setup (b); const double n10 = rmsDb (b.run (s10), .6, 1.4); chk (n5 < n10 - 15.0, "flanger: el peine anula 500 Hz con 1 ms (" + juce::String (n10 - n5, 1) + " dB menos)");
      Rig c (edit, T::xmlTypeName); setup (c); c.set (T::pInvert, 1.f); const double inv = rmsDb (c.run (s10), .6, 1.4); chk (inv < n10 - 15.0, "flanger: invert da la fase contraria");
      Rig d (edit, T::xmlTypeName); d.set (T::pMix, 0.f); chk (std::abs (rmsDb (d.run (s10), .6, 1.4) - rmsDb (s10, .6, 1.4)) < .05, "flanger: con mix 0 solo pasa la seca");
      Rig e (edit, T::xmlTypeName); e.set (T::pRegen, 10.f); e.set (T::pWidth, 10.f); e.set (T::pSpeed, 10.f); auto oe = e.run (noise (.8f, 3.0, 9)); float mx = 0; for (int i = 0; i < oe.getNumSamples(); ++i) mx = std::max (mx, std::abs (oe.getSample (0, i))); chk (std::isfinite (mx) && mx < 12.f, "flanger: regen a tope sigue acotado (" + juce::String (mx, 2) + ")");
      Rig f (edit, T::xmlTypeName); f.set (T::pMode, 1.f); auto of = f.run (noise (.5f, 2.0, 3)); chk (finite (of) && rmsDb (of, .5, 1.8) > rmsDb (noise (.5f, 2.0, 3), .5, 1.8) - 1.0, "flanger: el modo doubler suena y es finito");
      auto ed = a.fx->createEditor(); chk (ed != nullptr && ed->getWidth() > 900, "flanger: la ventana con el panel se crea"); }
    { using T = InjChorusDim; auto nz = noise (.3f, 1.5, 21);
      Rig a (edit, T::xmlTypeName); a.set (T::pMode, 0.f); chk (std::abs (rmsDb (a.run (nz), .3, 1.4) - rmsDb (nz, .3, 1.4)) < .05, "chorus: modo off deja pasar la seca");
      Rig b (edit, T::xmlTypeName); b.set (T::pMode, 3.f); b.set (T::pBypass, 1.f); chk (std::abs (rmsDb (b.run (nz), .3, 1.4) - rmsDb (nz, .3, 1.4)) < .05, "chorus: bypass deja pasar la seca");
      Rig c (edit, T::xmlTypeName); c.set (T::pMode, 3.f); auto oc = c.run (nz); double dif = 0.0; for (int i = 20000; i < 60000; ++i) { const double d = oc.getSample (0, i) - oc.getSample (1, i); dif += d * d; } dif = 10.0 * std::log10 (dif / 40000.0 + 1e-12);
      chk (dif > -40.0 && finite (oc), "chorus: el modo 3 abre el estereo con una sola senal (" + juce::String (dif, 1) + " dB)");
      Rig d (edit, T::xmlTypeName); d.set (T::pMode, 3.f); d.set (T::pPower, 0.f); chk (std::abs (rmsDb (d.run (nz), .3, 1.4) - rmsDb (nz, .3, 1.4)) < .05, "chorus: apagado no toca la senal");
      chk (c.fx->meterA.load() > -60.f && c.fx->meterA.load() < 6.f, "chorus: el medidor de salida marca el nivel (" + juce::String (c.fx->meterA.load(), 1) + " dB)");
      auto ed = a.fx->createEditor(); chk (ed != nullptr && ed->getWidth() > 900, "chorus: la ventana con el panel se crea"); }
    // ---- Injustice Analyzer
    { using T = InjAnalyzer; Rig r (edit, T::xmlTypeName); auto in = stereoOf (sine (997, db2lin (-23.f), 6.0), sine (997, db2lin (-23.f), 6.0)); auto o = r.run (in); auto* an = dynamic_cast<T*> (r.fx);
      chk (maxDiff (o, in) < 1e-7, "analyzer: no cambia el sonido"); const double li = an->loudness().integrated(); chk (std::abs (li + 23.0) < .3, "analyzer: LUFS integrado de un tono a -23 dBFS (" + juce::String (li, 2) + ")");
      chk (an->choice ("top") == 1 && an->choice ("bands") == 3 && an->choice ("detection") == 3, "analyzer: ajustes por defecto como en el plugin VST3"); an->setChoice ("range", 5); an->setFlag ("compact", true); chk (an->choice ("range") == 5 && an->compactUi.load(), "analyzer: los ajustes se cambian y se leen");
      an->setFlag ("compact", false); auto ed = r.fx->createEditor(); chk (ed != nullptr && ed->getWidth() > 600 && ed->allowWindowResizing(), "analyzer: la ventana se crea y se puede redimensionar");
    }
    // ---- Injustice Tune
    { using T = InjTunePro; auto st = [&] (Rig& r, float key, float scale, float speed, float tol) { r.set (T::pKey, key); r.set (T::pScale, scale); r.set (T::pSpeed, speed); r.set (T::pHuman, 0.f); r.set (T::pTol, tol); };
      Rig r (edit, T::xmlTypeName); st (r, 0.f, 0.f, 0.f, 0.f); auto o = r.run (sine (450, .4f, 2.0));
      chk (std::abs (r.fx->meterA.load() - 450.f) < 6.f && r.fx->getLatencySeconds() > 0.0, "injustice tune: detecta 450 Hz (" + juce::String (r.fx->meterA.load(), 1) + ")");
      chk (amp (o, 440, 1.0, 1.9) > amp (o, 450, 1.0, 1.9) * 1.5 && finite (o), "injustice tune: cromatica, lleva 450 Hz a La 440");
      chk (dynamic_cast<T*> (r.fx)->noteOut.load() == 69, "injustice tune: la nota destino mostrada es La (" + juce::String (dynamic_cast<T*> (r.fx)->noteOut.load()) + ")");
      Rig m (edit, T::xmlTypeName); st (m, 0.f, 1.f, 0.f, 0.f); auto om = m.run (sine (270, .4f, 2.0)); chk (amp (om, 262, 1.0, 1.9) > amp (om, 270, 1.0, 1.9) * 1.5, "injustice tune: Do mayor lleva 270 Hz a Do4 (261,6 Hz)");
      Rig rf (edit, T::xmlTypeName); st (rf, 0.f, 0.f, 0.f, 0.f); rf.set (T::pRef, 432.f); auto orf = rf.run (sine (440, .4f, 2.0)); chk (amp (orf, 432, 1.0, 1.9) > amp (orf, 440, 1.0, 1.9) * 1.5, "injustice tune: referencia 432 Hz lleva 440 Hz a 432 Hz");
      Rig tl (edit, T::xmlTypeName); st (tl, 0.f, 0.f, 0.f, 100.f); auto ot = tl.run (sine (450, .4f, 2.0)); chk (amp (ot, 450, 1.0, 1.9) > amp (ot, 440, 1.0, 1.9) * 1.5, "injustice tune: la tolerancia de nota deja pasar una desafinacion pequena");
      Rig cu (edit, T::xmlTypeName); st (cu, 0.f, 11.f, 0.f, 0.f); cu.set (T::pMask, 1.f); auto oc = cu.run (sine (300, .4f, 2.0)); chk (amp (oc, 262, 1.0, 1.9) > amp (oc, 300, 1.0, 1.9) * 1.5, "injustice tune: escala personalizada solo Do lleva 300 Hz a 261,6 Hz");
      Rig b (edit, T::xmlTypeName); st (b, 0.f, 0.f, 0.f, 0.f); b.set (T::pBypass, 1.f); auto in = sine (450, .4f, 1.0); auto ob = b.run (in); const int lat = (int) std::lround (b.fx->getLatencySeconds() * SR); float md = 0.f; for (int i = 4000; i < 30000; ++i) md = std::max (md, std::abs (ob.getSample (0, i + lat - 1) - in.getSample (0, i)));
      chk (md < 1e-5f, "injustice tune: bypass deja la senal igual (con latencia " + juce::String (lat) + " muestras)");
      Rig m0 (edit, T::xmlTypeName); st (m0, 0.f, 0.f, 0.f, 0.f); m0.set (T::pMix, 0.f); auto o0 = m0.run (in); md = 0.f; for (int i = 4000; i < 30000; ++i) md = std::max (md, std::abs (o0.getSample (0, i + lat - 1) - in.getSample (0, i))); chk (md < 1e-5f, "injustice tune: mezcla 0 deja la senal igual");
      Rig fo (edit, T::xmlTypeName); st (fo, 0.f, 0.f, 0.f, 0.f); fo.set (T::pForm, 12.f); auto of = fo.run (noise (.3f, 1.0)); Rig fn (edit, T::xmlTypeName); fn.set (T::pForm, -12.f); auto ofn = fn.run (sine (200, .4f, 1.0)); float pk = 0.f; for (int i = 0; i < of.getNumSamples(); ++i) pk = std::max (pk, std::abs (of.getSample (0, i)));
      chk (finite (of) && finite (ofn) && pk < 6.f, "injustice tune: formante a los extremos da audio finito y acotado (pico " + juce::String (pk, 2) + ")");
      Rig nz (edit, T::xmlTypeName); nz.run (noise (.3f, 1.0)); chk (nz.fx->meterA.load() == 0.f && dynamic_cast<T*> (nz.fx)->noteOut.load() == -1, "injustice tune: con ruido no detecta tono");
      Rig vb (edit, T::xmlTypeName); st (vb, 0.f, 0.f, 0.f, 0.f); vb.set (T::pVib, 100.f); auto ov = vb.run (sine (440, .4f, 2.0)); chk (finite (ov) && rmsDb (ov, 1.0, 1.9) > -16.0, "injustice tune: el vibrato no rompe la senal (" + juce::String (rmsDb (ov, 1.0, 1.9), 1) + " dB)");
      Rig ab (edit, T::xmlTypeName); auto* tp = dynamic_cast<T*> (ab.fx); ab.set (T::pSpeed, 77.f); tp->switchSlot (1); ab.set (T::pSpeed, 10.f); tp->switchSlot (0); const float sA = tp->v (T::pSpeed); tp->switchSlot (1); const float sB = tp->v (T::pSpeed);
      chk (std::abs (sA - 77.f) < .01f && std::abs (sB - 10.f) < .01f, "injustice tune: A y B guardan ajustes distintos (" + juce::String (sA, 0) + " / " + juce::String (sB, 0) + ")");
      bool ok = true; { auto ps = ab.fx->presets(); chk (ps.size() >= 5, "injustice tune: tiene presets"); for (auto& pr : ps) for (size_t i = 0; i < pr.v.size(); ++i) ok = ok && pr.v[i] >= ab.fx->specs()[i].lo && pr.v[i] <= ab.fx->specs()[i].hi; chk (ok, "injustice tune: valores de preset dentro de rango"); }
      auto ed = ab.fx->createEditor(); chk (ed != nullptr && ed->getWidth() > 800 && ed->getHeight() > 400, "injustice tune: la ventana con el panel se crea"); }
    // ---- Attack Shaper
    { auto in = sine (1000, .3f, 1.5); auto burst = sine (1000, .5f, 0.4);
      Rig a (edit, InjAttackShaper::xmlTypeName); auto oa = a.run (in); chk (std::abs (rmsDb (oa, 1.0, 1.4) - rmsDb (in, 1.0, 1.4)) < .3, "attack shaper: en plano no cambia la senal");
      Rig b (edit, InjAttackShaper::xmlTypeName); for (int c = 0; c < 4; ++c) b.set (InjAttackShaper::ATK (c), 6.f); auto ob = b.run (burst);
      Rig c (edit, InjAttackShaper::xmlTypeName); for (int k = 0; k < 4; ++k) c.set (InjAttackShaper::ATK (k), -6.f); auto oc = c.run (burst);
      chk (rmsDb (ob, 0.0, 0.02) > rmsDb (oc, 0.0, 0.02) + 2.0, "attack shaper: el ataque +6 sube el inicio frente a -6 (" + juce::String (rmsDb (ob, 0.0, 0.02) - rmsDb (oc, 0.0, 0.02), 1) + " dB)");
      Rig d (edit, InjAttackShaper::xmlTypeName); d.set (InjAttackShaper::pPower, 0.f); d.set (InjAttackShaper::ATK (0), 6.f); auto od = d.run (burst); chk (std::abs (rmsDb (od, 0.0, 0.02) - rmsDb (burst, 0.0, 0.02)) < .05, "attack shaper: apagado no toca la senal");
      auto ed = a.fx->createEditor(); chk (ed != nullptr && ed->getWidth() > 800, "attack shaper: la ventana con el panel se crea"); }
    // ---- Cobalt Limiter
    { auto in = sine (1000, .5f, 1.5);
      Rig a (edit, InjCobalt::xmlTypeName); a.set (InjCobalt::INP (0), 12.f); a.set (InjCobalt::INP (1), 12.f); a.set (InjCobalt::MODE (0), 2.f); a.set (InjCobalt::MODE (1), 2.f); a.set (InjCobalt::ATK (0), .02f); a.set (InjCobalt::ATK (1), .02f); auto oa = a.run (in);
      chk (a.fx->meterA.load() > 8.f && a.fx->meterB.load() > 8.f, "cobalt: LIMIT comprime los dos lados (" + juce::String (a.fx->meterA.load(), 1) + " dB)");
      Rig b (edit, InjCobalt::xmlTypeName); b.set (InjCobalt::INP (0), 12.f); b.set (InjCobalt::INP (1), 12.f); b.set (InjCobalt::MODE (0), 0.f); b.set (InjCobalt::MODE (1), 0.f); b.run (in); chk (b.fx->meterA.load() < a.fx->meterA.load() - 1.f, "cobalt: COMP 1 comprime menos que LIMIT");
      Rig c (edit, InjCobalt::xmlTypeName); c.set (InjCobalt::INN (0), 0.f); c.set (InjCobalt::INN (1), 0.f); auto oc = c.run (in); chk (std::abs (rmsDb (oc, 1.0, 1.4) - rmsDb (in, 1.0, 1.4)) < .05, "cobalt: con IN apagado no toca la senal");
      Rig d (edit, InjCobalt::xmlTypeName); d.set (InjCobalt::OUT (0), 6.f); d.set (InjCobalt::OUT (1), 6.f); d.set (InjCobalt::INP (0), -20.f); d.set (InjCobalt::INP (1), -20.f); auto od = d.run (sine (1000, .05f, 1.5)); chk (std::abs ((rmsDb (od, 1.0, 1.4) - rmsDb (sine (1000, .05f, 1.5), 1.0, 1.4)) - (-14.0)) < .6, "cobalt: entrada -20 y salida +6 dan -14 dB");
      auto ed = a.fx->createEditor(); chk (ed != nullptr && ed->getWidth() > 800, "cobalt: la ventana con el panel se crea"); }
    // ---- Multiband
    { auto lo = sine (100, .3f, 1.5), mid = sine (1000, .3f, 1.5), hi = sine (8000, .3f, 1.5), loud = sine (1000, .5f, 1.5);
      Rig a (edit, InjMband::xmlTypeName); for (int b = 0; b < 3; ++b) a.set (InjMband::THR (b), 20.f);
      auto o1 = a.run (lo); auto o2 = a.run (mid); auto o3 = a.run (hi);
      chk (std::abs (rmsDb (o1, 1.0, 1.4) - rmsDb (lo, 1.0, 1.4)) < 1.5 && std::abs (rmsDb (o2, 1.0, 1.4) - rmsDb (mid, 1.0, 1.4)) < 1.5 && std::abs (rmsDb (o3, 1.0, 1.4) - rmsDb (hi, 1.0, 1.4)) < 1.5, "multiband: sin compresion suma plano en graves, medios y agudos");
      Rig b (edit, InjMband::xmlTypeName); b.set (InjMband::THR (1), -30.f); b.run (loud); chk (b.fx->meterA.load() < 1.f && static_cast<InjMband*> (b.fx)->grMeter[1].load() > 6.f, "multiband: solo comprime la banda media con un tono de 1 kHz");
      Rig c (edit, InjMband::xmlTypeName); c.set (InjMband::MODE (2), 1.f); auto oc = c.run (hi); chk (rmsDb (oc, 1.0, 1.4) < rmsDb (hi, 1.0, 1.4) - 20.0, "multiband: MUTE en la banda aguda silencia los 8 kHz");
      Rig d (edit, InjMband::xmlTypeName); d.set (InjMband::pPow, 0.f); d.set (InjMband::THR (1), -30.f); auto od = d.run (loud); chk (std::abs (rmsDb (od, 1.0, 1.4) - rmsDb (loud, 1.0, 1.4)) < .05, "multiband: apagado no toca la senal");
      Rig e (edit, InjMband::xmlTypeName); e.set (InjMband::pOut, 6.f); for (int k = 0; k < 3; ++k) e.set (InjMband::THR (k), 20.f); auto oe = e.run (mid); chk (std::abs ((rmsDb (oe, 1.0, 1.4) - rmsDb (mid, 1.0, 1.4)) - 6.0) < 1.5, "multiband: la ganancia de salida suma 6 dB");
      auto ed = a.fx->createEditor(); chk (ed != nullptr && ed->getWidth() > 800, "multiband: la ventana con el panel se crea"); }
    // ---- Stack Comp
    { auto in = sine (1000, .5f, 1.5);
      Rig a (edit, InjStack::xmlTypeName); a.set (InjStack::pThr, -24.f); a.set (InjStack::pRatio, 3.f); a.set (InjStack::pAtk, 1.f); a.set (InjStack::pRel, .3f); a.set (InjStack::pMk, 0.f); a.set (InjStack::pMix, 100.f); auto oa = a.run (in);
      const double g = rmsDb (oa, 1.0, 1.4) - rmsDb (in, 1.0, 1.4); chk (std::abs (g + 13.5) < 2.0, "stack: -24 dB, 4:1 baja ~13.5 dB (" + juce::String (g, 2) + ")");
      Rig b (edit, InjStack::xmlTypeName); b.set (InjStack::pThr, -24.f); b.set (InjStack::pRatio, 5.f); b.set (InjStack::pAtk, 1.f); b.run (in); chk (b.fx->meterA.load() > a.fx->meterA.load() + 1.f, "stack: inf:1 comprime mas que 4:1");
      Rig c (edit, InjStack::xmlTypeName); c.set (InjStack::pIn, 0.f); c.set (InjStack::pThr, -30.f); auto oc = c.run (in); chk (std::abs (rmsDb (oc, 1.0, 1.4) - rmsDb (in, 1.0, 1.4)) < .05, "stack: con IN apagado no toca la senal");
      Rig d (edit, InjStack::xmlTypeName); d.set (InjStack::pThr, -24.f); d.set (InjStack::pRatio, 3.f); d.set (InjStack::pAtk, 1.f); d.set (InjStack::pMix, 0.f); auto od = d.run (in); chk (std::abs (rmsDb (od, 1.0, 1.4) - rmsDb (in, 1.0, 1.4)) < .1, "stack: mezcla 0 deja el sonido seco");
      Rig e (edit, InjStack::xmlTypeName); e.set (InjStack::pThr, 10.f); e.set (InjStack::pMk, 6.f); auto oe = e.run (in); chk (std::abs ((rmsDb (oe, 1.0, 1.4) - rmsDb (in, 1.0, 1.4)) - 6.0) < .3, "stack: la ganancia de salida suma 6 dB");
      auto ed = a.fx->createEditor(); chk (ed != nullptr && ed->getHeight() > 600, "stack: la ventana con el panel se crea"); }
    // ---- mejoras del ecualizador y del compresor
    { Rig r (edit, InjEq::xmlTypeName); r.set (InjEq::pHp, 100.f); auto lo = sine (30, .1f, 1.0); const double g = rmsDb (r.run (lo), .3, .9) - rmsDb (lo, .3, .9); Rig r2 (edit, InjEq::xmlTypeName); r2.set (InjEq::pHp, 100.f); auto mid = sine (1000, .1f, 1.0); const double g2 = rmsDb (r2.run (mid), .3, .9) - rmsDb (mid, .3, .9);
      chk (g < -30.0 && std::abs (g2) < .3, "ecualizador: el corte de graves de 24 dB quita 30 Hz (" + juce::String (g, 1) + " dB) y no toca 1 kHz");
      Rig r3 (edit, InjEq::xmlTypeName); r3.set (InjEq::pLp, 2000.f); auto hi = sine (8000, .1f, 1.0); chk (rmsDb (r3.run (hi), .3, .9) - rmsDb (hi, .3, .9) < -30.0, "ecualizador: el corte de agudos quita 8 kHz");
      Rig r4 (edit, InjEq::xmlTypeName); auto in = sine (1000, .5f, 1.0); r4.run (in); auto* eq = dynamic_cast<InjEq*> (r4.fx); std::vector<float> db; eq->spectrum (db); int km = 1; for (int k = 1; k < (int) db.size(); ++k) if (db[(size_t) k] > db[(size_t) km]) km = k; chk (std::abs (km * (float) SR / 2048.f - 1000.f) < 25.f, "ecualizador: el analizador en vivo ve el seno de 1 kHz"); }
    { auto in = sine (60, .5f, 1.5); Rig a (edit, InjComp::xmlTypeName); a.set (InjComp::pThr, -24.f); a.set (InjComp::pRatio, 4.f); a.set (InjComp::pHp, 20.f); a.run (in); const float ga = a.fx->meterA.load();
      Rig b (edit, InjComp::xmlTypeName); b.set (InjComp::pThr, -24.f); b.set (InjComp::pRatio, 4.f); b.set (InjComp::pHp, 300.f); b.run (in); const float gb = b.fx->meterA.load(); chk (ga > 8.f && gb < ga - 6.f, "compresor: el filtro del detector ignora los graves (" + juce::String (ga, 1) + " dB sin filtro, " + juce::String (gb, 1) + " dB con filtro)");
      auto s1 = sine (1000, db2lin (-27.f), 1.0); Rig k0 (edit, InjComp::xmlTypeName); k0.set (InjComp::pThr, -24.f); k0.set (InjComp::pKnee, 0.f); k0.set (InjComp::pRatio, 8.f); k0.run (s1); Rig k1 (edit, InjComp::xmlTypeName); k1.set (InjComp::pThr, -24.f); k1.set (InjComp::pKnee, 12.f); k1.set (InjComp::pRatio, 8.f); k1.run (s1);
      chk (k0.fx->meterA.load() < .3f && k1.fx->meterA.load() > .1f, "compresor: un codo ancho empieza a comprimir antes del umbral (" + juce::String (k0.fx->meterA.load(), 2) + " vs " + juce::String (k1.fx->meterA.load(), 2) + " dB)"); }
    { Rig r (edit, InjReverb::xmlTypeName); r.set (InjReverb::pMix, 1.f); r.set (InjReverb::pWidth, 0.f); auto o = r.run (noise (.3f, 1.5)); double e = 0; for (int i = 20000; i < 60000; ++i) e = std::max (e, (double) std::abs (o.getSample (0, i) - o.getSample (1, i))); chk (e < 1e-4, "reverb: ancho 0 la deja en mono (" + juce::String (e, 6) + ")");
      Rig a (edit, InjReverb::xmlTypeName); a.set (InjReverb::pMix, 1.f); a.set (InjReverb::pLow, 400.f); Rig b (edit, InjReverb::xmlTypeName); b.set (InjReverb::pMix, 1.f); auto lo = sine (60, .3f, 2.0); chk (rmsDb (a.run (lo), 1.0, 1.9) < rmsDb (b.run (lo), 1.0, 1.9) - 15.0, "reverb: el corte de graves limpia la cola grave"); }
    // ---- robustez: todos los parametros se guardan y recuperan, otras frecuencias de muestreo y tamanos de bloque raros
    for (auto& e : catalog())
    {
        Rig r (edit, e.type); if (r.fx == nullptr) continue; for (int k = 0; k < r.fx->numSpecs(); ++k) { const auto& s = r.fx->specs()[k]; r.set (k, s.lo + (s.hi - s.lo) * (s.fmt == fChoice ? 1.f : .37f)); }
        r.fx->flushPluginStateToValueTree(); auto st = r.p->state.createCopy(); st.removeProperty ("id", nullptr); auto p2 = edit.getPluginCache().createNewPlugin (st); auto* f2 = dynamic_cast<InjFx*> (p2.get()); bool ok = f2 != nullptr;
        for (int k = 0; ok && k < r.fx->numSpecs(); ++k) if (std::abs (f2->v (k) - r.fx->v (k)) > 1e-3f * std::max (1.f, std::abs (r.fx->v (k)))) { ok = false; std::printf ("   parametro %d (%s) no coincide\n", k, r.fx->specs()[k].id); }
        chk (ok, juce::String ("guardado: todos los parametros de ") + e.label + " vuelven iguales");
    }
    for (double rate : { 48000.0, 96000.0 })
        for (auto& e : catalog())
        {
            auto p = edit.getPluginCache().createNewPlugin (e.type, {}); auto* fx = dynamic_cast<InjFx*> (p.get()); if (fx == nullptr) continue; fx->initialise ({ te::TimePosition(), rate, 512 }); fx->applyPreset (0);
            juce::Random rnd (5); const int total = (int) (rate * 1.5); Buf b (2, total); for (int c = 0; c < 2; ++c) for (int i = 0; i < total; ++i) b.setSample (c, i, .3f * (std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * i / rate) * .6f + (rnd.nextFloat() * 2.f - 1.f) * .4f));
            bool ok = true; int pos = 0; const int sizes[] { 512, 1, 37, 1024, 64, 3 }; int si = 0;
            while (pos < total) { const int n = std::min (sizes[si++ % 6], total - pos); te::PluginRenderContext ctx (&b, juce::AudioChannelSet::stereo(), pos, n, nullptr, 0.0, te::TimeRange (te::TimePosition::fromSeconds (pos / rate), te::TimeDuration::fromSeconds (n / rate)), true, false, false, false); fx->applyToBuffer (ctx); pos += n; }
            if (! finite (b) || peakOf (b, 0, 1.4) > 30.f) ok = false; if (! ok) { std::printf ("   %s a %.0f Hz\n", e.label, rate); } chk (ok, juce::String ("robustez: ") + e.label + " a " + juce::String (rate, 0) + " Hz con bloques de 1 a 1024 muestras");
        }
    { Rig r (edit, InjDelay::xmlTypeName); r.set (InjDelay::pTime, 4.f); r.set (InjDelay::pFree, 100.f); r.set (InjDelay::pFb, .6f); r.set (InjDelay::pMix, 1.f); r.set (InjDelay::pDuck, 1.f); auto in = sine (440, .5f, 1.0); auto o = r.run (in); Rig q (edit, InjDelay::xmlTypeName); q.set (InjDelay::pTime, 4.f); q.set (InjDelay::pFree, 100.f); q.set (InjDelay::pFb, .6f); q.set (InjDelay::pMix, 1.f); auto o2 = q.run (in);
      chk (rmsDb (o, .6, .95) < rmsDb (o2, .6, .95) - 3.0, "delay: el ducking baja los ecos mientras suena la entrada (" + juce::String (rmsDb (o, .6, .95) - rmsDb (o2, .6, .95), 1) + " dB)");
      Rig a (edit, InjDelay::xmlTypeName); a.set (InjDelay::pTime, 4.f); a.set (InjDelay::pFree, 50.f); a.set (InjDelay::pFb, .9f); a.set (InjDelay::pMix, 1.f); a.set (InjDelay::pLow, 500.f); a.set (InjDelay::pTone, 20000.f); auto lo = sine (60, .3f, 2.0); for (int c = 0; c < 2; ++c) for (int i = (int) (.5 * SR); i < lo.getNumSamples(); ++i) lo.setSample (c, i, 0.f); Rig b (edit, InjDelay::xmlTypeName); b.set (InjDelay::pTime, 4.f); b.set (InjDelay::pFree, 50.f); b.set (InjDelay::pFb, .9f); b.set (InjDelay::pMix, 1.f); b.set (InjDelay::pTone, 20000.f);
      const double da = rmsDb (a.run (lo), .8, 1.9), db_ = rmsDb (b.run (lo), .8, 1.9); chk (da < db_ - 6.0, "delay: el corte de graves adelgaza las repeticiones (" + juce::String (da - db_, 1) + " dB)"); }
    { Rig a (edit, InjFilter::xmlTypeName); a.set (InjFilter::pCut, 20000.f); a.set (InjFilter::pDrive, 1.f); auto s = sine (200, .8f, 1.0); auto oa = a.run (s); Rig b (edit, InjFilter::xmlTypeName); b.set (InjFilter::pCut, 20000.f); auto ob = b.run (s);
      chk (thd (oa, 200, .3, .9) > .05 && thd (ob, 200, .3, .9) < .02, "filtro: la saturacion agrega armonicos (" + juce::String (thd (oa, 200, .3, .9) * 100, 1) + " % contra " + juce::String (thd (ob, 200, .3, .9) * 100, 1) + " %)"); }
    // ---- material real: un bombo repetido por cada preset de cada efecto (valores validos, sin explosiones, sin continua)
    if (const char* sf = std::getenv ("INJ_SAMPLE_FILE"))
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (juce::File (juce::String::fromUTF8 (sf))));
        if (rd != nullptr)
        {
            const int kl = (int) std::min<juce::int64> (rd->lengthInSamples, (juce::int64) (.5 * SR)); juce::AudioBuffer<float> k ((int) rd->numChannels, kl); rd->read (&k, 0, kl, 0, true, true);
            const int beat = (int) (beatS * SR), total = beat * 8; Buf loop (2, total); loop.clear(); float kp = 0.f; for (int c = 0; c < k.getNumChannels(); ++c) for (int i = 0; i < kl; ++i) kp = std::max (kp, std::abs (k.getSample (c, i)));
            for (int b = 0; b < 8; ++b) for (int i = 0; i < kl && b * beat + i < total; ++i) for (int c = 0; c < 2; ++c) loop.addSample (c, b * beat + i, .8f / std::max (.01f, kp) * k.getSample (std::min (c, k.getNumChannels() - 1), i));
            int bad = 0, n = 0; for (auto& e : catalog())
            {
                Rig probe (edit, e.type); if (probe.fx == nullptr) continue; const int np = (int) probe.fx->presets().size();
                for (int pi = 0; pi < np; ++pi) { Rig r (edit, e.type); r.fx->applyPreset (pi); auto o = r.run (loop); double dc = 0; for (int i = total / 2; i < total; ++i) dc += o.getSample (0, i); dc /= (total / 2.0); ++n;
                    const bool ok = finite (o) && peakOf (o, 0, 99.0) < 12.f && std::abs (dc) < .08; if (! ok) { ++bad; std::printf ("   %s, preset %d: pico %.2f, continua %.3f\n", e.label, pi, peakOf (o, 0, 99.0), dc); } }
            }
            chk (bad == 0, "material real: " + juce::String (n) + " presets sobre un bombo, sin valores invalidos, sin explosiones ni continua");
        }
    }
    // ---- aliasing: un seno de 7 kHz muy distorsionado no debe dejar rastros no armonicos (9.1 kHz es el reflejo de la 5.a armonica)
    { auto in = sine (7000, .8f, 1.0); auto fund = [&] (const Buf& b) { return ampDb (b, 7000, .3, .9); };
      Rig c (edit, InjClipper::xmlTypeName); c.set (InjClipper::pDrive, 24.f); auto oc = c.run (in); chk (ampDb (oc, 9100, .3, .9) < fund (oc) - 45.0, "clipper: sin aliasing audible (" + juce::String (ampDb (oc, 9100, .3, .9) - fund (oc), 1) + " dB)");
      Rig s (edit, InjShaper::xmlTypeName); s.set (InjShaper::pDrive, 1.f); s.set (InjShaper::pType, 0.f); auto os = s.run (in); chk (ampDb (os, 9100, .3, .9) < fund (os) - 40.0, "waveshaper: sin aliasing audible (" + juce::String (ampDb (os, 9100, .3, .9) - fund (os), 1) + " dB)");
      Rig k (edit, InjKickDrive::xmlTypeName); k.set (InjKickDrive::pDrive, 1.f); auto ok = k.run (in); chk (ampDb (ok, 9100, .3, .9) < fund (ok) - 40.0, "drive de kick: sin aliasing audible (" + juce::String (ampDb (ok, 9100, .3, .9) - fund (ok), 1) + " dB)");
      Rig z (edit, InjSat::xmlTypeName); z.set (InjSat::pType, 4.f); z.set (InjSat::pAmt, 1.f); auto oz = z.run (in); chk (ampDb (oz, 9100, .3, .9) < fund (oz) - 40.0, "saturador: sin aliasing audible (" + juce::String (ampDb (oz, 9100, .3, .9) - fund (oz), 1) + " dB)"); }
    // ---- consumo de CPU de cada efecto (5 s de ruido estereo, bloques de 512)
    { auto in = noise (.3f, 5.0); for (auto& e : catalog())
      { Rig r (edit, e.type); r.fx->applyPreset (0); const double t0 = juce::Time::getMillisecondCounterHiRes(); auto o = r.run (in); const double ms = juce::Time::getMillisecondCounterHiRes() - t0; const double pct = ms / 50.0;
        chk (finite (o) && pct < 25.0, juce::String ("consumo: ") + e.label + " usa " + juce::String (pct, 1) + " % de un nucleo y no produce valores invalidos"); } }
}
} // namespace injfx::test
