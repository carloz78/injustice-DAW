#pragma once
#include "Harmony.h"
#include "Theme.h"
#include "TimeSig.h"
#include "Icons.h"
#include "Cursors.h"
#include "TrackUtil.h"
#include <set>
#include <tracktion_engine/tracktion_engine.h>
namespace te = tracktion;

// Piano Roll: teclado vertical, cuadrícula MIDI, notas compactas y carril de velocity. Edita un MidiClip de Tracktion.
class PianoRollView : public juce::Component, public juce::TooltipClient, private juce::Timer
{
public:
    static constexpr int kKeyW = 96, kBarH = 52, kRulerH = 32, kTop = kBarH + kRulerH;
    int kVelH = 90, kNoteH = 20;   // alto del carril de velocity y de cada nota (se cambian con el zoom vertical)
    juce::String tool = "pencil"; double quantum = 0.25; bool snap = true;   // 0.25 beats = 1/16
    std::function<void()> onClipChanged; std::function<void (const juce::String&)> onMessage; std::function<void (te::AudioTrack*, int)> onAudition;   // suena la nota en la pista del clip

    explicit PianoRollView (te::Edit& e) : edit (e) { setWantsKeyboardFocus (true); setOpaque (true); startTimerHz (60); scrollY = (127 - 76) * kNoteH; }

    void setClip (te::MidiClip* c) { clip = c; sel = nullptr; selSet.clear(); scrollX = 0; lane = c ? juce::jlimit (0, 2, (int) c->state.getProperty ("injLane", 0)) : 0; if (clip) { auto n = clip->getSequence().getNotes(); scrollY = n.size() ? (127 - n[0]->getNoteNumber() - 5) * (double) kNoteH : (127 - 76) * (double) kNoteH; } repaint(); }
    te::MidiClip* getClip() const { return clip.get(); }

    //==============================================================================
    double beatsToTime (double b) const { return edit.tempoSequence.toTime (te::BeatPosition::fromBeats (b)).inSeconds(); }
    double timeToBeats (double t) const { return edit.tempoSequence.toBeats (te::TimePosition::fromSeconds (t)).inBeats(); }
    double clipStartBeat() const { return clip ? timeToBeats (clip->getPosition().getStart().inSeconds()) : 0.0; }
    double clipLenBeats() const { return clip ? timeToBeats (clip->getPosition().getEnd().inSeconds()) - clipStartBeat() : 0.0; }
    float bx (double beat) const { return (float) (kKeyW + 8 + beat * ppb - scrollX); }
    double xb (float x) const { return (x - kKeyW - 8 + scrollX) / ppb; }
    float ny (int pitch) const { return (float) (kTop + (127 - pitch) * kNoteH - scrollY); }
    int yp (float y) const { return 127 - (int) std::floor ((y - kTop + scrollY) / kNoteH); }
    int gridBottom() const { return getHeight() - kVelH; }
    double q (double b) const { return snap ? std::floor (b / quantum + 0.5) * quantum : b; }

