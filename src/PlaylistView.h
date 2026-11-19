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
    static constexpr int kHeaderW = 176, kRulerH = 40, kGap = 3;
    int trackH = 100;   // alto de pista; Alt+rueda lo cambia (60-220)
    juce::String tool = "select"; bool snap = true;

    explicit PlaylistView (te::Edit& e) : edit (e), thumbCache (32)
    {
        fm.registerBasicFormats(); setWantsKeyboardFocus (true); startTimerHz (30);
    }

    //==============================================================================
    double beatsToTime (double b) const { return edit.tempoSequence.toTime (te::BeatPosition::fromBeats (b)).inSeconds(); }
    double timeToBeats (double t) const { return edit.tempoSequence.toBeats (te::TimePosition::fromSeconds (t)).inBeats(); }
    float beatToX (double b) const { return (float) (kHeaderW + 14 + b * pxPerBeat - scrollX); }
    double xToBeat (float x) const { return (x - kHeaderW - 14 + scrollX) / pxPerBeat; }
    int trackTop (int i) const { return kRulerH + i * (trackH + kGap) - (int) scrollY; }
    double snapBeat (double b) const { return snap ? std::round (b) : b; }
    int numTracks() const { return te::getAudioTracks (edit).size(); }
    int trackAt (float y) const { for (int i = 0; i < numTracks(); ++i) if (y >= trackTop (i) && y < trackTop (i) + trackH) return i; return -1; }

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
            auto row = juce::Rectangle<int> (kHeaderW + 6, trackTop (i), W, trackH);
            g.setColour ((i & 1) ? bgGridAlt : bgGrid); g.fillRect (row);
            g.setColour (rowLine); g.fillRect (row.getX(), row.getBottom(), W, kGap);
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
            g.setColour (accentHi); g.fillRect (px - .5f, (float) kRulerH - 6, 2.f, (float) (H - kRulerH + 6));
            g.fillEllipse (px - 11.f, kRulerH - 26.f, 22.f, 22.f);
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
        const int ti = trackAt ((float) e.y); if (ti < 0 || e.x < kHeaderW + 6) return;
        const double t = beatsToTime (xToBeat ((float) e.x));
        for (auto* c : te::getAudioTracks (edit)[ti]->getClips())
            if (auto* mc = dynamic_cast<te::MidiClip*> (c)) if (mc->getPosition().time.contains (te::TimePosition::fromSeconds (t))) { if (onOpenMidi) onOpenMidi (mc); return; }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.mouseWasDraggedSinceMouseDown() && e.y < kRulerH && e.x >= kHeaderW && dragClip == nullptr && selected == nullptr) { seekTo (e.x); return; }
        if (sbDrag) { dragScroll (e); return; }
        if (dragClip == nullptr || (tool != "select" && tool != "trim")) return;
        const double cs = timeToBeats (dragClip->getPosition().getStart().inSeconds()), ce = timeToBeats (dragClip->getPosition().getEnd().inSeconds());
        if (dragMode == 2) { const double nb = std::max (cs + 0.25, snapBeat (xToBeat ((float) e.x))); dragClip->setEnd (te::TimePosition::fromSeconds (beatsToTime (nb)), true); repaint(); return; }
        if (dragMode == 1) { const double nb = juce::jlimit (0.0, ce - 0.25, snapBeat (xToBeat ((float) e.x))); dragClip->setStart (te::TimePosition::fromSeconds (beatsToTime (nb)), true, false); repaint(); return; }
        if (tool == "trim") return;
        const double nb = std::max (0.0, snapBeat (xToBeat ((float) e.x) - dragOffsetBeats));
        dragClip->setStart (te::TimePosition::fromSeconds (beatsToTime (nb)), false, true); repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { sbDrag = 0; dragMode = 0; }
    void mouseMove (const juce::MouseEvent& e) override
    {
        auto cur = juce::MouseCursor::NormalCursor;
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
        if (e.mods.isAltDown()) { trackH = juce::jlimit (60, 220, trackH + (w.deltaY > 0 ? 10 : -10)); scrollY = juce::jlimit (0.0, juce::jmax (0.0, contentH() - (getHeight() - kRulerH)), scrollY); }
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
    double contentH() const { return (double) (numTracks() + 1) * (trackH + kGap) + 20; }
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
    juce::Rectangle<int> headerR (int i) const { return { 6, trackTop (i), kHeaderW - 6, trackH }; }
    juce::Rectangle<float> dotsR (int i) const { auto r = headerR (i); return { (float) r.getRight() - 32.f, (float) r.getY() + 6.f, 26.f, 26.f }; }
    juce::Rectangle<float> msR (int i, int k) const { auto r = headerR (i); return { (float) r.getX() + 10.f + k * 40.f, (float) r.getY() + 44.f, 34.f, 26.f }; }
    juce::Rectangle<float> addR() const { return { 6.f, (float) trackTop (numTracks()) + 6.f, (float) kHeaderW - 6.f, 42.f }; }
    void headerClick (const juce::MouseEvent& e)
    {
        auto tracks = te::getAudioTracks (edit); const auto p = e.position;
        if (addR().contains (p)) { if (trackutil::addTrack (edit) != nullptr) { if (onTracksChanged) onTracksChanged(); if (onTrackSelected) onTrackSelected (numTracks() - 1); } scrollY = juce::jmax (0.0, contentH() - (getHeight() - kRulerH)); repaint(); return; }
        for (int i = 0; i < tracks.size(); ++i)
        {
            if (! headerR (i).contains (e.getPosition())) continue;
            if (onTrackSelected) onTrackSelected (i);
            if (msR (i, 0).contains (p)) { tracks[i]->setSolo (! tracks[i]->isSolo (false)); repaint(); return; }
            if (msR (i, 1).contains (p)) { tracks[i]->setMute (! tracks[i]->isMuted (false)); repaint(); return; }
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
        m.addSubMenu ("Color", col); m.addSeparator();
        m.addItem (4, "Agregar pista al final"); m.addItem (5, "Eliminar pista", tracks.size() > 1);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (dotsR (i).toNearestInt())), [this, t, i] (int r)
        {
            if (r == 1) trackutil::renameDialog (edit, t.get(), [this] { if (onTracksChanged) onTracksChanged(); repaint(); });
            else if (r == 2) t->setMute (! t->isMuted (false));
            else if (r == 3) t->setSolo (! t->isSolo (false));
            else if (r >= 10 && r < 18) trackutil::setColour (edit, t.get(), r - 10);
            else if (r == 4) { trackutil::addTrack (edit); if (onTracksChanged) onTracksChanged(); if (onTrackSelected) onTrackSelected (numTracks() - 1); }
            else if (r == 5) { if (selected != nullptr && selected->getTrack() == t.get()) selected = nullptr; dragClip = nullptr; trackutil::deleteTrack (edit, t.get()); }
            if (r != 0 && onTracksChanged) onTracksChanged(); (void) i; repaint();
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

    void timerCallback() override { repaint(); }
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
        g.setColour (col); g.fillRoundedRectangle (r.toFloat(), 3.f);
        const auto fg = textOn (col);
        g.setColour (fg); g.setFont (juce::FontOptions (16.f, juce::Font::bold));
        g.drawText (juce::String (i + 1) + " " + (t.getName() == "Track" ? juce::String() : t.getName()), r.getX() + 10, r.getY() + 8, r.getWidth() - 50, 24, juce::Justification::centredLeft);
        icons::draw (g, icons::Dots, dotsR (i), fg);
        for (int k = 0; k < 2; ++k)
        {
            const bool on = k == 0 ? t.isSolo (false) : t.isMuted (false); auto b = msR (i, k);
            g.setColour (on ? juce::Colours::black.withAlpha (.55f) : juce::Colours::black.withAlpha (.22f)); g.fillRoundedRectangle (b, 5.f);
            g.setColour (on ? juce::Colours::white : fg); g.setFont (juce::FontOptions (14.f, juce::Font::bold)); g.drawText (k == 0 ? "S" : "M", b.toNearestInt(), juce::Justification::centred);
        }
        juce::String inst; for (auto* p : t.pluginList.getPlugins()) if (dynamic_cast<te::ExternalPlugin*> (p) || dynamic_cast<te::SamplerPlugin*> (p)) { inst = p->getName(); break; }
        g.setColour (fg.withAlpha (.8f)); g.setFont (juce::FontOptions (13.f));
        if (trackH >= 90) g.drawText (inst.isEmpty() ? juce::String::fromUTF8 ("Sin instrumento") : inst, r.getX() + 10, r.getY() + 72, r.getWidth() - 16, 20, juce::Justification::centredLeft);
        g.restoreState();
    }

    void drawClip (juce::Graphics& g, te::Clip& c, int ti)
    {
        using namespace theme;
        const float x0 = beatToX (timeToBeats (c.getPosition().getStart().inSeconds())), x1 = beatToX (timeToBeats (c.getPosition().getEnd().inSeconds()));
        auto r = juce::Rectangle<float> (x0, (float) trackTop (ti) + 3, std::max (2.f, x1 - x0), (float) trackH - 6);
        auto col = trackutil::colourOf (te::getAudioTracks (edit)[ti], ti);
        g.setColour (col.withAlpha (.28f)); g.fillRoundedRectangle (r, 3.f);
        g.setColour (col.withAlpha (&c == selected ? 1.f : .75f)); g.drawRoundedRectangle (r, 3.f, &c == selected ? 2.f : 1.2f);
        g.setColour (juce::Colours::white); g.setFont (juce::FontOptions (13.f, juce::Font::bold));
        g.drawText (c.getName(), r.toNearestInt().reduced (6, 3), juce::Justification::topLeft);
        if (auto* sc = dynamic_cast<te::StepClip*> (&c))
        {
            auto pat = sc->getPattern (0); const int n = pat.getNumNotes(); const double cs = timeToBeats (c.getPosition().getStart().inSeconds()), ce = timeToBeats (c.getPosition().getEnd().inSeconds());
            const int rows = std::min (4, sc->getChannels().size()); const float rh = std::min (14.f, (r.getHeight() - 30.f) / (float) std::max (1, rows));
            for (double b0 = 0; b0 < ce - cs; b0 += n * 0.25)
                for (int rr = 0; rr < rows; ++rr) for (int s = 0; s < n; ++s)
                {
                    if (! pat.getNote (rr, s)) continue; const double bb = b0 + s * 0.25; if (bb >= ce - cs) break;
                    g.setColour (col.brighter (.6f)); g.fillRect (beatToX (cs + bb), r.getY() + 24.f + rr * rh, std::max (2.f, (float) (0.25 * pxPerBeat) - 1.f), rh - 2.f);
                }
        }
        if (auto* mc = dynamic_cast<te::MidiClip*> (&c))
        {
            const double cs = timeToBeats (c.getPosition().getStart().inSeconds()); int lo = 127, hi = 0;
            for (auto* n : mc->getSequence().getNotes()) { lo = std::min (lo, n->getNoteNumber()); hi = std::max (hi, n->getNoteNumber()); }
            if (lo <= hi)
            {
                const float rh = std::min (8.f, (r.getHeight() - 30.f) / (float) (hi - lo + 1)); g.setColour (col.brighter (.6f));
                for (auto* n : mc->getSequence().getNotes())
                    g.fillRect (beatToX (cs + n->getStartBeat().inBeats()), r.getBottom() - 6 - (float) (n->getNoteNumber() - lo + 1) * rh, std::max (2.f, (float) (n->getLengthBeats().inBeats() * pxPerBeat)), std::max (2.f, rh - 1.f));
            }
        }
        if (auto* wc = dynamic_cast<te::WaveAudioClip*> (&c))
        {
            auto file = wc->getOriginalFile(); auto& th = thumbs[file.getFullPathName()];
            if (! th) { th = std::make_unique<juce::AudioThumbnail> (256, fm, thumbCache); th->setSource (new juce::FileInputSource (file)); }
            g.setColour (col.brighter (.5f).withAlpha (.9f));
            th->drawChannels (g, r.toNearestInt().reduced (2, 22), wc->getPosition().getOffset().inSeconds(), wc->getPosition().getOffset().inSeconds() + c.getPosition().getLength().inSeconds(), 1.f);
        }
    }
};
