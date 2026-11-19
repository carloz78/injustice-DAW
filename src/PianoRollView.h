#pragma once
#include "Theme.h"
#include <tracktion_engine/tracktion_engine.h>
namespace te = tracktion;

// Piano Roll: teclado vertical, cuadrícula MIDI, notas compactas y carril de velocity. Edita un MidiClip de Tracktion.
class PianoRollView : public juce::Component, private juce::Timer
{
public:
    static constexpr int kKeyW = 96, kRulerH = 36, kVelH = 124, kNoteH = 20;
    juce::String tool = "select"; double quantum = 0.25; bool snap = true;   // 0.25 beats = 1/16
    std::function<void()> onClipChanged;

    explicit PianoRollView (te::Edit& e) : edit (e) { setWantsKeyboardFocus (true); startTimerHz (30); scrollY = (127 - 76) * kNoteH; }

    void setClip (te::MidiClip* c) { clip = c; sel = nullptr; scrollX = 0; if (clip) { auto n = clip->getSequence().getNotes(); if (n.size()) scrollY = (127 - n[0]->getNoteNumber() - 5) * (double) kNoteH; } repaint(); }
    te::MidiClip* getClip() const { return clip.get(); }

    //==============================================================================
    double beatsToTime (double b) const { return edit.tempoSequence.toTime (te::BeatPosition::fromBeats (b)).inSeconds(); }
    double timeToBeats (double t) const { return edit.tempoSequence.toBeats (te::TimePosition::fromSeconds (t)).inBeats(); }
    double clipStartBeat() const { return clip ? timeToBeats (clip->getPosition().getStart().inSeconds()) : 0.0; }
    double clipLenBeats() const { return clip ? timeToBeats (clip->getPosition().getEnd().inSeconds()) - clipStartBeat() : 0.0; }
    float bx (double beat) const { return (float) (kKeyW + 8 + beat * ppb - scrollX); }
    double xb (float x) const { return (x - kKeyW - 8 + scrollX) / ppb; }
    float ny (int pitch) const { return (float) (kRulerH + (127 - pitch) * kNoteH - scrollY); }
    int yp (float y) const { return 127 - (int) std::floor ((y - kRulerH + scrollY) / kNoteH); }
    int gridBottom() const { return getHeight() - kVelH; }
    double q (double b) const { return snap ? std::floor (b / quantum + 0.5) * quantum : b; }

