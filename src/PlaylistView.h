#pragma once
#include "Theme.h"
#include "Icons.h"
#include <tracktion_engine/tracktion_engine.h>
#include <functional>

namespace te = tracktion;

// Playlist: cabeceras de pista (izquierda, tamaño fijo), regla de compases, cuadrícula con clips y cabezal.
class PlaylistView : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    static constexpr int kHeaderW = 176, kRulerH = 40, kTrackH = 150, kGap = 3;
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
    int trackTop (int i) const { return kRulerH + i * (kTrackH + kGap) - (int) scrollY; }
    double snapBeat (double b) const { return snap ? std::round (b) : b; }
    int numTracks() const { return te::getAudioTracks (edit).size(); }
    int trackAt (float y) const { for (int i = 0; i < numTracks(); ++i) if (y >= trackTop (i) && y < trackTop (i) + kTrackH) return i; return -1; }

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
            auto row = juce::Rectangle<int> (kHeaderW + 6, trackTop (i), W, kTrackH);
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
        grabKeyboardFocus();
        if (e.y < kRulerH && e.x >= kHeaderW) { seekTo (e.x); return; }
        dragClip = nullptr;
        if (e.x < kHeaderW + 6) return;
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
        if (dragClip == nullptr || tool != "select") return;
        const double nb = std::max (0.0, snapBeat (xToBeat ((float) e.x) - dragOffsetBeats));
        dragClip->setStart (te::TimePosition::fromSeconds (beatsToTime (nb)), false, true); repaint();
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (e.mods.isCtrlDown() || e.mods.isCommandDown())
        {
            const double anchor = xToBeat ((float) e.x); pxPerBeat = juce::jlimit (1.5, 120.0, pxPerBeat * (1.0 + w.deltaY * 0.6));
            scrollX = juce::jmax (0.0, anchor * pxPerBeat - (e.x - kHeaderW - 14));
        }
        else if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY)) scrollX = juce::jmax (0.0, scrollX - (w.deltaX != 0 ? w.deltaX : w.deltaY) * 600.0);
        else scrollY = juce::jlimit (0.0, (double) juce::jmax (0, numTracks() * (kTrackH + kGap) + kRulerH - getHeight() + 20), scrollY - w.deltaY * 400.0);
        repaint();
    }
    bool keyPressed (const juce::KeyPress& k) override
    {
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

    te::Clip* selected = nullptr; std::function<void (te::MidiClip*)> onOpenMidi;

private:
    te::Edit& edit; double pxPerBeat = 5.7, scrollX = 0, scrollY = 0, dragOffsetBeats = 0; te::Clip* dragClip = nullptr;
    juce::AudioFormatManager fm; juce::AudioThumbnailCache thumbCache; std::map<juce::String, std::unique_ptr<juce::AudioThumbnail>> thumbs;

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
        auto col = trackColours[i % 8]; auto r = juce::Rectangle<int> (6, trackTop (i), kHeaderW - 6, kTrackH);
        if (r.getBottom() < kRulerH || r.getY() > getHeight()) return;
        g.saveState(); g.reduceClipRegion (0, kRulerH, kHeaderW + 6, getHeight() - kRulerH);
        g.setColour (col); g.fillRoundedRectangle (r.toFloat(), 3.f);
        g.setColour (textOn (col));
        icons::draw (g, icons::Chevron, juce::Rectangle<float> ((float) r.getX() + 8, (float) r.getY() + 8, 22, 22), textOn (col));
        g.setFont (juce::FontOptions (16.f, juce::Font::bold));
        g.drawText (juce::String (i + 1) + " " + t.getName(), r.getX() + 34, r.getY() + 8, r.getWidth() - 40, 22, juce::Justification::centredLeft);
        g.restoreState();
    }

    void drawClip (juce::Graphics& g, te::Clip& c, int ti)
    {
        using namespace theme;
        const float x0 = beatToX (timeToBeats (c.getPosition().getStart().inSeconds())), x1 = beatToX (timeToBeats (c.getPosition().getEnd().inSeconds()));
        auto r = juce::Rectangle<float> (x0, (float) trackTop (ti) + 3, std::max (2.f, x1 - x0), (float) kTrackH - 6);
        auto col = trackColours[ti % 8];
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
