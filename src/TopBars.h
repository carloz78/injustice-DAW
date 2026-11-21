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
    struct Btn { juce::String id; icons::Id icon; juce::String label; juce::Rectangle<float> r; bool enabled = true; };

    std::function<void (const juce::String&)> onButton;
    std::function<void (double)> onBpm;
    std::function<void (int, int)> onSig;
    bool metro = false, pianoMode = false, mixerOpen = false, stepsOpen = false, metersOpen = false, perfOpen = false, browserOpen = true, recArmed = false;
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
        g.setColour (barLine.withAlpha (.7f)); g.drawLine (0, (float) kRow1, (float) getWidth(), (float) kRow1);
        g.setColour (juce::Colours::black.withAlpha (.35f)); g.drawLine (0, (float) kHeight - .5f, (float) getWidth(), (float) kHeight - .5f);

        drawLogo (g);
        // menús
        g.setFont (juce::FontOptions (14.f));
        for (auto& m : menus)
        {
            const bool hot = openMenu == m.name || (hoverId == "menu:" + m.name);
            if (hot) { g.setColour (juce::Colours::white.withAlpha (openMenu == m.name ? .16f : .08f)); g.fillRoundedRectangle (m.r, 6.f); }
            g.setColour (text); g.drawText (m.name, m.r.toNearestInt(), juce::Justification::centred);
        }
        // grupo de transporte
        g.setColour (boxFill); g.fillRoundedRectangle (transportR, 9.f); g.setColour (boxLine); g.drawRoundedRectangle (transportR.reduced (.5f), 9.f, 1.f);
        for (auto& b : btns) if (isTransport (b.id)) paintBtn (g, b);
        // cajas de BPM, compás y posición
        auto box = [&] (juce::Rectangle<float> r, bool hot) { g.setColour (boxFill); g.fillRoundedRectangle (r, 9.f); g.setColour (hot ? accent : boxLine); g.drawRoundedRectangle (r.reduced (.5f), 9.f, 1.f); };
        if (pulse > 0.f && playing) { g.setColour (accent.withAlpha (pulse * .55f)); g.drawRoundedRectangle (bpmR.expanded (pulse * 3.f), 11.f, 1.5f + pulse * 1.5f); }
        box (bpmR, hoverId == "bpm" || dragBpm);
        if (pulse > 0.f && playing) { g.setColour (accent.withAlpha (pulse * .22f)); g.fillRoundedRectangle (bpmR.reduced (1.f), 9.f); } box (sigR, false); box (posR, false);
        g.setFont (juce::FontOptions (20.f, juce::Font::bold)); g.setColour (accentHi); g.drawText (juce::String (bpm, 2), bpmR.toNearestInt(), juce::Justification::centred);
        g.setColour (text); g.drawText (juce::String (timesig::num) + "/" + juce::String (timesig::den), sigR.toNearestInt(), juce::Justification::centred);
        g.drawText (posText(), posR.toNearestInt(), juce::Justification::centred);
        for (auto& b : btns) if (b.id == "metro" || b.id == "loop") paintBtn (g, b);
        drawMeters (g);

        // fila 2: vistas y herramientas
        for (auto& b : btns) if (! isTransport (b.id) && b.id != "metro" && b.id != "loop") paintBtn (g, b);
        g.setColour (barLine); g.drawLine (divX, (float) kRow1 + 11.f, divX, (float) kHeight - 11.f);
        if (pianoMode)
        {
            const bool hot = hoverId == "quant" || pressedId == "quant";
            g.setColour (hot ? panelHi : boxFill); g.fillRoundedRectangle (quantR, 8.f); g.setColour (boxLine); g.drawRoundedRectangle (quantR.reduced (.5f), 8.f, 1.f);
            g.setColour (text); g.setFont (juce::FontOptions (14.f)); g.drawText (quantLabel, quantR.withTrimmedRight (20).toNearestInt(), juce::Justification::centred);
            icons::draw (g, icons::Chevron, { quantR.getRight() - 28.f, quantR.getY(), 26.f, quantR.getHeight() }, textDim);
        }
    }

    //==============================================================================
    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto p = e.position; pressedId = {};
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
        if (b.id == "layout") return browserOpen;
        if (b.id.startsWith ("view:")) return b.id.substring (5) == view;
        return false;
    }

    void paintBtn (juce::Graphics& g, const Btn& b)
    {
        using namespace theme;
        const bool on = isOn (b), hot = hoverId == b.id && b.enabled, down = pressedId == b.id;
        auto r = b.r; const bool isRec = b.id == "rec", isPlay = b.id == "play";
        juce::Colour fill = juce::Colours::transparentBlack;
        if (isRec) { if (on) fill = record.withAlpha (down ? .42f : .30f); else if (down) fill = juce::Colours::white.withAlpha (.18f); else if (hot) fill = juce::Colours::white.withAlpha (.09f); }
        else if (on) fill = down ? accent.darker (.25f) : (hot ? accent.brighter (.12f) : accent);
        else if (down) fill = juce::Colours::white.withAlpha (.20f);
        else if (hoverAmt (b.id) > .01f && b.enabled) fill = juce::Colours::white.withAlpha (.10f * hoverAmt (b.id));
        if (! fill.isTransparent()) { g.setColour (fill); g.fillRoundedRectangle (r.reduced (1.f), 8.f); }
        if (isRec && on) { g.setColour (record.withAlpha (.9f)); g.drawRoundedRectangle (r.reduced (1.5f), 8.f, 1.5f); }

        auto col = (on && ! isRec) ? juce::Colours::white : iconIdle;
        if (isRec) col = on ? record.brighter (.25f) : record;
        if (isPlay && ! on) col = juce::Colour (0xffd5d3e0);
        if (! b.enabled) col = col.withAlpha (.3f);
        if (b.label.isEmpty()) { icons::draw (g, b.icon, r, col); return; }
        icons::draw (g, b.icon, { r.getX() + 6.f, r.getY(), 30.f, r.getHeight() }, col);
        g.setColour (col); g.setFont (juce::FontOptions (14.f)); g.drawText (b.label, r.withTrimmedLeft (36.f).toNearestInt(), juce::Justification::centredLeft);
    }

    void drawLogo (juce::Graphics& g)
    {
        using namespace theme; const float x0 = 16.f, y0 = 14.f, c = 8.f, gap = 2.f;
        static const juce::uint32 cols[9] { 0xff40403f, 0xff828282, 0xffbbbbba, 0xff676767, 0xfffefefe, 0xffa19fa0, 0xff1c1c1b, 0xff383939, 0xff7d7d7d };   // logo nuevo: 3x3 en grises, centro blanco
        g.setColour (juce::Colours::black); g.fillRoundedRectangle (x0 - 5.f, y0 - 5.f, 3 * c + 2 * gap + 10.f, 3 * c + 2 * gap + 10.f, 6.f);
        for (int i = 0; i < 9; ++i) { g.setColour (juce::Colour (cols[i])); g.fillRoundedRectangle (x0 + (i % 3) * (c + gap), y0 + (i / 3) * (c + gap), c, c, 2.6f); }
        juce::Font f1 (juce::FontOptions (19.f, juce::Font::bold)); juce::GlyphArrangement ga; ga.addLineOfText (f1, "INJUSTICE", 0, 0);
        const int w1 = (int) std::ceil (ga.getBoundingBox (0, -1, true).getWidth());
        g.setColour (text); g.setFont (f1); g.drawText ("INJUSTICE", 60, 10, w1 + 4, 36, juce::Justification::centredLeft);
        g.setColour (accentHi); g.setFont (juce::FontOptions (14.f)); g.drawText ("DAW", 60 + w1 + 8, 10, 44, 36, juce::Justification::centredLeft);
    }

    void drawMeters (juce::Graphics& g)
    {
        using namespace theme;
        auto one = [&] (juce::Rectangle<float> r, const char* name, juce::String val, juce::String extra, float f)
        {
            g.setColour (textDim); g.setFont (juce::FontOptions (13.f, juce::Font::bold)); g.drawText (name, (int) r.getX(), (int) r.getY() + 6, 36, 16, juce::Justification::centredLeft);
            g.setColour (text); g.setFont (juce::FontOptions (14.f, juce::Font::bold)); g.drawText (val, (int) r.getX() + 36, (int) r.getY() + 5, 52, 18, juce::Justification::centredLeft);
            if (extra.isNotEmpty()) { g.setColour (textDim); g.setFont (juce::FontOptions (12.f)); g.drawText (extra, (int) r.getX() + 76, (int) r.getY() + 6, 60, 16, juce::Justification::centredLeft); }
            auto bar = juce::Rectangle<float> (r.getX(), r.getY() + 28.f, r.getWidth(), 5.f);
            g.setColour (panelHi); g.fillRoundedRectangle (bar, 2.5f);
            g.setColour (f > .85f ? record : f > .6f ? pink : accent); g.fillRoundedRectangle (bar.withWidth (juce::jmax (4.f, bar.getWidth() * juce::jlimit (0.f, 1.f, f))), 2.5f);
        };
        one (cpuR, "CPU", juce::String (juce::roundToInt (cpu * 100.0)) + "%", {}, (float) cpu);
        const float f = ramTotalMB > 0 ? (float) (ramMB / ramTotalMB) : 0.f;
        one (ramR, "RAM", juce::String (ramMB / 1024.0, 1), juce::String ("GB / ") + juce::String (juce::roundToInt (ramTotalMB / 1024.0)) + " GB", f);
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
        if (name == "File") { I ("new", "Proyecto nuevo\tCtrl+N"); S(); for (int k = 0; k < ideas::kNumStyles; ++k) items.push_back ({ "tpl:" + juce::String (k), juce::String::fromUTF8 ("Plantilla: ") + ideas::styleName (k) + " (" + juce::String ((int) ideas::styleBpm (k)) + " BPM)", false, true }); S(); I ("open", "Abrir…\tCtrl+O"); I ("save", "Guardar\tCtrl+S"); I ("saveas", "Guardar como…\tCtrl+Mayús+S"); I ("savesamples", "Guardar con los samples…"); S(); I ("importmidi", "Importar MIDI…"); I ("export", "Exportar audio…\tCtrl+E"); S(); I ("quit", "Salir"); }
        else if (name == "Edit") { I ("undo", "Deshacer\tCtrl+Z"); I ("redo", "Rehacer\tCtrl+Y"); S(); I ("dup", "Duplicar clip\tCtrl+D"); I ("del", "Eliminar clip\tSupr"); }
        else if (name == "View") { I ("view:playlist", "Playlist\tF5", view == "playlist" && ! pianoMode); I ("view:mixer", "Mixer\tF9", mixerOpen); I ("view:piano", "Piano Roll\tF7", pianoMode); I ("view:steps", "Step Sequencer\tF6", stepsOpen); I ("view:meters", "Medidores\tF11", metersOpen); I ("view:perf", "Modo ensayo\tF12", perfOpen); S(); I ("layout", "Explorador de samples\tF8", browserOpen); }
        else if (name == "Options") { I ("audio", "Ajustes de audio…"); S(); I ("metro", "Metrónomo", metro); I ("loop", "Repetir en bucle", looping); I ("snap", "Ajustar a la cuadrícula", snap); }
        else if (name == "Tools") { I ("plugins", "Lista de plugins VST3…\tF10"); I ("ideas", "Generador de ideas…\tCtrl+G"); S(); I ("tool:select", "Herramienta: seleccionar", tool == "select"); I ("tool:pencil", "Herramienta: lápiz", tool == "pencil"); I ("tool:brush", "Herramienta: pincel", tool == "brush"); I ("tool:auto", "Herramienta: automatización", tool == "auto"); I ("tool:cut", "Herramienta: cortar", tool == "cut"); I ("tool:trim", "Herramienta: recortar", tool == "trim"); }
        else if (name == "Help") { I ("help", "Ayuda y atajos\tF1"); I ("about", "Acerca de INJUSTICE DAW"); }
        juce::PopupMenu m; int n = 1;
        for (auto& it : items) { if (it.id.isEmpty()) m.addSeparator(); else m.addItem (n, it.label, it.en, it.tick); ++n; }
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
        ramR = { w - 14.f - 124.f, 8.f, 124.f, 40.f }; cpuR = { ramR.getX() - 22.f - 70.f, 8.f, 70.f, 40.f };

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