    void paint (juce::Graphics& g) override
    {
        using namespace theme;
        const int W = getWidth(), H = getHeight(), gb = gridBottom();
        g.fillAll (juce::Colour (0xff100f17));
        if (! clip) { g.setColour (textDim); g.setFont (juce::FontOptions (16.f)); g.drawText (juce::String::fromUTF8 ("Dibuja un clip MIDI en la Playlist (herramienta lápiz) y haz doble clic en él."), getLocalBounds(), juce::Justification::centred); return; }

        g.saveState(); g.reduceClipRegion (kKeyW, kRulerH, W - kKeyW, gb - kRulerH);
        // filas (teclas negras más oscuras)
        for (int p = 0; p <= 127; ++p)
        {
            const float y = ny (p); if (y > gb || y + kNoteH < kRulerH) continue;
            g.setColour (isBlack (p) ? juce::Colour (0xff14121b) : juce::Colour (0xff1b1723)); g.fillRect ((float) kKeyW, y, (float) W, (float) kNoteH);
            g.setColour (juce::Colour (0xff100f17)); g.fillRect ((float) kKeyW, y + kNoteH - 1.f, (float) W, 1.f);
            if (p % 12 == 0) { g.setColour (juce::Colour (0xff3a2a47)); g.fillRect ((float) kKeyW, y + kNoteH - 1.f, (float) W, 1.f); }
        }
        // zona fuera del clip, más oscura
        const float xe = bx (clipLenBeats());
        g.setColour (juce::Colours::black.withAlpha (.35f)); g.fillRect (xe, (float) kRulerH, (float) W, (float) (gb - kRulerH));
        // líneas verticales
        const double step = ppb * quantum >= 9 ? quantum : (ppb >= 9 ? 1.0 : 4.0);
        for (double b = std::floor (xb ((float) kKeyW) / step) * step; bx (b) < W; b += step)
        {
            const bool bar = std::fmod (std::round (b * 1000) / 1000, 4.0) == 0.0, beat = std::fmod (std::round (b * 1000) / 1000, 1.0) == 0.0;
            g.setColour (bar ? juce::Colour (0xff4a3a58) : (beat ? juce::Colour (0xff2c2436) : juce::Colour (0xff211b2a)));
            g.fillRect (bx (b), (float) kRulerH, 1.f, (float) (gb - kRulerH));
        }
        // notas
        for (auto* n : clip->getSequence().getNotes()) drawNote (g, *n);
        g.restoreState();

        drawRuler (g, W); drawKeyboard (g, gb); drawVelocity (g, W, H, gb);

        // cabezal (si cae dentro del clip)
        const double hb = timeToBeats (edit.getTransport().getPosition().inSeconds()) - clipStartBeat();
        const float px = bx (hb);
        if (hb >= 0 && px >= kKeyW + 8 && px <= W) { g.setColour (accentHi); g.fillRect (px - .5f, (float) kRulerH, 2.f, (float) (gb - kRulerH)); g.fillRect (px - .5f, (float) (gb + 2), 2.f, (float) (kVelH - 2)); }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus(); if (! clip) return;
        if (e.y >= gridBottom()) { velocityAt (e); return; }
        if (e.x < kKeyW || e.y < kRulerH) { if (e.y < kRulerH && e.x > kKeyW) seek (e.x); else if (e.x < kKeyW) { litKey = yp ((float) e.y); repaint(); } return; }
        auto* hit = noteAt (e.position); mode = Mode::none;
        if (tool == "erase") { if (hit) removeNote (hit); mode = Mode::erase; return; }
        if (hit == nullptr && tool == "pencil")
        {
            const double start = std::max (0.0, q (xb ((float) e.x)));
            if (start >= clipLenBeats()) return;
            auto* um = &edit.getUndoManager(); um->beginNewTransaction (juce::String::fromUTF8 ("Añadir nota"));
            sel = clip->getSequence().addNote (juce::jlimit (0, 127, yp ((float) e.y)), te::BeatPosition::fromBeats (start), te::BeatDuration::fromBeats (lastLen), lastVel, 0, um);
            mode = Mode::resize; anchor = start; repaint(); return;
        }
        sel = hit;
        if (hit)
        {
            const float rx = bx (hit->getEndBeat().inBeats());
            mode = (e.x > rx - 7 && tool != "pencil") ? Mode::resize : Mode::move; anchor = hit->getStartBeat().inBeats();
            grab = xb ((float) e.x) - anchor; edit.getUndoManager().beginNewTransaction ("Editar nota"); lastLen = hit->getLengthBeats().inBeats();
        }
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! clip) return;
        if (e.mouseWasDraggedSinceMouseDown() && e.y >= gridBottom() && mode == Mode::none) { velocityAt (e); return; }
        auto* um = &edit.getUndoManager();
        if (mode == Mode::erase) { if (auto* h = noteAt (e.position)) removeNote (h); return; }
        if (sel == nullptr) return;
        if (mode == Mode::move)
        {
            const double ns = std::max (0.0, q (xb ((float) e.x) - grab)); const int np = juce::jlimit (0, 127, yp ((float) e.y));
            if (np != sel->getNoteNumber()) sel->setNoteNumber (np, um);
            sel->setStartAndLength (te::BeatPosition::fromBeats (ns), sel->getLengthBeats(), um);
        }
        else if (mode == Mode::resize)
        {
            const double end = std::max (sel->getStartBeat().inBeats() + (snap ? quantum : 0.05), q (xb ((float) e.x)) + (snap ? quantum * 0 : 0));
            const double len = std::max (snap ? quantum : 0.05, (snap ? std::max (q (xb ((float) e.x)), sel->getStartBeat().inBeats() + quantum) : end) - sel->getStartBeat().inBeats());
            sel->setStartAndLength (sel->getStartBeat(), te::BeatDuration::fromBeats (len), um); lastLen = len;
        }
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { if (mode != Mode::none && onClipChanged) onClipChanged(); mode = Mode::none; litKey = -1; repaint(); }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (e.mods.isCtrlDown() || e.mods.isCommandDown()) { const double a = xb ((float) e.x); ppb = juce::jlimit (6.0, 400.0, ppb * (1.0 + w.deltaY * .6)); scrollX = juce::jmax (0.0, a * ppb - (e.x - kKeyW - 8)); }
        else if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY)) scrollX = juce::jmax (0.0, scrollX - (w.deltaX != 0 ? w.deltaX : w.deltaY) * 600.0);
        else scrollY = juce::jlimit (0.0, (double) (128 * kNoteH - (gridBottom() - kRulerH)), scrollY - w.deltaY * 400.0);
        repaint();
    }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if ((k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) && sel != nullptr) { removeNote (sel); return true; }
        return false;
    }

