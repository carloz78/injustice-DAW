#pragma once
#include "FloatingPanel.h"
#include <map>
#include "InjLimiter.h"
#include "InjStereo.h"
#include "FxAll.h"
#include "TrackUtil.h"
#include <tracktion_engine/tracktion_engine.h>
#include <array>
namespace te = tracktion;

// Mixer (ronda 3, paso 3): 10 canales a la vez, todos del mismo lila (sin colores por canal), Master morado a la derecha
// y a su lado la cadena de efectos del canal seleccionado (10 ranuras). Una pista = un canal; los medidores leen el hilo de audio.
class MixerPanel : public FloatingPanel, private juce::Timer
{
public:
    static constexpr int kStripW = 92, kStride = 95, kTop = kHead + 12, kStripH = 480, kMasterW = 92, kX0 = 12, kFxW = 268, kHdrH = 62, kCapH = 40, kMaxN = 15, kBottomRow = 42;
    // Escala del fader: lineal en dB, de -60 (abajo, -inf) a +6 (arriba)
    static float dbToPos (float db) { return db <= -60.f ? 0.f : juce::jlimit (0.f, 1.f, (db + 60.f) / 66.f); }
    static float posToDb (float p) { return p <= 0.004f ? -100.f : p * 66.f - 60.f; }
    std::function<void (const juce::String&)> onMessage; std::function<void()> onTracksChanged;
    std::function<void (int)> onAddEffect;              // índice de pista
    std::function<void (te::Plugin*)> onOpenPlugin;
    int selected = 0, first = 0;   // first = primera pista visible
    void refreshTracks() { auto n = te::getAudioTracks (edit).size(); first = juce::jlimit (0, juce::jmax (0, n - nv()), first); selected = juce::jlimit (0, juce::jmax (0, n - 1), selected); syncClients(); repaint(); }
    void showTrack (int t) { if (t < first) first = t; else if (t >= first + nv()) first = t - (nv() - 1); refreshTracks(); }

    bool compact = false;   // vista compacta: 15 canales angostos en lugar de 10
    int nv() const { return compact ? 15 : 10; } int stripW() const { return compact ? 60 : kStripW; } int stride() const { return compact ? 63 : kStride; }
    void setCompact (bool c) { compact = c; refreshTracks(); repaint(); }
    juce::Rectangle<float> compactR() const { return { (float) kX0 + 46.f, (float) (kTop + kStripH + 8), 70.f, 26.f }; }
    static bool narrow (juce::Rectangle<float> s) { return s.getWidth() < 80.f; }
    //---- geometría (coordenadas de diseño); pública para las pruebas
    juce::Rectangle<float> stripR (int i) const { return { (float) (kX0 + i * stride()), (float) kTop, (float) stripW(), (float) kStripH }; }
    juce::Rectangle<float> masterR() const { return { (float) (kX0 + nv() * stride() + 8), (float) kTop, (float) kMasterW, (float) kStripH }; }
    static juce::Rectangle<float> knobOf (juce::Rectangle<float> s) { const float k = narrow (s) ? 36.f : 44.f; return { s.getCentreX() - k * .5f, s.getY() + kHdrH + 16, k, k }; }
    static juce::Rectangle<float> faderOf (juce::Rectangle<float> s) { const float y0 = s.getY() + kHdrH + 16 + 44 + 26; return { s.getX() + (narrow (s) ? 7.f : 14.f), y0, 18, s.getBottom() - 112 - y0 }; }
    static juce::Rectangle<float> meterOf (juce::Rectangle<float> s) { auto f = faderOf (s); return { s.getX() + (narrow (s) ? 34.f : 67.f), f.getY() + 2, 21, f.getHeight() - 4 }; }
    static juce::Rectangle<float> peakOf (juce::Rectangle<float> s) { return { s.getX() + 2, s.getBottom() - 106, s.getWidth() - 4, 22.f }; }
    juce::Rectangle<float> peakR (juce::Rectangle<float> s) const { return peakOf (s); }
    static juce::Rectangle<float> armOf (juce::Rectangle<float> s) { return { s.getCentreX() - 10, s.getBottom() - 104, 20, 20 }; }
    static juce::Rectangle<float> scOf (juce::Rectangle<float> s) { if (narrow (s)) return {}; return { s.getRight() - 31, s.getBottom() - 104, 26, 20 }; }
    static juce::Rectangle<float> autoOf (juce::Rectangle<float> s) { if (narrow (s)) return {}; return { s.getX() + 6, s.getBottom() - 80, 22, 18 }; }
    static juce::Rectangle<float> sendKnobOf (juce::Rectangle<float> s) { if (narrow (s)) return {}; return { s.getX() + 6, s.getBottom() - 105, 22, 22 }; }
    juce::Rectangle<float> writeR() const { return { (float) (kX0 + nv() * stride() + 8), (float) (kTop + kStripH + 8), (float) kMasterW, 26.f }; }
    static juce::Rectangle<float> mixKnobOf (juce::Rectangle<float> row) { return { row.getRight() - 60, row.getCentreY() - 9, 18, 18 }; }
    static juce::Rectangle<float> chevOf (juce::Rectangle<float> s) { return { s.getCentreX() - 14, s.getBottom() - 80, 28, 18 }; }
    static juce::Rectangle<float> smOf (juce::Rectangle<float> s, int k) { return narrow (s) ? juce::Rectangle<float> { s.getX() + 3.f + k * 28.5f, s.getBottom() - 46.f, 26.f, 32.f } : juce::Rectangle<float> { s.getX() + 8.f + k * 40.f, s.getBottom() - 46.f, 36.f, 32.f }; }
    static juce::Rectangle<float> widthOf (juce::Rectangle<float> s) { if (narrow (s)) return {}; return { s.getX() + 24.f, knobOf (s).getBottom() + 8.f, s.getWidth() - 32.f, 16.f }; }
    juce::Rectangle<float> faderArea (int i) const { return faderOf (stripR (i)); }
    juce::Rectangle<float> smR (int i, int k) const { return smOf (stripR (i), k); }
    juce::Rectangle<float> masterFaderArea() const { return faderOf (masterR()); }

