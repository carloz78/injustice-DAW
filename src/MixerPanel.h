#pragma once
#include "FloatingPanel.h"
#include "TrackUtil.h"
#include <tracktion_engine/tracktion_engine.h>
#include <array>
namespace te = tracktion;

// Mixer: una pista = un canal. Volumen, pan, solo y mute actúan sobre el motor; los medidores leen el hilo de audio.
class MixerPanel : public FloatingPanel, private juce::Timer
{
public:
    static constexpr int kStripW = 78, kStride = 84, kTop = kHead + 10, kStripH = 486, kMasterW = 122, kX0 = 146, kFxW = 262, kHdrH = 54;
    // Escala del fader: lineal en dB, de -60 (abajo, -inf) a +6 (arriba), como en el diseño
    static float dbToPos (float db) { return db <= -60.f ? 0.f : juce::jlimit (0.f, 1.f, (db + 60.f) / 66.f); }
    static float posToDb (float p) { return p <= 0.004f ? -100.f : p * 66.f - 60.f; }
    std::function<void()> onTracksChanged;
    std::function<void (int)> onAddEffect;              // índice de pista
    std::function<void (te::Plugin*)> onOpenPlugin;
    int selected = 0, first = 0;   // first = primera pista visible (el mixer muestra 10 canales a la vez)
    void refreshTracks() { auto n = te::getAudioTracks (edit).size(); first = juce::jlimit (0, juce::jmax (0, n - 10), first); selected = juce::jlimit (0, juce::jmax (0, n - 1), selected); syncClients(); repaint(); }
    void showTrack (int t) { if (t < first) first = t; else if (t >= first + 10) first = t - 9; refreshTracks(); }

