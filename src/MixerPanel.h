#pragma once
#include "FloatingPanel.h"
#include "InjLimiter.h"
#include "TrackUtil.h"
#include <tracktion_engine/tracktion_engine.h>
#include <array>
namespace te = tracktion;

// Mixer (ronda 3, paso 3): 10 canales a la vez, todos del mismo lila (sin colores por canal), Master morado a la derecha
// y a su lado la cadena de efectos del canal seleccionado (10 ranuras). Una pista = un canal; los medidores leen el hilo de audio.
class MixerPanel : public FloatingPanel, private juce::Timer
{
public:
    static constexpr int kStripW = 92, kStride = 95, kTop = kHead + 12, kStripH = 480, kMasterW = 92, kX0 = 12, kFxW = 268, kHdrH = 62, kCapH = 62, kN = 10, kBottomRow = 42;
    // Escala del fader: lineal en dB, de -60 (abajo, -inf) a +6 (arriba)
    static float dbToPos (float db) { return db <= -60.f ? 0.f : juce::jlimit (0.f, 1.f, (db + 60.f) / 66.f); }
    static float posToDb (float p) { return p <= 0.004f ? -100.f : p * 66.f - 60.f; }
    std::function<void (const juce::String&)> onMessage; std::function<void()> onTracksChanged;
    std::function<void (int)> onAddEffect;              // índice de pista
    std::function<void (te::Plugin*)> onOpenPlugin;
    int selected = 0, first = 0;   // first = primera pista visible
    void refreshTracks() { auto n = te::getAudioTracks (edit).size(); first = juce::jlimit (0, juce::jmax (0, n - kN), first); selected = juce::jlimit (0, juce::jmax (0, n - 1), selected); syncClients(); repaint(); }
    void showTrack (int t) { if (t < first) first = t; else if (t >= first + kN) first = t - (kN - 1); refreshTracks(); }

    //---- geometría (coordenadas de diseño); pública para las pruebas
    juce::Rectangle<float> stripR (int i) const { return { (float) (kX0 + i * kStride), (float) kTop, (float) kStripW, (float) kStripH }; }
    juce::Rectangle<float> masterR() const { return { (float) (kX0 + kN * kStride + 8), (float) kTop, (float) kMasterW, (float) kStripH }; }
    static juce::Rectangle<float> knobOf (juce::Rectangle<float> s) { return { s.getCentreX() - 22, s.getY() + kHdrH + 16, 44, 44 }; }
    static juce::Rectangle<float> faderOf (juce::Rectangle<float> s) { const float y0 = s.getY() + kHdrH + 16 + 44 + 30; return { s.getX() + 14, y0, 18, s.getBottom() - 118 - y0 }; }
    static juce::Rectangle<float> meterOf (juce::Rectangle<float> s) { auto f = faderOf (s); return { s.getX() + 46, f.getY() + 2, 30, f.getHeight() - 4 }; }
    static juce::Rectangle<float> peakOf (juce::Rectangle<float> s) { return { s.getX() + 2, knobOf (s).getBottom() + 4, s.getWidth() - 4, 20.f }; }
    juce::Rectangle<float> peakR (juce::Rectangle<float> s) const { return peakOf (s); }
    static juce::Rectangle<float> armOf (juce::Rectangle<float> s) { return { s.getCentreX() - 10, s.getBottom() - 104, 20, 20 }; }
    static juce::Rectangle<float> chevOf (juce::Rectangle<float> s) { return { s.getCentreX() - 14, s.getBottom() - 80, 28, 18 }; }
    static juce::Rectangle<float> smOf (juce::Rectangle<float> s, int k) { return { s.getX() + 8.f + k * 40.f, s.getBottom() - 46.f, 36.f, 32.f }; }
    juce::Rectangle<float> faderArea (int i) const { return faderOf (stripR (i)); }
    juce::Rectangle<float> smR (int i, int k) const { return smOf (stripR (i), k); }
    juce::Rectangle<float> masterFaderArea() const { return faderOf (masterR()); }

