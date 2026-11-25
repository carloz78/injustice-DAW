#pragma once
#include "Icons.h"
#include "TimeSig.h"
#include "Ideas.h"
#include <functional>
#include <map>

// Barra superior única: logo, menús, transporte, tempo/compás/posición, metrónomo, CPU/RAM (fila 1)
// y vistas + herramientas (fila 2). Un solo componente con lista de zonas pulsables.
class TopBars : public juce::Component, public juce::TooltipClient, private juce::Timer
{
public:
    void openMenuForTest (const juce::String& n) { showMenu (n); }
    struct Btn { juce::String id; icons::Id icon; juce::String label; juce::Rectangle<float> r; bool enabled = true; };

    std::function<void (const juce::String&)> onButton;
    std::function<void (double)> onBpm;
    std::function<void (int, int)> onSig;
    int countIn = 0; bool metro = false, pianoMode = false, mixerOpen = false, stepsOpen = false, metersOpen = false, perfOpen = false, aedOpen = false, rackOpen = false, followOn = false, abOn = false, clickRec = false, browserOpen = true, recArmed = false;
    bool playing = false, looping = false, snap = true;
    juce::String quantLabel = "1/16", tool = "select", view = "playlist", crumb;
    bool timeMode = false; double bpm = 172.0, posBeats = 0.0, cpu = 0.0, ramMB = 0.0, ramTotalMB = 0.0;

    static constexpr int kRow1 = 56, kRow2 = 46, kHeight = kRow1 + kRow2;

    TopBars() { setOpaque (true); }
    // fundido de brillo al pasar el mouse sobre un botón (sube rápido, baja un poco más lento)
    std::map<juce::String, float> hAmt;
    float hoverAmt (const juce::String& id) const { auto it = hAmt.find (id); return it == hAmt.end() ? 0.f : it->second; }
    float pulse = 0.f;   // destello del tempo en cada pulso
    void beatPulse() { pulse = 1.f; startTimerHz (60); repaint(); }
    void timerCallback() override
    {
        bool moving = false; if (pulse > 0.f) { pulse = pulse < .03f ? 0.f : pulse * .86f; moving = true; }
        for (auto it = hAmt.begin(); it != hAmt.end();) { const float t = it->first == hoverId ? 1.f : 0.f, d = t - it->second; if (std::abs (d) < .02f) { it->second = t; if (t == 0.f) { it = hAmt.erase (it); continue; } } else { it->second += d * (d > 0 ? .4f : .22f); moving = true; } ++it; }
        if (! hoverId.isEmpty() && hAmt.find (hoverId) == hAmt.end()) hAmt[hoverId] = 0.f;
        if (moving) repaint(); else stopTimer();
    }
    void resized() override { build(); repaint(); }