    explicit MixerPanel (te::Edit& e) : edit (e)
    {
        title = "Mixer"; icon = icons::Mixer; setDesign (kX0 + 10 * kStride + 8 + kMasterW + 14 + kFxW + 14, kTop + kStripH + kBottomRow + 12);
        syncClients(); startTimerHz (30);
    }
    ~MixerPanel() override { for (auto& c : clients) if (c.meter) c.meter->measurer.removeClient (c.client); if (masterMeter) masterMeter->measurer.removeClient (masterClient.client); }

protected:
    void paintBody (juce::Graphics& g) override
    {
        using namespace theme;
        auto tracks = te::getAudioTracks (edit);
        for (int i = 0; i < nv(); ++i) drawStrip (g, stripR (i), i + first, i + first < tracks.size() ? tracks[i + first] : nullptr, clients[(size_t) i].l, clients[(size_t) i].r, false, clients[(size_t) i].pl, clients[(size_t) i].pr);
        drawStrip (g, masterR(), -1, nullptr, masterClient.l, masterClient.r, true, masterClient.pl, masterClient.pr);
        // fila inferior: agregar canal + barra de desplazamiento
        auto pr = plusR(); raised (g, pr, panel); icons::draw (g, icons::Plus, pr, iconIdle);
        { auto wr = writeR(); const bool w = edit.getAutomationRecordManager().isWritingAutomation(); if (w) { g.setColour (record); g.fillRoundedRectangle (wr, rad); } else raised (g, wr, panel);
          g.setColour (w ? juce::Colours::white : textDim.brighter (.2f)); g.setFont (fonts::ui (11.5f, true)); g.drawText (juce::String::fromUTF8 ("● Escribir aut."), wr.toNearestInt(), juce::Justification::centred); }
        { auto cr = compactR(); if (compact) { g.setColour (accent); g.fillRoundedRectangle (cr, rad); } else raised (g, cr, panel); g.setColour (compact ? juce::Colour (0xff17181b) : textDim.brighter (.2f)); g.setFont (fonts::ui (12.f, true)); g.drawText ("Compacta", cr.toNearestInt(), juce::Justification::centred); }
        auto sb = scrollR(); well (g, sb);
        if (tracks.size() > nv()) { const float w = std::max (40.f, sb.getWidth() * (float) nv() / (float) tracks.size()), x = sb.getX() + (sb.getWidth() - w) * (float) first / (float) (tracks.size() - nv()); g.setColour (juce::Colour (0xff6a6d77)); g.fillRoundedRectangle ({ x, sb.getY() + 3, w, sb.getHeight() - 6 }, 2.f); }
        if (active.kind == Hit::Move && dragTarget >= 0) { auto d = stripR (dragTarget); g.setColour (accent.withAlpha (.25f)); g.fillRoundedRectangle (d, 3.f); g.setColour (accent); g.drawRoundedRectangle (d.reduced (1.f), 3.f, 2.f); }
        drawEffects (g);
    }
    juce::String tipAt (juce::Point<float> p) override
    {
        for (int i = -1; i < nv(); ++i)
        {
            const bool master = i < 0; auto s = master ? masterR() : stripR (i); if (! s.contains (p)) continue;
            if (s.withHeight ((float) kHdrH).contains (p)) return master ? juce::String ("Master") : juce::String ("Canal");
            if (knobOf (s).expanded (4).contains (p)) return "Pan";
            if (widthOf (s).contains (p)) return juce::String::fromUTF8 ("Separación estéreo (0 = mono, doble clic = 100 %)");
            if (faderOf (s).expanded (22, 0).contains (p)) return "Volumen";
            if (meterOf (s).contains (p)) return "Nivel";
            if (master && peakR (s).contains (p)) return juce::String::fromUTF8 ("Pico máximo (clic para reiniciar)");
            if (! master && scOf (s).contains (p)) return juce::String::fromUTF8 ("Sidechain");
            if (! master && autoOf (s).contains (p)) return juce::String::fromUTF8 ("Automatización: mostrar u ocultar la curva en la Playlist");
            if (! master && sendKnobShown (i + first) && sendKnobOf (s).contains (p)) return juce::String::fromUTF8 ("Nivel del envío desde el canal elegido (doble clic = -6 dB)");
            if (armOf (s).expanded (4).contains (p)) return master ? juce::String() : juce::String::fromUTF8 ("Armar para grabar");
            if (chevOf (s).expanded (6, 4).contains (p)) return juce::String::fromUTF8 ("Menú del canal");
            if (smOf (s, 0).contains (p)) return master ? juce::String ("Limitador") : juce::String ("Solo");
            if (master && peakR (s).contains (p)) return juce::String::fromUTF8 ("Pico máximo (clic para reiniciar)");
            if (smOf (s, 1).contains (p)) return "Silenciar";
            return {};
        }
        if (plusR().contains (p)) return "Agregar canal";
        if (compactR().contains (p)) return juce::String::fromUTF8 ("Vista compacta: 15 canales más angostos");
        if (writeR().contains (p)) return juce::String::fromUTF8 ("Escribir automatización: mover un fader o pan mientras suena la guarda como curva");
        { auto fx = currentFx(); for (int n = 0; n < fx.size() && n < 10; ++n) if (asExt (fx[n]) && mixKnobOf (fxRow (n)).contains (p)) return juce::String::fromUTF8 ("Mezcla del efecto: ") + juce::String (juce::roundToInt (mixOf (fx[n]) * 100.f)) + juce::String::fromUTF8 (" % (doble clic = 100 %)"); }
        if (eqPhaseR().contains (p)) return juce::String::fromUTF8 ("Invertir fase del canal");
        if (eqPowerR().contains (p)) return juce::String::fromUTF8 ("Activar o apagar el ecualizador");
        if (eqGraphR().contains (p)) return juce::String::fromUTF8 ("Arrastra los puntos (frecuencia y ganancia); rueda = Q; doble clic = reiniciar");
        for (int n = 0; n < 10; ++n) if (fxRow (n).contains (p)) return juce::String::fromUTF8 ("Plugin VST3");
        return {};
    }
    void bodyDown (const juce::MouseEvent& e) override
    {
        auto p = e.position; auto tracks = te::getAudioTracks (edit); active = Hit {};
        if (compactR().contains (p)) { setCompact (! compact); return; }
        if (writeR().contains (p)) { auto& am = edit.getAutomationRecordManager(); am.setWritingAutomation (! am.isWritingAutomation()); repaint(); return; }
        if (plusR().contains (p)) { trackutil::ensureTracks (edit, tracks.size() + 1); selected = tracks.size(); showTrack (selected); if (onTracksChanged) onTracksChanged(); return; }
        if (tracks.size() > nv() && scrollR().expanded (0, 6).contains (p)) { const float f = juce::jlimit (0.f, 1.f, (p.x - scrollR().getX()) / scrollR().getWidth()); first = juce::roundToInt (f * (float) (tracks.size() - nv())); refreshTracks(); return; }
        if (fxR().contains (p) && (eqGraphR().expanded (0, 6).contains (p) || eqTitleR().contains (p))) { eqClick (p, e); return; }
        if (fxR().contains (p)) { fxClick (p); return; }
        if (masterR().contains (p))
        {
            auto s = masterR(); masterSel = true; repaint();
            if (widthOf (s).expanded (2).contains (p)) { active = { Hit::Width, -1 }; widthDrag (e, -1, true); return; }
            if (faderOf (s).expanded (16, 8).contains (p)) { active = { Hit::MasterFader, -1 }; beginGesture (edit.getMasterVolumePlugin()->volParam.get()); dragFader (e, true); }
            else if (knobOf (s).expanded (8).contains (p)) { active = { Hit::MasterPan, -1 }; lastY = p.y; beginGesture (edit.getMasterVolumePlugin()->panParam.get()); }
            else if (smOf (s, 0).contains (p)) { setLimiter (! limiterOn()); repaint(); }
            else if (peakR (s).contains (p)) { masterPeak = -100.f; repaint(); }
            else if (smOf (s, 1).contains (p)) { auto mv = edit.getMasterVolumePlugin(); masterMuted = ! masterMuted; if (masterMuted) { preMute = mv->getVolumeDb(); mv->setVolumeDb (-100.f); } else mv->setVolumeDb (preMute); repaint(); }
            return;
        }
        for (int i = 0; i < nv() && i + first < tracks.size(); ++i)
        {
            auto s = stripR (i); if (! s.contains (p)) continue;
            selected = i + first; masterSel = false; auto* tk = tracks[i + first];
            if (e.mods.isPopupMenu()) { stripMenu (i + first); return; }
            if (s.withHeight ((float) kHdrH).contains (p)) { active = { Hit::Move, i }; dragTarget = i; repaint(); return; }
            if (sendKnobShown (i + first) && sendKnobOf (s).expanded (3).contains (p)) { active = { Hit::Send, i }; lastY = p.y; return; }
            if (autoOf (s).expanded (2).contains (p)) { tk->state.setProperty ("injAuto", ! (bool) tk->state.getProperty ("injAuto", false), nullptr); if (onTracksChanged) onTracksChanged(); repaint(); return; }
            if (scOf (s).expanded (2).contains (p)) { sidechainPopup (i + first, s); return; }
            if (smOf (s, 0).contains (p))
            {   // Ctrl + clic en S = solo exclusivo (apaga el solo de las demás)
                const bool turnOn = ! tk->isSolo (false);
                if (e.mods.isCtrlDown() || e.mods.isCommandDown()) for (auto* o : tracks) o->setSolo (turnOn && o == tk);
                else tk->setSolo (turnOn);
                repaint(); return;
            }
            if (smOf (s, 1).contains (p)) { tk->setMute (! tk->isMuted (false)); repaint(); return; }
            if (armOf (s).expanded (6).contains (p)) { const bool now = ! trackutil::isArmed (*tk); const int n = trackutil::armTrack (edit, *tk, now); if (now && n == 0 && onMessage) onMessage (trackutil::hasInstrument (*tk) ? juce::String::fromUTF8 ("No hay teclado MIDI conectado para esta pista") : juce::String::fromUTF8 ("No hay entrada de audio disponible (revisa Opciones > Ajustes de audio)")); repaint(); return; }
            if (chevOf (s).expanded (6, 4).contains (p)) { channelMenu (i + first, s); return; }
            if (widthOf (s).expanded (2).contains (p)) { active = { Hit::Width, i }; widthDrag (e, i + first, false); return; }
            if (knobOf (s).expanded (8).contains (p)) { active = { Hit::Pan, i }; lastY = p.y; beginGesture (tk->getVolumePlugin()->panParam.get()); return; }
            if (faderOf (s).expanded (18, 8).contains (p)) { active = { Hit::Fader, i }; beginGesture (tk->getVolumePlugin()->volParam.get()); dragFader (e, false); return; }
            repaint(); return;
        }
    }
    void bodyDrag (const juce::MouseEvent& e) override
    {
        auto tracks = te::getAudioTracks (edit);
        if (active.kind == Hit::Eq) { eqDrag (e); return; }
        if (active.kind == Hit::FxMix && mixPlugin != nullptr) { setMixOf (mixPlugin.get(), mixOf (mixPlugin.get()) + (lastY - e.position.y) * 0.012f); lastY = e.position.y; repaint(); return; }
        if (active.kind == Hit::Send) { sendDrag (e); return; }
        if (active.kind == Hit::Move) { dragTarget = juce::jlimit (0, juce::jmax (0, std::min (nv(), tracks.size() - first) - 1), (int) ((e.position.x - kX0) / stride())); repaint(); return; }
        if (active.kind == Hit::Width) { widthDrag (e, active.idx + first, active.idx < 0); return; }
        if (active.kind == Hit::Fader || active.kind == Hit::MasterFader) dragFader (e, active.kind == Hit::MasterFader);
        else if (active.kind == Hit::Pan && active.idx + first < tracks.size()) { auto* vp = tracks[active.idx + first]->getVolumePlugin(); setLinkedPan (*tracks[active.idx + first], juce::jlimit (-1.f, 1.f, vp->getPan() + (lastY - e.position.y) * 0.014f)); lastY = e.position.y; repaint(); }
        else if (active.kind == Hit::MasterPan) { auto mv = edit.getMasterVolumePlugin(); mv->setPan (juce::jlimit (-1.f, 1.f, mv->getPan() + (lastY - e.position.y) * 0.014f)); lastY = e.position.y; repaint(); }
    }
    void bodyUp (const juce::MouseEvent&) override
    {
        if (active.kind == Hit::Move && dragTarget >= 0 && dragTarget != active.idx) moveChannelTo (active.idx + first, dragTarget + first);
        endGesture(); active = Hit {}; eqBand = -1; dragTarget = -1; mixPlugin = nullptr; repaint();
    }
    void bodyDouble (const juce::MouseEvent& e) override
    {
        auto tracks = te::getAudioTracks (edit);
        if (fxR().contains (e.position)) { auto fx = currentFx(); for (int n = 0; n < fx.size() && n < 10; ++n) if (asExt (fx[n]) && mixKnobOf (fxRow (n)).expanded (3).contains (e.position)) { setMixOf (fx[n], 1.f); repaint(); return; } }
        for (int i = 0; i < nv() && i + first < tracks.size(); ++i) if (sendKnobShown (i + first) && sendKnobOf (stripR (i)).expanded (3).contains (e.position)) { if (auto* sp = sendFromSelected (i + first)) { sp->setGainDb (-6.f); repaint(); } return; }
        if (eqGraphR().contains (e.position)) { if (auto* eq = eqTarget (false)) { const int b = eqNodeAt (e.position); if (b >= 0) { auto bd = bandOf (*eq, b); bd.g->setParameter (0.f, juce::sendNotification); bd.q->setParameter (1.f, juce::sendNotification); } else for (int k = 0; k < 4; ++k) { auto bd = bandOf (*eq, k); bd.g->setParameter (0.f, juce::sendNotification); } } repaint(); return; }
        if (masterR().contains (e.position) && widthOf (masterR()).expanded (2).contains (e.position)) { setWidthMaster (1.f); repaint(); return; }
        if (masterR().contains (e.position)) { auto s = masterR(); if (faderOf (s).expanded (16, 8).contains (e.position)) { edit.getMasterVolumePlugin()->setVolumeDb (0.f); masterMuted = false; repaint(); } else if (knobOf (s).expanded (8).contains (e.position)) { edit.getMasterVolumePlugin()->setPan (0.f); repaint(); } return; }
        for (int i = 0; i < nv() && i + first < tracks.size(); ++i)
        {
            auto s = stripR (i);
            if (widthOf (s).expanded (2).contains (e.position)) { for (auto* o : linkedWith (*tracks[i + first])) setWidthOfTrack (*o, 1.f); repaint(); return; }
            if (faderOf (s).expanded (18, 8).contains (e.position)) { setLinkedVolume (*tracks[i + first], 0.f); repaint(); return; }
            if (knobOf (s).expanded (8).contains (e.position)) { setLinkedPan (*tracks[i + first], 0.f); repaint(); return; }
            if (s.withHeight ((float) kHdrH).contains (e.position)) { rename (i + first); return; }
        }
    }
    void bodyWheel (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        auto tracks = te::getAudioTracks (edit);
        if (eqGraphR().contains (e.position)) { const int b = eqNodeAt (e.position); if (b >= 0) if (auto* eq = eqTarget (true)) { auto bd = bandOf (*eq, b); bd.q->setParameter (juce::jlimit (0.1f, 4.f, bd.q->getCurrentValue() * (1.f + w.deltaY * .6f)), juce::sendNotification); repaint(); } return; }
        if (tracks.size() > nv() && (e.mods.isShiftDown() || e.position.y < kTop + kHdrH || scrollR().expanded (0, 8).contains (e.position))) { first -= (w.deltaY > 0 ? 1 : -1) * (w.deltaY == 0 ? 0 : 1); refreshTracks(); return; }
        for (int i = 0; i < nv() && i + first < tracks.size(); ++i)
            if (stripR (i).contains (e.position))
            {
                auto* vp = tracks[i + first]->getVolumePlugin();
                if (knobOf (stripR (i)).expanded (8).contains (e.position)) setLinkedPan (*tracks[i + first], juce::jlimit (-1.f, 1.f, vp->getPan() + w.deltaY * 0.5f));
                else setLinkedVolume (*tracks[i + first], posToDb (juce::jlimit (0.f, 1.f, dbToPos (vp->getVolumeDb()) + w.deltaY * 0.2f)));
                repaint(); return;
            }
    }

private:
    struct Hit { enum Kind { None, Fader, MasterFader, Pan, MasterPan, Width, Eq, Move, FxMix, Send } kind = None; int idx = -1; } active;
    struct Client { te::LevelMeterPlugin* meter = nullptr; te::LevelMeasurer::Client client; float l = -100, r = -100, pl = -100, pr = -100; int hl = 0, hr = 0; };   // pl/pr: marca de pico que se queda un momento y luego cae
    te::Plugin::Ptr mixPlugin; te::AutomatableParameter* gestureParam = nullptr; bool masterSel = false; int dragTarget = -1; int eqBand = -1; te::Edit& edit; float lastY = 0; std::array<Client, kMaxN> clients; Client masterClient; te::LevelMeterPlugin* masterMeter = nullptr;
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

