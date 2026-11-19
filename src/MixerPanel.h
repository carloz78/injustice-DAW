#pragma once
#include "FloatingPanel.h"
#include <tracktion_engine/tracktion_engine.h>
#include <array>
namespace te = tracktion;

// Mixer: una pista = un canal. Volumen, pan, solo y mute actúan sobre el motor; los medidores leen el hilo de audio.
class MixerPanel : public FloatingPanel, private juce::Timer
{
public:
    static constexpr int kStripW = 76, kStride = 82, kTop = kHead + 10, kStripH = 470, kMasterW = 118, kX0 = 142, kFxW = 250;
    std::function<void()> onTracksChanged;

    explicit MixerPanel (te::Edit& e) : edit (e)
    {
        title = "Mixer"; icon = icons::Mixer; setSize (kX0 + 10 * kStride + 56 + kFxW + 14, kTop + kStripH + 14);
        syncClients(); startTimerHz (30);
    }
    ~MixerPanel() override { for (auto& c : clients) if (c.meter) c.meter->measurer.removeClient (c.client); }

protected:
    //==============================================================================
    void paintBody (juce::Graphics& g) override
    {
        using namespace theme;
        drawMaster (g);
        auto tracks = te::getAudioTracks (edit);
        for (int i = 0; i < 10; ++i) drawStrip (g, i, i < tracks.size() ? tracks[i] : nullptr);
        // botón +
        auto pr = plusR(); g.setColour (juce::Colour (0xff1f1d25)); g.fillRoundedRectangle (pr, 6.f); g.setColour (text);
        g.drawLine (pr.getCentreX() - 9, pr.getCentreY(), pr.getCentreX() + 9, pr.getCentreY(), 2.f); g.drawLine (pr.getCentreX(), pr.getCentreY() - 9, pr.getCentreX(), pr.getCentreY() + 9, 2.f);
        drawEffects (g);
    }
    void bodyDown (const juce::MouseEvent& e) override
    {
        auto p = e.position; auto tracks = te::getAudioTracks (edit); active = Hit {};
        if (plusR().contains (p)) { edit.ensureNumberOfAudioTracks (tracks.size() + 1); if (onTracksChanged) onTracksChanged(); repaint(); return; }
        if (masterFaderArea().expanded (14, 8).contains (p)) { active = { Hit::MasterFader, -1 }; dragFader (e, true); return; }
        for (int i = 0; i < tracks.size() && i < 10; ++i)
        {
            auto s = stripR (i);
            if (! s.contains (p)) continue;
            if (smR (i, 0).contains (p)) { tracks[i]->setSolo (! tracks[i]->isSolo (false)); repaint(); return; }
            if (smR (i, 1).contains (p)) { tracks[i]->setMute (! tracks[i]->isMuted (false)); repaint(); return; }
            if (knobR (i).expanded (6).contains (p)) { active = { Hit::Pan, i }; lastY = p.y; return; }
            if (faderArea (i).expanded (14, 8).contains (p)) { active = { Hit::Fader, i }; selected = i; dragFader (e, false); return; }
            selected = i; repaint(); return;
        }
    }
    void bodyDrag (const juce::MouseEvent& e) override
    {
        auto tracks = te::getAudioTracks (edit);
        if (active.kind == Hit::Fader || active.kind == Hit::MasterFader) dragFader (e, active.kind == Hit::MasterFader);
        else if (active.kind == Hit::Pan && active.idx < tracks.size()) { auto* vp = tracks[active.idx]->getVolumePlugin(); vp->setPan (juce::jlimit (-1.f, 1.f, vp->getPan() + (lastY - e.position.y) * 0.012f)); lastY = e.position.y; repaint(); }
    }
    void bodyUp (const juce::MouseEvent&) override { active = Hit {}; }
    void bodyDouble (const juce::MouseEvent& e) override
    {
        auto tracks = te::getAudioTracks (edit);
        if (masterFaderArea().expanded (14, 8).contains (e.position)) { edit.getMasterVolumePlugin()->setVolumeDb (0.f); repaint(); return; }
        for (int i = 0; i < tracks.size() && i < 10; ++i)
        {
            if (faderArea (i).expanded (14, 8).contains (e.position)) { tracks[i]->getVolumePlugin()->setVolumeDb (0.f); repaint(); return; }
            if (knobR (i).expanded (6).contains (e.position)) { tracks[i]->getVolumePlugin()->setPan (0.f); repaint(); return; }
        }
    }

private:
    struct Hit { enum Kind { None, Fader, MasterFader, Pan } kind = None; int idx = -1; } active;
    struct Client { te::LevelMeterPlugin* meter = nullptr; te::LevelMeasurer::Client client; float l = -100, r = -100; };
    te::Edit& edit; int selected = 0; float lastY = 0; std::array<Client, 10> clients; Client masterClient;

