#pragma once
#include "Icons.h"
#include <functional>
#include <map>

// Barra superior (transporte) + barra de herramientas. Un solo componente con lista de zonas pulsables.
class TopBars : public juce::Component, public juce::TooltipClient
{
public:
    struct Btn { juce::String id; icons::Id icon; juce::Rectangle<float> r; bool enabled = true, active = false, primary = false; };

    std::function<void (const juce::String&)> onButton;
    std::function<void (double)> onBpm;
    bool metro = false; bool pianoMode = false, mixerOpen = false, stepsOpen = false, browserOpen = true; juce::String quantLabel = "1/16"; double bpm = 172.0; bool playing = false, looping = false, snap = true; juce::String tool = "select", view = "playlist";
    juce::String crumb = "Playlist - Arrangement";

    static constexpr int kRow1 = 44, kRow2 = 42, kHeight = kRow1 + kRow2;

    void resized() override { build(); repaint(); }

    void paint (juce::Graphics& g) override
    {
        using namespace theme;
        g.setColour (bgBar);  g.fillRect (0, 0, getWidth(), kRow1);
        g.setColour (bgBar2); g.fillRect (0, kRow1, getWidth(), kRow2);
        for (auto& b : btns)
        {
            const bool on = isOn (b);
            auto r = b.r;
            const bool pill = b.primary || on;
            if (pill) { g.setColour (on || b.primary ? accent : accent.withAlpha (.4f)); g.fillRoundedRectangle (r.reduced (0, 1), 5.f); }
            auto col = pill ? juce::Colours::white : iconIdle;
            if (! b.enabled) col = col.withAlpha (.28f);
            icons::draw (g, b.icon, r, col);
        }
        // BPM / compás / tonalidad
        g.setFont (juce::FontOptions (19.f, juce::Font::bold));
        g.setColour (accentHi); g.drawText (juce::String (bpm, 2), bpmR.withTrimmedRight (42).toNearestInt(), juce::Justification::centredRight);
        g.setFont (juce::FontOptions (13.f, juce::Font::bold)); g.setColour (text);
        g.drawText ("BPM", bpmR.withLeft (bpmR.getRight() - 40).toNearestInt(), juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (18.f, juce::Font::bold)); g.drawText ("4/4", sigR.toNearestInt(), juce::Justification::centred);
        g.setColour (textDim); g.drawText ("F#m", keyR.toNearestInt(), juce::Justification::centred);
        g.setColour (text); g.setFont (juce::FontOptions (16.f));
        g.drawText (crumb + "  >", crumbR.toNearestInt(), juce::Justification::centredLeft);
        if (pianoMode) { g.setColour (juce::Colour (0xff1c1a22)); g.fillRoundedRectangle (quantR, 6.f); g.setColour (text); g.setFont (juce::FontOptions (16.f, juce::Font::bold)); g.drawText (quantLabel + "  v", quantR.toNearestInt(), juce::Justification::centred); }
        g.setColour (juce::Colours::white.withAlpha (.06f)); g.drawLine (0, (float) kRow1, (float) getWidth(), (float) kRow1);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (pianoMode && quantR.contains (e.position)) { if (onButton) onButton ("quant"); return; }
        for (auto& b : btns) if (b.enabled && b.r.contains (e.position)) { if (onButton) onButton (b.id); return; }
        dragBpm = bpmR.contains (e.position); lastY = e.position.y;
    }
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
        if (bpmR.contains (p)) return juce::String::fromUTF8 ("Tempo: arrastra o usa la rueda (Mayús = fino)");
        return {};
    }
    static juce::String tip (const juce::String& id)
    {
        static const std::map<juce::String, const char*> t {
            { "new", "Proyecto nuevo (Ctrl+N)" }, { "open", "Abrir proyecto (Ctrl+O)" }, { "layout", "Mostrar/ocultar el explorador (F8)" },
            { "undo", "Deshacer (Ctrl+Z)" }, { "redo", "Rehacer (Ctrl+Y)" }, { "play", "Reproducir / pausa (Espacio)" }, { "stop", "Detener y volver al inicio" },
            { "rec", "Grabar (pr\xc3\xb3ximamente)" }, { "loop", "Repetir en bucle" }, { "audio", "Ajustes de audio" }, { "plugins", "Lista de plugins VST3 (F10)" },
            { "export", "Exportar a WAV (Ctrl+E)" }, { "help", "Ayuda y atajos (F1)" }, { "tool:select", "Seleccionar" }, { "tool:pencil", "L\xc3\xa1piz: dibujar" },
            { "tool:cut", "Cortar clips" }, { "tool:trim", "Recortar el borde de un clip" }, { "tool:erase", "Borrador" }, { "metro", "Metr\xc3\xb3nomo" },
            { "snap", "Ajustar a la cuadr\xc3\xad" "cula" }, { "view:playlist", "Playlist (F5)" }, { "view:mixer", "Mixer (F9)" }, { "view:piano", "Piano Roll (F7)" }, { "view:steps", "Step Sequencer (F6)" } };
        auto it = t.find (id); return it == t.end() ? juce::String() : juce::String::fromUTF8 (it->second);
    }