    void paint (juce::Graphics& g) override
    {
        using namespace theme;
        const int W = getWidth(), H = getHeight(), gb = gridBottom();
        g.fillAll (bgBar2);
        drawToolbar (g, W);
        if (! clip) { g.setColour (textDim); g.setFont (juce::FontOptions (14.f)); g.drawText (juce::String::fromUTF8 ("Dibuja un clip MIDI en la Playlist (herramienta lápiz) y haz doble clic en él."), getLocalBounds().withTrimmedTop (kBarH), juce::Justification::centred); return; }

        g.saveState(); g.reduceClipRegion (kKeyW, kTop, W - kKeyW, gb - kTop);
        for (int p = 0; p <= 127; ++p)
        {
            const float y = ny (p); if (y > gb || y + kNoteH < kTop) continue;
            g.setColour (isBlack (p) ? bgGridAlt : bgGrid); g.fillRect ((float) kKeyW, y, (float) W, (float) kNoteH);
            if (scaleRoot() >= 0) { const int k = scaleKind (p); if (k == 0) { g.setColour (bgBar.withAlpha (.5f)); g.fillRect ((float) kKeyW, y, (float) W, (float) kNoteH); } else { g.setColour (accent.withAlpha (k == 2 ? .16f : .05f)); g.fillRect ((float) kKeyW, y, (float) W, (float) kNoteH); } }
            g.setColour (juce::Colour (0xff1a1b1f)); g.fillRect ((float) kKeyW, y + kNoteH - 1.f, (float) W, 1.f);
            if (p % 12 == 0) { g.setColour (barLine); g.fillRect ((float) kKeyW, y + kNoteH - 1.f, (float) W, 1.f); }
        }
        const float xe = bx (clipLenBeats());   // fuera del clip: color sólido más oscuro
        g.setColour (bgBar.withAlpha (.78f)); g.fillRect (xe, (float) kTop, (float) W, (float) (gb - kTop));
        const double step = ppb * quantum >= 9 ? quantum : (ppb >= 9 ? 1.0 : 4.0);
        for (double b = std::floor (xb ((float) kKeyW) / step) * step; bx (b) < W; b += step)
        {
            const bool bar = std::fmod (std::round (b * 1000) / 1000, timesig::bar()) == 0.0, beat = std::fmod (std::round (b * 1000) / 1000, 1.0) == 0.0;
            g.setColour (bar ? juce::Colour (0xff454851) : (beat ? barLine.withAlpha (.55f) : gridLine.withAlpha (.8f)));
            g.fillRect (bx (b), (float) kTop, 1.f, (float) (gb - kTop));
        }
        for (auto& gn : ghostNotes())
        {
            juce::Rectangle<float> r (bx (gn.start), ny (gn.pitch) + 2.f, std::max (3.f, (float) (gn.len * ppb) - 1.f), (float) kNoteH - 4.f);
            g.setColour (textDim.withAlpha (.14f)); g.fillRect (r); g.setColour (textDim.withAlpha (.35f)); g.drawRect (r, 1.f);
        }
        if (hoverPitch >= 0 && hover.x >= kKeyW) { g.setColour (juce::Colours::white.withAlpha (.035f)); g.fillRect ((float) kKeyW, ny (hoverPitch), (float) W, (float) kNoteH); }
        for (auto* n : clip->getSequence().getNotes()) drawNote (g, *n);
        if (cutShown && tool == "cut" && cutGX >= kKeyW)
        {
            const juce::Colour blue = accentHi; const float x = cutGX, w = 1.4f + juce::jmin (2.5f, std::abs (cutGV) / 500.f), y0 = (float) kTop, hh = (float) (gb - kTop);
            if (std::abs (cutGV) > 30.f) { const float tail = juce::jlimit (-60.f, 60.f, -cutGV * .045f); g.setColour (blue.withAlpha (.16f)); g.fillRect (std::min (x, x + tail), y0, std::abs (tail), hh); }
            g.setColour (blue.withAlpha (.2f)); g.fillRect (x - w * 3.f, y0, w * 6.f, hh); g.setColour (blue.withAlpha (.45f)); g.fillRect (x - w * 1.5f, y0, w * 3.f, hh); g.setColour (juce::Colours::white); g.fillRect (x - w * .5f, y0, w, hh);
        }
        if (mode == Mode::rubber && ! rubber.isEmpty()) { g.setColour (accent.withAlpha (.12f)); g.fillRect (rubber); g.setColour (accent); g.drawRect (rubber, 1.f); }
        g.restoreState();

        if (hoverPitch >= 0 && hover.x >= kKeyW && (tool == "pencil" || tool == "brush")) { auto lb = juce::Rectangle<float> (hover.x + 14.f, hover.y - 26.f, 42.f, 20.f); g.setColour (boxFill.withAlpha (.95f)); g.fillRoundedRectangle (lb, rad); g.setColour (boxLine); g.drawRoundedRectangle (lb.reduced (.5f), rad, 1.f); g.setColour (text); g.setFont (fonts::digits (12.f)); g.drawText (pitchName (hoverPitch), lb.toNearestInt(), juce::Justification::centred); }
        drawRuler (g, W); drawKeyboard (g, gb); if (kVelH > 0) { if (lane == 0) drawVelocity (g, W, H, gb); else drawLane (g, W, H, gb); icons::draw (g, icons::Chevron, laneLabelR().withX ((float) kKeyW - 26.f).withWidth (22.f), theme::textDim); }

        const double hb = timeToBeats (edit.getTransport().getPosition().inSeconds()) - clipStartBeat();
        const float px = bx (hb);
        if (hb >= 0 && px >= kKeyW + 8 && px <= W) { g.setColour (accentHi.withAlpha (.9f)); g.fillRect (px - .5f, (float) kBarH + 2.f, 1.f, (float) (H - kBarH - 2)); { juce::Path fl; const float fy = (float) kBarH + kRulerH - 9.f; fl.addTriangle (px - 5.f, fy, px + 5.f, fy, px, fy + 9.f); g.setColour (accentHi); g.fillPath (fl); } }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        if (e.y < kBarH) { toolbarClick (e); return; }
        if (! clip) return;
        if (e.y >= gridBottom()) { if (e.x < kKeyW) { laneMenu(); return; } if (lane == 0) { velocityAt (e); return; } laneDown (e); return; }
        if (e.x < kKeyW || e.y < kTop)
        {
            if (e.y < kTop && e.x > kKeyW) seek (e.x);
            else if (e.x < kKeyW) { litKey = yp ((float) e.y); audition (litKey); repaint(); }
            return;
        }
        auto* hit = noteAt (e.position); mode = Mode::none; grp.clear(); newNote = false; downPos = e.position;
        auto* um = &edit.getUndoManager();
        if (e.mods.isPopupMenu() || tool == "erase")   // clic derecho (o borrador): borra; arrastrando borra varias
        {
            um->beginNewTransaction (juce::String::fromUTF8 ("Borrar notas")); mode = Mode::erase; if (hit) removeNote (hit, false); return;
        }
        if (tool == "cut") { if (hit) splitNote (hit, q (xb ((float) e.x))); return; }
        if (tool == "brush") { um->beginNewTransaction (juce::String::fromUTF8 ("Pintar notas")); mode = Mode::paint; if (! hit) paintNote (e.position); return; }
        const bool add = e.mods.isShiftDown() || e.mods.isCtrlDown() || e.mods.isCommandDown();
        if (hit == nullptr && (tool == "select" || add))   // rectángulo de selección
        {
            if (! add) clearSel (); rubBase = selSet; rubA = e.position; rubber = { e.position, e.position }; mode = Mode::rubber; repaint(); return;
        }
        if (hit == nullptr)   // lápiz: una nota nueva (conserva la última longitud)
        {
            const double start = std::max (0.0, q (xb ((float) e.x)));
            if (start >= 4096.0) return; growTo (start);
            um->beginNewTransaction (juce::String::fromUTF8 ("Añadir nota"));
            auto* made = clip->getSequence().addNote (juce::jlimit (0, 127, yp ((float) e.y)), te::BeatPosition::fromBeats (start), te::BeatDuration::fromBeats (lastLen), lastVel, 0, um);
            clearSel(); sel = made; addSel (made); if (sel == nullptr) return; mode = Mode::resize; newNote = true; anchor = start; audition (sel->getNoteNumber()); repaint(); return;
        }
        if (add) { toggleSel (hit); sel = isSel (*hit) ? hit : nullptr; repaint(); return; }
        if (! isSel (*hit)) { clearSel(); addSel (hit); }
        sel = hit; audition (hit->getNoteNumber());
        const float rx = bx (hit->getEndBeat().inBeats());
        mode = e.x > rx - 7 ? Mode::resize : Mode::move; anchor = hit->getStartBeat().inBeats();
        grab = xb ((float) e.x) - anchor; um->beginNewTransaction ("Editar nota"); lastLen = hit->getLengthBeats().inBeats();
        for (auto* n : selNotes()) grp.push_back ({ n->state, n->getStartBeat().inBeats(), n->getLengthBeats().inBeats(), n->getNoteNumber() });
        anchorPitch = hit->getNoteNumber(); anchorEnd = hit->getEndBeat().inBeats();
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! clip) return;
        if (mode == Mode::lane) { laneDrag (e); return; }
        hover = e.position; if (e.y >= kTop && e.y < gridBottom()) hoverPitch = yp ((float) e.y);
        if (litKey >= 0 && e.getMouseDownX() < kKeyW) { const int k = yp ((float) e.y); if (k != litKey && k >= 0 && k <= 127) { litKey = k; audition (k); repaint(); } return; }
        if (e.mouseWasDraggedSinceMouseDown() && e.y >= gridBottom() && mode == Mode::none) { velocityAt (e); return; }
        auto* um = &edit.getUndoManager();
        if (mode == Mode::erase) { if (auto* h = noteAt (e.position)) removeNote (h, false); return; }
        if (mode == Mode::paint) { paintNote (e.position); return; }
        if (mode == Mode::rubber)
        {
            rubber = juce::Rectangle<float> (rubA, e.position); selSet = rubBase;
            for (auto* n : clip->getSequence().getNotes())
            {
                juce::Rectangle<float> r (bx (n->getStartBeat().inBeats()), ny (n->getNoteNumber()), (float) (n->getLengthBeats().inBeats() * ppb), (float) kNoteH);
                if (r.intersects (rubber) && ! isSel (*n)) selSet.push_back (n->state);
            }
            repaint(); return;
        }
        if (sel == nullptr) return;
        if (mode == Mode::move)
        {
            double dB = std::max (0.0, q (xb ((float) e.x) - grab)) - anchor; int dP = juce::jlimit (0, 127, yp ((float) e.y)) - anchorPitch;
            for (auto& o : grp) { dB = std::max (dB, -o.start); dP = juce::jlimit (-o.pitch, 127 - o.pitch, dP); }
            for (auto& o : grp) if (auto* n = noteOf (o.st))
            {
                if (n->getNoteNumber() != o.pitch + dP) n->setNoteNumber (o.pitch + dP, um);
                n->setStartAndLength (te::BeatPosition::fromBeats (o.start + dB), n->getLengthBeats(), um); growTo (o.start + dB + o.len - 0.25);
            }
            if (anchorPitch + dP != lastAudPitch) { lastAudPitch = anchorPitch + dP; audition (lastAudPitch); }
        }
        else if (mode == Mode::resize)
        {
            const double minLen = snap ? quantum : 0.05;
            if (newNote)
            {
                const double s0 = sel->getStartBeat().inBeats(), qe = q (xb ((float) e.x));
                const double len = (std::abs (e.x - downPos.x) < 4.f) ? lastLen : std::max (minLen, qe - s0);
                sel->setStartAndLength (sel->getStartBeat(), te::BeatDuration::fromBeats (len), um); lastLen = len; growTo (s0 + len - 0.25);
            }
            else
            {
                const double dEnd = q (xb ((float) e.x)) - q (anchorEnd);
                for (auto& o : grp) if (auto* n = noteOf (o.st)) { n->setStartAndLength (n->getStartBeat(), te::BeatDuration::fromBeats (std::max (minLen, o.len + dEnd)), um); growTo (n->getEndBeat().inBeats() - 0.25); }
                lastLen = std::max (minLen, grp.empty() ? lastLen : grp.front().len + dEnd);
            }
        }
        repaint();
    }
    void growTo (double beat)   // escribir más allá del final alarga el clip por compases
    {
        const double need = std::ceil ((beat + 0.25) / timesig::bar()) * timesig::bar();
        if (clip && need > clipLenBeats() + 1e-6) clip->setEnd (te::TimePosition::fromSeconds (beatsToTime (clipStartBeat() + need)), true);
    }
    void paintNote (juce::Point<float> pos)
    {
        if (pos.x < kKeyW || pos.y < kTop || pos.y >= gridBottom()) return;
        const double len = snap ? quantum : 0.25, start = std::floor (xb (pos.x) / len) * len; const int pitch = juce::jlimit (0, 127, yp (pos.y));
        if (start < 0 || start >= 4096.0) return; growTo (start);
        for (auto* n : clip->getSequence().getNotes()) if (n->getNoteNumber() == pitch && n->getStartBeat().inBeats() < start + len - 1e-6 && n->getEndBeat().inBeats() > start + 1e-6) return;
        sel = clip->getSequence().addNote (pitch, te::BeatPosition::fromBeats (start), te::BeatDuration::fromBeats (len), lastVel, 0, &edit.getUndoManager()); repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (mode != Mode::none && mode != Mode::rubber && onClipChanged) onClipChanged();
        mode = Mode::none; litKey = -1; lastAudPitch = -1; rubber = {}; repaint();
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (e.mods.isAltDown() || e.x < kKeyW) { zoomV (e.y, w.deltaY); repaint(); return; }
        if (e.y >= kBarH && e.y < kTop) { zoomH ((float) e.x, w.deltaY); repaint(); return; }   // rueda sobre la regla = zoom horizontal
        if (e.mods.isCtrlDown() || e.mods.isCommandDown()) { const double a = xb ((float) e.x); ppb = juce::jlimit (6.0, 400.0, ppb * (1.0 + w.deltaY * .6)); scrollX = juce::jmax (0.0, a * ppb - (e.x - kKeyW - 8)); }
        else if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY)) scrollX = juce::jmax (0.0, scrollX - (w.deltaX != 0 ? w.deltaX : w.deltaY) * 600.0);
        else scrollY = juce::jlimit (0.0, (double) (128 * kNoteH - (gridBottom() - kTop)), scrollY - w.deltaY * 400.0);
        repaint();
    }
    void zoomH (float x, double d) { const double base = ppbTarget > 0.f ? (double) ppbTarget : ppb; ppbTarget = (float) juce::jlimit (6.0, 400.0, base * (1.0 + d * .6)); ppbAnchor = x; }
    void zoomV (float y, double d)
    {
        const double a = (y - kTop + scrollY) / kNoteH; kNoteH = juce::jlimit (8, 48, (int) std::lround (kNoteH * (1.0 + d * .5) + (d > 0 ? 1 : -1)));
        scrollY = juce::jlimit (0.0, (double) std::max (0, 128 * kNoteH - (gridBottom() - kTop)), a * kNoteH - (y - kTop));
    }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (! clip) return false;
        { const int kc = k.getKeyCode(); const bool ctl = k.getModifiers().isCtrlDown() || k.getModifiers().isCommandDown();
          if (ctl && (kc == '+' || kc == '=' || kc == '-' || kc == '_')) { const double d = (kc == '+' || kc == '=') ? .25 : -.25; if (k.getModifiers().isAltDown()) zoomV ((float) (kTop + (gridBottom() - kTop) / 2), d); else zoomH ((float) (kKeyW + (getWidth() - kKeyW) / 2), d); repaint(); return true; }
          if (! ctl && ! audition::typingPiano && (kc == 'L' || kc == 'l')) { kVelH = kVelH > 0 ? 0 : 90; repaint(); return true; } }
        const bool ctrl = k.getModifiers().isCtrlDown() || k.getModifiers().isCommandDown(), shift = k.getModifiers().isShiftDown(); const int c = k.getKeyCode();
        if (ctrl && (c == 'A' || c == 'a')) { selSet.clear(); for (auto* n : clip->getSequence().getNotes()) selSet.push_back (n->state); repaint(); return true; }
        if (ctrl && (c == 'C' || c == 'c')) { copySel(); return true; }
        if (ctrl && (c == 'X' || c == 'x')) { copySel(); deleteSel(); return true; }
        if (ctrl && (c == 'V' || c == 'v')) { pasteAt (-1.0); return true; }
        if (ctrl && (c == 'D' || c == 'd')) { duplicateSel(); return true; }
        if (c == juce::KeyPress::escapeKey) { clearSel(); repaint(); return true; }
        if (c == juce::KeyPress::deleteKey || c == juce::KeyPress::backspaceKey) { if (selSet.empty() && sel != nullptr) addSel (sel); deleteSel(); return true; }
        if (! ctrl && (c == juce::KeyPress::leftKey || c == juce::KeyPress::rightKey || c == juce::KeyPress::upKey || c == juce::KeyPress::downKey) && ! selSet.empty())
        {
            auto* um = &edit.getUndoManager(); um->beginNewTransaction ("Mover notas"); const bool horiz = c == juce::KeyPress::leftKey || c == juce::KeyPress::rightKey;
            const double step = shift ? 1.0 : quantum; const int semis = shift ? 12 : 1; int lastP = -1;
            for (auto* n : selNotes())
            {
                if (horiz) n->setStartAndLength (te::BeatPosition::fromBeats (std::max (0.0, n->getStartBeat().inBeats() + (c == juce::KeyPress::rightKey ? step : -step))), n->getLengthBeats(), um);
                else { n->setNoteNumber (juce::jlimit (0, 127, n->getNoteNumber() + (c == juce::KeyPress::upKey ? semis : -semis)), um); lastP = n->getNoteNumber(); }
            }
            if (lastP >= 0) audition (lastP);
            if (onClipChanged) onClipChanged(); repaint(); return true;
        }
        if (! ctrl && (c == '[' || c == ']') && ! selSet.empty())
        {   // [ ] acortan / alargan las notas elegidas un paso de la cuadrícula
            auto* um = &edit.getUndoManager(); um->beginNewTransaction ("Largo de notas"); for (auto* n : selNotes()) n->setStartAndLength (n->getStartBeat(), te::BeatDuration::fromBeats (std::max (quantum, n->getLengthBeats().inBeats() + (c == ']' ? quantum : -quantum))), um);
            if (onClipChanged) onClipChanged(); repaint(); return true;
        }
        if (! ctrl && ! audition::typingPiano && (c == 'M' || c == 'm')) { muteSel(); return true; }
        if (! ctrl && ! audition::typingPiano && (c == 'G' || c == 'g')) { clip->state.setProperty ("injGhost", ! ghostOn(), nullptr); repaint(); return true; }
        if (ctrl && c == '0') { fitView(); return true; }
        if (k == juce::KeyPress ('q') && ! audition::typingPiano) { quantizeNotes(); return true; }
        return false;
    }
    // --- selección múltiple y portapapeles (públicos para las pruebas)
    int numSelected() const { return (int) selSet.size(); }
    void selectAll() { if (! clip) return; selSet.clear(); for (auto* n : clip->getSequence().getNotes()) selSet.push_back (n->state); repaint(); }
    void deleteSel()
    {
        if (! clip) return; auto ns = selNotes(); if (ns.empty()) return; auto* um = &edit.getUndoManager(); um->beginNewTransaction (juce::String::fromUTF8 ("Borrar notas"));
        sel = nullptr; for (auto* n : ns) clip->getSequence().removeNote (*n, um); selSet.clear(); if (onClipChanged) onClipChanged(); repaint();
    }
    void copySel()
    {
        auto ns = selNotes(); if (ns.empty()) return; clipboard.clear(); double t0 = 1e9; for (auto* n : ns) t0 = std::min (t0, n->getStartBeat().inBeats());
        for (auto* n : ns) clipboard.push_back ({ n->getNoteNumber(), n->getStartBeat().inBeats() - t0, n->getLengthBeats().inBeats(), n->getVelocity() });
        if (onMessage) onMessage (juce::String (clipboard.size()) + " notas copiadas");
    }
    // Pega en la posición `beat` del clip (-1 = donde está el cabezal si cae dentro del clip, si no al principio).
    void pasteAt (double beat)
    {
        if (! clip || clipboard.empty()) return;
        if (beat < 0) { const double hb = timeToBeats (edit.getTransport().getPosition().inSeconds()) - clipStartBeat(); beat = (hb >= 0 && hb < clipLenBeats()) ? q (hb) : 0.0; }
        auto* um = &edit.getUndoManager(); um->beginNewTransaction ("Pegar notas"); selSet.clear(); double end = 0;
        for (auto& c : clipboard) { growTo (beat + c.start); if (auto* n = clip->getSequence().addNote (c.pitch, te::BeatPosition::fromBeats (beat + c.start), te::BeatDuration::fromBeats (c.len), c.vel, 0, um)) { selSet.push_back (n->state); sel = n; } end = std::max (end, c.start + c.len); }
        growTo (beat + end); if (onClipChanged) onClipChanged(); repaint();
    }
    void duplicateSel()
    {
        auto ns = selNotes(); if (ns.empty()) return; copySel(); double t0 = 1e9, t1 = 0; for (auto* n : ns) { t0 = std::min (t0, n->getStartBeat().inBeats()); t1 = std::max (t1, n->getEndBeat().inBeats()); }
        pasteAt (t1);
    }
    std::vector<te::MidiNote*> selNotes() const
    {
        std::vector<te::MidiNote*> out; if (! clip) return out;
        for (auto* n : clip->getSequence().getNotes()) if (isSel (*n)) out.push_back (n);
        return out;
    }

