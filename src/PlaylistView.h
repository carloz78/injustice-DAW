#pragma once
#include <climits>
#include "Instruments.h"
#include "Theme.h"
#include "TimeSig.h"
#include "Icons.h"
#include "Cursors.h"
#include "TrackUtil.h"
#include "Arranger.h"
#include "Harmony.h"
#include "MidiTools.h"
#include "SampleAnalysis.h"
#include <tracktion_engine/tracktion_engine.h>
#include <functional>

namespace te = tracktion;

// Playlist: cabeceras de pista (izquierda, tamaño fijo), regla de compases, cuadrícula con clips y cabezal.
class PlaylistView : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    static constexpr int kHeaderW = 112, kBarsH = 40, kSecH = 20, kChordH = 20, kGap = 2;   // la regla de compases mide kBarsH; debajo va el carril de secciones (arreglo)
    mutable bool chordOn = false;
    void refreshChordLane() const { chordOn = (bool) edit.state.getProperty ("injChordLane", false) || edit.state.getChildWithName ("INJCHORD").isValid(); }
    int rulerH() const { return kBarsH + kSecH + (chordOn ? kChordH : 0); }   // regla + carril de secciones + (si se usa) carril de acordes
    // alturas por pista (guardadas en el estado de la pista como "injH")
    static constexpr int kMinH = 22, kMaxH = 300;
    mutable std::vector<int> hts, tops; mutable bool layoutDirty = true; int resizing = -1, resizeStartH = 0; float resizeStartY = 0;
    void ensureLayout() const
    {
        const int n = numTracks(); if (! layoutDirty && (int) hts.size() == n && (int) tops.size() == n + 1) return;
        auto tr = te::getAudioTracks (edit); hts.assign ((size_t) n, trackH); tops.assign ((size_t) n + 1, 0); int y = 0;
        for (int i = 0; i < n; ++i) { hts[(size_t) i] = juce::jlimit (kMinH, kMaxH, (int) tr[i]->state.getProperty ("injH", trackH)); tops[(size_t) i] = y; y += hts[(size_t) i] + kGap; }
        tops[(size_t) n] = y; layoutDirty = false;
    }
    int hOf (int i) const { ensureLayout(); return i >= 0 && i < (int) hts.size() ? hts[(size_t) i] : trackH; }
    void setTrackHeight (int i, int h) { auto tr = te::getAudioTracks (edit); if (i >= 0 && i < tr.size()) tr[i]->state.setProperty ("injH", juce::jlimit (kMinH, kMaxH, h), nullptr); tr[i]->state.setProperty ("injFold", false, nullptr); layoutDirty = true; }
    void setAllHeights (int h) { h = juce::jlimit (kMinH, kMaxH, h); trackH = h; for (auto* t : te::getAudioTracks (edit)) { t->state.setProperty ("injH", h, nullptr); t->state.setProperty ("injFold", false, nullptr); } layoutDirty = true; }
    int edgeAt (const juce::Point<int>& p) const { if (p.x >= kHeaderW + 6 || p.y < rulerH()) return -1; for (int i = 0; i < numTracks(); ++i) { const int b = trackTop (i) + hOf (i); if (p.y >= b - 4 && p.y <= b + kGap + 1) return i; } return -1; }
    int trackH = 92;   // alto de pista; Alt+rueda lo cambia (60-220)
    juce::String tool = "select"; bool snap = true;

    explicit PlaylistView (te::Edit& e) : edit (e), thumbCache (32)
    {
        fm.registerBasicFormats(); setWantsKeyboardFocus (true); setOpaque (true); startTimerHz (60);
    }

    //==============================================================================
    double beatsToTime (double b) const { return edit.tempoSequence.toTime (te::BeatPosition::fromBeats (b)).inSeconds(); }
    double timeToBeats (double t) const { return edit.tempoSequence.toBeats (te::TimePosition::fromSeconds (t)).inBeats(); }
    // tijeras: línea guía donde se va a cortar y destello al cortar
    float cutX = -1.f, cutV = 0.f, selFade = 0.f, mqFade = 0.f, zoomTarget = -1.f, zoomAnchorX = 0.f; int cutTrack = -1, flashTrack = -1; double cutBeat = -1, flashBeat = -1; float flashA = 0.f;
    juce::Point<float> chevronCentre (int i) const { return chevronR (i).getCentre(); }
    bool folded (int i) const { return isFolded (i); }
    std::function<juce::File()> samplesRoot; std::function<void (int, juce::String)> onInstrument;
    std::function<juce::Array<juce::PluginDescription>()> ourPlugins; std::function<void (const juce::PluginDescription&, int)> onAddPlugin;   // atajo: plugins de Injustice Audio directo a la pista
    bool hasHScroll() const { return needHScroll(); }
    void forgetTouched() { lastTouched = nullptr; }
    void showAutomationLanes (int sel)
    {   // al activar la herramienta: se ven las curvas que ya existen y la pista seleccionada
        auto tr = te::getAudioTracks (edit);
        for (int i = 0; i < tr.size(); ++i) { auto* vp = autoParamOf (*tr[i]); const bool used = vp != nullptr && vp->getCurve().getNumPoints() > 0; tr[i]->state.setProperty ("injAuto", used || i == sel, nullptr); }
        repaint();
    }
    void hideAllAutomation() { for (auto* t : te::getAudioTracks (edit)) t->state.setProperty ("injAuto", false, nullptr); repaint(); }
    float beatToX (double b) const { return (float) (kHeaderW + 6 + b * pxPerBeat - scrollX); }
    double xToBeat (float x) const { return (x - kHeaderW - 6 + scrollX) / pxPerBeat; }
    int trackTop (int i) const { ensureLayout(); const int n = (int) tops.size() - 1; return rulerH() + (i <= n ? tops[(size_t) std::max (0, i)] : tops[(size_t) n] + (i - n) * (trackH + kGap)) - (int) scrollY; }
    // Cuadrícula adaptable: el paso es el más fino que se ve separado (>= 12 px); con el zoom por defecto es el compás
    double snapStep() const { for (double st : { 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 }) if (st * pxPerBeat >= 12.0) return st; return 32.0; }
    bool altDown = false;   // Alt mientras arrastras = sin imán de cuadrícula (como en FL Studio)
    double snapBeat (double b) const { if (! snap || altDown) return b; const double st = snapStep(); return std::round (b / st) * st; }
    int numTracks() const { return te::getAudioTracks (edit).size(); }
    int trackAt (float y) const { for (int i = 0; i < numTracks(); ++i) if (y >= trackTop (i) && y < trackTop (i) + hOf (i)) return i; return -1; }

    //==============================================================================
    // cabezal suave: entre dos avisos del motor de audio se avanza con el reloj, para que no dé saltos
    double smoothPos()
    {
        auto& tc = edit.getTransport(); const double raw = tc.getPosition().inSeconds(), now = juce::Time::getMillisecondCounterHiRes() * .001;
        if (! tc.isPlaying()) { lastRaw = raw; lastRawAt = now; return raw; }
        if (raw != lastRaw) { lastRaw = raw; lastRawAt = now; }
        return raw + std::min (now - lastRawAt, 0.04);
    }
    double lastRaw = -1, lastRawAt = 0;
    void paint (juce::Graphics& g) override
    {
        refreshChordLane(); using namespace theme;
        const int W = getWidth(), H = getHeight();
        g.fillAll (bgBar);
        auto tracks = te::getAudioTracks (edit);

        // fondo de pistas + líneas de compás
        g.saveState(); g.reduceClipRegion (kHeaderW + 6, rulerH(), W, H - rulerH());
        for (int i = 0; i < tracks.size(); ++i)
        {
            auto row = juce::Rectangle<int> (kHeaderW + 6, trackTop (i), W, hOf (i));
            g.setColour (i == selTrack ? bgGrid.brighter (.07f) : (i % 2 ? bgGridAlt : bgGrid)); g.fillRect (row);
            if (i == selTrack) { g.setColour (theme::accent.withAlpha (.28f)); g.fillRect (row.getX(), row.getY(), W, 1); g.fillRect (row.getX(), row.getBottom() - 1, W, 1); }
            g.setColour (seam); g.fillRect (row.getX(), row.getBottom(), W, kGap);
        }
        const double b0 = std::floor (xToBeat ((float) kHeaderW)), b1 = xToBeat ((float) W) + 1;
        const bool showBeats = pxPerBeat >= 14;
        if (pxPerBeat >= 3.f)
        {   // los compases pares se aclaran apenas: ayuda a contar de un vistazo (como Studio One)
            const double bl = std::max (1.0, timesig::bar()); const int k0 = (int) std::floor (std::max (0.0, b0) / bl), k1 = (int) std::ceil (b1 / bl);
            const int grp = pxPerBeat * bl < 24.0 ? 4 : 1;   // muy alejado: la franja abarca 4 compases para no saturar la vista
            g.setColour (juce::Colours::white.withAlpha (.018f));
            for (int k = (k0 / grp) * grp; k <= k1; k += grp) if ((k / grp) % 2 == 1) { const float xa = beatToX (k * bl), xb = beatToX ((k + grp) * bl); g.fillRect (xa, (float) rulerH(), xb - xa, (float) (H - rulerH())); }
        }
        for (double b = std::max (0.0, b0); b <= b1; b += 1.0)
        {
            const bool bar = std::fmod (b, timesig::bar()) == 0.0;
            if (! bar && ! showBeats) continue;
            if (bar && pxPerBeat * timesig::bar() < 24.0 && std::fmod (b / timesig::bar(), 4.0) != 0.0) continue;
            g.setColour (bar ? barLine : gridLine.withAlpha (.55f));
            g.fillRect (beatToX (b), (float) rulerH(), 1.f, (float) (H - rulerH()));        }
        if (pxPerBeat >= 48)
        {   // al acercar aparecen las divisiones: medios pulsos y, más cerca, semicorcheas
            const int div = pxPerBeat >= 110 ? 4 : 2;
            for (double b = std::max (0.0, b0); b <= b1; b += 1.0 / div) { if (std::fmod (b, 1.0) < 1e-6) continue; g.setColour (gridLine.withAlpha (std::fmod (b * 2.0, 1.0) < 1e-6 ? .32f : .18f)); g.fillRect (beatToX (b), (float) rulerH(), 1.f, (float) (H - rulerH())); }
        }
        // clips
        auto visibleRow = [&] (int i) { const int t = trackTop (i); return t + hOf (i) >= rulerH() && t <= H; };   // solo se dibuja lo que se ve
        for (int i = 0; i < tracks.size(); ++i)
        {
            if (! visibleRow (i)) { if (trackTop (i) > H) break; continue; }
            for (auto* c : tracks[i]->getClips()) { drawClip (g, *c, i); drawClipHandles (g, *c, i); }
        }
        for (auto& gh : ghosts)
        {   // borrar un clip: se desvanece y se aplana hacia su línea central, sin destellos ni piezas
            const float t = 1.f - gh.a, e = t * t * (3.f - 2.f * t); const float x0 = beatToX (gh.b0), x1 = beatToX (gh.b1), y0 = (float) trackTop (gh.track), hh = (float) hOf (gh.track), w = std::max (2.f, x1 - x0);
            const float h2 = hh * (1.f - .55f * e); auto r = juce::Rectangle<float> (x0, y0 + (hh - h2) * .5f, w, h2);
            g.setColour (gh.col.darker (.2f).withAlpha (.8f * (1.f - e))); g.fillRoundedRectangle (r, 3.f);
        }
        for (int i = 0; i < tracks.size(); ++i) { if (! visibleRow (i)) { if (trackTop (i) > H) break; continue; } if (autoShown (*tracks[i])) drawAuto (g, i); }
        g.restoreState();

        drawRuler (g, W);
        g.setColour (theme::bgBar2); g.fillRect (0, 0, kHeaderW + 6, rulerH()); g.setColour (theme::seam); g.fillRect (0, rulerH() - 1, kHeaderW + 6, 1); g.fillRect (kHeaderW + 5, 0, 1, rulerH());

        {   // esquina de la regla: tonalidad del proyecto y nombre de cada carril
            const auto key = harmony::getKey (edit); auto chip = keyChipR().toFloat();
            g.setColour (key.active() ? accent.withAlpha (.16f) : boxFill); g.fillRoundedRectangle (chip, rad); g.setColour (key.active() ? accent.withAlpha (.55f) : boxLine); g.drawRoundedRectangle (chip.reduced (.5f), rad, 1.f);
            g.setColour (key.active() ? accentHi : textDim); g.setFont (fonts::ui (12.f, true)); g.drawText (harmony::keyText (key), chip.toNearestInt().withTrimmedLeft (8).withTrimmedRight (16), juce::Justification::centredLeft, true);
            icons::draw (g, icons::Chevron, { chip.getRight() - 16.f, chip.getCentreY() - 4.f, 8.f, 8.f }, key.active() ? accentHi : textDim);
            g.setColour (textFaint); g.setFont (fonts::label (10.f)); g.drawText ("Arreglo", 10, kBarsH, kHeaderW - 10, kSecH, juce::Justification::centredLeft);
            if (chordOn) g.drawText ("Acordes", 10, kBarsH + kSecH, kHeaderW - 10, kChordH, juce::Justification::centredLeft);
        }
        for (int i = 0; i < tracks.size(); ++i) { if (! visibleRow (i)) { if (trackTop (i) > H) break; continue; } drawHeader (g, *tracks[i], i); }
        { auto ar = addR(); if (ar.getBottom() > rulerH() && ar.getY() < H) { raised (g, ar, bgBar2);
            icons::draw (g, icons::Plus, { ar.getX() + 6, ar.getY(), 38.f, ar.getHeight() }, iconIdle); g.setColour (textDim); g.setFont (fonts::ui (13.f)); g.drawText ("Agregar pista", ar.withTrimmedLeft (46).toNearestInt(), juce::Justification::centredLeft); } }
        if (marquee && ! mqRect.isEmpty()) { const auto red = theme::accentHi; g.setColour (red.withAlpha (.12f * mqFade)); g.fillRect (mqRect); g.setColour (red.withAlpha (.9f)); g.drawRect (mqRect, 1.f); }
        drawCut (g);
        drawScrollbars (g);

        // cabezal
        const float px = beatToX (timeToBeats (smoothPos()));
        if (magnetBeat >= 0 && dragMode != 0)
        {
            const float mx = beatToX (magnetBeat); if (mx >= kHeaderW + 6 && mx <= W) { g.setColour (theme::accentHi.withAlpha (magnetHit ? .95f : .35f)); g.fillRect (mx - (magnetHit ? 1.f : .5f), (float) rulerH(), magnetHit ? 2.f : 1.f, (float) (H - rulerH())); }
        }
        if (px >= kHeaderW + 6 && px <= W)
        {
            g.setColour (juce::Colours::black.withAlpha (.35f)); g.fillRect (px + .5f, (float) rulerH(), 1.f, (float) (H - rulerH()));
            g.setColour (juce::Colour (0xfff4f5f7).withAlpha (.9f)); g.fillRect (px - .5f, (float) kBarsH - 4, 1.f, (float) (H - kBarsH + 4));
            g.setColour (accent); { juce::Path fl; fl.addRoundedRectangle (px, kBarsH - 21.f, 26.f, 14.f, 2.f); fl.addTriangle (px, kBarsH - 8.f, px + 7.f, kBarsH - 8.f, px, kBarsH - 2.f); g.fillPath (fl); }
            g.setColour (juce::Colour (0xff17181b)); g.setFont (fonts::digits (10.5f));
            g.drawText (juce::String ((int) (timeToBeats (smoothPos()) / timesig::bar()) + 1), juce::Rectangle<int> ((int) px + 4, kBarsH - 21, 22, 14), juce::Justification::centredLeft);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        altDown = e.mods.isAltDown();
        grabKeyboardFocus(); sbDrag = 0; edit.getUndoManager().beginNewTransaction();   // cada gesto = un solo "deshacer"
        if (tool == "auto" && e.x >= kHeaderW + 6 && e.y >= rulerH() && ! (needHScroll() && hScrollR().expanded (0, 4).contains (e.position)) && ! vScrollR().expanded (4, 0).contains (e.position)) { autoDown (e); return; }
        if (e.mods.isPopupMenu())
        {   // sobre los clips: si arrastras con el boton derecho BORRAS lo que cruzas; si solo haces clic, sale el menu al soltar
            if (e.x >= kHeaderW + 6 && e.y >= rulerH()) { rightDown = true; rightErase = false; rightLast = e.position; rightStart = e.position; rightEv = std::make_unique<juce::MouseEvent> (e); return; }
            contextMenu (e); return;
        }
        if ((needHScroll() && hScrollR().expanded (0, 4).contains (e.position))) { sbDrag = 1; sbGrab = e.x - hThumb().getX(); if (! hThumb().contains (e.position)) sbGrab = hThumb().getWidth() * .5f; dragScroll (e); return; }
        if (vScrollR().expanded (4, 0).contains (e.position)) { sbDrag = 2; sbGrab = e.y - vThumb().getY(); if (! vThumb().contains (e.position)) sbGrab = vThumb().getHeight() * .5f; dragScroll (e); return; }
        scrubbing = false; brushSrc = nullptr; brushTrack = -1; lastDT = 0; brushNew.clear(); brushLastIdx = LONG_MIN; brushLastTrack = -1;
        if (keyChipR().contains (e.getPosition())) { keyMenu(); return; }
        if (chordOn && e.y >= kBarsH + kSecH && e.y < rulerH() && e.x >= kHeaderW + 6)
        {   // carril de acordes: clic = ir al inicio; arrastrar = mover; borde derecho = duracion
            const int ci = chordAtX ((float) e.x);
            if (ci >= 0) { auto cs = harmony::list (edit); chIdx = ci; chMoved = false; chMode = std::abs (e.position.x - beatToX (cs[(size_t) ci].end())) < 7.f ? 2 : 1; chGrab = xToBeat ((float) e.x) - cs[(size_t) ci].beat; chCur = xToBeat ((float) e.x); return; }
            chIdx = -1; chMode = 0; return;
        }
        if (e.y >= kBarsH && e.y < kBarsH + kSecH && e.x >= kHeaderW + 6)
        {   // carril de secciones: clic = ir al inicio; arrastrar = reordenar; borde derecho = cambiar duracion
            const int si = secAtX ((float) e.x);
            if (si >= 0) { auto ss = arranger::list (edit); secIdx = si; secMoved = false; secMode = std::abs (e.position.x - beatToX (ss[(size_t) si].end())) < 7.f ? 2 : 1; secGrab = xToBeat ((float) e.x) - ss[(size_t) si].beat; secCur = xToBeat ((float) e.x); return; }
            secIdx = -1; secMode = 0; return;
        }
        if (e.y < rulerH() && e.x >= kHeaderW) { scrubbing = true; seekTo (e.x); return; }
        dragClip = nullptr; dragMode = 0;
        if (const int ed = edgeAt (e.getPosition()); ed >= 0) { resizing = ed; resizeStartY = e.position.y; resizeStartH = hOf (ed); return; }
        if (e.x < kHeaderW + 6) { headerClick (e); return; }
        const int ti = trackAt ((float) e.y); if (ti < 0) { selected = nullptr; repaint(); return; }
        if (ti != selTrack) { selTrack = ti; if (onTrackSelected) onTrackSelected (ti); }
        auto tr = te::getAudioTracks (edit)[ti];
        const double t = beatsToTime (xToBeat ((float) e.x));
        te::Clip* hit = nullptr;
        for (auto* c : tr->getClips()) if (c->getPosition().time.contains (te::TimePosition::fromSeconds (t))) hit = c;
        if (hit != nullptr && tryHandleDown (e, hit, ti)) return;
        if (tool == "cut" && hit != nullptr && isLocked (hit)) { selected = hit; repaint(); return; }
        if (tool == "cut" && hit != nullptr) { const double cb = snapBeat (xToBeat ((float) e.x)); tr->splitAt (te::TimePosition::fromSeconds (beatsToTime (cb))); flashBeat = cb; flashTrack = ti; flashA = 1.f; cutTrack = -1; repaint(); return; }
        if (tool == "brush")
        {   // pinta copias del clip bajo el cursor; si empiezas en vacío usa el clip seleccionado o el último que pintaste
            auto alive = [] (te::Clip* c) { return c != nullptr && c->getTrack() != nullptr; };
            if (alive (selected)) lastTouched = te::Clip::Ptr (selected);
            brushSrc = hit != nullptr ? hit : alive (lastTouched.get()) ? lastTouched.get() : nullptr;
            if (brushSrc != nullptr) lastTouched = te::Clip::Ptr (brushSrc);
            brushTrack = ti; if (hit != nullptr) selected = hit; brushPaint (ti, xToBeat ((float) e.x)); repaint(); return;
        }
        bool placedNew = false;
        if (tool == "pencil" && hit == nullptr && lastTouched != nullptr && lastTouched->getTrack() != nullptr)
        {   // lápiz: pega el último clip que tocaste en el hueco donde haces clic y lo puedes arrastrar sin soltar
            auto* nc = trackutil::copyClipTo (edit, *lastTouched, *tr, te::TimePosition::fromSeconds (beatsToTime (std::max (0.0, snapBeat (xToBeat ((float) e.x))))));
            if (nc != nullptr) { hit = nc; placedNew = true; if (onTracksChanged) onTracksChanged(); }
        }
        if (tool == "pencil" && hit == nullptr)
        {
            const double sb = std::max (0.0, std::floor (xToBeat ((float) e.x) / timesig::bar()) * timesig::bar());
            auto c = tr->insertMIDIClip ("MIDI", te::TimeRange (te::TimePosition::fromSeconds (beatsToTime (sb)), te::TimePosition::fromSeconds (beatsToTime (sb + 16.0))), nullptr);
            selected = c.get(); repaint(); return;
        }
        if (tool == "select" && hit == nullptr && e.x >= kHeaderW + 6)   // selector: rectángulo para elegir varios clips
        { marquee = true; mqStart = e.position; mqRect = {}; group.clear(); selected = nullptr; dragClip = nullptr; repaint(); return; }
        if (hit == nullptr) group.clear();
        else if (e.mods.isShiftDown()) { if (inGroup (hit)) group.removeObject (hit); else group.add (hit); }
        else if (! inGroup (hit)) { group.clear(); group.add (hit); }
        selected = hit; dragClip = hit; if (hit != nullptr && ! isLocked (hit) && (tool == "select" || tool == "pencil")) lastTouched = te::Clip::Ptr (hit);   // el lápiz recuerda el último clip que tocaste
        if (isLocked (hit)) dragClip = nullptr;   // clip bloqueado: se elige pero no se arrastra
        if (hit && ! isLocked (hit))
        {
            const float x0 = beatToX (timeToBeats (hit->getPosition().getStart().inSeconds())), x1 = beatToX (timeToBeats (hit->getPosition().getEnd().inSeconds()));
            const float zone = tool == "trim" ? 16.f : juce::jmin (tool == "pencil" ? 11.f : 7.f, (x1 - x0) * .3f);
            if (e.x >= x1 - zone) dragMode = 2; else if (e.x <= x0 + zone) dragMode = 1; else if (tool == "trim") dragClip = nullptr;
            if (placedNew) dragMode = 0;   // recién pegado: arrastrar lo mueve, no recorta
            origStartBeat = timeToBeats (hit->getPosition().getStart().inSeconds()); origEndBeat = timeToBeats (hit->getPosition().getEnd().inSeconds()); magnetBeat = -1; magnetHit = false;
        }
        if (hit) { dragOffsetBeats = xToBeat ((float) e.x) - timeToBeats (hit->getPosition().getStart().inSeconds()); }
        dragOrig.clear(); downTrack = ti; auto allT = te::getAudioTracks (edit);
        if (hit && dragMode == 0 && (tool == "select" || tool == "pencil")) for (auto* c : group) { if (c->getTrack() == nullptr || isLocked (c)) continue; int idx = 0; for (int k = 0; k < allT.size(); ++k) if (allT[k] == c->getTrack()) idx = k; dragOrig.push_back ({ c, timeToBeats (c->getPosition().getStart().inSeconds()), idx }); }
        repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (keyChipR().contains (e.getPosition())) return;
        if (chordOn && e.y >= kBarsH + kSecH && e.y < rulerH() && e.x >= kHeaderW + 6) { const int ci = chordAtX ((float) e.x); chordDialog (ci, snapBeat (std::max (0.0, xToBeat ((float) e.x)))); return; }
        if (e.y >= kBarsH && e.y < kBarsH + kSecH && e.x >= kHeaderW + 6) { const int si = secAtX ((float) e.x); if (si >= 0) renameSectionDialog (si); else cmdSectionsAt (snapBeatBar (xToBeat ((float) e.x))); return; }
        { te::WaveAudioClip* hw = nullptr; if (const int hm = handleModeAt (e.position, &hw); hm != 0 && hw != nullptr) { edit.getUndoManager().beginNewTransaction(); if (hm == 1) hw->setFadeIn (te::TimeDuration()); else if (hm == 2) hw->setFadeOut (te::TimeDuration()); else hw->setGainDB (0.f); repaint(); return; } }
        const int ti = trackAt ((float) e.y);
        if (ti >= 0 && e.x < kHeaderW + 6 && ! dotsR (ti).contains (e.position) && ! chevronR (ti).expanded (4.f).contains (e.position)) { auto* tk = te::getAudioTracks (edit)[ti]; tk->setMute (! tk->isMuted (false)); repaint(); return; }
        if (ti < 0 || e.x < kHeaderW + 6) return;
        const double t = beatsToTime (xToBeat ((float) e.x));
        openClipAt (ti, t);
    }
    // Abre el clip que hay en esa pista y ese instante: MIDI en el Piano Roll, audio en el editor de audio. Devuelve true si abriÃ³ algo.
    bool openClipAt (int ti, double t)
    {
        auto tl = te::getAudioTracks (edit); if (ti < 0 || ti >= tl.size()) return false;
        for (auto* c : tl[ti]->getClips())
        {
            if (! c->getPosition().time.contains (te::TimePosition::fromSeconds (t))) continue;
            if (auto* mc = dynamic_cast<te::MidiClip*> (c)) { if (onOpenMidi) { onOpenMidi (mc); return true; } }
            else if (auto* wc = dynamic_cast<te::WaveAudioClip*> (c)) { selected = c; if (onOpenAudio) { onOpenAudio (wc); return true; } }
        }
        return false;
    }
    // Borrar con el boton derecho: recorre el trayecto del raton (sin saltarse nada aunque vaya rapido) y borra los clips que cruza
    void rightEraseTo (juce::Point<float> to)
    {
        const auto from = rightLast; const float dist = from.getDistanceFrom (to); const int steps = std::max (1, (int) (dist / 3.f)); auto tl = te::getAudioTracks (edit); juce::Array<te::Clip*> victims;
        for (int k = 0; k <= steps; ++k)
        {
            const auto p = from + (to - from) * ((float) k / (float) steps); if (p.x < kHeaderW + 6 || p.y < rulerH()) continue;
            const int ti = trackAt (p.y); if (ti < 0 || ti >= tl.size()) continue; const double tt = beatsToTime (xToBeat (p.x));
            for (auto* c : tl[ti]->getClips()) if (c->getPosition().time.contains (te::TimePosition::fromSeconds (tt))) victims.addIfNotAlreadyThere (c);
        }
        for (auto* c : victims) { if (isLocked (c)) continue; if (selected == c) selected = nullptr; group.removeObject (c); if (dragClip == c) dragClip = nullptr; killClip (c); }
        rightLast = to; if (! victims.isEmpty()) repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (chMode != 0 && chIdx >= 0) { chCur = xToBeat ((float) e.x); if (std::abs (e.getDistanceFromDragStartX()) > 3) chMoved = true; repaint(); return; }
        if (secMode != 0 && secIdx >= 0) { secCur = xToBeat ((float) e.x); if (std::abs (e.getDistanceFromDragStartX()) > 3) secMoved = true; repaint(); return; }
        if (rightDown)
        {
            if (! rightErase && e.position.getDistanceFrom (rightStart) < 5.f) return;
            if (! rightErase) { rightErase = true; rightLast = rightStart; } rightEraseTo (e.position); return;
        }
        altDown = e.mods.isAltDown();
        if (fadeMode != 0) { handleDrag (e); return; }
        if (autoIdx >= 0) { autoDrag (e); return; }
        if (scrubbing) { seekTo (e.x); return; }   // arrastrar el cabezal en la regla (aunque haya un clip seleccionado)
        if (brushTrack >= 0) { if (const int ti = trackAt ((float) e.y); ti >= 0 && e.x >= kHeaderW + 6) brushPaint (ti, xToBeat ((float) e.x)); return; }
        if (resizing >= 0) { setTrackHeight (resizing, resizeStartH + (int) (e.position.y - resizeStartY)); repaint(); return; }
        if (sbDrag) { dragScroll (e); return; }
        if (marquee)
        {
            mqRect = juce::Rectangle<float>::leftTopRightBottom (std::min (mqStart.x, (float) e.x), std::min (mqStart.y, (float) e.y), std::max (mqStart.x, (float) e.x), std::max (mqStart.y, (float) e.y));
            group.clear(); auto tl = te::getAudioTracks (edit);
            for (int i = 0; i < tl.size(); ++i) { const float y0 = (float) trackTop (i), y1 = y0 + (float) hOf (i); if (y1 < mqRect.getY() || y0 > mqRect.getBottom()) continue;
                for (auto* c : tl[i]->getClips()) { const float x0 = beatToX (timeToBeats (c->getPosition().getStart().inSeconds())), x1 = beatToX (timeToBeats (c->getPosition().getEnd().inSeconds())); if (x1 >= mqRect.getX() && x0 <= mqRect.getRight()) group.add (c); } }
            selected = group.size() == 1 ? group.getUnchecked (0).get() : nullptr; repaint(); return;
        }
        if (dragClip == nullptr || (tool != "select" && tool != "trim" && tool != "pencil")) return;
        const double cs = timeToBeats (dragClip->getPosition().getStart().inSeconds()), ce = timeToBeats (dragClip->getPosition().getEnd().inSeconds());
        // Imán: al acercarse al borde que tenía el clip antes de empezar a arrastrar, se pega; si sigues jalando, se suelta.
        auto magnet = [this] (double nb, double orig) { return applyMagnet (nb, orig); };
        if (dragMode == 2) { const double nb = std::max (cs + 0.25, magnet (snapBeat (xToBeat ((float) e.x)), origEndBeat)); dragClip->setEnd (te::TimePosition::fromSeconds (beatsToTime (nb)), true); repaint(); return; }
        if (dragMode == 1) { const double nb = juce::jlimit (0.0, ce - 0.25, magnet (snapBeat (xToBeat ((float) e.x)), origStartBeat)); dragClip->setStart (te::TimePosition::fromSeconds (beatsToTime (nb)), true, false); repaint(); return; }
        if (tool == "trim") return;
        // mover: todos los clips elegidos juntos, en horizontal y también a otras pistas
        double primary = 0.0, minOrig = 1e9; int minT = 1 << 20, maxT = -1;
        for (auto& o : dragOrig) { if (o.clip == dragClip) primary = o.beat; minOrig = std::min (minOrig, o.beat); minT = std::min (minT, o.track); maxT = std::max (maxT, o.track); }
        if (dragOrig.empty()) { const double nb = std::max (0.0, snapBeat (xToBeat ((float) e.x) - dragOffsetBeats)); dragClip->setStart (te::TimePosition::fromSeconds (beatsToTime (nb)), false, true); repaint(); return; }
        double delta = snapBeat (xToBeat ((float) e.x) - dragOffsetBeats) - primary; delta = std::max (delta, -minOrig);
        auto allT = te::getAudioTracks (edit); const int tNow = trackAt ((float) e.y); int dT = tNow >= 0 ? tNow - downTrack : lastDT; dT = juce::jlimit (-minT, allT.size() - 1 - maxT, dT); lastDT = dT;
        for (auto& o : dragOrig) { if (o.clip->getTrack() == nullptr) continue; auto* dest = allT[o.track + dT]; if (o.clip->getTrack() != dest) o.clip->moveTo (*dest); o.clip->setStart (te::TimePosition::fromSeconds (beatsToTime (std::max (0.0, o.beat + delta))), false, true); }
        repaint();
    }
    void setZoom (double ppb) { pxPerBeat = ppb; repaint(); }
    void resetView() { scrollX = 0; scrollY = 0; repaint(); }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (chMode != 0 && chIdx >= 0) { commitChord(); return; }
        if (secMode != 0 && secIdx >= 0) { commitSection(); return; }
        if (rightDown) { const bool menu = ! rightErase; rightDown = false; rightErase = false; auto ev = std::move (rightEv); repaint(); if (menu && ev != nullptr) contextMenu (*ev); return; }
        mouseUpNormal();
    }
    void mouseUpNormal() { fadeMode = 0; fadeClip = nullptr; altDown = false; magnetBeat = -1; magnetHit = false; autoIdx = -1; sbDrag = 0; dragMode = 0; resizing = -1; scrubbing = false; brushTrack = -1; brushSrc = nullptr; brushNew.clear(); brushLastIdx = LONG_MIN; brushLastTrack = -1; marquee = false; dragOrig.clear(); lastDT = 0; repaint(); }
    // Pincel: sobre un hueco pinta clips MIDI de 1 compás; sobre un clip, lo copia a lo largo del arrastre (sin pisar otros clips).
    void brushPaint (int ti, double beat)
    {   // pinta el clip en el hueco bajo el cursor y en TODOS los huecos entre la ultima posicion y esta (si arrastras rapido el raton salta y no deja vacios)
        auto tracks = te::getAudioTracks (edit); if (ti < 0 || ti >= tracks.size()) return; auto* tr = tracks[ti];
        if (brushSrc != nullptr && brushSrc->getTrack() == nullptr) brushSrc = nullptr;
        double s0 = 0.0, len = 4.0;
        if (brushSrc != nullptr) { s0 = timeToBeats (brushSrc->getPosition().getStart().inSeconds()); len = timeToBeats (brushSrc->getPosition().getEnd().inSeconds()) - s0; if (len <= 0.0) return; }
        const long cur = (long) std::floor ((beat - s0) / len); long from = cur;
        if (brushLastTrack == ti && brushLastIdx != LONG_MIN) from = brushLastIdx;
        const long step = cur >= from ? 1 : -1; bool any = false;
        for (long k = from; ; k += step)
        {
            const double pos = s0 + (double) k * len;
            if (pos > -1e-6 && (k != brushLastIdx || brushLastTrack != ti || k == cur))
            {
                const auto a = te::TimePosition::fromSeconds (beatsToTime (pos)), b = te::TimePosition::fromSeconds (beatsToTime (pos + len)); bool busy = false;
                for (auto* c : tr->getClips()) if (c->getPosition().getStart() < b - te::TimeDuration::fromSeconds (1e-4) && c->getPosition().getEnd() > a + te::TimeDuration::fromSeconds (1e-4)) { busy = true; break; }
                if (! busy)
                {
                    te::Clip* nc = brushSrc != nullptr ? trackutil::copyClipTo (edit, *brushSrc, *tr, a) : tr->insertMIDIClip ("MIDI", te::TimeRange (a, b), nullptr).get();
                    if (nc != nullptr) { selected = nc; brushNew.add (te::Clip::Ptr (nc)); any = true; }
                }
            }
            if (k == cur) break;
        }
        brushLastTrack = ti; brushLastIdx = cur;
        if (any) { if (onTracksChanged) onTracksChanged(); repaint(); }
    }
    void updateCut (const juce::MouseEvent& e)
    {
        int ti = -1; double b = -1.0;
        if (tool == "cut" && e.x >= kHeaderW + 6 && e.y >= rulerH())
        {
            ti = trackAt ((float) e.y);
            if (ti >= 0)
            {
                b = snapBeat (xToBeat ((float) e.x)); bool over = false; const double tt = beatsToTime (b);
                for (auto* c : te::getAudioTracks (edit)[ti]->getClips()) if (tt > c->getPosition().getStart().inSeconds() + 1e-4 && tt < c->getPosition().getEnd().inSeconds() - 1e-4) over = true;
                if (! over) ti = -1;
            }
        }
        if (ti != cutTrack || (ti >= 0 && std::abs (b - cutBeat) > 1e-9)) { if (ti != cutTrack) cutX = -1.f; cutTrack = ti; cutBeat = b; repaint(); }
    }
    void drawCut (juce::Graphics& g)
    {
        const juce::Colour blue (0xff3da5ff);
        auto line = [&] (int ti, float x, float a, float w, juce::Colour c)
        {
            if (ti < 0) return; if (x < kHeaderW + 6 || x > getWidth()) return; const float y0 = (float) trackTop (ti), y1 = y0 + (float) hOf (ti);
            g.setColour (c.withAlpha (a * .22f)); g.fillRect (x - w * 3.f, y0, w * 6.f, y1 - y0);
            g.setColour (c.withAlpha (a * .45f)); g.fillRect (x - w * 1.5f, y0, w * 3.f, y1 - y0);
            g.setColour (juce::Colours::white.withAlpha (a)); g.fillRect (x - w * .5f, y0, w, y1 - y0);
            g.setColour (c.withAlpha (a)); juce::Path tri; tri.addTriangle (x - 6.f, y0 - 1.f, x + 6.f, y0 - 1.f, x, y0 + 7.f); g.fillPath (tri);
            juce::Path tri2; tri2.addTriangle (x - 6.f, y1 + 1.f, x + 6.f, y1 + 1.f, x, y1 - 7.f); g.fillPath (tri2);
        };
        if (cutTrack >= 0 && tool == "cut")
        {
            const float x = cutX >= 0.f ? cutX : beatToX (cutBeat);
            if (std::abs (cutV) > 30.f) { const float y0 = (float) trackTop (cutTrack), y1 = y0 + (float) hOf (cutTrack), tail = juce::jlimit (-60.f, 60.f, -cutV * .045f); g.setColour (blue.withAlpha (.16f)); g.fillRect (std::min (x, x + tail), y0, std::abs (tail), y1 - y0); }   // estela
            line (cutTrack, x, .95f, 1.4f + juce::jmin (2.5f, std::abs (cutV) / 500.f), blue);
        }
        if (flashA > 0.f) line (flashTrack, beatToX (flashBeat), flashA, 1.4f + 5.f * (1.f - flashA), blue);
    }
    void mouseExit (const juce::MouseEvent&) override { if (cutTrack >= 0) { cutTrack = -1; cutX = -1.f; repaint(); } }
    void mouseMove (const juce::MouseEvent& e) override
    {
        { const bool inChord = chordOn && e.y >= kBarsH + kSecH && e.y < rulerH() && e.x >= kHeaderW + 6; const int hc = inChord ? chordAtX ((float) e.x) : -1; if (hc != chHot) { chHot = hc; repaint(); }
          if (inChord) { auto cs = harmony::list (edit); const bool edge = hc >= 0 && std::abs (e.position.x - beatToX (cs[(size_t) hc].end())) < 7.f; setMouseCursor (edge ? juce::MouseCursor::LeftRightResizeCursor : (hc >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor)); return; } }
        { const bool inLane = e.y >= kBarsH && e.y < kBarsH + kSecH && e.x >= kHeaderW + 6; const int h = inLane ? secAtX ((float) e.x) : -1;
          if (h != secHot) { secHot = h; repaint(); }
          if (inLane) { auto ss = arranger::list (edit); const bool edge = h >= 0 && std::abs (e.position.x - beatToX (ss[(size_t) h].end())) < 7.f; setMouseCursor (edge ? juce::MouseCursor::LeftRightResizeCursor : (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor)); return; } }
        auto cur = juce::MouseCursor::NormalCursor;
        updateCut (e); if (tool == "cut" && e.x >= kHeaderW + 6 && e.y >= rulerH()) { setMouseCursor (cursors::forTool ("cut")); return; }
        if ((tool == "brush" || tool == "pencil") && e.x >= kHeaderW + 6 && e.y >= rulerH() && ! (needHScroll() && hScrollR().expanded (0, 4).contains (e.position))) { setMouseCursor (cursors::forTool (tool)); return; }
        if (edgeAt (e.getPosition()) >= 0) { setMouseCursor (juce::MouseCursor::UpDownResizeCursor); return; }
        if (const int hm = handleModeAt (e.position); hm != 0) { setMouseCursor (hm == 3 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::LeftRightResizeCursor); return; }
        if (e.x >= kHeaderW + 6 && e.y >= rulerH() && (tool == "select" || tool == "trim" || tool == "pencil"))
            if (auto ti = trackAt ((float) e.y); ti >= 0)
                for (auto* c : te::getAudioTracks (edit)[ti]->getClips())
                {
                    const float x0 = beatToX (timeToBeats (c->getPosition().getStart().inSeconds())), x1 = beatToX (timeToBeats (c->getPosition().getEnd().inSeconds()));
                    if (e.x >= x0 && e.x <= x1 && (e.x >= x1 - 7 || e.x <= x0 + 7 || tool == "trim")) cur = juce::MouseCursor::LeftRightResizeCursor;
                }
        setMouseCursor (cur);
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (hScrollR().expanded (0, 6).contains (e.position)) { zoomAt ((float) kHeaderW + 6 + (float) (getWidth() - kHeaderW) * 0.5f, w.deltaY != 0 ? w.deltaY : w.deltaX); return; }
        if (e.mods.isAltDown()) { setAllHeights ((int) std::round (trackH * (w.deltaY > 0 ? 1.15 : 1.0 / 1.15))); scrollY = juce::jlimit (0.0, juce::jmax (0.0, contentH() - (getHeight() - rulerH())), scrollY); }
        else if (e.mods.isCtrlDown() || e.mods.isCommandDown() || (e.y < rulerH() && e.x >= kHeaderW))   // rueda sobre la regla = zoom
        {
            zoomAt ((float) e.x, w.deltaY);
        }
        else if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY)) tgtX = juce::jmax (0.0, (tgtX >= 0 ? tgtX : scrollX) - (w.deltaX != 0 ? w.deltaX : w.deltaY) * 600.0);   // el desplazamiento arranca y frena con suavidad
        else tgtY = juce::jlimit (0.0, juce::jmax (0.0, contentH() - (getHeight() - rulerH())), (tgtY >= 0 ? tgtY : scrollY) - w.deltaY * 400.0);
        repaint();
    }
    bool keyPressed (const juce::KeyPress& k) override
    {
        auto lower = [&] { return juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) k.getKeyCode()); };
        if (! k.getModifiers().isCommandDown() && ! k.getModifiers().isAltDown() && ! selectedClips().isEmpty())
        {
            if (k.getKeyCode() == juce::KeyPress::leftKey || k.getKeyCode() == juce::KeyPress::rightKey) { nudgeSelection (k.getKeyCode() == juce::KeyPress::rightKey ? 1 : -1, k.getModifiers().isShiftDown()); return true; }
            if (lower() == 's') { splitSelectionAtPlayhead(); return true; }
        }
        if (k.getModifiers().isCommandDown() && lower() == 'l' && ! selectedClips().isEmpty()) { toggleLockSelection(); return true; }
        if (k.getModifiers().isCommandDown() && lower() == 'm') { addMarker (snapBeat (timeToBeats (edit.getTransport().getPosition().inSeconds()))); return true; }
        if (k.getModifiers().isCommandDown() && k.getKeyCode() == juce::KeyPress::rightKey) { jumpToMarker (1); return true; }
        if (k.getModifiers().isCommandDown() && k.getKeyCode() == juce::KeyPress::leftKey) { jumpToMarker (-1); return true; }
        if (k.getModifiers().isCommandDown() && lower() == 'c' && (selected != nullptr || group.size() > 0)) { copySelection(); return true; }
        if (k.getModifiers().isCommandDown() && lower() == 'v' && ! clipboard.empty()) { pasteAtPlayhead(); return true; }
        if (k.getModifiers().isCommandDown() && lower() == 'x' && (selected != nullptr || group.size() > 0)) { copySelection(); return keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)); }
        if (k.getModifiers().isCommandDown() && juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) k.getKeyCode()) == 'a')   // Ctrl+A: elegir todos los clips
        { group.clear(); for (auto* t : te::getAudioTracks (edit)) for (auto* c : t->getClips()) group.add (c); selected = group.size() == 1 ? group.getUnchecked (0).get() : nullptr; repaint(); return true; }
        if (! k.getModifiers().isCommandDown() && lower() == 'm' && (selected != nullptr || group.size() > 0))   // M = silenciar / activar los clips elegidos (como en FL Studio)
        { juce::Array<te::Clip*> cs; if (group.size() > 1) for (auto* c : group) cs.add (c); else if (selected) cs.add (selected); bool any = false; for (auto* c : cs) any = any || ! c->isMuted(); edit.getUndoManager().beginNewTransaction(); for (auto* c : cs) c->setMuted (any); repaint(); return true; }
        if (k == juce::KeyPress::escapeKey && (selected != nullptr || group.size() > 0)) { group.clear(); selected = nullptr; repaint(); return true; }
        if (k.getModifiers().isCommandDown() && juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) k.getKeyCode()) == 'd' && (selected != nullptr || group.size() > 0)) { if (group.size() > 1) { juce::ReferenceCountedArray<te::Clip> nw; for (auto* c : group) if (auto* nc = trackutil::duplicateClip (edit, *c)) nw.add (nc); group = nw; selected = nullptr; } else if (auto* nc = trackutil::duplicateClip (edit, *selected)) { selected = nc; group.clear(); group.add (nc); } repaint(); return true; }
        if ((k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) && (selected != nullptr || group.size() > 0)) { if (group.size() > 1) { for (auto* c : group) killClip (c); } else if (selected) killClip (selected); group.clear(); selected = nullptr; dragClip = nullptr; repaint(); return true; }
        return false;
    }

    // arrastrar archivos de audio desde el explorador a una pista
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (auto& f : files) if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) return true;
        return false;
    }
    void filesDropped (const juce::StringArray& files, int x, int y) override
    {
        int ti = trackAt ((float) y); if (ti < 0) ti = 0; if (numTracks() == 0) return;
        auto tr = te::getAudioTracks (edit)[ti]; double beat = std::max (0.0, snapBeat (xToBeat ((float) x)));
        for (auto& f : files)
        {
            juce::File file (f); if (! file.hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) continue;
            te::AudioFile af (edit.engine, file); const double len = af.getLength(); if (len <= 0) continue;
            const auto start = te::TimePosition::fromSeconds (beatsToTime (beat));
            tr->insertWaveClip (file.getFileNameWithoutExtension(), file, { te::TimeRange (start, te::TimeDuration::fromSeconds (len)), te::TimeDuration() }, false);
            beat += timeToBeats (len);
        }
        repaint();
    }

    struct Ghost { double b0, b1; int track; juce::Colour col; float a; };
    std::vector<Ghost> ghosts;
    // Borra un clip con animación: deja una sombra roja que se encoge y se desvanece donde estaba
    // Bloquear clip: no se mueve, no se recorta, no se corta, no se borra y no se le tocan fundidos ni ganancia. Se guarda en el proyecto.
    static bool isLocked (te::Clip* c) { return c != nullptr && (bool) c->state.getProperty ("injLocked", false); }
    void toggleLockSelection()
    {
        auto cs = selectedClips(); if (cs.isEmpty()) return; bool anyOpen = false; for (auto* c : cs) anyOpen = anyOpen || ! isLocked (c);
        edit.getUndoManager().beginNewTransaction(); for (auto* c : cs) c->state.setProperty ("injLocked", anyOpen, &edit.getUndoManager()); repaint();
    }
    void killClip (te::Clip* c)
    {
        if (c == nullptr || isLocked (c)) return;
        if (auto* tr = dynamic_cast<te::AudioTrack*> (c->getTrack())) { auto all = te::getAudioTracks (edit); const int ti = all.indexOf (tr);
            if (ti >= 0) { const int cc = (int) c->state.getProperty ("injColour", -1); ghosts.push_back ({ timeToBeats (c->getPosition().getStart().inSeconds()), timeToBeats (c->getPosition().getEnd().inSeconds()), ti, cc != -1 ? juce::Colour ((juce::uint32) cc) : trackutil::colourOf (tr, ti), 1.f }); } }
        c->removeFromParent();
    }
    // ---- funciones nuevas (v0.9.13)
    bool follow = false;   // seguir el cabezal: la vista avanza sola mientras suena
    double projectEndBeat() const { double e = 0; for (auto* t : te::getAudioTracks (edit)) for (auto* c : t->getClips()) e = std::max (e, timeToBeats (c->getPosition().getEnd().inSeconds())); return e; }
    juce::Array<te::Clip*> selectedClips() const { juce::Array<te::Clip*> v; if (group.size() > 1) { for (auto* c : group) if (c->getTrack() != nullptr) v.add (c); } else if (selected != nullptr && selected->getTrack() != nullptr) v.add (selected); return v; }
    void zoomFit (bool selOnly)
    {
        double b0 = 0, b1 = projectEndBeat();
        if (selOnly) { auto cs = selectedClips(); if (cs.isEmpty()) return; b0 = 1e9; b1 = 0; for (auto* c : cs) { b0 = std::min (b0, timeToBeats (c->getPosition().getStart().inSeconds())); b1 = std::max (b1, timeToBeats (c->getPosition().getEnd().inSeconds())); } }
        if (b1 - b0 < 0.5) return;
        const double avail = std::max (80.0, (double) getWidth() - kHeaderW - 6 - 40.0);
        zoomAnchorX = (float) kHeaderW + 6.f; tgtX = tgtY = -1; scrollX = std::max (0.0, b0 * pxPerBeat); zoomTarget = (float) juce::jlimit (0.25, 400.0, avail / (b1 - b0)); repaint();
    }
    void cancelZoomAnim() { zoomTarget = -1.f; }
    void say (const juce::String& s) { if (onInstrument) onInstrument (-1, s); }
    void normaliseClip (te::WaveAudioClip& wc)
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (wc.getOriginalFile())); if (! rd) { say (juce::String::fromUTF8 ("No se pudo leer el audio")); return; }
        float pk = 0.f; juce::Range<float> r[8]; const int nc = (int) std::min<unsigned> (rd->numChannels, 8u); std::vector<juce::Range<float>> rr ((size_t) nc); rd->readMaxLevels (0, rd->lengthInSamples, rr.data(), nc); for (auto& q : rr) pk = std::max (pk, std::max (std::abs (q.getStart()), std::abs (q.getEnd())));
        if (pk < 1e-5f) { say (juce::String::fromUTF8 ("El clip está en silencio")); return; }
        const float g = juce::jlimit (-24.f, 24.f, -juce::Decibels::gainToDecibels (pk) - 0.3f); edit.getUndoManager().beginNewTransaction(); wc.setGainDB (g); say (juce::String::fromUTF8 ("Normalizado: ") + juce::String (g, 1) + " dB"); repaint();
    }
    void reverseClip (te::WaveAudioClip& wc, te::AudioTrack& tr)
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (wc.getOriginalFile())); if (! rd || rd->lengthInSamples <= 0) { say (juce::String::fromUTF8 ("No se pudo leer el audio")); return; }
        const int n = (int) rd->lengthInSamples; juce::AudioBuffer<float> b ((int) rd->numChannels, n); rd->read (&b, 0, n, 0, true, true);
        for (int c = 0; c < b.getNumChannels(); ++c) std::reverse (b.getWritePointer (c), b.getWritePointer (c) + n);
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("INJUSTICE DAW").getChildFile ("Invertidos"); dir.createDirectory();
        auto out = dir.getChildFile (wc.getOriginalFile().getFileNameWithoutExtension() + "_inv_" + juce::String (juce::Time::currentTimeMillis()) + ".wav");
        { juce::WavAudioFormat wf; std::unique_ptr<juce::FileOutputStream> os (out.createOutputStream()); if (! os) return; std::unique_ptr<juce::AudioFormatWriter> w (wf.createWriterFor (os.get(), rd->sampleRate, (unsigned) b.getNumChannels(), 24, {}, 0)); if (! w) return; os.release(); w->writeFromAudioSampleBuffer (b, 0, n); }
        edit.getUndoManager().beginNewTransaction(); const auto rng = wc.getPosition().time; const auto nm = wc.getName() + " (inv)"; const float gdb = wc.getGainDB(); te::Clip* old = &wc;
        auto nc = tr.insertWaveClip (nm, out, { rng, te::TimeDuration() }, false); if (nc != nullptr) { if (auto* nw = dynamic_cast<te::WaveAudioClip*> (nc.get())) nw->setGainDB (gdb); selected = nc.get(); group.clear(); killClip (old); say (juce::String::fromUTF8 ("Audio invertido (clip nuevo)")); }
        repaint();
    }
    void compareTones()
    {
        juce::Array<te::WaveAudioClip*> ws; for (auto* c : group) if (auto* w = dynamic_cast<te::WaveAudioClip*> (c)) ws.add (w); if (ws.size() != 2) return;
        const auto a = sampleanalysis::analyse (ws[0]->getOriginalFile()), b = sampleanalysis::analyse (ws[1]->getOriginalFile());
        if (! a.oneShot || ! b.oneShot || a.midi < 0 || b.midi < 0) { say (juce::String::fromUTF8 ("Sirve para sonidos sueltos (un kick y un bajo), no para loops largos")); return; }
        const int d = ((a.midi - b.midi) % 12 + 12) % 12; juce::String v = d == 0 ? juce::String::fromUTF8 ("afinados") : (d == 1 || d == 11) ? juce::String::fromUTF8 ("a 1 semitono: chocan") : (d == 2 || d == 10) ? juce::String::fromUTF8 ("a 2 semitonos: se nota desafinado") : juce::String::fromUTF8 ("notas distintas (bien si es a propósito)");
        say (ws[0]->getName() + ": " + sampleanalysis::noteName (a.midi) + "  |  " + ws[1]->getName() + ": " + sampleanalysis::noteName (b.midi) + "  ->  " + v);
    }
    void nudgeSelection (int dir, bool bigStep)
    {
        auto cs = selectedClips(); if (cs.isEmpty()) return; edit.getUndoManager().beginNewTransaction();
        const double st = bigStep ? timesig::bar() : snapStep();
        for (auto* c : cs) { if (isLocked (c)) continue; const double nb = std::max (0.0, timeToBeats (c->getPosition().getStart().inSeconds()) + dir * st); c->setStart (te::TimePosition::fromSeconds (beatsToTime (nb)), false, true); }
        repaint();
    }
    void splitSelectionAtPlayhead()
    {
        const double t = edit.getTransport().getPosition().inSeconds(); auto cs = selectedClips(); juce::Array<te::AudioTrack*> trs;
        for (auto* c : cs) if (! isLocked (c) && c->getPosition().time.contains (te::TimePosition::fromSeconds (t))) if (auto* at = dynamic_cast<te::AudioTrack*> (c->getTrack())) trs.addIfNotAlreadyThere (at);
        if (trs.isEmpty()) return; edit.getUndoManager().beginNewTransaction(); for (auto* at : trs) at->splitAt (te::TimePosition::fromSeconds (t)); repaint();
    }
    void selectTrackClips (int ti)
    {
        auto ts = te::getAudioTracks (edit); group.clear(); if (ti < 0 || ti >= ts.size()) { selected = nullptr; return; } for (auto* c : ts[ti]->getClips()) group.add (c); selected = group.size() == 1 ? group.getUnchecked (0).get() : nullptr; selTrack = ti; repaint();
    }
    void loopClip (te::Clip& c) { auto& tc = edit.getTransport(); tc.setLoopRange (c.getPosition().time); tc.looping = true; repaint(); }
    double tgtX = -1, tgtY = -1;   // destino del scroll suave
    int selTrack = -1;   // pista marcada (se resalta en la lista de pistas)
    // Referencia a un clip que lo mantiene vivo: si el clip se borra (suprimir, deshacer...) el puntero no queda colgando y se puede comprobar con getTrack() == nullptr.
    struct ClipRef { te::Clip::Ptr p; ClipRef() = default; ClipRef (te::Clip* c) : p (c) {} ClipRef& operator= (te::Clip* c) { p = c; return *this; } operator te::Clip*() const { return p.get(); } te::Clip* operator->() const { return p.get(); } };
    ClipRef selected; std::function<void (te::MidiClip*)> onOpenMidi; std::function<void (te::WaveAudioClip*)> onOpenAudio; std::function<void()> onTracksChanged; std::function<void()> onClipChanged2; std::function<void (int)> onTrackSelected;

