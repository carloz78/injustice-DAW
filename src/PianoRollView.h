#pragma once
#include "Theme.h"
#include "Icons.h"
#include <tracktion_engine/tracktion_engine.h>
namespace te = tracktion;

// Piano Roll: teclado vertical, cuadrícula MIDI, notas compactas y carril de velocity. Edita un MidiClip de Tracktion.
class PianoRollView : public juce::Component, public juce::TooltipClient, private juce::Timer
{
public:
    static constexpr int kKeyW = 96, kBarH = 52, kRulerH = 32, kTop = kBarH + kRulerH, kVelH = 132, kNoteH = 20;
    juce::String tool = "select"; double quantum = 0.25; bool snap = true;   // 0.25 beats = 1/16
    std::function<void()> onClipChanged; std::function<void (const juce::String&)> onMessage;

    explicit PianoRollView (te::Edit& e) : edit (e) { setWantsKeyboardFocus (true); setOpaque (true); startTimerHz (30); scrollY = (127 - 76) * kNoteH; }

    void setClip (te::MidiClip* c) { clip = c; sel = nullptr; scrollX = 0; if (clip) { auto n = clip->getSequence().getNotes(); if (n.size()) scrollY = (127 - n[0]->getNoteNumber() - 5) * (double) kNoteH; } repaint(); }
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
        g.fillAll (juce::Colour (0xff20202d));
        drawToolbar (g, W);
        if (! clip) { g.setColour (textDim); g.setFont (juce::FontOptions (16.f)); g.drawText (juce::String::fromUTF8 ("Dibuja un clip MIDI en la Playlist (herramienta lápiz) y haz doble clic en él."), getLocalBounds().withTrimmedTop (kBarH), juce::Justification::centred); return; }

        g.saveState(); g.reduceClipRegion (kKeyW, kTop, W - kKeyW, gb - kTop);
        for (int p = 0; p <= 127; ++p)
        {
            const float y = ny (p); if (y > gb || y + kNoteH < kTop) continue;
            g.setColour (isBlack (p) ? juce::Colour (0xff1e1e29) : juce::Colour (0xff262633)); g.fillRect ((float) kKeyW, y, (float) W, (float) kNoteH);
            g.setColour (juce::Colour (0xff1b1b25)); g.fillRect ((float) kKeyW, y + kNoteH - 1.f, (float) W, 1.f);
            if (p % 12 == 0) { g.setColour (juce::Colour (0xff3d3d4f)); g.fillRect ((float) kKeyW, y + kNoteH - 1.f, (float) W, 1.f); }
        }
        const float xe = bx (clipLenBeats());   // fuera del clip: color sólido más oscuro
        g.setColour (juce::Colour (0xff17171f)); g.fillRect (xe, (float) kTop, (float) W, (float) (gb - kTop));
        const double step = ppb * quantum >= 9 ? quantum : (ppb >= 9 ? 1.0 : 4.0);
        for (double b = std::floor (xb ((float) kKeyW) / step) * step; bx (b) < W; b += step)
        {
            const bool bar = std::fmod (std::round (b * 1000) / 1000, 4.0) == 0.0, beat = std::fmod (std::round (b * 1000) / 1000, 1.0) == 0.0;
            g.setColour (bar ? juce::Colour (0xff4b4b60) : (beat ? juce::Colour (0xff30303f) : juce::Colour (0xff2a2a37)));
            g.fillRect (bx (b), (float) kTop, 1.f, (float) (gb - kTop));
        }
        for (auto* n : clip->getSequence().getNotes()) drawNote (g, *n);
        g.restoreState();

        drawRuler (g, W); drawKeyboard (g, gb); drawVelocity (g, W, H, gb);