private:
    enum class Mode { none, move, resize, erase, paint, rubber, lane };
    struct Orig { juce::ValueTree st; double start, len; int pitch; };
    struct ND { int pitch; double start, len; int vel; };
    std::vector<juce::ValueTree> selSet, rubBase; std::vector<Orig> grp; std::vector<ND> clipboard; juce::Rectangle<float> rubber; juce::Point<float> rubA, downPos;
    bool newNote = false; int anchorPitch = 0, lastAudPitch = -1; double anchorEnd = 0;
    bool isSel (const te::MidiNote& n) const { for (auto& v : selSet) if (v == n.state) return true; return false; }
    void clearSel() { selSet.clear(); sel = nullptr; }
    void addSel (te::MidiNote* n) { if (n && ! isSel (*n)) selSet.push_back (n->state); }
    void toggleSel (te::MidiNote* n) { for (size_t i = 0; i < selSet.size(); ++i) if (selSet[i] == n->state) { selSet.erase (selSet.begin() + (long) i); return; } selSet.push_back (n->state); }
    te::MidiNote* noteOf (const juce::ValueTree& v) const { if (clip) for (auto* n : clip->getSequence().getNotes()) if (n->state == v) return n; return nullptr; }
    void audition (int pitch) { if (onAudition && clip && pitch >= 0) if (auto* t = dynamic_cast<te::AudioTrack*> (clip->getTrack())) onAudition (t, pitch); }
    te::Edit& edit; te::MidiClip::Ptr clip; te::MidiNote* sel = nullptr; Mode mode = Mode::none;
    float cutGX = -1.f, cutGV = 0.f, cutTarget = -1.f, ppbTarget = -1.f, ppbAnchor = 0.f; bool cutShown = false;
    double ppb = 50.0, scrollX = 0, scrollY = 0, anchor = 0, grab = 0, lastLen = 0.25; int lastVel = 100, litKey = -1;

    // ajusta la vista para ver todas las notas del clip (como "Focus" en FL Studio)
    void fitView()
    {
        if (! clip) return; auto ns = clip->getSequence().getNotes(); if (ns.isEmpty()) return; int lo = 127, hi = 0; double e = 0;
        for (auto* n : ns) { lo = std::min (lo, n->getNoteNumber()); hi = std::max (hi, n->getNoteNumber()); e = std::max (e, n->getEndBeat().inBeats()); }
        const int rows = hi - lo + 5; kNoteH = juce::jlimit (8, 48, (gridBottom() - kTop) / std::max (6, rows));
        scrollY = juce::jlimit (0.0, (double) std::max (0, 128 * kNoteH - (gridBottom() - kTop)), (double) ((127 - (hi + 2)) * kNoteH));
        ppbTarget = (float) juce::jlimit (6.0, 400.0, (getWidth() - kKeyW - 40.0) / std::max (1.0, e)); ppbAnchor = (float) kKeyW + 8.f; scrollX = 0; repaint();
    }
    static juce::String pitchName (int p) { static const char* n[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return juce::String (n[((p % 12) + 12) % 12]) + juce::String (p / 12 - 1); }
    int hoverPitch = -1;
    static bool isBlack (int p) { switch (p % 12) { case 1: case 3: case 6: case 8: case 10: return true; default: return false; } }
    void timerCallback() override
    {
        if (loopSel && ! edit.getTransport().isPlaying()) stopLoopSel();
        if (! isVisible()) return;
        const double pos = edit.getTransport().getPosition().inSeconds();
        if (pos != lastPos) { lastPos = pos; repaint(); }
        if (cutShown && tool == "cut" && cutTarget >= 0.f)   // la guía de corte es un resorte: se desliza y se asienta
        {
            if (cutGX < 0.f) { cutGX = cutTarget; cutGV = 0.f; }
            else { const float dt = 1.f / 60.f, k = 520.f, c = 2.f * std::sqrt (k) * 0.55f; cutGV += (k * (cutTarget - cutGX) - c * cutGV) * dt; cutGX += cutGV * dt; if (std::abs (cutTarget - cutGX) < .25f && std::abs (cutGV) < 4.f) { cutGX = cutTarget; cutGV = 0.f; } else repaint(); }
        }
        if (ppbTarget > 0.f)   // zoom horizontal suave
        {
            const double a = xb (ppbAnchor); ppb += (ppbTarget - ppb) * .32; if (std::abs (ppb - ppbTarget) < .05) { ppb = ppbTarget; ppbTarget = -1.f; }
            scrollX = juce::jmax (0.0, a * ppb - (ppbAnchor - kKeyW - 8)); repaint();
        }   // el cabezal solo se mueve con la reproducci\xc3\xb3n o al hacer seek
    }
    double lastPos = -1.0;
    void seek (int x) { auto b = std::max (0.0, xb ((float) x)) + clipStartBeat(); edit.getTransport().setPosition (te::TimePosition::fromSeconds (beatsToTime (q (b)))); }
    void removeNote (te::MidiNote* n, bool tx = true) { if (tx) edit.getUndoManager().beginNewTransaction ("Borrar nota"); if (sel == n) sel = nullptr; for (size_t i = 0; i < selSet.size(); ++i) if (selSet[i] == n->state) { selSet.erase (selSet.begin() + (long) i); break; } clip->getSequence().removeNote (*n, &edit.getUndoManager()); if (onClipChanged) onClipChanged(); repaint(); }

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
        const float top = (float) gridBottom() + 34.f, bot = (float) getHeight() - 10.f; const int v = juce::jlimit (1, 127, (int) std::round (127.0 * (bot - e.y) / (bot - top)));
        edit.getUndoManager().beginNewTransaction ("Velocity"); best->setVelocity (v, &edit.getUndoManager()); lastVel = v; sel = best; repaint();
        if (onClipChanged) onClipChanged();
    }

    void drawNote (juce::Graphics& g, te::MidiNote& n)
    {
        using namespace theme;
        juce::Rectangle<float> r (bx (n.getStartBeat().inBeats()), ny (n.getNoteNumber()) + 1.f, std::max (3.f, (float) (n.getLengthBeats().inBeats() * ppb) - 1.f), (float) kNoteH - 2.f);
        const bool s = isSel (n); const float vel = juce::jlimit (0.f, 1.f, n.getVelocity() / 127.f);
        const juce::Colour tc = hostColour();
        const juce::Colour base = s ? accent.interpolatedWith (juce::Colours::white, .12f) : tc.darker (.55f).interpolatedWith (tc, vel * .8f + .1f);
        const bool mu = n.isMute(); if (mu) g.setColour (textFaint.withAlpha (s ? .9f : .55f)); else g.setGradientFill (juce::ColourGradient (base.brighter (.05f), 0.f, r.getY(), base.darker (.05f), 0.f, r.getBottom(), false)); g.fillRoundedRectangle (r, rad);
        g.setColour (juce::Colours::white.withAlpha (.06f)); g.fillRect (r.getX() + 1.f, r.getY() + 1.f, std::max (0.f, r.getWidth() - 2.f), 1.f);
        g.setColour (juce::Colours::black.withAlpha (.45f)); g.drawRoundedRectangle (r.reduced (.5f), rad, 1.f);
        if (s) { g.setColour (accentHi); g.drawRoundedRectangle (r.reduced (.5f), rad, 1.4f); }
        if ((bool) n.state.getProperty ("injSlide", false)) { juce::Path tp; tp.addTriangle (r.getX() + 1.f, r.getY() + 1.f, r.getX() + 9.f, r.getY() + 1.f, r.getX() + 1.f, r.getBottom() - 1.f); g.setColour (juce::Colours::white.withAlpha (.9f)); g.fillPath (tp); }
        if (mu) { g.saveState(); g.reduceClipRegion (r.toNearestInt()); g.setColour (juce::Colours::black.withAlpha (.35f)); for (float xx = r.getX() - r.getHeight(); xx < r.getRight(); xx += 6.f) g.drawLine (xx, r.getBottom(), xx + r.getHeight(), r.getY(), 1.f); g.restoreState(); }
        if (r.getWidth() >= 28.f && kNoteH >= 14) { g.setColour (juce::Colours::black.withAlpha (.7f)); g.setFont (fonts::ui (juce::jmin (11.f, kNoteH - 6.f), true)); g.drawText (pitchName (n.getNoteNumber()), r.reduced (4.f, 0.f), juce::Justification::centredLeft, false); }
    }
    void drawRuler (juce::Graphics& g, int W)
    {
        g.setColour (theme::bgBar); g.fillRect (kKeyW, kBarH, W, kRulerH);
        g.setColour (theme::seam); g.fillRect (kKeyW, kTop - 1, W, 1);
        g.setFont (theme::fonts::digits (11.f));
        for (int bt = 0; ; ++bt) { const float tx = bx ((double) bt); if (tx > W) break; if (tx < kKeyW + 4) continue; if (ppb >= 20.0) { g.setColour (theme::barLine); g.fillRect (tx, (float) kBarH + 21.f, 1.f, 7.f); } }
        for (int bar = 0; ; ++bar)
        {
            const float x = bx (bar * timesig::bar()); if (x > W) break; if (x < kKeyW + 4) continue;
            g.setColour (juce::Colour (0xff575a64)); g.fillRect (x, (float) kBarH + 5.f, 1.f, (float) kRulerH - 5.f);
            g.setColour (theme::textDim); g.drawText (juce::String (bar + 1), juce::Rectangle<float> (x + 6, (float) kBarH + 4.f, 40, 20).toNearestInt(), juce::Justification::centredLeft);
        }
        g.setColour (theme::bgBar); g.fillRect (0, kBarH, kKeyW, kRulerH);
        g.setColour (theme::seam); g.fillRect (kKeyW - 1, kBarH, 1, kRulerH);
    }
    void drawKeyboard (juce::Graphics& g, int gb)
    {
        g.saveState(); g.reduceClipRegion (0, kTop, kKeyW, gb - kTop);
        g.setColour (theme::bgBar); g.fillRect (0, kTop, kKeyW, gb - kTop);
        for (int p = 0; p <= 127; ++p)
        {
            const float y = ny (p); if (y > gb || y + kNoteH < kTop || isBlack (p)) continue;
            g.setColour (p == litKey ? theme::accentHi : (p == hoverPitch ? juce::Colour (0xfff3e6d6) : juce::Colour (0xffe4e5e8))); g.fillRect (6.f, y, (float) kKeyW - 10, (float) kNoteH - 1.f);
            g.setColour (juce::Colour (0xffa4a7ae)); g.fillRect (6.f, y + kNoteH - 1.f, (float) kKeyW - 10, 1.f);
            if (p % 12 == 0 || p == hoverPitch) { g.setColour (juce::Colour (0xff3a3d46)); g.setFont (theme::fonts::digits (11.f)); g.drawText (pitchName (p), juce::Rectangle<float> (6.f, y, (float) kKeyW - 14, (float) kNoteH).toNearestInt(), juce::Justification::centredRight); }
        }
        for (int p = 0; p <= 127; ++p)
        {
            const float y = ny (p); if (y > gb || y + kNoteH < kTop || ! isBlack (p)) continue;
            { const juce::Rectangle<float> bk (6.f, y + 1, 58.f, (float) kNoteH - 2.f); if (p == litKey) g.setColour (theme::accentDim); else g.setGradientFill (juce::ColourGradient (p == hoverPitch ? juce::Colour (0xff3d4048) : juce::Colour (0xff2c2e35), bk.getX(), 0.f, juce::Colour (0xff0e0f12), bk.getRight(), 0.f, false)); g.fillRoundedRectangle (bk, theme::rad); }
            if (p == hoverPitch) { g.setColour (theme::text); g.setFont (theme::fonts::digits (11.f)); g.drawText (pitchName (p), juce::Rectangle<float> (6.f, y, 52.f, (float) kNoteH).toNearestInt(), juce::Justification::centredRight); }
            g.setColour (juce::Colour (0xff050506)); g.drawRoundedRectangle (6.5f, y + 1.5f, 57.f, (float) kNoteH - 3.f, theme::rad, 1.f);
        }
        g.restoreState();
    }
    juce::Colour hostColour() const { juce::Colour tc (0xffffa070); if (clip) if (auto* at = dynamic_cast<te::AudioTrack*> (clip->getTrack())) { int ix = 0; for (auto* t2 : te::getAudioTracks (clip->edit)) { if (t2 == at) break; ++ix; } tc = trackutil::colourOf (at, ix); } return tc; }
    void drawVelocity (juce::Graphics& g, int W, int H, int gb)
    {
        g.setColour (theme::seam); g.fillRect (0, gb, W, 1);
        g.setColour (theme::boxFill); g.fillRect (kKeyW, gb + 1, W - kKeyW, kVelH - 1);
        g.setColour (theme::bgBar); g.fillRect (0, gb + 1, kKeyW, kVelH - 1);
        g.setColour (theme::textDim); g.setFont (theme::fonts::label (13.f)); g.drawText ("Velocity", 12, gb + 6, kKeyW - 12, 22, juce::Justification::centredLeft);
        const float top = (float) gb + 34.f, bot = (float) H - 10.f;   // escala 127 .. 0
        g.setFont (theme::fonts::digits (10.f));
        for (int v : { 127, 96, 64, 32, 0 })
        {
            const float y = bot - (bot - top) * (float) v / 127.f;
            g.setColour (theme::textDim); g.drawText (juce::String (v), juce::Rectangle<float> (4, y - 8, (float) kKeyW - 22, 16).toNearestInt(), juce::Justification::centredRight); g.fillRect ((float) kKeyW - 14, y, 8.f, 1.f);
            g.setColour (theme::gridLine); g.fillRect ((float) kKeyW, y, (float) (W - kKeyW), 1.f);
        }
        g.saveState(); g.reduceClipRegion (kKeyW, gb + 1, W - kKeyW, kVelH);
        for (auto* n : clip->getSequence().getNotes())
        {
            const float x = bx (n->getStartBeat().inBeats()), y = bot - (bot - top) * (float) n->getVelocity() / 127.f; const bool s = isSel (*n);
            g.setColour (s ? theme::accentHi : hostColour()); g.fillRect (x - .5f, y, 1.f, bot - y); g.fillRect (x - 3.f, y - 1.f, 7.f, 3.f);
        }
        g.restoreState();
    }

    //---- barra de herramientas propia del Piano Roll
    struct Btn { juce::String id, tip; icons::Id icon; juce::Rectangle<float> r; };
    std::vector<Btn> btns() const
    {
        std::vector<Btn> v; float x = 200.f; const float y = 8.f;
        auto add = [&] (const char* id, const char* tip, icons::Id ic) { v.push_back ({ id, juce::String::fromUTF8 (tip), ic, { x, y, 40, 36 } }); x += 44; };
        add ("pencil", "Lápiz: clic para poner una nota, arrastra para moverla; clic derecho la borra", icons::Pencil); add ("brush", "Pincel: arrastra para pintar varias notas", icons::Brush); add ("select", "Seleccionar: arrastra un rectángulo para elegir varias notas (Ctrl+A todas)", icons::Select); add ("erase", "Borrador", icons::Eraser); add ("cut", "Cortar una nota en dos", icons::Scissors);
        x += 10; v.push_back ({ "quant", juce::String::fromUTF8 ("Cuantización"), icons::Chevron, { x, y, 92, 36 }, }); x += 100;
        v.push_back ({ "scale", "Escala", icons::Chevron, { x, y, 132, 36 } }); x += 148;
        add ("tools", "Más: importar y exportar MIDI, humanizar, cuantizar, acordes, arpegio, legato, voltear...", icons::Dots);
        return v;
    }
    juce::String quantLabel() const { return snap ? (quantum >= 1.0 ? "1/4" : quantum >= .5 ? "1/8" : quantum >= .25 ? "1/16" : "1/32") : juce::String ("Libre"); }
    void drawToolbar (juce::Graphics& g, int W)
    {
        using namespace theme;
        g.setColour (bgBar); g.fillRect (0, 0, W, kBarH);
        g.setColour (seam); g.fillRect (0, kBarH - 1, W, 1); g.setColour (edgeLight); g.fillRect (0, 0, W, 1);
        icons::draw (g, icons::PianoRoll, { 6.f, 8.f, 36.f, 36.f }, iconIdle);
        const juce::String who = clip ? clip->getName() : juce::String ("sin clip");
        g.setColour (text); g.setFont (fonts::label (13.f)); g.drawText (who, juce::Rectangle<float> (44, 8, 290, 36).toNearestInt(), juce::Justification::centredLeft);
        for (auto& b : btns())
        {
            const bool active = b.id == tool, hot = b.r.contains (hover);
            if (active || hot || b.id == "quant" || b.id == "scale") { g.setColour (active ? accent.withAlpha (.2f) : (hot ? panelHi : boxFill)); g.fillRoundedRectangle (b.r, rad); g.setColour (active ? accent : boxLine); g.drawRoundedRectangle (b.r.reduced (.5f), rad, 1.f); }
            if (b.id == "quant") { g.setColour (text); g.setFont (fonts::digits (13.f)); g.drawText (quantLabel(), b.r.withTrimmedRight (22).toNearestInt(), juce::Justification::centred); icons::draw (g, icons::Chevron, { b.r.getRight() - 28.f, b.r.getY(), 26.f, b.r.getHeight() }, textDim); }
            else if (b.id == "scale") { g.setColour (scaleRoot() >= 0 ? accentHi : text); g.setFont (fonts::label (13.f)); g.drawText (scaleLabel(), b.r.withTrimmedRight (22).toNearestInt(), juce::Justification::centred); icons::draw (g, icons::Chevron, { b.r.getRight() - 28.f, b.r.getY(), 26.f, b.r.getHeight() }, textDim); }
            else icons::draw (g, b.icon, b.r, active ? accentHi : iconIdle);
        }
    }
    void toolbarClick (const juce::MouseEvent& e)
    {
        for (auto& b : btns()) if (b.r.contains (e.position))
        {
            if (b.id == "select" || b.id == "pencil" || b.id == "brush" || b.id == "erase" || b.id == "cut") { tool = b.id; repaint(); }
            else if (b.id == "quant") quantMenu (b.r); else if (b.id == "scale") scaleMenu (b.r);
            else if (b.id == "import") importMidi(); else if (b.id == "export") exportMidi(); else if (b.id == "humanize") humanize(); else if (b.id == "quantize") quantizeNotes(); else if (b.id == "tools") toolsMenu (b.r);
            return;
        }
    }
    // ---- escala resaltada y notas de otros clips (se guardan en el clip) ----
    static constexpr const char* kNoteNames[12] { "Do", "Do#", "Re", "Re#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" };
    struct ScaleDef { const char* name; std::vector<int> iv; };
    static const std::vector<ScaleDef>& scaleDefs()
    {
        static const std::vector<ScaleDef> d { { "Mayor", { 0, 2, 4, 5, 7, 9, 11 } }, { "Menor", { 0, 2, 3, 5, 7, 8, 10 } }, { "Menor armónica", { 0, 2, 3, 5, 7, 8, 11 } },
            { "Pentatónica menor", { 0, 3, 5, 7, 10 } }, { "Frigia", { 0, 1, 3, 5, 7, 8, 10 } }, { "Frigia dominante", { 0, 1, 4, 5, 7, 8, 10 } }, { "Dórica", { 0, 2, 3, 5, 7, 9, 10 } }, { "Cromática", { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } } };
        return d;
    }
    bool ownScale() const { return clip != nullptr && clip->state.hasProperty ("injScaleRoot"); }   // si el clip no tiene escala propia se usa la tonalidad del proyecto
    int scaleRoot() const { if (! clip) return -1; if (ownScale()) return (int) clip->state.getProperty ("injScaleRoot", -1); const auto k = harmony::getKey (edit); return k.active() ? k.root : -1; }
    int scaleType() const { return clip ? (int) clip->state.getProperty ("injScaleType", 1) : 1; }
    bool ghostOn() const { return clip == nullptr || (bool) clip->state.getProperty ("injGhost", true); }
    juce::String scaleLabel() const { const int r = scaleRoot(); if (r >= 0 && ! ownScale()) return harmony::keyText (harmony::getKey (edit)); return r < 0 ? juce::String ("Escala") : juce::String::fromUTF8 (kNoteNames[r]) + " " + juce::String::fromUTF8 (scaleDefs()[(size_t) juce::jlimit (0, (int) scaleDefs().size() - 1, scaleType())].name).toLowerCase(); }
    // 0 = fuera de la escala, 1 = dentro, 2 = tónica
    int scaleKind (int pitch) const
    {
        const int r = scaleRoot(); if (r < 0) return 1; if (! ownScale()) { const auto k = harmony::getKey (edit); return harmony::isRoot (k, pitch) ? 2 : (harmony::inScale (k, pitch) ? 1 : 0); }
        const int pc = ((pitch - r) % 12 + 12) % 12;
        if (pc == 0) return 2; for (int i : scaleDefs()[(size_t) juce::jlimit (0, (int) scaleDefs().size() - 1, scaleType())].iv) if (i == pc) return 1; return 0;
    }