    explicit MixerPanel (te::Edit& e) : edit (e)
    {
        title = "Mixer"; icon = icons::Mixer; setDesign (kX0 + kN * kStride + 8 + kMasterW + 14 + kFxW + 14, kTop + kStripH + kBottomRow + 12);
        syncClients(); startTimerHz (30);
    }
    ~MixerPanel() override { for (auto& c : clients) if (c.meter) c.meter->measurer.removeClient (c.client); if (masterMeter) masterMeter->measurer.removeClient (masterClient.client); }

protected:
    void paintBody (juce::Graphics& g) override
    {
        using namespace theme;
        auto tracks = te::getAudioTracks (edit);
        for (int i = 0; i < kN; ++i) drawStrip (g, stripR (i), i + first, i + first < tracks.size() ? tracks[i + first] : nullptr, clients[(size_t) i].l, clients[(size_t) i].r, false);
        drawStrip (g, masterR(), -1, nullptr, masterClient.l, masterClient.r, true);
        // fila inferior: agregar canal + barra de desplazamiento
        auto pr = plusR(); g.setColour (panelHi); g.fillRoundedRectangle (pr, 7.f); icons::draw (g, icons::Plus, pr, text);
        auto sb = scrollR(); g.setColour (boxFill); g.fillRoundedRectangle (sb, 6.f);
        if (tracks.size() > kN) { const float w = std::max (40.f, sb.getWidth() * (float) kN / (float) tracks.size()), x = sb.getX() + (sb.getWidth() - w) * (float) first / (float) (tracks.size() - kN); g.setColour (juce::Colour (0xff9d9dab)); g.fillRoundedRectangle ({ x, sb.getY() + 3, w, sb.getHeight() - 6 }, 4.f); }
        drawEffects (g);
    }
    juce::String tipAt (juce::Point<float> p) override
    {
        for (int i = -1; i < kN; ++i)
        {
            const bool master = i < 0; auto s = master ? masterR() : stripR (i); if (! s.contains (p)) continue;
            if (s.withHeight ((float) kHdrH).contains (p)) return master ? juce::String ("Master") : juce::String ("Canal");
            if (knobOf (s).expanded (4).contains (p)) return "Pan";
            if (faderOf (s).expanded (22, 0).contains (p)) return "Volumen";
            if (meterOf (s).contains (p)) return "Nivel";
            if (armOf (s).expanded (4).contains (p)) return master ? juce::String() : juce::String::fromUTF8 ("Armar para grabar");
            if (chevOf (s).expanded (6, 4).contains (p)) return juce::String::fromUTF8 ("Menú del canal");
            if (smOf (s, 0).contains (p)) return master ? juce::String ("Limitador") : juce::String ("Solo");
            if (master && peakR (s).contains (p)) return juce::String::fromUTF8 ("Pico máximo (clic para reiniciar)");
            if (smOf (s, 1).contains (p)) return "Silenciar";
            return {};
        }
        if (plusR().contains (p)) return "Agregar canal";
        for (int n = 0; n < 10; ++n) if (fxRow (n).contains (p)) return juce::String::fromUTF8 ("Plugin VST3");
        return {};
    }
    void bodyDown (const juce::MouseEvent& e) override
    {
        auto p = e.position; auto tracks = te::getAudioTracks (edit); active = Hit {};
        if (plusR().contains (p)) { trackutil::ensureTracks (edit, tracks.size() + 1); selected = tracks.size(); showTrack (selected); if (onTracksChanged) onTracksChanged(); return; }
        if (tracks.size() > kN && scrollR().expanded (0, 6).contains (p)) { const float f = juce::jlimit (0.f, 1.f, (p.x - scrollR().getX()) / scrollR().getWidth()); first = juce::roundToInt (f * (float) (tracks.size() - kN)); refreshTracks(); return; }
        if (fxR().contains (p)) { fxClick (p); return; }
        if (masterR().contains (p))
        {
            auto s = masterR();
            if (faderOf (s).expanded (16, 8).contains (p)) { active = { Hit::MasterFader, -1 }; dragFader (e, true); }
            else if (knobOf (s).expanded (8).contains (p)) { active = { Hit::MasterPan, -1 }; lastY = p.y; }
            else if (smOf (s, 0).contains (p)) { setLimiter (! limiterOn()); repaint(); }
            else if (peakR (s).contains (p)) { masterPeak = -100.f; repaint(); }
            else if (smOf (s, 1).contains (p)) { auto mv = edit.getMasterVolumePlugin(); masterMuted = ! masterMuted; if (masterMuted) { preMute = mv->getVolumeDb(); mv->setVolumeDb (-100.f); } else mv->setVolumeDb (preMute); repaint(); }
            return;
        }
        for (int i = 0; i < kN && i + first < tracks.size(); ++i)
        {
            auto s = stripR (i); if (! s.contains (p)) continue;
            selected = i + first; auto* tk = tracks[i + first];
            if (smOf (s, 0).contains (p)) { tk->setSolo (! tk->isSolo (false)); repaint(); return; }
            if (smOf (s, 1).contains (p)) { tk->setMute (! tk->isMuted (false)); repaint(); return; }
            if (armOf (s).expanded (6).contains (p)) { const bool now = ! trackutil::isArmed (*tk); const int n = trackutil::armTrack (edit, *tk, now); if (now && n == 0 && onMessage) onMessage (trackutil::hasInstrument (*tk) ? juce::String::fromUTF8 ("No hay teclado MIDI conectado para esta pista") : juce::String::fromUTF8 ("No hay entrada de audio disponible (revisa Opciones > Ajustes de audio)")); repaint(); return; }
            if (chevOf (s).expanded (6, 4).contains (p)) { channelMenu (i + first, s); return; }
            if (knobOf (s).expanded (8).contains (p)) { active = { Hit::Pan, i }; lastY = p.y; return; }
            if (faderOf (s).expanded (18, 8).contains (p)) { active = { Hit::Fader, i }; dragFader (e, false); return; }
            repaint(); return;
        }
    }
    void bodyDrag (const juce::MouseEvent& e) override
    {
        auto tracks = te::getAudioTracks (edit);
        if (active.kind == Hit::Fader || active.kind == Hit::MasterFader) dragFader (e, active.kind == Hit::MasterFader);
        else if (active.kind == Hit::Pan && active.idx + first < tracks.size()) { auto* vp = tracks[active.idx + first]->getVolumePlugin(); vp->setPan (juce::jlimit (-1.f, 1.f, vp->getPan() + (lastY - e.position.y) * 0.014f)); lastY = e.position.y; repaint(); }
        else if (active.kind == Hit::MasterPan) { auto mv = edit.getMasterVolumePlugin(); mv->setPan (juce::jlimit (-1.f, 1.f, mv->getPan() + (lastY - e.position.y) * 0.014f)); lastY = e.position.y; repaint(); }
    }
    void bodyUp (const juce::MouseEvent&) override { active = Hit {}; }
    void bodyDouble (const juce::MouseEvent& e) override
    {
        auto tracks = te::getAudioTracks (edit);
        if (masterR().contains (e.position)) { auto s = masterR(); if (faderOf (s).expanded (16, 8).contains (e.position)) { edit.getMasterVolumePlugin()->setVolumeDb (0.f); masterMuted = false; repaint(); } else if (knobOf (s).expanded (8).contains (e.position)) { edit.getMasterVolumePlugin()->setPan (0.f); repaint(); } return; }
        for (int i = 0; i < kN && i + first < tracks.size(); ++i)
        {
            auto s = stripR (i);
            if (faderOf (s).expanded (18, 8).contains (e.position)) { tracks[i + first]->getVolumePlugin()->setVolumeDb (0.f); repaint(); return; }
            if (knobOf (s).expanded (8).contains (e.position)) { tracks[i + first]->getVolumePlugin()->setPan (0.f); repaint(); return; }
            if (s.withHeight ((float) kHdrH).contains (e.position)) { rename (i + first); return; }
        }
    }
    void bodyWheel (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        auto tracks = te::getAudioTracks (edit);
        if (tracks.size() > kN && (e.mods.isShiftDown() || e.position.y < kTop + kHdrH || scrollR().expanded (0, 8).contains (e.position))) { first -= (w.deltaY > 0 ? 1 : -1) * (w.deltaY == 0 ? 0 : 1); refreshTracks(); return; }
        for (int i = 0; i < kN && i + first < tracks.size(); ++i)
            if (stripR (i).contains (e.position))
            {
                auto* vp = tracks[i + first]->getVolumePlugin();
                if (knobOf (stripR (i)).expanded (8).contains (e.position)) vp->setPan (juce::jlimit (-1.f, 1.f, vp->getPan() + w.deltaY * 0.5f));
                else vp->setVolumeDb (posToDb (juce::jlimit (0.f, 1.f, dbToPos (vp->getVolumeDb()) + w.deltaY * 0.2f)));
                repaint(); return;
            }
    }

private:
    struct Hit { enum Kind { None, Fader, MasterFader, Pan, MasterPan } kind = None; int idx = -1; } active;
    struct Client { te::LevelMeterPlugin* meter = nullptr; te::LevelMeasurer::Client client; float l = -100, r = -100; };
    te::Edit& edit; float lastY = 0; std::array<Client, kN> clients; Client masterClient; te::LevelMeterPlugin* masterMeter = nullptr;
    bool masterMuted = false; float preMute = 0.f, masterPeak = -100.f;
    te::Plugin* findLimiter() { for (auto* p : edit.getMasterPluginList().getPlugins()) if (dynamic_cast<InjLimiter*> (p)) return p; return nullptr; }
public:
    bool limiterOn() { auto* p = findLimiter(); return p != nullptr && p->isEnabled(); }
    void setLimiter (bool on)
    {
        auto& ml = edit.getMasterPluginList(); auto* p = findLimiter();
        if (p == nullptr)
        {
            if (! on) return; auto np = edit.getPluginCache().createNewPlugin (InjLimiter::xmlTypeName, {}); if (np == nullptr) return;
            np->setEnabled (true); int at = -1; if (masterMeter) at = ml.indexOf (masterMeter); ml.insertPlugin (np, at, nullptr); return;
        }
        p->setEnabled (on);
    }
    float peakHold() const { return masterPeak; }
    InjLimiter* limiterPlugin() { return dynamic_cast<InjLimiter*> (findLimiter()); }
private:

    juce::Rectangle<float> scrollR() const { return { (float) kX0 + 46.f, (float) (kTop + kStripH + 8), (float) (kN * kStride - 52), 26.f }; }
    juce::Rectangle<float> plusR() const { return { (float) kX0, (float) (kTop + kStripH + 8), 38.f, 26.f }; }
    juce::Rectangle<float> fxR() const { return { (float) (designW - kFxW - 14), (float) kTop, (float) kFxW, (float) (kStripH + kBottomRow) }; }
    juce::Rectangle<float> fxRow (int n) const { auto r = fxR(); return { r.getX() + 10, r.getY() + 58 + n * 40.f, r.getWidth() - 20, 36 }; }

public:
    static constexpr int kBuses = 2;
    static juce::String busName (int b) { return juce::String::charToString ((juce::juce_wchar) ('A' + b)); }
    te::AuxSendPlugin* sendOf (te::AudioTrack& t, int bus) { for (auto* p : t.pluginList.getPlugins()) if (auto* sp = dynamic_cast<te::AuxSendPlugin*> (p)) if (sp->busNumber.get() == bus) return sp; return nullptr; }
    te::AuxSendPlugin* firstSend (te::AudioTrack& t) { for (auto* p : t.pluginList.getPlugins()) if (auto* sp = dynamic_cast<te::AuxSendPlugin*> (p)) return sp; return nullptr; }
    te::AuxReturnPlugin* returnOf (te::AudioTrack& t, int bus) { for (auto* p : t.pluginList.getPlugins()) if (auto* rp = dynamic_cast<te::AuxReturnPlugin*> (p)) if (rp->busNumber.get() == bus) return rp; return nullptr; }
    // Envío posterior al fader (-6 dB al crearlo) hacia el retorno `bus`; si ya existe, lo quita.
    void toggleSend (te::AudioTrack& t, int bus)
    {
        if (auto* sp = sendOf (t, bus)) { sp->deleteFromParent(); return; }
        auto np = edit.getPluginCache().createNewPlugin (te::AuxSendPlugin::xmlTypeName, {}); auto* sp = dynamic_cast<te::AuxSendPlugin*> (np.get()); if (sp == nullptr) return;
        sp->busNumber = bus; t.pluginList.insertPlugin (np, -1, nullptr); sp->setGainDb (-6.f);
    }
    // Convierte la pista en retorno: recibe lo que los envíos mandan y lo pasa por sus propios efectos.
    void toggleReturn (te::AudioTrack& t, int bus)
    {
        if (auto* rp = returnOf (t, bus)) { rp->deleteFromParent(); return; }
        auto np = edit.getPluginCache().createNewPlugin (te::AuxReturnPlugin::xmlTypeName, {}); auto* rp = dynamic_cast<te::AuxReturnPlugin*> (np.get()); if (rp == nullptr) return;
        rp->busNumber = bus; t.pluginList.insertPlugin (np, 0, nullptr);
    }
private:
    te::AudioTrack* trackAt (int i) { auto t = te::getAudioTracks (edit); return i >= 0 && i < t.size() ? t[i] : nullptr; }
    juce::Array<te::Plugin*> fxOf (te::AudioTrack* t) { juce::Array<te::Plugin*> a; if (t) for (auto* p : t->pluginList.getPlugins()) if (dynamic_cast<te::ExternalPlugin*> (p)) a.add (p); return a; }
    void channelMenu (int ti, juce::Rectangle<float> s)
    {
        auto* tk = trackAt (ti); if (! tk) return; te::AudioTrack::Ptr t (tk); juce::PopupMenu m;
        m.addItem (1, juce::String::fromUTF8 ("Renombrar…")); m.addItem (2, t->isMuted (false) ? "Quitar silencio" : "Silenciar"); m.addItem (3, t->isSolo (false) ? "Quitar solo" : "Solo");
        m.addSeparator(); m.addItem (4, juce::String::fromUTF8 ("Agregar efecto…"));
        { juce::PopupMenu sm; for (int b = 0; b < kBuses; ++b) sm.addItem (20 + b, juce::String::fromUTF8 ("Enviar al Retorno ") + busName (b), true, sendOf (*t, b) != nullptr);
          if (auto* any = firstSend (*t)) { sm.addSeparator(); static const float dbs[] { -24.f, -12.f, -6.f, 0.f }; for (int i = 0; i < 4; ++i) sm.addItem (40 + i, juce::String::fromUTF8 ("Nivel de envío: ") + juce::String (dbs[i], 0) + " dB", true, std::abs (any->getGainDb() - dbs[i]) < 1.f); }
          m.addSubMenu (juce::String::fromUTF8 ("Envíos"), sm); }
        { juce::PopupMenu rm; for (int b = 0; b < kBuses; ++b) rm.addItem (30 + b, juce::String::fromUTF8 ("Esta pista es el Retorno ") + busName (b), true, returnOf (*t, b) != nullptr); m.addSubMenu (juce::String::fromUTF8 ("Retornos (reverb, delay…)"), rm); }
        m.addItem (5, juce::String::fromUTF8 ("Salida: Master"), false);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (toLocal (chevOf (s)))), [this, t, ti] (int r)
        {
            if (r == 1) rename (ti); else if (r == 2) t->setMute (! t->isMuted (false)); else if (r == 3) t->setSolo (! t->isSolo (false)); else if (r == 4 && onAddEffect) onAddEffect (ti);
            else if (r >= 20 && r < 20 + kBuses) toggleSend (*t, r - 20);
            else if (r >= 30 && r < 30 + kBuses) toggleReturn (*t, r - 30);
            else if (r >= 40 && r < 44) { static const float dbs[] { -24.f, -12.f, -6.f, 0.f }; for (auto* p : t->pluginList.getPlugins()) if (auto* sp = dynamic_cast<te::AuxSendPlugin*> (p)) sp->setGainDb (dbs[r - 40]); }
            repaint();
        });
    }
    void fxClick (juce::Point<float> p)
    {
        auto* t = trackAt (selected); if (! t) return; auto fx = fxOf (t);
        for (int n = 0; n < 10; ++n)
        {
            auto row = fxRow (n); if (! row.contains (p)) continue;
            if (n < fx.size())
            {
                if (juce::Rectangle<float> (row.getX(), row.getY(), 40, row.getHeight()).contains (p)) { fx[n]->setEnabled (! fx[n]->isEnabled()); repaint(); }
                else if (juce::Rectangle<float> (row.getRight() - 36, row.getY(), 36, row.getHeight()).contains (p))
                {
                    te::Plugin::Ptr pl (fx[n]); juce::PopupMenu m; m.addItem (1, "Abrir"); m.addItem (2, "Quitar efecto");
                    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (toLocal (row))), [this, pl] (int r) { if (r == 1 && onOpenPlugin) onOpenPlugin (pl.get()); else if (r == 2) { pl->deleteFromParent(); repaint(); } });
                }
                else if (onOpenPlugin) onOpenPlugin (fx[n]);
            }
            else if (onAddEffect) onAddEffect (selected);
            return;
        }
    }
    void rename (int i) { trackutil::renameDialog (edit, trackAt (i), [this] { if (onTracksChanged) onTracksChanged(); repaint(); }); }
    void dragFader (const juce::MouseEvent& e, bool master)
    {
        auto a = master ? masterFaderArea() : faderArea (active.idx);
        const float pos = juce::jlimit (0.f, 1.f, 1.f - (e.position.y - a.getY() - kCapH * .5f) / (a.getHeight() - kCapH));   // el centro de la tapa recorre el slot
        const float db = pos <= 0.001f ? -100.f : posToDb (pos);
        if (master) { edit.getMasterVolumePlugin()->setVolumeDb (db); masterMuted = false; } else if (auto* t = trackAt (active.idx + first)) t->getVolumePlugin()->setVolumeDb (db);
        repaint();
    }
    void syncClients()
    {
        for (auto& c : clients) { if (c.meter) c.meter->measurer.removeClient (c.client); c.meter = nullptr; }
        auto tracks = te::getAudioTracks (edit);
        for (int i = 0; i < kN && i + first < tracks.size(); ++i) if (auto* lm = tracks[i + first]->getLevelMeterPlugin()) { clients[(size_t) i].meter = lm; lm->measurer.addClient (clients[(size_t) i].client); }
    }
    void pollMaster()
    {
        if (masterMeter == nullptr)
        {
            auto& mt = edit.getMasterPluginList();
            for (auto* lm : mt.getPluginsOfType<te::LevelMeterPlugin>()) { masterMeter = lm; break; }
            if (masterMeter == nullptr) { auto p = edit.getPluginCache().createNewPlugin (te::LevelMeterPlugin::xmlTypeName, {}); mt.insertPlugin (p, -1, nullptr); masterMeter = dynamic_cast<te::LevelMeterPlugin*> (p.get()); }
            if (masterMeter) masterMeter->measurer.addClient (masterClient.client);
        }
        if (masterMeter) { const float fall = 26.f / 30.f; masterPeak = std::max (masterPeak, std::max (masterClient.l, masterClient.r)); masterClient.l = std::max (masterClient.client.getAndClearAudioLevel (0).dB, masterClient.l - fall); masterClient.r = std::max (masterClient.client.getAndClearAudioLevel (1).dB, masterClient.r - fall); }
    }
    void timerCallback() override
    {
        if (! isVisible()) return;
        const float fall = 26.f / 30.f;
        bool changed = false;
        for (auto& c : clients) { if (! c.meter) continue; const float ol = c.l, orr = c.r; c.l = std::max (c.client.getAndClearAudioLevel (0).dB, c.l - fall); c.r = std::max (c.client.getAndClearAudioLevel (1).dB, c.r - fall); c.l = juce::jmax (c.l, -100.f); c.r = juce::jmax (c.r, -100.f); if (c.l != ol || c.r != orr) changed = true; }
        const float ml0 = masterClient.l, mr0 = masterClient.r; pollMaster(); masterClient.l = juce::jmax (masterClient.l, -100.f); masterClient.r = juce::jmax (masterClient.r, -100.f); if (ml0 != masterClient.l || mr0 != masterClient.r) changed = true;
        if (changed) repaint();   // sin sonido no hay nada que redibujar
    }

    //---- dibujo plano y pastel (sin brillos decorativos)
    void ledMeter (juce::Graphics& g, juce::Rectangle<float> r, float db)
    {
        constexpr int nSeg = 28; const float h = (r.getHeight() - (nSeg - 1) * 2.f) / nSeg; const float pos = db <= -90 ? 0.f : dbToPos (db);
        for (int s = 0; s < nSeg; ++s)
        {
            const float y = r.getBottom() - (s + 1) * h - s * 2.f, segDb = posToDb ((float) s / nSeg); const bool lit = pos > (float) (s + 1) / nSeg - .5f / nSeg;
            juce::Colour on = segDb > -1.5f ? juce::Colour (0xffff6b6b) : (segDb > -9.f ? juce::Colour (0xffe6e052) : juce::Colour (0xff85f855));
            g.setColour (lit ? on : juce::Colour (0xff282934)); g.fillRect (r.getX(), y, r.getWidth(), h);
        }
    }
    // Tapa del fader (muestreada de la referencia): bulbo redondeado y claro arriba, línea guía oscura al centro,
    // banda clara, banda oscura y cuerpo que vuelve a aclararse abajo. En el canal seleccionado se tiñe de lila.
    void cap (juce::Graphics& g, float cx, float cy, bool sel = false)
    {
        juce::Rectangle<float> r (cx - 18, cy - kCapH * .5f, 36, (float) kCapH);
        g.setColour (juce::Colour (0xff15151b)); g.fillRoundedRectangle (r.translated (0, 3.f).expanded (1, 0), 6.f);   // sombra sólida
        static const struct { float f; juce::uint32 c; } stops[] { { .03f, 0xff91969c }, { .17f, 0xffa1a4ab }, { .30f, 0xffc5c8cf }, { .34f, 0xffd3d6dd }, { .39f, 0xffb0b3ba }, { .43f, 0xffa6a9b0 }, { .48f, 0xff7f8289 }, { .52f, 0xff5a5d64 },
                                                                    { .57f, 0xff8a8d94 }, { .61f, 0xff7f8289 }, { .66f, 0xff464451 }, { .70f, 0xff34333f }, { .74f, 0xff3e3e48 }, { .79f, 0xff4d4d57 }, { .83f, 0xff60606a }, { .87f, 0xff6e6f78 }, { .91f, 0xff7e8188 }, { .97f, 0xff7b7e85 } };
        juce::ColourGradient gr (juce::Colour (stops[0].c), r.getX(), r.getY(), juce::Colour (stops[17].c), r.getX(), r.getBottom(), false);
        for (int i = 1; i < 17; ++i) { auto c = juce::Colour (stops[i].c); if (sel) c = c.interpolatedWith (juce::Colour (0xffc58cf0), .38f); gr.addColour (stops[i].f, c); }
        if (sel) { gr.point1 = r.getTopLeft(); }
        g.setGradientFill (gr);
        juce::Path bulb; bulb.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight() * .42f, 6.f, 6.f, true, true, false, false);
        juce::Path body; body.addRoundedRectangle (r.getX() + 2, r.getY() + r.getHeight() * .36f, r.getWidth() - 4, r.getHeight() * .64f, 5.f, 5.f, false, false, true, true);
        g.fillPath (bulb); g.fillPath (body);
        g.setColour (juce::Colour (0xff1c1c22)); g.fillRect (r.getX() + 3, cy - .5f, r.getWidth() - 6, 1.5f);          // línea guía
    }
    void faderTrack (juce::Graphics& g, juce::Rectangle<float> fa)
    {
        auto lane = fa.withSizeKeepingCentre (22, fa.getHeight() - 6); g.setColour (juce::Colour (0xff23242e)); g.fillRoundedRectangle (lane, 5.f);
        g.setColour (juce::Colour (0xff181920)); g.drawRoundedRectangle (lane.reduced (.5f), 5.f, 1.f);
        for (float db : { 6.f, 0.f, -6.f, -12.f, -24.f, -36.f, -48.f })   // marcas de dB atravesando el carril
        {
            const float y = fa.getBottom() - kCapH * .5f - (fa.getHeight() - kCapH) * dbToPos (db);
            g.setColour (db == 0.f ? juce::Colour (0xff8a899c) : juce::Colour (0xff3a3b48)); g.fillRect (lane.getX() + 1, y - .5f, lane.getWidth() - 2, 1.f);
        }
        g.setColour (juce::Colour (0xff6c6d80)); g.fillRoundedRectangle (fa.withSizeKeepingCentre (3, fa.getHeight() - 14), 1.5f);   // línea del carril
    }
    void knob (juce::Graphics& g, juce::Rectangle<float> r, float pan)
    {
        g.setColour (juce::Colour (0xff1b1b23)); g.fillEllipse (r);
        g.setColour (juce::Colour (0xff4a4a5c)); g.drawEllipse (r.reduced (1.f), 2.f);
        const float ang = juce::MathConstants<float>::pi * 0.75f * pan; const auto c = r.getCentre();
        g.setColour (juce::Colour (0xfff7e5fb)); g.drawLine (c.x + std::sin (ang) * r.getWidth() * .14f, c.y - std::cos (ang) * r.getWidth() * .14f, c.x + std::sin (ang) * r.getWidth() * .40f, c.y - std::cos (ang) * r.getWidth() * .40f, 2.6f);
    }
    void smButton (juce::Graphics& g, juce::Rectangle<float> r, const char* label, bool on, bool solo)
    {
        using namespace theme; g.setColour (on ? (solo ? juce::Colour (0xfff2ecf9) : accent) : juce::Colour (0xff464352)); g.fillRoundedRectangle (r, 5.f);
        g.setColour (on ? (solo ? juce::Colour (0xff3b2c4b) : juce::Colours::white) : juce::Colour (0xffe4e4e9)); g.setFont (juce::FontOptions (16.f, juce::Font::bold)); g.drawText (label, r.toNearestInt(), juce::Justification::centred);
    }
    void drawStrip (juce::Graphics& g, juce::Rectangle<float> s, int index, te::AudioTrack* t, float lvL, float lvR, bool master)
    {
        using namespace theme;
        const bool empty = ! master && t == nullptr;
        g.setColour (master ? juce::Colour (0xff3b2c4b) : juce::Colour (0xff343540)); g.fillRoundedRectangle (s, 6.f);
        auto hd = s.withHeight ((float) kHdrH); const auto hc = master ? juce::Colour (0xff6a368f) : (empty ? juce::Colour (0xff3d3e4b) : juce::Colour (0xff9d9dab));
        { juce::Path p; p.addRoundedRectangle (hd.getX(), hd.getY(), hd.getWidth(), hd.getHeight(), 6.f, 6.f, true, true, false, false); g.setColour (hc); g.fillPath (p); }
        const juce::String nm = master ? juce::String ("Master") : ((t && t->getName().isNotEmpty() && t->getName() != "Track") ? t->getName() : juce::String());
        g.setColour (master || ! empty ? juce::Colour (0xff1a1522) : textDim);
        if (master) { g.setColour (juce::Colours::white); g.setFont (juce::FontOptions (18.f, juce::Font::bold)); g.drawText ("Master", hd.toNearestInt(), juce::Justification::centred); }
        else
        {
            g.setFont (juce::FontOptions (19.f, juce::Font::bold)); g.drawText (juce::String (index + 1), hd.withTrimmedBottom (nm.isEmpty() ? 0.f : 24.f).toNearestInt(), juce::Justification::centred);
            if (nm.isNotEmpty()) { g.setFont (juce::FontOptions (16.f)); g.drawText (nm, hd.withTop (hd.getY() + 32).toNearestInt().reduced (3, 0), juce::Justification::centredTop); }
        }
        if (! master && index == selected && ! empty) { g.setColour (juce::Colour (0xffe4e4ee)); g.drawRoundedRectangle (s.reduced (.75f), 6.f, 1.5f); }
        te::VolumeAndPanPlugin* vp = master ? edit.getMasterVolumePlugin().get() : (t ? t->getVolumePlugin() : nullptr);
        knob (g, knobOf (s), vp ? vp->getPan() : 0.f);
        const bool sig = std::max (lvL, lvR) > -60.f;
        if (master) { const bool hot = masterPeak > -0.1f; g.setColour (hot ? record : textDim); g.setFont (juce::FontOptions (12.f, juce::Font::bold)); g.drawText (masterPeak <= -99.f ? juce::String ("pico -inf") : juce::String ("pico ") + juce::String (masterPeak, 1), peakR (s).toNearestInt(), juce::Justification::centred); }
        else { g.setColour (sig ? accentHi : juce::Colour (0xff637171)); g.fillEllipse (s.getCentreX() - 4.f, knobOf (s).getBottom() + 10.f, 8.f, 8.f); }
        auto fa = faderOf (s); faderTrack (g, fa);
        auto mr = meterOf (s); g.setColour (juce::Colour (0xff1b1b23)); g.fillRoundedRectangle (mr.expanded (3, 3), 3.f);
        ledMeter (g, { mr.getX(), mr.getY(), 12, mr.getHeight() }, lvL); ledMeter (g, { mr.getX() + 16, mr.getY(), 12, mr.getHeight() }, lvR);
        const float pos = vp ? dbToPos (master && masterMuted ? preMute : vp->getVolumeDb()) : dbToPos (0.f);
        cap (g, fa.getCentreX(), fa.getBottom() - kCapH * .5f - (fa.getHeight() - kCapH) * pos, ! master && index == selected && ! empty);
        if (! master && ! empty && t)
        {
            auto ar = armOf (s); const bool armed = (bool) t->state.getProperty ("injArm", false);
            g.setColour (armed ? record : juce::Colour (0xffb9b8c6)); if (armed) g.fillEllipse (ar.reduced (2)); else g.drawEllipse (ar.reduced (2), 1.6f);
        }
        else if (master) { auto ar = armOf (s); g.setColour (juce::Colour (0xffb9b8c6)); g.drawEllipse (ar.reduced (2), 1.6f); }
        { auto cv = chevOf (s); juce::Path p; p.startNewSubPath (cv.getCentreX() - 6, cv.getCentreY() - 3); p.lineTo (cv.getCentreX(), cv.getCentreY() + 3); p.lineTo (cv.getCentreX() + 6, cv.getCentreY() - 3); g.setColour (juce::Colour (0xffb9b8c6)); g.strokePath (p, juce::PathStrokeType (1.8f)); }
        if (! master) smButton (g, smOf (s, 0), "S", t && t->isSolo (false), true); else smButton (g, smOf (s, 0), "LIM", limiterOn(), false);
        smButton (g, smOf (s, 1), "M", master ? masterMuted : (t && t->isMuted (false)), false);
    }
    void drawEffects (juce::Graphics& g)
    {
        using namespace theme; auto r = fxR();
        g.setColour (juce::Colour (0xff383946)); g.fillRoundedRectangle (r, 8.f);
        auto* t = trackAt (selected); const juce::String nm = t ? (t->getName() == "Track" ? juce::String (selected + 1) : t->getName()) : juce::String();
        g.setColour (text); g.setFont (juce::FontOptions (18.f)); g.drawText ("Mixer - " + nm, r.withHeight (50.f).withTrimmedLeft (14).toNearestInt(), juce::Justification::centredLeft);
        auto fx = fxOf (t);
        for (int n = 0; n < 10; ++n)
        {
            auto row = fxRow (n); const bool used = n < fx.size();
            g.setColour (used ? juce::Colour (0xff2e2f3b) : juce::Colour (0xff2b2c37)); g.fillRoundedRectangle (row, 4.f);
            auto dot = juce::Rectangle<float> (row.getX() + 10, row.getCentreY() - 10, 20, 20);
            if (used) { const bool on = fx[n]->isEnabled(); g.setColour (on ? juce::Colour (0xffc58cf0) : juce::Colour (0xff5a5a6c)); g.fillEllipse (dot); }
            else { g.setColour (juce::Colour (0xff6a6a7c)); g.drawEllipse (dot.reduced (1), 1.6f); }
            g.setColour (used ? text : juce::Colour (0xff8b8b9d)); g.setFont (juce::FontOptions (16.f));
            g.drawText (used ? fx[n]->getName() : "Slot " + juce::String (n + 1), row.withTrimmedLeft (42).withTrimmedRight (40).toNearestInt(), juce::Justification::centredLeft);
            if (used) icons::draw (g, icons::Dots, { row.getRight() - 34, row.getY(), 28, row.getHeight() }, juce::Colour (0xffd9d8e3));
            else { juce::Path p; auto c = juce::Point<float> (row.getRight() - 20, row.getCentreY()); p.startNewSubPath (c.x - 5, c.y - 2.5f); p.lineTo (c.x, c.y + 2.5f); p.lineTo (c.x + 5, c.y - 2.5f); g.setColour (juce::Colour (0xff8b8b9d)); g.strokePath (p, juce::PathStrokeType (1.6f)); }
        }
    }
};