    juce::Rectangle<float> scrollR() const { return { (float) kX0 + 122.f, (float) (kTop + kStripH + 8), (float) (nv() * stride() - 128), 26.f }; }
    juce::Rectangle<float> plusR() const { return { (float) kX0, (float) (kTop + kStripH + 8), 38.f, 26.f }; }
    juce::Rectangle<float> fxR() const { return { (float) (designW - kFxW - 14), (float) kTop, (float) kFxW, (float) (kStripH + kBottomRow) }; }
    juce::Rectangle<float> fxRow (int n) const { auto r = fxR(); return { r.getX() + 10, r.getY() + 46 + n * 27.f, r.getWidth() - 20, 24 }; }
    juce::Rectangle<float> eqTitleR() const { auto r = fxR(); return { r.getX() + 10, r.getY() + 46 + 10 * 27.f + 6, r.getWidth() - 20, 20 }; }
    juce::Rectangle<float> eqGraphR() const { auto r = eqTitleR(); return { r.getX(), r.getBottom() + 4, r.getWidth(), fxR().getBottom() - 10 - (r.getBottom() + 4) }; }
    juce::Rectangle<float> eqPowerR() const { auto r = eqTitleR(); return { r.getX() + 4, r.getY() + 1, 18, 18 }; }
    juce::Rectangle<float> eqPhaseR() const { auto r = eqTitleR(); return { r.getRight() - 24, r.getY() - 1, 24, 22 }; }

public:
    InjStereo* stereoOfList (te::PluginList& pl, te::Plugin* before, bool create)
    {
        for (auto* p : pl.getPlugins()) if (auto* st = dynamic_cast<InjStereo*> (p)) return st;
        if (! create) return nullptr;
        auto np = edit.getPluginCache().createNewPlugin (InjStereo::xmlTypeName, {}); auto* st = dynamic_cast<InjStereo*> (np.get()); if (st == nullptr) return nullptr;
        const int vi = before ? pl.indexOf (before) : -1; pl.insertPlugin (np, vi, nullptr); return st;   // justo antes del volumen
    }
    InjStereo* stereoOf (te::AudioTrack& t, bool create) { return stereoOfList (t.pluginList, t.getVolumePlugin(), create); }
    float widthOfMaster() { auto* st = stereoOfList (edit.getMasterPluginList(), edit.getMasterVolumePlugin().get(), false); return st ? st->width.load() : 1.f; }
    void setWidthMaster (float w) { if (auto* st = stereoOfList (edit.getMasterPluginList(), edit.getMasterVolumePlugin().get(), std::abs (w - 1.f) > .001f)) st->setWidth (w); }
    //---- enlace de canales: mover un fader o pan mueve también a los enlazados (la misma diferencia)
    static juce::String linkOf (te::AudioTrack& t) { return t.state.getProperty ("injLink", "").toString(); }
    juce::Array<te::AudioTrack*> linkedWith (te::AudioTrack& t) { juce::Array<te::AudioTrack*> a; a.add (&t); const auto id = linkOf (t); if (id.isNotEmpty()) for (auto* o : te::getAudioTracks (edit)) if (o != &t && linkOf (*o) == id) a.add (o); return a; }
    void linkChannels (te::AudioTrack& a, te::AudioTrack& b)
    {
        auto* um = &edit.getUndoManager(); auto id = linkOf (a).isNotEmpty() ? linkOf (a) : (linkOf (b).isNotEmpty() ? linkOf (b) : juce::Uuid().toString());
        a.state.setProperty ("injLink", id, um); b.state.setProperty ("injLink", id, um);
    }
    void unlinkChannel (te::AudioTrack& t)
    {
        auto* um = &edit.getUndoManager(); const auto id = linkOf (t); t.state.removeProperty ("injLink", um); auto rest = id.isNotEmpty() ? linkedWith (t) : juce::Array<te::AudioTrack*>();
        juce::Array<te::AudioTrack*> others; for (auto* o : te::getAudioTracks (edit)) if (o != &t && linkOf (*o) == id && id.isNotEmpty()) others.add (o);
        if (others.size() == 1) others[0]->state.removeProperty ("injLink", um);   // un enlace de uno solo no sirve
    }
    void setLinkedVolume (te::AudioTrack& t, float db)
    {
        auto* vp = t.getVolumePlugin(); const float old = vp->getVolumeDb(); vp->setVolumeDb (db);
        for (auto* o : linkedWith (t)) if (o != &t) { auto* ov = o->getVolumePlugin(); ov->setVolumeDb ((old > -90.f && db > -90.f) ? juce::jlimit (-100.f, 6.f, ov->getVolumeDb() + (db - old)) : db); }
    }
    void setLinkedPan (te::AudioTrack& t, float pan)
    {
        auto* vp = t.getVolumePlugin(); const float old = vp->getPan(); vp->setPan (pan);
        for (auto* o : linkedWith (t)) if (o != &t) { auto* ov = o->getVolumePlugin(); ov->setPan (juce::jlimit (-1.f, 1.f, ov->getPan() + (pan - old))); }
    }
    //---- orden de los canales (mover / intercambiar)
    void moveChannelTo (int from, int to)
    {
        auto tl = te::getAudioTracks (edit); if (from < 0 || to < 0 || from >= tl.size() || to >= tl.size() || from == to) return;
        te::Track::Ptr tk (tl[from]); edit.getUndoManager().beginNewTransaction(); edit.moveTrack (tk, te::TrackInsertPoint (*tl[to], to < from));
        selected = to; masterSel = false; showTrack (to); if (onTracksChanged) onTracksChanged(); repaint();
    }
    void setEqMid (te::AudioTrack& t, float hz, float db) { if (auto* e = eqOf (&t, true)) { e->setMidFreq1 (hz); e->setMidGain1 (db); } }   // para pruebas y demos
    void eqDemo (te::AudioTrack& t) { if (auto* e = eqOf (&t, true)) { e->setLowGain (-6.f); e->setLowFreq (70.f); e->setMidGain2 (-4.f); e->setMidFreq2 (3500.f); e->setHighGain (5.f); e->setHighFreq (10000.f); } repaint(); }
    void setMasterEqMid (float hz, float db) { if (auto* e = eqOfList (edit.getMasterPluginList(), edit.getMasterVolumePlugin().get(), true)) { e->setMidFreq1 (hz); e->setMidGain1 (db); } }
    float eqMidGainOf (te::AudioTrack& t) { auto* e = eqOf (&t, false); return e ? e->midGain1->getCurrentValue() : -99.f; }
    void clearChain (te::AudioTrack& t) { juce::Array<te::Plugin*> old; for (auto* p : t.pluginList.getPlugins()) if (isChainPlugin (p)) old.add (p); for (auto* p : old) p->deleteFromParent(); }
    void selectMaster (bool b) { masterSel = b; repaint(); }
    void clearMasterEq() { if (auto* e = eqOfList (edit.getMasterPluginList(), nullptr, false)) e->deleteFromParent(); }
    void snapshotSidechain (te::AudioTrack& tr, const juce::File& out) { SidechainPop p (edit, tr, {}); p.setBounds (0, 0, 330, 168); auto img = p.createComponentSnapshot (p.getLocalBounds(), true, 2.f); juce::PNGImageFormat f; out.deleteFile(); if (auto os = out.createOutputStream()) f.writeImageToStream (img, *os); }
    float widthOfTrack (te::AudioTrack& t) { auto* st = stereoOf (t, false); return st ? st->width.load() : 1.f; }
    void setWidthOfTrack (te::AudioTrack& t, float w) { if (auto* st = stereoOf (t, std::abs (w - 1.f) > .001f)) st->setWidth (w); }
    void beginGesture (te::AutomatableParameter* p) { endGesture(); gestureParam = p; if (p) p->parameterChangeGestureBegin(); }
    void endGesture() { if (gestureParam) { gestureParam->parameterChangeGestureEnd(); gestureParam = nullptr; } }
    static bool hasAuto (te::AudioTrack& t) { auto* vp = t.getVolumePlugin(); return vp && ((vp->volParam && vp->volParam->getCurve().getNumPoints() > 0) || (vp->panParam && vp->panParam->getCurve().getNumPoints() > 0)); }
    te::AuxSendPlugin* sendFromSelected (int di) { if (masterSel) return nullptr; auto* src = trackAt (selected); auto* dst = trackAt (di); if (! src || ! dst || src == dst) return nullptr; return sendTo (*src, *dst); }
    bool sendKnobShown (int di) { return sendFromSelected (di) != nullptr; }
    void sendDrag (const juce::MouseEvent& e) { if (auto* sp = sendFromSelected (active.idx + first)) { sp->setGainDb (juce::jlimit (-60.f, 6.f, sp->getGainDb() + (lastY - e.position.y) * 0.4f)); lastY = e.position.y; repaint(); } }
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
    //---- ruteo estilo FL: salida de un canal hacia otro (bus) y envíos con nivel propio
    static bool isBus (te::AudioTrack& t) { return (bool) t.state.getProperty ("injBus", false); }
    static te::AudioTrack* outputOf (te::AudioTrack& t) { return t.getOutput().getDestinationTrack(); }
    // false si el destino es la propia pista, si ya desemboca en ella (lazo por salida) o si ya le manda un envío (lazo por envío)
    bool canRoute (te::AudioTrack& src, te::AudioTrack& dest)
    {
        if (&src == &dest || dest.getOutput().feedsInto (&src)) return false;
        return sendTo (dest, src) == nullptr;
    }
    bool setOutput (te::AudioTrack& t, te::AudioTrack* dest)
    {
        if (dest != nullptr && ! canRoute (t, *dest)) return false;
        t.getOutput().setOutputToTrack (dest);
        if (dest == nullptr) t.getOutput().setOutputToDefaultDevice (false);
        if (dest != nullptr) dest->state.setProperty ("injBus", true, &edit.getUndoManager());
        return true;
    }
    te::AudioTrack* newBus (const juce::String& name)
    {
        auto* b = trackutil::addTrack (edit); if (b == nullptr) return nullptr;
        b->setName (name); b->state.setProperty ("injBus", true, &edit.getUndoManager()); return b;
    }
    // el retorno de `dest` (si no tiene, se crea con un número libre)
    te::AuxReturnPlugin* returnFor (te::AudioTrack& dest, bool create)
    {
        for (auto* p : dest.pluginList.getPlugins()) if (auto* rp = dynamic_cast<te::AuxReturnPlugin*> (p)) return rp;
        if (! create) return nullptr;
        int mx = 1; for (auto* tr : te::getAudioTracks (edit)) for (auto* p : tr->pluginList.getPlugins()) { if (auto* a = dynamic_cast<te::AuxReturnPlugin*> (p)) mx = std::max (mx, (int) a->busNumber.get()); if (auto* b = dynamic_cast<te::AuxSendPlugin*> (p)) mx = std::max (mx, (int) b->busNumber.get()); }
        auto np = edit.getPluginCache().createNewPlugin (te::AuxReturnPlugin::xmlTypeName, {}); auto* rp = dynamic_cast<te::AuxReturnPlugin*> (np.get()); if (rp == nullptr) return nullptr;
        rp->busNumber = mx + 1; dest.pluginList.insertPlugin (np, 0, nullptr); return rp;
    }
    te::AuxSendPlugin* sendTo (te::AudioTrack& src, te::AudioTrack& dest)
    {
        auto* rp = returnFor (dest, false); if (rp == nullptr) return nullptr; return sendOf (src, rp->busNumber.get());
    }
    bool canSend (te::AudioTrack& src, te::AudioTrack& dest) { return &src != &dest && ! dest.getOutput().feedsInto (&src) && sendTo (dest, src) == nullptr; }
    void toggleSendTo (te::AudioTrack& src, te::AudioTrack& dest)
    {
        if (auto* sp = sendTo (src, dest)) { const int bus = sp->busNumber.get(); sp->deleteFromParent(); bool used = false; for (auto* tr : te::getAudioTracks (edit)) if (sendOf (*tr, bus)) used = true; if (! used) if (auto* rp = returnFor (dest, false)) rp->deleteFromParent(); return; }
        if (! canSend (src, dest)) return;
        auto* rp = returnFor (dest, true); if (rp == nullptr) return;
        auto np = edit.getPluginCache().createNewPlugin (te::AuxSendPlugin::xmlTypeName, {}); auto* sp = dynamic_cast<te::AuxSendPlugin*> (np.get()); if (sp == nullptr) return;
        sp->busNumber = rp->busNumber.get(); src.pluginList.insertPlugin (np, -1, nullptr); sp->setGainDb (-6.f); setSendPre (src, *sp, false);
    }
    // antes o después del fader: el envío se coloca antes del volumen de la pista (pre) o al final (post)
    void setSendPre (te::AudioTrack& t, te::AuxSendPlugin& sp, bool pre)
    {
        sp.state.setProperty ("injPre", pre, nullptr); te::Plugin::Ptr keep (&sp); auto* vp = t.getVolumePlugin();
        const int vi = vp ? t.pluginList.indexOf (vp) : -1;
        sp.removeFromParent (); t.pluginList.insertPlugin (keep, pre && vi >= 0 ? vi : -1, nullptr);
    }
private:
    te::AudioTrack* trackAt (int i) { auto t = te::getAudioTracks (edit); return i >= 0 && i < t.size() ? t[i] : nullptr; }
    static juce::Array<te::Plugin*> fxOfList (te::PluginList& pl) { juce::Array<te::Plugin*> a; for (auto* p : pl.getPlugins()) if (dynamic_cast<te::ExternalPlugin*> (p) || dynamic_cast<injfx::InjFx*> (p)) a.add (p); return a; }
    juce::Array<te::Plugin*> fxOf (te::AudioTrack* t) { return t ? fxOfList (t->pluginList) : juce::Array<te::Plugin*>(); }
    juce::Array<te::Plugin*> currentFx() { return masterSel ? fxOfList (edit.getMasterPluginList()) : fxOf (trackAt (selected)); }
    //---- mezcla seca/húmeda de un plugin (como la perilla Mix de cada ranura de FL)
    static te::ExternalPlugin* asExt (te::Plugin* p) { auto* e = dynamic_cast<te::ExternalPlugin*> (p); return e && ! e->isSynth() ? e : nullptr; }
    static float mixOf (te::Plugin* p) { auto* e = asExt (p); return e ? juce::jlimit (0.f, 1.f, e->wetGain->getCurrentValue()) : 1.f; }
    static void setMixOf (te::Plugin* p, float m) { if (auto* e = asExt (p)) { m = juce::jlimit (0.f, 1.f, m); e->wetGain->setParameter (m, juce::sendNotification); e->dryGain->setParameter (1.f - m, juce::sendNotification); } }
    static void drawMiniKnob (juce::Graphics& g, juce::Rectangle<float> r, float v01, juce::Colour col)
    {   // perilla pequeña: arco de valor, disco plano y aguja (v01: 0..1)
        const auto c = r.getCentre(); const float R = r.getWidth() * .5f, pi = juce::MathConstants<float>::pi, a0 = -pi * .75f, a1 = a0 + pi * 1.5f * juce::jlimit (0.f, 1.f, v01);
        juce::Path tr; tr.addCentredArc (c.x, c.y, R, R, 0.f, a0, pi * .75f, true); g.setColour (theme::seam); g.strokePath (tr, juce::PathStrokeType (3.f));
        if (v01 > .01f) { juce::Path va; va.addCentredArc (c.x, c.y, R, R, 0.f, a0, a1, true); g.setColour (col); g.strokePath (va, juce::PathStrokeType (2.f)); }
        const auto in = r.reduced (4.f); g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3d3f47), c.x, in.getY(), juce::Colour (0xff2a2c32), c.x, in.getBottom(), false)); g.fillEllipse (in);
        g.setColour (juce::Colour (0xfff0f1f4)); g.drawLine (c.x + std::sin (a1) * R * .2f, c.y - std::cos (a1) * R * .2f, c.x + std::sin (a1) * R * .66f, c.y - std::cos (a1) * R * .66f, 1.5f);
    }
    juce::String chanName (te::AudioTrack& t) { auto i = te::getAudioTracks (edit).indexOf (&t); return t.getName().isNotEmpty() && t.getName() != "Track" ? t.getName() : "Canal " + juce::String (i + 1); }