    explicit MixerPanel (te::Edit& e) : edit (e)
    {
        title = "Mixer"; icon = icons::Mixer; setDesign (kX0 + 10 * kStride + 58 + kFxW + 16, kTop + kStripH + 16);
        syncClients(); startTimerHz (30);
    }
    ~MixerPanel() override { for (auto& c : clients) if (c.meter) c.meter->measurer.removeClient (c.client); if (masterMeter) masterMeter->measurer.removeClient (masterClient.client); }

protected:
    void paintBody (juce::Graphics& g) override
    {
        using namespace theme;
        drawMaster (g);
        auto tracks = te::getAudioTracks (edit);
        for (int i = 0; i < 10; ++i) drawStrip (g, i, i + first < tracks.size() ? tracks[i + first] : nullptr);
        if (tracks.size() > 10) { auto sb = scrollR(); g.setColour (juce::Colour (0xff1a1920)); g.fillRoundedRectangle (sb, 3.f); const float w = sb.getWidth() * 10.f / tracks.size(); g.setColour (theme::accent.withAlpha (.8f)); g.fillRoundedRectangle ({ sb.getX() + (sb.getWidth() - w) * first / (float) (tracks.size() - 10), sb.getY(), w, sb.getHeight() }, 3.f); }
        auto pr = plusR(); g.setColour (juce::Colour (0xff1d1c22)); g.fillRoundedRectangle (pr, 8.f); g.setColour (juce::Colour (0xff34323a)); g.drawRoundedRectangle (pr.reduced (.5f), 8.f, 1.f);
        icons::draw (g, icons::Plus, pr, text);
        drawEffects (g);
    }
    void bodyDown (const juce::MouseEvent& e) override
    {
        auto p = e.position; auto tracks = te::getAudioTracks (edit); active = Hit {};
        if (plusR().contains (p)) { trackutil::ensureTracks (edit, tracks.size() + 1); selected = tracks.size(); showTrack (selected); if (onTracksChanged) onTracksChanged(); return; }
        if (tracks.size() > 10 && scrollR().expanded (0, 8).contains (p)) { const float f = juce::jlimit (0.f, 1.f, (p.x - scrollR().getX()) / scrollR().getWidth()); first = juce::roundToInt (f * (tracks.size() - 10)); refreshTracks(); return; }
        if (masterFaderArea().expanded (16, 8).contains (p)) { active = { Hit::MasterFader, -1 }; dragFader (e, true); return; }
        if (fxR().contains (p)) { fxClick (p); return; }
        for (int i = 0; i < 10 && i + first < tracks.size(); ++i)
        {
            auto s = stripR (i); if (! s.contains (p)) continue;
            selected = i + first; auto* tk = tracks[i + first];
            if (smR (i, 0).contains (p)) { tk->setSolo (! tk->isSolo (false)); repaint(); return; }
            if (smR (i, 1).contains (p)) { tk->setMute (! tk->isMuted (false)); repaint(); return; }
            if (knobR (i).expanded (8).contains (p)) { active = { Hit::Pan, i }; lastY = p.y; return; }
            if (faderArea (i).expanded (18, 8).contains (p)) { active = { Hit::Fader, i }; dragFader (e, false); return; }
            repaint(); return;
        }
    }
    void bodyDrag (const juce::MouseEvent& e) override
    {
        auto tracks = te::getAudioTracks (edit);
        if (active.kind == Hit::Fader || active.kind == Hit::MasterFader) dragFader (e, active.kind == Hit::MasterFader);
        else if (active.kind == Hit::Pan && active.idx + first < tracks.size()) { auto* vp = tracks[active.idx + first]->getVolumePlugin(); vp->setPan (juce::jlimit (-1.f, 1.f, vp->getPan() + (lastY - e.position.y) * 0.014f)); lastY = e.position.y; repaint(); }
    }
    void bodyUp (const juce::MouseEvent&) override { active = Hit {}; }
    void bodyDouble (const juce::MouseEvent& e) override
    {
        auto tracks = te::getAudioTracks (edit);
        if (masterFaderArea().expanded (16, 8).contains (e.position)) { edit.getMasterVolumePlugin()->setVolumeDb (0.f); repaint(); return; }
        for (int i = 0; i < 10 && i + first < tracks.size(); ++i)
        {
            if (faderArea (i).expanded (18, 8).contains (e.position)) { tracks[i + first]->getVolumePlugin()->setVolumeDb (0.f); repaint(); return; }
            if (knobR (i).expanded (8).contains (e.position)) { tracks[i + first]->getVolumePlugin()->setPan (0.f); repaint(); return; }
            if (stripR (i).withHeight ((float) kHdrH).contains (e.position)) { rename (i + first); return; }
        }
    }
    void bodyWheel (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        auto tracks = te::getAudioTracks (edit);
        if (tracks.size() > 10 && (e.mods.isShiftDown() || e.position.y < kTop + kHdrH || scrollR().expanded (0, 8).contains (e.position))) { first -= (w.deltaY > 0 ? 1 : -1) * (w.deltaY == 0 ? 0 : 1); refreshTracks(); return; }
        for (int i = 0; i < 10 && i + first < tracks.size(); ++i)
            if (stripR (i).contains (e.position))
            {
                auto* vp = tracks[i + first]->getVolumePlugin();
                if (knobR (i).expanded (8).contains (e.position)) vp->setPan (juce::jlimit (-1.f, 1.f, vp->getPan() + w.deltaY * 0.5f));
                else vp->setVolumeDb (posToDb (juce::jlimit (0.f, 1.f, dbToPos (vp->getVolumeDb()) + w.deltaY * 0.2f)));
                repaint(); return;
            }
    }

private:
    struct Hit { enum Kind { None, Fader, MasterFader, Pan } kind = None; int idx = -1; } active;
    struct Client { te::LevelMeterPlugin* meter = nullptr; te::LevelMeasurer::Client client; float l = -100, r = -100; };
    te::Edit& edit; float lastY = 0; std::array<Client, 10> clients; Client masterClient; te::LevelMeterPlugin* masterMeter = nullptr;