private:
    std::vector<Btn> btns; juce::Rectangle<float> bpmR, sigR, keyR, crumbR, quantR; bool dragBpm = false; float lastY = 0;

    bool isOn (const Btn& b) const
    {
        if (b.id == "play") return playing;
        if (b.id == "loop") return looping;
        if (b.id == "snap") return snap;
        if (b.id == "metro") return metro;
        if (b.id.startsWith ("tool:")) return b.id.substring (5) == tool;
        if (b.id == "view:mixer") return mixerOpen;
        if (b.id == "view:steps") return stepsOpen;
        if (b.id == "layout") return browserOpen;
        if (b.id.startsWith ("view:")) return b.id.substring (5) == view;
        return false;
    }

    void build()
    {
        btns.clear(); const float w = (float) getWidth(), cy = kRow1 * .5f, sz = 40.f;
        auto add = [&] (const char* id, icons::Id ic, float x, float y, bool en = true, bool pr = false, float ww = 40.f) { btns.push_back ({ id, ic, { x, y - 19.f, ww, 38.f }, en, false, pr }); };
        float x = 8;
        for (auto [id, ic, en] : { std::tuple<const char*, icons::Id, bool> { "new", icons::NewFile, true }, { "open", icons::OpenFile, true }, { "layout", icons::Layout, true } }) { add (id, ic, x, cy, en); x += 46; }
        x += 14; add ("undo", icons::Undo, x, cy); x += 44; add ("redo", icons::Redo, x, cy);
        // transporte centrado
        const float cx = w * .5f - 110; add ("play", icons::Play, cx, cy, true, true, 70); add ("stop", icons::Stop, cx + 80, cy); add ("rec", icons::Rec, cx + 126, cy); add ("loop", icons::Loop, cx + 172, cy);
        bpmR = { cx + 235, 4, 150, (float) kRow1 - 8 }; sigR = { bpmR.getRight() + 8, 4, 62, (float) kRow1 - 8 }; keyR = { sigR.getRight() + 8, 4, 74, (float) kRow1 - 8 };
        // 4 iconos de la derecha: ajustes de audio, plugins, exportar, ayuda
        float rx = w - 8 - 4 * 46 + 4;
        add ("audio", icons::Audio, rx, cy); add ("plugins", icons::Plugins, rx + 46, cy); add ("export", icons::Export, rx + 92, cy); add ("help", icons::Help, rx + 138, cy);
        // fila 2: herramientas
        const float y2 = kRow1 + kRow2 * .5f; x = 8;
        if (pianoMode)
        { add ("tool:select", icons::Select, x, y2, true, false, 52); x += 56; add ("tool:pencil", icons::Pencil, x, y2, true, false, 52); x += 56; add ("tool:erase", icons::Eraser, x, y2, true, false, 52); x += 70; }
        else
        {
            for (auto [id, ic] : { std::pair<const char*, icons::Id> { "tool:select", icons::Select }, { "tool:pencil", icons::Pencil }, { "tool:cut", icons::Scissors }, { "tool:trim", icons::Stretch } })
            { add (id, ic, x, y2, true, false, 52); x += 56; }
            add ("metro", icons::Speaker, x, y2, true, false, 52); x += 70;
        }
        add ("snap", icons::Magnet, x, y2, true, false, 52); crumbR = { x + 66, (float) kRow1, 400, (float) kRow2 };
        // navegación de vistas, a la derecha de la fila 2
        const float rightEdge = pianoMode ? w - 8 - 110 : w - 8; quantR = { w - 8 - 100, (float) kRow1 + 3, 100, (float) kRow2 - 6 };
        const float vx = rightEdge - 4 * 52; add ("view:playlist", icons::Playlist, vx, y2, true, false, 48); add ("view:mixer", icons::Mixer, vx + 52, y2, true, false, 48); add ("view:piano", icons::PianoRoll, vx + 104, y2, true, false, 48); add ("view:steps", icons::Steps, vx + 156, y2, true, false, 48);
    }
};