        const double hb = timeToBeats (edit.getTransport().getPosition().inSeconds()) - clipStartBeat();
        const float px = bx (hb);
        if (hb >= 0 && px >= kKeyW + 8 && px <= W) { g.setColour (juce::Colour (0xffd9d8e6)); g.fillRect (px - .5f, (float) kBarH + 2.f, 1.f, (float) (H - kBarH - 2)); g.setColour (accentHi); g.fillRect (px - 4.f, (float) kBarH + kRulerH - 6.f, 9.f, 6.f); }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        if (e.y < kBarH) { toolbarClick (e); return; }
        if (! clip) return;
        if (e.y >= gridBottom()) { velocityAt (e); return; }
        if (e.x < kKeyW || e.y < kTop) { if (e.y < kTop && e.x > kKeyW) seek (e.x); else if (e.x < kKeyW) { litKey = yp ((float) e.y); repaint(); } return; }
        auto* hit = noteAt (e.position); mode = Mode::none;
        if (tool == "erase") { if (hit) removeNote (hit); mode = Mode::erase; return; }
        if (tool == "cut") { if (hit) splitNote (hit, q (xb ((float) e.x))); return; }
        if (tool == "brush") { edit.getUndoManager().beginNewTransaction (juce::String::fromUTF8 ("Pintar notas")); mode = Mode::paint; if (! hit) paintNote (e.position); return; }
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
        if (mode == Mode::paint) { paintNote (e.position); return; }
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
    void paintNote (juce::Point<float> pos)
    {
        if (pos.x < kKeyW || pos.y < kTop || pos.y >= gridBottom()) return;
        const double len = snap ? quantum : 0.25, start = std::floor (xb (pos.x) / len) * len; const int pitch = juce::jlimit (0, 127, yp (pos.y));
        if (start < 0 || start >= clipLenBeats()) return;
        for (auto* n : clip->getSequence().getNotes()) if (n->getNoteNumber() == pitch && n->getStartBeat().inBeats() < start + len - 1e-6 && n->getEndBeat().inBeats() > start + 1e-6) return;
        sel = clip->getSequence().addNote (pitch, te::BeatPosition::fromBeats (start), te::BeatDuration::fromBeats (len), lastVel, 0, &edit.getUndoManager()); repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { if (mode != Mode::none && onClipChanged) onClipChanged(); mode = Mode::none; litKey = -1; repaint(); }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (e.mods.isCtrlDown() || e.mods.isCommandDown()) { const double a = xb ((float) e.x); ppb = juce::jlimit (6.0, 400.0, ppb * (1.0 + w.deltaY * .6)); scrollX = juce::jmax (0.0, a * ppb - (e.x - kKeyW - 8)); }
        else if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY)) scrollX = juce::jmax (0.0, scrollX - (w.deltaX != 0 ? w.deltaX : w.deltaY) * 600.0);
        else scrollY = juce::jlimit (0.0, (double) (128 * kNoteH - (gridBottom() - kTop)), scrollY - w.deltaY * 400.0);
        repaint();
    }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if ((k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) && sel != nullptr) { removeNote (sel); return true; }
        return false;
    }