    // ---- geometría (coordenadas del panel)
    juce::Rectangle<float> stripR (int i) const { return { (float) (kX0 + i * kStride), (float) kTop, (float) kStripW, (float) kStripH }; }
    juce::Rectangle<float> knobR (int i) const { auto s = stripR (i); return { s.getCentreX() - 19, s.getY() + 54, 38, 38 }; }
    juce::Rectangle<float> faderArea (int i) const { auto s = stripR (i); return { s.getCentreX() - 6, s.getY() + 112, 12, s.getHeight() - 112 - 58 }; }
    juce::Rectangle<float> smR (int i, int k) const { auto s = stripR (i); return { s.getX() + 5 + k * 35.f, s.getBottom() - 40, 31, 28 }; }
    juce::Rectangle<float> plusR() const { return { (float) (kX0 + 10 * kStride + 4), (float) kTop, 40, 40 }; }
    juce::Rectangle<float> masterR() const { return { 12.f, (float) kTop, (float) kMasterW, (float) kStripH }; }
    juce::Rectangle<float> masterFaderArea() const { auto m = masterR(); return { m.getX() + 58, m.getY() + 112, 12, m.getHeight() - 112 - 24 }; }
    juce::Rectangle<float> fxR() const { return { (float) (getWidth() - kFxW - 12), (float) kTop, (float) kFxW, (float) kStripH }; }

    void dragFader (const juce::MouseEvent& e, bool master)
    {
        auto a = master ? masterFaderArea() : faderArea (active.idx);
        const float pos = juce::jlimit (0.f, 1.f, 1.f - (e.position.y - a.getY()) / a.getHeight());
        auto tracks = te::getAudioTracks (edit);
        const float db = pos <= 0.001f ? -100.f : te::volumeFaderPositionToDB (pos);
        if (master) edit.getMasterVolumePlugin()->setVolumeDb (db); else if (active.idx < tracks.size()) tracks[active.idx]->getVolumePlugin()->setVolumeDb (db);
        repaint();
    }

    void syncClients()
    {
        for (auto& c : clients) { if (c.meter) c.meter->measurer.removeClient (c.client); c.meter = nullptr; }
        auto tracks = te::getAudioTracks (edit);
        for (int i = 0; i < 10 && i < tracks.size(); ++i) if (auto* lm = tracks[i]->getLevelMeterPlugin()) { clients[(size_t) i].meter = lm; lm->measurer.addClient (clients[(size_t) i].client); }
    }
    void timerCallback() override
    {
        if (! isVisible()) return;
        const float fall = 26.f / 30.f;
        for (auto& c : clients)
        {
            if (! c.meter) continue;
            const float l = c.client.getAndClearAudioLevel (0).dB, r = c.client.getAndClearAudioLevel (1).dB;
            c.l = std::max (l, c.l - fall); c.r = std::max (r, c.r - fall);
        }
        pollMaster(); repaint();
    }

