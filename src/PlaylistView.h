#pragma once
#include "Instruments.h"
#include "Theme.h"
#include "TimeSig.h"
#include "Icons.h"
#include "Cursors.h"
#include "TrackUtil.h"
#include <tracktion_engine/tracktion_engine.h>
#include <functional>

namespace te = tracktion;

// Playlist: cabeceras de pista (izquierda, tamaño fijo), regla de compases, cuadrícula con clips y cabezal.
class PlaylistView : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    static constexpr int kHeaderW = 150, kRulerH = 40, kGap = 2;
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
    int edgeAt (const juce::Point<int>& p) const { if (p.x >= kHeaderW + 6 || p.y < kRulerH) return -1; for (int i = 0; i < numTracks(); ++i) { const int b = trackTop (i) + hOf (i); if (p.y >= b - 4 && p.y <= b + kGap + 1) return i; } return -1; }
    int trackH = 116;   // alto de pista; Alt+rueda lo cambia (60-220)
    juce::String tool = "select"; bool snap = true;

    explicit PlaylistView (te::Edit& e) : edit (e), thumbCache (32)
    {
        fm.registerBasicFormats(); setWantsKeyboardFocus (true); setOpaque (true); startTimerHz (30);
    }

    //==============================================================================
    double beatsToTime (double b) const { return edit.tempoSequence.toTime (te::BeatPosition::fromBeats (b)).inSeconds(); }
    double timeToBeats (double t) const { return edit.tempoSequence.toBeats (te::TimePosition::fromSeconds (t)).inBeats(); }
    // tijeras: línea guía donde se va a cortar y destello al cortar
    float cutX = -1.f; int cutTrack = -1, flashTrack = -1; double cutBeat = -1, flashBeat = -1; float flashA = 0.f;
    juce::Point<float> chevronCentre (int i) const { return chevronR (i).getCentre(); }
    bool folded (int i) const { return isFolded (i); }
    std::function<juce::File()> samplesRoot; std::function<void (int, juce::String)> onInstrument;
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
    int trackTop (int i) const { ensureLayout(); const int n = (int) tops.size() - 1; return kRulerH + (i <= n ? tops[(size_t) std::max (0, i)] : tops[(size_t) n] + (i - n) * (trackH + kGap)) - (int) scrollY; }
    // Cuadrícula adaptable: el paso es el más fino que se ve separado (>= 12 px); con el zoom por defecto es el compás
    double snapStep() const { for (double st : { 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 }) if (st * pxPerBeat >= 12.0) return st; return 32.0; }
    double snapBeat (double b) const { if (! snap) return b; const double st = snapStep(); return std::round (b / st) * st; }
    int numTracks() const { return te::getAudioTracks (edit).size(); }
    int trackAt (float y) const { for (int i = 0; i < numTracks(); ++i) if (y >= trackTop (i) && y < trackTop (i) + hOf (i)) return i; return -1; }

    //==============================================================================
    void paint (juce::Graphics& g) override
    {
        using namespace theme;
        const int W = getWidth(), H = getHeight();
        g.fillAll (bgBar);
        auto tracks = te::getAudioTracks (edit);

        // fondo de pistas + líneas de compás
        g.saveState(); g.reduceClipRegion (kHeaderW + 6, kRulerH, W, H - kRulerH);
        for (int i = 0; i < tracks.size(); ++i)
        {
            auto row = juce::Rectangle<int> (kHeaderW + 6, trackTop (i), W, hOf (i));
            g.setColour (juce::Colour (0xff32353e)); g.fillRect (row);
            g.setColour (juce::Colour (0xff1f2128)); g.fillRect (row.getX(), row.getBottom(), W, kGap);
        }
        const double b0 = std::floor (xToBeat ((float) kHeaderW)), b1 = xToBeat ((float) W) + 1;
        const bool showBeats = pxPerBeat >= 14;
        for (double b = std::max (0.0, b0); b <= b1; b += 1.0)
        {
            const bool bar = std::fmod (b, timesig::bar()) == 0.0;
            if (! bar && ! showBeats) continue;
            g.setColour (bar ? barLine : gridLine.withAlpha (.55f));
            g.fillRect (beatToX (b), (float) kRulerH, 1.f, (float) (H - kRulerH));
        }
        // clips
        auto visibleRow = [&] (int i) { const int t = trackTop (i); return t + hOf (i) >= kRulerH && t <= H; };   // solo se dibuja lo que se ve
        for (int i = 0; i < tracks.size(); ++i)
        {
            if (! visibleRow (i)) { if (trackTop (i) > H) break; continue; }
            for (auto* c : tracks[i]->getClips()) drawClip (g, *c, i);
        }
        for (int i = 0; i < tracks.size(); ++i) { if (! visibleRow (i)) { if (trackTop (i) > H) break; continue; } if (autoShown (*tracks[i])) drawAuto (g, i); }
        g.restoreState();

        drawRuler (g, W);
        for (int i = 0; i < tracks.size(); ++i) { if (! visibleRow (i)) { if (trackTop (i) > H) break; continue; } drawHeader (g, *tracks[i], i); }
        { auto ar = addR(); if (ar.getBottom() > kRulerH && ar.getY() < H) { g.setColour (juce::Colour (0xff1d1c22)); g.fillRoundedRectangle (ar, 6.f); g.setColour (juce::Colour (0xff34323a)); g.drawRoundedRectangle (ar.reduced (.5f), 6.f, 1.f);
            icons::draw (g, icons::Plus, { ar.getX() + 6, ar.getY(), 38.f, ar.getHeight() }, text); g.setColour (textDim); g.setFont (juce::FontOptions (14.f)); g.drawText ("Agregar pista", ar.withTrimmedLeft (46).toNearestInt(), juce::Justification::centredLeft); } }
        if (marquee && ! mqRect.isEmpty()) { g.setColour (juce::Colours::white); g.drawRect (mqRect, 1.5f); }
        drawCut (g);
        drawScrollbars (g);

        // cabezal
        const float px = beatToX (timeToBeats (edit.getTransport().getPosition().inSeconds()));
        if (magnetBeat >= 0 && dragMode != 0)
        {
            const float mx = beatToX (magnetBeat); if (mx >= kHeaderW + 6 && mx <= W) { g.setColour (theme::accentHi.withAlpha (magnetHit ? .95f : .35f)); g.fillRect (mx - (magnetHit ? 1.f : .5f), (float) kRulerH, magnetHit ? 2.f : 1.f, (float) (H - kRulerH)); }
        }
        if (px >= kHeaderW + 6 && px <= W)
        {
            g.setColour (juce::Colours::white.withAlpha (.75f)); g.fillRect (px - .5f, (float) kRulerH - 6, 1.f, (float) (H - kRulerH + 6));
            g.setColour (accentHi); g.fillEllipse (px - 11.f, kRulerH - 26.f, 22.f, 22.f);
            g.setColour (juce::Colours::white); g.setFont (juce::FontOptions (13.f, juce::Font::bold));
            g.drawText (juce::String ((int) (timeToBeats (edit.getTransport().getPosition().inSeconds()) / timesig::bar()) + 1), juce::Rectangle<int> ((int) px - 11, kRulerH - 26, 22, 22), juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus(); sbDrag = 0; edit.getUndoManager().beginNewTransaction();   // cada gesto = un solo "deshacer"
        if (tool == "auto" && e.x >= kHeaderW + 6 && e.y >= kRulerH && ! (needHScroll() && hScrollR().expanded (0, 4).contains (e.position)) && ! vScrollR().expanded (4, 0).contains (e.position)) { autoDown (e); return; }
        if (e.mods.isPopupMenu()) { contextMenu (e); return; }
        if ((needHScroll() && hScrollR().expanded (0, 4).contains (e.position))) { sbDrag = 1; sbGrab = e.x - hThumb().getX(); if (! hThumb().contains (e.position)) sbGrab = hThumb().getWidth() * .5f; dragScroll (e); return; }
        if (vScrollR().expanded (4, 0).contains (e.position)) { sbDrag = 2; sbGrab = e.y - vThumb().getY(); if (! vThumb().contains (e.position)) sbGrab = vThumb().getHeight() * .5f; dragScroll (e); return; }
        scrubbing = false; brushSrc = nullptr; brushTrack = -1; lastDT = 0;
        if (e.y < kRulerH && e.x >= kHeaderW) { scrubbing = true; seekTo (e.x); return; }
        dragClip = nullptr; dragMode = 0;
        if (const int ed = edgeAt (e.getPosition()); ed >= 0) { resizing = ed; resizeStartY = e.position.y; resizeStartH = hOf (ed); return; }
        if (e.x < kHeaderW + 6) { headerClick (e); return; }
        const int ti = trackAt ((float) e.y); if (ti < 0) { selected = nullptr; repaint(); return; }
        auto tr = te::getAudioTracks (edit)[ti];
        const double t = beatsToTime (xToBeat ((float) e.x));
        te::Clip* hit = nullptr;
        for (auto* c : tr->getClips()) if (c->getPosition().time.contains (te::TimePosition::fromSeconds (t))) hit = c;
        if (tool == "cut" && hit != nullptr) { const double cb = snapBeat (xToBeat ((float) e.x)); tr->splitAt (te::TimePosition::fromSeconds (beatsToTime (cb))); flashBeat = cb; flashTrack = ti; flashA = 1.f; cutTrack = -1; repaint(); return; }
        if (tool == "brush")
        {   // pinta copias del clip bajo el cursor; si empiezas en vacío usa el clip seleccionado o el último que pintaste
            auto alive = [] (te::Clip* c) { return c != nullptr && c->getTrack() != nullptr; };
            if (alive (selected)) lastTouched = te::Clip::Ptr (selected);
            brushSrc = hit != nullptr ? hit : alive (lastTouched.get()) ? lastTouched.get() : nullptr;
            if (brushSrc != nullptr) lastTouched = te::Clip::Ptr (brushSrc);
            brushTrack = ti; if (hit != nullptr) selected = hit; brushPaint (ti, xToBeat ((float) e.x)); repaint(); return;
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
        selected = hit; dragClip = hit;
        if (hit)
        {
            const float x0 = beatToX (timeToBeats (hit->getPosition().getStart().inSeconds())), x1 = beatToX (timeToBeats (hit->getPosition().getEnd().inSeconds()));
            const float zone = tool == "trim" ? 16.f : juce::jmin (7.f, (x1 - x0) * .3f);
            if (e.x >= x1 - zone) dragMode = 2; else if (e.x <= x0 + zone) dragMode = 1; else if (tool == "trim") dragClip = nullptr;
            origStartBeat = timeToBeats (hit->getPosition().getStart().inSeconds()); origEndBeat = timeToBeats (hit->getPosition().getEnd().inSeconds()); magnetBeat = -1; magnetHit = false;
        }
        if (hit) { dragOffsetBeats = xToBeat ((float) e.x) - timeToBeats (hit->getPosition().getStart().inSeconds()); }
        dragOrig.clear(); downTrack = ti; auto allT = te::getAudioTracks (edit);
        if (hit && dragMode == 0 && tool == "select") for (auto* c : group) { if (c->getTrack() == nullptr) continue; int idx = 0; for (int k = 0; k < allT.size(); ++k) if (allT[k] == c->getTrack()) idx = k; dragOrig.push_back ({ c, timeToBeats (c->getPosition().getStart().inSeconds()), idx }); }
        repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int ti = trackAt ((float) e.y);
        if (ti >= 0 && e.x < kHeaderW + 6 && ! dotsR (ti).contains (e.position) && ! chevronR (ti).expanded (4.f).contains (e.position)) { auto* tk = te::getAudioTracks (edit)[ti]; tk->setMute (! tk->isMuted (false)); repaint(); return; }
        if (ti < 0 || e.x < kHeaderW + 6) return;
        const double t = beatsToTime (xToBeat ((float) e.x));
        for (auto* c : te::getAudioTracks (edit)[ti]->getClips())
            if (auto* mc = dynamic_cast<te::MidiClip*> (c)) if (mc->getPosition().time.contains (te::TimePosition::fromSeconds (t))) { if (onOpenMidi) onOpenMidi (mc); return; }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
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
        if (dragClip == nullptr || (tool != "select" && tool != "trim")) return;
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
    void mouseUp (const juce::MouseEvent&) override { magnetBeat = -1; magnetHit = false; autoIdx = -1; sbDrag = 0; dragMode = 0; resizing = -1; scrubbing = false; brushTrack = -1; brushSrc = nullptr; marquee = false; dragOrig.clear(); lastDT = 0; repaint(); }
    // Pincel: sobre un hueco pinta clips MIDI de 1 compás; sobre un clip, lo copia a lo largo del arrastre (sin pisar otros clips).
    void brushPaint (int ti, double beat)
    {
        auto tracks = te::getAudioTracks (edit); if (ti < 0 || ti >= tracks.size()) return; auto* tr = tracks[ti];
        if (brushSrc != nullptr && brushSrc->getTrack() == nullptr) brushSrc = nullptr;
        double s0 = 0.0, len = 4.0;
        if (brushSrc != nullptr) { s0 = timeToBeats (brushSrc->getPosition().getStart().inSeconds()); len = timeToBeats (brushSrc->getPosition().getEnd().inSeconds()) - s0; if (len <= 0.0) return; }
        const double pos = s0 + std::floor ((beat - s0) / len) * len; if (pos < -1e-6) return;
        const auto a = te::TimePosition::fromSeconds (beatsToTime (pos)), b = te::TimePosition::fromSeconds (beatsToTime (pos + len));
        for (auto* c : tr->getClips()) if (c->getPosition().getStart() < b - te::TimeDuration::fromSeconds (1e-4) && c->getPosition().getEnd() > a + te::TimeDuration::fromSeconds (1e-4)) return;
        te::Clip* nc = brushSrc != nullptr ? trackutil::copyClipTo (edit, *brushSrc, *tr, a) : tr->insertMIDIClip ("MIDI", te::TimeRange (a, b), nullptr).get();
        if (nc != nullptr) { selected = nc; if (onTracksChanged) onTracksChanged(); repaint(); }
    }
    void updateCut (const juce::MouseEvent& e)
    {
        int ti = -1; double b = -1.0;
        if (tool == "cut" && e.x >= kHeaderW + 6 && e.y >= kRulerH)
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
        if (cutTrack >= 0 && tool == "cut") line (cutTrack, cutX >= 0.f ? cutX : beatToX (cutBeat), .95f, 1.4f, blue);
        if (flashA > 0.f) line (flashTrack, beatToX (flashBeat), flashA, 1.4f + 5.f * (1.f - flashA), blue);
    }
    void mouseExit (const juce::MouseEvent&) override { if (cutTrack >= 0) { cutTrack = -1; cutX = -1.f; repaint(); } }
    void mouseMove (const juce::MouseEvent& e) override
    {
        auto cur = juce::MouseCursor::NormalCursor;
        updateCut (e); if (tool == "cut" && e.x >= kHeaderW + 6 && e.y >= kRulerH) { setMouseCursor (cursors::forTool ("cut")); return; }
        if ((tool == "brush" || tool == "pencil") && e.x >= kHeaderW + 6 && e.y >= kRulerH && ! (needHScroll() && hScrollR().expanded (0, 4).contains (e.position))) { setMouseCursor (cursors::forTool (tool)); return; }
        if (edgeAt (e.getPosition()) >= 0) { setMouseCursor (juce::MouseCursor::UpDownResizeCursor); return; }
        if (e.x >= kHeaderW + 6 && e.y >= kRulerH && (tool == "select" || tool == "trim"))
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
        if (e.mods.isAltDown()) { setAllHeights ((int) std::round (trackH * (w.deltaY > 0 ? 1.15 : 1.0 / 1.15))); scrollY = juce::jlimit (0.0, juce::jmax (0.0, contentH() - (getHeight() - kRulerH)), scrollY); }
        else if (e.mods.isCtrlDown() || e.mods.isCommandDown() || (e.y < kRulerH && e.x >= kHeaderW))   // rueda sobre la regla = zoom
        {
            zoomAt ((float) e.x, w.deltaY);
        }
        else if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY)) scrollX = juce::jmax (0.0, scrollX - (w.deltaX != 0 ? w.deltaX : w.deltaY) * 600.0);
        else scrollY = juce::jlimit (0.0, juce::jmax (0.0, contentH() - (getHeight() - kRulerH)), scrollY - w.deltaY * 400.0);
        repaint();
    }
    bool keyPressed (const juce::KeyPress& k) override
    {
        auto lower = [&] { return juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) k.getKeyCode()); };
        if (k.getModifiers().isCommandDown() && lower() == 'm') { addMarker (snapBeat (timeToBeats (edit.getTransport().getPosition().inSeconds()))); return true; }
        if (k.getModifiers().isCommandDown() && k.getKeyCode() == juce::KeyPress::rightKey) { jumpToMarker (1); return true; }
        if (k.getModifiers().isCommandDown() && k.getKeyCode() == juce::KeyPress::leftKey) { jumpToMarker (-1); return true; }
        if (k.getModifiers().isCommandDown() && lower() == 'c' && (selected != nullptr || group.size() > 0)) { copySelection(); return true; }
        if (k.getModifiers().isCommandDown() && lower() == 'v' && ! clipboard.empty()) { pasteAtPlayhead(); return true; }
        if (k.getModifiers().isCommandDown() && lower() == 'x' && (selected != nullptr || group.size() > 0)) { copySelection(); return keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)); }
        if (k.getModifiers().isCommandDown() && juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) k.getKeyCode()) == 'a')   // Ctrl+A: elegir todos los clips
        { group.clear(); for (auto* t : te::getAudioTracks (edit)) for (auto* c : t->getClips()) group.add (c); selected = group.size() == 1 ? group.getUnchecked (0).get() : nullptr; repaint(); return true; }
        if (k == juce::KeyPress::escapeKey && (selected != nullptr || group.size() > 0)) { group.clear(); selected = nullptr; repaint(); return true; }
        if (k.getModifiers().isCommandDown() && juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) k.getKeyCode()) == 'd' && (selected != nullptr || group.size() > 0)) { if (group.size() > 1) { juce::ReferenceCountedArray<te::Clip> nw; for (auto* c : group) if (auto* nc = trackutil::duplicateClip (edit, *c)) nw.add (nc); group = nw; selected = nullptr; } else if (auto* nc = trackutil::duplicateClip (edit, *selected)) { selected = nc; group.clear(); group.add (nc); } repaint(); return true; }
        if ((k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) && (selected != nullptr || group.size() > 0)) { if (group.size() > 1) { for (auto* c : group) c->removeFromParent(); } else if (selected) selected->removeFromParent(); group.clear(); selected = nullptr; dragClip = nullptr; repaint(); return true; }
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

    te::Clip* selected = nullptr; std::function<void (te::MidiClip*)> onOpenMidi; std::function<void()> onTracksChanged; std::function<void (int)> onTrackSelected;