public:
    void setScale (int root, int type) { if (! clip) return; clip->state.setProperty ("injScaleRoot", root, nullptr); clip->state.setProperty ("injScaleType", type, nullptr); repaint(); }
    int scaleKindOf (int pitch) const { return scaleKind (pitch); }
    void setGhost (bool on) { if (clip) { clip->state.setProperty ("injGhost", on, nullptr); repaint(); } }
    // notas de otros clips MIDI que suenan a la vez, en beats de este clip
    struct Ghost { double start, len; int pitch; };
    std::vector<Ghost> ghostNotes() const
    {
        std::vector<Ghost> out; if (! clip || ! ghostOn()) return out;
        const double mine0 = clip->getStartBeat().inBeats(), mineLen = clipLenBeats();
        for (auto* t : te::getAudioTracks (edit)) for (auto* c : t->getClips())
        {
            auto* mc = dynamic_cast<te::MidiClip*> (c); if (mc == nullptr || mc == clip.get()) continue;
            const double g0 = mc->getStartBeat().inBeats(), g1 = g0 + mc->getLengthInBeats().inBeats(); if (g1 <= mine0 || g0 >= mine0 + mineLen) continue;
            for (auto* n : mc->getSequence().getNotes()) { const double st = g0 + n->getStartBeat().inBeats() - mine0; if (st + n->getLengthBeats().inBeats() > 0 && st < mineLen) out.push_back ({ st, n->getLengthBeats().inBeats(), n->getNoteNumber() }); if (out.size() > 4000) return out; }
        }
        return out;
    }