private:
    enum class Mode { none, move, resize, erase, paint };
    te::Edit& edit; te::MidiClip::Ptr clip; te::MidiNote* sel = nullptr; Mode mode = Mode::none;
    double ppb = 50.0, scrollX = 0, scrollY = 0, anchor = 0, grab = 0, lastLen = 0.25; int lastVel = 100, litKey = -1;

    static bool isBlack (int p) { switch (p % 12) { case 1: case 3: case 6: case 8: case 10: return true; default: return false; } }
    void timerCallback() override
    {
        if (! isVisible()) return;
        const double pos = edit.getTransport().getPosition().inSeconds();
        if (pos != lastPos) { lastPos = pos; repaint(); }   // el cabezal solo se mueve con la reproducci\xc3\xb3n o al hacer seek
    }
    double lastPos = -1.0;
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
        const float top = (float) gridBottom() + 34.f, bot = (float) getHeight() - 10.f; const int v = juce::jlimit (1, 127, (int) std::round (127.0 * (bot - e.y) / (bot - top)));
        edit.getUndoManager().beginNewTransaction ("Velocity"); best->setVelocity (v, &edit.getUndoManager()); lastVel = v; sel = best; repaint();
        if (onClipChanged) onClipChanged();
    }

    void drawNote (juce::Graphics& g, te::MidiNote& n)
    {
        juce::Rectangle<float> r (bx (n.getStartBeat().inBeats()), ny (n.getNoteNumber()) + 1.f, std::max (3.f, (float) (n.getLengthBeats().inBeats() * ppb) - 1.f), (float) kNoteH - 2.f);
        const bool s = (&n == sel);
        g.setColour (s ? juce::Colour (0xffd58cff) : juce::Colour (0xffa044d7)); g.fillRect (r);
        g.setColour (s ? juce::Colours::white : juce::Colour (0xff8a32bd)); g.drawRect (r, 1.f);
    }
    void drawRuler (juce::Graphics& g, int W)
    {
        g.setColour (juce::Colour (0xff20202d)); g.fillRect (kKeyW, kBarH, W, kRulerH);
        g.setColour (juce::Colour (0xff3d3d4f)); g.fillRect (kKeyW, kTop - 1, W, 1);
        g.setFont (juce::FontOptions (14.f));
        for (int bar = 0; ; ++bar)
        {
            const float x = bx (bar * 4.0); if (x > W) break; if (x < kKeyW + 4) continue;
            g.setColour (juce::Colour (0xff4b4b60)); g.fillRect (x, (float) kBarH + 6.f, 1.f, (float) kRulerH - 6.f);
            g.setColour (theme::text); g.drawText (juce::String (bar + 1), juce::Rectangle<float> (x + 6, (float) kBarH + 4.f, 40, 20).toNearestInt(), juce::Justification::centredLeft);
        }
        g.setColour (juce::Colour (0xff20202d)); g.fillRect (0, kBarH, kKeyW, kRulerH);
    }
    void drawKeyboard (juce::Graphics& g, int gb)
    {
        g.saveState(); g.reduceClipRegion (0, kTop, kKeyW, gb - kTop);
        g.setColour (juce::Colour (0xff20202d)); g.fillRect (0, kTop, kKeyW, gb - kTop);
        for (int p = 0; p <= 127; ++p)
        {
            const float y = ny (p); if (y > gb || y + kNoteH < kTop || isBlack (p)) continue;
            g.setColour (p == litKey ? juce::Colour (0xffe0b4f5) : juce::Colour (0xffecebf2)); g.fillRect (6.f, y, (float) kKeyW - 10, (float) kNoteH - 1.f);
            g.setColour (juce::Colour (0xffb5b4c4)); g.fillRect (6.f, y + kNoteH - 1.f, (float) kKeyW - 10, 1.f);
            if (p % 12 == 0) { g.setColour (juce::Colour (0xff2a2a35)); g.setFont (juce::FontOptions (13.f, juce::Font::bold)); g.drawText ("C" + juce::String (p / 12 - 1), juce::Rectangle<float> (6.f, y, (float) kKeyW - 14, (float) kNoteH).toNearestInt(), juce::Justification::centredRight); }
        }
        for (int p = 0; p <= 127; ++p)
        {
            const float y = ny (p); if (y > gb || y + kNoteH < kTop || ! isBlack (p)) continue;
            g.setColour (p == litKey ? juce::Colour (0xff9a5cb8) : juce::Colour (0xff2b2b35)); g.fillRoundedRectangle (6.f, y + 1, 58.f, (float) kNoteH - 2.f, 3.f);
            g.setColour (juce::Colour (0xff45454f)); g.fillRect (10.f, y + 2, 44.f, 1.f);
        }
        g.restoreState();
    }
    void drawVelocity (juce::Graphics& g, int W, int H, int gb)
    {
        g.setColour (juce::Colour (0xff3d3d4f)); g.fillRect (0, gb, W, 1);
        g.setColour (juce::Colour (0xff1c1c26)); g.fillRect (kKeyW, gb + 1, W - kKeyW, kVelH - 1);
        g.setColour (juce::Colour (0xff20202d)); g.fillRect (0, gb + 1, kKeyW, kVelH - 1);
        g.setColour (theme::text); g.setFont (juce::FontOptions (14.f)); g.drawText ("Velocity", 12, gb + 6, kKeyW - 12, 22, juce::Justification::centredLeft);
        const float top = (float) gb + 34.f, bot = (float) H - 10.f;   // escala 127 .. 0
        g.setFont (juce::FontOptions (12.f));
        for (int v : { 127, 96, 64, 32, 0 })
        {
            const float y = bot - (bot - top) * (float) v / 127.f;
            g.setColour (theme::textDim); g.drawText (juce::String (v), juce::Rectangle<float> (4, y - 8, (float) kKeyW - 22, 16).toNearestInt(), juce::Justification::centredRight); g.fillRect ((float) kKeyW - 14, y, 8.f, 1.f);
            g.setColour (juce::Colour (0xff262633)); g.fillRect ((float) kKeyW, y, (float) (W - kKeyW), 1.f);
        }
        g.saveState(); g.reduceClipRegion (kKeyW, gb + 1, W - kKeyW, kVelH);
        for (auto* n : clip->getSequence().getNotes())
        {
            const float x = bx (n->getStartBeat().inBeats()), y = bot - (bot - top) * (float) n->getVelocity() / 127.f; const bool s = (&*n == sel);
            g.setColour (s ? juce::Colour (0xffd58cff) : juce::Colour (0xffb45ee6)); g.fillRect (x - 1.f, y, 3.f, bot - y); g.fillEllipse (x - 4.5f, y - 4.5f, 9.f, 9.f);
        }
        g.restoreState();
    }

    //---- barra de herramientas propia del Piano Roll
    struct Btn { juce::String id, tip; icons::Id icon; juce::Rectangle<float> r; };
    std::vector<Btn> btns() const
    {
        std::vector<Btn> v; float x = 350.f; const float y = 8.f;
        auto add = [&] (const char* id, const char* tip, icons::Id ic) { v.push_back ({ id, juce::String::fromUTF8 (tip), ic, { x, y, 40, 36 } }); x += 44; };
        add ("select", "Seleccionar y mover notas", icons::Select); add ("pencil", "Lápiz: dibuja notas", icons::Pencil); add ("brush", "Pincel: arrastra para pintar varias notas", icons::Brush); add ("erase", "Borrador", icons::Eraser); add ("cut", "Cortar una nota en dos", icons::Scissors);
        x += 10; v.push_back ({ "quant", juce::String::fromUTF8 ("Cuantización"), icons::Chevron, { x, y, 92, 36 }, }); x += 108;
        add ("import", "Importar archivo MIDI a este clip", icons::OpenFile); add ("export", "Exportar este clip como archivo MIDI", icons::Export); add ("humanize", "Humanizar: mueve un poco el tiempo y la fuerza de las notas", icons::Sliders);
        return v;
    }
    juce::String quantLabel() const { return snap ? (quantum >= 1.0 ? "1/4" : quantum >= .5 ? "1/8" : quantum >= .25 ? "1/16" : "1/32") : juce::String ("Libre"); }
    void drawToolbar (juce::Graphics& g, int W)
    {
        using namespace theme;
        g.setColour (juce::Colour (0xff1b1b25)); g.fillRect (0, 0, W, kBarH);
        g.setColour (juce::Colour (0xff2d2d3b)); g.fillRect (0, kBarH - 1, W, 1);
        auto tb = juce::Rectangle<float> (8.f, 8.f, 330.f, 36.f); g.setColour (boxFill); g.fillRoundedRectangle (tb, 8.f); g.setColour (boxLine); g.drawRoundedRectangle (tb.reduced (.5f), 8.f, 1.f);
        icons::draw (g, icons::PianoRoll, { 12.f, 8.f, 36.f, 36.f }, accentHi);
        const juce::String who = clip ? clip->getName() : juce::String ("sin clip");
        g.setColour (text); g.setFont (juce::FontOptions (16.f)); g.drawText (juce::String::fromUTF8 ("Piano Roll  \xe2\x80\x93  ") + who, juce::Rectangle<float> (54, 8, 276, 36).toNearestInt(), juce::Justification::centredLeft);
        for (auto& b : btns())
        {
            const bool active = b.id == tool, hot = b.r.contains (hover);
            g.setColour (active ? accent : (hot ? panelHi : boxFill)); g.fillRoundedRectangle (b.r, 8.f); g.setColour (active ? accent : boxLine); g.drawRoundedRectangle (b.r.reduced (.5f), 8.f, 1.f);
            if (b.id == "quant") { g.setColour (text); g.setFont (juce::FontOptions (15.f)); g.drawText (quantLabel(), b.r.withTrimmedRight (22).toNearestInt(), juce::Justification::centred); icons::draw (g, icons::Chevron, { b.r.getRight() - 28.f, b.r.getY(), 26.f, b.r.getHeight() }, textDim); }
            else icons::draw (g, b.icon, b.r, active ? juce::Colours::white : iconIdle);
        }
    }
    void toolbarClick (const juce::MouseEvent& e)
    {
        for (auto& b : btns()) if (b.r.contains (e.position))
        {
            if (b.id == "select" || b.id == "pencil" || b.id == "brush" || b.id == "erase" || b.id == "cut") { tool = b.id; repaint(); }
            else if (b.id == "quant") quantMenu (b.r);
            else if (b.id == "import") importMidi(); else if (b.id == "export") exportMidi(); else if (b.id == "humanize") humanize();
            return;
        }
    }
    void quantMenu (juce::Rectangle<float> r)
    {
        juce::PopupMenu m; m.addItem (1, "1/4", true, snap && quantum == 1.0); m.addItem (2, "1/8", true, snap && quantum == .5); m.addItem (3, "1/16", true, snap && quantum == .25); m.addItem (4, "1/32", true, snap && quantum == .125); m.addSeparator(); m.addItem (5, "Libre (sin imán)", true, ! snap);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (r.toNearestInt())), [this] (int c)
        { if (c >= 1 && c <= 4) { quantum = c == 1 ? 1.0 : c == 2 ? .5 : c == 3 ? .25 : .125; snap = true; } else if (c == 5) snap = false; repaint(); });
    }
    void mouseMove (const juce::MouseEvent& e) override { hover = e.position; if (e.y < kBarH || hoverWas) repaint (0, 0, getWidth(), kBarH); hoverWas = e.y < kBarH; }
    void mouseExit (const juce::MouseEvent&) override { hover = { -1, -1 }; repaint (0, 0, getWidth(), kBarH); }