    //---- dibujo
    static juce::String dbText (float db) { return db <= -99 ? "-inf" : juce::String (db, 0); }
    void drawMeter (juce::Graphics& g, juce::Rectangle<float> r, float db)
    {
        g.setColour (juce::Colour (0xff0e0d11)); g.fillRoundedRectangle (r, 2.f);
        const float pos = db <= -90 ? 0.f : te::decibelsToVolumeFaderPosition (db);
        if (pos > 0) { g.setColour (db > -1.f ? juce::Colour (0xffe0306a) : theme::accent); g.fillRoundedRectangle (r.withTop (r.getBottom() - r.getHeight() * pos), 2.f); }
    }
    void drawFader (juce::Graphics& g, juce::Rectangle<float> a, float pos)
    {
        g.setColour (juce::Colour (0xff0b0a0e)); g.fillRoundedRectangle (a.withSizeKeepingCentre (6.f, a.getHeight()), 3.f);   // recorrido sin iluminar
        const float y = a.getBottom() - a.getHeight() * pos; juce::Rectangle<float> h (a.getCentreX() - 17, y - 12, 34, 24);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd9d8de), h.getX(), h.getY(), juce::Colour (0xff8d8b95), h.getX(), h.getBottom(), false)); g.fillRoundedRectangle (h, 4.f);
        g.setColour (juce::Colour (0xff3b3a42)); g.fillRect (h.getX() + 4, h.getCentreY() - 1, h.getWidth() - 8, 2.f);
    }
    void drawKnob (juce::Graphics& g, juce::Rectangle<float> r, float pan)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3942), r.getX(), r.getY(), juce::Colour (0xff1a191f), r.getX(), r.getBottom(), false)); g.fillEllipse (r);
        g.setColour (juce::Colour (0xff4c4b55)); g.drawEllipse (r.reduced (.5f), 1.2f);
        const float ang = juce::MathConstants<float>::pi * 0.75f * pan; const auto c = r.getCentre();
        g.setColour (juce::Colour (0xfff0eff4)); g.drawLine (c.x, c.y, c.x + std::sin (ang) * r.getWidth() * .38f, c.y - std::cos (ang) * r.getWidth() * .38f, 2.4f);
    }
    void drawStrip (juce::Graphics& g, int i, te::AudioTrack* t)
    {
        using namespace theme; auto s = stripR (i);
        g.setColour (juce::Colour (0xff1c1b21)); g.fillRoundedRectangle (s, 6.f);
        auto hd = s.withHeight (44.f); const bool has = t != nullptr; auto col = has ? trackColours[i % 8] : juce::Colour (0xff2b2a31);
        g.setColour (col); g.fillRoundedRectangle (hd, 6.f); g.fillRect (hd.withTop (hd.getY() + 20));
        g.setColour (has ? textOn (col) : textDim); g.setFont (juce::FontOptions (16.f, juce::Font::bold)); g.drawText (juce::String (i + 1), hd.toNearestInt(), juce::Justification::centred);
        if (i == selected && has) { g.setColour (juce::Colours::white.withAlpha (.8f)); g.drawRoundedRectangle (s.reduced (.5f), 6.f, 1.4f); }
        auto* vp = t ? t->getVolumePlugin() : nullptr;
        drawKnob (g, knobR (i), vp ? vp->getPan() : 0.f);
        g.setColour (textDim); g.setFont (juce::FontOptions (11.f)); g.drawText ("L", (int) s.getX() + 6, (int) s.getY() + 96, 14, 14, juce::Justification::centred); g.drawText ("R", (int) s.getRight() - 20, (int) s.getY() + 96, 14, 14, juce::Justification::centred);
        auto fa = faderArea (i); drawFader (g, fa, vp ? te::decibelsToVolumeFaderPosition (vp->getVolumeDb()) : te::decibelsToVolumeFaderPosition (0.f));
        auto& c = clients[(size_t) i];
        drawMeter (g, { fa.getCentreX() + 20, fa.getY(), 5, fa.getHeight() }, c.l); drawMeter (g, { fa.getCentreX() + 27, fa.getY(), 5, fa.getHeight() }, c.r);
        for (int k = 0; k < 2; ++k)
        {
            const bool on = t && (k == 0 ? t->isSolo (false) : t->isMuted (false)); auto b = smR (i, k);
            g.setColour (on ? accent : juce::Colour (0xff2a2930)); g.fillRoundedRectangle (b, 4.f);
            g.setColour (on ? juce::Colours::white : text); g.setFont (juce::FontOptions (14.f, juce::Font::bold)); g.drawText (k == 0 ? "S" : "M", b.toNearestInt(), juce::Justification::centred);
        }
    }
    void drawMaster (juce::Graphics& g)
    {
        using namespace theme; auto m = masterR();
        g.setColour (juce::Colour (0xff1c1b21)); g.fillRoundedRectangle (m, 6.f);
        g.setColour (text); g.setFont (juce::FontOptions (16.f, juce::Font::bold)); g.drawText ("Master", m.withHeight (44.f).toNearestInt(), juce::Justification::centred);
        auto fa = masterFaderArea(); g.setFont (juce::FontOptions (12.f)); g.setColour (textDim);
        for (float db : { 6.f, 0.f, -6.f, -12.f, -24.f, -36.f, -48.f })
        { const float y = fa.getBottom() - fa.getHeight() * te::decibelsToVolumeFaderPosition (db); g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), (int) m.getX() + 8, (int) y - 8, 34, 16, juce::Justification::centredRight); }
        g.drawText ("-inf", (int) m.getX() + 8, (int) fa.getBottom() - 8, 34, 16, juce::Justification::centredRight);
        drawFader (g, fa, te::decibelsToVolumeFaderPosition (edit.getMasterVolumePlugin()->getVolumeDb()));
        drawMeter (g, { fa.getCentreX() + 20, fa.getY(), 6, fa.getHeight() }, masterClient.l); drawMeter (g, { fa.getCentreX() + 29, fa.getY(), 6, fa.getHeight() }, masterClient.r);
    }
    void drawEffects (juce::Graphics& g)
    {
        using namespace theme; auto r = fxR();
        g.setColour (juce::Colour (0xff1c1b21)); g.fillRoundedRectangle (r, 8.f);
        auto tracks = te::getAudioTracks (edit); const int idx = juce::jlimit (0, juce::jmax (0, tracks.size() - 1), selected);
        g.setColour (text); g.setFont (juce::FontOptions (17.f, juce::Font::bold));
        g.drawText ("Efectos - " + (tracks.size() > idx ? juce::String (idx + 1) + " " + tracks[idx]->getName() : juce::String()), r.withHeight (50.f).reduced (14, 0).toNearestInt(), juce::Justification::centredLeft);
        float y = r.getY() + 58; int n = 0;
        if (tracks.size() > idx)
            for (auto* p : tracks[idx]->pluginList.getPlugins())
            {
                if (dynamic_cast<te::ExternalPlugin*> (p) == nullptr) continue; ++n;
                juce::Rectangle<float> row (r.getX() + 10, y, r.getWidth() - 20, 44);
                g.setColour (juce::Colour (0xff25232b)); g.fillRoundedRectangle (row, 6.f);
                g.setColour (p->isEnabled() ? accent : juce::Colour (0xff3a3943)); g.fillRoundedRectangle ({ row.getX() + 4, row.getY() + 4, 36, 36 }, 5.f);
                g.setColour (text); g.setFont (juce::FontOptions (15.f)); g.drawText (p->getName(), row.withLeft (row.getX() + 50).toNearestInt(), juce::Justification::centredLeft); y += 54;
            }
        juce::Rectangle<float> add (r.getX() + 10, y, r.getWidth() - 20, 44);
        g.setColour (juce::Colour (0xff25232b)); g.fillRoundedRectangle (add, 6.f); g.setColour (textDim); g.setFont (juce::FontOptions (15.f));
        g.drawText (juce::String::fromUTF8 ("+   Agregar efecto...  (próximamente)"), add.toNearestInt().reduced (12, 0), juce::Justification::centredLeft);
        if (n == 0) { g.setColour (textDim); g.setFont (juce::FontOptions (13.f)); g.drawText ("Sin efectos en este canal", r.withTop (add.getBottom() + 8).withHeight (22.f).toNearestInt(), juce::Justification::centred); }
    }
public:
    void pollMaster()   // el medidor del Master se lee con el mismo temporizador
    {
        if (auto* mt = edit.getMasterTrack())
        {
            if (masterMeter == nullptr)
            {
                for (auto* lm : mt->pluginList.getPluginsOfType<te::LevelMeterPlugin>()) { masterMeter = lm; break; }
                if (masterMeter == nullptr) { auto p = edit.getPluginCache().createNewPlugin (te::LevelMeterPlugin::xmlTypeName, {}); mt->pluginList.insertPlugin (p, -1, nullptr); masterMeter = dynamic_cast<te::LevelMeterPlugin*> (p.get()); }
                if (masterMeter) masterMeter->measurer.addClient (masterClient.client);
            }
        }
        if (masterMeter) { const float fall = 26.f / 30.f; masterClient.l = std::max (masterClient.client.getAndClearAudioLevel (0).dB, masterClient.l - fall); masterClient.r = std::max (masterClient.client.getAndClearAudioLevel (1).dB, masterClient.r - fall); }
    }
private:
    te::LevelMeterPlugin* masterMeter = nullptr;
};
