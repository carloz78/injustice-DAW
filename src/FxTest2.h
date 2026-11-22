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