public:
    //---- cadenas de canal: guardar y cargar ecualizador, estéreo y plugins de un canal
    static juce::File chainDir() { auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("INJUSTICE DAW").getChildFile ("Cadenas"); d.createDirectory(); return d; }
    static bool isChainPlugin (te::Plugin* p) { return dynamic_cast<te::ExternalPlugin*> (p) || dynamic_cast<te::EqualiserPlugin*> (p) || dynamic_cast<InjStereo*> (p) || dynamic_cast<injfx::InjFx*> (p); }
    bool saveChain (te::AudioTrack& t, const juce::String& name, const juce::File& dir = chainDir())
    {
        juce::XmlElement root ("INJCHAIN"); int n = 0;
        for (auto* p : t.pluginList.getPlugins()) if (isChainPlugin (p)) { if (auto xml = p->state.createXml()) { root.addChildElement (xml.release()); ++n; } }
        if (n == 0 || name.trim().isEmpty()) return false;
        return root.writeTo (dir.getChildFile (juce::File::createLegalFileName (name.trim()) + ".injchain"));
    }
    bool loadChain (te::AudioTrack& t, const juce::File& f)
    {
        auto xml = juce::parseXML (f); if (xml == nullptr || xml->getTagName() != "INJCHAIN") return false;
        edit.getUndoManager().beginNewTransaction();
        juce::Array<te::Plugin*> old; for (auto* p : t.pluginList.getPlugins()) if (isChainPlugin (p)) old.add (p);
        for (auto* p : old) p->deleteFromParent();
        for (auto* c : xml->getChildIterator())
        {
            auto vt = juce::ValueTree::fromXml (*c); vt.removeProperty ("id", nullptr); auto np = edit.getPluginCache().createNewPlugin (vt); if (np == nullptr) continue;
            auto* vp = t.getVolumePlugin(); const int vi = vp ? t.pluginList.indexOf (vp) : -1; t.pluginList.insertPlugin (np, vi, nullptr);
        }
        return true;
    }
    juce::Array<juce::File> chainFiles() { return chainDir().findChildFiles (juce::File::findFiles, false, "*.injchain"); }
    void askChainName (te::AudioTrack::Ptr t)
    {
        auto* aw = new juce::AlertWindow (juce::String::fromUTF8 ("Guardar cadena"), juce::String::fromUTF8 ("Nombre de la cadena"), juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("n", chanName (*t)); aw->addButton ("Guardar", 1, juce::KeyPress (juce::KeyPress::returnKey)); aw->addButton ("Cancelar", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, t] (int r) { if (r == 1) { if (saveChain (*t, aw->getTextEditorContents ("n")) && onMessage) onMessage (juce::String::fromUTF8 ("Cadena guardada")); else if (onMessage) onMessage (juce::String::fromUTF8 ("Este canal no tiene efectos ni ecualizador para guardar")); } delete aw; }), false);
    }