    //==============================================================================
    void paint (juce::Graphics& g) override
    {
        using namespace theme;
        g.setColour (bgBar);  g.fillRect (0, 0, getWidth(), kRow1);
        g.setColour (bgBar2); g.fillRect (0, kRow1, getWidth(), kRow2);
        g.setColour (seam); g.fillRect (0, kRow1 - 1, getWidth(), 1);
        g.setColour (edgeLight); g.fillRect (0, kRow1, getWidth(), 1);
        g.setColour (seam); g.fillRect (0, kHeight - 1, getWidth(), 1);

        drawLogo (g);
        // menús: texto limpio; al pasar o abrir, una pastilla muy sutil
        g.setFont (fonts::label (13.5f));
        for (auto& m : menus)
        {
            const bool open = openMenu == m.name, hot = open || (hoverId == "menu:" + m.name);
            if (hot) { g.setColour (juce::Colours::white.withAlpha (open ? .10f : .055f)); g.fillRoundedRectangle (m.r.reduced (0.f, 4.f), rad); }
            g.setColour (hot ? text : text.withAlpha (.82f)); g.drawText (m.name, m.r.toNearestInt(), juce::Justification::centred);
        }
        // grupo de transporte: un solo bloque hundido con tres botones separados por finas líneas
        well (g, transportR);
        g.setColour (boxLine); for (int k = 1; k < 3; ++k) g.fillRect (transportR.getX() + k * transportR.getWidth() / 3.f, transportR.getY() + 8.f, 1.f, transportR.getHeight() - 16.f);
        for (auto& b : btns) if (isTransport (b.id)) paintBtn (g, b);
        // contadores: tempo, compás y posición (cifras tabulares, rótulo pequeño arriba)
        auto counter = [&] (juce::Rectangle<float> r, const char* cap, const juce::String& val, juce::Colour vc, bool hot)
        {
            well (g, r, hot);
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (.16f), 0.f, r.getY() + 1.f, juce::Colours::transparentBlack, 0.f, r.getY() + r.getHeight() * .55f, false));   // pantalla LCD: sombra interior arriba
            g.fillRoundedRectangle (r.reduced (1.f), rad);
            g.setColour (textFaint); g.setFont (fonts::label (9.5f)); g.drawText (juce::String::fromUTF8 (cap), r.toNearestInt().withTrimmedTop (3).withHeight (12), juce::Justification::centred);
            g.setFont (fonts::digits (18.f));
            g.setColour (vc.withAlpha (.16f)); for (float dx : { -1.f, 1.f }) g.drawText (val, r.toNearestInt().withTrimmedTop (13).translated ((int) dx, 0), juce::Justification::centred);   // halo suave de la cifra
            g.setColour (vc); g.drawText (val, r.toNearestInt().withTrimmedTop (13), juce::Justification::centred);
        };
        if (pulse > 0.f && playing) { g.setColour (accent.withAlpha (pulse * .5f)); g.drawRoundedRectangle (bpmR.expanded (pulse * 2.f), rad, 1.f + pulse); }
        counter (bpmR, "TEMPO", juce::String (bpm, 2), accentHi, hoverId == "bpm" || dragBpm);
        if (pulse > 0.f && playing) { g.setColour (accent.withAlpha (pulse * .16f)); g.fillRoundedRectangle (bpmR.reduced (1.f), rad); }
        counter (sigR, juce::CharPointer_UTF8 ("COMPÁS"), juce::String (timesig::num) + "/" + juce::String (timesig::den), text, false);
        counter (posR, timeMode ? "TIEMPO" : juce::CharPointer_UTF8 ("POSICIÓN"), posText(), text, false);
        for (auto& b : btns) if (b.id == "metro" || b.id == "loop") paintBtn (g, b);
        drawMeters (g);

        // fila 2: vistas y herramientas
        for (auto& b : btns) if (! isTransport (b.id) && b.id != "metro" && b.id != "loop") paintBtn (g, b);
        g.setColour (boxLine); g.fillRect (divX, (float) kRow1 + 12.f, 1.f, (float) kRow2 - 24.f);
        if (pianoMode)
        {
            const bool hot = hoverId == "quant" || pressedId == "quant";
            raised (g, quantR, hot ? panelHi : panel);
            g.setColour (text); g.setFont (fonts::digits (13.f)); g.drawText (quantLabel, quantR.withTrimmedRight (20).toNearestInt(), juce::Justification::centred);
            icons::draw (g, icons::Chevron, { quantR.getRight() - 28.f, quantR.getY(), 26.f, quantR.getHeight() }, textDim);
        }
    }

    //==============================================================================
    juce::StringArray recents, myTemplates;   // proyectos recientes y plantillas propias (las llena la ventana principal)
    std::function<void()> onTap;   // tap tempo (clic derecho sobre el BPM)
    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto p = e.position; pressedId = {};
        if (e.mods.isPopupMenu() && bpmR.contains (p)) { if (onTap) onTap(); beatPulse(); return; }
        for (auto& m : menus) if (m.r.contains (p)) { showMenu (m.name); return; }
        if (pianoMode && quantR.contains (p)) { pressedId = "quant"; repaint(); if (onButton) onButton ("quant"); return; }
        for (auto& b : btns) if (b.enabled && b.r.contains (p)) { pressedId = b.id; repaint(); if (onButton) onButton (b.id); return; }
        if (sigR.contains (p)) { showSigMenu(); return; }
        if (posR.contains (p)) { timeMode = ! timeMode; repaint(); return; }
        dragBpm = bpmR.contains (p); lastY = p.y;
    }
    void mouseUp (const juce::MouseEvent&) override { if (pressedId.isNotEmpty() || dragBpm) { pressedId = {}; dragBpm = false; repaint(); } }
    void mouseMove (const juce::MouseEvent& e) override { setHover (hitId (e.position)); }
    void mouseExit (const juce::MouseEvent&) override { setHover ({}); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragBpm) return;
        const double d = (lastY - e.position.y) * (e.mods.isShiftDown() ? 0.01 : 0.25); lastY = e.position.y;
        bpm = juce::jlimit (20.0, 400.0, bpm + d); if (onBpm) onBpm (bpm); repaint();
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (bpmR.contains (e.position)) { bpm = juce::jlimit (20.0, 400.0, bpm + w.deltaY * 4.0); if (onBpm) onBpm (bpm); repaint(); }
    }

    juce::String getTooltip() override
    {
        auto p = getMouseXYRelative().toFloat();
        for (auto& b : btns) if (b.r.contains (p)) return tip (b.id);
        if (sigR.contains (p)) return juce::String::fromUTF8 ("Comp\xc3\xa1s: clic para cambiarlo (3/4, 6/8, 7/8...)");
        if (bpmR.contains (p)) return juce::String::fromUTF8 ("Tempo: arrastra o usa la rueda (Mayús = fino)");
        if (posR.contains (p)) return juce::String::fromUTF8 ("Posición (clic: compases / minutos)");
        if (pianoMode && quantR.contains (p)) return juce::String::fromUTF8 ("Cuantización (clic para cambiar)");
        return {};
    }
    static juce::String tip (const juce::String& id)
    {
        static const std::map<juce::String, const char*> t {
            { "layout", "Mostrar/ocultar el explorador (F8)" }, { "undo", "Deshacer (Ctrl+Z)" }, { "redo", "Rehacer (Ctrl+Y)" },
            { "play", "Reproducir / pausa (Espacio)" }, { "stop", "Detener y volver al inicio" }, { "rec", "Grabar en las pistas armadas (c\xc3\xadrculo del Mixer). Ctrl+T: teclado como piano" },
            { "loop", "Repetir en bucle" }, { "audio", "Ajustes de audio" }, { "plugins", "Lista de plugins VST3 (F10)" },
            { "tool:select", "Seleccionar" }, { "tool:pencil", "L\xc3\xa1piz: dibujar" }, { "tool:brush", "Pincel: pinta clips arrastrando (sobre un clip lo copia)" }, { "tool:auto", "Automatizaci\xc3\xb3n: curva de volumen (clic crea punto, clic derecho lo borra)" }, { "tool:cut", "Cortar clips" }, { "tool:trim", "Recortar el borde de un clip" },
            { "tool:erase", "Borrador" }, { "metro", "Metr\xc3\xb3nomo" }, { "snap", "Ajustar a la cuadr\xc3\xad" "cula" },
            { "view:playlist", "Playlist (F5)" }, { "view:mixer", "Mixer (F9)" }, { "view:piano", "Piano Roll (F7)" }, { "view:steps", "Step Sequencer (F6)" } };
        auto it = t.find (id); return it == t.end() ? juce::String() : juce::String::fromUTF8 (it->second);
    }