private:
public:
    double applyMagnet (double nb, double orig) { magnetBeat = orig; magnetHit = std::abs (beatToX (nb) - beatToX (orig)) <= kMagnetPx; return magnetHit ? orig : nb; }
    void zoomAt (float x, double deltaY) { const double anchor = xToBeat (x); pxPerBeat = juce::jlimit (1.5, 120.0, pxPerBeat * (1.0 + deltaY * 0.6)); scrollX = juce::jmax (0.0, anchor * pxPerBeat - (x - kHeaderW - 6)); repaint(); }
    double zoomPx() const { return pxPerBeat; }
private:
    static constexpr float kMagnetPx = 9.f; double origStartBeat = 0, origEndBeat = 0, magnetBeat = -1; bool magnetHit = false;
    int dragMode = 0, sbDrag = 0; float sbGrab = 0;   // dragMode: 0 mover, 1 recortar inicio, 2 recortar final
    struct Orig { te::Clip::Ptr clip; double beat; int track; };
    juce::ReferenceCountedArray<te::Clip> group; std::vector<Orig> dragOrig; bool marquee = false; juce::Point<float> mqStart; juce::Rectangle<float> mqRect; int downTrack = 0, lastDT = 0;
    bool inGroup (te::Clip* c) const { return group.contains (c); }
    bool scrubbing = false; te::Clip* brushSrc = nullptr; te::Clip::Ptr lastTouched; int brushTrack = -1;
    te::Edit& edit; double pxPerBeat = 3.6, scrollX = 0, scrollY = 0, dragOffsetBeats = 0; te::Clip* dragClip = nullptr;
    juce::AudioFormatManager fm; juce::AudioThumbnailCache thumbCache; std::map<juce::String, std::unique_ptr<juce::AudioThumbnail>> thumbs;


    //---- barras de desplazamiento
    double contentBeats() const { double e = 64; for (auto* t : te::getAudioTracks (edit)) for (auto* c : t->getClips()) e = std::max (e, timeToBeats (c->getPosition().getEnd().inSeconds())); return e + 32; }
    double contentH() const { ensureLayout(); return (double) tops.back() + trackH + kGap + 20; }
    juce::Rectangle<float> hScrollR() const { return { (float) kHeaderW + 6, (float) getHeight() - 18.f, (float) getWidth() - kHeaderW - 22.f, 12.f }; }
    bool needHScroll() const { return true; }   // la barra de abajo siempre está
    juce::Rectangle<float> vScrollR() const { return { (float) getWidth() - 11.f, (float) kRulerH, 8.f, (float) getHeight() - kRulerH - 14.f }; }
    juce::Rectangle<float> hThumb() const { auto r = hScrollR(); const double tot = contentBeats() * pxPerBeat, vis = getWidth() - kHeaderW - 6; const float w = (float) juce::jlimit (30.0, (double) r.getWidth(), r.getWidth() * vis / tot); return { r.getX() + (float) ((r.getWidth() - w) * juce::jlimit (0.0, 1.0, scrollX / std::max (1.0, tot - vis))), r.getY(), w, r.getHeight() }; }
    juce::Rectangle<float> vThumb() const { auto r = vScrollR(); const double tot = contentH(), vis = getHeight() - kRulerH; const float h = (float) juce::jlimit (30.0, (double) r.getHeight(), r.getHeight() * vis / tot); return { r.getX(), r.getY() + (float) ((r.getHeight() - h) * juce::jlimit (0.0, 1.0, scrollY / std::max (1.0, tot - vis))), r.getWidth(), h }; }
    void dragScroll (const juce::MouseEvent& e)
    {
        if (sbDrag == 1) { auto r = hScrollR(); const double tot = contentBeats() * pxPerBeat, vis = getWidth() - kHeaderW - 6; const float tw = hThumb().getWidth(); scrollX = juce::jlimit (0.0, std::max (0.0, tot - vis), (double) ((e.x - sbGrab - r.getX()) / std::max (1.f, r.getWidth() - tw)) * (tot - vis)); }
        else { auto r = vScrollR(); const double tot = contentH(), vis = getHeight() - kRulerH; const float th = vThumb().getHeight(); scrollY = juce::jlimit (0.0, std::max (0.0, tot - vis), (double) ((e.y - sbGrab - r.getY()) / std::max (1.f, r.getHeight() - th)) * (tot - vis)); }
        repaint();
    }
    void drawScrollbars (juce::Graphics& g)
    {
        const bool needH = true, needV = contentH() > getHeight() - kRulerH;
        g.setColour (juce::Colours::white.withAlpha (.07f));
        if (needH)
        {   // vista general: miniatura de todo el proyecto; el recuadro es lo que se ve ahora
            auto r = hScrollR(); g.setColour (juce::Colour (0xe017171f)); g.fillRoundedRectangle (r, 5.f);
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
            auto th = hThumb(); g.setColour (theme::accent.withAlpha (.28f)); g.fillRoundedRectangle (th, 5.f); g.setColour (theme::accentHi.withAlpha (.9f)); g.drawRoundedRectangle (th.reduced (.5f), 5.f, 1.2f);
        }
        g.setColour (juce::Colours::white.withAlpha (.07f));
        if (needV) { g.fillRoundedRectangle (vScrollR(), 4.f); g.setColour (theme::accent.withAlpha (.75f)); g.fillRoundedRectangle (vThumb(), 4.f); }
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
    juce::Rectangle<float> dotsR (int i) const { auto r = headerR (i); const float h = (float) r.getHeight(); const float s = std::min (26.f, h - 2.f); return { (float) r.getRight() - 32.f, (float) r.getY() + (h >= 40.f ? 6.f : (h - s) * .5f), 26.f, s }; }
    juce::Rectangle<float> addR() const { return { 6.f, (float) trackTop (numTracks()) + 6.f, (float) kHeaderW - 6.f, 42.f }; }
    void headerClick (const juce::MouseEvent& e)
    {
        auto tracks = te::getAudioTracks (edit); const auto p = e.position;
        if (addR().contains (p)) { if (trackutil::addTrack (edit) != nullptr) { layoutDirty = true; if (onTracksChanged) onTracksChanged(); if (onTrackSelected) onTrackSelected (numTracks() - 1); } scrollY = juce::jmax (0.0, contentH() - (getHeight() - kRulerH)); repaint(); return; }
        for (int i = 0; i < tracks.size(); ++i)
        {
            if (! headerR (i).contains (e.getPosition())) continue;
            if (onTrackSelected) onTrackSelected (i);
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
        static const char* cn[] { "Violeta", "Magenta", "Rosa", "Azul", "Cian", "Verde", "Naranja", "Amarillo" };
        for (int c = 0; c < 8; ++c) col.addColouredItem (10 + c, cn[c], theme::trackColours[c], true, false, nullptr);
        m.addSubMenu ("Color", col);
        { juce::PopupMenu hm; hm.addItem (20, "Pequeñas (todas)"); hm.addItem (21, "Medianas (todas)"); hm.addItem (22, "Grandes (todas)"); hm.addItem (23, "Esta pista: pequeña"); hm.addItem (24, "Esta pista: grande"); m.addSubMenu ("Altura de pistas", hm); }
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
        m.addSeparator();
        m.addItem (4, "Agregar pista al final"); m.addItem (5, "Eliminar pista", tracks.size() > 1);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (dotsR (i).toNearestInt())), [this, t, i] (int r)
        {
            if (r == 1) trackutil::renameDialog (edit, t.get(), [this] { if (onTracksChanged) onTracksChanged(); repaint(); });
            else if (r == 2) t->setMute (! t->isMuted (false));
            else if (r == 3) t->setSolo (! t->isSolo (false));
            else if (r == 20) setAllHeights (34); else if (r == 21) setAllHeights (76); else if (r == 22) setAllHeights (116);
            else if (r == 23) setTrackHeight (i, 34); else if (r == 24) setTrackHeight (i, 116);
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
private:
    std::unique_ptr<juce::AlertWindow> markerDlg;
    void rulerMenu (const juce::MouseEvent& e)
    {
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
        if (e.y < kRulerH && e.x >= kHeaderW) { rulerMenu (e); return; }
        if (e.x < kHeaderW + 6) { const int ti = trackAt ((float) e.y); if (ti >= 0) trackMenu (ti); return; }
        const int ti = trackAt ((float) e.y); if (ti < 0) return;
        auto tr = te::getAudioTracks (edit)[ti]; const double t = beatsToTime (xToBeat ((float) e.x)); te::Clip* hit = nullptr;
        for (auto* c : tr->getClips()) if (c->getPosition().time.contains (te::TimePosition::fromSeconds (t))) hit = c;
        if (hit != nullptr && ! e.mods.isShiftDown())
        {   // clic derecho sobre un clip = borrarlo (si es parte de una selección, borra toda la selección). Mayús + clic derecho abre el menú del clip.
            if (group.size() > 1 && group.contains (hit)) { for (auto* c : group) c->removeFromParent(); } else hit->removeFromParent();
            group.clear(); selected = nullptr; dragClip = nullptr; layoutDirty = true; if (onTracksChanged) onTracksChanged(); repaint(); return;
        }
        juce::PopupMenu m; const double clickBeat = snapBeat (xToBeat ((float) e.x));
        if (hit != nullptr)
        {
            selected = hit; te::Clip::Ptr c (hit);
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
            if (dynamic_cast<te::MidiClip*> (hit)) m.addItem (8, juce::String::fromUTF8 ("Exportar como archivo MIDI…"));
            m.addItem (5, juce::String::fromUTF8 ("Renombrar…")); m.addItem (6, hit->isMuted() ? "Quitar silencio del clip" : "Silenciar clip"); m.addSeparator(); m.addItem (2, "Duplicar  (Ctrl+D)"); m.addItem (3, "Dividir aqu\xc3\xad"); m.addSeparator(); m.addItem (4, "Eliminar  (Supr)");
            m.showMenuAsync (juce::PopupMenu::Options(), [this, c, tr, clickBeat] (int r)
            {
                if (r == 1) { if (onOpenMidi) onOpenMidi (dynamic_cast<te::MidiClip*> (c.get())); }
                else if (r == 2) { if (auto* nc = trackutil::duplicateClip (edit, *c)) selected = nc; }
                else if (r == 3) tr->splitAt (te::TimePosition::fromSeconds (beatsToTime (clickBeat)));
                else if (r == 4) { if (selected == c.get()) selected = nullptr; dragClip = nullptr; c->removeFromParent(); }
                else if (r >= 200 && r < 205) { static const double fd[] { 0.0, 0.01, 0.05, 0.25, 1.0 }; if (auto* wc = dynamic_cast<te::WaveAudioClip*> (c.get())) { edit.getUndoManager().beginNewTransaction(); wc->setFadeIn (te::TimeDuration::fromSeconds (fd[r - 200])); } }
                else if (r >= 210 && r < 215) { static const double fd[] { 0.0, 0.01, 0.05, 0.25, 1.0 }; if (auto* wc = dynamic_cast<te::WaveAudioClip*> (c.get())) { edit.getUndoManager().beginNewTransaction(); wc->setFadeOut (te::TimeDuration::fromSeconds (fd[r - 210])); } }
                else if (r >= 400 && r < 413) { static const int st[] { -12, -7, -5, -3, -2, -1, 0, 1, 2, 3, 5, 7, 12 }; if (auto* wc = dynamic_cast<te::WaveAudioClip*> (c.get())) { edit.getUndoManager().beginNewTransaction(); setClipPitch (*wc, (float) st[r - 400]); } }
                else if (r >= 430 && r < 438) { static const int sr[] { 50, 75, 90, 100, 110, 125, 150, 200 }; if (auto* wc = dynamic_cast<te::WaveAudioClip*> (c.get())) { edit.getUndoManager().beginNewTransaction(); setClipSpeed (*wc, sr[r - 430] / 100.0); } }
                else if (r == 450 || r == 451) { if (auto* wc = dynamic_cast<te::WaveAudioClip*> (c.get())) { edit.getUndoManager().beginNewTransaction(); wc->setTimeStretchMode (r == 451 ? te::TimeStretcher::rubberbandPercussive : te::TimeStretcher::rubberbandMelodic); } }
                else if (r == 8) { if (auto* mc = dynamic_cast<te::MidiClip*> (c.get())) exportMidiDialog (*mc); }
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
            m.addItem (1, "Crear clip MIDI aqu\xc3\xad");
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
        if (flashA > 0.f) { flashA = std::max (0.f, flashA - .09f); repaint(); }
        if (cutTrack >= 0 && tool == "cut")   // la línea azul de las tijeras se desliza hasta el punto de corte
        {
            const float tx = beatToX (cutBeat); if (cutX < 0.f) cutX = tx; else if (std::abs (tx - cutX) > .4f) { cutX += (tx - cutX) * .5f; repaint(); } else cutX = tx;
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
        g.setColour (bgBar); g.fillRect (kHeaderW + 6, 0, W, kRulerH);
        g.setColour (juce::Colours::white.withAlpha (.07f)); g.fillRect (kHeaderW + 6, 0, W, 4);
        g.setFont (juce::FontOptions (14.f, juce::Font::bold)); g.setColour (text);
        const int stride = pxPerBeat * 4 >= 64 ? 1 : (pxPerBeat * 16 >= 64 ? 4 : 8);
        for (int bar = 0; ; bar += 1)
        {
            const float x = beatToX (bar * timesig::bar()); if (x > W) break;
            if (x < kHeaderW + 10 || (bar % stride) != (stride == 1 ? 0 : 0)) { if (x < kHeaderW + 10 || bar % stride != 0) continue; }
            g.drawText (juce::String (bar + 1), juce::Rectangle<float> (x - 20, 18, 40, 20).toNearestInt(), juce::Justification::centred);
        }
        g.saveState(); g.reduceClipRegion (kHeaderW + 6, 0, W, kRulerH);
        for (auto& mk : markers())
        {
            const float x = beatToX (mk.beat); if (x < kHeaderW - 20 || x > W) continue;
            g.setColour (pink.withAlpha (.5f)); g.fillRect (x, (float) kRulerH - 14.f, 1.f, (float) getHeight());
            const float w = 10.f + 7.f * (float) std::min (14, mk.name.length()); g.setColour (pink); g.fillRoundedRectangle (x, 5.f, w, 14.f, 3.f);
            g.setColour (juce::Colour (0xff1a1522)); g.setFont (juce::FontOptions (11.f, juce::Font::bold)); g.drawText (mk.name, juce::Rectangle<float> (x + 4.f, 5.f, w - 6.f, 14.f), juce::Justification::centredLeft, true);
        }
        g.restoreState();
    }

    void drawHeader (juce::Graphics& g, te::AudioTrack& t, int i)
    {
        using namespace theme;
        auto col = trackutil::colourOf (&t, i); auto r = headerR (i);
        if (r.getBottom() < kRulerH || r.getY() > getHeight()) return;
        g.saveState(); g.reduceClipRegion (0, kRulerH, kHeaderW + 6, getHeight() - kRulerH);
        g.setColour (col); g.fillRect (r);
        const auto fg = juce::Colour (0xff1a1522);
        { const float ry = r.getHeight() >= 40 ? 7.f : (r.getHeight() - 15.f) * .5f; auto c = juce::Rectangle<float> ((float) r.getX() + 8.f, (float) r.getY() + ry, 15.f, 15.f); g.setColour (fg.withAlpha (.75f)); g.drawEllipse (c, 1.3f);
          juce::Path tri; tri.addTriangle (c.getX() + 4.f, c.getY() + 5.5f, c.getX() + 11.f, c.getY() + 5.5f, c.getX() + 7.5f, c.getY() + 10.5f); if (isFolded (i)) tri.applyTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, c.getCentreX(), c.getCentreY())); g.fillPath (tri); }
        g.setColour (fg); g.setFont (juce::FontOptions (13.f));
        g.drawText (t.getName() == "Track" ? juce::String::fromUTF8 ("Pista ") + juce::String (i + 1) : t.getName(), r.getX() + 29, r.getHeight() >= 40 ? r.getY() + 4 : r.getY() + (r.getHeight() - 20) / 2, r.getWidth() - 62, 20, juce::Justification::centredLeft);
        if (r.getHeight() >= 26) icons::draw (g, icons::Dots, dotsR (i), fg);
        juce::String inst; for (auto* p : t.pluginList.getPlugins()) if (dynamic_cast<te::ExternalPlugin*> (p) || dynamic_cast<te::SamplerPlugin*> (p) || dynamic_cast<te::FourOscPlugin*> (p)) { inst = p->state.getProperty ("injPreset", p->getName()).toString(); break; }
        g.setColour (fg.withAlpha (.8f)); g.setFont (juce::FontOptions (13.f));
        if (r.getHeight() >= 60 && inst.isNotEmpty()) g.drawText (inst, r.getX() + 29, r.getY() + 24, r.getWidth() - 36, 18, juce::Justification::centredLeft);
        if (r.getHeight() >= 60 && (t.isMuted (false) || t.isSolo (false))) { g.setColour (fg.withAlpha (.85f)); g.setFont (juce::FontOptions (12.f, juce::Font::bold)); g.drawText (t.isSolo (false) ? "SOLO" : juce::String::fromUTF8 ("SILENCIADA"), r.getX() + 10, r.getBottom() - 26, r.getWidth() - 16, 20, juce::Justification::centredLeft); }
        { const auto gid = trackutil::groupOf (t);
          if (gid.isNotEmpty())
          {
              const auto gc = theme::trackColours[trackutil::groupColour (gid)]; auto all = te::getAudioTracks (edit);
              const bool first = i == 0 || trackutil::groupOf (*all[i - 1]) != gid, last = i + 1 >= all.size() || trackutil::groupOf (*all[i + 1]) != gid;
              g.setColour (juce::Colour (0xff1a1522)); g.fillRect (r.getX(), r.getY(), 7, r.getHeight()); g.setColour (gc); g.fillRect (r.getX() + 1, r.getY() + (first ? 2 : 0), 5, r.getHeight() - (first ? 2 : 0) - (last ? 2 : 0));
              if (first && r.getHeight() >= 40) { g.setColour (juce::Colour (0xff1a1522)); g.setFont (juce::FontOptions (11.f, juce::Font::bold)); g.drawText (trackutil::groupName (t), r.getX() + 29, r.getBottom() - 20, r.getWidth() - 40, 16, juce::Justification::centredLeft, true); }
          } }
        if (t.isMuted (false)) { g.setColour (juce::Colour (0xff32353e).withAlpha (.6f)); g.fillRect (r); }
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
        for (int i = 0; i < n; ++i) { const float x = beatToX (timeToBeats (cv.getPointTime (i).inSeconds())), y = yOfValue (cv.getPointValue (i)); g.setColour (juce::Colour (0xff15151d)); g.fillEllipse (x - 6, y - 6, 12, 12); g.setColour (i == autoIdx && ti == autoTrack ? juce::Colours::white : theme::pink); g.fillEllipse (x - 4, y - 4, 8, 8); }
        if (autoIdx >= 0 && ti == autoTrack && autoIdx < n)
        {
            const float x = beatToX (timeToBeats (cv.getPointTime (autoIdx).inSeconds())), y = yOfValue (cv.getPointValue (autoIdx)); const float db = te::volumeFaderPositionToDB (cv.getPointValue (autoIdx)); const bool isVol = autoIsVolume (*tk); const juce::String lab = isVol ? (db <= -99.f ? juce::String ("-inf dB") : juce::String (db, 1) + " dB") : p->valueToString (cv.getPointValue (autoIdx));
            g.setColour (juce::Colour (0xe01b1b25)); g.fillRoundedRectangle (x + 10, y - 24, 62, 20, 5.f); g.setColour (theme::text); g.setFont (juce::FontOptions (12.f)); g.drawText (lab, juce::Rectangle<float> (x + 10, y - 24, 62, 20).toNearestInt(), juce::Justification::centred);
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
        const auto ink = inkOn (col);
        juce::Path shape; shape.addRectangle (r);
        g.setColour (col); g.fillPath (shape);
        { g.saveState(); g.reduceClipRegion (shape, {}); g.setColour (juce::Colours::white.withAlpha (.17f));
          const double gs = pxPerBeat >= 14 ? 1.0 : timesig::bar(); for (double b = std::floor (xToBeat (r.getX()) / gs) * gs; beatToX (b) < r.getRight(); b += gs) g.fillRect (beatToX (b), r.getY(), 1.f, r.getHeight()); g.restoreState(); }
        g.setColour (juce::Colour (0xff15151d)); g.drawRect (r, 1.5f);
        if (&c == selected || inGroup (&c)) { g.setColour (juce::Colours::white); g.drawRect (r, 2.f); }
        { const float bandH = r.getHeight() >= 60.f ? 16.f : (r.getHeight() >= 40.f ? 8.f : 0.f); if (bandH > 0.f) { g.setColour (juce::Colour (0xff15151d)); g.fillRect (r.getX(), r.getY() + bandH, r.getWidth(), 1.5f); } }
        if (&c == selected && r.getHeight() >= 26.f) { g.setColour (ink.withAlpha (.8f)); g.setFont (juce::FontOptions (11.f, juce::Font::bold));
        g.drawText (c.getName(), r.toNearestInt().reduced (5, 1).withHeight (15), juce::Justification::centredLeft); }
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
                const float band = r.getHeight() >= 60.f ? 16.f : (r.getHeight() >= 40.f ? 8.f : 0.f), pad = r.getHeight() >= 60.f ? 8.f : 3.f, area = r.getHeight() - band - 2.f * pad, rows = (float) (hi - lo + 1), step = area / std::max (rows, 4.f), th = juce::jlimit (2.f, 4.f, step * .7f);
                const float top = r.getY() + band + pad + (area - step * rows) * .5f; g.setColour (ink);
                for (auto* n : mc->getSequence().getNotes())
                    g.fillRect (beatToX (cs + n->getStartBeat().inBeats()), top + (float) (hi - n->getNoteNumber()) * step + (step - th) * .5f, std::max (3.f, (float) (n->getLengthBeats().inBeats() * pxPerBeat) - 1.f), th);
            }
        }
        if (auto* wc = dynamic_cast<te::WaveAudioClip*> (&c))
        {
            auto file = wc->getOriginalFile(); auto& th = thumbs[file.getFullPathName()];
            if (! th) { th = std::make_unique<juce::AudioThumbnail> (256, fm, thumbCache); th->setSource (new juce::FileInputSource (file)); }
            g.setColour (ink);
            th->drawChannels (g, (r.getHeight() >= 60.f ? r.toNearestInt().reduced (0, 6).withTrimmedTop (16) : (r.getHeight() >= 40.f ? r.toNearestInt().reduced (0, 2).withTrimmedTop (8) : r.toNearestInt().reduced (0, 2))), wc->getPosition().getOffset().inSeconds(), wc->getPosition().getOffset().inSeconds() + c.getPosition().getLength().inSeconds(), 1.f);
        }
    }
};