private:
    //---- colores e iconos de canal
    static const char* colourName (int k) { static const char* n[] { "Azul", "Turquesa", "Amarillo", "Naranja", "Rosa", "Verde", "Lila", "Rojo" }; return n[juce::jlimit (0, 7, k)]; }
    static const char* iconName (int k) { static const char* n[] { "Ninguno", "Bombo", "Caja", "Hats", "Bajo", "Sinte", "Voz", "Guitarra", "FX" }; return n[juce::jlimit (0, 8, k)]; }
    static void drawChannelIcon (juce::Graphics& g, juce::Rectangle<float> r, int k, juce::Colour col)
    {
        if (k <= 0) return; g.setColour (col); const auto c = r.getCentre(); const float w = r.getWidth();
        switch (k)
        {
            case 1: g.drawEllipse (r.reduced (1.5f), 1.6f); g.fillEllipse (r.reduced (w * .32f)); break;                                                     // bombo
            case 2: g.drawEllipse (r.withHeight (w * .5f).translated (0, w * .12f), 1.5f); g.drawLine (r.getX() + 1, r.getY() + w * .38f, r.getX() + 1, r.getY() + w * .8f, 1.5f); g.drawLine (r.getRight() - 1, r.getY() + w * .38f, r.getRight() - 1, r.getY() + w * .8f, 1.5f); g.drawEllipse (r.withTop (r.getY() + w * .6f).withHeight (w * .4f), 1.5f); break;   // caja
            case 3: g.fillEllipse (r.getX(), c.y - 2.5f, w, 4.f); g.fillEllipse (r.getX() + 1, c.y + 2.f, w - 2, 3.f); break;                                // hats
            case 4: { juce::Path p; p.startNewSubPath (r.getX(), c.y); for (int i = 1; i <= 8; ++i) p.lineTo (r.getX() + w * i / 8.f, c.y + std::sin (i * 1.57f) * w * .32f); g.strokePath (p, juce::PathStrokeType (1.8f)); break; }   // bajo (onda)
            case 5: g.drawRect (r.reduced (.5f, 2.5f), 1.4f); for (int i = 1; i < 4; ++i) g.drawLine (r.getX() + w * i / 4.f, r.getY() + 2.5f, r.getX() + w * i / 4.f, r.getBottom() - 2.5f, 1.2f); break;   // sinte
            case 6: g.fillRoundedRectangle (c.x - w * .16f, r.getY() + 1, w * .32f, w * .5f, w * .16f); g.drawLine (c.x, r.getY() + w * .6f, c.x, r.getBottom() - 1, 1.5f); g.drawLine (c.x - w * .22f, r.getBottom() - 1, c.x + w * .22f, r.getBottom() - 1, 1.5f); break;   // voz
            case 7: { juce::Path p; p.addEllipse (r.getX() + 1, c.y, w * .6f, w * .5f); g.strokePath (p, juce::PathStrokeType (1.5f)); g.drawLine (c.x + w * .1f, c.y + 2, r.getRight() - 1, r.getY() + 1, 1.6f); break; }   // guitarra
            default: { juce::Path p; p.addStar (c, 5, w * .22f, w * .5f, 0.f); g.fillPath (p); break; }                                                       // fx
        }
    }
    //---- pantalla de sidechain: de quién se alimenta el canal, fuerza y recuperación
    struct SidechainPop : public juce::Component
    {
        SidechainPop (te::Edit& e, te::AudioTrack& tr, std::function<void()> changed) : edit (e), t (&tr), onChange (std::move (changed))
        {
            setSize (330, 168); auto all = te::getAudioTracks (edit);
            title.setText (juce::String::fromUTF8 ("Sidechain: ") + (tr.getName() == "Track" ? juce::String::fromUTF8 ("este canal") : tr.getName()) + juce::String::fromUTF8 (" baja cuando suena…"), juce::dontSendNotification); title.setFont (juce::FontOptions (13.f, juce::Font::bold)); addAndMakeVisible (title);
            src.addItem ("(sin sidechain)", 1); for (int i = 0; i < all.size(); ++i) if (all[i] != &tr) src.addItem (juce::String (i + 1) + "  " + (all[i]->getName() == "Track" ? juce::String::fromUTF8 ("Canal ") + juce::String (i + 1) : all[i]->getName()), 2 + i);
            auto* cur = trackutil::sidechainSource (edit, tr); src.setSelectedId (cur ? 2 + all.indexOf (cur) : 1, juce::dontSendNotification); addAndMakeVisible (src);
            for (auto* sl : { &amt, &rel }) { sl->setSliderStyle (juce::Slider::LinearHorizontal); sl->setTextBoxStyle (juce::Slider::TextBoxRight, false, 62, 22); addAndMakeVisible (sl); }
            amt.setRange (0.0, 100.0, 1.0); amt.setTextValueSuffix (" %"); amt.setValue (trackutil::sidechainPlugin (tr) ? trackutil::sidechainAmount (tr) * 100.0 : 70.0, juce::dontSendNotification);
            rel.setRange (10.0, 300.0, 1.0); rel.setTextValueSuffix (" ms"); rel.setValue (trackutil::sidechainPlugin (tr) ? trackutil::sidechainRelease (tr) : 120.0, juce::dontSendNotification);
            la.setText ("Fuerza", juce::dontSendNotification); lr.setText (juce::String::fromUTF8 ("Recuperación"), juce::dontSendNotification); addAndMakeVisible (la); addAndMakeVisible (lr);
            src.onChange = [this] { apply(); }; amt.onValueChange = [this] { apply(); }; rel.onValueChange = [this] { apply(); }; refreshEnabled();
        }
        void resized() override { auto r = getLocalBounds().reduced (12); title.setBounds (r.removeFromTop (24)); r.removeFromTop (6); src.setBounds (r.removeFromTop (28)); r.removeFromTop (12);
            auto a = r.removeFromTop (26); la.setBounds (a.removeFromLeft (96)); amt.setBounds (a); r.removeFromTop (8); auto b = r.removeFromTop (26); lr.setBounds (b.removeFromLeft (96)); rel.setBounds (b); }
        void refreshEnabled() { const bool on = src.getSelectedId() > 1; amt.setEnabled (on); rel.setEnabled (on); }
        void apply()
        {
            auto all = te::getAudioTracks (edit); const int id = src.getSelectedId(); edit.getUndoManager().beginNewTransaction();
            if (id <= 1) trackutil::removeSidechain (*t); else if (id - 2 < all.size()) trackutil::setSidechain (edit, *t, all[id - 2], amt.getValue() / 100.0, rel.getValue());
            refreshEnabled(); if (onChange) onChange();
        }
        te::Edit& edit; te::AudioTrack::Ptr t; std::function<void()> onChange; juce::Label title, la, lr; juce::ComboBox src; juce::Slider amt, rel;
    };
    void sidechainPopup (int ti, juce::Rectangle<float> s)
    {
        auto* tk = trackAt (ti); if (! tk) return;
        juce::CallOutBox::launchAsynchronously (std::make_unique<SidechainPop> (edit, *tk, [this] { repaint(); }), localAreaToGlobal (toLocal (scOf (s))).toNearestInt(), nullptr);
    }
    void channelMenu (int ti, juce::Rectangle<float> s)
    {
        auto* tk = trackAt (ti); if (! tk) return; te::AudioTrack::Ptr t (tk); juce::PopupMenu m; auto all = te::getAudioTracks (edit);
        m.addItem (1, juce::String::fromUTF8 ("Renombrar…")); m.addItem (2, t->isMuted (false) ? "Quitar silencio" : "Silenciar"); m.addItem (3, t->isSolo (false) ? "Quitar solo" : "Solo");
        m.addSeparator(); m.addItem (4, juce::String::fromUTF8 ("Agregar efecto…")); m.addItem (9, "Mono", true, widthOfTrack (*t) < .02f);
        m.addSeparator();
        {   // Salida: Master u otro canal (bus). Los destinos que crearían un lazo salen deshabilitados.
            juce::PopupMenu om; auto* cur = outputOf (*t); om.addItem (100, "Master", true, cur == nullptr);
            for (int i = 0; i < all.size(); ++i) if (all[i] != t.get()) om.addItem (200 + i, juce::String (i + 1) + "  " + chanName (*all[i]) + (isBus (*all[i]) ? "  (bus)" : ""), canRoute (*t, *all[i]), cur == all[i]);
            m.addSubMenu (cur ? juce::String::fromUTF8 ("Salida: ") + chanName (*cur) : juce::String::fromUTF8 ("Salida: Master"), om);
        }
        m.addItem (6, juce::String::fromUTF8 ("Nuevo bus con este canal"));
        if (trackutil::groupOf (*t).isNotEmpty()) m.addItem (7, juce::String::fromUTF8 ("Nuevo bus con todo su grupo"));
        m.addItem (10, juce::String::fromUTF8 ("Mover a la izquierda"), ti > 0); m.addItem (11, juce::String::fromUTF8 ("Mover a la derecha"), ti < all.size() - 1);
        {   // enlazar: los canales enlazados mueven volumen, pan y estéreo juntos
            juce::PopupMenu lm; for (int i = 0; i < all.size(); ++i) if (all[i] != t.get()) lm.addItem (1200 + i, juce::String (i + 1) + "  " + chanName (*all[i]), true, linkOf (*t).isNotEmpty() && linkOf (*all[i]) == linkOf (*t));
            if (linkOf (*t).isNotEmpty()) { lm.addSeparator(); lm.addItem (1190, "Quitar enlace"); }
            m.addSubMenu (juce::String::fromUTF8 ("Enlazar volumen y pan con…"), lm);
        }
        m.addItem (12, juce::String::fromUTF8 ("Sidechain…"));
        {   // aspecto: color e icono
            juce::PopupMenu cm, im; for (int k = 0; k < 8; ++k) cm.addItem (1600 + k, juce::String::fromUTF8 (colourName (k)), true, (int) t->state.getProperty ("injColour", -1) == k);
            for (int k = 0; k <= 8; ++k) im.addItem (1700 + k, juce::String::fromUTF8 (iconName (k)), true, (int) t->state.getProperty ("injIcon", 0) == k);
            m.addSubMenu ("Color", cm); m.addSubMenu ("Icono", im);
        }
        {   // cadenas guardadas
            juce::PopupMenu cp; cp.addItem (1500, juce::String::fromUTF8 ("Guardar esta cadena…")); auto files = chainFiles(); if (! files.isEmpty()) cp.addSeparator();
            for (int i = 0; i < files.size() && i < 90; ++i) cp.addItem (1510 + i, files[i].getFileNameWithoutExtension());
            m.addSubMenu ("Cadena (ecualizador y efectos)", cp);
        }
        {   // entrada de audio para grabar
            juce::PopupMenu ip; const auto cur = t->state.getProperty ("injInput", "").toString(); ip.addItem (1800, juce::String::fromUTF8 ("Automática"), true, cur.isEmpty()); int k = 0;
            for (auto* inst : edit.getAllInputDevices()) if (inst->getInputDevice().getDeviceType() == te::InputDevice::waveDevice) { ip.addItem (1801 + k, inst->getInputDevice().getName(), true, cur == inst->getInputDevice().getName()); ++k; }
            m.addSubMenu (juce::String::fromUTF8 ("Entrada de audio"), ip);
        }
        if (isBus (*t)) m.addItem (8, juce::String::fromUTF8 ("Esta pista deja de ser bus"));
        {   // Envíos: a cualquier otro canal, con nivel propio por envío
            juce::PopupMenu sm, lv;
            for (int i = 0; i < all.size(); ++i) if (all[i] != t.get()) sm.addItem (400 + i, juce::String (i + 1) + "  " + chanName (*all[i]), canSend (*t, *all[i]) || sendTo (*t, *all[i]) != nullptr, sendTo (*t, *all[i]) != nullptr);
            bool any = false;
            for (int i = 0; i < all.size(); ++i) if (auto* sp = sendTo (*t, *all[i])) { any = true; juce::PopupMenu one; static const float dbs[] { -24.f, -12.f, -6.f, 0.f };
                for (int k = 0; k < 4; ++k) one.addItem (600 + i * 8 + k, juce::String (dbs[k], 0) + " dB", true, std::abs (sp->getGainDb() - dbs[k]) < .5f);
                one.addItem (600 + i * 8 + 4, juce::String::fromUTF8 ("Antes del fader"), true, sp->state.getProperty ("injPre", false));
                lv.addSubMenu (juce::String::fromUTF8 ("Nivel → ") + chanName (*all[i]), one); }
            if (any) { sm.addSeparator(); sm.addSubMenu (juce::String::fromUTF8 ("Nivel de cada envío"), lv); }
            m.addSubMenu (juce::String::fromUTF8 ("Envíos"), sm);
        }
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (toLocal (chevOf (s)))), [this, t, ti] (int r)
        {
            auto all2 = te::getAudioTracks (edit); edit.getUndoManager().beginNewTransaction();
            if (r == 1) rename (ti); else if (r == 2) t->setMute (! t->isMuted (false)); else if (r == 3) t->setSolo (! t->isSolo (false)); else if (r == 4 && onAddEffect) onAddEffect (ti);
            else if (r == 9) setWidthOfTrack (*t, widthOfTrack (*t) < .02f ? 1.f : 0.f);
            else if (r == 100) setOutput (*t, nullptr);
            else if (r >= 200 && r < 400 && r - 200 < all2.size()) setOutput (*t, all2[r - 200]);
            else if (r == 6) { if (auto* b = newBus (juce::String::fromUTF8 ("Bus ") + chanName (*t))) setOutput (*t, b); if (onTracksChanged) onTracksChanged(); }
            else if (r == 7) { auto* b = newBus (trackutil::groupName (*t)); if (b) for (auto* mem : trackutil::groupMembers (edit, trackutil::groupOf (*t))) setOutput (*mem, b); if (onTracksChanged) onTracksChanged(); }
            else if (r == 10) moveChannelTo (ti, ti - 1);
            else if (r == 11) moveChannelTo (ti, ti + 1);
            else if (r == 12) sidechainPopup (ti, stripR (juce::jlimit (0, nv() - 1, ti - first)));
            else if (r == 1190) unlinkChannel (*t);
            else if (r >= 1200 && r < 1400 && r - 1200 < all2.size()) linkChannels (*t, *all2[r - 1200]);
            else if (r >= 1600 && r < 1608) trackutil::setColour (edit, t.get(), r - 1600);
            else if (r >= 1700 && r <= 1708) t->state.setProperty ("injIcon", r - 1700, &edit.getUndoManager());
            else if (r == 1500) askChainName (t);
            else if (r >= 1510 && r < 1600) { auto files = chainFiles(); if (r - 1510 < files.size() && loadChain (*t, files[r - 1510]) && onMessage) onMessage (juce::String::fromUTF8 ("Cadena cargada: ") + files[r - 1510].getFileNameWithoutExtension()); }
            else if (r >= 1800 && r < 1900)
            {
                juce::StringArray names; for (auto* inst : edit.getAllInputDevices()) if (inst->getInputDevice().getDeviceType() == te::InputDevice::waveDevice) names.add (inst->getInputDevice().getName());
                const bool wasArmed = trackutil::isArmed (*t); if (wasArmed) trackutil::armTrack (edit, *t, false);
                if (r == 1800) t->state.removeProperty ("injInput", nullptr); else if (r - 1801 < names.size()) t->state.setProperty ("injInput", names[r - 1801], nullptr);
                if (wasArmed) trackutil::armTrack (edit, *t, true);
            }
            else if (r == 8) t->state.removeProperty ("injBus", &edit.getUndoManager());
            else if (r >= 400 && r < 600 && r - 400 < all2.size()) toggleSendTo (*t, *all2[r - 400]);
            else if (r >= 600 && r < 1000) { const int i = (r - 600) / 8, k = (r - 600) % 8; if (i < all2.size()) if (auto* sp = sendTo (*t, *all2[i])) { static const float dbs[] { -24.f, -12.f, -6.f, 0.f }; if (k < 4) sp->setGainDb (dbs[k]); else setSendPre (*t, *sp, ! (bool) sp->state.getProperty ("injPre", false)); } }
            if (onTracksChanged) onTracksChanged();
            repaint();
        });
    }
    // efecto integrado nuevo en la cadena del canal (antes del volumen), con su ventana abierta
public:
    te::Plugin* addBuiltinFx (const char* type)
    {
        auto* t = masterSel ? nullptr : trackAt (selected); if (! masterSel && t == nullptr) return nullptr;
        auto& pl = masterSel ? edit.getMasterPluginList() : t->pluginList; auto* vol = masterSel ? edit.getMasterVolumePlugin().get() : t->getVolumePlugin();
        edit.getUndoManager().beginNewTransaction();
        auto np = edit.getPluginCache().createNewPlugin (type, {}); if (np == nullptr) return nullptr;
        pl.insertPlugin (np, vol ? pl.indexOf (vol) : -1, nullptr); np->setEnabled (true); repaint(); return np.get();
    }