private:
    void scaleMenu (juce::Rectangle<float> r)
    {
        juce::PopupMenu m, roots, types; const int cr = scaleRoot(), ct = scaleType();
        for (int i = 0; i < 12; ++i) roots.addItem (100 + i, juce::String::fromUTF8 (kNoteNames[i]), true, cr == i);
        for (int i = 0; i < (int) scaleDefs().size(); ++i) types.addItem (200 + i, juce::String::fromUTF8 (scaleDefs()[(size_t) i].name), true, ct == i);
        m.addItem (1, "Sin escala", true, cr < 0); m.addSubMenu (juce::String::fromUTF8 ("Tónica"), roots); m.addSubMenu ("Tipo", types); m.addSeparator(); m.addItem (2, "Notas de otros clips", true, ghostOn());
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (r.toNearestInt())), [this] (int c)
        {
            if (c == 1) setScale (-1, scaleType()); else if (c == 2) setGhost (! ghostOn());
            else if (c >= 100 && c < 112) setScale (c - 100, scaleType()); else if (c >= 200 && c < 200 + (int) scaleDefs().size()) setScale (scaleRoot() < 0 ? 0 : scaleRoot(), c - 200);
        });
    }
    void quantMenu (juce::Rectangle<float> r)
    {
        juce::PopupMenu m; m.addItem (1, "1/4", true, snap && quantum == 1.0); m.addItem (2, "1/8", true, snap && quantum == .5); m.addItem (3, "1/16", true, snap && quantum == .25); m.addItem (4, "1/32", true, snap && quantum == .125); m.addSeparator(); m.addItem (5, juce::String::fromUTF8 ("Libre (sin imán)"), true, ! snap);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (r.toNearestInt())), [this] (int c)
        { if (c >= 1 && c <= 4) { quantum = c == 1 ? 1.0 : c == 2 ? .5 : c == 3 ? .25 : .125; snap = true; } else if (c == 5) snap = false; repaint(); });
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        hover = e.position; if (e.y < kBarH || hoverWas) repaint (0, 0, getWidth(), kBarH); hoverWas = e.y < kBarH;
        { const int hp = (clip && e.y >= kTop && e.y < gridBottom()) ? yp ((float) e.y) : -1; if (hp != hoverPitch || hp >= 0) { hoverPitch = hp; repaint(); } }
        cutShown = clip != nullptr && tool == "cut" && e.x >= kKeyW && e.y >= kTop && e.y < gridBottom(); if (cutShown) { cutTarget = bx (q (xb ((float) e.x))); repaint(); }
        juce::MouseCursor cur = juce::MouseCursor::NormalCursor;
        if (clip && e.y >= kTop && e.y < gridBottom())
        {
            if (e.x < kKeyW) cur = juce::MouseCursor::PointingHandCursor;
            else if (auto* h = noteAt (e.position)) cur = (tool != "erase" && tool != "cut" && tool != "brush" && e.x > bx (h->getEndBeat().inBeats()) - 7) ? juce::MouseCursor::LeftRightResizeCursor : (tool == "erase" || tool == "cut") ? cursors::forTool (tool) : juce::MouseCursor::NormalCursor;
            else cur = cursors::forTool (tool);
        }
        setMouseCursor (cur);
    }
    void mouseExit (const juce::MouseEvent&) override { hover = { -1, -1 }; hoverPitch = -1; if (cutShown) { cutShown = false; cutGX = -1.f; repaint(); } else repaint (0, 0, getWidth(), kBarH); }
public:
    juce::String getTooltip() override { for (auto& b : btns()) if (b.r.contains (hover)) return b.tip; return {}; }
    // --- acciones sobre el clip (públicas para las pruebas)

    // ---- carril de control: modulación (CC1) y pitch bend; el carril Velocity es el de siempre ----
    int lane = 0;   // 0 velocity, 1 modulación, 2 pitch bend (se guarda en el clip)
    bool laneErase = false; double lastLB = -1.0; int lastLV = -1;
    static constexpr double kLaneStep = 0.0625;
    int laneType() const { return lane == 2 ? (int) te::MidiControllerEvent::pitchWheelType : 1; }
    int laneMax() const { return lane == 2 ? 16383 : 127; }
    int laneRest() const { return lane == 2 ? 8192 : 0; }
    juce::Rectangle<float> laneLabelR() const { return { 8.f, (float) gridBottom() + 4.f, (float) kKeyW - 14.f, 24.f }; }
    void setLane (int l) { lane = juce::jlimit (0, 2, l); if (clip) clip->state.setProperty ("injLane", lane, nullptr); repaint(); }
public:
    int bendRange() const { return clip ? juce::jlimit (1, 48, (int) clip->state.getProperty ("injBendRange", 2)) : 2; }
    int laneNumber() const { return lane; }
    void selectNotesForTest (const std::vector<te::MidiNote*>& ns) { selSet.clear(); sel = nullptr; for (auto* n : ns) selSet.push_back (n->state); if (! ns.empty()) sel = ns.front(); }
    int selectedVelocityForTest() const { auto ns = selNotes(); return ns.empty() ? -1 : ns.front()->getVelocity(); }
    void showLane (int l) { setLane (l); }
    std::vector<te::MidiControllerEvent*> eventsOfType (int type) const
    {
        std::vector<te::MidiControllerEvent*> v; if (clip) for (auto* ce : clip->getSequence().getControllerEvents()) if (ce->getType() == type) v.push_back (ce);
        return v;
    }
    // pone un valor de la curva (crea el punto o cambia el que ya está en ese tiempo)
    void setCurvePoint (int type, double beat, int value)
    {
        if (! clip) return; auto* um = &edit.getUndoManager();
        for (auto* ce : eventsOfType (type)) if (std::abs (ce->getBeatPosition().inBeats() - beat) < 1e-4) { ce->setControllerValue (value, um); return; }
        clip->getSequence().addControllerEvent (te::BeatPosition::fromBeats (beat), type, value, um);
    }
    void clearCurve (int type, double from = -1.0, double to = 1e9)
    {
        if (! clip) return; auto* um = &edit.getUndoManager();
        for (auto* ce : eventsOfType (type)) { const double b = ce->getBeatPosition().inBeats(); if (b >= from - 1e-6 && b <= to + 1e-6) clip->getSequence().removeControllerEvent (*ce, um); }
    }
