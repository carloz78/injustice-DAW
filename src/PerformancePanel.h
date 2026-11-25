#pragma once
#include "FloatingPanel.h"
#include "TrackUtil.h"
#include "Theme.h"
#include <tracktion_engine/tracktion_engine.h>
namespace te = tracktion;

// Modo ensayo: parrilla de clips para lanzar en vivo (como las escenas de Ableton). Cada columna es una pista, cada fila una escena.
// Un clic en una celda lanza o detiene su clip, ajustado al compás; el botón ▶ de la fila lanza toda la escena.
class PerformancePanel : public FloatingPanel, private juce::Timer
{
public:
    static constexpr int kCols = 8, kRows = 8, kCellW = 100, kCellH = 46, kGap = 5, kGridX = 100, kGridY = 140;
    std::function<void (const juce::String&)> onMessage;
    int firstTrack = 0;

    explicit PerformancePanel (te::Edit& e) : edit (e)
    {
        title = "Modo ensayo"; icon = icons::Playlist; setDesign (kGridX + kCols * (kCellW + kGap) + 16, kGridY + kRows * (kCellH + kGap) + 20); startTimerHz (30);
    }
    ~PerformancePanel() override { stopTimer(); }

    // ---- lógica (pública para las pruebas) ----
    te::AudioTrack* trackAt (int col) const { auto t = te::getAudioTracks (edit); const int i = firstTrack + col; return i >= 0 && i < t.size() ? t[i] : nullptr; }
    te::ClipSlot* slotAt (int col, int row) const
    {
        auto* t = trackAt (col); if (t == nullptr) return nullptr; auto& list = t->getClipSlotList(); list.ensureNumberOfSlots (kRows);
        auto s = list.getClipSlots(); return row >= 0 && row < s.size() ? s[row] : nullptr;
    }
    te::Clip* clipAt (int col, int row) const { auto* s = slotAt (col, row); return s != nullptr ? s->getClip() : nullptr; }
    // 0 vacía, 1 detenida, 2 sonando, 3 en cola para empezar, 4 en cola para detenerse
    int stateAt (int col, int row) const
    {
        auto* c = clipAt (col, row); if (c == nullptr) return 0; auto lh = c->getLaunchHandle(); if (! lh) return 1;
        const auto q = lh->getQueuedStatus();
        if (q && *q == te::LaunchHandle::QueueState::playQueued) return 3;
        if (q && *q == te::LaunchHandle::QueueState::stopQueued) return 4;
        return lh->getPlayingStatus() == te::LaunchHandle::PlayState::playing ? 2 : 1;
    }
    // Lugar donde empezará el clip según la cuantización global (nullopt = ya)
    std::optional<te::MonotonicBeat> launchPos()
    {
        auto& tc = edit.getTransport(); if (! tc.isPlaying()) return std::nullopt;
        auto epc = tc.getCurrentPlaybackContext(); if (! epc) return std::nullopt; auto sp = epc->getSyncPoint(); if (! sp) return std::nullopt;
        auto& lq = edit.getLaunchQuantisation(); te::BeatPosition q = sp->beat;
        for (int k = 0; k < 4; ++k) { q = lq.getNext (sp->beat); if (q >= te::BeatPosition::fromBeats (0.0)) break; }
        return te::MonotonicBeat { sp->monotonicBeat.v + (q - sp->beat) };
    }
    void startTransportIfNeeded()
    {
        auto& tc = edit.getTransport(); if (tc.isPlaying()) return;
        tc.setPosition (te::TimePosition()); tc.play (false);
    }
    void launch (int col, int row)
    {
        auto* c = clipAt (col, row); if (c == nullptr) return; auto lh = c->getLaunchHandle(); if (! lh) return;
        const int st = stateAt (col, row);
        if (st == 2 || st == 3) { lh->stop (launchPos()); repaint(); return; }
        // en una pista suena un solo clip a la vez: los demás se detienen al entrar éste
        if (auto* t = trackAt (col)) for (auto* s : t->getClipSlotList().getClipSlots()) if (s != nullptr && s->getClip() != nullptr && s->getClip() != c) if (auto oh = s->getClip()->getLaunchHandle()) if (oh->getPlayingStatus() == te::LaunchHandle::PlayState::playing || oh->getQueuedStatus().has_value()) oh->stop (launchPos());
        const bool wasPlaying = edit.getTransport().isPlaying();
        lh->play (wasPlaying ? launchPos() : std::nullopt); startTransportIfNeeded(); repaint();
    }
    void launchScene (int row) { for (int c = 0; c < kCols; ++c) if (clipAt (c, row) != nullptr && stateAt (c, row) != 2 && stateAt (c, row) != 3) launch (c, row); }
    void stopAll()
    {
        const auto pos = launchPos();
        for (int c = 0; c < kCols; ++c) for (int r = 0; r < kRows; ++r) if (auto* cl = clipAt (c, r)) if (auto lh = cl->getLaunchHandle()) if (stateAt (c, r) == 2 || stateAt (c, r) == 3) lh->stop (pos);
        repaint();
    }
    // Copia un clip de la pista (de la Playlist) a la celda; queda en bucle
    te::Clip* putClip (int col, int row, te::Clip& src)
    {
        auto* slot = slotAt (col, row); if (slot == nullptr) return nullptr; if (auto* old = slot->getClip()) old->removeFromParent();
        auto xml = src.state.createXml(); if (xml == nullptr) return nullptr; te::EditItemID::remapIDs (*xml, edit); auto vt = juce::ValueTree::fromXml (*xml);
        auto* nc = te::insertClipWithState (*slot, vt); if (nc == nullptr) return nullptr;
        const auto len = nc->getPosition().getLength(); nc->setStart (te::TimePosition(), false, true); nc->setLength (len, true);
        finishSlotClip (*nc); return nc;
    }
    te::Clip* makeEmptyMidi (int col, int row, double bars = 4.0)
    {
        auto* slot = slotAt (col, row); if (slot == nullptr) return nullptr; if (auto* old = slot->getClip()) old->removeFromParent();
        const double beats = bars * 4.0; auto mc = te::insertMIDIClip (*slot, juce::String::fromUTF8 ("Escena ") + juce::String (row + 1), edit.tempoSequence.toTime ({ te::BeatPosition::fromBeats (0.0), te::BeatPosition::fromBeats (beats) }));
        if (mc == nullptr) return nullptr; finishSlotClip (*mc); return mc.get();
    }
    void removeClip (int col, int row) { if (auto* c = clipAt (col, row)) { if (auto lh = c->getLaunchHandle()) lh->stop (std::nullopt); c->removeFromParent(); repaint(); } }
    void cycleQuant()
    {
        static const te::LaunchQType q[] { te::LaunchQType::bar, te::LaunchQType::half, te::LaunchQType::quarter, te::LaunchQType::none, te::LaunchQType::twoBars };
        auto& lq = edit.getLaunchQuantisation(); int i = 0; for (int k = 0; k < 5; ++k) if (lq.type.get() == q[k]) i = k; lq.type = q[(i + 1) % 5]; repaint();
    }
    juce::String quantName() const
    {
        switch (edit.getLaunchQuantisation().type.get()) { case te::LaunchQType::bar: return "1 compas"; case te::LaunchQType::half: return "1/2 compas"; case te::LaunchQType::quarter: return "1 tiempo"; case te::LaunchQType::twoBars: return "2 compases"; case te::LaunchQType::none: return "Inmediato"; default: return te::getName (edit.getLaunchQuantisation().type.get()); }
    }

protected:
    void paintBody (juce::Graphics& g) override
    {
        using namespace theme;
        const auto tr = te::getAudioTracks (edit); const float t = (float) juce::Time::getMillisecondCounter() * 0.001f;
        auto btn = [&] (juce::Rectangle<float> r, const juce::String& s, bool hot) { g.setColour (hot ? panelHi : boxFill); g.fillRoundedRectangle (r, 3.f); g.setColour (boxLine); g.drawRoundedRectangle (r.reduced (.5f), 3.f, 1.f); g.setColour (text); g.setFont (theme::fonts::ui (15.f)); g.drawText (s, r.toNearestInt(), juce::Justification::centred); };
        btn (stopR(), "Detener todo", hoverPos.x >= 0 && stopR().contains (hoverPos)); btn (quantR(), juce::String::fromUTF8 ("Entra en: ") + quantName(), hoverPos.x >= 0 && quantR().contains (hoverPos));
        btn (prevR(), "<", hoverPos.x >= 0 && prevR().contains (hoverPos)); btn (nextR(), ">", hoverPos.x >= 0 && nextR().contains (hoverPos));
        for (int c = 0; c < kCols; ++c)
        {
            auto h = juce::Rectangle<float> ((float) cellX (c), (float) (kGridY - 30), (float) kCellW, 24.f); const int ti = firstTrack + c;
            if (ti < tr.size()) { g.setColour (trackutil::colourOf (tr[ti], ti)); g.fillRoundedRectangle (h, 3.f); g.setColour (juce::Colour (0xe0000000)); g.setFont (theme::fonts::ui (13.f, true)); g.drawText (tr[ti]->getName() == "Track" ? juce::String::fromUTF8 ("Pista ") + juce::String (ti + 1) : tr[ti]->getName(), h.reduced (6, 0).toNearestInt(), juce::Justification::centredLeft); }
        }
        for (int r = 0; r < kRows; ++r)
        {
            auto sb = juce::Rectangle<float> (14.f, (float) cellY (r), (float) kGridX - 26.f, (float) kCellH); const bool hot = hoverPos.x >= 0 && sb.contains (hoverPos);
            g.setColour (hot ? accent : boxFill); g.fillRoundedRectangle (sb, 3.f); g.setColour (boxLine); g.drawRoundedRectangle (sb.reduced (.5f), 3.f, 1.f);
            juce::Path tri; const float cx = sb.getX() + 20.f, cy = sb.getCentreY(); tri.addTriangle (cx - 6.f, cy - 8.f, cx - 6.f, cy + 8.f, cx + 8.f, cy); g.setColour (hot ? juce::Colours::white : iconIdle); g.fillPath (tri);
            g.setColour (text); g.setFont (theme::fonts::ui (14.f)); g.drawText (juce::String (r + 1), sb.withTrimmedLeft (34.f).toNearestInt(), juce::Justification::centredLeft);
            for (int c = 0; c < kCols; ++c)
            {
                auto cell = juce::Rectangle<float> ((float) cellX (c), (float) cellY (r), (float) kCellW, (float) kCellH); const int st = stateAt (c, r); const bool exists = trackAt (c) != nullptr;
                if (! exists) { g.setColour (theme::bgGridAlt); g.fillRoundedRectangle (cell, 2.f); continue; }
                const auto col = trackutil::colourOf (trackAt (c), firstTrack + c);
                if (st == 0) { g.setColour (theme::bgGrid); g.fillRoundedRectangle (cell, 2.f); g.setColour (theme::seam); g.drawRoundedRectangle (cell.reduced (.5f), 3.f, 1.f); g.setColour (theme::barLine); g.drawLine (cell.getCentreX() - 6, cell.getCentreY(), cell.getCentreX() + 6, cell.getCentreY(), 1.6f); g.drawLine (cell.getCentreX(), cell.getCentreY() - 6, cell.getCentreX(), cell.getCentreY() + 6, 1.6f); continue; }
                const float pulse = st == 2 ? 0.5f + 0.5f * std::sin (t * 7.f) : (st == 3 || st == 4 ? (std::fmod (t, 0.5f) < 0.25f ? 1.f : 0.f) : 0.f);
                g.setColour (col.darker (.35f).withAlpha (st == 1 ? 0.55f : 0.95f)); g.fillRoundedRectangle (cell, 2.f);
                if (st != 1) { g.setColour (juce::Colours::white.withAlpha (0.10f + 0.18f * pulse)); g.fillRoundedRectangle (cell, 2.f); g.setColour (juce::Colours::white.withAlpha (0.6f + 0.4f * pulse)); g.drawRoundedRectangle (cell.reduced (1.f), 3.f, 2.f); }
                juce::Path tri; const float cx = cell.getX() + 14.f, cy = cell.getCentreY(); const bool playing = st == 2 || st == 4;
                if (playing) { g.setColour (juce::Colour (0xf0f2f3f5)); g.fillRect (cx - 6.f, cy - 6.f, 12.f, 12.f); } else { tri.addTriangle (cx - 5.f, cy - 7.f, cx - 5.f, cy + 7.f, cx + 7.f, cy); g.setColour (juce::Colour (0xf0f2f3f5)); g.fillPath (tri); }
                g.setColour (juce::Colour (0xf0f2f3f5)); g.setFont (theme::fonts::ui (12.5f, true)); auto* cl = clipAt (c, r); g.drawText (cl ? cl->getName() : juce::String(), cell.withTrimmedLeft (28.f).reduced (3, 0).toNearestInt(), juce::Justification::centredLeft, true);
            }
        }
    }
    void bodyDown (const juce::MouseEvent& e) override
    {
        const auto p = e.position;
        if (stopR().contains (p)) { stopAll(); return; } if (quantR().contains (p)) { cycleQuant(); return; }
        if (prevR().contains (p)) { firstTrack = std::max (0, firstTrack - 4); repaint(); return; } if (nextR().contains (p)) { firstTrack += 4; repaint(); return; }
        for (int r = 0; r < kRows; ++r)
        {
            if (juce::Rectangle<float> (14.f, (float) cellY (r), (float) kGridX - 26.f, (float) kCellH).contains (p)) { launchScene (r); return; }
            for (int c = 0; c < kCols; ++c) if (juce::Rectangle<float> ((float) cellX (c), (float) cellY (r), (float) kCellW, (float) kCellH).contains (p))
            {
                if (trackAt (c) == nullptr) return;
                if (e.mods.isPopupMenu() || clipAt (c, r) == nullptr) cellMenu (c, r); else launch (c, r);
                return;
            }
        }
    }
    juce::String tipAt (juce::Point<float> p) override
    {
        if (stopR().contains (p)) return juce::String::fromUTF8 ("Detiene todos los clips (al llegar al compás)");
        if (quantR().contains (p)) return juce::String::fromUTF8 ("Cuándo entra un clip que lanzas: haz clic para cambiar");
        if (prevR().contains (p) || nextR().contains (p)) return juce::String::fromUTF8 ("Ver otras pistas");
        return {};
    }

private:
    te::Edit& edit;
    juce::Rectangle<float> stopR() const { return { 14.f, 58.f, 130.f, 34.f }; }
    juce::Rectangle<float> quantR() const { return { 154.f, 58.f, 210.f, 34.f }; }
    juce::Rectangle<float> prevR() const { return { (float) designW - 150.f, 58.f, 60.f, 34.f }; }
    juce::Rectangle<float> nextR() const { return { (float) designW - 80.f, 58.f, 60.f, 34.f }; }
    static int cellX (int c) { return kGridX + c * (kCellW + kGap); }
    static int cellY (int r) { return kGridY + r * (kCellH + kGap); }
    void finishSlotClip (te::Clip& c)
    {   // el clip se repite mientras esté lanzado
        const auto len = c.getLengthInBeats();
        if (auto* mc = dynamic_cast<te::MidiClip*> (&c)) mc->setLoopRangeBeats ({ te::BeatPosition::fromBeats (0.0), te::BeatPosition::fromBeats (len.inBeats()) });
        else if (auto* wc = dynamic_cast<te::WaveAudioClip*> (&c)) wc->setLoopRangeBeats ({ te::BeatPosition::fromBeats (0.0), te::BeatPosition::fromBeats (len.inBeats()) });
    }
    void cellMenu (int col, int row)
    {
        auto* t = trackAt (col); if (t == nullptr) return; juce::PopupMenu m, from; auto clips = t->getClips(); const int n = std::min (clips.size(), 40);
        for (int k = 0; k < n; ++k) { auto* c = clips[k]; const double b = edit.tempoSequence.toBeats (c->getPosition().getStart()).inBeats(); from.addItem (100 + k, (c->getName().isEmpty() ? juce::String ("Clip") : c->getName()) + juce::String::fromUTF8 ("  (compás ") + juce::String ((int) std::floor (b / 4.0) + 1) + ")"); }
        if (n == 0) from.addItem (99, juce::String::fromUTF8 ("(esta pista no tiene clips en la Playlist)"), false);
        m.addSubMenu (juce::String::fromUTF8 ("Poner un clip de la Playlist"), from); m.addItem (1, juce::String::fromUTF8 ("Crear clip MIDI vacío (4 compases)")); if (clipAt (col, row)) { m.addSeparator(); m.addItem (2, "Quitar clip de esta celda"); }
        m.showMenuAsync (juce::PopupMenu::Options(), [this, col, row, t = te::AudioTrack::Ptr (t)] (int r)
        {
            if (r == 1) makeEmptyMidi (col, row); else if (r == 2) removeClip (col, row);
            else if (r >= 100 && r < 140) { auto cl = t->getClips(); if (r - 100 < cl.size()) putClip (col, row, *cl[r - 100]); }
            repaint();
        });
    }
    void timerCallback() override { if (isVisible()) repaint(); }
};