    //---- geometría (coordenadas de diseño)
    juce::Rectangle<float> stripR (int i) const { return { (float) (kX0 + i * kStride), (float) kTop, (float) kStripW, (float) kStripH }; }
    juce::Rectangle<float> knobR (int i) const { auto s = stripR (i); return { s.getCentreX() - 23, s.getY() + kHdrH + 14, 46, 46 }; }
    juce::Rectangle<float> faderArea (int i) const { auto s = stripR (i); return { s.getCentreX() - 18, s.getY() + kHdrH + 92, 26, s.getHeight() - kHdrH - 92 - 66 }; }
    juce::Rectangle<float> smR (int i, int k) const { auto s = stripR (i); return { s.getX() + 5.f + k * 36.f, s.getBottom() - 46, 32, 32 }; }
    juce::Rectangle<float> scrollR() const { return { (float) kX0, (float) (kTop + kStripH + 5), (float) (10 * kStride - 6), 6.f }; }
    juce::Rectangle<float> plusR() const { return { (float) (kX0 + 10 * kStride + 6), (float) kTop, 42, 42 }; }
    juce::Rectangle<float> masterR() const { return { 12.f, (float) kTop, (float) kMasterW, (float) kStripH }; }
    juce::Rectangle<float> masterFaderArea() const { auto m = masterR(); return { m.getX() + 66, m.getY() + 74, 26, m.getHeight() - 74 - 34 }; }
    juce::Rectangle<float> fxR() const { return { (float) (designW - kFxW - 14), (float) kTop, (float) kFxW, (float) kStripH }; }
    juce::Rectangle<float> fxRow (int n) const { auto r = fxR(); return { r.getX() + 12, r.getY() + 64 + n * 54.f, r.getWidth() - 24, 46 }; }

    te::AudioTrack* trackAt (int i) { auto t = te::getAudioTracks (edit); return i < t.size() ? t[i] : nullptr; }
    juce::Array<te::Plugin*> fxOf (te::AudioTrack* t) { juce::Array<te::Plugin*> a; if (t) for (auto* p : t->pluginList.getPlugins()) if (dynamic_cast<te::ExternalPlugin*> (p)) a.add (p); return a; }
    void fxClick (juce::Point<float> p)
    {
        auto* t = trackAt (selected); if (! t) return; auto fx = fxOf (t);
        for (int n = 0; n < fx.size(); ++n)
        {
            auto row = fxRow (n); if (! row.contains (p)) continue;
            if (juce::Rectangle<float> (row.getX() + 4, row.getY() + 4, 38, 38).contains (p)) { fx[n]->setEnabled (! fx[n]->isEnabled()); repaint(); }
            else if (onOpenPlugin) onOpenPlugin (fx[n]);
            return;
        }
        if (fxRow (fx.size()).contains (p) && onAddEffect) onAddEffect (selected);
    }
    void rename (int i) { trackutil::renameDialog (edit, trackAt (i), [this] { if (onTracksChanged) onTracksChanged(); repaint(); }); }
    void dragFader (const juce::MouseEvent& e, bool master)
    {
        auto a = master ? masterFaderArea() : faderArea (active.idx);
        const float pos = juce::jlimit (0.f, 1.f, 1.f - (e.position.y - a.getY() - 17.f) / (a.getHeight() - 34.f));   // el centro de la tapa recorre el slot
        const float db = pos <= 0.001f ? -100.f : posToDb (pos);
        if (master) edit.getMasterVolumePlugin()->setVolumeDb (db); else if (auto* t = trackAt (active.idx + first)) t->getVolumePlugin()->setVolumeDb (db);
        repaint();
    }
    void syncClients()
    {
        for (auto& c : clients) { if (c.meter) c.meter->measurer.removeClient (c.client); c.meter = nullptr; }
        auto tracks = te::getAudioTracks (edit);
        for (int i = 0; i < 10 && i + first < tracks.size(); ++i) if (auto* lm = tracks[i + first]->getLevelMeterPlugin()) { clients[(size_t) i].meter = lm; lm->measurer.addClient (clients[(size_t) i].client); }
    }
    void pollMaster()
    {
        if (auto* mt = edit.getMasterTrack(); mt && masterMeter == nullptr)
        {
            for (auto* lm : mt->pluginList.getPluginsOfType<te::LevelMeterPlugin>()) { masterMeter = lm; break; }
            if (masterMeter == nullptr) { auto p = edit.getPluginCache().createNewPlugin (te::LevelMeterPlugin::xmlTypeName, {}); mt->pluginList.insertPlugin (p, -1, nullptr); masterMeter = dynamic_cast<te::LevelMeterPlugin*> (p.get()); }
            if (masterMeter) masterMeter->measurer.addClient (masterClient.client);
        }
        if (masterMeter) { const float fall = 26.f / 30.f; masterClient.l = std::max (masterClient.client.getAndClearAudioLevel (0).dB, masterClient.l - fall); masterClient.r = std::max (masterClient.client.getAndClearAudioLevel (1).dB, masterClient.r - fall); }
    }
    void timerCallback() override
    {
        if (! isVisible()) return;
        const float fall = 26.f / 30.f;
        for (auto& c : clients) { if (! c.meter) continue; c.l = std::max (c.client.getAndClearAudioLevel (0).dB, c.l - fall); c.r = std::max (c.client.getAndClearAudioLevel (1).dB, c.r - fall); }
        pollMaster(); repaint();
    }