private:
    int laneValueAt (float y) const
    {
        const float top = (float) gridBottom() + 34.f, bot = (float) getHeight() - 10.f; const float f = juce::jlimit (0.f, 1.f, (bot - y) / (bot - top));
        int v = (int) std::lround (f * (float) laneMax()); if (lane == 2 && std::abs (f - .5f) < .04f) v = 8192; return v;
    }
    void laneDown (const juce::MouseEvent& e)
    {
        mode = Mode::lane; laneErase = e.mods.isPopupMenu(); lastLB = -1.0; lastLV = -1;
        edit.getUndoManager().beginNewTransaction (laneErase ? juce::String::fromUTF8 ("Borrar curva") : juce::String::fromUTF8 ("Dibujar curva")); laneDrag (e);
    }
    void laneDrag (const juce::MouseEvent& e)
    {
        if (! clip) return; const double step = kLaneStep;
        const double b = juce::jlimit (0.0, std::max (0.0, clipLenBeats() - step), std::floor (xb ((float) e.x) / step + 0.5) * step); const int v = laneValueAt ((float) e.y);
        const double a = lastLB < 0 ? b : lastLB; const int va = lastLV < 0 ? v : lastLV;
        if (laneErase) clearCurve (laneType(), std::min (a, b) - step * .5, std::max (a, b) + step * .5);
        else
        {
            const int n = std::max (1, (int) std::lround (std::abs (b - a) / step));
            for (int i = 0; i <= n; ++i) { const double f = (double) i / n; setCurvePoint (laneType(), std::floor ((a + (b - a) * f) / step + 0.5) * step, (int) std::lround (va + (v - va) * f)); }
        }
        lastLB = b; lastLV = v; repaint();
    }
    void drawLane (juce::Graphics& g, int W, int H, int gb)
    {
        g.setColour (theme::seam); g.fillRect (0, gb, W, 1);
        g.setColour (theme::boxFill); g.fillRect (kKeyW, gb + 1, W - kKeyW, kVelH - 1);
        g.setColour (theme::bgBar); g.fillRect (0, gb + 1, kKeyW, kVelH - 1);
        g.setColour (theme::textDim); g.setFont (theme::fonts::label (13.f)); g.drawText (lane == 2 ? "Pitch bend" : juce::String::fromUTF8 ("Modulación"), 10, gb + 6, kKeyW - 28, 22, juce::Justification::centredLeft);
        const float top = (float) gb + 34.f, bot = (float) H - 10.f; const float mx = (float) laneMax();
        auto yOf = [&] (int v) { return bot - (bot - top) * (float) v / mx; };
        g.setFont (theme::fonts::digits (10.f));
        if (lane == 2) { const int rg = bendRange(); const char* nm[] { "+", "", "-" }; const float ys[] { top, (top + bot) * .5f, bot };
            for (int k = 0; k < 3; ++k) { g.setColour (theme::textDim); g.drawText (k == 1 ? juce::String ("0") : juce::String (nm[k]) + juce::String (rg), juce::Rectangle<float> (4, ys[k] - 8, (float) kKeyW - 22, 16).toNearestInt(), juce::Justification::centredRight); g.fillRect ((float) kKeyW - 14, ys[k], 8.f, 1.f); g.setColour (k == 1 ? juce::Colour (0xff3a3a3a) : juce::Colour (0xff2a2a2a)); g.fillRect ((float) kKeyW, ys[k], (float) (W - kKeyW), 1.f); } }
        else for (int v : { 127, 96, 64, 32, 0 }) { const float y = yOf (v); g.setColour (theme::textDim); g.drawText (juce::String (v), juce::Rectangle<float> (4, y - 8, (float) kKeyW - 22, 16).toNearestInt(), juce::Justification::centredRight); g.fillRect ((float) kKeyW - 14, y, 8.f, 1.f); g.setColour (theme::gridLine); g.fillRect ((float) kKeyW, y, (float) (W - kKeyW), 1.f); }
        g.saveState(); g.reduceClipRegion (kKeyW, gb + 1, W - kKeyW, kVelH);
        auto evs = eventsOfType (laneType()); const juce::Colour hc = hostColour();
        if (evs.empty()) { g.setColour (theme::textDim.withAlpha (.7f)); g.setFont (juce::FontOptions (13.f)); g.drawText (juce::String::fromUTF8 ("Dibuja con el lápiz aquí; clic derecho borra"), kKeyW + 12, gb + 8, 360, 20, juce::Justification::centredLeft); }
        else
        {
            juce::Path pth; float lx = (float) kKeyW, ly = yOf (laneRest()); pth.startNewSubPath (lx, ly);
            for (auto* ce : evs) { const float x = bx (ce->getBeatPosition().inBeats()), y = yOf (ce->getControllerValue()); pth.lineTo (x, ly); pth.lineTo (x, y); ly = y; lx = x; }
            pth.lineTo ((float) W, ly); g.setColour (hc.withAlpha (.95f)); g.strokePath (pth, juce::PathStrokeType (1.6f));
            if (evs.size() < 160) for (auto* ce : evs) g.fillRect (bx (ce->getBeatPosition().inBeats()) - 2.5f, yOf (ce->getControllerValue()) - 2.5f, 5.f, 5.f);
        }
        g.restoreState();
    }
    void laneMenu()
    {
        juce::PopupMenu m, rg; m.addItem (1, "Velocity", true, lane == 0); m.addItem (2, juce::String::fromUTF8 ("Modulación (CC1)"), true, lane == 1); m.addItem (3, "Pitch bend", true, lane == 2);
        if (lane != 0) { m.addSeparator(); m.addItem (10, juce::String::fromUTF8 ("Borrar toda la curva")); }
        for (int r : { 2, 12, 24 }) rg.addItem (20 + r, juce::String::fromUTF8 ("\xc2\xb1") + juce::String (r) + " semitonos", true, bendRange() == r);
        m.addSubMenu (juce::String::fromUTF8 ("Rango de pitch bend de tu instrumento"), rg);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (laneLabelR().toNearestInt())), [this] (int c)
        {
            if (c >= 1 && c <= 3) setLane (c - 1); else if (c == 10) { auto* um = &edit.getUndoManager(); um->beginNewTransaction (juce::String::fromUTF8 ("Borrar curva")); clearCurve (laneType()); if (onClipChanged) onClipChanged(); repaint(); }
            else if (c >= 22 && c <= 44 && clip) { clip->state.setProperty ("injBendRange", c - 20, nullptr); repaint(); }
        });
    }
public:
    // ---- silenciar notas ----
    int muteSel()
    {
        if (! clip) return 0; auto ns = selNotes(); if (ns.empty()) { if (onMessage) onMessage (juce::String::fromUTF8 ("Elige notas primero")); return 0; }
        bool all = true; for (auto* n : ns) if (! n->isMute()) all = false;
        auto* um = &edit.getUndoManager(); um->beginNewTransaction (juce::String::fromUTF8 ("Silenciar notas")); for (auto* n : ns) n->setMute (! all, um);
        if (onClipChanged) onClipChanged(); repaint(); return (int) ns.size();
    }
    // ---- slide: la nota baja o sube desde el tono de la nota anterior con pitch bend ----
    int toggleSlide()
    {
        if (! clip) return 0; auto ns = selNotes(); if (ns.empty()) { if (onMessage) onMessage (juce::String::fromUTF8 ("Elige notas primero")); return 0; }
        auto* um = &edit.getUndoManager(); um->beginNewTransaction ("Slide"); int done = 0; const int type = te::MidiControllerEvent::pitchWheelType; const double step = kLaneStep;
        for (auto* n : ns)
        {
            const double s0 = n->getStartBeat().inBeats(), l0 = n->getLengthBeats().inBeats(); const double glide = std::min (l0, 0.5);
            clearCurve (type, s0 - 1e-4, s0 + glide + step);
            if ((bool) n->state.getProperty ("injSlide", false)) { n->state.setProperty ("injSlide", false, um); ++done; continue; }
            te::MidiNote* prev = nullptr; for (auto* o : clip->getSequence().getNotes()) if (o != n && o->getStartBeat().inBeats() < s0 - 1e-4 && (prev == nullptr || o->getStartBeat().inBeats() > prev->getStartBeat().inBeats())) prev = o;
            if (prev == nullptr || prev->getNoteNumber() == n->getNoteNumber()) continue;
            const int rg = bendRange(); const double semis = juce::jlimit ((double) -rg, (double) rg, (double) (prev->getNoteNumber() - n->getNoteNumber()));
            const int steps = std::max (1, (int) std::lround (glide / step));
            for (int i = 0; i <= steps; ++i) { const double f = (double) i / steps; setCurvePoint (type, s0 + glide * f, juce::jlimit (0, 16383, 8192 + (int) std::lround (semis / rg * 8192.0 * (1.0 - f)))); }
            n->state.setProperty ("injSlide", true, um); ++done;
        }
        if (onClipChanged) onClipChanged(); repaint(); if (done == 0 && onMessage) onMessage (juce::String::fromUTF8 ("Hace falta una nota anterior con otro tono")); return done;
    }
    // ---- tocar solo la selección (o todo el clip) en bucle; al parar se restaura el bucle de antes ----
    bool loopSel = false; te::TimeRange savedLoop; bool savedLooping = false;
    void stopLoopSel()
    {
        if (! loopSel) return; auto& tc = edit.getTransport(); loopSel = false; tc.looping = savedLooping; tc.setLoopRange (savedLoop); if (tc.isPlaying()) tc.stop (false, false);
    }
    void playLoopSelection()
    {
        if (! clip) return; auto& tc = edit.getTransport(); if (loopSel) { stopLoopSel(); return; }
        double b0 = 1e9, b1 = 0.0; auto ns = selNotes(); if (ns.empty()) { b0 = 0.0; b1 = clipLenBeats(); } else for (auto* n : ns) { b0 = std::min (b0, n->getStartBeat().inBeats()); b1 = std::max (b1, n->getEndBeat().inBeats()); }
        if (b1 - b0 < 0.05) return;
        const double t0 = beatsToTime (clipStartBeat() + b0), t1 = beatsToTime (clipStartBeat() + b1);
        savedLoop = tc.getLoopRange(); savedLooping = tc.looping; tc.setLoopRange (te::TimeRange (te::TimePosition::fromSeconds (t0), te::TimePosition::fromSeconds (t1)));
        tc.looping = true; tc.setPosition (te::TimePosition::fromSeconds (t0)); tc.play (false); loopSel = true;
    }