private:
public:
    double applyMagnet (double nb, double orig) { magnetBeat = orig; magnetHit = std::abs (beatToX (nb) - beatToX (orig)) <= kMagnetPx; return magnetHit ? orig : nb; }
    void zoomAt (float x, double deltaY) { tgtX = tgtY = -1; const double base = zoomTarget > 0.f ? (double) zoomTarget : pxPerBeat; zoomTarget = (float) juce::jlimit (0.25, 400.0, base * (1.0 + deltaY * 0.6)); zoomAnchorX = x; }
    double zoomPx() const { return zoomTarget > 0.f ? (double) zoomTarget : pxPerBeat; }
private:
    static constexpr float kMagnetPx = 9.f; double origStartBeat = 0, origEndBeat = 0, magnetBeat = -1; bool magnetHit = false;
    int dragMode = 0, sbDrag = 0; float sbGrab = 0;   // dragMode: 0 mover, 1 recortar inicio, 2 recortar final
    struct Orig { te::Clip::Ptr clip; double beat; int track; };
    juce::ReferenceCountedArray<te::Clip> group; std::vector<Orig> dragOrig; bool marquee = false; juce::Point<float> mqStart; juce::Rectangle<float> mqRect; int downTrack = 0, lastDT = 0;
    bool inGroup (te::Clip* c) const { return group.contains (c); }
    bool rightDown = false, rightErase = false; juce::Point<float> rightLast, rightStart; std::unique_ptr<juce::MouseEvent> rightEv;   // clic derecho: borrar arrastrando
    juce::ReferenceCountedArray<te::Clip> brushNew; int brushLastTrack = -1; long brushLastIdx = LONG_MIN;   // pincel: clips de este trazo (se ven rojos) y ultimo hueco pintado
    bool scrubbing = false; ClipRef brushSrc; te::Clip::Ptr lastTouched; int brushTrack = -1;
    te::Edit& edit; double pxPerBeat = 3.6, scrollX = 0, scrollY = 0, dragOffsetBeats = 0; ClipRef dragClip;
    juce::AudioFormatManager fm; juce::AudioThumbnailCache thumbCache; std::map<juce::String, std::unique_ptr<juce::AudioThumbnail>> thumbs;


    //---- barras de desplazamiento
    double contentBeats() const { double e = 64; for (auto* t : te::getAudioTracks (edit)) for (auto* c : t->getClips()) e = std::max (e, timeToBeats (c->getPosition().getEnd().inSeconds())); return e + 32; }
    double contentH() const { ensureLayout(); return (double) tops.back() + trackH + kGap + 30; }
    juce::Rectangle<float> hScrollR() const { return { (float) kHeaderW + 6, (float) getHeight() - 28.f, (float) getWidth() - kHeaderW - 22.f, 20.f }; }
    bool needHScroll() const { return true; }   // la barra de abajo siempre está
    juce::Rectangle<float> vScrollR() const { return { (float) getWidth() - 11.f, (float) rulerH(), 8.f, (float) getHeight() - rulerH() - 14.f }; }
    juce::Rectangle<float> hThumb() const { auto r = hScrollR(); const double tot = contentBeats() * pxPerBeat, vis = getWidth() - kHeaderW - 6; const float w = (float) juce::jlimit (30.0, (double) r.getWidth(), r.getWidth() * vis / tot); return { r.getX() + (float) ((r.getWidth() - w) * juce::jlimit (0.0, 1.0, scrollX / std::max (1.0, tot - vis))), r.getY(), w, r.getHeight() }; }
    juce::Rectangle<float> vThumb() const { auto r = vScrollR(); const double tot = contentH(), vis = getHeight() - rulerH(); const float h = (float) juce::jlimit (30.0, (double) r.getHeight(), r.getHeight() * vis / tot); return { r.getX(), r.getY() + (float) ((r.getHeight() - h) * juce::jlimit (0.0, 1.0, scrollY / std::max (1.0, tot - vis))), r.getWidth(), h }; }
    void dragScroll (const juce::MouseEvent& e)
    {
        tgtX = tgtY = -1;
        if (sbDrag == 1) { auto r = hScrollR(); const double tot = contentBeats() * pxPerBeat, vis = getWidth() - kHeaderW - 6; const float tw = hThumb().getWidth(); scrollX = juce::jlimit (0.0, std::max (0.0, tot - vis), (double) ((e.x - sbGrab - r.getX()) / std::max (1.f, r.getWidth() - tw)) * (tot - vis)); }
        else { auto r = vScrollR(); const double tot = contentH(), vis = getHeight() - rulerH(); const float th = vThumb().getHeight(); scrollY = juce::jlimit (0.0, std::max (0.0, tot - vis), (double) ((e.y - sbGrab - r.getY()) / std::max (1.f, r.getHeight() - th)) * (tot - vis)); }
        repaint();
    }
    void drawScrollbars (juce::Graphics& g)
    {
        const bool needH = true, needV = contentH() > getHeight() - rulerH();
        g.setColour (juce::Colours::white.withAlpha (.07f));
        if (needH)
        {   // vista general: miniatura de todo el proyecto; el recuadro es lo que se ve ahora
            g.setColour (theme::bgBar2); g.fillRect (0, getHeight() - 30, getWidth(), 30); g.setColour (theme::seam); g.fillRect (0, getHeight() - 30, getWidth(), 1);
            auto r = hScrollR(); g.setColour (theme::boxFill); g.fillRoundedRectangle (r, theme::rad); g.setColour (theme::seam); g.drawRoundedRectangle (r.reduced (.5f), theme::rad, 1.f);
            const double tot = contentBeats(); int n = 4; { auto tt = te::getAudioTracks (edit); for (int i = 0; i < tt.size(); ++i) if (! tt[i]->getClips().isEmpty()) n = std::max (n, i + 1); }
            const float rowH = std::min (6.f, (r.getHeight() - 6.f) / (float) n);
            g.saveState(); g.reduceClipRegion (r.toNearestInt());
            auto tracks = te::getAudioTracks (edit);
            for (int i = 0; i < tracks.size(); ++i)
            {
                const int cv = (int) tracks[i]->state.getProperty ("injColour", -1); const auto col = cv != -1 ? juce::Colour ((juce::uint32) cv) : theme::trackColours[i % 8];
                for (auto* c : tracks[i]->getClips())
                {
                    const double b0 = timeToBeats (c->getPosition().getStart().inSeconds()), b1 = timeToBeats (c->getPosition().getEnd().inSeconds());
                    const float x0 = r.getX() + (float) (b0 / tot) * r.getWidth(), x1 = r.getX() + (float) (b1 / tot) * r.getWidth();
                    const int cc = (int) c->state.getProperty ("injColour", -1);
                    g.setColour ((cc != -1 ? juce::Colour ((juce::uint32) cc) : col).withAlpha (.85f)); g.fillRect (x0, r.getY() + 3.f + (float) i * rowH, std::max (1.5f, x1 - x0), std::max (1.f, rowH - 0.5f));
                }
            }
            g.restoreState();
            auto th = hThumb(); g.setColour (theme::accent.withAlpha (.08f)); g.fillRoundedRectangle (th, theme::rad); g.setColour (theme::accent.withAlpha (.7f)); g.drawRoundedRectangle (th.reduced (.5f), theme::rad, 1.f);
        }
        g.setColour (juce::Colours::white.withAlpha (.07f));
        if (needV) { g.setColour (theme::boxFill.withAlpha (.8f)); g.fillRoundedRectangle (vScrollR(), theme::rad); g.setColour (juce::Colour (0xff6a6d77).withAlpha (.8f)); g.fillRoundedRectangle (vThumb(), theme::rad); }
    }

    //---- cabeceras de pista: nombre, M/S, men\xc3\xba y "agregar pista"
    juce::Rectangle<int> headerR (int i) const { return { 6, trackTop (i), kHeaderW, hOf (i) }; }
    juce::Rectangle<float> chevronR (int i) const { auto r = headerR (i); const float ry = r.getHeight() >= 40 ? 7.f : ((float) r.getHeight() - 15.f) * .5f; return { (float) r.getX() + 8.f, (float) r.getY() + ry, 15.f, 15.f }; }
    bool isFolded (int i) const { auto tr = te::getAudioTracks (edit); return i >= 0 && i < tr.size() && (bool) tr[i]->state.getProperty ("injFold", false); }
    void toggleFold (int i)
    {   // la flechita pliega la pista a su altura mínima y la devuelve a la que tenía
        auto tr = te::getAudioTracks (edit); if (i < 0 || i >= tr.size()) return; auto& st = tr[i]->state;
        if (isFolded (i)) { setTrackHeight (i, (int) st.getProperty ("injHPrev", trackH)); st.setProperty ("injFold", false, nullptr); }
        else { st.setProperty ("injHPrev", hOf (i), nullptr); setTrackHeight (i, kMinH); st.setProperty ("injFold", true, nullptr); }
        repaint();
    }
    juce::Rectangle<float> dotsR (int i) const { auto r = headerR (i); const float h = (float) r.getHeight(); const float s = std::min (26.f, h - 2.f); return { (float) r.getRight() - 24.f, (float) r.getY() + (h >= 40.f ? 4.f : (h - s) * .5f), 22.f, s }; }
    juce::Rectangle<float> addR() const { return { 6.f, (float) trackTop (numTracks()) + 6.f, (float) kHeaderW - 6.f, 42.f }; }
    void headerClick (const juce::MouseEvent& e)
    {
        auto tracks = te::getAudioTracks (edit); const auto p = e.position;
        if (addR().contains (p)) { if (trackutil::addTrack (edit) != nullptr) { layoutDirty = true; if (onTracksChanged) onTracksChanged(); if (onTrackSelected) onTrackSelected (numTracks() - 1); } scrollY = juce::jmax (0.0, contentH() - (getHeight() - rulerH())); repaint(); return; }
        for (int i = 0; i < tracks.size(); ++i)
        {
            if (! headerR (i).contains (e.getPosition())) continue;
            selTrack = i; if (onTrackSelected) onTrackSelected (i);
            if (e.mods.isAltDown()) { tracks[i]->setSolo (! tracks[i]->isSolo (false)); repaint(); return; }
            if (dotsR (i).contains (p)) { trackMenu (i); return; }
            if (chevronR (i).expanded (4.f).contains (p)) { toggleFold (i); return; }
            return;
        }
    }
    void trackMenu (int i)
    {
        auto tracks = te::getAudioTracks (edit); if (i >= tracks.size()) return;
        te::AudioTrack::Ptr t (tracks[i]); juce::PopupMenu m, col;
        m.addItem (1, juce::String::fromUTF8 ("Renombrar…"));
        m.addItem (2, t->isMuted (false) ? juce::String::fromUTF8 ("Quitar silencio") : juce::String::fromUTF8 ("Silenciar"));
        m.addItem (3, t->isSolo (false) ? "Quitar solo" : "Solo");
        m.addItem (32, juce::String::fromUTF8 ("Seleccionar todos los clips de esta pista"), ! t->getClips().isEmpty());
        m.addItem (33, t->isFrozen (te::Track::individualFreeze) ? juce::String::fromUTF8 ("Descongelar pista") : juce::String::fromUTF8 ("Congelar pista (convierte a audio, ahorra CPU)"));
        static const char* cn[] { "Violeta", "Magenta", "Rosa", "Azul", "Cian", "Verde", "Naranja", "Amarillo" };
        for (int c = 0; c < 8; ++c) col.addColouredItem (10 + c, cn[c], theme::trackColours[c], true, false, nullptr);
        m.addSubMenu ("Color", col);
        { juce::PopupMenu hm; hm.addItem (20, juce::String::fromUTF8 ("Pequeñas (todas)")); hm.addItem (21, "Medianas (todas)"); hm.addItem (22, "Grandes (todas)"); hm.addItem (23, juce::String::fromUTF8 ("Esta pista: pequeña")); hm.addItem (24, "Esta pista: grande"); m.addSubMenu ("Altura de pistas", hm); }
        m.addSeparator();
        {
            juce::PopupMenu im; juce::String lastG;
            for (int k : instruments::sortedIdx()) { const auto& pr = instruments::list()[(size_t) k]; if (lastG.isNotEmpty() && lastG != pr.group) im.addSeparator(); lastG = pr.group; im.addItem (500 + k, juce::String (pr.group) + ": " + pr.name); }
            im.addSeparator(); for (size_t k = 0; k < instruments::kits().size(); ++k) im.addItem (600 + (int) k, juce::String::fromUTF8 ("Batería: ") + instruments::kits()[k].name);
            m.addSubMenu ("Instrumento incluido", im);
        }
        {
            auto* vp = autoParamOf (*t); const juce::String an = autoParamName (*t);
            m.addItem (6, autoShown (*t) ? juce::String::fromUTF8 ("Ocultar automatización") : juce::String::fromUTF8 ("Mostrar automatización: ") + an);
            m.addItem (7, juce::String::fromUTF8 ("Borrar automatización de ") + an, vp != nullptr && vp->getCurve().getNumPoints() > 0);
            juce::PopupMenu ap; const juce::String cur = t->state.getProperty ("injAutoParam", "vol").toString();
            ap.addItem (300, "Volumen", true, cur == "vol"); ap.addItem (301, "Pan", true, cur == "pan");
            auto pl = t->pluginList.getPlugins();
            for (int pi = 0; pi < pl.size(); ++pi)
            {
                if (dynamic_cast<te::VolumeAndPanPlugin*> (pl[pi].get()) || dynamic_cast<te::LevelMeterPlugin*> (pl[pi].get())) continue;
                juce::PopupMenu pm; const int np = std::min (200, pl[pi]->getNumAutomatableParameters());
                for (int qi = 0; qi < np; ++qi) { auto prm = pl[pi]->getAutomatableParameter (qi); if (prm == nullptr) continue; pm.addItem (1000 + pi * 256 + qi, prm->getParameterName(), true, cur == "P" + pl[pi]->itemID.toString() + "|" + prm->paramID); }
                if (np > 0) ap.addSubMenu (pl[pi]->getName(), pm);
            }
            m.addSubMenu (juce::String::fromUTF8 ("Parámetro a automatizar"), ap);
        }
        { const auto gid = trackutil::groupOf (*t);
          m.addItem (40, juce::String::fromUTF8 ("Agrupar con la pista de arriba"), i > 0);
          if (gid.isNotEmpty())
          {
              juce::PopupMenu gm; gm.addItem (41, juce::String::fromUTF8 ("Renombrar grupo…")); gm.addItem (42, juce::String::fromUTF8 ("Plegar o desplegar el grupo"));
              gm.addItem (43, trackutil::groupMuted (edit, gid) ? juce::String::fromUTF8 ("Quitar silencio al grupo") : juce::String::fromUTF8 ("Silenciar el grupo")); gm.addItem (44, juce::String::fromUTF8 ("Subir 1 dB al grupo")); gm.addItem (45, juce::String::fromUTF8 ("Bajar 1 dB al grupo"));
              gm.addSeparator(); gm.addItem (46, juce::String::fromUTF8 ("Quitar esta pista del grupo")); m.addSubMenu (juce::String::fromUTF8 ("Grupo: ") + trackutil::groupName (*t), gm);
          } }
        {   // sidechain: esta pista baja cuando suena otra
            juce::PopupMenu sc; auto* cur = trackutil::sidechainSource (edit, *t); int shown = 0;
            for (int k = 0; k < tracks.size(); ++k) if (tracks[k] != t.get() && ! tracks[k]->getClips().isEmpty()) { sc.addItem (700 + k, tracks[k]->getName() == "Track" ? juce::String::fromUTF8 ("Pista ") + juce::String (k + 1) : tracks[k]->getName(), true, cur == tracks[k]); ++shown; }
            if (shown == 0) sc.addItem (699, juce::String::fromUTF8 ("(no hay otras pistas con clips)"), false);
            if (cur != nullptr)
            {
                sc.addSeparator(); static const double am[] { 0.4, 0.7, 0.9, 1.0 }; static const char* an[] { "Suave", "Medio", "Fuerte", "Total" }; static const double rl[] { 60, 100, 150, 250 };
                for (int k = 0; k < 4; ++k) sc.addItem (800 + k, juce::String ("Fuerza: ") + an[k], true, std::abs (trackutil::sidechainAmount (*t) - am[k]) < 0.01);
                for (int k = 0; k < 4; ++k) sc.addItem (810 + k, juce::String::fromUTF8 ("Recuperación: ") + juce::String ((int) rl[k]) + " ms", true, std::abs (trackutil::sidechainRelease (*t) - rl[k]) < 1.0);
                sc.addSeparator(); sc.addItem (790, "Quitar sidechain");
            }
            m.addSubMenu (cur != nullptr ? juce::String::fromUTF8 ("Sidechain: baja con ") + (cur->getName() == "Track" ? juce::String::fromUTF8 ("otra pista") : cur->getName()) : juce::String::fromUTF8 ("Sidechain (que baje con otra pista)"), sc);
        }
        if (ourPlugins)
        {   // atajo a tus plugins (distorsión, compresor, etc.) sin abrir la lista completa
            juce::PopupMenu fx; auto ours = ourPlugins(); int n = 0;
            for (int k = 0; k < ours.size(); ++k) if (! ours.getReference (k).isInstrument) { fx.addItem (900 + k, ours.getReference (k).name); ++n; }
            if (n == 0) fx.addItem (899, juce::String::fromUTF8 ("(no encuentro tus plugins; escanéalos en Herramientas > Plugins)"), false);
            m.addSubMenu ("Efectos INJUSTICE AUDIO", fx);
        }
        m.addSeparator();
        m.addItem (4, "Agregar pista al final"); m.addItem (5, "Eliminar pista", tracks.size() > 1);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (dotsR (i).toNearestInt())), [this, t, i] (int r)
        {
            if (r == 32) selectTrackClips (i);
            else if (r == 33) { edit.getUndoManager().beginNewTransaction(); t->setFrozen (! t->isFrozen (te::Track::individualFreeze), te::Track::individualFreeze); say (t->isFrozen (te::Track::individualFreeze) ? juce::String::fromUTF8 ("Pista congelada") : juce::String::fromUTF8 ("Pista descongelada")); repaint(); }
            else if (r == 1) trackutil::renameDialog (edit, t.get(), [this] { if (onTracksChanged) onTracksChanged(); repaint(); });
            else if (r == 2) t->setMute (! t->isMuted (false));
            else if (r == 3) t->setSolo (! t->isSolo (false));
            else if (r == 20) setAllHeights (34); else if (r == 21) setAllHeights (76); else if (r == 22) setAllHeights (116);
            else if (r == 23) setTrackHeight (i, 34); else if (r == 24) setTrackHeight (i, 116);
            else if (r >= 900 && r < 1000) { if (ourPlugins && onAddPlugin) { auto ours = ourPlugins(); if (r - 900 < ours.size()) onAddPlugin (ours.getReference (r - 900), i); } }
            else if (r >= 700 && r < 790) { auto tl = te::getAudioTracks (edit); if (r - 700 < tl.size()) trackutil::setSidechain (edit, *t, tl[r - 700], trackutil::sidechainPlugin (*t) ? trackutil::sidechainAmount (*t) : 0.7, trackutil::sidechainPlugin (*t) ? trackutil::sidechainRelease (*t) : 120.0); repaint(); }
            else if (r == 790) { trackutil::removeSidechain (*t); repaint(); }
            else if ((r >= 800 && r < 804) || (r >= 810 && r < 814)) { static const double am[] { 0.4, 0.7, 0.9, 1.0 }; static const double rl[] { 60, 100, 150, 250 }; if (auto* src = trackutil::sidechainSource (edit, *t)) trackutil::setSidechain (edit, *t, src, r < 810 ? am[r - 800] : trackutil::sidechainAmount (*t), r >= 810 ? rl[r - 810] : trackutil::sidechainRelease (*t)); }
            else if (r >= 10 && r < 18) trackutil::setColour (edit, t.get(), r - 10);
            else if (r >= 500 && r < 500 + (int) instruments::list().size()) { if (instruments::addSynth (edit, *t, r - 500) != nullptr) { repaint(); if (onInstrument) onInstrument (i, juce::String (instruments::list()[(size_t) (r - 500)].name)); } }
            else if (r >= 600 && r < 600 + (int) instruments::kits().size())
            {
                if (instruments::addKit (edit, *t, samplepack::packsDir(), r - 600) != nullptr) { repaint(); if (onInstrument) onInstrument (i, instruments::kits()[(size_t) (r - 600)].name); }
                else if (onInstrument) onInstrument (-1, juce::String::fromUTF8 ("Los sonidos se están creando; inténtalo en unos segundos."));
            }
            else if (r == 40 && i > 0) { auto all = te::getAudioTracks (edit); edit.getUndoManager().beginNewTransaction(); trackutil::joinGroup (edit, *t, *all[i - 1]); }
            else if (r == 41) { const auto gid = trackutil::groupOf (*t); auto* aw = new juce::AlertWindow (juce::String::fromUTF8 ("Nombre del grupo"), {}, juce::MessageBoxIconType::NoIcon); aw->addTextEditor ("n", trackutil::groupName (*t), "");
                aw->addButton ("Aceptar", 1, juce::KeyPress (juce::KeyPress::returnKey)); aw->addButton ("Cancelar", 0, juce::KeyPress (juce::KeyPress::escapeKey)); juce::Component::SafePointer<PlaylistView> self (this);
                aw->enterModalState (true, juce::ModalCallbackFunction::create ([self, aw, gid] (int rr) { if (rr == 1 && self != nullptr) { const auto nm = aw->getTextEditorContents ("n").trim(); if (nm.isNotEmpty()) { trackutil::renameGroup (self->edit, gid, nm); self->repaint(); } } }), true); }
            else if (r == 42) { const auto gid = trackutil::groupOf (*t); auto mem = trackutil::groupMembers (edit, gid); bool anyOpen = false; for (auto* x : mem) if (! (bool) x->state.getProperty ("injFold", false)) anyOpen = true;
                auto all = te::getAudioTracks (edit); for (auto* x : mem) { const int ix = all.indexOf (x); if (ix >= 0 && isFolded (ix) != anyOpen) toggleFold (ix); } }
            else if (r == 43) { const auto gid = trackutil::groupOf (*t); trackutil::groupMute (edit, gid, ! trackutil::groupMuted (edit, gid)); }
            else if (r == 44) trackutil::groupGainDb (edit, trackutil::groupOf (*t), 1.f);
            else if (r == 45) trackutil::groupGainDb (edit, trackutil::groupOf (*t), -1.f);
            else if (r == 46) { edit.getUndoManager().beginNewTransaction(); trackutil::leaveGroup (edit, *t); }
            else if (r == 6) t->state.setProperty ("injAuto", ! autoShown (*t), nullptr);
            else if (r == 7) { if (auto* vp = autoParamOf (*t)) { edit.getUndoManager().beginNewTransaction(); vp->getCurve().clear(); } }
            else if (r == 300) { t->state.setProperty ("injAutoParam", "vol", nullptr); t->state.setProperty ("injAuto", true, nullptr); }
            else if (r == 301) { t->state.setProperty ("injAutoParam", "pan", nullptr); t->state.setProperty ("injAuto", true, nullptr); }
            else if (r >= 1000) { auto pl = t->pluginList.getPlugins(); const int pi = (r - 1000) / 256, qi = (r - 1000) % 256; if (pi < pl.size()) if (auto prm = pl[pi]->getAutomatableParameter (qi)) { t->state.setProperty ("injAutoParam", "P" + pl[pi]->itemID.toString() + "|" + prm->paramID, nullptr); t->state.setProperty ("injAuto", true, nullptr); } }
            else if (r == 4) { trackutil::addTrack (edit); if (onTracksChanged) onTracksChanged(); if (onTrackSelected) onTrackSelected (numTracks() - 1); }
            else if (r == 5) { if (selected != nullptr && selected->getTrack() == t.get()) selected = nullptr; dragClip = nullptr; trackutil::deleteTrack (edit, t.get()); }
            layoutDirty = true; if (r != 0 && onTracksChanged) onTracksChanged(); repaint();
        });
    }
    // ---- marcadores: se guardan en el proyecto (hijos INJMARKER de edit.state) ----