private:
    struct Menu { juce::String name; juce::Rectangle<float> r; };
    std::vector<Btn> btns; std::vector<Menu> menus;
    juce::Rectangle<float> bpmR, sigR, posR, quantR, transportR, cpuR, ramR; float divX = 0;
    juce::String hoverId, pressedId, openMenu; bool dragBpm = false; float lastY = 0;

    static bool isTransport (const juce::String& id) { return id == "play" || id == "stop" || id == "rec"; }
    juce::String posText() const
    {
        if (timeMode) { const double sec = std::max (0.0, posBeats) * 60.0 / std::max (1.0, bpm); const int m = (int) (sec / 60.0); return juce::String (m) + ":" + juce::String (std::fmod (sec, 60.0), 2).paddedLeft ('0', 5); }
        const double b = std::max (0.0, posBeats); const int bar = (int) (b / 4.0) + 1; const double inBar = std::fmod (b, 4.0);
        return juce::String (bar) + "." + juce::String ((int) inBar + 1) + "." + juce::String ((int) ((inBar - std::floor (inBar)) * 4.0) + 1);
    }
    juce::String hitId (juce::Point<float> p) const
    {
        for (auto& m : menus) if (m.r.contains (p)) return "menu:" + m.name;
        for (auto& b : btns) if (b.enabled && b.r.contains (p)) return b.id;
        if (pianoMode && quantR.contains (p)) return "quant";
        if (bpmR.contains (p)) return "bpm";
        return {};
    }
    void setHover (const juce::String& id) { if (id != hoverId) { hoverId = id; if (! id.isEmpty() && hAmt.find (id) == hAmt.end()) hAmt[id] = 0.f; startTimerHz (60); repaint(); } }

    bool isOn (const Btn& b) const
    {
        if (b.id == "play") return playing;
        if (b.id == "loop") return looping;
        if (b.id == "snap") return snap;
        if (b.id == "metro") return metro;
        if (b.id == "rec") return recArmed;
        if (b.id.startsWith ("tool:")) return b.id.substring (5) == tool;
        if (b.id == "view:mixer") return mixerOpen;
        if (b.id == "view:steps") return stepsOpen;
        if (b.id == "view:meters") return metersOpen;
        if (b.id == "view:perf") return perfOpen;
        if (b.id == "view:aed") return aedOpen;
        if (b.id == "view:rack") return rackOpen;
        if (b.id == "layout") return browserOpen;
        if (b.id.startsWith ("view:")) return b.id.substring (5) == view;
        return false;
    }

    void paintBtn (juce::Graphics& g, const Btn& b)
    {
        using namespace theme;
        const bool on = isOn (b), hot = hoverId == b.id && b.enabled, down = pressedId == b.id;
        auto r = b.r; const bool isRec = b.id == "rec", isPlay = b.id == "play", isTransportBtn = isTransport (b.id), isTab = b.id.startsWith ("view:") && b.label.isNotEmpty();
        const float hv = hoverAmt (b.id);
        auto col = iconIdle; auto lab = textDim;
        if (isTab)
        {   // pestañas de vista: activa = panel elevado + línea naranja abajo (como Studio One)
            if (on) { raised (g, r.reduced (0.f, 4.f), panel); g.setColour (accent); g.fillRect (r.getX() + 6.f, r.getBottom() - 6.f, r.getWidth() - 12.f, 2.f); col = accentHi; lab = text; }
            else { if (hv > .01f && b.enabled) { g.setColour (juce::Colours::white.withAlpha (.05f * hv)); g.fillRoundedRectangle (r.reduced (0.f, 4.f), rad); } col = hot ? text : iconIdle.withAlpha (.8f); lab = hot ? text : textDim; }
        }
        else if (isTransportBtn)
        {
            auto cell = r.reduced (3.f);
            if (isRec) { if (on) { g.setColour (record.withAlpha (down ? .5f : .34f)); g.fillRoundedRectangle (cell, rad); g.setColour (record.withAlpha (.9f)); g.drawRoundedRectangle (cell.reduced (.5f), rad, 1.f); } }
            else if (isPlay && on) { g.setColour (ok.withAlpha (.20f)); g.fillRoundedRectangle (cell, rad); g.setColour (ok.withAlpha (.75f)); g.drawRoundedRectangle (cell.reduced (.5f), rad, 1.f); }
            if (down) { g.setColour (juce::Colours::white.withAlpha (.12f)); g.fillRoundedRectangle (cell, rad); }
            else if (hv > .01f && b.enabled) { g.setColour (juce::Colours::white.withAlpha (.07f * hv)); g.fillRoundedRectangle (cell, rad); }
            col = isRec ? (on ? record.brighter (.3f) : record) : (isPlay ? ok : iconIdle);
        }
        else
        {   // herramientas y botones de icono: activo = tinte naranja con borde fino; encima = velo claro
            auto cell = r.reduced (2.f, 4.f);
            if (on) { g.setColour (accent.withAlpha (down ? .32f : .22f)); g.fillRoundedRectangle (cell, rad); g.setColour (accent.withAlpha (.85f)); g.drawRoundedRectangle (cell.reduced (.5f), rad, 1.f); col = accentHi; }
            else if (down) { g.setColour (juce::Colours::white.withAlpha (.12f)); g.fillRoundedRectangle (cell, rad); }
            else if (hv > .01f && b.enabled) { g.setColour (juce::Colours::white.withAlpha (.07f * hv)); g.fillRoundedRectangle (cell, rad); col = text; }
        }
        if (! b.enabled) { col = col.withAlpha (.3f); lab = lab.withAlpha (.3f); }
        if (b.label.isEmpty()) { icons::draw (g, b.icon, r, col); return; }
        icons::draw (g, b.icon, { r.getX() + 8.f, r.getY(), 28.f, r.getHeight() }, col);
        g.setColour (lab); g.setFont (fonts::label (13.5f)); g.drawText (b.label, r.withTrimmedLeft (38.f).toNearestInt(), juce::Justification::centredLeft);
    }

    void drawLogo (juce::Graphics& g)
    {
        using namespace theme;
        // marca: cuadrado grafito con una barra inclinada naranja (la "I" de Injustice cortada en diagonal)
        const juce::Rectangle<float> m (16.f, 12.f, 32.f, 32.f);
        g.setColour (juce::Colour (0xff0b0b0d)); g.fillRoundedRectangle (m, 4.f);
        g.setColour (juce::Colour (0xff2a2c33)); g.drawRoundedRectangle (m.reduced (.5f), 4.f, 1.f);
        { juce::Path s; s.addQuadrilateral (m.getX() + 12.f, m.getBottom() - 7.f, m.getX() + 17.f, m.getY() + 7.f, m.getX() + 23.f, m.getY() + 7.f, m.getX() + 18.f, m.getBottom() - 7.f); g.setColour (accent); g.fillPath (s); }
        g.setColour (juce::Colours::white.withAlpha (.9f)); g.fillRect (m.getX() + 5.f, m.getBottom() - 8.f, 7.f, 2.f);
        juce::Font f1 = fonts::ui (16.5f, true); juce::GlyphArrangement ga; ga.addLineOfText (f1, "INJUSTICE", 0, 0);
        const int w1 = (int) std::ceil (ga.getBoundingBox (0, -1, true).getWidth());
        g.setColour (text); g.setFont (f1); g.drawText ("INJUSTICE", 58, 8, w1 + 4, 40, juce::Justification::centredLeft);
        g.setColour (accentHi); g.setFont (fonts::label (11.f)); g.drawText ("DAW", 58 + w1 + 7, 8, 40, 40, juce::Justification::centredLeft);
    }

    void drawMeters (juce::Graphics& g)
    {
        using namespace theme;
        auto one = [&] (juce::Rectangle<float> r, const char* name, juce::String val, juce::String extra, float f)
        {
            g.setColour (textFaint); g.setFont (fonts::label (10.f)); g.drawText (name, (int) r.getX(), (int) r.getY() + 7, 34, 14, juce::Justification::centredLeft);
            g.setColour (text); g.setFont (fonts::digits (13.f)); g.drawText (val, (int) r.getX() + 32, (int) r.getY() + 6, 44, 16, juce::Justification::centredLeft);
            if (extra.isNotEmpty()) { g.setColour (textDim); g.setFont (fonts::digits (11.f)); g.drawText (extra, (int) r.getX() + 66, (int) r.getY() + 7, 70, 14, juce::Justification::centredLeft); }
            auto bar = juce::Rectangle<float> (r.getX(), r.getY() + 27.f, r.getWidth(), 4.f);
            g.setColour (boxFill); g.fillRoundedRectangle (bar, 1.f); g.setColour (seam); g.drawRoundedRectangle (bar.reduced (.5f), 1.f, 1.f);
            g.setColour (f > .85f ? record : f > .6f ? pink : accent); g.fillRoundedRectangle (bar.reduced (1.f).withWidth (juce::jmax (2.f, (bar.getWidth() - 2.f) * juce::jlimit (0.f, 1.f, f))), 1.f);
        };
        if (! cpuR.isEmpty()) one (cpuR, "CPU", juce::String (juce::roundToInt (cpu * 100.0)) + "%", {}, (float) cpu);
        const float f = ramTotalMB > 0 ? (float) (ramMB / ramTotalMB) : 0.f;
        if (! ramR.isEmpty()) one (ramR, "RAM", juce::String (ramMB / 1024.0, 1), juce::String ("/ ") + juce::String (juce::roundToInt (ramTotalMB / 1024.0)) + " GB", f);
    }

    void showSigMenu()
    {
        juce::PopupMenu m; const std::pair<int, int> sigs[] = { { 4, 4 }, { 3, 4 }, { 2, 4 }, { 5, 4 }, { 6, 4 }, { 7, 4 }, { 6, 8 }, { 7, 8 }, { 9, 8 }, { 12, 8 }, { 3, 8 }, { 5, 8 } };
        int id = 1; for (auto& sg : sigs) m.addItem (id++, juce::String (sg.first) + "/" + juce::String (sg.second), true, sg.first == timesig::num && sg.second == timesig::den);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (sigR.toNearestInt())), [this] (int r) { if (r > 0 && onSig) onSig (sigs_[r - 1].first, sigs_[r - 1].second); });
    }
    static constexpr std::pair<int, int> sigs_[12] = { { 4, 4 }, { 3, 4 }, { 2, 4 }, { 5, 4 }, { 6, 4 }, { 7, 4 }, { 6, 8 }, { 7, 8 }, { 9, 8 }, { 12, 8 }, { 3, 8 }, { 5, 8 } };
    void showMenu (const juce::String& name)
    {
        struct It { juce::String id, label; bool tick = false, en = true; };
        std::vector<It> items;   // id vacío = separador
        auto I = [&] (const char* id, const char* l, bool tick = false) { items.push_back ({ id, juce::String::fromUTF8 (l), tick, true }); };
        auto S = [&] { items.push_back ({}); };
        if (name == "File") { I ("new", "Proyecto nuevo\tCtrl+N"); S(); for (int k = 0; k < ideas::kNumStyles; ++k) items.push_back ({ "tpl:" + juce::String (k), juce::String::fromUTF8 ("Plantilla: ") + ideas::styleName (k) + " (" + juce::String ((int) ideas::styleBpm (k)) + " BPM)", false, true }); S(); I ("savetpl", "Guardar como plantilla…"); for (int k = 0; k < myTemplates.size(); ++k) items.push_back ({ "mytpl:" + juce::String (k), juce::String::fromUTF8 ("Mi plantilla: ") + myTemplates[k], false, true }); S(); I ("open", "Abrir…\tCtrl+O"); for (int k = 0; k < recents.size(); ++k) items.push_back ({ "recent:" + juce::String (k), juce::String::fromUTF8 ("Reciente: ") + juce::File (recents[k]).getFileNameWithoutExtension(), false, true }); S(); I ("save", "Guardar\tCtrl+S"); I ("saveas", "Guardar como…\tCtrl+Mayús+S"); I ("savesamples", "Guardar con los samples…"); S(); I ("importmidi", "Importar MIDI…"); I ("export", "Exportar audio…\tCtrl+E"); S(); I ("quit", "Salir"); }
        else if (name == "Edit") { I ("undo", "Deshacer\tCtrl+Z"); I ("redo", "Rehacer\tCtrl+Y"); S(); I ("history", "Historial de cambios…"); S(); I ("dup", "Duplicar clip\tCtrl+D"); I ("del", "Eliminar clip\tSupr"); S(); I ("clearsolo", "Quitar todos los solos y silencios"); S(); I ("copy", "Copiar\tCtrl+C"); I ("cut", "Cortar\tCtrl+X"); I ("paste", "Pegar en el cursor\tCtrl+V"); I ("selall", "Elegir todos los clips\tCtrl+A"); I ("invertsel", "Invertir la selección\tCtrl+I"); S(); I ("palette", "Paleta de comandos…\tCtrl+K"); }
        else if (name == "View") { I ("view:playlist", "Playlist\tF5", view == "playlist" && ! pianoMode); I ("view:mixer", "Mixer\tF9", mixerOpen); I ("view:piano", "Piano Roll\tF7", pianoMode); I ("view:steps", "Step Sequencer\tF6", stepsOpen); I ("view:meters", "Medidores\tF11", metersOpen); I ("view:perf", "Modo ensayo\tF12", perfOpen); I ("view:aed", "Editor de audio\tCtrl+Mayús+E", aedOpen); I ("view:pitch", "Editor de afinación (Injustice Auto)\tCtrl+Mayús+P"); I ("view:rack", "Rack de pads", rackOpen); S(); S(); I ("follow", "Seguir el cabezal\tF4", followOn); I ("zoomall", "Ver todo el proyecto\tCtrl+0"); I ("zoomsel", "Ver lo elegido\tCtrl+Mayús+0"); S(); I ("layout", "Explorador de samples\tF8", browserOpen); }
        else if (name == "Options") { I ("audio", "Ajustes de audio…"); S(); I ("metro", "Metrónomo", metro); I ("loop", "Repetir en bucle", looping); I ("snap", "Ajustar a la cuadrícula", snap); I ("clickrec", "Metrónomo solo al grabar", clickRec); S(); { const char* nm[] = { "Sin cuenta regresiva", "Cuenta regresiva: 1 compás", "Cuenta regresiva: 2 compases", "Cuenta regresiva: 2 tiempos", "Cuenta regresiva: 1 tiempo" }; for (int k = 0; k < 5; ++k) items.push_back ({ "countin:" + juce::String (k), juce::String::fromUTF8 (nm[k]), countIn == k, true }); } S(); I ("tap", "Tap tempo  (clic derecho en el BPM)"); }
        else if (name == "Tools") { I ("loadref", "Referencia: cargar canción…"); I ("ab", "Comparar con la referencia (A/B)\tF3", abOn); S(); I ("palette", "Paleta de comandos…\tCtrl+K"); I ("midilearn", "MIDI learn: asignar mandos…"); I ("plugins", "Lista de plugins VST3…\tF10"); I ("ideas", "Generador de ideas…\tCtrl+G"); I ("ideastudio", "IA de ideas: escribe qué quieres…"); S(); I ("tool:select", "Herramienta: seleccionar", tool == "select"); I ("tool:pencil", "Herramienta: lápiz", tool == "pencil"); I ("tool:brush", "Herramienta: pincel", tool == "brush"); I ("tool:auto", "Herramienta: automatización", tool == "auto"); I ("tool:cut", "Herramienta: cortar", tool == "cut"); I ("tool:trim", "Herramienta: recortar", tool == "trim"); }
        else if (name == "Help") { I ("help", "Ayuda y atajos\tF1"); I ("about", "Acerca de INJUSTICE DAW"); }
        juce::PopupMenu m; int n = 1;
        for (auto& it : items) { if (it.id.isEmpty()) m.addSeparator(); else { juce::PopupMenu::Item pi (it.label.upToFirstOccurrenceOf ("\t", false, false)); pi.itemID = n; pi.isEnabled = it.en; pi.isTicked = it.tick; if (it.label.contains ("\t")) pi.shortcutKeyDescription = it.label.fromFirstOccurrenceOf ("\t", false, false); m.addItem (pi); } ++n; }
        openMenu = name; repaint(); juce::Rectangle<float> tr;
        for (auto& mm : menus) if (mm.name == name) tr = mm.r;
        auto ids = std::make_shared<std::vector<juce::String>>(); for (auto& it : items) ids->push_back (it.id);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (tr.toNearestInt())).withMinimumWidth (240),
            [this, ids, safe = juce::Component::SafePointer<TopBars> (this)] (int r)
            {
                if (safe == nullptr) return;
                openMenu = {}; repaint();
                if (r > 0 && r <= (int) ids->size() && onButton) onButton ((*ids)[(size_t) (r - 1)]);
            });
    }

    void build()
    {
        btns.clear(); menus.clear(); const float w = (float) getWidth();
        auto add = [&] (const char* id, icons::Id ic, float x, float y, float ww, float hh, const char* label = "")
        { btns.push_back ({ id, ic, juce::String::fromUTF8 (label), { x, y - hh * .5f, ww, hh }, true }); };

        // ---- fila 1
        const float cy = kRow1 * .5f; float x = 232.f;
        for (auto [n, ww] : { std::pair<const char*, float> { "File", 46 }, { "Edit", 46 }, { "View", 50 }, { "Options", 68 }, { "Tools", 56 }, { "Help", 50 } })
        { menus.push_back ({ n, { x, cy - 17.f, ww, 34.f } }); x += ww + 2; }
        const float clusterW = 128 + 8 + 100 + 6 + 54 + 6 + 84 + 8 + 38 + 4 + 38;
        float cx = std::max (x + 20.f, (w - clusterW) * .5f);
        transportR = { cx, cy - 20.f, 128.f, 40.f };
        add ("play", icons::Play, cx + 2, cy, 40, 36); add ("stop", icons::Stop, cx + 44, cy, 40, 36); add ("rec", icons::Rec, cx + 86, cy, 40, 36);
        cx += 136; bpmR = { cx, cy - 20.f, 100.f, 40.f }; cx += 106; sigR = { cx, cy - 20.f, 54.f, 40.f }; cx += 60; posR = { cx, cy - 20.f, 84.f, 40.f }; cx += 92;
        add ("metro", icons::Metro, cx, cy, 38, 36); add ("loop", icons::Loop, cx + 42, cy, 38, 36);
        const float free0 = cx + 80.f + 16.f;   // fin de los botones de metronomo/bucle
        ramR = { w - 14.f - 124.f, 8.f, 124.f, 40.f }; cpuR = { ramR.getX() - 22.f - 70.f, 8.f, 70.f, 40.f };
        if (cpuR.getX() < free0) { cpuR = { w - 14.f - 70.f, 8.f, 70.f, 40.f }; ramR = {}; }   // poco ancho: solo CPU
        if (cpuR.getX() < free0) cpuR = {};

        // ---- fila 2
        const float y2 = kRow1 + kRow2 * .5f; x = 12;
        add ("layout", icons::Layout, x, y2, 40, 34); x += 50;
        add ("view:playlist", icons::Playlist, x, y2, 108, 34, "Playlist"); x += 114; add ("view:mixer", icons::Mixer, x, y2, 90, 34, "Mixer"); x += 96;
        add ("view:piano", icons::PianoRoll, x, y2, 116, 34, "Piano Roll"); x += 122; add ("view:steps", icons::Steps, x, y2, 154, 34, "Step Sequencer"); x += 164;
        divX = x; x += 14;
        if (! pianoMode) for (auto [id, ic] : { std::pair<const char*, icons::Id> { "tool:select", icons::Select }, { "tool:pencil", icons::Pencil }, { "tool:brush", icons::Brush }, { "tool:auto", icons::Curve }, { "tool:cut", icons::Scissors }, { "tool:trim", icons::Stretch } }) { add (id, ic, x, y2, 40, 34); x += 44; }
        add ("snap", icons::Magnet, x, y2, 40, 34); x += 50;
        quantR = {};
        float rx = w - 12.f - 3 * 44.f; add ("undo", icons::Undo, rx, y2, 40, 34); add ("redo", icons::Redo, rx + 44, y2, 40, 34); add ("plugins", icons::Plugins, rx + 88, y2, 40, 34);
    }
};
