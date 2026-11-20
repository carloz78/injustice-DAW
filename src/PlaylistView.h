#pragma once
#include "Theme.h"
#include "Icons.h"
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
    void setTrackHeight (int i, int h) { auto tr = te::getAudioTracks (edit); if (i >= 0 && i < tr.size()) tr[i]->state.setProperty ("injH", juce::jlimit (kMinH, kMaxH, h), nullptr); layoutDirty = true; }
    void setAllHeights (int h) { h = juce::jlimit (kMinH, kMaxH, h); trackH = h; for (auto* t : te::getAudioTracks (edit)) t->state.setProperty ("injH", h, nullptr); layoutDirty = true; }
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
    float beatToX (double b) const { return (float) (kHeaderW + 14 + b * pxPerBeat - scrollX); }
    double xToBeat (float x) const { return (x - kHeaderW - 14 + scrollX) / pxPerBeat; }
    int trackTop (int i) const { ensureLayout(); const int n = (int) tops.size() - 1; return kRulerH + (i <= n ? tops[(size_t) std::max (0, i)] : tops[(size_t) n] + (i - n) * (trackH + kGap)) - (int) scrollY; }
    double snapBeat (double b) const { return snap ? std::round (b) : b; }
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
            const bool bar = std::fmod (b, 4.0) == 0.0;
            if (! bar && ! showBeats) continue;
            g.setColour (bar ? barLine : gridLine.withAlpha (.55f));
            g.fillRect (beatToX (b), (float) kRulerH, 1.f, (float) (H - kRulerH));
        }
        // clips
        for (int i = 0; i < tracks.size(); ++i)
            for (auto* c : tracks[i]->getClips()) drawClip (g, *c, i);
        g.restoreState();

        drawRuler (g, W);
        for (int i = 0; i < tracks.size(); ++i) drawHeader (g, *tracks[i], i);
        { auto ar = addR(); if (ar.getBottom() > kRulerH && ar.getY() < H) { g.setColour (juce::Colour (0xff1d1c22)); g.fillRoundedRectangle (ar, 6.f); g.setColour (juce::Colour (0xff34323a)); g.drawRoundedRectangle (ar.reduced (.5f), 6.f, 1.f);
            icons::draw (g, icons::Plus, { ar.getX() + 6, ar.getY(), 38.f, ar.getHeight() }, text); g.setColour (textDim); g.setFont (juce::FontOptions (14.f)); g.drawText ("Agregar pista", ar.withTrimmedLeft (46).toNearestInt(), juce::Justification::centredLeft); } }
        drawScrollbars (g);

        // cabezal
        const float px = beatToX (timeToBeats (edit.getTransport().getPosition().inSeconds()));
        if (px >= kHeaderW + 6 && px <= W)
        {
            g.setColour (juce::Colours::white.withAlpha (.75f)); g.fillRect (px - .5f, (float) kRulerH - 6, 1.f, (float) (H - kRulerH + 6));
            g.setColour (accentHi); g.fillEllipse (px - 11.f, kRulerH - 26.f, 22.f, 22.f);
            g.setColour (juce::Colours::white); g.setFont (juce::FontOptions (13.f, juce::Font::bold));
            g.drawText (juce::String ((int) (timeToBeats (edit.getTransport().getPosition().inSeconds()) / 4.0) + 1), juce::Rectangle<int> ((int) px - 11, kRulerH - 26, 22, 22), juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus(); sbDrag = 0;
        if (e.mods.isPopupMenu()) { contextMenu (e); return; }
        if (hScrollR().expanded (0, 4).contains (e.position)) { sbDrag = 1; sbGrab = e.x - hThumb().getX(); if (! hThumb().contains (e.position)) sbGrab = hThumb().getWidth() * .5f; dragScroll (e); return; }
        if (vScrollR().expanded (4, 0).contains (e.position)) { sbDrag = 2; sbGrab = e.y - vThumb().getY(); if (! vThumb().contains (e.position)) sbGrab = vThumb().getHeight() * .5f; dragScroll (e); return; }
        if (e.y < kRulerH && e.x >= kHeaderW) { seekTo (e.x); return; }
        dragClip = nullptr; dragMode = 0;
        if (const int ed = edgeAt (e.getPosition()); ed >= 0) { resizing = ed; resizeStartY = e.position.y; resizeStartH = hOf (ed); return; }
        if (e.x < kHeaderW + 6) { headerClick (e); return; }
        const int ti = trackAt ((float) e.y); if (ti < 0) { selected = nullptr; repaint(); return; }
        auto tr = te::getAudioTracks (edit)[ti];
        const double t = beatsToTime (xToBeat ((float) e.x));
        te::Clip* hit = nullptr;
        for (auto* c : tr->getClips()) if (c->getPosition().time.contains (te::TimePosition::fromSeconds (t))) hit = c;
        if (tool == "cut" && hit != nullptr) { tr->splitAt (te::TimePosition::fromSeconds (beatsToTime (snapBeat (xToBeat ((float) e.x))))); repaint(); return; }
        if (tool == "pencil" && hit == nullptr)
        {
            const double sb = std::max (0.0, std::floor (xToBeat ((float) e.x) / 4.0) * 4.0);
            auto c = tr->insertMIDIClip ("MIDI", te::TimeRange (te::TimePosition::fromSeconds (beatsToTime (sb)), te::TimePosition::fromSeconds (beatsToTime (sb + 16.0))), nullptr);
            selected = c.get(); repaint(); return;
        }
        selected = hit; dragClip = hit;
        if (hit)
        {
            const float x0 = beatToX (timeToBeats (hit->getPosition().getStart().inSeconds())), x1 = beatToX (timeToBeats (hit->getPosition().getEnd().inSeconds()));
            const float zone = tool == "trim" ? 16.f : juce::jmin (7.f, (x1 - x0) * .3f);
            if (e.x >= x1 - zone) dragMode = 2; else if (e.x <= x0 + zone) dragMode = 1; else if (tool == "trim") dragClip = nullptr;
        }
        if (hit) { dragOffsetBeats = xToBeat ((float) e.x) - timeToBeats (hit->getPosition().getStart().inSeconds()); }
        repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int ti = trackAt ((float) e.y);
        if (ti >= 0 && e.x < kHeaderW + 6 && ! dotsR (ti).contains (e.position)) { auto* tk = te::getAudioTracks (edit)[ti]; tk->setMute (! tk->isMuted (false)); repaint(); return; }
        if (ti < 0 || e.x < kHeaderW + 6) return;
        const double t = beatsToTime (xToBeat ((float) e.x));
        for (auto* c : te::getAudioTracks (edit)[ti]->getClips())
            if (auto* mc = dynamic_cast<te::MidiClip*> (c)) if (mc->getPosition().time.contains (te::TimePosition::fromSeconds (t))) { if (onOpenMidi) onOpenMidi (mc); return; }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.mouseWasDraggedSinceMouseDown() && e.y < kRulerH && e.x >= kHeaderW && dragClip == nullptr && selected == nullptr) { seekTo (e.x); return; }
        if (resizing >= 0) { setTrackHeight (resizing, resizeStartH + (int) (e.position.y - resizeStartY)); repaint(); return; }
        if (sbDrag) { dragScroll (e); return; }
        if (dragClip == nullptr || (tool != "select" && tool != "trim")) return;
        const double cs = timeToBeats (dragClip->getPosition().getStart().inSeconds()), ce = timeToBeats (dragClip->getPosition().getEnd().inSeconds());
        if (dragMode == 2) { const double nb = std::max (cs + 0.25, snapBeat (xToBeat ((float) e.x))); dragClip->setEnd (te::TimePosition::fromSeconds (beatsToTime (nb)), true); repaint(); return; }
        if (dragMode == 1) { const double nb = juce::jlimit (0.0, ce - 0.25, snapBeat (xToBeat ((float) e.x))); dragClip->setStart (te::TimePosition::fromSeconds (beatsToTime (nb)), true, false); repaint(); return; }
        if (tool == "trim") return;
        const double nb = std::max (0.0, snapBeat (xToBeat ((float) e.x) - dragOffsetBeats));
        dragClip->setStart (te::TimePosition::fromSeconds (beatsToTime (nb)), false, true); repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { sbDrag = 0; dragMode = 0; resizing = -1; }
    void mouseMove (const juce::MouseEvent& e) override
    {
        auto cur = juce::MouseCursor::NormalCursor;
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
        else if (e.mods.isCtrlDown() || e.mods.isCommandDown())
        {
            const double anchor = xToBeat ((float) e.x); pxPerBeat = juce::jlimit (1.5, 120.0, pxPerBeat * (1.0 + w.deltaY * 0.6));
            scrollX = juce::jmax (0.0, anchor * pxPerBeat - (e.x - kHeaderW - 14));
        }
        else if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY)) scrollX = juce::jmax (0.0, scrollX - (w.deltaX != 0 ? w.deltaX : w.deltaY) * 600.0);
        else scrollY = juce::jlimit (0.0, juce::jmax (0.0, contentH() - (getHeight() - kRulerH)), scrollY - w.deltaY * 400.0);
        repaint();
    }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k.getModifiers().isCommandDown() && juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) k.getKeyCode()) == 'd' && selected != nullptr) { if (auto* nc = trackutil::duplicateClip (edit, *selected)) selected = nc; repaint(); return true; }
        if ((k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) && selected != nullptr) { selected->removeFromParent(); selected = nullptr; dragClip = nullptr; repaint(); return true; }
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
    int dragMode = 0, sbDrag = 0; float sbGrab = 0;   // dragMode: 0 mover, 1 recortar inicio, 2 recortar final
    te::Edit& edit; double pxPerBeat = 3.6, scrollX = 0, scrollY = 0, dragOffsetBeats = 0; te::Clip* dragClip = nullptr;
    juce::AudioFormatManager fm; juce::AudioThumbnailCache thumbCache; std::map<juce::String, std::unique_ptr<juce::AudioThumbnail>> thumbs;


    //---- barras de desplazamiento
    double contentBeats() const { double e = 64; for (auto* t : te::getAudioTracks (edit)) for (auto* c : t->getClips()) e = std::max (e, timeToBeats (c->getPosition().getEnd().inSeconds())); return e + 32; }
    double contentH() const { ensureLayout(); return (double) tops.back() + trackH + kGap + 20; }
    juce::Rectangle<float> hScrollR() const { return { (float) kHeaderW + 6, (float) getHeight() - 11.f, (float) getWidth() - kHeaderW - 22.f, 8.f }; }
    juce::Rectangle<float> vScrollR() const { return { (float) getWidth() - 11.f, (float) kRulerH, 8.f, (float) getHeight() - kRulerH - 14.f }; }
    juce::Rectangle<float> hThumb() const { auto r = hScrollR(); const double tot = contentBeats() * pxPerBeat, vis = getWidth() - kHeaderW - 14; const float w = (float) juce::jlimit (30.0, (double) r.getWidth(), r.getWidth() * vis / tot); return { r.getX() + (float) ((r.getWidth() - w) * juce::jlimit (0.0, 1.0, scrollX / std::max (1.0, tot - vis))), r.getY(), w, r.getHeight() }; }
    juce::Rectangle<float> vThumb() const { auto r = vScrollR(); const double tot = contentH(), vis = getHeight() - kRulerH; const float h = (float) juce::jlimit (30.0, (double) r.getHeight(), r.getHeight() * vis / tot); return { r.getX(), r.getY() + (float) ((r.getHeight() - h) * juce::jlimit (0.0, 1.0, scrollY / std::max (1.0, tot - vis))), r.getWidth(), h }; }
    void dragScroll (const juce::MouseEvent& e)
    {
        if (sbDrag == 1) { auto r = hScrollR(); const double tot = contentBeats() * pxPerBeat, vis = getWidth() - kHeaderW - 14; const float tw = hThumb().getWidth(); scrollX = juce::jlimit (0.0, std::max (0.0, tot - vis), (double) ((e.x - sbGrab - r.getX()) / std::max (1.f, r.getWidth() - tw)) * (tot - vis)); }
        else { auto r = vScrollR(); const double tot = contentH(), vis = getHeight() - kRulerH; const float th = vThumb().getHeight(); scrollY = juce::jlimit (0.0, std::max (0.0, tot - vis), (double) ((e.y - sbGrab - r.getY()) / std::max (1.f, r.getHeight() - th)) * (tot - vis)); }
        repaint();
    }
    void drawScrollbars (juce::Graphics& g)
    {
        const bool needH = contentBeats() * pxPerBeat > getWidth() - kHeaderW - 14, needV = contentH() > getHeight() - kRulerH;
        g.setColour (juce::Colours::white.withAlpha (.07f));
        if (needH) { g.fillRoundedRectangle (hScrollR(), 4.f); g.setColour (theme::accent.withAlpha (.75f)); g.fillRoundedRectangle (hThumb(), 4.f); }
        g.setColour (juce::Colours::white.withAlpha (.07f));
        if (needV) { g.fillRoundedRectangle (vScrollR(), 4.f); g.setColour (theme::accent.withAlpha (.75f)); g.fillRoundedRectangle (vThumb(), 4.f); }
    }

    //---- cabeceras de pista: nombre, M/S, men\xc3\xba y "agregar pista"
    juce::Rectangle<int> headerR (int i) const { return { 6, trackTop (i), kHeaderW - 6, hOf (i) }; }
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
        m.addItem (4, "Agregar pista al final"); m.addItem (5, "Eliminar pista", tracks.size() > 1);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (dotsR (i).toNearestInt())), [this, t, i] (int r)
        {
            if (r == 1) trackutil::renameDialog (edit, t.get(), [this] { if (onTracksChanged) onTracksChanged(); repaint(); });
            else if (r == 2) t->setMute (! t->isMuted (false));
            else if (r == 3) t->setSolo (! t->isSolo (false));
            else if (r == 20) setAllHeights (34); else if (r == 21) setAllHeights (76); else if (r == 22) setAllHeights (116);
            else if (r == 23) setTrackHeight (i, 34); else if (r == 24) setTrackHeight (i, 116);
            else if (r >= 10 && r < 18) trackutil::setColour (edit, t.get(), r - 10);
            else if (r == 4) { trackutil::addTrack (edit); if (onTracksChanged) onTracksChanged(); if (onTrackSelected) onTrackSelected (numTracks() - 1); }
            else if (r == 5) { if (selected != nullptr && selected->getTrack() == t.get()) selected = nullptr; dragClip = nullptr; trackutil::deleteTrack (edit, t.get()); }
            layoutDirty = true; if (r != 0 && onTracksChanged) onTracksChanged(); repaint();
        });
    }
    void contextMenu (const juce::MouseEvent& e)
    {
        if (e.x < kHeaderW + 6) { const int ti = trackAt ((float) e.y); if (ti >= 0) trackMenu (ti); return; }
        const int ti = trackAt ((float) e.y); if (ti < 0) return;
        auto tr = te::getAudioTracks (edit)[ti]; const double t = beatsToTime (xToBeat ((float) e.x)); te::Clip* hit = nullptr;
        for (auto* c : tr->getClips()) if (c->getPosition().time.contains (te::TimePosition::fromSeconds (t))) hit = c;
        juce::PopupMenu m; const double clickBeat = snapBeat (xToBeat ((float) e.x));
        if (hit != nullptr)
        {
            selected = hit; te::Clip::Ptr c (hit);
            if (dynamic_cast<te::MidiClip*> (hit)) m.addItem (1, "Abrir en Piano Roll");
            m.addItem (2, "Duplicar  (Ctrl+D)"); m.addItem (3, "Dividir aqu\xc3\xad"); m.addSeparator(); m.addItem (4, "Eliminar  (Supr)");
            m.showMenuAsync (juce::PopupMenu::Options(), [this, c, tr, clickBeat] (int r)
            {
                if (r == 1) { if (onOpenMidi) onOpenMidi (dynamic_cast<te::MidiClip*> (c.get())); }
                else if (r == 2) { if (auto* nc = trackutil::duplicateClip (edit, *c)) selected = nc; }
                else if (r == 3) tr->splitAt (te::TimePosition::fromSeconds (beatsToTime (clickBeat)));
                else if (r == 4) { if (selected == c.get()) selected = nullptr; dragClip = nullptr; c->removeFromParent(); }
                repaint();
            });
        }
        else
        {
            m.addItem (1, "Crear clip MIDI aqu\xc3\xad");
            m.showMenuAsync (juce::PopupMenu::Options(), [this, tr, clickBeat] (int r)
            {
                if (r != 1) return; const double sb = std::max (0.0, std::floor (clickBeat / 4.0) * 4.0);
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
        return h;
    }
    bool thumbsPending() const { for (auto& kv : thumbs) if (kv.second && ! kv.second->isFullyLoaded()) return true; return false; }
    void timerCallback() override
    {
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
            const float x = beatToX (bar * 4.0); if (x > W) break;
            if (x < kHeaderW + 10 || (bar % stride) != (stride == 1 ? 0 : 0)) { if (x < kHeaderW + 10 || bar % stride != 0) continue; }
            g.drawText (juce::String (bar + 1), juce::Rectangle<float> (x - 20, 18, 40, 20).toNearestInt(), juce::Justification::centred);
        }
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
          juce::Path tri; tri.addTriangle (c.getX() + 4.f, c.getY() + 5.5f, c.getX() + 11.f, c.getY() + 5.5f, c.getX() + 7.5f, c.getY() + 10.5f); g.fillPath (tri); }
        g.setColour (fg); g.setFont (juce::FontOptions (13.f));
        g.drawText (t.getName() == "Track" ? juce::String::fromUTF8 ("Pista ") + juce::String (i + 1) : t.getName(), r.getX() + 29, r.getHeight() >= 40 ? r.getY() + 4 : r.getY() + (r.getHeight() - 20) / 2, r.getWidth() - 62, 20, juce::Justification::centredLeft);
        if (r.getHeight() >= 26) icons::draw (g, icons::Dots, dotsR (i), fg);
        juce::String inst; for (auto* p : t.pluginList.getPlugins()) if (dynamic_cast<te::ExternalPlugin*> (p) || dynamic_cast<te::SamplerPlugin*> (p)) { inst = p->getName(); break; }
        g.setColour (fg.withAlpha (.8f)); g.setFont (juce::FontOptions (13.f));
        if (r.getHeight() >= 60 && inst.isNotEmpty()) g.drawText (inst, r.getX() + 29, r.getY() + 24, r.getWidth() - 36, 18, juce::Justification::centredLeft);
        if (r.getHeight() >= 60 && (t.isMuted (false) || t.isSolo (false))) { g.setColour (fg.withAlpha (.85f)); g.setFont (juce::FontOptions (12.f, juce::Font::bold)); g.drawText (t.isSolo (false) ? "SOLO" : juce::String::fromUTF8 ("SILENCIADA"), r.getX() + 10, r.getBottom() - 26, r.getWidth() - 16, 20, juce::Justification::centredLeft); }
        if (t.isMuted (false)) { g.setColour (juce::Colour (0xff32353e).withAlpha (.6f)); g.fillRect (r); }
        g.restoreState();
    }

    void drawClip (juce::Graphics& g, te::Clip& c, int ti)
    {
        using namespace theme;
        const float x0 = beatToX (timeToBeats (c.getPosition().getStart().inSeconds())), x1 = beatToX (timeToBeats (c.getPosition().getEnd().inSeconds()));
        auto r = juce::Rectangle<float> (x0, (float) trackTop (ti), std::max (2.f, x1 - x0), (float) hOf (ti));
        auto col = trackutil::colourOf (te::getAudioTracks (edit)[ti], ti);
        const bool muted = te::getAudioTracks (edit)[ti]->isMuted (false);
        if (muted) col = col.withSaturation (col.getSaturation() * .25f).darker (.35f);
        const auto ink = inkOn (col);
        juce::Path shape; shape.addRectangle (r);
        g.setColour (col); g.fillPath (shape);
        { g.saveState(); g.reduceClipRegion (shape, {}); g.setColour (juce::Colours::white.withAlpha (.17f));
          const double gs = pxPerBeat >= 14 ? 1.0 : 4.0; for (double b = std::floor (xToBeat (r.getX()) / gs) * gs; beatToX (b) < r.getRight(); b += gs) g.fillRect (beatToX (b), r.getY(), 1.f, r.getHeight()); g.restoreState(); }
        g.setColour (juce::Colour (0xff15151d)); g.drawRect (r, 1.5f);
        if (&c == selected) { g.setColour (juce::Colours::white); g.drawRect (r, 2.f); }
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