public:
    struct Marker { juce::ValueTree v; double beat; juce::String name; };
    std::vector<Marker> markers() const
    {
        std::vector<Marker> out; for (auto c : edit.state) if (c.hasType ("INJMARKER")) out.push_back ({ c, (double) c.getProperty ("beat", 0.0), c.getProperty ("name", "").toString() });
        std::sort (out.begin(), out.end(), [] (const Marker& a, const Marker& b) { return a.beat < b.beat; }); return out;
    }
    void addMarker (double beat, const juce::String& name = {})
    {
        juce::ValueTree v ("INJMARKER"); v.setProperty ("beat", beat, nullptr); v.setProperty ("name", name.isEmpty() ? juce::String (markers().size() + 1) : name, nullptr);
        edit.getUndoManager().beginNewTransaction ("Marcador"); edit.state.addChild (v, -1, &edit.getUndoManager()); repaint();
    }
    int markerAt (float x) const { auto ms = markers(); for (int i = 0; i < (int) ms.size(); ++i) { const float mx = beatToX (ms[(size_t) i].beat); if (x >= mx - 4 && x <= mx + 8 + 7.f * (float) std::min (14, ms[(size_t) i].name.length()) + 4) return i; } return -1; }
    void jumpToMarker (int dir)   // -1 anterior, +1 siguiente
    {
        auto ms = markers(); const double cur = timeToBeats (edit.getTransport().getPosition().inSeconds()); const double eps = 1e-3;
        if (dir > 0) { for (auto& m : ms) if (m.beat > cur + eps) { edit.getTransport().setPosition (te::TimePosition::fromSeconds (beatsToTime (m.beat))); repaint(); return; } }
        else for (int i = (int) ms.size() - 1; i >= 0; --i) if (ms[(size_t) i].beat < cur - eps) { edit.getTransport().setPosition (te::TimePosition::fromSeconds (beatsToTime (ms[(size_t) i].beat))); repaint(); return; }
    }


    // ---- tonalidad y pista de acordes: ver Harmony.h ----
    int chHot = -1, chIdx = -1, chMode = 0; double chGrab = 0, chCur = 0; bool chMoved = false;
    void openKeyMenu() { keyMenu(); }
    juce::Rectangle<int> keyChipR() const { return { 8, 9, kHeaderW - 6, 22 }; }
    int chordAtX (float x) const { auto cs = harmony::list (edit); for (int i = 0; i < (int) cs.size(); ++i) if (x >= beatToX (cs[(size_t) i].beat) && x < beatToX (cs[(size_t) i].end())) return i; return -1; }
    static std::vector<te::MidiClip*> midiOf (const juce::Array<te::Clip*>& cs) { std::vector<te::MidiClip*> o; for (auto* c : cs) if (auto* m = dynamic_cast<te::MidiClip*> (c)) o.push_back (m); return o; }
    // Herramientas de notas (ver MidiTools.h) sobre los clips MIDI elegidos. Devuelve cuántas notas quedan escritas (0 = no hizo nada).
    int cmdNotes (const juce::String& op)
    {
        namespace mt = miditools; auto* um = &edit.getUndoManager(); auto mcs = midiOf (selectedClips()); int n = 0;
        if (mcs.empty()) { say (juce::String::fromUTF8 ("Elige primero un clip MIDI")); return 0; }
        auto sameNotes = [] (mt::Notes a, mt::Notes b)
        {
            auto key = [] (const mt::Nt& x, const mt::Nt& y) { return x.start != y.start ? x.start < y.start : (x.pitch != y.pitch ? x.pitch < y.pitch : x.len < y.len); };
            if (a.size() != b.size()) return false; std::sort (a.begin(), a.end(), key); std::sort (b.begin(), b.end(), key);
            for (size_t i = 0; i < a.size(); ++i) if (a[i].pitch != b[i].pitch || a[i].vel != b[i].vel || std::abs (a[i].start - b[i].start) > 1e-7 || std::abs (a[i].len - b[i].len) > 1e-7) return false;
            return true;
        };
        bool started = false, any = false;
        for (auto* m : mcs)
        {
            if (isLocked (m)) continue; auto& seq = m->getSequence(); if (seq.getNumNotes() == 0) continue; any = true;
            mt::Notes in; for (auto* nt : seq.getNotes()) in.push_back ({ nt->getNoteNumber(), nt->getStartBeat().inBeats(), nt->getLengthBeats().inBeats(), nt->getVelocity() });
            const double L = timeToBeats (m->getPosition().getEnd().inSeconds()) - timeToBeats (m->getPosition().getStart().inSeconds()); mt::Notes out; bool known = true;
            if (op.startsWith ("arp"))
            {
                auto& s = mt::arpSettings(); if (op == "arp_up") s.pattern = mt::Up; else if (op == "arp_down") s.pattern = mt::Down; else if (op == "arp_updown") s.pattern = mt::UpDown; else if (op == "arp_downup") s.pattern = mt::DownUp;
                else if (op == "arp_random") { s.pattern = mt::Random; s.seed = (int) (juce::Time::getMillisecondCounter() & 0xffff); } else if (op == "arp_played") s.pattern = mt::AsPlayed;
                out = mt::arpeggiate (in, s, L);
            }
            else if (op == "humanize") out = mt::humanize (in, 0.03, 8, (int) (juce::Time::getMillisecondCounter() & 0xffff), L);
            else if (op == "legato") out = mt::legato (in);
            else if (op == "rampup") out = mt::velRamp (in, 30, 127);
            else if (op == "rampdown") out = mt::velRamp (in, 127, 30);
            else if (op == "strumdown") out = mt::strum (in, 1.0 / 32.0, true);
            else if (op == "strumup") out = mt::strum (in, 1.0 / 32.0, false);
            else if (op == "ratchet2") out = mt::ratchet (in, 2, L);
            else if (op == "ratchet3") out = mt::ratchet (in, 3, L);
            else if (op == "ratchet4") out = mt::ratchet (in, 4, L);
            else if (op == "nooverlap") out = mt::noOverlap (in);
            else if (op == "fixlen16") out = mt::fixedLen (in, 0.25);
            else if (op == "octup") out = mt::addVoice (in, 12);
            else if (op == "octdown") out = mt::addVoice (in, -12);
            else if (op == "fifth") out = mt::addVoice (in, 7);
            else if (op == "mirror") out = mt::mirrorPitch (in);
            else if (op == "thin") out = mt::thin (in);
            else if (op == "swing16") out = mt::swing16 (in, 0.5);
            else if (op == "p_n_t1") out = mt::transpose (in, 1);
            else if (op == "p_n_tm1") out = mt::transpose (in, -1);
            else if (op == "p_n_t7") out = mt::transpose (in, 7);
            else if (op == "p_n_tm7") out = mt::transpose (in, -7);
            else if (op == "p_n_vel08") out = mt::velScale (in, 0.8);
            else if (op == "p_n_vel12") out = mt::velScale (in, 1.2);
            else if (op == "p_n_squash") out = mt::velSquash (in);
            else if (op == "p_n_len05") out = mt::lenScale (in, 0.5);
            else if (op == "p_n_len2") out = mt::lenScale (in, 2.0);
            else if (op == "p_n_triad") out = mt::stack (in, { 4, 7 });
            else if (op == "p_n_triadm") out = mt::stack (in, { 3, 7 });
            else if (op == "p_n_seventh") out = mt::stack (in, { 4, 7, 10 });
            else if (op == "p_n_top") out = mt::keepExtreme (in, true);
            else if (op == "p_n_bottom") out = mt::keepExtreme (in, false);
            else if (op == "p_n_shiftl") out = mt::shiftBeats (in, -0.25);
            else if (op == "p_n_shiftr") out = mt::shiftBeats (in, 0.25);
            else if (op == "p_n_accent") out = mt::accent (in);
            else if (op == "p_n_ghost") out = mt::ghost (in);
            else if (op == "p_n_half") out = mt::halfTime (in);
            else if (op == "p_n_double") out = mt::doubleTime (in);
            else if (op == "p_n_scale") { const auto key = harmony::getKey (edit); if (! key.active()) { say (juce::String::fromUTF8 ("Primero elige la tonalidad del proyecto")); return 0; } out = mt::toScaleSteps (in, key.root, harmony::scales()[(size_t) key.scale].steps); }
            else known = false;
            if (! known) return 0;
            if (out.empty() || sameNotes (in, out)) continue;
            if (! started) { um->beginNewTransaction (juce::String::fromUTF8 ("Herramientas de notas")); started = true; }
            std::vector<te::MidiNote*> old; for (auto* nt : seq.getNotes()) old.push_back (nt); for (auto* nt : old) seq.removeNote (*nt, um);
            for (auto& x : out) seq.addNote (x.pitch, te::BeatPosition::fromBeats (std::max (0.0, x.start)), te::BeatDuration::fromBeats (std::max (1.0 / 64.0, x.len)), x.vel, 0, um);
            n += (int) out.size();
        }
        if (! any) say (juce::String::fromUTF8 ("Los clips elegidos no tienen notas")); else if (n == 0) say (juce::String::fromUTF8 ("Eso no cambia nada en las notas elegidas"));
        if (onClipChanged2) onClipChanged2(); repaint(); return n;
    }
    // Operaciones de armonía sobre los clips MIDI elegidos. Devuelve cuántas notas/clips/acordes cambia (0 = no hizo nada).
    int cmdHarmony (const juce::String& op)
    {
        auto* um = &edit.getUndoManager(); const auto key = harmony::getKey (edit); auto mcs = midiOf (selectedClips()); int n = 0;
        auto notesOf = [] (te::MidiClip* m) { std::vector<te::MidiNote*> v; for (auto* nt : m->getSequence().getNotes()) v.push_back (nt); return v; };
        auto absBeat = [this] (te::MidiClip* m, te::MidiNote* nt) { return timeToBeats (m->getPosition().getStart().inSeconds()) + nt->getStartBeat().inBeats() - m->getOffsetInBeats().inBeats(); };
        if (op == "chordlane") { edit.state.setProperty ("injChordLane", ! chordOn, um); refreshChordLane(); layoutDirty = true; repaint(); return 1; }
        if (op == "clearchords") { n = (int) harmony::list (edit).size(); harmony::clearAll (edit); refreshChordLane(); layoutDirty = true; repaint(); return n; }
        if (op == "keynone") { if (! key.active()) return 0; harmony::setKey (edit, { 0, 0 }); repaint(); if (onClipChanged2) onClipChanged2(); return 1; }
        if (op == "chordclip")
        {
            auto cs = harmony::list (edit); auto tr = te::getAudioTracks (edit); if (cs.empty()) { say (juce::String::fromUTF8 ("Primero escribe alg\xc3\xban acorde en el carril de acordes")); return 0; }
            if (selTrack < 0 || selTrack >= tr.size()) { say (juce::String::fromUTF8 ("Elige primero una pista")); return 0; }
            const double b0 = cs.front().beat, b1 = cs.back().end(), bl = timesig::bar(); const int bars = std::max (1, (int) std::ceil ((b1 - b0) / bl - 1e-9));
            if (bars > 256) { say (juce::String::fromUTF8 ("Los acordes abarcan demasiados compases")); return 0; }
            std::vector<ideas::N> nn; for (auto& c : cs) for (int p : harmony::pitches (c, 48)) nn.push_back ({ p, c.beat - b0, c.len, 100 });
            um->beginNewTransaction (juce::String::fromUTF8 ("Clip de acordes")); auto* mc = ideas::insert (edit, *tr[selTrack], b0, bars, nn, "Acordes"); if (mc == nullptr) return 0;
            layoutDirty = true; repaint(); if (onTracksChanged) onTracksChanged(); return (int) nn.size();
        }
        if (mcs.empty()) { say (juce::String::fromUTF8 ("Elige clips MIDI con notas")); return 0; }
        if (op == "detectkey")
        {
            std::vector<int> all; for (auto* m : mcs) for (auto* nt : notesOf (m)) all.push_back (nt->getNoteNumber()); harmony::Key k;
            if (! harmony::detectKey (all, k)) { say (juce::String::fromUTF8 ("No hay notas para deducir la tonalidad")); return 0; }
            harmony::setKey (edit, k); repaint(); say (juce::String::fromUTF8 ("Tonalidad: ") + harmony::keyText (k)); if (onClipChanged2) onClipChanged2(); return 1;
        }
        if (op == "namechord")
        {
            auto* m = mcs.front(); std::vector<int> ps; for (auto* nt : notesOf (m)) ps.push_back (nt->getNoteNumber()); int r = 0, q = 0;
            if (! harmony::detectChord (ps, r, q)) { say (juce::String::fromUTF8 ("No reconozco un acorde con esas notas")); return 0; }
            const double b = timeToBeats (m->getPosition().getStart().inSeconds()), e = timeToBeats (m->getPosition().getEnd().inSeconds());
            if (! harmony::add (edit, b, std::max (0.25, e - b), r, q)) { say (juce::String::fromUTF8 ("Ya hay un acorde en ese punto")); return 0; }
            edit.state.setProperty ("injChordLane", true, um); refreshChordLane (); layoutDirty = true; repaint(); return 1;
        }
        if (op == "snapscale")
        {
            if (! key.active()) { say (juce::String::fromUTF8 ("Elige primero una tonalidad (cuadro arriba a la izquierda)")); return 0; }
            um->beginNewTransaction (juce::String::fromUTF8 ("Ajustar a la escala"));
            for (auto* m : mcs) for (auto* nt : notesOf (m)) { const int t = harmony::snapToScale (key, nt->getNoteNumber()); if (t != nt->getNoteNumber()) { nt->setNoteNumber (juce::jlimit (0, 127, t), um); ++n; } }
        }
        else if (op == "fitkey")
        {
            if (! key.active()) { say (juce::String::fromUTF8 ("Elige primero una tonalidad (cuadro arriba a la izquierda)")); return 0; }
            um->beginNewTransaction (juce::String::fromUTF8 ("Transponer a la tonalidad"));
            for (auto* m : mcs)
            {
                auto ns = notesOf (m); std::vector<int> ps; for (auto* nt : ns) ps.push_back (nt->getNoteNumber()); const int sh = harmony::bestShift (key, ps); if (sh == 0) continue;
                bool ok = true; for (int p : ps) ok = ok && p + sh >= 0 && p + sh <= 127; if (! ok) continue;
                for (auto* nt : ns) nt->setNoteNumber (nt->getNoteNumber() + sh, um); ++n;
            }
        }
        else if (op == "snapchord")
        {
            if (harmony::list (edit).empty()) { say (juce::String::fromUTF8 ("Primero escribe alg\xc3\xban acorde en el carril de acordes")); return 0; }
            um->beginNewTransaction (juce::String::fromUTF8 ("Ajustar al acorde"));
            for (auto* m : mcs) for (auto* nt : notesOf (m))
            {
                const int ci = harmony::indexAt (edit, absBeat (m, nt)); if (ci < 0) continue; const int t = harmony::snapToChord (harmony::list (edit)[(size_t) ci], nt->getNoteNumber());
                if (t != nt->getNoteNumber()) { nt->setNoteNumber (t, um); ++n; }
            }
        }
        if (n > 0 && onClipChanged2) onClipChanged2(); repaint(); return n;
    }
    // ---- pista de arreglo (secciones): ver Arranger.h ----
    int secHot = -1, secIdx = -1, secMode = 0; double secGrab = 0, secCur = 0; bool secMoved = false;
    double barSnap (double b) const { const double bl = timesig::bar(); return std::max (0.0, std::round (b / bl) * bl); }
    double snapBeatBar (double b) const { return barSnap (b); }
    void cmdSectionsAt (double beat) { if (arranger::add (edit, beat, 8 * timesig::bar())) { layoutDirty = true; repaint(); } }
    int secAtX (float x) const { auto ss = arranger::list (edit); for (int i = 0; i < (int) ss.size(); ++i) { const float a = beatToX (ss[(size_t) i].beat), b = beatToX (ss[(size_t) i].end()); if (x >= a && x < b) return i; } return -1; }
    int currentSection() const { return arranger::indexAt (edit, timeToBeats (edit.getTransport().getPosition().inSeconds())); }
    int secTarget() const   // posicion que ocupara la seccion arrastrada al soltarla
    {
        auto ss = arranger::list (edit); if (secIdx < 0 || secIdx >= (int) ss.size()) return secIdx;
        const double centre = secCur - secGrab + ss[(size_t) secIdx].len * .5; int to = 0;
        for (int i = 0; i < (int) ss.size(); ++i) if (i != secIdx && ss[(size_t) i].beat + ss[(size_t) i].len * .5 < centre) ++to;
        return to;
    }
    // Operaciones sobre la seccion bajo el cabezal (o la indicada). Devuelve cuantas cosas cambia (0 = no pudo).
    int cmdSections (const juce::String& op, int idx = -1)
    {
        auto ss = arranger::list (edit); const double now = timeToBeats (edit.getTransport().getPosition().inSeconds()); const double bl = timesig::bar();
        if (idx < 0) idx = arranger::indexAt (edit, now);
        auto needSec = [&] { if (idx < 0 || idx >= (int) ss.size()) { say (juce::String::fromUTF8 ("Pon el cabezal dentro de una secci\xc3\xb3n")); return false; } return true; };
        int r = 0;
        if (op == "new") { r = arranger::add (edit, barSnap (now), 8 * bl) ? 1 : 0; if (! r) say (juce::String::fromUTF8 ("No hay sitio: ya hay una secci\xc3\xb3n en ese punto")); }
        else if (op == "frommarkers") { r = arranger::fromMarkers (edit, 8 * bl); if (! r) say (juce::String::fromUTF8 ("Necesitas marcadores que no tengan ya una secci\xc3\xb3n")); }
        else if (op == "clear") { r = (int) ss.size(); arranger::clearAll (edit); }
        else if (op == "next" || op == "prev")
        {
            const bool nx = op == "next";
            if (nx) { for (auto& s : ss) if (s.beat > now + 1e-3) { edit.getTransport().setPosition (te::TimePosition::fromSeconds (beatsToTime (s.beat))); r = 1; break; } }
            else for (int i = (int) ss.size() - 1; i >= 0; --i) if (ss[(size_t) i].beat < now - 1e-3) { edit.getTransport().setPosition (te::TimePosition::fromSeconds (beatsToTime (ss[(size_t) i].beat))); r = 1; break; }
        }
        else if (! needSec()) return 0;
        else if (op == "del") { arranger::remove (edit, idx); r = 1; }
        else if (op == "colour") { arranger::cycleColour (edit, idx); r = 1; }
        else if (op == "dup") { r = arranger::duplicate (edit, idx) ? 1 : 0; layoutDirty = true; }
        else if (op == "left") { r = arranger::move (edit, idx, idx - 1) ? 1 : 0; if (! r) say (juce::String::fromUTF8 ("Ya es la primera secci\xc3\xb3n")); }
        else if (op == "right") { r = arranger::move (edit, idx, idx + 1) ? 1 : 0; if (! r) say (juce::String::fromUTF8 ("Ya es la \xc3\xbaltima secci\xc3\xb3n")); }
        else if (op == "grow" || op == "shrink") { const double nl = ss[(size_t) idx].len + (op == "grow" ? bl : -bl); r = std::abs (arranger::setLen (edit, idx, nl) - ss[(size_t) idx].len) > 1e-9 ? 1 : 0; }
        else if (op == "loop")
        {
            auto& tc = edit.getTransport(); tc.setLoopRange (te::TimeRange (te::TimePosition::fromSeconds (beatsToTime (ss[(size_t) idx].beat)), te::TimePosition::fromSeconds (beatsToTime (ss[(size_t) idx].end())))); tc.looping = true; r = 1;
        }
        else if (op == "select")
        {
            juce::Array<te::Clip*> cs; for (auto* t : te::getAudioTracks (edit)) for (auto* c : t->getClips()) { const double b = timeToBeats (c->getPosition().getStart().inSeconds()); if (b >= ss[(size_t) idx].beat - 1e-9 && b < ss[(size_t) idx].end() - 1e-9) cs.add (c); }
            selectClips (cs); r = cs.size(); if (! r) say (juce::String::fromUTF8 ("No hay clips en esa secci\xc3\xb3n"));
        }
        layoutDirty = true; repaint(); if (onClipChanged2 && (op == "left" || op == "right" || op == "dup")) onClipChanged2(); return r;
    }