public:
    void splitNote (te::MidiNote* n, double at)
    {
        const double s0 = n->getStartBeat().inBeats(), l0 = n->getLengthBeats().inBeats(); if (at <= s0 + 1e-6 || at >= s0 + l0 - 1e-6) return;
        auto* um = &edit.getUndoManager(); um->beginNewTransaction (juce::String::fromUTF8 ("Cortar nota"));
        const int pitch = n->getNoteNumber(), vel = n->getVelocity(); n->setStartAndLength (n->getStartBeat(), te::BeatDuration::fromBeats (at - s0), um);
        clip->getSequence().addNote (pitch, te::BeatPosition::fromBeats (at), te::BeatDuration::fromBeats (s0 + l0 - at), vel, 0, um); if (onClipChanged) onClipChanged(); repaint();
    }
    // Ajusta el inicio de cada nota a la rejilla actual (la longitud se conserva). Devuelve cuántas notas se movieron.
    // ---- herramientas de notas al estilo FL Studio: actúan sobre las notas elegidas (o todas si no hay ninguna) ----
    struct NoteData { int pitch; double start, len; int vel; bool mute = false; };
    std::vector<NoteData> targetNotes (std::vector<te::MidiNote*>& ptrs) const
    {
        ptrs = selNotes(); if (ptrs.empty()) for (auto* n : clip->getSequence().getNotes()) ptrs.push_back (n);
        std::vector<NoteData> d; for (auto* n : ptrs) d.push_back ({ n->getNoteNumber(), n->getStartBeat().inBeats(), n->getLengthBeats().inBeats(), n->getVelocity(), n->isMute() });
        std::sort (d.begin(), d.end(), [] (const NoteData& a, const NoteData& b) { return a.start < b.start || (a.start == b.start && a.pitch < b.pitch); });
        return d;
    }
    // op: chord:<intervalos separados por coma> arp:up|down|updown|random strum legato staccato chop:N flipH flipV oct:+12 oct:-12 velrand crescendo decrescendo glue
    int noteTool (const juce::String& op)
    {
        if (! clip) return 0; std::vector<te::MidiNote*> ptrs; auto d = targetNotes (ptrs); if (d.empty()) { if (onMessage) onMessage ("No hay notas"); return 0; }
        auto* um = &edit.getUndoManager(); um->beginNewTransaction (juce::String::fromUTF8 ("Herramienta de notas")); juce::Random rnd (42);
        std::vector<NoteData> out; const double q = quantum;
        if (op.startsWith ("chord:"))
        {
            juce::StringArray iv; iv.addTokens (op.fromFirstOccurrenceOf (":", false, false), ",", ""); out = d;
            for (auto& n : d) for (auto& t : iv) { const int st = t.getIntValue(); if (st != 0) out.push_back ({ juce::jlimit (0, 127, n.pitch + st), n.start, n.len, n.vel }); }
        }
        else if (op.startsWith ("arp:"))
        {   // agrupa por inicio, cada grupo de 2+ notas se reparte en pasos de la cuadrícula dentro de su duración
            const auto mode = op.substring (4); size_t i = 0;
            while (i < d.size())
            {
                size_t j = i; while (j < d.size() && std::abs (d[j].start - d[i].start) < 1e-3) ++j;
                std::vector<NoteData> grp (d.begin() + (long) i, d.begin() + (long) j); i = j;
                if (grp.size() < 2) { out.push_back (grp[0]); continue; }
                std::vector<int> order; for (size_t k = 0; k < grp.size(); ++k) order.push_back ((int) k);
                if (mode == "down") std::reverse (order.begin(), order.end());
                else if (mode == "updown") for (int k = (int) grp.size() - 2; k >= 1; --k) order.push_back (k);
                else if (mode == "random") for (int k = (int) order.size() - 1; k > 0; --k) std::swap (order[(size_t) k], order[(size_t) rnd.nextInt (k + 1)]);
                const double total = std::max (q, grp[0].len); const int steps = std::max (1, (int) std::lround (total / q));
                for (int k = 0; k < steps; ++k) { const auto& src = grp[(size_t) order[(size_t) k % order.size()]]; out.push_back ({ src.pitch, grp[0].start + k * q, q * 0.95, src.vel }); }
            }
        }
        else if (op == "strum")
        {   // las notas simultáneas entran escalonadas
            size_t i = 0; while (i < d.size()) { size_t j = i; while (j < d.size() && std::abs (d[j].start - d[i].start) < 1e-3) ++j;
                for (size_t k = i; k < j; ++k) { auto n = d[k]; const double off = (double) (k - i) * std::min (0.125, q * 0.5); n.start += off; n.len = std::max (0.05, n.len - off); out.push_back (n); } i = j; }
        }
        else if (op == "legato")
        {
            out = d; for (size_t a = 0; a < out.size(); ++a) { double nxt = -1; for (size_t b = 0; b < out.size(); ++b) if (out[b].start > out[a].start + 1e-4 && (nxt < 0 || out[b].start < nxt)) nxt = out[b].start; if (nxt > 0) out[a].len = nxt - out[a].start; }
        }
        else if (op == "staccato") { out = d; for (auto& n : out) n.len = std::max (0.03, n.len * 0.5); }
        else if (op.startsWith ("chop:")) { const int parts = std::max (2, op.fromFirstOccurrenceOf (":", false, false).getIntValue()); for (auto& n : d) for (int k = 0; k < parts; ++k) out.push_back ({ n.pitch, n.start + n.len * k / parts, n.len / parts * 0.95, n.vel }); }
        else if (op == "flipH") { double t0 = 1e9, t1 = 0; for (auto& n : d) { t0 = std::min (t0, n.start); t1 = std::max (t1, n.start + n.len); } out = d; for (auto& n : out) n.start = t0 + t1 - (n.start + n.len); }
        else if (op == "flipV") { int lo = 127, hi = 0; for (auto& n : d) { lo = std::min (lo, n.pitch); hi = std::max (hi, n.pitch); } out = d; for (auto& n : out) n.pitch = lo + hi - n.pitch; }
        else if (op.startsWith ("oct:")) { const int st = op.fromFirstOccurrenceOf (":", false, false).getIntValue(); out = d; for (auto& n : out) n.pitch = juce::jlimit (0, 127, n.pitch + st); }
        else if (op == "randpitch")
        {   // cambia el tono de cada nota a otro de la escala marcada (o de la menor natural si no hay escala), cerca del original
            const int rt = scaleRoot() >= 0 ? scaleRoot() : 0; out = d;
            for (auto& n : out) { for (int tries = 0; tries < 30; ++tries) { const int p2 = juce::jlimit (0, 127, n.pitch + rnd.nextInt (13) - 6); if (scaleRoot() >= 0 ? scaleKind (p2) != 0 : (((p2 - rt) % 12 + 12) % 12 == 0 || ((p2 - rt) % 12 + 12) % 12 == 2 || ((p2 - rt) % 12 + 12) % 12 == 3 || ((p2 - rt) % 12 + 12) % 12 == 5 || ((p2 - rt) % 12 + 12) % 12 == 7 || ((p2 - rt) % 12 + 12) % 12 == 8 || ((p2 - rt) % 12 + 12) % 12 == 10)) { n.pitch = p2; break; } } }
        }
        else if (op == "toscale")
        {   // lleva las notas que quedan fuera de la escala al tono permitido más cercano
            out = d; if (scaleRoot() < 0) { if (onMessage) onMessage ("Elige primero una escala"); return 0; }
            for (auto& n : out) for (int dd = 0; dd < 7; ++dd) { if (scaleKind (n.pitch + dd) != 0 && n.pitch + dd <= 127) { n.pitch += dd; break; } if (scaleKind (n.pitch - dd) != 0 && n.pitch - dd >= 0) { n.pitch -= dd; break; } }
        }
        else if (op == "invert") { out = d; auto lo = std::min_element (out.begin(), out.end(), [] (const NoteData& a, const NoteData& b) { return a.pitch < b.pitch; }); lo->pitch = std::min (127, lo->pitch + 12); }
        else if (op.startsWith ("stretch:")) { const double f = op.fromFirstOccurrenceOf (":", false, false).getDoubleValue(); double t0 = 1e9; for (auto& n : d) t0 = std::min (t0, n.start); out = d; for (auto& n : out) { n.start = t0 + (n.start - t0) * f; n.len = std::max (0.03, n.len * f); } }
        else if (op == "velup" || op == "veldown") { out = d; const double f = op == "velup" ? 1.1 : 0.9; for (auto& n : out) n.vel = juce::jlimit (1, 127, (int) std::lround (n.vel * f + (op == "velup" ? 1 : -1))); }
        else if (op.startsWith ("ratchet:")) { const int parts = std::max (2, op.fromFirstOccurrenceOf (":", false, false).getIntValue()); for (auto& n : d) for (int k = 0; k < parts; ++k) out.push_back ({ n.pitch, n.start + n.len * k / parts, n.len / parts * 0.85, n.vel, n.mute }); }
        else if (op == "velrand") { out = d; for (auto& n : out) n.vel = 50 + rnd.nextInt (78); }
        else if (op == "crescendo" || op == "decrescendo") { out = d; const double t0 = d.front().start, t1 = std::max (t0 + 1e-3, d.back().start); for (auto& n : out) { double f = (n.start - t0) / (t1 - t0); if (op == "decrescendo") f = 1.0 - f; n.vel = (int) std::lround (40 + 87 * f); } }
        else if (op == "glue")
        {   // une notas del mismo tono que se tocan o se solapan
            std::sort (d.begin(), d.end(), [] (const NoteData& a, const NoteData& b) { return a.pitch < b.pitch || (a.pitch == b.pitch && a.start < b.start); });
            for (auto& n : d) { if (! out.empty() && out.back().pitch == n.pitch && n.start <= out.back().start + out.back().len + 1e-3) out.back().len = std::max (out.back().len, n.start + n.len - out.back().start); else out.push_back (n); }
        }
        else return 0;
        sel = nullptr; selSet.clear(); for (auto* n : ptrs) clip->getSequence().removeNote (*n, um);
        for (auto& n : out) { auto* nn = clip->getSequence().addNote (n.pitch, te::BeatPosition::fromBeats (n.start), te::BeatDuration::fromBeats (n.len), juce::jlimit (1, 127, n.vel), 0, um); if (nn != nullptr) { if (n.mute) nn->setMute (true, um); selSet.push_back (nn->state); } }
        if (onClipChanged) onClipChanged(); repaint(); if (onMessage) onMessage (juce::String (out.size()) + " notas"); return (int) out.size();
    }
    void toolsMenu (juce::Rectangle<float> r)
    {
        juce::PopupMenu m, chords, arps; struct C { const char* n; const char* iv; };
        static const C cs[] { { "Mayor", "0,4,7" }, { "Menor", "0,3,7" }, { "Quinta (power)", "0,7" }, { "Octava", "0,12" }, { "Séptima", "0,4,7,10" }, { "Menor 7", "0,3,7,10" }, { "Mayor 7", "0,4,7,11" }, { "sus2", "0,2,7" }, { "sus4", "0,5,7" }, { "Disminuido", "0,3,6" }, { "Aumentado", "0,4,8" } };
        for (int i = 0; i < (int) std::size (cs); ++i) chords.addItem (100 + i, juce::String::fromUTF8 (cs[i].n));
        arps.addItem (200, "Arriba"); arps.addItem (201, "Abajo"); arps.addItem (202, "Arriba y abajo"); arps.addItem (203, "Aleatorio");
        m.addItem (304, "Ajustar la vista a las notas  (Ctrl+0)"); m.addSeparator(); m.addItem (300, "Importar archivo MIDI..."); m.addItem (301, "Exportar clip como MIDI..."); m.addItem (302, "Humanizar"); m.addItem (303, "Cuantizar notas (Q)"); m.addSeparator();
        m.addSubMenu (juce::String::fromUTF8 ("Estampar acorde"), chords); m.addSubMenu ("Arpegiar", arps); m.addItem (3, "Strum (escalonar acorde)"); m.addSeparator();
        m.addItem (4, "Legato"); m.addItem (5, "Staccato"); m.addItem (6, "Unir notas iguales"); m.addItem (7, "Cortar en 2"); m.addItem (8, "Cortar en 4"); m.addItem (9, "Cortar en 8"); m.addSeparator();
        m.addItem (10, "Voltear en el tiempo"); m.addItem (11, "Voltear de agudo a grave"); m.addItem (12, "Octava arriba"); m.addItem (13, "Octava abajo"); m.addSeparator();
        m.addItem (14, "Velocidad aleatoria"); m.addItem (15, "Crescendo"); m.addItem (16, "Decrescendo"); m.addSeparator();
        m.addItem (17, "Tonos aleatorios (en la escala)"); m.addItem (18, "Ajustar notas a la escala"); m.addItem (19, "Invertir el acorde"); m.addItem (20, juce::String::fromUTF8 ("Estirar x2 (m\xc3\xa1s lento)")); m.addItem (21, juce::String::fromUTF8 ("Encoger x0.5 (m\xc3\xa1s r\xc3\xa1pido)"));
        { juce::PopupMenu rat; for (int k = 0; k < 5; ++k) rat.addItem (410 + k, "x" + juce::String (std::array<int, 5> { 2, 3, 4, 6, 8 }[(size_t) k]));
          m.addSeparator(); m.addItem (400, "Velocidad +10 %"); m.addItem (401, "Velocidad -10 %"); m.addSubMenu (juce::String::fromUTF8 ("Ratchet (repetir la nota)"), rat); m.addSeparator();
          m.addItem (420, juce::String::fromUTF8 ("Silenciar / activar notas  (M)")); m.addItem (421, juce::String::fromUTF8 ("Slide: deslizar desde la nota anterior"));
          m.addItem (430, juce::String::fromUTF8 ("Tocar la selección en bucle"), true, loopSel); }
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (r.toNearestInt())), [this] (int c)
        {
            if (c == 400) { noteTool ("velup"); return; } if (c == 401) { noteTool ("veldown"); return; }
            if (c >= 410 && c <= 414) { static const int rs[] { 2, 3, 4, 6, 8 }; noteTool ("ratchet:" + juce::String (rs[c - 410])); return; }
            if (c == 420) { muteSel(); return; } if (c == 421) { toggleSlide(); return; } if (c == 430) { playLoopSelection(); return; }
            if (c == 300) { importMidi(); return; } if (c == 301) { exportMidi(); return; } if (c == 302) { humanize(); return; } if (c == 303) { quantizeNotes(); return; } if (c == 304) { fitView(); return; }
            static const char* iv[] { "0,4,7", "0,3,7", "0,7", "0,12", "0,4,7,10", "0,3,7,10", "0,4,7,11", "0,2,7", "0,5,7", "0,3,6", "0,4,8" };
            static const char* ar[] { "up", "down", "updown", "random" }; static const char* op[] { "", "", "", "strum", "legato", "staccato", "glue", "chop:2", "chop:4", "chop:8", "flipH", "flipV", "oct:12", "oct:-12", "velrand", "crescendo", "decrescendo", "randpitch", "toscale", "invert", "stretch:2", "stretch:0.5" };
            if (c >= 100 && c < 100 + 11) noteTool (juce::String ("chord:") + iv[c - 100]); else if (c >= 200 && c < 204) noteTool (juce::String ("arp:") + ar[c - 200]); else if (c >= 3 && c <= 21) noteTool (op[c]);
        });
    }
    int quantizeNotes()
    {
        if (! clip) return 0; auto* um = &edit.getUndoManager(); um->beginNewTransaction ("Cuantizar"); const double q = quantum; int moved = 0;
        for (auto* note : clip->getSequence().getNotes())
        {
            const double s0 = note->getStartBeat().inBeats(), t = std::max (0.0, std::round (s0 / q) * q);
            if (std::abs (t - s0) > 1e-6) { note->setStartAndLength (te::BeatPosition::fromBeats (t), note->getLengthBeats(), um); ++moved; }
        }
        if (onClipChanged) onClipChanged(); repaint(); if (onMessage) onMessage (juce::String::fromUTF8 ("Cuantizado: ") + juce::String (moved) + " notas"); return moved;
    }
    int humanize()
    {
        if (! clip) return 0; auto* um = &edit.getUndoManager(); um->beginNewTransaction ("Humanizar"); juce::Random rnd; int n = 0;
        for (auto* note : clip->getSequence().getNotes())
        {
            const double s0 = note->getStartBeat().inBeats(); const double j = (rnd.nextDouble() - .5) * 0.06;   // ±0.03 tiempos
            note->setStartAndLength (te::BeatPosition::fromBeats (std::max (0.0, s0 + j)), note->getLengthBeats(), um);
            note->setVelocity (juce::jlimit (1, 127, note->getVelocity() + rnd.nextInt (21) - 10), um); ++n;
        }
        if (onClipChanged) onClipChanged(); repaint(); if (onMessage) onMessage (juce::String::fromUTF8 ("Humanizado: ") + juce::String (n) + " notas"); return n;
    }
    bool exportMidiTo (const juce::File& f)
    {
        if (! clip) return false; constexpr int ppq = 960; juce::MidiMessageSequence seq;
        const double bpm = edit.tempoSequence.getTempos().size() ? edit.tempoSequence.getTempos()[0]->getBpm() : 120.0;
        seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / bpm)), 0);
        for (auto* n : clip->getSequence().getNotes())
        {
            const double t0 = n->getStartBeat().inBeats() * ppq, t1 = n->getEndBeat().inBeats() * ppq;
            seq.addEvent (juce::MidiMessage::noteOn (1, n->getNoteNumber(), (juce::uint8) n->getVelocity()), t0); seq.addEvent (juce::MidiMessage::noteOff (1, n->getNoteNumber()), t1);
        }
        for (auto* ce : clip->getSequence().getControllerEvents())
        {
            const double tt = ce->getBeatPosition().inBeats() * ppq;
            if (ce->getType() == te::MidiControllerEvent::pitchWheelType) seq.addEvent (juce::MidiMessage::pitchWheel (1, juce::jlimit (0, 16383, ce->getControllerValue())), tt);
            else if (ce->getType() >= 0 && ce->getType() < 128) seq.addEvent (juce::MidiMessage::controllerEvent (1, ce->getType(), juce::jlimit (0, 127, ce->getControllerValue())), tt);
        }
        seq.updateMatchedPairs(); juce::MidiFile mf; mf.setTicksPerQuarterNote (ppq); mf.addTrack (seq); f.deleteFile();
        if (auto os = f.createOutputStream()) return mf.writeTo (*os);
        return false;
    }
    int importMidiFrom (const juce::File& f)
    {
        if (! clip) return 0; juce::MidiFile mf; std::unique_ptr<juce::FileInputStream> is (f.createInputStream()); if (! is || ! mf.readFrom (*is)) return 0;
        const int tpq = mf.getTimeFormat(); if (tpq <= 0) return 0; auto* um = &edit.getUndoManager(); um->beginNewTransaction ("Importar MIDI"); int count = 0; double maxEnd = 0.0;
        for (int t = 0; t < mf.getNumTracks(); ++t)
        {
            juce::MidiMessageSequence seq (*mf.getTrack (t)); seq.updateMatchedPairs();
            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                auto* ev = seq.getEventPointer (i);
                if (ev->message.isPitchWheel()) { clip->getSequence().addControllerEvent (te::BeatPosition::fromBeats (ev->message.getTimeStamp() / tpq), te::MidiControllerEvent::pitchWheelType, ev->message.getPitchWheelValue(), um); continue; }
                if (ev->message.isController()) { clip->getSequence().addControllerEvent (te::BeatPosition::fromBeats (ev->message.getTimeStamp() / tpq), ev->message.getControllerNumber(), ev->message.getControllerValue(), um); continue; }
                if (! ev->message.isNoteOn() || ev->noteOffObject == nullptr) continue;
                const double s0 = ev->message.getTimeStamp() / tpq, l0 = std::max (0.03125, (ev->noteOffObject->message.getTimeStamp() - ev->message.getTimeStamp()) / tpq);
                clip->getSequence().addNote (ev->message.getNoteNumber(), te::BeatPosition::fromBeats (s0), te::BeatDuration::fromBeats (l0), ev->message.getVelocity(), 0, um); maxEnd = std::max (maxEnd, s0 + l0); ++count;
            }
        }
        if (maxEnd > clipLenBeats()) clip->setEnd (te::TimePosition::fromSeconds (beatsToTime (clipStartBeat() + std::ceil (maxEnd / timesig::bar()) * timesig::bar())), true);
        if (onClipChanged) onClipChanged(); repaint(); if (onMessage) onMessage (juce::String::fromUTF8 ("Importadas ") + juce::String (count) + " notas"); return count;
    }
private:
    void importMidi() { if (! clip) return; chooser = std::make_unique<juce::FileChooser> (juce::String::fromUTF8 ("Importar MIDI"), juce::File(), "*.mid;*.midi"); chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) importMidiFrom (fc.getResult()); }); }
    void exportMidi() { if (! clip) return; chooser = std::make_unique<juce::FileChooser> (juce::String::fromUTF8 ("Exportar MIDI"), juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (clip->getName() + ".mid"), "*.mid"); chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc) { auto f = fc.getResult(); if (f != juce::File()) { if (! f.hasFileExtension ("mid")) f = f.withFileExtension ("mid"); if (exportMidiTo (f) && onMessage) onMessage (juce::String::fromUTF8 ("MIDI exportado: ") + f.getFileName()); } }); }
    juce::Point<float> hover { -1, -1 }; bool hoverWas = false; std::unique_ptr<juce::FileChooser> chooser;
};