public:
    juce::String getTooltip() override { for (auto& b : btns()) if (b.r.contains (hover)) return b.tip; return {}; }
    // --- acciones sobre el clip (públicas para las pruebas)
    void splitNote (te::MidiNote* n, double at)
    {
        const double s0 = n->getStartBeat().inBeats(), l0 = n->getLengthBeats().inBeats(); if (at <= s0 + 1e-6 || at >= s0 + l0 - 1e-6) return;
        auto* um = &edit.getUndoManager(); um->beginNewTransaction (juce::String::fromUTF8 ("Cortar nota"));
        const int pitch = n->getNoteNumber(), vel = n->getVelocity(); n->setStartAndLength (n->getStartBeat(), te::BeatDuration::fromBeats (at - s0), um);
        clip->getSequence().addNote (pitch, te::BeatPosition::fromBeats (at), te::BeatDuration::fromBeats (s0 + l0 - at), vel, 0, um); if (onClipChanged) onClipChanged(); repaint();
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
                auto* ev = seq.getEventPointer (i); if (! ev->message.isNoteOn() || ev->noteOffObject == nullptr) continue;
                const double s0 = ev->message.getTimeStamp() / tpq, l0 = std::max (0.03125, (ev->noteOffObject->message.getTimeStamp() - ev->message.getTimeStamp()) / tpq);
                clip->getSequence().addNote (ev->message.getNoteNumber(), te::BeatPosition::fromBeats (s0), te::BeatDuration::fromBeats (l0), ev->message.getVelocity(), 0, um); maxEnd = std::max (maxEnd, s0 + l0); ++count;
            }
        }
        if (maxEnd > clipLenBeats()) clip->setEnd (te::TimePosition::fromSeconds (beatsToTime (clipStartBeat() + std::ceil (maxEnd / 4.0) * 4.0)), true);
        if (onClipChanged) onClipChanged(); repaint(); if (onMessage) onMessage (juce::String::fromUTF8 ("Importadas ") + juce::String (count) + " notas"); return count;
    }
private:
    void importMidi() { if (! clip) return; chooser = std::make_unique<juce::FileChooser> (juce::String::fromUTF8 ("Importar MIDI"), juce::File(), "*.mid;*.midi"); chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) importMidiFrom (fc.getResult()); }); }
    void exportMidi() { if (! clip) return; chooser = std::make_unique<juce::FileChooser> (juce::String::fromUTF8 ("Exportar MIDI"), juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (clip->getName() + ".mid"), "*.mid"); chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc) { auto f = fc.getResult(); if (f != juce::File()) { if (! f.hasFileExtension ("mid")) f = f.withFileExtension ("mid"); if (exportMidiTo (f) && onMessage) onMessage (juce::String::fromUTF8 ("MIDI exportado: ") + f.getFileName()); } }); }
    juce::Point<float> hover { -1, -1 }; bool hoverWas = false; std::unique_ptr<juce::FileChooser> chooser;
};