private:
    std::unique_ptr<juce::AlertWindow> markerDlg;

    void commitChord()
    {
        auto cs = harmony::list (edit); const int i = chIdx, mode = chMode; const bool moved = chMoved; const double cur = chCur, grab = chGrab; chIdx = -1; chMode = 0; chMoved = false;
        if (i >= 0 && i < (int) cs.size())
        {
            if (! moved) edit.getTransport().setPosition (te::TimePosition::fromSeconds (beatsToTime (cs[(size_t) i].beat)));
            else if (mode == 1) harmony::moveTo (edit, i, snapBeat (cur - grab));
            else if (mode == 2) harmony::setLen (edit, i, snapBeat (cur) - cs[(size_t) i].beat);
        }
        repaint();
    }
    void chordDialog (int idx, double beat)   // idx >= 0 edita ese acorde; si no, crea uno en `beat`
    {
        auto cs = harmony::list (edit); const bool isNew = idx < 0 || idx >= (int) cs.size();
        markerDlg = std::make_unique<juce::AlertWindow> ("Acorde", "", juce::MessageBoxIconType::NoIcon); markerDlg->addTextEditor ("n", isNew ? juce::String ("C") : cs[(size_t) idx].name(), ""); markerDlg->addButton ("Aceptar", 1); markerDlg->addButton ("Cancelar", 0);
        markerDlg->enterModalState (true, juce::ModalCallbackFunction::create ([this, idx, beat, isNew] (int ok)
        {
            if (ok == 1 && markerDlg)
            {
                int r = 0, q = 0; if (harmony::parse (markerDlg->getTextEditorContents ("n"), r, q))
                {
                    if (isNew) { if (harmony::add (edit, beat, timesig::bar(), r, q)) { edit.state.setProperty ("injChordLane", true, &edit.getUndoManager()); refreshChordLane(); layoutDirty = true; } else say (juce::String::fromUTF8 ("Ya hay un acorde en ese punto")); }
                    else harmony::set (edit, idx, r, q);
                }
                else say (juce::String::fromUTF8 ("No entiendo ese acorde. Prueba con Am, F5, G7 o C#m7"));
            }
            markerDlg.reset(); repaint();
        }), false);
    }
    void chordMenu (const juce::MouseEvent& e)
    {
        const int ci = chordAtX ((float) e.x); const double beat = snapBeat (std::max (0.0, xToBeat ((float) e.x))); const bool on = ci >= 0; juce::PopupMenu m;
        m.addItem (1, juce::String::fromUTF8 ("Nuevo acorde aqu\xc3\xad\xe2\x80\xa6"), ! on); m.addItem (2, juce::String::fromUTF8 ("Editar acorde\xe2\x80\xa6"), on); m.addItem (3, juce::String::fromUTF8 ("Duplicar (copia detr\xc3\xa1s)"), on); m.addItem (4, "Eliminar acorde", on);
        m.addSeparator(); m.addItem (5, juce::String::fromUTF8 ("Crear un clip MIDI con los acordes en la pista elegida")); m.addItem (6, "Poner nombre de acorde al clip MIDI elegido"); m.addItem (7, "Ajustar las notas elegidas a los acordes");
        m.addSeparator(); m.addItem (8, "Quitar todos los acordes", ! harmony::list (edit).empty());
        m.showMenuAsync (juce::PopupMenu::Options(), [this, ci, beat] (int r)
        {
            if (r == 1) chordDialog (-1, beat); else if (r == 2) chordDialog (ci, 0);
            else if (r == 3) { auto cs = harmony::list (edit); if (ci >= 0 && ci < (int) cs.size()) harmony::add (edit, cs[(size_t) ci].end(), cs[(size_t) ci].len, cs[(size_t) ci].root, cs[(size_t) ci].quality); }
            else if (r == 4) harmony::remove (edit, ci); else if (r == 5) cmdHarmony ("chordclip"); else if (r == 6) cmdHarmony ("namechord"); else if (r == 7) cmdHarmony ("snapchord"); else if (r == 8) cmdHarmony ("clearchords");
            refreshChordLane(); layoutDirty = true; repaint();
        });
    }
    void keyMenu()
    {
        const auto key = harmony::getKey (edit); juce::PopupMenu m, roots, scs;
        for (int r = 0; r < 12; ++r) roots.addItem (1000 + r, harmony::noteName (r), true, key.active() && key.root == r);
        for (int k = 1; k < (int) harmony::scales().size(); ++k) scs.addItem (2000 + k, juce::String::fromUTF8 (harmony::scales()[(size_t) k].name), true, key.active() && key.scale == k);
        m.addSubMenu ("Nota base", roots); m.addSubMenu ("Escala", scs); m.addSeparator();
        m.addItem (1, "Deducirla de los clips MIDI elegidos"); m.addItem (2, "Ajustar las notas elegidas a la escala"); m.addItem (3, "Transponer los clips elegidos para que encajen"); m.addSeparator();
        m.addItem (4, juce::String::fromUTF8 ("Mostrar el carril de acordes"), true, chordOn); m.addItem (5, "Quitar la tonalidad", key.active());
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (keyChipR())), [this, key] (int r)
        {
            if (r >= 1000 && r < 1012) harmony::setKey (edit, { r - 1000, key.active() ? key.scale : 1 });
            else if (r >= 2000 && r < 2100) harmony::setKey (edit, { key.active() ? key.root : 0, r - 2000 });
            else if (r == 1) cmdHarmony ("detectkey"); else if (r == 2) cmdHarmony ("snapscale"); else if (r == 3) cmdHarmony ("fitkey"); else if (r == 4) cmdHarmony ("chordlane"); else if (r == 5) cmdHarmony ("keynone");
            if (onClipChanged2) onClipChanged2(); refreshChordLane(); layoutDirty = true; repaint();
        });
    }
    void commitSection()
    {
        auto ss = arranger::list (edit); const int i = secIdx, mode = secMode; const bool moved = secMoved; const double cur = secCur;
        secIdx = -1; secMode = 0; secMoved = false;
        if (i >= 0 && i < (int) ss.size())
        {
            if (! moved) edit.getTransport().setPosition (te::TimePosition::fromSeconds (beatsToTime (ss[(size_t) i].beat)));
            else if (mode == 1) { arranger::move (edit, i, [&] { secIdx = i; secMode = 1; secCur = cur; const int t = secTarget(); secIdx = -1; secMode = 0; return t; }()); }
            else if (mode == 2) arranger::setLen (edit, i, barSnap (cur) - ss[(size_t) i].beat);
        }
        layoutDirty = true; if (onClipChanged2) onClipChanged2(); repaint();
    }
    void renameSectionDialog (int i)
    {
        auto ss = arranger::list (edit); if (i < 0 || i >= (int) ss.size()) return; auto v = ss[(size_t) i].v;
        markerDlg = std::make_unique<juce::AlertWindow> (juce::String::fromUTF8 ("Secci\xc3\xb3n"), "", juce::MessageBoxIconType::NoIcon); markerDlg->addTextEditor ("n", ss[(size_t) i].name, ""); markerDlg->addButton ("Aceptar", 1); markerDlg->addButton ("Cancelar", 0);
        markerDlg->enterModalState (true, juce::ModalCallbackFunction::create ([this, v] (int ok) mutable { if (ok == 1 && markerDlg) { const auto n = markerDlg->getTextEditorContents ("n").trim(); if (n.isNotEmpty()) { edit.getUndoManager().beginNewTransaction ("Renombrar secci\xc3\xb3n"); v.setProperty ("name", n, &edit.getUndoManager()); } } markerDlg.reset(); repaint(); }), false);
    }
    void sectionMenu (const juce::MouseEvent& e)
    {
        const int si = secAtX ((float) e.x); const double beat = barSnap (std::max (0.0, xToBeat ((float) e.x))); const bool on = si >= 0; auto ss = arranger::list (edit); juce::PopupMenu m;
        m.addItem (1, juce::String::fromUTF8 ("Nueva secci\xc3\xb3n aqu\xc3\xad (8 compases)"), ! on);
        m.addSeparator();
        m.addItem (2, juce::String::fromUTF8 ("Renombrar\xe2\x80\xa6"), on); m.addItem (3, "Cambiar color", on); m.addItem (4, juce::String::fromUTF8 ("Duplicar (inserta una copia detr\xc3\xa1s)"), on);
        m.addItem (5, "Mover a la izquierda", on && si > 0); m.addItem (6, "Mover a la derecha", on && si + 1 < (int) ss.size());
        m.addItem (7, juce::String::fromUTF8 ("Elegir los clips de la secci\xc3\xb3n"), on); m.addItem (8, juce::String::fromUTF8 ("Repetir la secci\xc3\xb3n en bucle"), on); m.addItem (9, juce::String::fromUTF8 ("Eliminar secci\xc3\xb3n"), on);
        m.addSeparator(); m.addItem (10, "Crear secciones desde los marcadores"); m.addItem (11, "Quitar todas las secciones", ! ss.empty());
        m.showMenuAsync (juce::PopupMenu::Options(), [this, si, beat] (int r)
        {
            if (r == 1) { arranger::add (edit, beat, 8 * timesig::bar()); }
            else if (r == 2) renameSectionDialog (si);
            else if (r == 3) cmdSections ("colour", si); else if (r == 4) cmdSections ("dup", si); else if (r == 5) cmdSections ("left", si); else if (r == 6) cmdSections ("right", si);
            else if (r == 7) cmdSections ("select", si); else if (r == 8) cmdSections ("loop", si); else if (r == 9) cmdSections ("del", si);
            else if (r == 10) cmdSections ("frommarkers"); else if (r == 11) cmdSections ("clear");
            layoutDirty = true; repaint();
        });
    }
    void rulerMenu (const juce::MouseEvent& e)
    {
        if (e.y >= kBarsH + kSecH && chordOn) { chordMenu (e); return; }
        if (e.y >= kBarsH) { sectionMenu (e); return; }
        const double beat = snapBeat (std::max (0.0, xToBeat ((float) e.x))); const int mi = markerAt ((float) e.x); auto ms = markers(); juce::PopupMenu m;
        m.addItem (1, juce::String::fromUTF8 ("Agregar marcador aquí"));
        if (mi >= 0) { m.addItem (2, juce::String::fromUTF8 ("Renombrar marcador…")); m.addItem (3, "Eliminar marcador"); }
        m.showMenuAsync (juce::PopupMenu::Options(), [this, beat, mi, ms] (int r)
        {
            if (r == 1) addMarker (beat);
            else if (r == 3 && mi >= 0) { edit.getUndoManager().beginNewTransaction ("Marcador"); edit.state.removeChild (ms[(size_t) mi].v, &edit.getUndoManager()); repaint(); }
            else if (r == 2 && mi >= 0)
            {
                auto v = ms[(size_t) mi].v; markerDlg = std::make_unique<juce::AlertWindow> ("Marcador", "", juce::MessageBoxIconType::NoIcon); markerDlg->addTextEditor ("n", ms[(size_t) mi].name, ""); markerDlg->addButton ("Aceptar", 1); markerDlg->addButton ("Cancelar", 0);
                markerDlg->enterModalState (true, juce::ModalCallbackFunction::create ([this, v] (int ok) mutable { if (ok == 1 && markerDlg) { edit.getUndoManager().beginNewTransaction ("Marcador"); v.setProperty ("name", markerDlg->getTextEditorContents ("n"), &edit.getUndoManager()); } markerDlg.reset(); repaint(); }), false);
            }
        });
    }
    void contextMenu (const juce::MouseEvent& e)
    {
        if (e.y < rulerH() && e.x >= kHeaderW) { rulerMenu (e); return; }
        if (e.x < kHeaderW + 6) { const int ti = trackAt ((float) e.y); if (ti >= 0) trackMenu (ti); return; }
        const int ti = trackAt ((float) e.y); if (ti < 0) return;
        auto tr = te::getAudioTracks (edit)[ti]; const double t = beatsToTime (xToBeat ((float) e.x)); te::Clip* hit = nullptr;
        for (auto* c : tr->getClips()) if (c->getPosition().time.contains (te::TimePosition::fromSeconds (t))) hit = c;
        if (hit != nullptr && ! e.mods.isShiftDown())
        {   // clic derecho sobre un clip = borrarlo (si es parte de una selección, borra toda la selección). Mayús + clic derecho abre el menú del clip.
            if (group.size() > 1 && group.contains (hit)) { for (auto* c : group) killClip (c); } else killClip (hit);
            group.clear(); selected = nullptr; dragClip = nullptr; layoutDirty = true; if (onTracksChanged) onTracksChanged(); repaint(); return;
        }
        juce::PopupMenu m; const double clickBeat = snapBeat (xToBeat ((float) e.x));
        if (hit != nullptr)
        {
            selected = hit; te::Clip::Ptr c (hit);
            if (isLocked (hit))
            {   // clip bloqueado: solo se puede desbloquear, duplicar, silenciar o renombrar
                juce::PopupMenu lm; if (dynamic_cast<te::MidiClip*> (hit)) lm.addItem (1, "Abrir en Piano Roll");
                lm.addItem (80, "Desbloquear clip  (Ctrl+L)"); lm.addSeparator(); lm.addItem (2, "Duplicar  (Ctrl+D)"); lm.addItem (6, hit->isMuted() ? "Quitar silencio del clip" : "Silenciar clip"); lm.addItem (5, juce::String::fromUTF8 ("Renombrar…"));
                lm.showMenuAsync (juce::PopupMenu::Options(), [this, c] (int r)
                {
                    if (r == 1) { if (onOpenMidi) onOpenMidi (dynamic_cast<te::MidiClip*> (c.get())); }
                    else if (r == 80) { edit.getUndoManager().beginNewTransaction(); c->state.setProperty ("injLocked", false, &edit.getUndoManager()); }
                    else if (r == 2) { if (auto* nc = trackutil::duplicateClip (edit, *c)) { selected = nc; nc->state.setProperty ("injLocked", false, &edit.getUndoManager()); } }
                    else if (r == 6) { edit.getUndoManager().beginNewTransaction(); c->setMuted (! c->isMuted()); }
                    else if (r == 5)
                    {
                        auto* aw = new juce::AlertWindow (juce::String::fromUTF8 ("Renombrar clip"), "", juce::MessageBoxIconType::NoIcon); aw->addTextEditor ("n", c->getName()); aw->addButton ("Aceptar", 1); aw->addButton ("Cancelar", 0);
                        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, c, aw] (int res) { if (res == 1) { edit.getUndoManager().beginNewTransaction(); c->setName (aw->getTextEditorContents ("n")); repaint(); } delete aw; }));
                    }
                    repaint();
                });
                return;
            }
            if (dynamic_cast<te::MidiClip*> (hit)) m.addItem (1, "Abrir en Piano Roll");
            { juce::PopupMenu cm; cm.addItem (99, "Color de la pista"); cm.addSeparator(); for (int ci = 0; ci < 8; ++ci) cm.addColouredItem (100 + ci, "      ", theme::trackColours[ci], false, false, nullptr); m.addSubMenu ("Color del clip", cm); }
            if (auto* wc = dynamic_cast<te::WaveAudioClip*> (hit))
            {
                static const double fd[] { 0.0, 0.01, 0.05, 0.25, 1.0 }; static const char* fl[] { "Ninguno", "10 ms", "50 ms", "250 ms", "1 s" };
                juce::PopupMenu fi, fo; for (int k = 0; k < 5; ++k) { fi.addItem (200 + k, fl[k], true, std::abs (wc->getFadeIn().inSeconds() - fd[k]) < 0.002); fo.addItem (210 + k, fl[k], true, std::abs (wc->getFadeOut().inSeconds() - fd[k]) < 0.002); }
                m.addSubMenu ("Fundido de entrada", fi); m.addSubMenu ("Fundido de salida", fo);
                juce::PopupMenu pt, sp, md; static const int st[] { -12, -7, -5, -3, -2, -1, 0, 1, 2, 3, 5, 7, 12 }; static const int sr[] { 50, 75, 90, 100, 110, 125, 150, 200 };
                for (int k = 0; k < 13; ++k) pt.addItem (400 + k, (st[k] > 0 ? "+" : "") + juce::String (st[k]) + " semitonos", true, std::abs (wc->getPitchChange() - (float) st[k]) < 0.01f);
                for (int k = 0; k < 8; ++k) sp.addItem (430 + k, juce::String (sr[k]) + " %", true, std::abs (wc->getSpeedRatio() * 100.0 - sr[k]) < 0.5);
                md.addItem (450, juce::String::fromUTF8 ("Melódico"), true, wc->getTimeStretchMode() != te::TimeStretcher::rubberbandPercussive); md.addItem (451, "Percusivo", true, wc->getTimeStretchMode() == te::TimeStretcher::rubberbandPercussive);
                m.addSubMenu ("Tono", pt); m.addSubMenu ("Velocidad", sp); m.addSubMenu ("Modo de estiramiento", md);
            }
            if (dynamic_cast<te::WaveAudioClip*> (hit))
            {
                m.addItem (72, juce::String::fromUTF8 ("Invertir audio (reversa)")); m.addItem (73, juce::String::fromUTF8 ("Normalizar volumen del clip"));
                int nw = 0; for (auto* gc : group) if (dynamic_cast<te::WaveAudioClip*> (gc)) ++nw; if (nw == 2 && group.contains (hit)) m.addItem (71, juce::String::fromUTF8 ("Comparar tono de los 2 clips (kick vs bajo)"));
            }
            if (dynamic_cast<te::WaveAudioClip*> (hit)) { m.addItem (60, juce::String::fromUTF8 ("Detectar tempo y tono")); m.addItem (61, juce::String::fromUTF8 ("Ajustar al tempo del proyecto")); }
            m.addItem (70, juce::String::fromUTF8 ("Repetir este clip en bucle")); if (dynamic_cast<te::MidiClip*> (hit)) m.addItem (8, juce::String::fromUTF8 ("Exportar como archivo MIDI…"));
            m.addItem (80, "Bloquear clip  (Ctrl+L)"); m.addItem (5, juce::String::fromUTF8 ("Renombrar…")); m.addItem (6, hit->isMuted() ? "Quitar silencio del clip" : "Silenciar clip"); m.addSeparator(); m.addItem (2, "Duplicar  (Ctrl+D)"); m.addItem (30, juce::String::fromUTF8 ("Repetir 2 veces m\xc3\xa1s")); m.addItem (31, juce::String::fromUTF8 ("Repetir 4 veces m\xc3\xa1s")); m.addItem (32, juce::String::fromUTF8 ("Repetir 8 veces m\xc3\xa1s")); m.addItem (3, juce::String::fromUTF8 ("Dividir aqu\xc3\xad")); m.addItem (33, "Dividir en 2 partes iguales"); m.addItem (34, "Dividir en 4 partes iguales"); m.addItem (35, "Dividir en 8 partes iguales"); m.addSeparator(); m.addItem (4, "Eliminar  (Supr)");
            m.showMenuAsync (juce::PopupMenu::Options(), [this, c, tr, clickBeat] (int r)
            {
                if (r == 1) { if (onOpenMidi) onOpenMidi (dynamic_cast<te::MidiClip*> (c.get())); }
                else if (r == 2) { if (auto* nc = trackutil::duplicateClip (edit, *c)) selected = nc; }
                else if (r == 3) tr->splitAt (te::TimePosition::fromSeconds (beatsToTime (clickBeat)));
                else if (r >= 30 && r <= 32) { edit.getUndoManager().beginNewTransaction(); te::Clip* last = c.get(); for (int k = 0, n = r == 30 ? 2 : (r == 31 ? 4 : 8); k < n && last != nullptr; ++k) last = trackutil::duplicateClip (edit, *last); if (last) selected = last; }
                else if (r >= 33 && r <= 35) { edit.getUndoManager().beginNewTransaction(); const int parts = r == 33 ? 2 : (r == 34 ? 4 : 8); const double t0 = c->getPosition().getStart().inSeconds(), len = c->getPosition().getLength().inSeconds(); for (int k = 1; k < parts; ++k) tr->splitAt (te::TimePosition::fromSeconds (t0 + len * k / parts)); }
                else if (r == 4) { if (selected == c.get()) selected = nullptr; dragClip = nullptr; killClip (c.get()); }
                else if (r >= 200 && r < 205) { static const double fd[] { 0.0, 0.01, 0.05, 0.25, 1.0 }; if (auto* wc = dynamic_cast<te::WaveAudioClip*> (c.get())) { edit.getUndoManager().beginNewTransaction(); wc->setFadeIn (te::TimeDuration::fromSeconds (fd[r - 200])); } }
                else if (r >= 210 && r < 215) { static const double fd[] { 0.0, 0.01, 0.05, 0.25, 1.0 }; if (auto* wc = dynamic_cast<te::WaveAudioClip*> (c.get())) { edit.getUndoManager().beginNewTransaction(); wc->setFadeOut (te::TimeDuration::fromSeconds (fd[r - 210])); } }
                else if (r >= 400 && r < 413) { static const int st[] { -12, -7, -5, -3, -2, -1, 0, 1, 2, 3, 5, 7, 12 }; if (auto* wc = dynamic_cast<te::WaveAudioClip*> (c.get())) { edit.getUndoManager().beginNewTransaction(); setClipPitch (*wc, (float) st[r - 400]); } }
                else if (r >= 430 && r < 438) { static const int sr[] { 50, 75, 90, 100, 110, 125, 150, 200 }; if (auto* wc = dynamic_cast<te::WaveAudioClip*> (c.get())) { edit.getUndoManager().beginNewTransaction(); setClipSpeed (*wc, sr[r - 430] / 100.0); } }
                else if (r == 450 || r == 451) { if (auto* wc = dynamic_cast<te::WaveAudioClip*> (c.get())) { edit.getUndoManager().beginNewTransaction(); wc->setTimeStretchMode (r == 451 ? te::TimeStretcher::rubberbandPercussive : te::TimeStretcher::rubberbandMelodic); } }
                else if (r == 80) { edit.getUndoManager().beginNewTransaction(); c->state.setProperty ("injLocked", true, &edit.getUndoManager()); }
                else if (r == 70) loopClip (*c);
                else if (r == 71) compareTones();
                else if (r == 72) { if (auto* wc = dynamic_cast<te::WaveAudioClip*> (c.get())) reverseClip (*wc, *tr); }
                else if (r == 73) { if (auto* wc = dynamic_cast<te::WaveAudioClip*> (c.get())) normaliseClip (*wc); }
                else if (r == 8) { if (auto* mc = dynamic_cast<te::MidiClip*> (c.get())) exportMidiDialog (*mc); }
                else if (r == 60 || r == 61)
                {
                    if (auto* wc = dynamic_cast<te::WaveAudioClip*> (c.get()))
                    {
                        const auto res = sampleanalysis::analyse (wc->getOriginalFile());
                        if (r == 60) { if (onInstrument) onInstrument (-1, sampleanalysis::describe (res)); }
                        else if (res.bpm > 0 && ! res.oneShot) { const double proj = edit.tempoSequence.getTempo (0)->getBpm(); edit.getUndoManager().beginNewTransaction(); setClipSpeed (*wc, proj / res.bpm); if (onInstrument) onInstrument (-1, juce::String::fromUTF8 ("Ajustado: ") + juce::String (res.bpm, 1) + " -> " + juce::String (proj, 1) + " BPM"); }
                        else if (onInstrument) onInstrument (-1, juce::String::fromUTF8 ("No se pudo detectar el tempo de este clip"));
                    }
                }
                else if (r == 99) { edit.getUndoManager().beginNewTransaction(); c->state.removeProperty ("injColour", &edit.getUndoManager()); }
                else if (r >= 100 && r < 108) { edit.getUndoManager().beginNewTransaction(); c->state.setProperty ("injColour", (int) theme::trackColours[r - 100].getARGB(), &edit.getUndoManager()); }
                else if (r == 6) { edit.getUndoManager().beginNewTransaction(); c->setMuted (! c->isMuted()); }
                else if (r == 5)
                {
                    auto* aw = new juce::AlertWindow (juce::String::fromUTF8 ("Renombrar clip"), "", juce::MessageBoxIconType::NoIcon); aw->addTextEditor ("n", c->getName()); aw->addButton ("Aceptar", 1); aw->addButton ("Cancelar", 0);
                    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, c, aw] (int res) { if (res == 1) { edit.getUndoManager().beginNewTransaction(); c->setName (aw->getTextEditorContents ("n")); repaint(); } delete aw; }));
                }
                repaint();
            });
        }
        else
        {
            m.addItem (1, juce::String::fromUTF8 ("Crear clip MIDI aqu\xc3\xad"));
            m.showMenuAsync (juce::PopupMenu::Options(), [this, tr, clickBeat] (int r)
            {
                if (r != 1) return; const double sb = std::max (0.0, std::floor (clickBeat / timesig::bar()) * timesig::bar());
                selected = tr->insertMIDIClip ("MIDI", te::TimeRange (te::TimePosition::fromSeconds (beatsToTime (sb)), te::TimePosition::fromSeconds (beatsToTime (sb + 16.0))), nullptr).get(); repaint();
            });
        }
    }

    // Redibuja solo lo necesario: todo si algo cambió; si no, solo la franja del cabezal mientras se mueve.
    uint64_t signature() const
    {
        uint64_t h = 1469598103934665603ull; auto mix = [&h] (uint64_t v) { h ^= v; h *= 1099511628211ull; };
        for (auto* t : te::getAudioTracks (edit))
        {
            mix (t->isMuted (false) ? 1 : 2); mix (t->isSolo (false) ? 3 : 4); mix ((uint64_t) t->getName().hashCode64()); mix ((uint64_t) (int) t->state.getProperty ("injColour", -1));
            for (auto* c : t->getClips()) { mix (std::hash<double>{} (c->getPosition().getStart().inSeconds())); mix (std::hash<double>{} (c->getPosition().getLength().inSeconds())); }
        }
        for (auto& mk : markers()) { mix (std::hash<double>{} (mk.beat)); mix ((uint64_t) mk.name.hashCode64()); }
        return h;
    }
    bool thumbsPending() const { for (auto& kv : thumbs) if (kv.second && ! kv.second->isFullyLoaded()) return true; return false; }
    void timerCallback() override
    {
        if (! ghosts.empty()) { for (auto& gh : ghosts) gh.a -= .075f; ghosts.erase (std::remove_if (ghosts.begin(), ghosts.end(), [] (const Ghost& gh) { return gh.a <= 0.f; }), ghosts.end()); repaint(); }
        if (tgtX >= 0) { scrollX += (tgtX - scrollX) * .26; if (std::abs (tgtX - scrollX) < .5) { scrollX = tgtX; tgtX = -1; } repaint(); }
        if (follow && edit.getTransport().isPlaying() && tgtX < 0 && zoomTarget < 0.f)
        { const double pb = timeToBeats (smoothPos()); const float pxp = beatToX (pb); if (pxp > getWidth() - 70.f || pxp < kHeaderW + 6) tgtX = std::max (0.0, pb * pxPerBeat - 90.0); }
        if (tgtY >= 0) { scrollY += (tgtY - scrollY) * .26; if (std::abs (tgtY - scrollY) < .5) { scrollY = tgtY; tgtY = -1; } repaint(); }
        if (flashA > 0.f) { flashA = std::max (0.f, flashA - .06f); repaint(); }
        { const float t = marquee && ! mqRect.isEmpty() ? 1.f : 0.f; if (std::abs (mqFade - t) > .01f) { mqFade += (t - mqFade) * .35f; repaint(); } else mqFade = t; }
        if (zoomTarget > 0.f)   // zoom suave: el acercamiento se desliza hacia el valor pedido manteniendo fijo el punto bajo el cursor
        {
            const double anchor = xToBeat (zoomAnchorX); pxPerBeat += (zoomTarget - pxPerBeat) * .32; if (std::abs (pxPerBeat - zoomTarget) < .02) { pxPerBeat = zoomTarget; zoomTarget = -1.f; }
            scrollX = juce::jmax (0.0, anchor * pxPerBeat - (zoomAnchorX - kHeaderW - 6)); repaint();
        }
        if (cutTrack >= 0 && tool == "cut")   // la línea azul de las tijeras se desliza hasta el punto de corte
        {
            const float tx = beatToX (cutBeat);   // resorte: la línea acelera hacia el punto de corte, se pasa un poco y se asienta (como si tuviera masa)
            if (cutX < 0.f) { cutX = tx; cutV = 0.f; }
            else { const float dt = 1.f / 60.f, k = 520.f, c = 2.f * std::sqrt (k) * 0.55f; cutV += (k * (tx - cutX) - c * cutV) * dt; cutX += cutV * dt; if (std::abs (tx - cutX) < .25f && std::abs (cutV) < 4.f) { cutX = tx; cutV = 0.f; } else repaint(); }
        }
        if (selected != nullptr && selected->getTrack() != nullptr && lastTouched.get() != selected) lastTouched = te::Clip::Ptr (selected);
        const double pos = edit.getTransport().getPosition().inSeconds(); const auto sig = signature();
        if (sig != lastSig || thumbsPending() || ! isVisible()) { lastSig = sig; lastPos = pos; if (isVisible()) repaint(); return; }
        if (pos != lastPos)
        {
            const float a = beatToX (timeToBeats (lastPos)), b = beatToX (timeToBeats (pos)); lastPos = pos;
            repaint (juce::Rectangle<int> ((int) std::min (a, b) - 16, 0, (int) std::abs (b - a) + 34, getHeight()));
        }
    }
    uint64_t lastSig = 0; double lastPos = -1.0;
    void seekTo (int x) { auto b = std::max (0.0, xToBeat ((float) x)); edit.getTransport().setPosition (te::TimePosition::fromSeconds (beatsToTime (snapBeat (b)))); repaint(); }

    void drawRuler (juce::Graphics& g, int W)
    {
        using namespace theme;
        g.setColour (bgBar2); g.fillRect (kHeaderW + 6, 0, W, rulerH());
        g.setColour (seam); g.fillRect (kHeaderW + 6, rulerH() - 1, W, 1);
        g.setFont (fonts::digits (11.5f)); g.setColour (textDim);
        { auto& tcl = edit.getTransport(); if (tcl.looping) { const float lx0 = beatToX (timeToBeats (tcl.getLoopRange().getStart().inSeconds())), lx1 = beatToX (timeToBeats (tcl.getLoopRange().getEnd().inSeconds())); g.setColour (accent); g.fillRect (std::max (lx0, (float) kHeaderW + 6.f), 6.f, lx1 - std::max (lx0, (float) kHeaderW + 6.f), 3.f); g.fillRect (lx0, 6.f, 2.f, 10.f); g.fillRect (lx1 - 2.f, 6.f, 2.f, 10.f); g.setColour (text); } }
        for (int bt = (int) std::floor (xToBeat ((float) kHeaderW + 6.f)); beatToX ((double) bt) < (float) W; ++bt) { const float tx = beatToX ((double) bt); if (tx < kHeaderW + 6) continue; const bool isBar = (bt % (int) std::max (1.0, timesig::bar())) == 0; if (pxPerBeat < 6 && ! isBar) continue; g.setColour (isBar ? barLine.brighter (.35f) : gridLine.brighter (.25f)); g.fillRect (tx, isBar ? 18.f : 30.f, 1.f, isBar ? 21.f : 9.f); }
        if (pxPerBeat >= 48)
        {   const int div = pxPerBeat >= 110 ? 4 : 2; const double bb0 = std::floor (xToBeat ((float) kHeaderW + 6.f)), bb1 = xToBeat ((float) W) + 1;
            for (double b = std::max (0.0, bb0); b <= bb1; b += 1.0 / div) { if (std::fmod (b, 1.0) < 1e-6) continue; const float tx = beatToX (b); if (tx < kHeaderW + 6) continue; g.setColour (gridLine.brighter (.1f)); g.fillRect (tx, 34.f, 1.f, 6.f); } }
        g.setColour (textDim.brighter (.15f));
        const int stride = pxPerBeat * 4 >= 64 ? 1 : (pxPerBeat * 16 >= 64 ? 4 : 8);
        for (int bar = 0; ; bar += 1)
        {
            const float x = beatToX (bar * timesig::bar()); if (x > W) break;
            const float nx = beatToX ((bar + 1) * timesig::bar());
            if (x < kHeaderW + 10)
            {   // el compás que ocupa el borde izquierdo conserva su número pegado al borde mientras dure
                if (nx > kHeaderW + 48 && (stride == 1 || bar % stride == 0)) g.drawText (juce::String (bar + 1), juce::Rectangle<float> ((float) kHeaderW + 12, 20.f, 40.f, 18.f).toNearestInt(), juce::Justification::centredLeft);
                continue;
            }
            if (bar % stride != 0) continue;
            g.drawText (juce::String (bar + 1), juce::Rectangle<float> (x + 4, 20, 40, 18).toNearestInt(), juce::Justification::centredLeft);
        }
        g.saveState(); g.reduceClipRegion (kHeaderW + 6, 0, W, rulerH());
        {   // carril de secciones (arreglo)
            const float ly = (float) kBarsH, lh = (float) kSecH;
            g.setColour (boxFill.withAlpha (.55f)); g.fillRect ((float) kHeaderW + 6.f, ly, (float) W, lh); g.setColour (seam); g.fillRect ((float) kHeaderW + 6.f, ly, (float) W, 1.f);
            auto ss = arranger::list (edit);
            for (int i = 0; i < (int) ss.size(); ++i)
            {
                auto sc = ss[(size_t) i]; double b0 = sc.beat, l0 = sc.len; const bool drag = i == secIdx && secMoved;
                if (drag && secMode == 2) l0 = std::max (1.0, barSnap (secCur) - b0);
                if (drag && secMode == 1) b0 = std::max (0.0, barSnap (secCur - secGrab));
                const float xa = beatToX (b0), xb = beatToX (b0 + l0); if (xb < kHeaderW || xa > W) continue;
                auto col = arranger::colourOf (sc.col); auto r = juce::Rectangle<float> (xa + .5f, ly + 3.f, std::max (3.f, xb - xa - 1.f), lh - 6.f);
                const bool hot = i == secHot || drag;
                g.setGradientFill (juce::ColourGradient (col.withAlpha (hot ? .5f : .38f), 0.f, r.getY(), col.withAlpha (hot ? .36f : .24f), 0.f, r.getBottom(), false)); g.fillRoundedRectangle (r, 2.f);
                g.setColour (col.withAlpha (.9f)); g.fillRect (r.getX(), r.getY(), 2.f, r.getHeight());
                g.setColour (col.withAlpha (drag ? .9f : .45f)); g.drawRoundedRectangle (r.reduced (.5f), 2.f, 1.f);
                g.saveState(); g.reduceClipRegion (r.toNearestInt().withTrimmedLeft (5).withTrimmedRight (2)); g.setColour (text.withAlpha (.95f)); g.setFont (fonts::ui (11.5f, true));
                g.drawText (sc.name, r.toNearestInt().withTrimmedLeft (7), juce::Justification::centredLeft, false); g.restoreState();
                if (! drag) { g.setColour (col.withAlpha (.2f)); g.fillRect (xa, (float) rulerH(), 1.f, (float) (getHeight() - rulerH())); }
            }
            if (secMoved && secMode == 1 && secIdx >= 0 && secIdx < (int) ss.size())
            {   // donde caera la seccion al soltar: raya naranja en el borde de destino
                const int to = secTarget(); if (to != secIdx) { const auto& o = ss[(size_t) std::min ((int) ss.size() - 1, to)]; const float x = beatToX (to > secIdx ? o.end() : o.beat); g.setColour (accentHi); g.fillRect (x - 1.f, ly, 2.f, lh); }
            }
        }

        if (chordOn)
        {   // carril de acordes
            const float cy = (float) (kBarsH + kSecH), ch = (float) kChordH;
            g.setColour (boxFill.withAlpha (.4f)); g.fillRect ((float) kHeaderW + 6.f, cy, (float) W, ch); g.setColour (seam); g.fillRect ((float) kHeaderW + 6.f, cy, (float) W, 1.f);
            auto cs = harmony::list (edit); const auto key = harmony::getKey (edit);
            for (int i = 0; i < (int) cs.size(); ++i)
            {
                auto c = cs[(size_t) i]; double b0 = c.beat, l0 = c.len; const bool drag = i == chIdx && chMoved;
                if (drag && chMode == 2) l0 = std::max (0.25, snapBeat (chCur) - b0);
                if (drag && chMode == 1) b0 = std::max (0.0, snapBeat (chCur - chGrab));
                const float xa = beatToX (b0), xb = beatToX (b0 + l0); if (xb < kHeaderW || xa > W) continue;
                const bool out = key.active() && ! harmony::inScale (key, c.root);
                const juce::Colour col = out ? juce::Colour (0xffd98a5f) : juce::Colour (0xff7f93b8); auto r = juce::Rectangle<float> (xa + .5f, cy + 3.f, std::max (3.f, xb - xa - 1.f), ch - 6.f);
                const bool hot = i == chHot || drag;
                g.setGradientFill (juce::ColourGradient (col.withAlpha (hot ? .5f : .36f), 0.f, r.getY(), col.withAlpha (hot ? .36f : .22f), 0.f, r.getBottom(), false)); g.fillRoundedRectangle (r, 2.f);
                g.setColour (col.withAlpha (drag ? .9f : .45f)); g.drawRoundedRectangle (r.reduced (.5f), 2.f, 1.f);
                g.saveState(); g.reduceClipRegion (r.toNearestInt().reduced (3, 0)); g.setColour (text.withAlpha (.95f)); g.setFont (fonts::ui (11.5f, true)); g.drawText (c.name(), r.toNearestInt().withTrimmedLeft (3), juce::Justification::centredLeft, false); g.restoreState();
            }
        }
        for (auto& mk : markers())
        {
            const float x = beatToX (mk.beat); if (x < kHeaderW - 20 || x > W) continue;
            g.setColour (pink.withAlpha (.5f)); g.fillRect (x, (float) kBarsH - 14.f, 1.f, (float) getHeight());
            const float w = 10.f + 7.f * (float) std::min (14, mk.name.length()); g.setColour (pink); g.fillRoundedRectangle (x, 5.f, w, 14.f, 2.f);
            g.setColour (juce::Colour (0xff17181b)); g.setFont (fonts::label (11.f)); g.drawText (mk.name, juce::Rectangle<float> (x + 4.f, 5.f, w - 6.f, 14.f), juce::Justification::centredLeft, true);
        }
        g.restoreState();
    }

    void drawHeader (juce::Graphics& g, te::AudioTrack& t, int i)
    {
        using namespace theme;
        auto col = trackutil::colourOf (&t, i); auto r = headerR (i);
        if (r.getBottom() < rulerH() || r.getY() > getHeight()) return;
        g.saveState(); g.reduceClipRegion (0, rulerH(), kHeaderW + 6, getHeight() - rulerH());
        const bool sel = i == selTrack, muted = t.isMuted (false);
        // cabecera oscura con tira de color a la izquierda (el color identifica la pista sin gritar)
        g.setGradientFill (juce::ColourGradient ((sel ? panelHi : panel).brighter (.025f), 0.f, (float) r.getY(), (sel ? panelHi : panel).darker (.03f), 0.f, (float) r.getBottom(), false)); g.fillRect (r);
        g.setColour (edgeLight); g.fillRect (r.getX(), r.getY(), r.getWidth(), 1);
        g.setColour (seam); g.fillRect (r.getX(), r.getBottom() - 1, r.getWidth(), 1); g.fillRect (r.getRight() - 1, r.getY(), 1, r.getHeight());
        g.setColour (muted ? col.withMultipliedSaturation (.3f).darker (.5f) : col); g.fillRect (r.getX(), r.getY(), sel ? 6 : 4, r.getHeight() - 1);
        if (sel) { g.setColour (juce::Colours::white.withAlpha (.05f)); g.fillRect (r.reduced (0, 1)); }
        const auto fg = muted ? textFaint : text; const int tx = r.getX() + 16;
        {   // flechita de plegar
            const float ry = r.getHeight() >= 40 ? 9.f : (r.getHeight() - 11.f) * .5f; auto c = juce::Rectangle<float> ((float) r.getX() + 12.f, (float) r.getY() + ry, 11.f, 11.f);
            juce::Path tri; tri.addTriangle (c.getX() + 1.5f, c.getY() + 3.f, c.getRight() - 1.5f, c.getY() + 3.f, c.getCentreX(), c.getY() + 8.f); if (isFolded (i)) tri.applyTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, c.getCentreX(), c.getCentreY())); g.setColour (textDim); g.fillPath (tri);
        }
        g.setColour (fg); g.setFont (fonts::ui (12.5f, true));
        g.drawText (t.getName() == "Track" ? juce::String::fromUTF8 ("Pista ") + juce::String (i + 1) : t.getName(), r.getX() + 30, r.getHeight() >= 40 ? r.getY() + 5 : r.getY() + (r.getHeight() - 20) / 2, r.getWidth() - 52, 20, juce::Justification::centredLeft, true);
        if (r.getHeight() >= 26) icons::draw (g, icons::Dots, dotsR (i), textDim);
        juce::String inst; for (auto* p : t.pluginList.getPlugins()) if (dynamic_cast<te::ExternalPlugin*> (p) || dynamic_cast<te::SamplerPlugin*> (p) || dynamic_cast<te::FourOscPlugin*> (p)) { inst = p->state.getProperty ("injPreset", p->getName()).toString(); break; }
        g.setColour (textDim); g.setFont (fonts::ui (11.5f));
        if (r.getHeight() >= 60 && inst.isNotEmpty()) g.drawText (inst, r.getX() + 30, r.getY() + 25, r.getWidth() - 40, 16, juce::Justification::centredLeft, true);
        if (r.getHeight() >= 60 && (t.isMuted (false) || t.isSolo (false)))
        {   // etiqueta pequeña: solo (amarillo) o silenciada (naranja)
            const bool solo = t.isSolo (false); const auto bc = solo ? juce::Colour (0xffe9c545) : juce::Colour (0xffe8873a); const juce::String lbl = solo ? "SOLO" : juce::String::fromUTF8 ("SILENCIADA");
            auto pr = juce::Rectangle<float> ((float) r.getX() + 30.f, (float) r.getBottom() - 24.f, solo ? 38.f : 76.f, 16.f);
            g.setColour (bc.withAlpha (.18f)); g.fillRoundedRectangle (pr, rad); g.setColour (bc.withAlpha (.8f)); g.drawRoundedRectangle (pr.reduced (.5f), rad, 1.f);
            g.setColour (bc); g.setFont (fonts::label (10.f)); g.drawText (lbl, pr.toNearestInt(), juce::Justification::centred);
        }
        { const auto gid = trackutil::groupOf (t);
          if (gid.isNotEmpty())
          {
              const auto gc = theme::trackColours[trackutil::groupColour (gid)]; auto all = te::getAudioTracks (edit);
              const bool first = i == 0 || trackutil::groupOf (*all[i - 1]) != gid, last = i + 1 >= all.size() || trackutil::groupOf (*all[i + 1]) != gid;
              g.setColour (seam); g.fillRect (r.getX() + 6, r.getY(), 6, r.getHeight()); g.setColour (gc); g.fillRect (r.getX() + 7, r.getY() + (first ? 2 : 0), 4, r.getHeight() - (first ? 2 : 0) - (last ? 2 : 0));
              if (first && r.getHeight() >= 40) { g.setColour (gc.brighter (.2f)); g.setFont (fonts::label (10.5f)); g.drawText (trackutil::groupName (t), r.getX() + 30, r.getBottom() - 19, r.getWidth() - 40, 14, juce::Justification::centredLeft, true); }
          } }
        g.restoreState();
    }

    // ---- Automatización de volumen ----
    int autoIdx = -1, autoTrack = -1;