private:
    void addEffectMenu (juce::Rectangle<float> row)
    {
        juce::PopupMenu m; const auto& cat = injfx::catalog(); const char* fam = nullptr; juce::PopupMenu sub; int k = 1;
        for (auto& e : cat) { if (fam == nullptr || std::strcmp (fam, e.family) != 0) { if (fam != nullptr) m.addSubMenu (juce::String::fromUTF8 (fam), sub); sub = juce::PopupMenu(); fam = e.family; } sub.addItem (k++, juce::String::fromUTF8 (e.label)); }
        if (fam != nullptr) m.addSubMenu (juce::String::fromUTF8 (fam), sub); m.addSeparator(); m.addItem (100, "Plugins VST3...");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (toLocal (row))), [this] (int r)
        {
            if (r == 100) { if (onAddEffect) onAddEffect (masterSel ? -2 : selected); return; }
            if (r >= 1 && r <= (int) injfx::catalog().size()) if (auto* np = addBuiltinFx (injfx::catalog()[(size_t) r - 1].type)) if (onOpenPlugin) onOpenPlugin (np);
        });
    }
    void fxClick (juce::Point<float> p)
    {
        if (! masterSel && trackAt (selected) == nullptr) return; auto fx = currentFx();
        for (int n = 0; n < 10; ++n)
        {
            auto row = fxRow (n); if (! row.contains (p)) continue;
            if (n < fx.size())
            {
                if (asExt (fx[n]) && mixKnobOf (row).expanded (3).contains (p)) { active = { Hit::FxMix, n }; mixPlugin = fx[n]; lastY = p.y; }
                else if (juce::Rectangle<float> (row.getX(), row.getY(), 40, row.getHeight()).contains (p)) { fx[n]->setEnabled (! fx[n]->isEnabled()); repaint(); }
                else if (juce::Rectangle<float> (row.getRight() - 36, row.getY(), 36, row.getHeight()).contains (p))
                {
                    te::Plugin::Ptr pl (fx[n]); juce::PopupMenu m; m.addItem (1, "Abrir"); m.addItem (2, "Quitar efecto");
                    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (toLocal (row))), [this, pl] (int r) { if (r == 1 && onOpenPlugin) onOpenPlugin (pl.get()); else if (r == 2) { pl->deleteFromParent(); repaint(); } });
                }
                else if (onOpenPlugin) onOpenPlugin (fx[n]);
            }
            else addEffectMenu (row);
            return;
        }
    }
    void stripMenu (int idx)
    {
        juce::PopupMenu m; m.addItem (1, juce::String::fromUTF8 ("Reiniciar volumen y pan de este canal")); m.addItem (2, juce::String::fromUTF8 ("Reiniciar todos los canales")); m.addItem (3, juce::String::fromUTF8 ("Quitar todos los solos y silencios"));
        m.showMenuAsync (juce::PopupMenu::Options(), [this, idx] (int r)
        {
            if (r == 0) return; edit.getUndoManager().beginNewTransaction(); auto tracks = te::getAudioTracks (edit);
            if (r == 1 && idx < tracks.size()) { tracks[idx]->getVolumePlugin()->setVolumeDb (0.f); tracks[idx]->getVolumePlugin()->setPan (0.f); }
            else if (r == 2) for (auto* t : tracks) { t->getVolumePlugin()->setVolumeDb (0.f); t->getVolumePlugin()->setPan (0.f); }
            else if (r == 3) for (auto* t : tracks) { t->setSolo (false); t->setMute (false); }
            repaint();
        });
    }
    void rename (int i) { trackutil::renameDialog (edit, trackAt (i), [this] { if (onTracksChanged) onTracksChanged(); repaint(); }); }
    void widthDrag (const juce::MouseEvent& e, int ti, bool master)
    {
        auto* t = master ? nullptr : trackAt (ti); if (! master && ! t) return; auto wr = widthOf (master ? masterR() : stripR (ti - first)); float w = juce::jlimit (0.f, 2.f, (e.position.x - wr.getX()) / wr.getWidth() * 2.f);
        if (std::abs (w - 1.f) < .04f) w = 1.f; else if (w < .04f) w = 0.f;   // imanes en 100 % y mono
        if (master) setWidthMaster (w); else for (auto* o : linkedWith (*t)) setWidthOfTrack (*o, w);
        repaint();
    }
    void dragFader (const juce::MouseEvent& e, bool master)
    {
        auto a = master ? masterFaderArea() : faderArea (active.idx);
        const float pos = juce::jlimit (0.f, 1.f, 1.f - (e.position.y - a.getY() - kCapH * .5f) / (a.getHeight() - kCapH));   // el centro de la tapa recorre el slot
        const float db = pos <= 0.001f ? -100.f : posToDb (pos);
        if (master) { edit.getMasterVolumePlugin()->setVolumeDb (db); masterMuted = false; } else if (auto* t = trackAt (active.idx + first)) setLinkedVolume (*t, db);
        repaint();
    }
    void syncClients()
    {
        for (auto& c : clients) { if (c.meter) c.meter->measurer.removeClient (c.client); c.meter = nullptr; }
        auto tracks = te::getAudioTracks (edit);
        for (int i = 0; i < nv() && i + first < tracks.size(); ++i) if (auto* lm = tracks[i + first]->getLevelMeterPlugin()) { clients[(size_t) i].meter = lm; lm->measurer.addClient (clients[(size_t) i].client); }
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
    static void holdPeak (float& pk, int& hold, float level)
    {   // el pico se queda ~1 s y después cae despacio
        if (level >= pk) { pk = level; hold = 30; } else if (hold > 0) --hold; else pk = std::max (level, pk - 14.f / 30.f);
        if (pk < -90.f) pk = -100.f;
    }
    void timerCallback() override
    {
        if (! isVisible()) return;
        const float fall = 26.f / 30.f;
        bool changed = false;
        if (edit.getTransport().isPlaying()) for (auto* tr : te::getAudioTracks (edit)) if (hasAuto (*tr)) { changed = true; break; }   // los faders se mueven con la automatización
        for (auto& c : clients) { if (! c.meter) continue; const float ol = c.l, orr = c.r; c.l = std::max (c.client.getAndClearAudioLevel (0).dB, c.l - fall); c.r = std::max (c.client.getAndClearAudioLevel (1).dB, c.r - fall); c.l = juce::jmax (c.l, -100.f); c.r = juce::jmax (c.r, -100.f); const float opl = c.pl, opr = c.pr; holdPeak (c.pl, c.hl, c.l); holdPeak (c.pr, c.hr, c.r); if (c.l != ol || c.r != orr || c.pl != opl || c.pr != opr) changed = true; }
        const float ml0 = masterClient.l, mr0 = masterClient.r; pollMaster(); masterClient.l = juce::jmax (masterClient.l, -100.f); masterClient.r = juce::jmax (masterClient.r, -100.f); const float mpl = masterClient.pl, mpr = masterClient.pr; holdPeak (masterClient.pl, masterClient.hl, masterClient.l); holdPeak (masterClient.pr, masterClient.hr, masterClient.r); if (ml0 != masterClient.l || mr0 != masterClient.r || mpl != masterClient.pl || mpr != masterClient.pr) changed = true;
        for (int i = -1; i < nv(); ++i)   // brillo suave del fader y la perilla al pasar el ratón o arrastrar
        {
            auto sr = i < 0 ? masterR() : stripR (i); const int key = i < 0 ? -1 : i + first;
            const bool hot = faderOf (sr).expanded (22, 0).contains (hoverPos) || knobOf (sr).expanded (4).contains (hoverPos)
                             || (i >= 0 && (active.kind == Hit::Fader || active.kind == Hit::Pan) && active.idx == i) || (i < 0 && (active.kind == Hit::MasterFader || active.kind == Hit::MasterPan));
            float& gv = glow[key]; const float t = hot ? 1.f : 0.f, d = t - gv; if (std::abs (d) > .01f) { gv += d * .35f; changed = true; } else gv = t;
        }
        if (changed) repaint();   // sin sonido no hay nada que redibujar
    }

    //---- dibujo plano y pastel (sin brillos decorativos)
    void ledMeter (juce::Graphics& g, juce::Rectangle<float> r, float db, float pkDb = -100.f)
    {   // medidor de segmentos: verde, ámbar y rojo apagados cuando no suenan
        constexpr int nSeg = 32; const float h = (r.getHeight() - (nSeg - 1) * 1.f) / nSeg; const float pos = db <= -90 ? 0.f : dbToPos (db);
        for (int s = 0; s < nSeg; ++s)
        {
            const float y = r.getBottom() - (s + 1) * h - s * 1.f, segDb = posToDb ((float) s / nSeg); const bool lit = pos > (float) (s + 1) / nSeg - .5f / nSeg;
            juce::Colour on = segDb > -1.5f ? theme::record : (segDb > -9.f ? juce::Colour (0xffe9c545) : theme::ok);
            const bool isPk = pkDb > -60.f && std::abs (dbToPos (pkDb) * (float) nSeg - ((float) s + .5f)) < .5f && ! lit;
            g.setColour (lit ? on : (isPk ? on.withAlpha (.9f) : juce::Colour (0xff1e2025))); g.fillRect (r.getX(), y, r.getWidth(), h);
        }
    }
    // Tapa del fader: pieza plana grafito con línea central clara (la del canal elegido, naranja) y estrías de agarre
    void cap (juce::Graphics& g, float cx, float cy, bool sel = false)
    {
        juce::Rectangle<float> r (cx - 17, cy - 13.f, 34, 26.f);
        g.setColour (juce::Colours::black.withAlpha (.28f)); g.fillRoundedRectangle (r.translated (0, 1.5f), 3.f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff52555d), r.getX(), r.getY(), juce::Colour (0xff45484f), r.getX(), r.getBottom(), false)); g.fillRoundedRectangle (r, 3.f);
        g.setColour (juce::Colours::white.withAlpha (.08f)); g.fillRect (r.getX() + 2.f, r.getY() + .5f, r.getWidth() - 4.f, 1.f);
        g.setColour (theme::seam); g.drawRoundedRectangle (r.reduced (.5f), 3.f, 1.f);
        g.setColour (juce::Colours::black.withAlpha (.4f)); for (int k : { -1, 1 }) g.fillRect (r.getX() + 5.f, cy + k * 5.f - .5f, r.getWidth() - 10.f, 1.f);
        g.setColour (sel ? theme::accentHi : juce::Colour (0xfff2f3f6)); g.fillRect (r.getX() + 3.f, cy - 1.f, r.getWidth() - 6.f, 2.f);
    }
    void faderTrack (juce::Graphics& g, juce::Rectangle<float> fa, juce::Colour col = theme::accent, float pos = 0.f, bool lit = false)
    {
        auto lane = fa.withSizeKeepingCentre (20, fa.getHeight() - 6);
        theme::well (g, lane.expanded (1.f), false, 3.f);
        const float y0 = fa.getBottom() - kCapH * .5f, span = fa.getHeight() - kCapH; auto yOf = [&] (float db) { return y0 - span * dbToPos (db); };
        for (int db = -60; db <= 6; db += 3)   // marcas cada 3 dB (más largas cada 6)
        {
            const float y = yOf ((float) db); const bool b6 = db % 6 == 0; const float len = b6 ? 6.f : 3.f;
            g.setColour (juce::Colour (b6 ? 0xff555862 : 0xff3a3d45)); g.fillRect (lane.getX() + 1.f, y - .5f, len, 1.f); g.fillRect (lane.getRight() - 1.f - len, y - .5f, len, 1.f);
        }
        g.setFont (theme::fonts::digits (10.f));
        for (float db : { 6.f, 0.f, -6.f, -12.f, -18.f, -24.f, -30.f, -36.f, -42.f, -48.f, -54.f })   // cifras de dB junto al carril
        {
            if (compact && db != 0.f) continue; const float y = yOf (db); const bool z = db == 0.f;
            g.setColour (z ? theme::text : theme::textDim);
            if (! compact) g.drawText (db > 0 ? "+" + juce::String ((int) db) : juce::String ((int) db), juce::Rectangle<float> (fa.getCentreX() + 15.5f, y - 7.f, 22.f, 14.f).toNearestInt(), juce::Justification::centredRight);
            if (z) { g.setColour (theme::accent.withAlpha (.9f)); g.fillRect (lane.getX() + 1.f, y - .5f, lane.getWidth() - 2.f, 1.f); }   // línea de 0 dB
        }
        g.setColour (juce::Colour (0xff23252b)); g.fillRoundedRectangle (fa.withSizeKeepingCentre (2, fa.getHeight() - 14), 1.f);   // riel central
        if (lit)   // relleno del color de la pista desde abajo hasta la tapa
        {
            const float capY = fa.getBottom() - kCapH * .5f - (fa.getHeight() - kCapH) * pos, bot = fa.getBottom() - 7.f;
            if (bot > capY) { g.setColour (col.withAlpha (.28f)); g.fillRoundedRectangle (fa.getCentreX() - 3.f, capY, 6.f, bot - capY, 2.f); g.setColour (col.withAlpha (.9f)); g.fillRoundedRectangle (fa.getCentreX() - 1.f, capY, 2.f, bot - capY, 1.f); }
        }
    }
    void knob (juce::Graphics& g, juce::Rectangle<float> r, float pan, juce::Colour col = theme::accent)
    {
        const auto c = r.getCentre(); const float R = r.getWidth() * .5f, pi = juce::MathConstants<float>::pi, ang = pi * .75f * pan;
        { const float ar = R + 4.f; juce::Path tr; tr.addCentredArc (c.x, c.y, ar, ar, 0.f, -pi * .75f, pi * .75f, true); g.setColour (theme::seam); g.strokePath (tr, juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
          if (std::abs (pan) > .01f) { juce::Path va; va.addCentredArc (c.x, c.y, ar, ar, 0.f, std::min (0.f, ang), std::max (0.f, ang), true); g.setColour (col); g.strokePath (va, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
          g.setColour (theme::textDim); g.fillRect (c.x - .75f, c.y - ar - 3.f, 1.5f, 4.f); }   // marca del centro
        g.setColour (juce::Colours::black.withAlpha (.5f)); g.fillEllipse (r.translated (0.f, 1.5f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff484a53), c.x, r.getY(), juce::Colour (0xff2b2d33), c.x, r.getBottom(), false)); g.fillEllipse (r);
        g.setColour (theme::seam); g.drawEllipse (r, 1.f);
        g.setColour (juce::Colours::white.withAlpha (.13f)); { juce::Path hl; hl.addCentredArc (c.x, c.y, R - 1.2f, R - 1.2f, 0.f, -pi * .8f, pi * .1f, true); g.strokePath (hl, juce::PathStrokeType (1.f)); }
        g.setColour (juce::Colour (0xfff2f3f6)); g.drawLine (c.x + std::sin (ang) * R * .34f, c.y - std::cos (ang) * R * .34f, c.x + std::sin (ang) * R * .82f, c.y - std::cos (ang) * R * .82f, 2.2f);
    }
    void smButton (juce::Graphics& g, juce::Rectangle<float> r, const char* label, bool on, bool solo)
    {
        using namespace theme; const bool lim = std::string (label) == "LIM";
        const juce::Colour oc = lim ? accent : (solo ? juce::Colour (0xffe9c545) : juce::Colour (0xffe0663a));
        if (on) { g.setColour (oc); g.fillRoundedRectangle (r, rad); g.setColour (juce::Colours::white.withAlpha (.2f)); g.fillRect (r.getX() + rad, r.getY(), r.getWidth() - 2 * rad, 1.f); }
        else raised (g, r, panelHi.darker (.15f));
        g.setColour (on ? juce::Colour (0xff17181b) : textDim.brighter (.2f)); g.setFont (fonts::ui (13.f, true)); g.drawText (label, r.toNearestInt(), juce::Justification::centred);
    }
    std::map<int, float> glow;
    void drawWidth (juce::Graphics& g, juce::Rectangle<float> s, float wd, juce::Colour hc)
    {
        auto wr = widthOf (s);   // separación estéreo: 0 = mono, 1 = normal, 2 = ancho
        theme::well (g, wr);
        const float cx = wr.getX() + wr.getWidth() * .5f, vx = wr.getX() + wr.getWidth() * wd * .5f; g.setColour (hc.withAlpha (.7f)); g.fillRect (juce::Rectangle<float> (std::min (cx, vx), wr.getY() + 2.f, std::abs (vx - cx), wr.getHeight() - 4.f));
        g.setColour (theme::textDim); g.fillRect (cx - .5f, wr.getY() + 1.f, 1.f, 2.5f); g.fillRect (cx - .5f, wr.getBottom() - 3.5f, 1.f, 2.5f); g.setColour (theme::text); g.setFont (theme::fonts::digits (10.5f));
        g.drawText (wd < .02f ? juce::String ("MONO") : juce::String (juce::roundToInt (wd * 100.f)) + "%", wr.toNearestInt(), juce::Justification::centred);
    }
    void drawStrip (juce::Graphics& g, juce::Rectangle<float> s, int index, te::AudioTrack* t, float lvL, float lvR, bool master, float pkL = -100.f, float pkR = -100.f)
    {
        using namespace theme;
        const bool empty = ! master && t == nullptr;
        // canal: cuerpo grafito, cabecera más oscura con una barra de color arriba
        theme::raised (g, s, master ? panelHi.darker (.1f) : panel, 3.f);
        { juce::Path bz; bz.addRoundedRectangle (s.reduced (1.f), 3.f); g.saveState(); g.reduceClipRegion (bz); g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (.015f), 0.f, s.getY() + kHdrH, juce::Colours::black.withAlpha (.05f), 0.f, s.getBottom(), false)); g.fillRect (s); g.restoreState(); }   // bisel del canal: luz arriba, sombra abajo
        auto hd = s.withHeight ((float) kHdrH); const auto hc = master ? juce::Colour (0xffc4c7ce) : (empty ? juce::Colour (0xff3a3d45) : (t != nullptr ? trackutil::colourOf (t, index) : juce::Colour (0xff8d909a)));
        { juce::Path p; p.addRoundedRectangle (hd.getX() + 1.f, hd.getY() + 1.f, hd.getWidth() - 2.f, hd.getHeight() - 1.f, 3.f, 3.f, true, true, false, false); g.setColour (bgBar.brighter (.03f)); g.fillPath (p); }
        { juce::Path p; p.addRoundedRectangle (hd.getX() + 1.f, hd.getY() + 1.f, hd.getWidth() - 2.f, 4.f, 3.f, 3.f, true, true, false, false); g.setColour (hc); g.fillPath (p); }
        g.setColour (theme::seam); g.fillRect (hd.getX() + 1.f, hd.getBottom(), hd.getWidth() - 2.f, 1.f);
        const juce::String nm = master ? juce::String ("Master") : ((t && t->getName().isNotEmpty() && t->getName() != "Track") ? t->getName() : juce::String());
        if (master) { g.setColour (juce::Colours::white); g.setFont (fonts::ui (16.f, true)); g.drawText ("Master", hd.withTrimmedTop (4.f).toNearestInt(), juce::Justification::centred); }
        else
        {
            g.setColour (empty ? textFaint : text); g.setFont (fonts::digits (18.f)); g.drawText (juce::String (index + 1), hd.withTrimmedTop (6.f).withTrimmedBottom (nm.isEmpty() ? 0.f : 22.f).toNearestInt(), juce::Justification::centred);
            if (nm.isNotEmpty()) { g.setColour (text.withAlpha (.9f)); g.setFont (fonts::label (13.f)); g.drawText (nm, hd.withTop (hd.getY() + 31).withHeight (16.f).toNearestInt().reduced (4, 0), juce::Justification::centredTop, true); }
            if (t != nullptr)
            {   // destino de la salida (bus) o aviso de envíos; los buses llevan una marca
                auto* dst = outputOf (*t); int ns = 0; for (auto* p : t->pluginList.getPlugins()) if (dynamic_cast<te::AuxSendPlugin*> (p)) ++ns;
                juce::String lab = dst ? juce::String::fromUTF8 ("→ ") + chanName (*dst) : (ns > 0 ? juce::String::fromUTF8 ("↗ ") + juce::String (ns) + juce::String::fromUTF8 (ns == 1 ? " envío" : " envíos") : juce::String());
                g.setColour (textDim); g.setFont (fonts::ui (10.5f)); if (lab.isNotEmpty()) g.drawText (lab, hd.withTop (hd.getY() + 46).toNearestInt().reduced (4, 0), juce::Justification::centredTop, true);
                if (const int ic = (int) t->state.getProperty ("injIcon", 0)) drawChannelIcon (g, { hd.getX() + 6.f, hd.getY() + 9.f, 15.f, 15.f }, ic, textDim);
                auto pill = [&] (juce::Rectangle<float> pr, const char* tx) { g.setColour (boxFill); g.fillRoundedRectangle (pr, 2.f); g.setColour (hc.brighter (.2f)); g.setFont (fonts::label (9.5f)); g.drawText (tx, pr.toNearestInt(), juce::Justification::centred); };
                const bool narrow = hd.getWidth() < 80.f;
                if (linkOf (*t).isNotEmpty()) pill (narrow ? juce::Rectangle<float> (hd.getRight() - 17, hd.getY() + 9, 13, 13) : juce::Rectangle<float> (hd.getRight() - 34, hd.getY() + 9, 30, 13), narrow ? "L" : "LINK");
                if (isBus (*t)) pill (narrow ? juce::Rectangle<float> (hd.getRight() - 17, hd.getY() + 24, 13, 13) : juce::Rectangle<float> (hd.getX() + ((int) t->state.getProperty ("injIcon", 0) > 0 ? 24.f : 5.f), hd.getY() + 9, 28, 13), narrow ? "B" : "BUS");
            }
        }
        if (((! master && index == selected && ! masterSel) || (master && masterSel)) && ! empty) { g.setColour (theme::accent); g.drawRoundedRectangle (s.reduced (.5f), 3.f, 1.5f); }
        te::VolumeAndPanPlugin* vp = master ? edit.getMasterVolumePlugin().get() : (t ? t->getVolumePlugin() : nullptr);
        const float gl = glow.count (index) ? glow[index] : 0.f;
        if (gl > .01f && ! empty) { g.setColour (accentHi.withAlpha (gl * .55f)); g.drawEllipse (knobOf (s).expanded (2.f), 1.2f); }
        knob (g, knobOf (s), vp ? vp->getPan() : 0.f, (master || empty) ? accent : hc);
        const bool sig = std::max (lvL, lvR) > -60.f;
        if (master) { const bool hot = masterPeak > -0.1f; g.setColour (hot ? record : textDim); g.setFont (fonts::digits (11.f)); g.drawText (masterPeak <= -99.f ? juce::String ("pico -inf") : juce::String ("pico ") + juce::String (masterPeak, 1), peakR (s).toNearestInt(), juce::Justification::centred); }
        else
        {
            g.setColour (sig ? accentHi : juce::Colour (0xff3d4048)); g.fillEllipse (s.getX() + 10.f, knobOf (s).getBottom() + 12.f, 7.f, 7.f);
            drawWidth (g, s, t ? widthOfTrack (*t) : 1.f, hc);
        }
        auto fa = faderOf (s);
        if (gl > .01f && ! empty) { g.setColour (accentHi.withAlpha (gl * .5f)); g.drawRoundedRectangle (fa.withSizeKeepingCentre (22, fa.getHeight() - 6).expanded (2.f), 3.f, 1.f); }
        faderTrack (g, fa, master ? accent : hc, vp ? dbToPos (master && masterMuted ? preMute : vp->getVolumeDb()) : dbToPos (0.f), ! empty);
        auto mr = meterOf (s); theme::well (g, mr.expanded (3, 3), false, 2.f);
        ledMeter (g, { mr.getX(), mr.getY(), 9, mr.getHeight() }, lvL, pkL); ledMeter (g, { mr.getX() + 12, mr.getY(), 9, mr.getHeight() }, lvR, pkR);
        const float pos = vp ? dbToPos (master && masterMuted ? preMute : vp->getVolumeDb()) : dbToPos (0.f);
        cap (g, fa.getCentreX(), fa.getBottom() - kCapH * .5f - (fa.getHeight() - kCapH) * pos, ! master && index == selected && ! masterSel && ! empty);
        if (! master && ! empty && t)
        {
            auto ar = armOf (s); const bool armed = (bool) t->state.getProperty ("injArm", false);
            g.setColour (armed ? record : textDim); if (armed) g.fillEllipse (ar.reduced (2)); else g.drawEllipse (ar.reduced (2), 1.4f);
        }
        else if (master) drawWidth (g, s, widthOfMaster(), juce::Colour (0xff9a9da6));
        if (! master && ! empty && t && ! narrow (s))
        {
            auto ab = autoOf (s); const bool has = hasAuto (*t), shown = (bool) t->state.getProperty ("injAuto", false); if (has) { g.setColour (accent); g.fillRoundedRectangle (ab, theme::rad); } else theme::raised (g, ab, panelHi.darker (.15f));
            if (shown) { g.setColour (juce::Colours::white.withAlpha (.85f)); g.drawRoundedRectangle (ab.reduced (.5f), theme::rad, 1.f); }
            g.setColour (has ? juce::Colour (0xff17181b) : textDim.brighter (.2f)); g.setFont (fonts::ui (12.f, true)); g.drawText ("A", ab.toNearestInt(), juce::Justification::centred);
            if (auto* sp = sendFromSelected (index)) { drawMiniKnob (g, sendKnobOf (s), dbToPos (sp->getGainDb()), accent); }
        }
        if (! master && ! empty && t && ! narrow (s)) { auto sc = scOf (s); const bool on = trackutil::sidechainPlugin (*t) != nullptr; if (on) { g.setColour (accent); g.fillRoundedRectangle (sc, theme::rad); } else theme::raised (g, sc, panelHi.darker (.15f));
            g.setColour (on ? juce::Colour (0xff17181b) : textDim.brighter (.2f)); g.setFont (fonts::ui (11.f, true)); g.drawText ("SC", sc.toNearestInt(), juce::Justification::centred); }
        { auto cv = chevOf (s); juce::Path p; p.startNewSubPath (cv.getCentreX() - 6, cv.getCentreY() - 3); p.lineTo (cv.getCentreX(), cv.getCentreY() + 3); p.lineTo (cv.getCentreX() + 6, cv.getCentreY() - 3); g.setColour (textDim.brighter (.2f)); g.strokePath (p, juce::PathStrokeType (1.6f)); }
        if (! master) smButton (g, smOf (s, 0), "S", t && t->isSolo (false), true); else smButton (g, smOf (s, 0), "LIM", limiterOn(), false);
        smButton (g, smOf (s, 1), "M", master ? masterMuted : (t && t->isMuted (false)), false);
    }
    //---- ecualizador básico de 4 bandas por canal (plugin de Tracktion), como el de la mezcladora de FL
    struct Band { te::AutomatableParameter* f; te::AutomatableParameter* g; te::AutomatableParameter* q; };
    te::EqualiserPlugin* eqOfList (te::PluginList& pl, te::Plugin* before, bool create)
    {
        for (auto* p : pl.getPlugins()) if (auto* e = dynamic_cast<te::EqualiserPlugin*> (p)) return e;
        if (! create) return nullptr;
        auto np = edit.getPluginCache().createNewPlugin (te::EqualiserPlugin::xmlTypeName, {}); auto* e = dynamic_cast<te::EqualiserPlugin*> (np.get()); if (e == nullptr) return nullptr;
        const int vi = before ? pl.indexOf (before) : -1; pl.insertPlugin (np, vi, nullptr);   // después de los efectos, antes del volumen
        e->setLowFreq (100.f); e->setMidFreq1 (400.f); e->setMidFreq2 (2500.f); e->setHighFreq (8000.f); e->setLowGain (0.f); e->setMidGain1 (0.f); e->setMidGain2 (0.f); e->setHighGain (0.f);
        e->setLowQ (1.f); e->setMidQ1 (1.f); e->setMidQ2 (1.f); e->setHighQ (1.f); return e;
    }
    te::EqualiserPlugin* eqOf (te::AudioTrack* t, bool create) { return t ? eqOfList (t->pluginList, t->getVolumePlugin(), create) : nullptr; }
    te::EqualiserPlugin* eqTarget (bool create) { return masterSel ? eqOfList (edit.getMasterPluginList(), edit.getMasterVolumePlugin().get(), create) : eqOf (trackAt (selected), create); }
    static Band bandOf (te::EqualiserPlugin& e, int b) { return b == 0 ? Band { e.loFreq.get(), e.loGain.get(), e.loQ.get() } : b == 1 ? Band { e.midFreq1.get(), e.midGain1.get(), e.midQ1.get() } : b == 2 ? Band { e.midFreq2.get(), e.midGain2.get(), e.midQ2.get() } : Band { e.hiFreq.get(), e.hiGain.get(), e.hiQ.get() }; }
    static float fToX (juce::Rectangle<float> r, float f) { return r.getX() + 6.f + (r.getWidth() - 12.f) * std::log (juce::jlimit (20.f, 20000.f, f) / 20.f) / std::log (1000.f); }
    static float xToF (juce::Rectangle<float> r, float x) { return 20.f * std::pow (1000.f, juce::jlimit (0.f, 1.f, (x - r.getX() - 6.f) / (r.getWidth() - 12.f))); }
    static float gToY (juce::Rectangle<float> r, float g) { return r.getCentreY() - g / 20.f * (r.getHeight() * .5f - 6.f); }
    static float yToG (juce::Rectangle<float> r, float y) { return juce::jlimit (-20.f, 20.f, (r.getCentreY() - y) / (r.getHeight() * .5f - 6.f) * 20.f); }
    int eqNodeAt (juce::Point<float> p)
    {
        auto* e = eqTarget (false); auto gr = eqGraphR(); int best = -1; float bd = 14.f;
        for (int b = 0; b < 4; ++b)
        {
            float f = (float) (100.0 * std::pow (80.0, b / 3.0)) , gdb = 0.f;   // posiciones por defecto si aún no hay ecualizador
            if (e) { auto bd2 = bandOf (*e, b); f = bd2.f->getCurrentValue(); gdb = bd2.g->getCurrentValue(); } else f = b == 0 ? 100.f : b == 1 ? 400.f : b == 2 ? 2500.f : 8000.f;
            const float d = p.getDistanceFrom ({ fToX (gr, f), gToY (gr, gdb) }); if (d < bd) { bd = d; best = b; }
        }
        return best;
    }
    void eqDrag (const juce::MouseEvent& e)
    {
        auto* eq = eqTarget (true); if (eq == nullptr || eqBand < 0) return; auto gr = eqGraphR(); auto b = bandOf (*eq, eqBand);
        b.f->setParameter (juce::jlimit (20.f, 20000.f, xToF (gr, e.position.x)), juce::sendNotification);
        float g = yToG (gr, e.position.y); if (std::abs (g) < .6f) g = 0.f; b.g->setParameter (g, juce::sendNotification); repaint();
    }
    void eqClick (juce::Point<float> p, const juce::MouseEvent& e)
    {
        if (! masterSel && trackAt (selected) == nullptr) return;
        if (eqPowerR().expanded (3).contains (p)) { if (auto* eq = eqTarget (true)) eq->setEnabled (! eq->isEnabled()); repaint(); return; }
        if (eqPhaseR().contains (p)) { if (auto* eq = eqTarget (true)) eq->phaseInvert = ! (bool) eq->phaseInvert.get(); repaint(); return; }
        eqBand = eqNodeAt (p); active = { Hit::Eq, eqBand }; if (eqBand >= 0) eqDrag (e);
    }
    void drawEq (juce::Graphics& g, te::AudioTrack* t)
    {
        using namespace theme; juce::ignoreUnused (t); auto* eq = eqTarget (false); auto tr = eqTitleR(); auto gr = eqGraphR();
        g.setColour (text); g.setFont (fonts::ui (13.f)); g.drawText (juce::String::fromUTF8 ("Ecualizador"), tr.withTrimmedLeft (30).toNearestInt(), juce::Justification::centredLeft);
        auto pw = eqPowerR(); const bool on = eq == nullptr || eq->isEnabled(); g.setColour (on && eq ? accent : textFaint); if (eq && on) g.fillEllipse (pw.reduced (3)); else g.drawEllipse (pw.reduced (3), 1.3f);
        auto ph = eqPhaseR(); const bool inv = eq && (bool) eq->phaseInvert.get(); if (inv) { g.setColour (accent); g.fillRoundedRectangle (ph, rad); } else raised (g, ph, panelHi.darker (.15f));
        g.setColour (inv ? juce::Colour (0xff17181b) : textDim.brighter (.2f)); g.setFont (fonts::ui (13.f, true)); g.drawText (juce::String::fromUTF8 ("Ø"), ph.toNearestInt(), juce::Justification::centred);
        well (g, gr, false, 3.f);
        g.setFont (fonts::digits (10.f));
        for (float db : { -15.f, -10.f, -5.f, 5.f, 10.f, 15.f }) { g.setColour (juce::Colour (0xff23252b)); g.fillRect (gr.getX() + 4, gToY (gr, db) - .5f, gr.getWidth() - 8, 1.f); }
        for (float f : { 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f })
        {
            const float x = fToX (gr, f), lab = f == 100.f || f == 1000.f || f == 10000.f; g.setColour (juce::Colour (lab ? 0xff30323a : 0xff1f2126)); g.fillRect (x - .5f, gr.getY() + 4, 1.f, gr.getHeight() - 8);
            if (lab) { g.setColour (textDim); g.drawText (f == 100.f ? "100" : (f == 1000.f ? "1k" : "10k"), juce::Rectangle<float> (x - 14, gr.getBottom() - 15, 28, 12).toNearestInt(), juce::Justification::centred); }
        }
        g.setColour (juce::Colour (0xff3d4048)); g.fillRect (gr.getX() + 4, gr.getCentreY() - .5f, gr.getWidth() - 8, 1.f);   // 0 dB
        juce::Path curve; juce::Path fillP; const int N = 96; bool started = false;
        for (int i = 0; i <= N; ++i)
        {
            const float x = gr.getX() + 6.f + (gr.getWidth() - 12.f) * (float) i / N, f = xToF (gr, x); const float db = eq ? juce::jlimit (-20.f, 20.f, eq->getDBGainAtFrequency (f)) : 0.f, y = gToY (gr, db);
            if (! started) { curve.startNewSubPath (x, y); fillP.startNewSubPath (x, gr.getCentreY()); fillP.lineTo (x, y); started = true; } else { curve.lineTo (x, y); fillP.lineTo (x, y); }
        }
        fillP.lineTo (gr.getRight() - 6.f, gr.getCentreY()); fillP.closeSubPath();
        g.setColour (accent.withAlpha (on ? .16f : .05f)); g.fillPath (fillP); g.setColour (on ? accentHi : textFaint); g.strokePath (curve, juce::PathStrokeType (1.6f));
        for (int b = 0; b < 4; ++b)
        {
            float f = b == 0 ? 100.f : b == 1 ? 400.f : b == 2 ? 2500.f : 8000.f, gdb = 0.f; if (eq) { auto bd = bandOf (*eq, b); f = bd.f->getCurrentValue(); gdb = bd.g->getCurrentValue(); }
            const juce::Point<float> c (fToX (gr, f), gToY (gr, gdb)); const bool act = active.kind == Hit::Eq && active.idx == b;
            g.setColour (juce::Colours::black.withAlpha (.5f)); g.fillEllipse (c.x - 6.f, c.y - 4.f, 12.f, 12.f);
            g.setColour (act ? juce::Colours::white : accent); g.fillEllipse (c.x - 5.5f, c.y - 5.5f, 11.f, 11.f); g.setColour (juce::Colour (0xff17181b)); g.drawEllipse (c.x - 5.5f, c.y - 5.5f, 11.f, 11.f, 1.f);
            g.setFont (fonts::digits (9.5f)); g.drawText (juce::String (b + 1), juce::Rectangle<float> (c.x - 6, c.y - 6, 12, 12).toNearestInt(), juce::Justification::centred);
            if (act && eq) { g.setColour (juce::Colours::white); g.setFont (juce::FontOptions (11.f)); auto bd = bandOf (*eq, b); const float ff = bd.f->getCurrentValue();
                g.drawText ((ff >= 1000.f ? juce::String (ff / 1000.f, 2) + " kHz" : juce::String (juce::roundToInt (ff)) + " Hz") + "   " + juce::String (bd.g->getCurrentValue(), 1) + " dB   Q " + juce::String (bd.q->getCurrentValue(), 1), gr.withHeight (16.f).reduced (6, 0).toNearestInt(), juce::Justification::centredLeft); }
        }
    }
    void drawEffects (juce::Graphics& g)
    {
        using namespace theme; auto r = fxR();
        raised (g, r, panel, 3.f);
        auto* t = trackAt (selected); const juce::String nm = masterSel ? juce::String ("Master") : (t ? (t->getName() == "Track" ? juce::String (selected + 1) : t->getName()) : juce::String());
        g.setColour (text); g.setFont (fonts::ui (13.f, true)); g.drawText (juce::String::fromUTF8 ("Mixer \xe2\x80\x94 ") + nm, r.withHeight (42.f).withTrimmedLeft (14).toNearestInt(), juce::Justification::centredLeft);
        g.setColour (seam); g.fillRect (r.getX() + 8.f, r.getY() + 42.f, r.getWidth() - 16.f, 1.f);
        auto fx = currentFx();
        drawEq (g, t);
        for (int n = 0; n < 10; ++n)
        {
            auto row = fxRow (n); const bool used = n < fx.size();
            if (used) { g.setColour (bgBar2.brighter (.06f)); g.fillRoundedRectangle (row, rad); g.setColour (seam); g.drawRoundedRectangle (row.reduced (.5f), rad, 1.f); }
            else { g.setColour (boxFill.withAlpha (.55f)); g.fillRoundedRectangle (row, rad); g.setColour (boxLine.withAlpha (.6f)); g.drawRoundedRectangle (row.reduced (.5f), rad, 1.f); }
            auto dot = juce::Rectangle<float> (row.getX() + 10, row.getCentreY() - 6, 12, 12);
            if (used) { const bool on = fx[n]->isEnabled(); if (on) { g.setColour (accent.withAlpha (.25f)); g.fillEllipse (dot.expanded (2.f)); g.setColour (accent); g.fillEllipse (dot); } else { g.setColour (juce::Colour (0xff4a4d56)); g.fillEllipse (dot); } }
            else { g.setColour (textFaint); g.drawEllipse (dot.reduced (1), 1.2f); }
            g.setColour (used ? text : textFaint); g.setFont (fonts::ui (13.f));
            g.drawText (used ? fx[n]->getName() : "Slot " + juce::String (n + 1), row.withTrimmedLeft (34).withTrimmedRight (asExt (used ? fx[n] : nullptr) ? 66 : 36).toNearestInt(), juce::Justification::centredLeft);
            if (used && asExt (fx[n])) drawMiniKnob (g, mixKnobOf (row), mixOf (fx[n]), accent);
            if (used) icons::draw (g, icons::Dots, { row.getRight() - 32, row.getY(), 26, row.getHeight() }, textDim);
            if (! used) { juce::Path p; auto c = juce::Point<float> (row.getRight() - 20, row.getCentreY()); p.startNewSubPath (c.x - 5, c.y - 2.5f); p.lineTo (c.x, c.y + 2.5f); p.lineTo (c.x + 5, c.y - 2.5f); g.setColour (textFaint); g.strokePath (p, juce::PathStrokeType (1.4f)); }
        }
    }
};