private:
    enum class Mode { none, move, resize, erase };
    te::Edit& edit; te::MidiClip::Ptr clip; te::MidiNote* sel = nullptr; Mode mode = Mode::none;
    double ppb = 50.0, scrollX = 0, scrollY = 0, anchor = 0, grab = 0, lastLen = 0.25; int lastVel = 100, litKey = -1;

    static bool isBlack (int p) { switch (p % 12) { case 1: case 3: case 6: case 8: case 10: return true; default: return false; } }
    void timerCallback() override { if (isVisible()) repaint(); }
    void seek (int x) { auto b = std::max (0.0, xb ((float) x)) + clipStartBeat(); edit.getTransport().setPosition (te::TimePosition::fromSeconds (beatsToTime (q (b)))); }
    void removeNote (te::MidiNote* n) { edit.getUndoManager().beginNewTransaction ("Borrar nota"); if (sel == n) sel = nullptr; clip->getSequence().removeNote (*n, &edit.getUndoManager()); if (onClipChanged) onClipChanged(); repaint(); }

    te::MidiNote* noteAt (juce::Point<float> p) const
    {
        for (auto* n : clip->getSequence().getNotes())
        {
            juce::Rectangle<float> r (bx (n->getStartBeat().inBeats()), ny (n->getNoteNumber()), (float) (n->getLengthBeats().inBeats() * ppb), (float) kNoteH);
            if (r.contains (p)) return n;
        }
        return nullptr;
    }
    void velocityAt (const juce::MouseEvent& e)
    {
        te::MidiNote* best = nullptr; float bd = 1e9f;
        for (auto* n : clip->getSequence().getNotes()) { const float d = std::abs (bx (n->getStartBeat().inBeats()) - (float) e.x); if (d < bd) { bd = d; best = n; } }
        if (best == nullptr || bd > 14.f) return;
        const int v = juce::jlimit (1, 127, (int) std::round (127.0 * (getHeight() - 6 - e.y) / (kVelH - 14.0)));
        edit.getUndoManager().beginNewTransaction ("Velocity"); best->setVelocity (v, &edit.getUndoManager()); lastVel = v; sel = best; repaint();
        if (onClipChanged) onClipChanged();
    }

    void drawNote (juce::Graphics& g, te::MidiNote& n)
    {
        juce::Rectangle<float> r (bx (n.getStartBeat().inBeats()), ny (n.getNoteNumber()) + 2, std::max (3.f, (float) (n.getLengthBeats().inBeats() * ppb) - 1.f), (float) kNoteH - 4);
        const bool s = (&n == sel);
        g.setColour (s ? juce::Colour (0xffff7bff) : theme::accentHi.withMultipliedBrightness (.6f + .4f * (float) n.getVelocity() / 127.f)); g.fillRoundedRectangle (r, 2.f);
    }
    void drawRuler (juce::Graphics& g, int W)
    {
        g.setColour (juce::Colour (0xff2a1233)); g.fillRect (kKeyW, 0, W, kRulerH);
        g.setColour (theme::text); g.setFont (juce::FontOptions (15.f, juce::Font::bold));
        for (int bar = 0; ; ++bar) { const float x = bx (bar * 4.0); if (x > W) break; if (x < kKeyW + 4) continue; g.drawText (juce::String (bar + 1), juce::Rectangle<float> (x + 6, 8, 40, 20).toNearestInt(), juce::Justification::centredLeft); }
        g.setColour (juce::Colour (0xff100f17)); g.fillRect (0, 0, kKeyW, kRulerH);
    }
    void drawKeyboard (juce::Graphics& g, int gb)
    {
        g.saveState(); g.reduceClipRegion (0, kRulerH, kKeyW, gb - kRulerH);
        g.setColour (juce::Colour (0xff100f17)); g.fillRect (0, kRulerH, kKeyW, gb - kRulerH);
        for (int p = 0; p <= 127; ++p)
        {
            const float y = ny (p); if (y > gb || y + kNoteH < kRulerH) continue;
            if (! isBlack (p)) { g.setColour (p == litKey ? juce::Colour (0xffe9a6f0) : juce::Colour (0xffe4e2e8)); g.fillRect (2.f, y, (float) kKeyW - 8, (float) kNoteH - 1.f);
                if (p % 12 == 0) { g.setColour (juce::Colour (0xff1c1a22)); g.setFont (juce::FontOptions (13.f, juce::Font::bold)); g.drawText ("C" + juce::String (p / 12 - 1), juce::Rectangle<float> (2.f, y, (float) kKeyW - 12, (float) kNoteH).toNearestInt(), juce::Justification::centredRight); } }
        }
        for (int p = 0; p <= 127; ++p)
        {
            const float y = ny (p); if (y > gb || y + kNoteH < kRulerH || ! isBlack (p)) continue;
            g.setColour (p == litKey ? juce::Colour (0xffb06ab8) : juce::Colour (0xff17151d)); g.fillRect (2.f, y + 1, 56.f, (float) kNoteH - 2.f);
        }
        g.restoreState();
    }
    void drawVelocity (juce::Graphics& g, int W, int H, int gb)
    {
        g.setColour (juce::Colour (0xff4a2a58)); g.fillRect (kKeyW, gb, W, 1);
        g.setColour (juce::Colour (0xff13111a)); g.fillRect (0, gb + 1, kKeyW, kVelH - 1);
        g.setColour (theme::text); g.setFont (juce::FontOptions (14.f, juce::Font::bold)); g.drawText ("Velocity", 12, gb + 8, kKeyW - 12, 22, juce::Justification::centredLeft);
        g.saveState(); g.reduceClipRegion (kKeyW, gb + 1, W - kKeyW, kVelH);
        for (auto* n : clip->getSequence().getNotes())
        {
            const float x = bx (n->getStartBeat().inBeats()), h = (kVelH - 14.f) * n->getVelocity() / 127.f;
            g.setColour (&*n == sel ? juce::Colour (0xffff7bff) : theme::accentHi); g.fillRect (x, H - 4 - h, 3.f, h); g.fillEllipse (x - 2, H - 4 - h - 2, 7.f, 7.f);
        }
        g.restoreState();
    }
};