public:
    static te::AutomatableParameter* volParamOf (te::AudioTrack& t) { auto* v = t.getVolumePlugin(); return v ? v->volParam.get() : nullptr; }
    // Parametro que se automatiza en la pista: "vol" (por defecto), "pan" o "P<plugin>:<parametro>"
    static te::AutomatableParameter* autoParamOf (te::AudioTrack& t)
    {
        const juce::String id = t.state.getProperty ("injAutoParam", "vol").toString();
        if (id == "pan") { auto* v = t.getVolumePlugin(); return v ? v->panParam.get() : nullptr; }
        if (id.startsWithChar ('P'))   // "P<id del plugin>|<id del parametro>": no depende del orden de los plugins
        {
            const juce::String pid = id.substring (1).upToFirstOccurrenceOf ("|", false, false), prm = id.fromFirstOccurrenceOf ("|", false, false);
            for (auto* pl : t.pluginList.getPlugins()) if (pl->itemID.toString() == pid) return pl->getAutomatableParameterByID (prm).get();
        }
        return volParamOf (t);
    }
    static bool autoIsVolume (te::AudioTrack& t) { return autoParamOf (t) == volParamOf (t); }
    static juce::String autoParamName (te::AudioTrack& t) { auto* p = autoParamOf (t); if (! p) return {}; return autoIsVolume (t) ? juce::String ("Volumen") : p->getPluginAndParamName(); }
    bool autoShown (te::AudioTrack& t) const { return (bool) t.state.getProperty ("injAuto", false); }
    float autoY (int ti, float norm) const { const float h = (float) hOf (ti); return (float) trackTop (ti) + h - 6.f - norm * (h - 12.f); }
    float autoNorm (int ti, float y) const { const float h = (float) hOf (ti); return juce::jlimit (0.f, 1.f, ((float) trackTop (ti) + h - 6.f - y) / (h - 12.f)); }
    double timeAtX (float x) const { return beatsToTime (xToBeat (x)); }
    // Cambia el tono (semitonos) sin cambiar la duracion. Activa el motor de estiramiento si hace falta.
    static void setClipPitch (te::WaveAudioClip& c, float semitones)
    {
        if (c.getTimeStretchMode() == te::TimeStretcher::disabled) c.setTimeStretchMode (te::TimeStretcher::rubberbandMelodic);
        c.setPitchChange (semitones);
    }
    // Cambia la velocidad (1.0 = original) y ajusta la longitud del clip para que se oiga todo el audio.
    static void setClipSpeed (te::WaveAudioClip& c, double ratio)
    {
        ratio = juce::jlimit (0.25, 4.0, ratio); const double old = c.getSpeedRatio(); if (std::abs (old - ratio) < 1e-6) return;
        if (c.getTimeStretchMode() == te::TimeStretcher::disabled) c.setTimeStretchMode (te::TimeStretcher::rubberbandMelodic);
        const auto pos = c.getPosition(); const double newLen = pos.getLength().inSeconds() * old / ratio;
        c.setSpeedRatio (ratio); c.setLength (te::TimeDuration::fromSeconds (newLen), true);
    }
    static void setAutoParam (te::AudioTrack& t, const juce::String& id) { t.state.setProperty ("injAutoParam", id, nullptr); }
    void addAutoPoint (int ti, double timeSec, float norm)   // para el editor y las pruebas
    {
        auto tracks = te::getAudioTracks (edit); if (ti < 0 || ti >= tracks.size()) return; auto* p = autoParamOf (*tracks[ti]); if (! p) return;
        tracks[ti]->state.setProperty ("injAuto", true, nullptr);
        p->getCurve().addPoint (te::TimePosition::fromSeconds (std::max (0.0, timeSec)), p->valueRange.convertFrom0to1 (norm), 0.f); repaint();
    }