    //---- dibujo realista (sin brillos decorativos; recorrido del fader sin iluminar)
    void slot (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (juce::Colour (0xff09080b)); g.fillRoundedRectangle (r, r.getWidth() * .5f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (.55f), r.getX(), r.getY(), juce::Colours::transparentBlack, r.getX() + r.getWidth() * .5f, r.getY(), false)); g.fillRoundedRectangle (r, r.getWidth() * .5f);
        g.setColour (juce::Colour (0xff1f1d24)); g.drawRoundedRectangle (r.reduced (.5f), r.getWidth() * .5f, 1.f);
    }
    void meter (juce::Graphics& g, juce::Rectangle<float> r, float db)
    {
        g.setColour (juce::Colour (0xff0b0a0e)); g.fillRoundedRectangle (r, 2.f);
        const float pos = db <= -90 ? 0.f : dbToPos (db);
        if (pos > 0) { g.setColour (db > -1.f ? juce::Colour (0xffe0306a) : theme::accent); g.fillRoundedRectangle (r.withTop (r.getBottom() - r.getHeight() * pos), 2.f); }
    }
    void cap (juce::Graphics& g, float cx, float cy)
    {
        juce::Rectangle<float> r (cx - 24, cy - 17, 48, 34);
        g.setColour (juce::Colours::black.withAlpha (.55f)); g.fillRoundedRectangle (r.translated (0, 3), 6.f);
        juce::ColourGradient gr (juce::Colour (0xfff4f3f8), r.getX(), r.getY(), juce::Colour (0xffd9d7df), r.getX(), r.getBottom(), false); gr.addColour (.5, juce::Colour (0xffbdbbc6)); g.setGradientFill (gr); g.fillRoundedRectangle (r, 6.f);
        g.setColour (juce::Colour (0xff77757f)); g.drawRoundedRectangle (r.reduced (.5f), 6.f, 1.f);
        auto in = r.reduced (7, 7); g.setGradientFill (juce::ColourGradient (juce::Colour (0xff9d9ba6), in.getX(), in.getY(), juce::Colour (0xfff0eff5), in.getX(), in.getBottom(), false)); g.fillRoundedRectangle (in, 4.f);
        g.setColour (juce::Colour (0xff6a6873).withAlpha (.8f)); g.drawRoundedRectangle (in.reduced (.5f), 4.f, 1.f);
    }
    void knob (juce::Graphics& g, juce::Rectangle<float> r, float pan)
    {
        g.setColour (juce::Colours::black.withAlpha (.5f)); g.fillEllipse (r.translated (0, 2.f));
        g.setColour (juce::Colour (0xff0d0c10)); g.fillEllipse (r);
        auto in = r.reduced (3.5f); g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3c3a43), in.getCentreX(), in.getY(), juce::Colour (0xff131216), in.getCentreX(), in.getBottom(), false)); g.fillEllipse (in);
        g.setColour (juce::Colour (0xff55535e).withAlpha (.7f)); g.drawEllipse (in.reduced (.5f), 1.f);
        const float ang = juce::MathConstants<float>::pi * 0.75f * pan; const auto c = r.getCentre();
        g.setColour (juce::Colour (0xfff2f1f6)); g.drawLine (c.x + std::sin (ang) * r.getWidth() * .12f, c.y - std::cos (ang) * r.getWidth() * .12f, c.x + std::sin (ang) * r.getWidth() * .40f, c.y - std::cos (ang) * r.getWidth() * .40f, 2.4f);
    }
    void drawStrip (juce::Graphics& g, int i, te::AudioTrack* t)
    {
        using namespace theme; auto s = stripR (i);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff232127), s.getX(), s.getY(), juce::Colour (0xff17161a), s.getX(), s.getBottom(), false)); g.fillRoundedRectangle (s, 9.f);
        g.setColour (juce::Colour (0xff2e2c34)); g.drawRoundedRectangle (s.reduced (.5f), 9.f, 1.f);
        auto hd = s.withHeight ((float) kHdrH); const bool has = t != nullptr; auto col = has ? trackutil::colourOf (t, i + first) : juce::Colour (0xff2a282f);
        g.setGradientFill (juce::ColourGradient (col.brighter (.1f), hd.getX(), hd.getY(), col.darker (.12f), hd.getX(), hd.getBottom(), false)); g.fillRoundedRectangle (hd, 9.f); g.fillRect (hd.withTop (hd.getY() + 24));
        const juce::String nm = (has && t->getName().isNotEmpty() && t->getName() != "Track") ? t->getName() : juce::String();
        g.setColour (has ? textOn (col) : textDim); g.setFont (juce::FontOptions (18.f, juce::Font::bold));
        g.drawText (juce::String (i + first + 1), hd.withTrimmedBottom (nm.isEmpty() ? 0.f : 20.f).toNearestInt(), juce::Justification::centred);
        if (nm.isNotEmpty()) { g.setFont (juce::FontOptions (15.f)); g.drawText (nm, hd.withTop (hd.getY() + 28).toNearestInt().reduced (2, 0), juce::Justification::centredTop); }
        if (i + first == selected && has) { g.setColour (juce::Colours::white.withAlpha (.55f)); g.drawRoundedRectangle (s.reduced (.5f), 9.f, 1.4f); }
        auto* vp = t ? t->getVolumePlugin() : nullptr;
        knob (g, knobR (i), vp ? vp->getPan() : 0.f);
        g.setColour (text); g.setFont (juce::FontOptions (13.f, juce::Font::bold)); auto k = knobR (i);
        g.drawText ("L", (int) (k.getX() - 6), (int) k.getBottom() - 1, 16, 16, juce::Justification::centred); g.drawText ("R", (int) (k.getRight() - 10), (int) k.getBottom() - 1, 16, 16, juce::Justification::centred);
        auto fa = faderArea (i); slot (g, fa);
        auto& c = clients[(size_t) i]; meter (g, { fa.getRight() + 8, fa.getY() + 6, 5, fa.getHeight() - 12 }, c.l); meter (g, { fa.getRight() + 15, fa.getY() + 6, 5, fa.getHeight() - 12 }, c.r);
        const float pos = vp ? dbToPos (vp->getVolumeDb()) : dbToPos (0.f);
        cap (g, fa.getCentreX(), fa.getBottom() - 17 - (fa.getHeight() - 34) * pos);
        for (int b = 0; b < 2; ++b)
        {
            const bool on = t && (b == 0 ? t->isSolo (false) : t->isMuted (false)); auto r = smR (i, b);
            g.setColour (on ? accent : juce::Colour (0xff2a282f)); g.fillRoundedRectangle (r, 7.f);
            g.setColour (on ? accent.brighter (.2f) : juce::Colour (0xff3a3841)); g.drawRoundedRectangle (r.reduced (.5f), 7.f, 1.f);
            g.setColour (on ? juce::Colours::white : text); g.setFont (juce::FontOptions (16.f, juce::Font::bold)); g.drawText (b == 0 ? "S" : "M", r.toNearestInt(), juce::Justification::centred);
        }
    }
    void drawMaster (juce::Graphics& g)
    {
        using namespace theme; auto m = masterR();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff232127), m.getX(), m.getY(), juce::Colour (0xff17161a), m.getX(), m.getBottom(), false)); g.fillRoundedRectangle (m, 9.f);
        g.setColour (juce::Colour (0xff2e2c34)); g.drawRoundedRectangle (m.reduced (.5f), 9.f, 1.f); screws (g, m);
        g.setColour (text); g.setFont (juce::FontOptions (19.f, juce::Font::bold)); g.drawText ("Master", m.withTrimmedTop (14).withHeight (34.f).toNearestInt(), juce::Justification::centred);
        auto fa = masterFaderArea(); g.setFont (juce::FontOptions (14.f));
        for (float db : { 0.f, -6.f, -12.f, -18.f, -24.f, -30.f, -36.f, -42.f, -48.f, -54.f })
        {
            const float y = fa.getBottom() - 17 - (fa.getHeight() - 34) * dbToPos (db);
            g.setColour (text); g.drawText (juce::String ((int) db), (int) m.getX() + 8, (int) y - 9, 36, 18, juce::Justification::centredRight);
            g.setColour (juce::Colour (0xff4a4852)); g.fillRect (m.getX() + 50, y, 7.f, 1.f);
        }
        g.setColour (text); g.drawText (juce::String::fromUTF8 ("-∞"), (int) m.getX() + 8, (int) fa.getBottom() - 26, 36, 18, juce::Justification::centredRight);
        slot (g, fa); auto mr = fa.translated (36, 0); slot (g, mr);
        meter (g, { mr.getX() + 4, mr.getY() + 8, 8, mr.getHeight() - 16 }, masterClient.l); meter (g, { mr.getX() + 14, mr.getY() + 8, 8, mr.getHeight() - 16 }, masterClient.r);
        cap (g, fa.getCentreX(), fa.getBottom() - 17 - (fa.getHeight() - 34) * dbToPos (edit.getMasterVolumePlugin()->getVolumeDb()));
    }
    void drawEffects (juce::Graphics& g)
    {
        using namespace theme; auto r = fxR();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff222026), r.getX(), r.getY(), juce::Colour (0xff17161a), r.getX(), r.getBottom(), false)); g.fillRoundedRectangle (r, 10.f);
        g.setColour (juce::Colour (0xff2e2c34)); g.drawRoundedRectangle (r.reduced (.5f), 10.f, 1.f); screws (g, r);
        auto* t = trackAt (selected); const juce::String nm = t ? (t->getName() == "Track" ? juce::String (selected + 1) : t->getName()) : juce::String();
        g.setColour (text); g.setFont (juce::FontOptions (18.f, juce::Font::bold)); g.drawText ("Efectos - " + nm, r.withHeight (60.f).withTrimmedLeft (18).toNearestInt(), juce::Justification::centredLeft);
        auto fx = fxOf (t);
        for (int n = 0; n < fx.size(); ++n)
        {
            auto row = fxRow (n); g.setColour (juce::Colour (0xff2a282f)); g.fillRoundedRectangle (row, 9.f);
            auto pb = juce::Rectangle<float> (row.getX() + 5, row.getY() + 4, 38, 38); const bool on = fx[n]->isEnabled();
            g.setColour (on ? accent : juce::Colour (0xff3a3841)); g.fillRoundedRectangle (pb, 8.f); icons::draw (g, icons::Power, pb, on ? juce::Colours::white : textDim);
            g.setColour (text); g.setFont (juce::FontOptions (16.f)); g.drawText (fx[n]->getName(), row.withTrimmedLeft (54).withTrimmedRight (44).toNearestInt(), juce::Justification::centredLeft);
            icons::draw (g, icons::Sliders, { row.getRight() - 42, row.getY(), 36, row.getHeight() }, text);
        }
        auto add = fxRow (fx.size()); g.setColour (juce::Colour (0xff222026)); g.fillRoundedRectangle (add, 9.f); g.setColour (juce::Colour (0xff302e37)); g.drawRoundedRectangle (add.reduced (.5f), 9.f, 1.f);
        icons::draw (g, icons::Plus, { add.getX() + 4, add.getY(), 38, add.getHeight() }, text); g.setColour (textDim); g.setFont (juce::FontOptions (16.f)); g.drawText ("Agregar efecto...", add.withTrimmedLeft (54).toNearestInt(), juce::Justification::centredLeft);
    }
};