private:
    void drawAuto (juce::Graphics& g, int ti)
    {
        auto* tk = te::getAudioTracks (edit)[ti]; auto* p = autoParamOf (*tk); if (! p) return; auto& cv = p->getCurve();
        const float x0 = (float) (kHeaderW + 6), x1 = (float) getWidth(); const float top = (float) trackTop (ti), bot = top + (float) hOf (ti);
        g.setColour (juce::Colours::black.withAlpha (.28f)); g.fillRect (x0, top, x1 - x0, bot - top);
        juce::Path line; const int n = cv.getNumPoints();
        auto yOfValue = [&] (float v) { return autoY (ti, p->valueRange.convertTo0to1 (v)); };
        if (n == 0) { const float y = yOfValue (p->getCurrentValue()); line.startNewSubPath (x0, y); line.lineTo (x1, y); }
        else
        {
            line.startNewSubPath (x0, yOfValue (cv.getPointValue (0)));
            for (int i = 0; i < n; ++i) { const float x = beatToX (timeToBeats (cv.getPointTime (i).inSeconds())); line.lineTo (x, yOfValue (cv.getPointValue (i))); }
            line.lineTo (x1, yOfValue (cv.getPointValue (n - 1)));
        }
        g.setColour (theme::pink); g.strokePath (line, juce::PathStrokeType (2.f));
        for (int i = 0; i < n; ++i) { const float x = beatToX (timeToBeats (cv.getPointTime (i).inSeconds())), y = yOfValue (cv.getPointValue (i)); g.setColour (juce::Colour (0xff191919)); g.fillEllipse (x - 6, y - 6, 12, 12); g.setColour (i == autoIdx && ti == autoTrack ? juce::Colours::white : theme::pink); g.fillEllipse (x - 4, y - 4, 8, 8); }
        if (autoIdx >= 0 && ti == autoTrack && autoIdx < n)
        {
            const float x = beatToX (timeToBeats (cv.getPointTime (autoIdx).inSeconds())), y = yOfValue (cv.getPointValue (autoIdx)); const float db = te::volumeFaderPositionToDB (cv.getPointValue (autoIdx)); const bool isVol = autoIsVolume (*tk); const juce::String lab = isVol ? (db <= -99.f ? juce::String ("-inf dB") : juce::String (db, 1) + " dB") : p->valueToString (cv.getPointValue (autoIdx));
            g.setColour (juce::Colour (0xe01b1b25)); g.fillRoundedRectangle (x + 10, y - 24, 62, 20, 3.f); g.setColour (theme::text); g.setFont (juce::FontOptions (12.f)); g.drawText (lab, juce::Rectangle<float> (x + 10, y - 24, 62, 20).toNearestInt(), juce::Justification::centred);
        }
    }
    void autoDown (const juce::MouseEvent& e)
    {
        const int ti = trackAt ((float) e.y); if (ti < 0) return; auto* tk = te::getAudioTracks (edit)[ti]; auto* p = autoParamOf (*tk); if (! p) return;
        tk->state.setProperty ("injAuto", true, nullptr); auto& cv = p->getCurve(); autoTrack = ti; autoIdx = -1;
        int hit = -1; float best = 11.f;
        for (int i = 0; i < cv.getNumPoints(); ++i) { const float x = beatToX (timeToBeats (cv.getPointTime (i).inSeconds())), y = autoY (ti, p->valueRange.convertTo0to1 (cv.getPointValue (i))); const float d = std::hypot (x - (float) e.x, y - (float) e.y); if (d < best) { best = d; hit = i; } }
        if (e.mods.isPopupMenu()) { if (hit >= 0) cv.removePoint (hit); repaint(); return; }
        if (hit < 0) { const double tb = snapBeat (xToBeat ((float) e.x)); hit = cv.addPoint (te::TimePosition::fromSeconds (std::max (0.0, beatsToTime (tb))), p->valueRange.convertFrom0to1 (autoNorm (ti, (float) e.y)), 0.f); }
        autoIdx = hit; repaint();
    }
    void autoDrag (const juce::MouseEvent& e)
    {
        auto* tk = te::getAudioTracks (edit)[autoTrack]; auto* p = autoParamOf (*tk); if (! p || autoIdx < 0) return; auto& cv = p->getCurve();
        const double tb = snapBeat (xToBeat ((float) e.x));
        autoIdx = cv.movePoint (autoIdx, te::TimePosition::fromSeconds (std::max (0.0, beatsToTime (tb))), p->valueRange.convertFrom0to1 (autoNorm (autoTrack, (float) e.y)), false); repaint();
    }
    std::unique_ptr<juce::FileChooser> midiChooser;
    void exportMidiDialog (te::MidiClip& mc)
    {
        te::MidiClip::Ptr keep (&mc); midiChooser = std::make_unique<juce::FileChooser> ("Exportar MIDI", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (mc.getName() + ".mid"), "*.mid");
        midiChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [this, keep] (const juce::FileChooser& fc)
        { auto f = fc.getResult(); if (f != juce::File()) trackutil::exportMidiClip (edit, *keep, f.withFileExtension ("mid")); });
    }
    struct Copied { te::Clip::Ptr clip; double beat; int track; };
    std::vector<Copied> clipboard;
    void copySelection()
    {
        clipboard.clear(); auto tracks = te::getAudioTracks (edit);
        auto add = [&] (te::Clip* c) { clipboard.push_back ({ c, timeToBeats (c->getPosition().getStart().inSeconds()), tracks.indexOf (dynamic_cast<te::AudioTrack*> (c->getTrack())) }); };
        if (group.size() > 1) { for (auto* c : group) add (c); } else if (selected) add (selected);
    }
public:
    int clipboardSize() const { return (int) clipboard.size(); }
    double beatsToTimePublic (double b) const { return beatsToTime (b); }
    double timeToBeatsPublic (double t) const { return timeToBeats (t); }
    // Pega lo copiado empezando en el cursor de reproducción (o donde se indique), en las mismas pistas.
    void pasteAtPlayhead (double atBeat = -1.0)
    {
        if (clipboard.empty()) return; auto tracks = te::getAudioTracks (edit);
        double first = 1e12; for (auto& c : clipboard) first = std::min (first, c.beat);
        const double target = atBeat >= 0 ? atBeat : timeToBeats (edit.getTransport().getPosition().inSeconds());
        edit.getUndoManager().beginNewTransaction(); group.clear(); selected = nullptr;
        for (auto& c : clipboard)
        {
            if (c.track < 0 || c.track >= tracks.size()) continue;
            if (auto* nc = trackutil::copyClipTo (edit, *c.clip, *tracks[c.track], te::TimePosition::fromSeconds (beatsToTime (target + (c.beat - first))))) group.add (nc);
        }
        if (group.size() == 1) { selected = group.getUnchecked (0).get(); group.clear(); }
        repaint();
    }
private:
public:
    bool demoHandles = false;
    void selectForDemo (te::Clip* c) { demoHandles = true; selected = c; group.clear(); if (c) group.add (c); repaint(); }
    // Prueba interna: arrastra las manijas con eventos de raton sinteticos; devuelve "" si todo va bien
    juce::String selfTestPencil (te::WaveAudioClip& w, int ti)
    {   // lápiz: recuerda el último clip tocado, lo pega en un hueco, se arrastra sin soltar y recorta por el borde derecho
        selectForDemo (&w); auto src = juce::Desktop::getInstance().getMainMouseSource();
        auto ev = [&] (juce::Point<float> p) { return juce::MouseEvent (src, p, juce::ModifierKeys(), 1.f, 0.f, 0.f, 0.f, 0.f, this, this, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false); };
        auto* tr = dynamic_cast<te::AudioTrack*> (w.getTrack()); if (tr == nullptr) return "sin pista";
        const auto oldTool = tool; tool = "select"; const auto r = clipRect (w, ti); mouseDown (ev (r.getCentre())); mouseUp (ev (r.getCentre()));
        tool = "pencil"; const int n0 = tr->getClips().size();
        const float y = r.getCentreY(), x = beatToX (timeToBeats (w.getPosition().getEnd().inSeconds()) + 3.0);
        mouseDown (ev ({ x, y })); const int n1 = tr->getClips().size(); te::Clip* nc = selected;
        if (n1 != n0 + 1 || nc == nullptr || nc == &w) { tool = oldTool; return "el lápiz no pegó el último clip (" + juce::String (n0) + " -> " + juce::String (n1) + ")"; }
        const double s1 = nc->getPosition().getStart().inSeconds(), len1 = nc->getPosition().getLength().inSeconds();
        mouseDrag (ev ({ x + 60.f, y })); mouseUp (ev ({ x + 60.f, y }));
        const double s2 = nc->getPosition().getStart().inSeconds(); juce::String err;
        if (std::abs (nc->getPosition().getLength().inSeconds() - len1) > 0.01) err = "al arrastrar recién pegado cambió de largo";
        else if (s2 <= s1 + 0.01) err = "no se pudo arrastrar sin soltar";
        if (err.isEmpty())
        {   // recortar: arrastra el borde derecho hacia la izquierda
            const auto rr = clipRect (*nc, ti); const juce::Point<float> e0 (rr.getRight() - 3.f, rr.getCentreY());
            if (rr.getWidth() < 24.f) err = "clip demasiado angosto para probar el recorte";
            else { mouseDown (ev (e0)); mouseDrag (ev ({ e0.x - rr.getWidth() * .5f, e0.y })); mouseUp (ev ({ e0.x - rr.getWidth() * .5f, e0.y }));
                   if (nc->getPosition().getLength().inSeconds() >= len1 - 0.01) err = "el lápiz no recortó el clip"; }
        }
        nc->removeFromParent(); tool = oldTool; selected = nullptr; return err;
    }
    juce::String selfTestBrush (te::WaveAudioClip& w, int ti)
    {   // pincel: un salto rapido del raton no deja huecos y lo pintado es rojo solo mientras arrastras
        selectForDemo (&w); tool = "brush"; auto src = juce::Desktop::getInstance().getMainMouseSource();
        auto ev = [&] (juce::Point<float> p) { return juce::MouseEvent (src, p, juce::ModifierKeys(), 1.f, 0.f, 0.f, 0.f, 0.f, this, this, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false); };
        auto* tr = dynamic_cast<te::AudioTrack*> (w.getTrack()); if (tr == nullptr) return "sin pista";
        const double s0 = w.getPosition().getStart().inSeconds(), len = w.getPosition().getLength().inSeconds(); const auto r = clipRect (w, ti); const int n0 = tr->getClips().size();
        const auto c = r.getCentre(); mouseDown (ev (c)); const float jump = (float) (pxPerBeat * timeToBeats (len) * 5.0);
        mouseDrag (ev ({ c.x + jump, c.y })); const int redDuring = brushNew.size(); mouseUp (ev ({ c.x + jump, c.y }));
        int found = 0; for (int k = 1; k <= 5; ++k) for (auto* cl : tr->getClips()) if (std::abs (cl->getPosition().getStart().inSeconds() - (s0 + k * len)) < 0.01) { ++found; break; }
        const int redAfter = brushNew.size(); tool = "select";
        for (int k = tr->getClips().size(); --k >= 0;) { auto* cl = tr->getClips()[k]; if (cl != &w && cl->getName().startsWith (w.getName().substring (0, 4))) cl->removeFromParent(); }
        if (found != 5) return "hay huecos: se pintaron " + juce::String (found) + " de 5";
        if (redDuring < 5) return "lo pintado no se marca en rojo mientras arrastras";
        if (redAfter != 0) return "lo rojo no se limpia al soltar";
        juce::ignoreUnused (n0); return {};
    }
    juce::String selfTestRightErase (te::WaveAudioClip& w, int ti)
    {   // clic derecho + arrastrar borra todos los clips que cruza (menos los bloqueados) y un clic derecho sin arrastrar no borra nada
        selectForDemo (&w); tool = "select"; auto src = juce::Desktop::getInstance().getMainMouseSource(); auto* tr = dynamic_cast<te::AudioTrack*> (w.getTrack()); if (tr == nullptr) return "sin pista";
        auto ev = [&] (juce::Point<float> p) { return juce::MouseEvent (src, p, juce::ModifierKeys (juce::ModifierKeys::rightButtonModifier), 1.f, 0.f, 0.f, 0.f, 0.f, this, this, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false); };
        const double s0 = timeToBeats (w.getPosition().getStart().inSeconds()), len = timeToBeats (w.getPosition().getEnd().inSeconds()) - s0; brushSrc = &w;
        for (int k = 1; k <= 4; ++k) brushPaint (ti, s0 + (double) k * len + 0.1);
        brushSrc = nullptr; brushNew.clear(); brushLastIdx = LONG_MIN; brushLastTrack = -1; const int made = tr->getClips().size() - 1; if (made != 4) return "no se pudieron preparar 4 clips (" + juce::String (made) + ")";
        te::Clip* keep = nullptr; for (auto* c : tr->getClips()) if (c != &w && std::abs (timeToBeats (c->getPosition().getStart().inSeconds()) - (s0 + 3.0 * len)) < 0.01) keep = c;
        if (keep == nullptr) return "falta el clip 3"; keep->state.setProperty ("injLocked", true, nullptr);
        const float y = clipRect (w, ti).getCentreY(), xa = beatToX (s0 + len * 1.2), xb = beatToX (s0 + len * 5.2);
        mouseDown (ev ({ xa, y })); mouseDrag (ev ({ xa + 2.f, y })); mouseDrag (ev ({ xb, y })); mouseUp (ev ({ xb, y }));
        int left = 0; for (auto* c : tr->getClips()) if (c != &w) ++left;
        const bool lockedSurvives = keep->getTrack() != nullptr; keep->state.setProperty ("injLocked", false, nullptr);
        for (int k = tr->getClips().size(); --k >= 0;) { auto* c = tr->getClips()[k]; if (c != &w) c->removeFromParent(); }
        if (w.getTrack() == nullptr) return "se borro el clip de origen (no se cruzo)";
        if (left != 1 || ! lockedSurvives) return "sobraron " + juce::String (left) + " clips (debia quedar solo el bloqueado)";
        return {};
    }
    juce::String selfTestLock (te::WaveAudioClip& w, int ti)
    {   // bloquear clip: con candado no se mueve (raton ni flechas), no se borra y al desbloquear vuelve a funcionar
        selectForDemo (&w); tool = "select"; auto src = juce::Desktop::getInstance().getMainMouseSource();
        auto ev = [&] (juce::Point<float> p) { return juce::MouseEvent (src, p, juce::ModifierKeys(), 1.f, 0.f, 0.f, 0.f, 0.f, this, this, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false); };
        const double s0 = w.getPosition().getStart().inSeconds(); const auto r = clipRect (w, ti); if (r.getWidth() < 60.f) return "clip demasiado corto";
        const auto ctrlL = juce::KeyPress ('l', juce::ModifierKeys::commandModifier, 0);
        keyPressed (ctrlL); if (! isLocked (&w)) return "Ctrl+L no bloquea";
        const auto c = r.getCentre(); mouseDown (ev (c)); mouseDrag (ev ({ c.x + 60.f, c.y })); mouseUp (ev ({ c.x + 60.f, c.y }));
        if (std::abs (w.getPosition().getStart().inSeconds() - s0) > 1e-6) return "el clip bloqueado se mueve con el raton";
        selectForDemo (&w); keyPressed (juce::KeyPress (juce::KeyPress::rightKey));
        if (std::abs (w.getPosition().getStart().inSeconds() - s0) > 1e-6) return "el clip bloqueado se mueve con las flechas";
        selectForDemo (&w); keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)); if (w.getTrack() == nullptr) return "el clip bloqueado se borra";
        selectForDemo (&w); keyPressed (ctrlL); if (isLocked (&w)) return "Ctrl+L no desbloquea";
        keyPressed (juce::KeyPress (juce::KeyPress::rightKey)); if (std::abs (w.getPosition().getStart().inSeconds() - s0) < 1e-6) return "al desbloquear el clip no se mueve";
        w.setStart (te::TimePosition::fromSeconds (s0), false, true); return {};
    }
    juce::String selfTestHandles (te::WaveAudioClip& w, int ti)
    {
        selectForDemo (&w); w.setFadeIn (te::TimeDuration()); w.setFadeOut (te::TimeDuration()); w.setGainDB (0.f);
        auto src = juce::Desktop::getInstance().getMainMouseSource();
        auto ev = [&] (juce::Point<float> p) { return juce::MouseEvent (src, p, juce::ModifierKeys(), 1.f, 0.f, 0.f, 0.f, 0.f, this, this, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false); };
        auto r = clipRect (w, ti); if (r.getWidth() < 80.f) return "clip demasiado corto";
        auto h = fadeInH (w, r).getCentre(); mouseDown (ev (h)); mouseDrag (ev ({ h.x + 40.f, h.y })); mouseUp (ev ({ h.x + 40.f, h.y }));
        if (w.getFadeIn().inSeconds() <= 0.0) return "fade in no cambia";
        h = fadeOutH (w, r).getCentre(); mouseDown (ev (h)); mouseDrag (ev ({ h.x - 40.f, h.y })); mouseUp (ev ({ h.x - 40.f, h.y }));
        if (w.getFadeOut().inSeconds() <= 0.0) return "fade out no cambia";
        h = gainH (r, 0.f).getCentre(); mouseDown (ev (h)); mouseDrag (ev ({ h.x, h.y + 25.f })); mouseUp (ev ({ h.x, h.y + 25.f }));
        if (w.getGainDB() >= -1.f) return "ganancia no baja";
        h = gainH (r, w.getGainDB()).getCentre(); mouseDoubleClick (ev (h)); if (std::abs (w.getGainDB()) > .01f) return "doble clic no resetea la ganancia";
        w.setFadeIn (te::TimeDuration()); w.setFadeOut (te::TimeDuration()); w.setGainDB (0.f); return {};
    }
    // ---- Fundidos y ganancia del clip de audio (manijas como en Ableton/FL) ----
    int fadeMode = 0; float lastGainY = 0.f; te::WaveAudioClip* fadeClip = nullptr;   // 1 entrada, 2 salida, 3 ganancia
    static constexpr float kGainMin = -24.f, kGainMax = 12.f;
    juce::Rectangle<float> clipRect (te::Clip& c, int ti) const { const float x0 = beatToX (timeToBeats (c.getPosition().getStart().inSeconds())), x1 = beatToX (timeToBeats (c.getPosition().getEnd().inSeconds())); return { x0, (float) trackTop (ti), std::max (2.f, x1 - x0), (float) hOf (ti) }; }
    float secToPx (double s) const { return beatToX (timeToBeats (s)) - beatToX (0.0); }
    float gainY (const juce::Rectangle<float>& r, float db) const { const float top = r.getY() + 18.f, h = r.getHeight() - 22.f; return top + (1.f - (db - kGainMin) / (kGainMax - kGainMin)) * h; }
    float gainFromY (const juce::Rectangle<float>& r, float y) const { const float top = r.getY() + 18.f, h = r.getHeight() - 22.f; float db = kGainMin + (1.f - (y - top) / h) * (kGainMax - kGainMin); db = juce::jlimit (kGainMin, kGainMax, db); return std::abs (db) < .6f ? 0.f : std::round (db * 2.f) / 2.f; }
    juce::Rectangle<float> fadeInH (te::WaveAudioClip& w, const juce::Rectangle<float>& r) const { return juce::Rectangle<float> (r.getX() + std::min (secToPx (w.getFadeIn().inSeconds()), r.getWidth() * .5f), r.getY() + 2.f, 9.f, 9.f); }
    juce::Rectangle<float> fadeOutH (te::WaveAudioClip& w, const juce::Rectangle<float>& r) const { return juce::Rectangle<float> (r.getRight() - std::min (secToPx (w.getFadeOut().inSeconds()), r.getWidth() * .5f) - 9.f, r.getY() + 2.f, 9.f, 9.f); }
    juce::Rectangle<float> gainH (const juce::Rectangle<float>& r, float db) const { return juce::Rectangle<float> (r.getX() + 6.f, gainY (r, db) - 5.f, 10.f, 10.f); }
    // true si el clic cayo en una manija; prepara el arrastre
    int handleModeAt (juce::Point<float> p, te::WaveAudioClip** out = nullptr)
    {
        if (p.x < kHeaderW + 6 || p.y < rulerH()) return 0; const int ti = trackAt (p.y); if (ti < 0) return 0;
        for (auto* c : te::getAudioTracks (edit)[ti]->getClips())
        {
            auto* w = dynamic_cast<te::WaveAudioClip*> (c); if (w == nullptr || ! (c == selected || inGroup (c))) continue;
            const auto r = clipRect (*w, ti); if (r.getWidth() < 30.f || r.getHeight() < 50.f || ! r.contains (p)) continue;
            int m = 0; if (fadeInH (*w, r).expanded (4.f).contains (p)) m = 1; else if (fadeOutH (*w, r).expanded (4.f).contains (p)) m = 2; else if (gainH (r, w->getGainDB()).expanded (4.f).contains (p)) m = 3;
            if (m) { if (out) *out = w; return m; }
        }
        return 0;
    }
    bool tryHandleDown (const juce::MouseEvent& e, te::Clip* hit, int ti)
    {
        auto* w = dynamic_cast<te::WaveAudioClip*> (hit); if (w == nullptr || isLocked (hit) || ! (tool == "select" || tool == "pencil") || ! (hit == selected || inGroup (hit))) return false;
        const auto r = clipRect (*w, ti); if (r.getWidth() < 30.f || r.getHeight() < 50.f) return false; const auto p = e.position;
        if (fadeInH (*w, r).expanded (4.f).contains (p)) fadeMode = 1; else if (fadeOutH (*w, r).expanded (4.f).contains (p)) fadeMode = 2; else if (gainH (r, w->getGainDB()).expanded (4.f).contains (p)) fadeMode = 3; else return false;
        fadeClip = w; lastGainY = e.position.y; selected = w; group.clear(); group.add (w); dragClip = nullptr; repaint(); return true;
    }
    void handleDrag (const juce::MouseEvent& e)
    {
        if (fadeClip == nullptr || fadeClip->getTrack() == nullptr) { fadeMode = 0; return; }
        const double cs = fadeClip->getPosition().getStart().inSeconds(), ce = fadeClip->getPosition().getEnd().inSeconds(), t = beatsToTime (xToBeat ((float) e.x));
        if (fadeMode == 1) fadeClip->setFadeIn (te::TimeDuration::fromSeconds (juce::jlimit (0.0, (ce - cs) - fadeClip->getFadeOut().inSeconds(), t - cs)));
        else if (fadeMode == 2) fadeClip->setFadeOut (te::TimeDuration::fromSeconds (juce::jlimit (0.0, (ce - cs) - fadeClip->getFadeIn().inSeconds(), ce - t)));
        else if (fadeMode == 3) { int ti = 0; auto tt = te::getAudioTracks (edit); for (int i = 0; i < tt.size(); ++i) if (tt[i] == fadeClip->getTrack()) ti = i; fadeClip->setGainDB (e.mods.isShiftDown() ? juce::jlimit (kGainMin, kGainMax, std::round ((fadeClip->getGainDB() - (float) (e.position.y - lastGainY) * .1f) * 10.f) / 10.f) : gainFromY (clipRect (*fadeClip, ti), e.position.y)); lastGainY = e.position.y; }
        repaint();
    }
    void drawClipHandles (juce::Graphics& g, te::Clip& c, int ti)
    {
        auto* w = dynamic_cast<te::WaveAudioClip*> (&c); if (w == nullptr) return;
        const auto r = clipRect (c, ti); if (r.getWidth() < 30.f || r.getHeight() < 50.f || r.getRight() < kHeaderW || r.getX() > getWidth()) return;
        g.saveState(); g.reduceClipRegion (r.toNearestInt());
        const float fi = std::min (secToPx (w->getFadeIn().inSeconds()), r.getWidth() * .5f), fo = std::min (secToPx (w->getFadeOut().inSeconds()), r.getWidth() * .5f);
        const float db = w->getGainDB(); const bool sel = &c == selected || inGroup (&c) || demoHandles || std::getenv ("INJ_DEMO_FADE") != nullptr;
        if (fi > 1.f) { juce::Path pa; pa.addTriangle (r.getX(), r.getY(), r.getX() + fi, r.getY(), r.getX(), r.getBottom()); g.setColour (juce::Colours::black.withAlpha (.28f)); g.fillPath (pa); g.setColour (juce::Colour (0xff191919).withAlpha (.8f)); g.drawLine (r.getX(), r.getBottom(), r.getX() + fi, r.getY(), 1.3f); }
        if (fo > 1.f) { juce::Path pa; pa.addTriangle (r.getRight(), r.getY(), r.getRight() - fo, r.getY(), r.getRight(), r.getBottom()); g.setColour (juce::Colours::black.withAlpha (.28f)); g.fillPath (pa); g.setColour (juce::Colour (0xff191919).withAlpha (.8f)); g.drawLine (r.getRight(), r.getBottom(), r.getRight() - fo, r.getY(), 1.3f); }
        const float ly = gainY (r, db);
        if (sel || db != 0.f) { g.setColour (juce::Colour (0xff191919).withAlpha (db != 0.f ? .9f : .45f)); for (float x = r.getX() + 18.f; x < r.getRight() - 4.f; x += 8.f) g.fillRect (x, ly - .5f, 4.f, 1.f); }
        if (sel)
        {
            g.setColour (juce::Colour (0xff191919)); g.fillRect (fadeInH (*w, r)); g.fillRect (fadeOutH (*w, r));
            auto gh = gainH (r, db); juce::Path d; d.addTriangle (gh.getCentreX(), gh.getY(), gh.getRight(), gh.getCentreY(), gh.getCentreX(), gh.getBottom()); d.addTriangle (gh.getCentreX(), gh.getY(), gh.getX(), gh.getCentreY(), gh.getCentreX(), gh.getBottom()); g.fillPath (d); g.setColour (juce::Colours::white.withAlpha (.75f)); g.strokePath (d, juce::PathStrokeType (1.f));
        }
        if (sel || db != 0.f) { g.setColour (juce::Colour (0xff191919)); g.setFont (juce::FontOptions (11.f, juce::Font::bold)); g.drawText ((db > 0 ? "+" : "") + juce::String (db, 1) + " dB", juce::Rectangle<float> (r.getX() + 20.f, r.getBottom() - 17.f, 70.f, 14.f).toNearestInt(), juce::Justification::centredLeft); }
        g.restoreState();
    }
    void drawClip (juce::Graphics& g, te::Clip& c, int ti)
    {
        using namespace theme;
        if (c.isMuted()) g.beginTransparencyLayer (.35f);
        struct EndLayer { juce::Graphics& g; bool on; ~EndLayer() { if (on) g.endTransparencyLayer(); } } endLayer { g, c.isMuted() };
        const float x0 = beatToX (timeToBeats (c.getPosition().getStart().inSeconds())), x1 = beatToX (timeToBeats (c.getPosition().getEnd().inSeconds()));
        if (x1 < (float) kHeaderW || x0 > (float) getWidth()) return;   // fuera de pantalla
        auto r = juce::Rectangle<float> (x0, (float) trackTop (ti), std::max (2.f, x1 - x0), (float) hOf (ti));
        auto col = trackutil::colourOf (dynamic_cast<te::AudioTrack*> (c.getTrack()), ti);
        if (c.state.hasProperty ("injColour")) col = juce::Colour ((juce::uint32) (int) c.state.getProperty ("injColour"));
        const bool muted = dynamic_cast<te::AudioTrack*> (c.getTrack())->isMuted (false);
        if (muted) col = col.withSaturation (col.getSaturation() * .25f).darker (.35f);
        // cuerpo oscuro y apagado, cabecera con el color pleno, tinta clara para ondas y notas
        const auto head = col.darker (.12f), body = col.darker (.62f).withMultipliedSaturation (.88f), ink = col.interpolatedWith (juce::Colours::white, .6f);
        col = body;
        const float cr = r.getWidth() > 6.f ? 3.f : 0.f; juce::Path shape; shape.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight() - 1.f, cr);
        g.setGradientFill (juce::ColourGradient (body.brighter (.04f), 0.f, r.getY(), body.darker (.05f), 0.f, r.getBottom(), false)); g.fillPath (shape);
        { g.saveState(); g.reduceClipRegion (shape, {}); g.setColour (juce::Colours::white.withAlpha (.05f));
          const double gs = pxPerBeat >= 14 ? 1.0 : timesig::bar(); for (double b = std::floor (xToBeat (r.getX()) / gs) * gs; beatToX (b) < r.getRight(); b += gs) g.fillRect (beatToX (b), r.getY(), 1.f, r.getHeight());
          const float bh = r.getHeight() >= 60.f ? 16.f : (r.getHeight() >= 40.f ? 15.f : 0.f);
          if (bh > 0.f) { g.setGradientFill (juce::ColourGradient (head.brighter (.07f), 0.f, r.getY(), head.darker (.03f), 0.f, r.getY() + bh, false)); g.fillRect (r.getX(), r.getY(), r.getWidth(), bh); g.setColour (juce::Colours::black.withAlpha (.12f)); g.fillRect (r.getX(), r.getY() + bh - 1.f, r.getWidth(), 1.f); g.setColour (juce::Colours::white.withAlpha (.07f)); g.fillRect (r.getX(), r.getY(), r.getWidth(), 1.f); }
          else { g.setColour (head); g.fillRect (r.getX(), r.getY(), 3.f, r.getHeight()); }
          g.restoreState(); }
        g.setColour (head.darker (.75f).withAlpha (.9f)); g.drawRoundedRectangle (r.getX() + .5f, r.getY() + .5f, r.getWidth() - 1.f, r.getHeight() - 2.f, cr, 1.f);
        if (c.isMuted()) { g.setColour (juce::Colour (0xff1b1b1b).withAlpha (.62f)); g.fillRect (r); }
        if (brushNew.contains (&c)) { g.setColour (accent.withAlpha (.38f)); g.fillRect (r); g.setColour (accentHi); g.drawRect (r, 1.5f); }
        if (isLocked (&c) && r.getWidth() > 18.f && r.getHeight() > 14.f)
        {   // candado en la esquina superior derecha
            const float lx = r.getRight() - 15.f, ly = r.getY() + 3.f; g.setColour (juce::Colours::black.withAlpha (.55f)); g.fillRoundedRectangle (lx - 2.f, ly - 2.f, 14.f, 16.f, 3.f);
            g.setColour (juce::Colours::white.withAlpha (.95f)); g.fillRoundedRectangle (lx, ly + 5.f, 10.f, 7.f, 1.5f);
            juce::Path sh; sh.addArc (lx + 2.f, ly, 6.f, 8.f, -juce::MathConstants<float>::halfPi * 2.f, 0.f, true); g.strokePath (sh, juce::PathStrokeType (1.5f));
            g.setColour (juce::Colours::white.withAlpha (.3f)); g.drawRect (r.reduced (.5f), 1.f);
        }
        if (&c == selected || inGroup (&c)) { g.setColour (juce::Colours::white.withAlpha (.10f)); g.fillRect (r); g.setColour (juce::Colour (0xfff4f5f7)); g.drawRoundedRectangle (r.getX() + .5f, r.getY() + .5f, r.getWidth() - 1.f, r.getHeight() - 2.f, cr, 1.5f); }
                if (r.getHeight() >= 26.f && r.getWidth() > 24.f) { g.setColour (r.getHeight() >= 40.f ? textOn (head) : juce::Colour (0xfff4f5f7).withAlpha (.92f)); g.setFont (fonts::label (11.f));
        g.drawText (c.getName(), r.toNearestInt().reduced (6, 1).withHeight (r.getHeight() >= 60.f ? 16 : 15), juce::Justification::centredLeft, true); }
        if (auto* sc = dynamic_cast<te::StepClip*> (&c))
        {
            auto pat = sc->getPattern (0); const int n = pat.getNumNotes(); const double cs = timeToBeats (c.getPosition().getStart().inSeconds()), ce = timeToBeats (c.getPosition().getEnd().inSeconds());
            const int rows = std::min (4, sc->getChannels().size()); const float rh = std::min (14.f, (r.getHeight() - 30.f) / (float) std::max (1, rows));
            for (double b0 = 0; b0 < ce - cs; b0 += n * 0.25)
                for (int rr = 0; rr < rows; ++rr) for (int s = 0; s < n; ++s)
                {
                    if (! pat.getNote (rr, s)) continue; const double bb = b0 + s * 0.25; if (bb >= ce - cs) break;
                    g.setColour (ink); g.fillRect (beatToX (cs + bb), r.getY() + 24.f + rr * rh, std::max (2.f, (float) (0.25 * pxPerBeat) - 1.f), rh - 2.f);
                }
        }
        if (auto* mc = dynamic_cast<te::MidiClip*> (&c))
        {
            const double cs = timeToBeats (c.getPosition().getStart().inSeconds()); int lo = 127, hi = 0;
            for (auto* n : mc->getSequence().getNotes()) { lo = std::min (lo, n->getNoteNumber()); hi = std::max (hi, n->getNoteNumber()); }
            if (lo <= hi)
            {
                const float band = r.getHeight() >= 60.f ? 16.f : (r.getHeight() >= 40.f ? 15.f : 0.f), pad = r.getHeight() >= 60.f ? 8.f : 3.f, area = r.getHeight() - band - 2.f * pad, rows = (float) (hi - lo + 1), step = area / std::max (rows, 4.f), th = juce::jlimit (2.f, 4.f, step * .7f);
                const float top = r.getY() + band + pad + (area - step * rows) * .5f; g.setColour (ink);
                for (auto* n : mc->getSequence().getNotes())
                    g.fillRect (beatToX (cs + n->getStartBeat().inBeats()), top + (float) (hi - n->getNoteNumber()) * step + (step - th) * .5f, std::max (3.f, (float) (n->getLengthBeats().inBeats() * pxPerBeat) - 1.f), th);
            }
        }
        if (auto* wc = dynamic_cast<te::WaveAudioClip*> (&c))
        {
            auto file = wc->getOriginalFile(); auto& th = thumbs[file.getFullPathName()];
            if (! th) { th = std::make_unique<juce::AudioThumbnail> (48, fm, thumbCache); th->setSource (new juce::FileInputSource (file)); }
            g.setColour (ink.withAlpha (.62f));
            const auto wr = (r.getHeight() >= 60.f ? r.toNearestInt().reduced (0, 6).withTrimmedTop (16) : (r.getHeight() >= 40.f ? r.toNearestInt().reduced (0, 2).withTrimmedTop (8) : r.toNearestInt().reduced (0, 2)));
            th->drawChannels (g, wr, wc->getPosition().getOffset().inSeconds(), wc->getPosition().getOffset().inSeconds() + c.getPosition().getLength().inSeconds(), juce::jmin (4.f, juce::Decibels::decibelsToGain (wc->getGainDB())));
            {   // la onda sigue el fundido: se tapa lo que queda fuera de la envolvente
                const float fi = std::min (secToPx (wc->getFadeIn().inSeconds()), r.getWidth() * .5f), fo = std::min (secToPx (wc->getFadeOut().inSeconds()), r.getWidth() * .5f);
                const auto wf = wr.toFloat(); const float mid = wf.getCentreY(); g.setColour (col);
                if (fi > 2.f) { juce::Path a1, a2; a1.addTriangle (wf.getX(), wf.getY(), wf.getX() + fi, wf.getY(), wf.getX(), mid); a2.addTriangle (wf.getX(), wf.getBottom(), wf.getX() + fi, wf.getBottom(), wf.getX(), mid); g.fillPath (a1); g.fillPath (a2); }
                if (fo > 2.f) { juce::Path a1, a2; a1.addTriangle (wf.getRight(), wf.getY(), wf.getRight() - fo, wf.getY(), wf.getRight(), mid); a2.addTriangle (wf.getRight(), wf.getBottom(), wf.getRight() - fo, wf.getBottom(), wf.getRight(), mid); g.fillPath (a1); g.fillPath (a2); }
            }
        }
    }
public:
    //==============================================================================
    // Comandos de la paleta (Ctrl+K). Cada uno hace una sola cosa, es un solo paso de deshacer y devuelve cuántas cosas cambió.
    int cmdClips (const juce::String& op)
    {
        auto* um = &edit.getUndoManager(); auto& tc = edit.getTransport(); auto tracks = te::getAudioTracks (edit);
        auto bS = [&] (te::Clip* c) { return timeToBeats (c->getPosition().getStart().inSeconds()); };
        auto bE = [&] (te::Clip* c) { return timeToBeats (c->getPosition().getEnd().inSeconds()); };
        auto atB = [&] (double b) { return te::TimePosition::fromSeconds (beatsToTime (std::max (0.0, b))); };
        const double head = timeToBeats (tc.getPosition().inSeconds());
        auto allClips = [&] { juce::Array<te::Clip*> v; for (auto* t : tracks) for (auto* c : t->getClips()) v.add (c); return v; };
        auto choose = [&] (const juce::Array<te::Clip*>& v) { group.clear(); for (auto* c : v) group.add (c); selected = group.size() == 1 ? group.getUnchecked (0).get() : nullptr; if (group.size() == 1) group.clear(); repaint(); return v.size(); };
        if (op == "selright" || op == "selleft") { juce::Array<te::Clip*> v; for (auto* c : allClips()) if (op == "selright" ? bS (c) >= head - 1e-6 : bE (c) <= head + 1e-6) v.add (c); if (v.isEmpty()) say (juce::String::fromUTF8 ("No hay clips de ese lado del cabezal")); return choose (v); }
        if (op == "invertsel") { auto cur = selectedClips(); juce::Array<te::Clip*> v; for (auto* c : allClips()) if (! cur.contains (c)) v.add (c); return choose (v); }
        auto cs = selectedClips();
        if (op == "selmidi" || op == "selaudio" || op == "selmuted") { juce::Array<te::Clip*> v; for (auto* c : allClips()) { const bool mi = dynamic_cast<te::MidiClip*> (c) != nullptr, au = dynamic_cast<te::WaveAudioClip*> (c) != nullptr; if (op == "selmidi" ? mi : (op == "selaudio" ? au : c->isMuted())) v.add (c); } if (v.isEmpty()) say (juce::String::fromUTF8 ("No hay clips de ese tipo")); return choose (v); }
        if (op == "selalt") { auto cur = selectedClips(); if (cur.size() < 2) { say (juce::String::fromUTF8 ("Elige al menos 2 clips")); return 0; } std::sort (cur.begin(), cur.end(), [&] (te::Clip* x, te::Clip* y) { return bS (x) < bS (y); }); juce::Array<te::Clip*> v; for (int i = 0; i < cur.size(); i += 2) v.add (cur[i]); return choose (v); }
        if (op == "delmuted") { juce::Array<te::Clip*> v; for (auto* c : (cs.isEmpty() ? allClips() : cs)) if (c->isMuted() && ! isLocked (c)) v.add (c); if (v.isEmpty()) { say (juce::String::fromUTF8 ("No hay clips silenciados")); return 0; } um->beginNewTransaction (juce::String::fromUTF8 ("Eliminar silenciados")); group.clear(); selected = nullptr; for (auto* c : v) killClip (c); return v.size(); }
        if (cs.isEmpty()) { say (juce::String::fromUTF8 ("Elige primero uno o más clips")); return 0; }
        double lo = 1e12, hi = 0; for (auto* c : cs) { lo = std::min (lo, bS (c)); hi = std::max (hi, bE (c)); }
        int n = 0;
        if (op == "gainup" || op == "gaindown")
        {
            um->beginNewTransaction (juce::String::fromUTF8 ("Ganancia del clip")); for (auto* c : cs) if (auto* wc = dynamic_cast<te::WaveAudioClip*> (c)) if (! isLocked (c)) { wc->setGainDB (juce::jlimit (-60.f, 24.f, wc->getGainDB() + (op == "gainup" ? 3.f : -3.f))); ++n; }
            if (n == 0) say (juce::String::fromUTF8 ("La ganancia es solo para clips de audio")); repaint(); return n;
        }
        if (op == "fadeshort" || op == "nofades")
        {
            um->beginNewTransaction (juce::String::fromUTF8 ("Fundidos")); const auto d = te::TimeDuration::fromSeconds (op == "fadeshort" ? 0.005 : 0.0);
            for (auto* c : cs) if (auto* wc = dynamic_cast<te::WaveAudioClip*> (c)) if (! isLocked (c)) { wc->setFadeIn (d); wc->setFadeOut (d); ++n; }
            if (n == 0) say (juce::String::fromUTF8 ("Los fundidos son solo para clips de audio")); repaint(); return n;
        }
        if (op == "quant")
        {
            um->beginNewTransaction (juce::String::fromUTF8 ("Cuantizar clips")); const double st = snapStep();
            for (auto* c : cs) { if (isLocked (c)) continue; const double b = bS (c), q = std::round (b / st) * st; if (std::abs (q - b) > 1e-6) { c->setStart (atB (q), false, true); ++n; } }
            repaint(); return n;
        }
        if (op == "closegaps" || op == "endtoend")
        {
            um->beginNewTransaction (juce::String::fromUTF8 (op == "closegaps" ? "Cerrar huecos" : "Poner uno tras otro"));
            for (auto* t : tracks)
            {
                std::vector<te::Clip*> v; for (auto* c : t->getClips()) if (op == "closegaps" ? cs.contains (c) || (cs.size() == 1 && c->getTrack() == cs[0]->getTrack()) : cs.contains (c)) v.push_back (c);
                if (op == "closegaps" && cs.size() >= 1) { v.clear(); bool use = false; for (auto* c : cs) if (c->getTrack() == t) use = true; if (use) for (auto* c : t->getClips()) v.push_back (c); }
                std::sort (v.begin(), v.end(), [&] (te::Clip* a, te::Clip* b) { return bS (a) < bS (b); });
                double cur = v.empty() ? 0.0 : bS (v.front());
                for (auto* c : v) { if (isLocked (c)) { cur = std::max (cur, bE (c)); continue; } const double len = bE (c) - bS (c); if (std::abs (bS (c) - cur) > 1e-6) { c->setStart (atB (cur), false, true); ++n; } cur += len; }
            }
            repaint(); return n;
        }
        if (op == "aligncursor")
        {
            um->beginNewTransaction (juce::String::fromUTF8 ("Mover al cabezal")); const double d = head - lo;
            for (auto* c : cs) if (! isLocked (c)) { c->setStart (atB (bS (c) + d), false, true); ++n; } repaint(); return n;
        }
        if (op == "splitbars" || op.startsWith ("split:"))
        {   // corta solo los clips elegidos (no los vecinos de la pista); los trozos no bajan de 1/16 de pulso*4 = 0.25 pulsos
            const int parts = op.startsWith ("split:") ? std::max (2, op.fromFirstOccurrenceOf (":", false, false).getIntValue()) : 0;
            struct Job { te::Clip* c; te::AudioTrack* at; std::vector<double> cuts; }; std::vector<Job> jobs; int tooSmall = 0;
            for (auto* c : cs)
            {
                if (isLocked (c)) continue; auto* at = dynamic_cast<te::AudioTrack*> (c->getTrack()); if (at == nullptr) continue; const double a = bS (c), b = bE (c); Job j { c, at, {} };
                if (parts > 0) { if ((b - a) / parts < 0.25 - 1e-9) { ++tooSmall; continue; } for (int k = 1; k < parts; ++k) j.cuts.push_back (a + (b - a) * k / parts); }
                else { const double bar = timesig::bar(); for (double x = std::floor (a / bar + 1e-9) * bar + bar; x < b - 1e-6; x += bar) if (x - a >= 0.25 - 1e-9 && b - x >= 0.25 - 1e-9) j.cuts.push_back (x); }
                if (! j.cuts.empty()) jobs.push_back (std::move (j));
            }
            if (jobs.empty()) { say (juce::String::fromUTF8 (tooSmall > 0 ? "Los clips son muy cortos para dividirlos así" : "Nada que dividir")); return 0; }
            um->beginNewTransaction (juce::String::fromUTF8 ("Dividir clips")); juce::Array<te::Clip*> pieces;
            for (auto& j : jobs) { te::Clip* cur = j.c; pieces.add (cur); for (double cut : j.cuts) { auto* nc = j.at->splitClip (*cur, atB (cut)); if (nc == nullptr) break; pieces.add (nc); cur = nc; ++n; } }
            selectClips (pieces); return n;
        }
        {   // ---- herramientas de clips MIDI: actúan sobre las notas de los clips MIDI elegidos
            const bool midiOp = op == "quantnotes" || op == "vel100" || op == "dedupe" || op == "killshort" || op == "reversemidi" || op == "doublemidi" || op == "stretch2" || op == "stretchhalf";
            if (midiOp)
            {
                um->beginNewTransaction (juce::String::fromUTF8 ("Notas de los clips")); int clipsDone = 0;
                for (auto* c : cs)
                {
                    auto* mc = dynamic_cast<te::MidiClip*> (c); if (mc == nullptr || isLocked (c)) continue; auto& seq = mc->getSequence(); const double L = bE (c) - bS (c); bool did = false;
                    if ((op == "doublemidi" || op == "stretch2") && (L * 2.0 > 256.0 || (op == "doublemidi" && mc->getSequence().getNumNotes() * 2 > 4096))) { say (juce::String::fromUTF8 ("El clip ya es muy largo para duplicarlo")); continue; }
                    std::vector<te::MidiNote*> ns; for (auto* nt : seq.getNotes()) ns.push_back (nt);
                    if (op == "quantnotes") { for (auto* nt : ns) { const double st = nt->getStartBeat().inBeats(), q = std::round (st / 0.25) * 0.25; if (std::abs (q - st) > 1e-6) { nt->setStartAndLength (te::BeatPosition::fromBeats (std::max (0.0, q)), nt->getLengthBeats(), um); ++n; did = true; } } }
                    else if (op == "vel100") { for (auto* nt : ns) if (nt->getVelocity() != 100) { nt->setVelocity (100, um); ++n; did = true; } }
                    else if (op == "dedupe") { for (size_t i = 0; i < ns.size(); ++i) for (size_t j = i + 1; j < ns.size(); ++j) if (ns[i] != nullptr && ns[j] != nullptr && ns[i]->getNoteNumber() == ns[j]->getNoteNumber() && std::abs (ns[i]->getStartBeat().inBeats() - ns[j]->getStartBeat().inBeats()) < 1e-4) { seq.removeNote (*ns[j], um); ns[j] = nullptr; ++n; did = true; } }
                    else if (op == "killshort") { for (auto* nt : ns) if (nt->getLengthBeats().inBeats() < 0.125 - 1e-9) { seq.removeNote (*nt, um); ++n; did = true; } }
                    else if (op == "reversemidi") { for (auto* nt : ns) { const double st = nt->getStartBeat().inBeats(), ln = nt->getLengthBeats().inBeats(); nt->setStartAndLength (te::BeatPosition::fromBeats (std::max (0.0, L - st - ln)), nt->getLengthBeats(), um); ++n; did = true; } }
                    else if (op == "doublemidi") { for (auto* nt : ns) { seq.addNote (nt->getNoteNumber(), te::BeatPosition::fromBeats (nt->getStartBeat().inBeats() + L), nt->getLengthBeats(), nt->getVelocity(), 0, um); ++n; } mc->setEnd (atB (bS (c) + 2.0 * L), true); did = true; }
                    else { const double f = op == "stretch2" ? 2.0 : 0.5; for (auto* nt : ns) { nt->setStartAndLength (te::BeatPosition::fromBeats (nt->getStartBeat().inBeats() * f), te::BeatDuration::fromBeats (nt->getLengthBeats().inBeats() * f), um); ++n; } mc->setEnd (atB (bS (c) + L * f), true); did = true; }
                    if (did) ++clipsDone;
                }
                if (clipsDone == 0 && n == 0) say (juce::String::fromUTF8 ("Elige clips MIDI con notas que cambiar")); if (onClipChanged2) onClipChanged2(); repaint(); return n;
            }
        }
        if (op == "repeat2" || op == "repeat4" || op == "repeat8")
        {
            const int times = op == "repeat2" ? 1 : op == "repeat4" ? 3 : 7; int total = 0; for (auto* t : tracks) total += t->getClips().size();
            if (total + cs.size() * times > 3000) { say (juce::String::fromUTF8 ("El proyecto ya es demasiado grande para repetir tantos clips")); return 0; }
            um->beginNewTransaction (juce::String::fromUTF8 ("Repetir clips")); juce::Array<te::Clip*> made;
            for (auto* c : cs) { auto* ct = dynamic_cast<te::ClipTrack*> (c->getTrack()); if (ct == nullptr) continue; const auto len = c->getPosition().getLength();
                for (int k = 1; k <= times; ++k) if (auto* nc = trackutil::copyClipTo (edit, *c, *ct, c->getPosition().getStart() + len * (double) k)) { made.add (nc); ++n; } }
            if (n > 0) choose (made); repaint(); return n;
        }
        if (op == "nudgel" || op == "nudger" || op == "nudgelb" || op == "nudgerb")
        {
            const double d = (op.endsWith ("b") ? timesig::bar() : 0.25) * (op.startsWith ("nudgel") ? -1.0 : 1.0); um->beginNewTransaction (juce::String::fromUTF8 ("Empujar clips"));
            for (auto* c : cs) { if (isLocked (c)) continue; const double b = bS (c), nb = std::max (0.0, b + d); if (std::abs (nb - b) > 1e-9) { c->setStart (atB (nb), false, true); ++n; } }
            repaint(); return n;
        }
        if (op == "reverseaudio")
        {
            um->beginNewTransaction (juce::String::fromUTF8 ("Invertir audio"));
            for (auto* c : cs) if (auto* ac = dynamic_cast<te::AudioClipBase*> (c)) if (! isLocked (c)) { ac->setIsReversed (! ac->getIsReversed()); ++n; }
            if (n == 0) say (juce::String::fromUTF8 ("Invertir el sonido es solo para clips de audio")); repaint(); return n;
        }
        if (op == "crossfade")
        {
            um->beginNewTransaction (juce::String::fromUTF8 ("Fundido cruzado"));
            for (auto* t : tracks)
            {
                std::vector<te::WaveAudioClip*> v; for (auto* c : t->getClips()) if (auto* wc = dynamic_cast<te::WaveAudioClip*> (c)) if (cs.isEmpty() || cs.contains (c)) v.push_back (wc);
                std::sort (v.begin(), v.end(), [] (te::WaveAudioClip* a, te::WaveAudioClip* b) { return a->getPosition().getStart() < b->getPosition().getStart(); });
                for (size_t i = 0; i + 1 < v.size(); ++i)
                {
                    auto *a = v[i], *b = v[i + 1]; if (isLocked (a) || isLocked (b)) continue; const double ov = a->getPosition().getEnd().inSeconds() - b->getPosition().getStart().inSeconds();
                    if (ov > 0.0005 && ov < b->getPosition().getLength().inSeconds()) { a->setFadeOut (te::TimeDuration::fromSeconds (ov)); b->setFadeIn (te::TimeDuration::fromSeconds (ov)); ++n; }
                }
            }
            if (n == 0) say (juce::String::fromUTF8 ("No hay clips de audio que se solapen")); repaint(); return n;
        }
        if (op == "lockclips" || op == "unlockclips")
        {
            um->beginNewTransaction (juce::String::fromUTF8 (op == "lockclips" ? "Bloquear clips" : "Desbloquear clips"));
            for (auto* c : cs) { c->state.setProperty ("injLocked", op == "lockclips", um); ++n; } repaint(); return n;
        }
        if (op == "trimbars")
        {
            um->beginNewTransaction (juce::String::fromUTF8 ("Largo en compases enteros")); const double bar = timesig::bar();
            for (auto* c : cs) { if (isLocked (c)) continue; const double len = bE (c) - bS (c), w = std::max (1.0, std::round (len / bar)) * bar; if (std::abs (w - len) > 1e-4) { c->setEnd (atB (bS (c) + w), true); ++n; } } repaint(); return n;
        }
        if (op == "resetcolor") { um->beginNewTransaction (juce::String::fromUTF8 ("Color de los clips")); for (auto* c : cs) if (c->state.hasProperty ("injColour")) { c->state.removeProperty ("injColour", um); ++n; } repaint(); return n; }
        if (op == "nameastrack")
        {
            um->beginNewTransaction (juce::String::fromUTF8 ("Nombre de los clips"));
            for (auto* c : cs) if (auto* at = dynamic_cast<te::AudioTrack*> (c->getTrack())) { const juce::String nm = at->getName() == "Track" ? juce::String::fromUTF8 ("Pista ") + juce::String (tracks.indexOf (at) + 1) : at->getName(); if (c->getName() != nm) { c->setName (nm); ++n; } }
            repaint(); return n;
        }
        if (op == "flipclips" || op == "spread")
        {
            if (cs.size() < 2) { say (juce::String::fromUTF8 ("Elige al menos 2 clips")); return 0; } um->beginNewTransaction (juce::String::fromUTF8 (op == "flipclips" ? "Voltear clips" : "Un clip por compás"));
            if (op == "flipclips") { std::vector<std::pair<te::Clip*, double>> to; for (auto* c : cs) if (! isLocked (c)) to.push_back ({ c, lo + hi - bE (c) }); for (auto& pr : to) { pr.first->setStart (atB (pr.second), false, true); ++n; } }
            else { auto v = cs; std::sort (v.begin(), v.end(), [&] (te::Clip* x, te::Clip* y) { return bS (x) < bS (y) || (bS (x) == bS (y) && tracks.indexOf (dynamic_cast<te::AudioTrack*> (x->getTrack())) < tracks.indexOf (dynamic_cast<te::AudioTrack*> (y->getTrack()))); });
                   int k = 0; for (auto* c : v) { if (! isLocked (c)) { c->setStart (atB (lo + k * timesig::bar()), false, true); ++n; } ++k; } }
            repaint(); return n;
        }
        if (op == "loopsel") { tc.setLoopRange (te::TimeRange (atB (lo), atB (hi))); tc.looping = true; repaint(); return (int) cs.size(); }
        if (op == "gotostart") { tc.setPosition (atB (lo)); repaint(); return 1; }
        if (op == "gotoend") { tc.setPosition (atB (hi)); repaint(); return 1; }
        if (op == "markers") { for (auto* c : cs) { addMarker (bS (c), c->getName()); ++n; } return n; }
        if (op == "up" || op == "down")
        {
            const int dir = op == "up" ? -1 : 1; for (auto* c : cs) { const int i = tracks.indexOf (dynamic_cast<te::AudioTrack*> (c->getTrack())); if (i < 0 || i + dir < 0 || i + dir >= tracks.size() || isLocked (c)) { say (juce::String::fromUTF8 ("No hay más pistas en esa dirección")); return 0; } }
            um->beginNewTransaction (juce::String::fromUTF8 ("Mover de pista")); for (auto* c : cs) { const int i = tracks.indexOf (dynamic_cast<te::AudioTrack*> (c->getTrack())); c->moveTo (*tracks[i + dir]); ++n; }
            if (! cs.isEmpty()) { selTrack = tracks.indexOf (dynamic_cast<te::AudioTrack*> (cs[0]->getTrack())); if (onTrackSelected) onTrackSelected (selTrack); } repaint(); return n;
        }
        if (op == "dupnewtrack")
        {
            { int total = 0; for (auto* t : tracks) total += t->getClips().size(); if (total + cs.size() > 3000) { say (juce::String::fromUTF8 ("Ya hay demasiados clips en el proyecto")); return 0; } }
            um->beginNewTransaction (juce::String::fromUTF8 ("Clips a pista nueva")); auto ntp = edit.insertNewAudioTrack (te::TrackInsertPoint::getEndOfTracks (edit), nullptr); auto* nt = ntp.get(); if (nt == nullptr) return 0;
            const double d = 0.0; group.clear(); juce::Array<te::Clip*> made; for (auto* c : cs) if (auto* nc = trackutil::copyClipTo (edit, *c, *nt, atB (bS (c) + d))) made.add (nc);
            if (onTracksChanged) onTracksChanged(); selTrack = te::getAudioTracks (edit).indexOf (nt); if (onTrackSelected) onTrackSelected (selTrack); return choose (made);
        }
        return 0;
    }
    int cmdTracks (const juce::String& op)
    {
        auto* um = &edit.getUndoManager(); auto tracks = te::getAudioTracks (edit); const int ti = selTrack >= 0 && selTrack < tracks.size() ? selTrack : (tracks.isEmpty() ? -1 : 0);
        auto pick = [&] (int i) { i = juce::jlimit (0, std::max (0, tracks.size() - 1), i); selTrack = i; if (onTrackSelected) onTrackSelected (i); repaint(); return 1; };
        if (op == "prevtrack") return tracks.isEmpty() ? 0 : pick (ti - 1);
        if (op == "nexttrack") return tracks.isEmpty() ? 0 : pick (ti + 1);
        if (op == "foldall" || op == "unfoldall") { int n = 0; for (int i = 0; i < tracks.size(); ++i) if (isFolded (i) != (op == "foldall")) { toggleFold (i); ++n; } return n; }
        if (op == "mutetrack" || op == "solotrack") { if (ti < 0) return 0; auto* t = tracks[ti]; if (op == "mutetrack") t->setMute (! t->isMuted (false)); else t->setSolo (! t->isSolo (false)); repaint(); return 1; }
        if (op == "vol0" || op == "headroom") { int n = 0; um->beginNewTransaction (juce::String::fromUTF8 (op == "vol0" ? "Volúmenes a 0 dB" : "Margen de 3 dB")); for (auto* t : tracks) if (auto* vp = t->getVolumePlugin()) { const float v = op == "vol0" ? 0.f : juce::jmax (-60.f, vp->getVolumeDb() - 3.f); if (std::abs (vp->getVolumeDb() - v) > 1e-3f) { vp->setVolumeDb (v); ++n; } } repaint(); return n; }
        if (op == "rainbow") { um->beginNewTransaction (juce::String::fromUTF8 ("Colores por pista")); int n = 0; for (int i = 0; i < tracks.size(); ++i) { if ((int) tracks[i]->state.getProperty ("injColour", -1) != i % 8) ++n; trackutil::setColour (edit, tracks[i], i % 8); } if (onTracksChanged) onTracksChanged(); repaint(); return n; }
        if (op == "panreset") { int n = 0; um->beginNewTransaction (juce::String::fromUTF8 ("Centrar paneos")); for (auto* t : tracks) if (auto* vp = t->getVolumePlugin()) if (std::abs (vp->getPan()) > 1e-4f) { vp->setPan (0.f); ++n; } repaint(); return n; }
        if (op == "delempty")
        {
            std::vector<te::AudioTrack*> v; for (auto* t : tracks) if (t->getClips().isEmpty()) v.push_back (t);
            if ((int) v.size() >= tracks.size()) v.pop_back();   // siempre queda una pista
            if (v.empty()) { say (juce::String::fromUTF8 ("No hay pistas vacías")); return 0; }
            um->beginNewTransaction (juce::String::fromUTF8 ("Quitar pistas vacías")); selected = nullptr; group.clear(); dragClip = nullptr; for (auto* t : v) trackutil::deleteTrack (edit, t);
            selTrack = -1; if (onTracksChanged) onTracksChanged(); repaint(); return (int) v.size();
        }
        if (op == "duptrack")
        {
            if (ti < 0) return 0; { int total = 0; for (auto* t : tracks) total += t->getClips().size(); if (total + tracks[ti]->getClips().size() > 3000) { say (juce::String::fromUTF8 ("El proyecto ya es demasiado grande para duplicar la pista")); return 0; } }
            auto* src = tracks[ti]; um->beginNewTransaction (juce::String::fromUTF8 ("Duplicar pista"));
            auto ntp = edit.insertNewAudioTrack (te::TrackInsertPoint (nullptr, src), nullptr); auto* nt = ntp.get(); if (nt == nullptr) return 0;
            nt->setName (src->getName() == "Track" ? juce::String::fromUTF8 ("Pista ") + juce::String (ti + 1) + " copia" : src->getName() + " copia");
            for (auto prop : { "injColour", "injIcon", "injH" }) if (src->state.hasProperty (prop)) nt->state.setProperty (prop, src->state.getProperty (prop), um);
            if (auto* a = src->getVolumePlugin()) if (auto* b = nt->getVolumePlugin()) { b->setVolumeDb (a->getVolumeDb()); b->setPan (a->getPan()); }
            trackutil::copyPlugins (edit, *src, *nt);
            for (auto* c : src->getClips()) trackutil::copyClipTo (edit, *c, *nt, c->getPosition().getStart());
            if (onTracksChanged) onTracksChanged(); selTrack = te::getAudioTracks (edit).indexOf (nt); if (onTrackSelected) onTrackSelected (selTrack); repaint(); return 1;
        }
        if (op == "clearmarkers") { auto ms = markers(); if (ms.empty()) { say (juce::String::fromUTF8 ("No hay marcadores")); return 0; } um->beginNewTransaction (juce::String::fromUTF8 ("Quitar marcadores")); for (auto& m : ms) edit.state.removeChild (m.v, um); repaint(); return (int) ms.size(); }
        return 0;
    }
    int selectClips (const juce::Array<te::Clip*>& v) { group.clear(); for (auto* c : v) group.add (c); selected = group.size() == 1 ? group.getUnchecked (0).get() : nullptr; if (group.size() == 1) group.clear(); repaint(); return v.size(); }
    int selTrackIndex() const { return selTrack; }
    // Funciones "pro" al estilo Pro Tools / Ableton / Logic / Studio One: fundidos, ganancia, pan, volúmenes, efectos y bucles. Devuelve cuántos elementos cambió.
    int cmdPro (const juce::String& op)
    {
        auto* um = &edit.getUndoManager(); auto& tc = edit.getTransport(); auto tracks = te::getAudioTracks (edit);
        auto atB = [&] (double b) { return te::TimePosition::fromSeconds (beatsToTime (std::max (0.0, b))); };
        const double head = timeToBeats (tc.getPosition().inSeconds()), bar = timesig::bar();
        auto bS = [&] (te::Clip* c) { return timeToBeats (c->getPosition().getStart().inSeconds()); };
        auto bE = [&] (te::Clip* c) { return timeToBeats (c->getPosition().getEnd().inSeconds()); };
        const int ti = selTrack >= 0 && selTrack < tracks.size() ? selTrack : (tracks.isEmpty() ? -1 : 0);
        int n = 0;
        if (op.startsWith ("fi:") || op.startsWith ("fo:"))
        {   // fundido de entrada / salida en milisegundos
            auto cs = selectedClips(); if (cs.isEmpty()) { say (juce::String::fromUTF8 ("Elige primero uno o más clips")); return 0; }
            const auto d = te::TimeDuration::fromSeconds (op.fromFirstOccurrenceOf (":", false, false).getIntValue() / 1000.0);
            um->beginNewTransaction (juce::String::fromUTF8 ("Fundidos"));
            for (auto* c : cs) { auto* ac = dynamic_cast<te::AudioClipBase*> (c); if (isLocked (c) || ac == nullptr) continue; if (op.startsWith ("fi:")) ac->setFadeIn (d); else ac->setFadeOut (d); ++n; }
            repaint(); return n;
        }
        if (op == "gain1up" || op == "gain1down" || op == "gain0" || op.startsWith ("gaind:"))
        {
            um->beginNewTransaction (juce::String::fromUTF8 ("Ganancia del clip"));
            for (auto* c : selectedClips()) if (auto* wc = dynamic_cast<te::WaveAudioClip*> (c)) if (! isLocked (c)) { wc->setGainDB (op == "gain0" ? 0.f : juce::jlimit (-60.f, 24.f, wc->getGainDB() + (op.startsWith ("gaind:") ? op.fromFirstOccurrenceOf (":", false, false).getFloatValue() : op == "gain1up" ? 1.f : -1.f))); ++n; }
            if (n == 0) say (juce::String::fromUTF8 ("Elige un clip de audio")); repaint(); return n;
        }
        if (op == "clip_tostart" || op == "clip_snapbar" || op == "clip_trimhead" || op == "clip_trimtail" || op == "clip_numnames" || op == "clip_unmute" || op == "clip_insbar" || op == "clip_delshort")
        {
            auto cs = selectedClips();
            if (op == "clip_unmute") { um->beginNewTransaction (juce::String::fromUTF8 ("Activar clips")); for (auto* t : tracks) for (auto* c : t->getClips()) if (c->isMuted()) { c->setMuted (false); ++n; } repaint(); return n; }
            if (op == "clip_insbar") { um->beginNewTransaction (juce::String::fromUTF8 ("Insertar un compás")); juce::Array<te::Clip*> mv; for (auto* t : tracks) for (auto* c : t->getClips()) if (bS (c) >= head - 1e-6 && ! isLocked (c)) mv.add (c);
                std::sort (mv.begin(), mv.end(), [&] (te::Clip* a, te::Clip* b) { return bS (a) > bS (b); }); for (auto* c : mv) { c->setStart (atB (bS (c) + bar), false, true); ++n; } if (n == 0) say (juce::String::fromUTF8 ("No hay clips después del cabezal")); repaint(); return n; }
            if (op == "clip_delshort") { juce::Array<te::Clip*> v; for (auto* t : tracks) for (auto* c : t->getClips()) if (bE (c) - bS (c) < 0.25 && ! isLocked (c)) v.add (c); if (v.isEmpty()) { say (juce::String::fromUTF8 ("No hay clips más cortos que 1/16")); return 0; }
                um->beginNewTransaction (juce::String::fromUTF8 ("Quitar clips diminutos")); selectClips ({}); for (auto* c : v) { c->removeFromParent(); ++n; } repaint(); return n; }
            if (cs.isEmpty()) { say (juce::String::fromUTF8 ("Elige primero uno o más clips")); return 0; }
            um->beginNewTransaction (juce::String::fromUTF8 ("Herramientas de clips")); int k = 0;
            std::sort (cs.begin(), cs.end(), [&] (te::Clip* a, te::Clip* b) { return bS (a) < bS (b); });
            for (auto* c : cs)
            {
                if (isLocked (c)) continue; ++k;
                if (op == "clip_tostart") { c->setStart (atB (0.0), false, true); ++n; }
                else if (op == "clip_snapbar") { c->setStart (atB (std::round (bS (c) / bar) * bar), false, true); ++n; }
                else if (op == "clip_trimhead") { if (head > bS (c) + 1e-3 && head < bE (c) - 1e-3) { c->setStart (atB (head), true, false); ++n; } }
                else if (op == "clip_trimtail") { if (head > bS (c) + 1e-3 && head < bE (c) - 1e-3) { c->setEnd (atB (head), true); ++n; } }
                else if (op == "clip_numnames") { c->setName ("Clip " + juce::String (k)); ++n; }
            }
            if (n == 0 && (op == "clip_trimhead" || op == "clip_trimtail")) say (juce::String::fromUTF8 ("Pon el cabezal dentro del clip"));
            repaint(); return n;
        }
        if (op == "clip_snapend" || op.startsWith ("clip_len:"))
        {
            auto cs = selectedClips(); if (cs.isEmpty()) { say (juce::String::fromUTF8 ("Elige primero uno o más clips")); return 0; }
            const double bars = op.startsWith ("clip_len:") ? op.fromFirstOccurrenceOf (":", false, false).getDoubleValue() : 0.0;
            um->beginNewTransaction (juce::String::fromUTF8 ("Largo de clips"));
            for (auto* c : cs) { if (isLocked (c)) continue; const double e = bars > 0 ? bS (c) + bars * bar : std::max (bS (c) + bar, std::round (bE (c) / bar) * bar); c->setEnd (atB (e), true); ++n; }
            repaint(); return n;
        }
        if (op == "t_pan0" || op == "t_vol0" || op == "t_colors" || op == "t_solosel")
        {
            um->beginNewTransaction (juce::String::fromUTF8 ("Pistas"));
            for (int i = 0; i < tracks.size(); ++i)
            {
                if (op == "t_pan0") { if (auto* vp = tracks[i]->getVolumePlugin()) { vp->setPan (0.f); ++n; } }
                else if (op == "t_vol0") { if (auto* vp = tracks[i]->getVolumePlugin()) { vp->setVolumeDb (0.f); ++n; } }
                else if (op == "t_colors") { tracks[i]->state.setProperty ("injColour", i % 8, nullptr); ++n; }
                else { const bool on = i == ti; if (tracks[i]->isSolo (false) != on) { tracks[i]->setSolo (on); ++n; } }
            }
            repaint(); return n;
        }
        if (op == "p_bpmup" || op == "p_bpmdown" || op == "p_bpmround")
        {
            auto* tp = edit.tempoSequence.getTempo (0); if (tp == nullptr) return 0; const double b0 = tp->getBpm();
            const double nb = op == "p_bpmround" ? std::round (b0) : juce::jlimit (20.0, 999.0, b0 + (op == "p_bpmup" ? 1.0 : -1.0)); um->beginNewTransaction (juce::String::fromUTF8 ("Tempo")); tp->setBpm (nb); repaint(); return 1;
        }
        if (op.startsWith ("p_loop:") || op == "p_loopall" || op == "p_nextbar" || op == "p_prevbar")
        {
            if (op == "p_nextbar") { tc.setPosition (atB ((std::floor (head / bar + 1e-6) + 1.0) * bar)); repaint(); return 1; }
            if (op == "p_prevbar") { tc.setPosition (atB (std::max (0.0, (std::ceil (head / bar - 1e-6) - 1.0) * bar))); repaint(); return 1; }
            double s0 = std::floor (head / bar) * bar, len = op.fromFirstOccurrenceOf (":", false, false).getDoubleValue() * bar;
            if (op == "p_loopall") { double hi = 0; for (auto* t : tracks) for (auto* c : t->getClips()) hi = std::max (hi, bE (c)); if (hi <= 0) { say (juce::String::fromUTF8 ("No hay clips")); return 0; } s0 = 0; len = std::ceil (hi / bar) * bar; }
            tc.setLoopRange (te::TimeRange (atB (s0), atB (s0 + len))); tc.looping = true; repaint(); return 1;
        }
        if (op == "t_vol1up" || op == "t_vol1down" || op == "t_vol6down")
        {
            const float d = op == "t_vol1up" ? 1.f : op == "t_vol1down" ? -1.f : -6.f; um->beginNewTransaction (juce::String::fromUTF8 ("Volumen de las pistas"));
            for (auto* t : tracks) if (auto* vp = t->getVolumePlugin()) { vp->setVolumeDb (juce::jlimit (-60.f, 6.f, vp->getVolumeDb() + d)); ++n; } repaint(); return n;
        }
        if (op == "t_panalt" || op == "t_panwide" || op == "t_panrand")
        {
            um->beginNewTransaction (juce::String::fromUTF8 ("Pan de las pistas")); juce::Random rnd ((int) juce::Time::getMillisecondCounter());
            for (int i = 0; i < tracks.size(); ++i) if (auto* vp = tracks[i]->getVolumePlugin())
            {
                float p = 0.f; const int N = tracks.size();
                if (op == "t_panalt") p = (i % 2 == 0 ? -0.5f : 0.5f); else if (op == "t_panwide") p = N > 1 ? -0.8f + 1.6f * (float) i / (float) (N - 1) : 0.f; else p = rnd.nextFloat() * 1.6f - 0.8f;
                vp->setPan (p); ++n;
            }
            repaint(); return n;
        }
        if (op == "t_numnames") { um->beginNewTransaction (juce::String::fromUTF8 ("Nombres de pistas")); for (int i = 0; i < tracks.size(); ++i) { tracks[i]->setName ("Pista " + juce::String (i + 1)); ++n; } if (onTracksChanged) onTracksChanged(); repaint(); return n; }
        if (op == "t_unsolo") { for (auto* t : tracks) if (t->isSolo (false)) { t->setSolo (false); ++n; } repaint(); return n; }
        if (op == "t_unmute") { for (auto* t : tracks) if (t->isMuted (false)) { t->setMute (false); ++n; } repaint(); return n; }
        if (op == "t_muteothers") { if (ti < 0) return 0; um->beginNewTransaction (juce::String::fromUTF8 ("Silenciar las demás")); for (int i = 0; i < tracks.size(); ++i) { const bool m = i != ti; if (tracks[i]->isMuted (false) != m) { tracks[i]->setMute (m); ++n; } } repaint(); return n; }
        if (op == "t_fxbypass" || op == "t_fxenable" || op == "t_fxbypass_sel")
        {
            const bool en = op == "t_fxenable"; um->beginNewTransaction (juce::String::fromUTF8 ("Efectos de las pistas"));
            for (int i = 0; i < tracks.size(); ++i) { if (op == "t_fxbypass_sel" && i != ti) continue;
                for (auto* p : tracks[i]->pluginList.getPlugins()) { const auto ty = p->getPluginType(); if (! ty.startsWith ("inj") || ty == "injstereo") continue; if (p->isEnabled() != en) { p->setEnabled (en); ++n; } } }
            repaint(); return n;
        }
        if (op == "p_loopcursor") { const double s = std::floor (head / bar) * bar; tc.setLoopRange (te::TimeRange (atB (s), atB (s + bar))); tc.looping = true; repaint(); return 1; }
        if (op == "p_loopclear") { tc.looping = false; repaint(); return 1; }
        if (op == "p_gostart") { tc.setPosition (te::TimePosition::fromSeconds (0.0)); repaint(); return 1; }
        if (op == "p_goend") { double hi = 0; for (auto* t : tracks) for (auto* c : t->getClips()) hi = std::max (hi, bE (c)); tc.setPosition (atB (hi)); repaint(); return 1; }
        return 0;
    }
    void setSelTrackForTest (int i) { selTrack = i; }
    void gotoBar (int dir)
    {   // compás anterior / siguiente al cabezal
        auto& tc = edit.getTransport(); const double bar = timesig::bar(), cur = timeToBeats (tc.getPosition().inSeconds());
        const double k = dir > 0 ? std::floor (cur / bar + 1e-6) + 1 : std::ceil (cur / bar - 1e-6) - 1;
        tc.setPosition (te::TimePosition::fromSeconds (beatsToTime (std::max (0.0, k * bar)))); repaint();
    }
    void zoomBars (double bars) { const double avail = std::max (80.0, (double) getWidth() - kHeaderW - 6 - 40.0); zoomAnchorX = (float) kHeaderW + 6.f; tgtX = tgtY = -1; scrollX = std::max (0.0, timeToBeats (edit.getTransport().getPosition().inSeconds()) * pxPerBeat); zoomTarget = (float) juce::jlimit (0.25, 400.0, avail / (bars * timesig::bar())); repaint(); }
};
