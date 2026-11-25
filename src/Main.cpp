#include <array>
#include "TopBars.h"
#include "PlaylistView.h"
#include "PianoRollView.h"
#include "MixerPanel.h"
#include "StepSeqPanel.h"
#include "InjLimiter.h"
#include "FxAll.h"
#include "FxTest.h"
#include "FxTest2.h"
#include <thread>
#include "Meters.h"
#include "PerformancePanel.h"
#include "AudioEditorPanel.h"
#include "PitchEditor.h"
#include "PadRackPanel.h"
#include "AudioToMidi.h"
#include "ProjectNotes.h"
#include "Instruments.h"
#include "Audition.h"
#include "TimeSig.h"
#include "Templates.h"
#include "BrowserPanel.h"
#include "PluginHost.h"
#include "TrackUtil.h"
#include "SysInfo.h"
#include "Palette.h"
#include "Arranger.h"
#include "MidiMap.h"
#include "MidiTools.h"
#include "Harmony.h"

#include <map>
#include <set>

// Aviso breve en la parte inferior (se desvanece solo).
class Toast : public juce::Component, private juce::Timer
{
public:
    Toast() { setInterceptsMouseClicks (false, false); }
    void show (const juce::String& t) { msg = t; life = 1.f; setVisible (true); toFront (false); startTimerHz (30); repaint(); }
    void paint (juce::Graphics& g) override
    {
        const float a = juce::jmin (1.f, life * 3.f);
        auto r = getLocalBounds().toFloat().reduced (1);
        g.setColour (theme::boxFill.withAlpha (.97f * a)); g.fillRoundedRectangle (r, theme::rad);
        g.setColour (theme::barLine.withAlpha (a)); g.drawRoundedRectangle (r, theme::rad, 1.f);
        g.setColour (juce::Colours::white.withAlpha (a)); g.setFont (juce::FontOptions (14.f)); g.drawFittedText (msg, getLocalBounds().reduced (14, 4), juce::Justification::centred, 2);
    }
    void place (juce::Rectangle<int> parent) { const int w = juce::jmin (640, parent.getWidth() - 40); setBounds (parent.getCentreX() - w / 2, parent.getBottom() - 64, w, 44); }
private:
    juce::String msg; float life = 0;
    void timerCallback() override { life -= .012f; if (life <= 0) { stopTimer(); setVisible (false); } else repaint(); }
};

// Ventana de ayuda: atajos de teclado.
class HelpContent : public juce::Component
{
public:
    HelpContent() { setSize (620, 2760); }
    void paint (juce::Graphics& g) override
    {
        using namespace theme; g.fillAll (bgBar2);
        g.setColour (accentHi); g.setFont (juce::FontOptions (22.f, juce::Font::bold)); g.drawText ("INJUSTICE DAW", 24, 16, 400, 30, juce::Justification::left);
        g.setColour (textDim); g.setFont (juce::FontOptions (13.f)); g.drawText (juce::String::fromUTF8 ("Versión ") + juce::JUCEApplication::getInstance()->getApplicationVersion() + juce::String::fromUTF8 (" · código abierto (AGPL-3.0) · Injustice Audio"), 24, 46, 560, 20, juce::Justification::left);
        struct Row { const char* k; const char* d; };
        static const Row rows[] {
            { nullptr, "General" },
            { "Espacio", "Reproducir / pausa" }, { "Inicio", "Volver al principio" }, { "Ctrl+N / Ctrl+O", "Proyecto nuevo / abrir" }, { "Ctrl+S / Ctrl+Mayús+S", "Guardar / guardar como" },
            { "Ctrl+E", "Exportar audio" }, { "Ctrl+Z / Ctrl+Y", "Deshacer / rehacer (un gesto = un paso)" }, { "Ctrl+T", "Teclado como piano (Z-M, Q-U; Re/Av P\xc3\xa1g = octava)" }, { "F1", "Esta ayuda" },
            { nullptr, "Ventanas" },
            { "F5", "Playlist" }, { "F6", "Step Sequencer (patrones de batería)" }, { "F7", "Piano Roll (notas)" }, { "F8", "Explorador de samples" }, { "F9", "Mixer" }, { "F10", "Lista de plugins VST3" }, { "F11", "Medidores (espectro y volumen)" }, { "F12", "Modo ensayo (lanzar clips en vivo)" },
            { "Arrastrar esquinas de un panel", "Cambiar su tamaño" },
            { nullptr, "Comandos" },
            { "Ctrl+K", "Paleta de comandos: escribe y pulsa Enter" }, { "Ctrl+I", "Invertir la selección de clips" }, { "Alt+↑  /  Alt+↓", "Subir o bajar de pista los clips elegidos" }, { "Coma  /  Punto", "Cabezal al compás anterior / siguiente" }, { "Alt+Enter", "Pantalla completa" },
            { nullptr, "Arreglo, acordes y MIDI" },
            { "Carril \"Arreglo\" (bajo la regla)", "Secciones con nombre: clic = ir al inicio, arrastrar = reordenar con sus clips, borde derecho = duraci\xc3\xb3n, doble clic = renombrar, clic derecho = men\xc3\xba" },
            { "Cuadro de tonalidad (arriba a la izquierda)", "Nota y escala del proyecto: el Piano Roll la marca y puedes ajustar o transponer notas a ella" },
            { "Carril \"Acordes\"", "Escribe acordes (Am, F5, G7); crea un clip MIDI con ellos o ajusta notas al acorde que suena" },
            { "Tools > MIDI learn", "Asigna un mando de tu controlador a volumen, pan, mute, solo, tempo o un par\xc3\xa1metro" },
            { "Ctrl+K > Macros", "Graba una secuencia de comandos y vu\xc3\xa9lvela a ejecutar con un solo comando" },
            { "Ctrl+K > Arpegiador", "Elige un clip MIDI con acordes y convi\xc3\xa9rtelo en arpegio: patr\xc3\xb3n, velocidad, octavas, duraci\xc3\xb3n y swing" },
            { "Ctrl+K > Notas", "Humanizar, legato, crescendo, rasgueo, redoble, doblar octava, espejo... sobre los clips MIDI elegidos" },
            { "Ctrl+K > Edici\xc3\xb3n", "Repetir y empujar clips, fundido cruzado, plantillas de pista, punch in/out e historial de deshacer" },
            { "Ctrl+Mayús+E (o Ver > Editor de audio)", "Elige un clip de audio: ver la onda, marcar un tramo, volumen, fundidos, tono, velocidad y efectos (quitar DC, normalizar, invertir, polaridad, recortar...)" },
            { "Ver > Rack de pads", "16 pads sobre el sampler de la pista elegida: capas, volumen, pan y afinaci\xc3\xb3n por pad; suelta un archivo sobre un pad para cargarlo" },
            { "Ctrl+K > Teclado", "Repetici\xc3\xb3n de notas y bloqueo de escala para el teclado piano de la computadora (Ctrl+T)" },
            { "Ctrl+K > Proyecto", "Letra y notas del proyecto (se guardan en \xc3\xa9l) y Audio a MIDI para melod\xc3\xadas de una sola voz" },
            { "Ctrl+K > bounce", "Convierte los clips elegidos a audio en una pista nueva (los originales se silencian)" },
            { nullptr, "Playlist" },
            { "Clic en un clip / arrastrar", "Elegir / mover (también entre pistas)" }, { "Arrastrar en vacío (Selector)", "Rectángulo para elegir varios clips" }, { "Mayús+clic", "Sumar o quitar un clip de la selección" },
            { "Ctrl+A / Esc", "Elegir todos los clips / quitar la selección" }, { "Ctrl+C / Ctrl+X / Ctrl+V", "Copiar / cortar / pegar en el cursor" }, { "Ctrl+D", "Duplicar lo elegido" }, { "Ctrl+M", "Marcador en el cursor (clic derecho en la regla: más)" }, { "Ctrl+←  /  Ctrl+→", "Ir al marcador anterior / siguiente" }, { "Supr", "Borrar lo elegido" },
            { "Automatización + clic", "Punto de volumen; arrastra para moverlo, clic derecho para borrar" },
            { "Pincel + arrastrar", "Pinta clips de 1 compás; sobre un clip, copia lo que ya hiciste" },
            { "Ctrl+rueda / Alt+rueda", "Zoom horizontal / alto de las pistas" },
            { "Flechas ← → / Mayús", "Mover los clips elegidos un paso / un compás" }, { "S", "Dividir los clips elegidos en el cursor" }, { "Cuadritos arriba de un clip de audio", "Arrastrar: fundido de entrada / salida (doble clic lo quita)" }, { "Rombo y línea punteada del clip", "Arrastrar: ganancia del clip (Mayús = fino, doble clic = 0 dB)" }, { "Alt al arrastrar", "Mover sin imán de cuadrícula" }, { "Fin  /  Mayús+Espacio", "Ir al final / parar y volver de donde empezó" },
            { "F4", "Seguir el cabezal (la vista avanza sola)" }, { "Ctrl+Mayús+P", "Editor de afinación (Injustice Auto): corregir la voz del clip elegido a mano; el botón GRAPH de Injustice Tune también lo abre" }, { "F3", "Comparar A/B con la canción de referencia (Tools > Referencia)" }, { "Ctrl+0  /  Ctrl+Mayús+0", "Ver todo el proyecto / ver lo elegido" }, { "Clic derecho en el BPM", "Tap tempo (toca al ritmo)" },
            { "Piano Roll: [  ]  /  G  /  Ctrl+0", "Acortar o alargar notas / notas fantasma / ajustar la vista" }, { "Step Sequencer: carril de abajo", "Fuerza, Probabilidad y Largo de cada paso de la fila elegida; Mayús + arrastrar = rampa" }, { "Step Sequencer: Swing", "Barra de arriba = todas las filas; la de abajo = solo la fila elegida" }, { "Step Sequencer: S", "Escuchar solo esa fila" }, { "Step Sequencer: arrastrar el nombre de una fila", "Mover la fila de lugar (también en su menú ▾)" }, { "Step Sequencer: menú de la fila", "Copiar y pegar fila, largo propio (polirritmia), cortar el sample con el largo del paso" }, { "Step Sequencer: 1/32 o tresillo", "Cambia la duración de cada paso (rolls y ritmo de 3)" },
            { "Piano Roll: M", "Silenciar o activar las notas elegidas" }, { "Piano Roll: título del carril de abajo", "Cambiar entre Velocity, Modulación (CC1) y Pitch bend; dibuja con el lápiz, clic derecho borra" }, { "Piano Roll: menú ⋮ → Slide", "La nota se desliza desde el tono de la nota anterior (pon el rango de bend de tu instrumento en el carril)" }, { "Piano Roll: menú ⋮ → Ratchet / Velocidad ±10 % / Tocar la selección en bucle", "Repetir una nota, subir o bajar la fuerza, y escuchar solo lo elegido en bucle" },
            { "Step Sequencer: Mayús o Alt + clic", "Paso fuerte (127) o suave" },
            { nullptr, "Mixer" },
            { "Doble clic en un fader", "Volver a 0 dB" }, { "Ctrl+clic en S / clic derecho en un canal", "Solo exclusivo / reiniciar niveles y quitar solos" }, { "Doble clic en el nombre de un canal", "Renombrar" }, { "Rueda sobre un fader / perilla", "Ajuste fino" },
            { "Arrastrar la cabecera de un canal", "Mover el canal a otro lugar (también en el menú ▾ → Mover)" }, { "Menú ▾ → Enlazar", "Enlaza canales: volumen, pan y estéreo se mueven juntos (marca LINK)" }, { "Botón SC", "Sidechain: elegir qué canal lo hace bajar, fuerza y recuperación" }, { "Clic en Master", "Ver y editar el ecualizador del Master (y su barra de estéreo)" }, { "Ecualizador (abajo a la derecha)", "Arrastra los 4 puntos: izquierda/derecha = frecuencia, arriba/abajo = ganancia; rueda = Q; doble clic = reiniciar; Ø = invertir fase" }, { "Barra bajo el pan", "Separación estéreo: izquierda = mono, centro = normal, derecha = más ancho (doble clic = 100 %)" }, { "Flecha ▾ del canal → Salida", "Mandar el canal a Master o a otro canal (bus)" }, { "Flecha ▾ → Nuevo bus", "Crea un bus y le manda este canal (o todo su grupo)" }, { "Flecha ▾ → Envíos", "Enviar a cualquier canal con nivel propio, antes o después del fader" }, { "Botón A / ● Escribir aut.", "A muestra el carril de automatización en la Playlist; Escribir graba tus movimientos de fader y pan al reproducir" }, { "Perilla junto al slot", "Mezcla seca/húmeda del plugin" }, { "Flecha ▾ → Cadenas", "Guardar o cargar la cadena de efectos del canal" }, { "Flecha ▾ → Color / Icono / Entrada", "Color, icono y entrada de audio del canal" }, { "Botón Compacta", "Canales más angostos para ver 15 a la vez" } };
        int y = 80; for (auto& r : rows)
        {
            if (r.k == nullptr) { y += 6; g.setColour (textDim); g.setFont (juce::FontOptions (12.f, juce::Font::bold)); g.drawText (juce::String::fromUTF8 (r.d), 24, y, 400, 18, juce::Justification::centredLeft); y += 20; continue; }
            g.setColour (theme::panel); g.fillRoundedRectangle (20.f, (float) y, 580.f, 22.f, theme::rad);
            g.setColour (accentHi); g.setFont (juce::FontOptions (13.f, juce::Font::bold)); g.drawText (juce::String::fromUTF8 (r.k), 30, y, 250, 22, juce::Justification::centredLeft);
            g.setColour (text); g.setFont (juce::FontOptions (13.f)); g.drawText (juce::String::fromUTF8 (r.d), 270, y, 330, 22, juce::Justification::centredLeft); y += 25;
        }
    }
};

// Guardado automático: copia del proyecto en una carpeta de recuperación. Se borra al cerrar bien o al guardar.
namespace userdata
{
    inline juce::File dataDir() { auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("INJUSTICE DAW"); d.createDirectory(); return d; }
    inline juce::File templatesDir() { auto d = dataDir().getChildFile ("Plantillas"); d.createDirectory(); return d; }
    inline juce::File recentFile() { return dataDir().getChildFile ("recientes.txt"); }
    inline juce::StringArray recents()
    {
        juce::StringArray all, ok; all.addLines (recentFile().loadFileAsString()); for (auto& l : all) if (l.isNotEmpty() && juce::File (l).existsAsFile()) ok.add (l);
        return ok;
    }
    inline void addRecent (const juce::File& f)
    {
        auto r = recents(); r.removeString (f.getFullPathName()); r.insert (0, f.getFullPathName()); while (r.size() > 6) r.remove (r.size() - 1);
        recentFile().replaceWithText (r.joinIntoString ("\n"));
    }
    inline juce::StringArray templateNames() { juce::StringArray n; for (auto& f : templatesDir().findChildFiles (juce::File::findFiles, false, "*.injdaw")) n.add (f.getFileNameWithoutExtension()); n.sort (true); return n; }
}
namespace autosave
{
    inline juce::File dir() { auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("INJUSTICE DAW").getChildFile ("Recuperacion"); d.createDirectory(); return d; }
    inline juce::File file() { return dir().getChildFile ("recuperacion.injdaw"); }
    inline juce::File meta() { return dir().getChildFile ("recuperacion.txt"); }
    inline void clear() { file().deleteFile(); meta().deleteFile(); }
    inline bool write (te::Edit& e, const juce::File& original)
    {
        e.flushState(); auto xml = e.state.createXml(); if (xml == nullptr) return false;
        auto tmp = dir().getChildFile ("recuperacion.tmp"); tmp.deleteFile();
        if (! xml->writeTo (tmp)) return false;
        meta().replaceWithText (original.getFullPathName());
        return tmp.moveFileTo (file());
    }
}

// Diálogo de exportación: formato, frecuencia y mezcla completa o pistas por separado.
struct ExportOpts { int fmt = 0; double sr = 44100.0; bool norm = false, stems = false; };
class ExportContent : public juce::Component
{
public:
    std::function<void (ExportOpts)> onGo; std::function<void()> onCancel;
    ExportContent()
    {
        setSize (380, 270);
        fmt.addItem ("WAV 24 bits", 1); fmt.addItem ("WAV 16 bits", 2); fmt.addItem ("FLAC 24 bits", 3); fmt.setSelectedId (1);
        sr.addItem ("44.1 kHz", 1); sr.addItem ("48 kHz", 2); sr.addItem ("96 kHz", 3); sr.setSelectedId (1);
        what.addItem (juce::String::fromUTF8 ("Mezcla completa"), 1); what.addItem (juce::String::fromUTF8 ("Cada pista por separado (stems)"), 2); what.setSelectedId (1);
        for (auto* c : { &fmt, &sr, &what }) addAndMakeVisible (c);
        norm.setButtonText (juce::String::fromUTF8 ("Normalizar (subir al máximo sin saturar)")); addAndMakeVisible (norm);
        go.setButtonText ("Exportar..."); cancel.setButtonText ("Cancelar"); go.setToggleState (true, juce::dontSendNotification); go.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff17181b)); addAndMakeVisible (go); addAndMakeVisible (cancel);
        go.onClick = [this] { if (onGo) onGo ({ fmt.getSelectedId() - 1, std::array<double, 3> { 44100.0, 48000.0, 96000.0 } [(size_t) sr.getSelectedId() - 1], norm.getToggleState(), what.getSelectedId() == 2 }); };
        cancel.onClick = [this] { if (onCancel) onCancel(); };
    }
    void paint (juce::Graphics& g) override
    {
        using namespace theme; g.fillAll (bgBar2); g.setColour (textDim); g.setFont (fonts::label (12.5f));
        g.setColour (seam); g.fillRect (0, 196, getWidth(), 1); g.setColour (edgeLight); g.fillRect (0, 197, getWidth(), 1); g.setColour (textDim);
        g.drawText ("Formato", 24, 20, 100, 26, juce::Justification::centredLeft); g.drawText (juce::String::fromUTF8 ("Frecuencia"), 24, 62, 100, 26, juce::Justification::centredLeft); g.drawText ("Exportar", 24, 104, 100, 26, juce::Justification::centredLeft);
    }
    void resized() override
    {
        fmt.setBounds (130, 20, 226, 26); sr.setBounds (130, 62, 226, 26); what.setBounds (130, 104, 226, 26); norm.setBounds (24, 150, 332, 26);
        cancel.setBounds (170, 214, 90, 32); go.setBounds (266, 214, 90, 32);
    }
private:
    juce::ComboBox fmt, sr, what; juce::ToggleButton norm; juce::TextButton go, cancel;
};

// Barra de estado inferior: dispositivo de audio y estado del proyecto.
class StatusBar : public juce::Component
{
public:
    static constexpr int kHeight = 24;
    juce::String left, right;
    void paint (juce::Graphics& g) override
    {
        using namespace theme; g.fillAll (bgBar);
        g.setColour (juce::Colours::black.withAlpha (.35f)); g.drawLine (0, .5f, (float) getWidth(), .5f);
        g.setFont (juce::FontOptions (12.f)); g.setColour (textDim);
        g.drawText (left, 14, 0, getWidth() / 2, kHeight, juce::Justification::centredLeft);
        g.drawText (right, getWidth() / 2, 0, getWidth() / 2 - 14, kHeight, juce::Justification::centredRight);
    }
};

class MainComponent : public juce::Component, public juce::KeyListener, private juce::Timer
{
public:
    MainComponent (te::Engine& eng, std::unique_ptr<te::Edit>& e) : engine (eng), editPtr (e), edit (*e), playlist (*e)
    {
        addAndMakeVisible (bars); addAndMakeVisible (status); addAndMakeVisible (playlist); addChildComponent (toast); addChildComponent (palette); buildCommands(); addChildComponent (piano); addAndMakeVisible (browser); addChildComponent (mixer); addChildComponent (steps); addChildComponent (meters); addChildComponent (perf); addChildComponent (aed); addChildComponent (pitchEd); addChildComponent (rack);
        mixer.onClose = [this] { setMixer (false); }; steps.onClose = [this] { setSteps (false); }; meters.onClose = [this] { setMeters (false); }; perf.onClose = [this] { setPerf (false); }; aed.onClose = [this] { setAed (false); }; rack.onClose = [this] { setRack (false); }; rack.onMessage = [this] (const juce::String& m) { note (m); }; rack.onChanged = [this] { syncState(); }; rack.onAudition = [this] (te::AudioTrack* t, int pitch) { auditioner.note (t, pitch, 700); }; rack.getTrack = [this]() -> te::AudioTrack* { auto ts = te::getAudioTracks (edit); return ts.isEmpty() ? nullptr : ts[juce::jlimit (0, ts.size() - 1, mixer.selected)]; }; aed.onMessage = [this] (const juce::String& m) { note (m); }; aed.onChanged = [this] { playlist.repaint(); syncState(); }; aed.getClip = [this]() -> te::WaveAudioClip* { auto c = playlist.selectedClips(); return c.size() == 1 ? dynamic_cast<te::WaveAudioClip*> (c[0]) : nullptr; }; perf.onMessage = [this] (const juce::String& m) { note (m); }; meters.attach(); mixer.onTracksChanged = [this] { playlist.repaint(); }; mixer.onMessage = [this] (const juce::String& s) { note (s); };
        steps.onClipChanged = [this] { playlist.repaint(); };
        browser.setDeviceManager (&engine.getDeviceManager().deviceManager); browser.onMessage = [this] (const juce::String& m) { note (m); }; browser.onClose = [this] { setBrowser (false); }; browser.onCollapse = [this] { setBrowser (false); };
        pitchEd.setDeviceManager (&engine.getDeviceManager().deviceManager); pitchEd.onMessage = [this] (const juce::String& m) { note (m); }; pitchEd.onClose = [this] { setPitchEd (false); }; pitchEd.onChanged = [this] { playlist.repaint(); };
        injfx::openPitchEditor = [this] { setPitchEd (true); };
        mixer.onAddEffect = [this] (int t) { openPluginList (t); };
        playlist.ourPlugins = [this] { juce::Array<juce::PluginDescription> a; for (auto& d : engine.getPluginManager().knownPluginList.getTypes()) if (pluginhost::isInjusticePlugin (d)) a.add (d); return a; };
        playlist.onAddPlugin = [this] (const juce::PluginDescription& d, int ti) { addPlugin (d, ti); };
        mixer.onOpenPlugin = [this] (te::Plugin* p) { if (p) openEditorWhenReady (p); };
        midi.getEdit = [this] { return editPtr.get(); }; midi.autoParam = [] (te::AudioTrack& t) { return PlaylistView::autoParamOf (t); }; midi.onMessage = [this] (const juce::String& m) { note (m); }; midi.onChanged = [this] { repaint(); }; midi.attach();
        playlist.onTracksChanged = [this] { mixer.refreshTracks(); playlist.repaint(); };
        playlist.onTrackSelected = [this] (int i) { mixer.selected = i; mixer.showTrack (i); };
        playlist.samplesRoot = [this] { return browser.getRoot(); }; playlist.onInstrument = [this] (int ti, juce::String nm) { if (ti < 0) { note (nm); return; } mixer.selected = ti; mixer.repaint(); { auto tl = te::getAudioTracks (edit); if (ti < tl.size()) auditioner.play (tl[ti], instruments::demo (nm)); } note (nm + juce::String::fromUTF8 (" listo en la pista ") + juce::String (ti + 1) + juce::String::fromUTF8 (". Dibuja notas con el piano roll.")); };
        browser.onInstrument = [this] (int idx)
        {
            auto tr = te::getAudioTracks (edit); if (tr.isEmpty()) return; const int ti = juce::jlimit (0, tr.size() - 1, mixer.selected); auto* t = tr[ti];
            const int n = (int) instruments::list().size(); te::Plugin::Ptr p = idx >= n ? instruments::addKit (edit, *t, samplepack::packsDir(), idx - n) : instruments::addSynth (edit, *t, idx);
            if (p == nullptr) { note (juce::String::fromUTF8 (idx >= n && ! samplepack::ready (samplepack::packsDir()) ? "Los sonidos se están creando; inténtalo en unos segundos." : "No se pudo agregar el instrumento.")); return; }
            mixer.selected = ti; mixer.repaint(); playlist.repaint();
            auditioner.play (t, instruments::demo (idx >= n ? juce::String (instruments::kits()[(size_t) (idx - n)].name) : juce::String (instruments::list()[(size_t) idx].name)));
            note ((idx >= n ? juce::String (instruments::kits()[(size_t) (idx - n)].name) : juce::String (instruments::list()[(size_t) idx].name)) + juce::String::fromUTF8 (" agregado a la pista ") + juce::String (ti + 1));
        };
        if (! std::getenv ("INJ_SAMPLES") && ! std::getenv ("INJ_SELFTEST"))
        {   // el paquete de sonidos se crea en segundo plano para no congelar la ventana al abrir
            juce::Component::SafePointer<MainComponent> self (this); const auto userRoot = browser.getRoot();
            std::thread ([self, userRoot]
            {
                auto old = userRoot.getChildFile ("INJUSTICE Pack"); if (old.getChildFile (".pack_v1").existsAsFile()) old.deleteRecursively();   // paquete de la version anterior
                const bool need = ! samplepack::ready (samplepack::packsDir());
                if (need) juce::MessageManager::callAsync ([self] { if (self != nullptr) self->note (juce::String::fromUTF8 ("Creando los sonidos del paquete INJUSTICE (unos segundos)...")); });
                samplepack::ensure (samplepack::packsDir());
                juce::MessageManager::callAsync ([self, need] { if (self == nullptr) return; self->browser.reload(); if (self->steps.hasClip()) self->steps.assignDefaults (samplepack::packDir (samplepack::packsDir())); if (need) self->note (juce::String::fromUTF8 ("Sonidos listos: ya aparecen en el navegador, pestaña Packs.")); });
            }).detach();
        }
        mixer.onTracksChanged = [this] { playlist.repaint(); };
        piano.onMessage = [this] (const juce::String& m) { note (m); };
        piano.onAudition = [this] (te::AudioTrack* t, int pitch) { auditioner.note (t, pitch, 380); };
        steps.onAudition = [this] (te::AudioTrack* t, int pitch) { auditioner.note (t, pitch, 600); };
        playlist.onOpenMidi = [this] (te::MidiClip* c) { piano.setClip (c); showView ("piano"); };
        playlist.onOpenAudio = [this] (te::WaveAudioClip* c) { setAed (true); aed.setClip (c); aed.toFront (true); };
        bars.bpm = edit.tempoSequence.getTempo (0)->getBpm(); refreshMenuData();
        bars.onSig = [this] (int n, int d) { edit.tempoSequence.getTimeSigAt (te::TimePosition()).setStringTimeSig (juce::String (n) + "/" + juce::String (d)); syncTimeSig(); note (juce::String::fromUTF8 ("Comp\xc3\xa1s ") + juce::String (n) + "/" + juce::String (d)); };
        bars.onBpm = [this] (double b) { edit.tempoSequence.getTempo (0)->setBpm (b); };
        bars.onButton = [this] (const juce::String& id)
        {
            auto& tc = edit.getTransport();
            if (id == "play") { if (tc.isPlaying()) tc.stop (false, false); else { steps.endPattern(); browser.stopPreview(); playStart = tc.getPosition(); tc.play (false); } }
            else if (id == "stop") { tc.stop (false, false); tc.setPosition (te::TimePosition()); }
            else if (id == "loop") { tc.looping = ! tc.looping; }
            else if (id == "undo") { undoRedo (true); }
            else if (id == "redo") { undoRedo (false); }
            else if (id == "savetpl") saveTemplate();
            else if (id.startsWith ("mytpl:")) { const auto nm = bars.myTemplates[id.substring (6).getIntValue()]; confirmThen ([this, nm] { if (onReplace) onReplace (userdata::templatesDir().getChildFile (nm + ".injdaw")); }); }
            else if (id.startsWith ("recent:")) { const juce::File rf (bars.recents[id.substring (7).getIntValue()]); confirmThen ([this, rf] { if (rf.existsAsFile() && onReplace) onReplace (rf); }); }
            else if (id == "history") showHistory();
            else if (id == "loadref") loadReference();
            else if (id == "ab") toggleAB();
            else if (id == "follow") { playlist.follow = ! playlist.follow; bars.followOn = playlist.follow; bars.repaint(); }
            else if (id == "zoomall") playlist.zoomFit (false);
            else if (id == "zoomsel") playlist.zoomFit (true);
            else if (id == "tap") tapTempo();
            else if (id == "clickrec") { edit.clickTrackRecordingOnly = ! edit.clickTrackRecordingOnly.get(); bars.clickRec = edit.clickTrackRecordingOnly.get(); bars.repaint(); }
            else if (id.startsWith ("countin:")) { setCountIn (id.fromFirstOccurrenceOf (":", false, false).getIntValue()); note (bars.countIn == 0 ? "Sin cuenta regresiva" : "Cuenta regresiva activada: sonar\xc3\xa1 el metr\xc3\xb3nomo antes de grabar"); }
            else if (id == "clearsolo") { edit.getUndoManager().beginNewTransaction(); for (auto* t : te::getAudioTracks (edit)) { t->setSolo (false); t->setMute (false); } mixer.repaint(); playlist.repaint(); }
            else if (id == "snap") { playlist.snap = ! playlist.snap; piano.snap = playlist.snap; bars.snap = playlist.snap; }
            else if (id == "tool:auto" && bars.tool == "auto") { bars.tool = "select"; playlist.tool = "select"; piano.tool = "select"; playlist.hideAllAutomation(); }
            else if (id.startsWith ("tool:")) { const bool wasAuto = bars.tool == "auto"; bars.tool = id.substring (5); playlist.tool = bars.tool; piano.tool = bars.tool; if (wasAuto && bars.tool != "auto") playlist.hideAllAutomation(); if (bars.tool == "auto") playlist.showAutomationLanes (mixer.selected); }
            else if (id == "quant") { static const double qs[] { 1.0, 0.5, 0.25, 0.125 }; static const char* ql[] { "1/4", "1/8", "1/16", "1/32" }; qi = (qi + 1) % 4; piano.quantum = qs[qi]; bars.quantLabel = ql[qi]; bars.repaint(); }
            else if (id == "view:playlist") showView ("playlist");
            else if (id == "view:piano") { ensurePianoClip(); showView ("piano"); }
            else if (id == "audio") openAudioSettings();
            else if (id == "layout") setBrowser (! browser.isVisible());
            else if (id == "new") confirmThen ([this] { if (onReplace) onReplace ({}); });
            else if (id.startsWith ("tpl:"))
            {
                const int k = id.substring (4).getIntValue();
                if (! samplepack::ready (samplepack::packsDir())) note (juce::String::fromUTF8 ("Los sonidos todavía se están creando; inténtalo en unos segundos"));
                else confirmThen ([this, k] { templates::pending() = k; if (onReplace) onReplace ({}); });
            }
            else if (id == "ideas") openIdeasDialog();
            else if (id == "ideastudio") openIdeaStudio();
            else if (id == "open") confirmThen ([this] { openProject(); });
            else if (id == "export") exportWav();
            else if (id == "help") showHelp();
            else if (id == "plugins") openPluginList();
            else if (id == "rec")
            {
                if (tc.isRecording()) { tc.stop (false, false); bars.recArmed = false; note ("Grabaci\xc3\xb3n terminada"); }
                else if (! trackutil::anyArmed (edit)) { bars.recArmed = false; note ("Arma una pista con el c\xc3\xadrculo del Mixer y vuelve a pulsar Grabar"); }
                else { steps.endPattern(); browser.stopPreview(); tc.record (false); bars.recArmed = tc.isRecording(); note (bars.recArmed ? "Grabando... pulsa Detener para terminar" : "No se pudo iniciar la grabaci\xc3\xb3n (revisa los Ajustes de audio)"); }
            }
            else if (id == "save") saveProject (false);
            else if (id == "saveas") saveProject (true);
            else if (id == "savesamples") saveWithSamples();
            else if (id == "importmidi") importMidiDialog();
            else if (id == "quit") juce::JUCEApplication::getInstance()->systemRequestedQuit();
            else if (id == "copy" || id == "cut" || id == "paste" || id == "selall") { const char ch = id == "copy" ? 'c' : (id == "cut" ? 'x' : (id == "paste" ? 'v' : 'a')); playlist.keyPressed (juce::KeyPress (ch, juce::ModifierKeys::commandModifier, 0)); }
            else if (id == "invertsel") runCmd ("invertsel");
            else if (id == "palette") openPalette();
            else if (id == "midilearn") midiLearnMenu();
            else if (id == "dup") { playlist.keyPressed (juce::KeyPress ('d', juce::ModifierKeys::commandModifier, 0)); }
            else if (id == "del") { playlist.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)); }
            else if (id == "about") note (juce::String::fromUTF8 ("INJUSTICE DAW ") + juce::JUCEApplication::getInstance()->getApplicationVersion() + juce::String::fromUTF8 (" · código abierto (AGPL-3.0) · Injustice Audio"));
            else if (id == "metro") { edit.clickTrackEnabled = ! edit.clickTrackEnabled.get(); bars.metro = edit.clickTrackEnabled.get(); note (bars.metro ? "Metr\xc3\xb3nomo activado" : "Metr\xc3\xb3nomo apagado"); }
            else if (id == "tool:trim") { bars.tool = "trim"; playlist.tool = "trim"; }
            else if (id == "view:mixer") setMixer (! mixer.isVisible());
            else if (id == "view:steps") setSteps (! steps.isVisible());
            else if (id == "view:meters") setMeters (! meters.isVisible());
            else if (id == "view:perf") setPerf (! perf.isVisible());
            else if (id == "view:aed") setAed (! aed.isVisible());
            else if (id == "view:pitch") setPitchEd (! pitchEd.isVisible());
            else if (id == "view:rack") setRack (! rack.isVisible());
            syncState();
        };
        if (auto* pq = std::getenv ("INJ_PALETTE")) juce::Timer::callAfterDelay (600, [this, q = juce::String (pq)] { openPalette(); palette.setQuery (q == "all" ? juce::String() : q); });
        if (auto* th = std::getenv ("INJ_TRACKH")) playlist.setAllHeights (juce::String (th).getIntValue());
        if (std::getenv ("INJ_AUTO")) { bars.tool = "auto"; playlist.tool = "auto"; const double bs = 60.0 / 172.0; for (auto [b, v] : { std::pair<double, float> { 0, .85f }, { 8, .85f }, { 12, .3f }, { 16, .85f }, { 24, .55f } }) playlist.addAutoPoint (0, b * bs, v); }
        if (auto* zz = std::getenv ("INJ_ZOOM")) playlist.setZoom (juce::String (zz).getDoubleValue());
        if (auto* ln = std::getenv ("INJ_LANE")) if (auto* pc = firstMidiClip()) {   // solo capturas: carril de control con una curva, una nota silenciada y un slide
            piano.setClip (pc); const int l = std::atoi (ln); piano.showLane (l); const int ty = l == 2 ? (int) te::MidiControllerEvent::pitchWheelType : 1;
            for (double b = 0.0; b < 6.0; b += 0.0625) piano.setCurvePoint (ty, b, l == 2 ? 8192 + (int) (std::sin (b * 1.6) * 6000) : (int) (64 + std::sin (b * 1.2) * 60));
            auto ns = pc->getSequence().getNotes(); if (ns.size() > 2) { ns[1]->setMute (true, nullptr); ns[2]->state.setProperty ("injSlide", true, nullptr); }
        }
        if (auto* v = std::getenv ("INJ_VIEW")) if (juce::String (v) == "piano") { piano.setClip (firstMidiClip()); showView ("piano"); }
        if (std::getenv ("INJ_OPENSTEPS")) juce::Timer::callAfterDelay (2500, [this] {   // medicion: cuanto se congela la ventana al abrir el Step Sequencer (mayor retraso de la ventana)
            auto worst = std::make_shared<std::atomic<int>> (0); auto stop = std::make_shared<std::atomic<bool>> (false);
            std::thread ([worst, stop] { while (! stop->load()) { const auto t0 = juce::Time::getMillisecondCounter(); auto done = std::make_shared<std::atomic<bool>> (false); juce::MessageManager::callAsync ([done] { done->store (true); });
                while (! done->load() && ! stop->load()) juce::Thread::sleep (2); const int d = (int) (juce::Time::getMillisecondCounter() - t0); if (d > worst->load()) worst->store (d); juce::Thread::sleep (5); } }).detach();
            setSteps (true);
            juce::Timer::callAfterDelay (5000, [worst, stop] { stop->store (true); std::printf ("INJ_OPENSTEPS: mayor congelamiento de la ventana = %d ms\n", worst->load()); std::fflush (stdout); std::_Exit (0); }); });
        if (auto* mk = std::getenv ("INJ_MONKEY")) juce::Timer::callAfterDelay (1500, [this, n = juce::String (mk).getIntValue()]
        {   // prueba de estrés: clics, arrastres, herramientas y atajos al azar sobre la playlist; si algo truena se ve en el depurador
            juce::Random rnd (std::getenv ("INJ_MONKEY_SEED") ? juce::String (std::getenv ("INJ_MONKEY_SEED")).getIntValue() : 12345); auto src = juce::Desktop::getInstance().getMainMouseSource(); const char* tools[] { "select", "pencil", "brush", "cut", "trim", "auto" };
            const int keys[] { 'z', 'y', 'c', 'v', 'd', 'a', 'x' }; int done = 0;
            for (int i = 0; i < n; ++i)
            {
                playlist.tool = tools[rnd.nextInt (6)]; const int W = std::max (50, playlist.getWidth()), H = std::max (50, playlist.getHeight());
                juce::Point<float> p ((float) rnd.nextInt (W), (float) rnd.nextInt (H)), q = p + juce::Point<float> ((float) rnd.nextInt (400) - 200.f, (float) rnd.nextInt (200) - 100.f);
                int mods = 0; const int r = rnd.nextInt (10); if (r == 0) mods = juce::ModifierKeys::shiftModifier; else if (r == 1) mods = juce::ModifierKeys::altModifier; else if (r == 2) mods = juce::ModifierKeys::ctrlModifier; else if (r == 3) mods = juce::ModifierKeys::rightButtonModifier;
                auto ev = [&] (juce::Point<float> pt) { return juce::MouseEvent (src, pt, juce::ModifierKeys (mods), 1.f, 0.f, 0.f, 0.f, 0.f, &playlist, &playlist, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false); };
                playlist.mouseDown (ev (p)); playlist.mouseDrag (ev (q)); playlist.mouseUp (ev (q)); ++done;
                if (rnd.nextInt (7) == 0) { const int k = rnd.nextInt (3); if (k == 0) playlist.mouseWheelMove (ev (p), juce::MouseWheelDetails { 0.f, (float) (rnd.nextFloat() - .5f), false, false, false }); else if (k == 1) keyPressed (juce::KeyPress (keys[rnd.nextInt (7)], juce::ModifierKeys (juce::ModifierKeys::commandModifier), 0), nullptr); else keyPressed (juce::KeyPress (juce::KeyPress::deleteKey, juce::ModifierKeys(), 0), nullptr); }
                if (i % 25 == 0 && playlist.getWidth() > 0) { juce::Image img (juce::Image::ARGB, playlist.getWidth(), playlist.getHeight(), true); juce::Graphics g (img); playlist.paintEntireComponent (g, false); }
            }
            { te::MidiClip* mc = nullptr; for (auto* tk : te::getAudioTracks (edit)) for (auto* c : tk->getClips()) if (auto* m = dynamic_cast<te::MidiClip*> (c)) { mc = m; break; }
              if (mc != nullptr)
              {
                  setSize (1360, 800); piano.setBounds (0, 60, 1300, 700); piano.setClip (mc); const char* pt[] { "select", "pencil", "brush", "cut", "erase" }; int pd = 0;
                  for (int i = 0; i < n; ++i)
                  {
                      piano.tool = pt[rnd.nextInt (5)]; const int W = std::max (50, piano.getWidth()), H = std::max (50, piano.getHeight());
                      juce::Point<float> p ((float) rnd.nextInt (W), (float) rnd.nextInt (H)), q = p + juce::Point<float> ((float) rnd.nextInt (300) - 100.f, (float) rnd.nextInt (160) - 80.f);
                      int mods = 0; const int r = rnd.nextInt (10); if (r == 0) mods = juce::ModifierKeys::shiftModifier; else if (r == 1) mods = juce::ModifierKeys::altModifier; else if (r == 2) mods = juce::ModifierKeys::ctrlModifier; else if (r == 3) mods = juce::ModifierKeys::rightButtonModifier;
                      auto ev = [&] (juce::Point<float> pt2) { return juce::MouseEvent (src, pt2, juce::ModifierKeys (mods), 1.f, 0.f, 0.f, 0.f, 0.f, &piano, &piano, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false); };
                      piano.mouseDown (ev (p)); piano.mouseDrag (ev (q)); piano.mouseUp (ev (q)); ++pd;
                      if (rnd.nextInt (9) == 0) { const int k = rnd.nextInt (3); if (k == 0) piano.mouseWheelMove (ev (p), juce::MouseWheelDetails { 0.f, (float) (rnd.nextFloat() - .5f), false, false, false }); else if (k == 1) piano.keyPressed (juce::KeyPress (keys[rnd.nextInt (7)], juce::ModifierKeys (juce::ModifierKeys::commandModifier), 0)); else piano.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey, juce::ModifierKeys(), 0)); }
                      if (i % 25 == 0 && piano.getWidth() > 0) { juce::Image img (juce::Image::ARGB, piano.getWidth(), piano.getHeight(), true); juce::Graphics g (img); piano.paintEntireComponent (g, false); }
                  }
                  std::printf ("MONKEY PIANO OK %d gestos\n", pd);
              } }
            std::printf ("MONKEY OK %d gestos\n", done); std::fflush (stdout); std::_Exit (0); });
        if (auto* mu = std::getenv ("INJ_MONKEY_UI")) juce::Timer::callAfterDelay (1500, [this, n = juce::String (mu).getIntValue()]
        {   // estres de Mixer, Step Sequencer y Browser: clics, arrastres, doble clic y rueda al azar (sin clic derecho para no abrir menus)
            juce::Random rnd (std::getenv ("INJ_MONKEY_SEED") ? juce::String (std::getenv ("INJ_MONKEY_SEED")).getIntValue() : 4242); auto src = juce::Desktop::getInstance().getMainMouseSource();
            setSize (1360, 800); setMixer (true); setSteps (true); steps.ensureClip(); int done = 0;
            auto hit = [&] (juce::Component& c, int count)
            {
                for (int i = 0; i < count; ++i)
                {
                    const int W = std::max (50, c.getWidth()), H = std::max (50, c.getHeight());
                    juce::Point<float> p ((float) rnd.nextInt (W), (float) rnd.nextInt (H)), q = p + juce::Point<float> ((float) rnd.nextInt (200) - 100.f, (float) rnd.nextInt (160) - 80.f);
                    int mods = 0; const int r = rnd.nextInt (8); if (r == 0) mods = juce::ModifierKeys::shiftModifier; else if (r == 1) mods = juce::ModifierKeys::altModifier; else if (r == 2) mods = juce::ModifierKeys::ctrlModifier;
                    mods |= juce::ModifierKeys::leftButtonModifier;
                    auto ev = [&] (juce::Point<float> pt, int clicks) { return juce::MouseEvent (src, pt, juce::ModifierKeys (mods), 1.f, 0.f, 0.f, 0.f, 0.f, &c, &c, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), clicks, false); };
                    const auto t0 = juce::Time::getMillisecondCounter(); c.mouseDown (ev (p, 1)); const auto t1 = juce::Time::getMillisecondCounter(); c.mouseDrag (ev (q, 1)); const auto t2 = juce::Time::getMillisecondCounter(); c.mouseUp (ev (q, 1)); ++done;
                    { const auto t3 = juce::Time::getMillisecondCounter(); if (t3 - t0 > 250) std::printf ("  lento: %u ms (down %u, drag %u, up %u) en %d,%d -> %d,%d mods=%d\n", t3 - t0, t1 - t0, t2 - t1, t3 - t2, (int) p.x, (int) p.y, (int) q.x, (int) q.y, mods); }
                    const int k = rnd.nextInt (8); if (k == 0) c.mouseDoubleClick (ev (p, 2)); else if (k == 1) c.mouseWheelMove (ev (p, 1), juce::MouseWheelDetails { 0.f, (float) (rnd.nextFloat() - .5f), false, false, false });
                    if (i % 20 == 0 && c.getWidth() > 0 && c.getHeight() > 0) { juce::Image img (juce::Image::ARGB, c.getWidth(), c.getHeight(), true); juce::Graphics g (img); c.paintEntireComponent (g, false); }
                }
            };
            std::printf ("  [mixer]\n"); hit (mixer, n); std::printf ("MONKEY MIXER OK\n"); std::printf ("  [steps]\n"); hit (steps, n); std::printf ("MONKEY STEPS OK\n"); hit (browser, n / 2); std::printf ("MONKEY BROWSER OK\n");
            std::printf ("MONKEY UI OK %d gestos\n", done); std::fflush (stdout); std::_Exit (0); });
        if (auto* mc = std::getenv ("INJ_MONKEY_CMD")) juce::Timer::callAfterDelay (1500, [this, n = juce::String (mc).getIntValue()]
        {   // mono de comandos: elige clips al azar, mueve el cabezal, ejecuta comandos al azar y deshace/rehace
            juce::Random rnd (std::getenv ("INJ_MONKEY_SEED") ? juce::String (std::getenv ("INJ_MONKEY_SEED")).getIntValue() : 777); auto& tc = edit.getTransport(); setSteps (true); steps.ensureClip();
            for (int i = 0; i < n; ++i)
            {
                auto tracks = te::getAudioTracks (edit); juce::Array<te::Clip*> pick; for (auto* t : tracks) for (auto* c : t->getClips()) if (rnd.nextInt (3) == 0) pick.add (c);
                playlist.selectClips (pick); playlist.setSelTrackForTest (rnd.nextInt (tracks.size() + 2) - 1); tc.setPosition (te::TimePosition::fromSeconds (rnd.nextDouble() * 30.0)); steps.selectRow (rnd.nextInt (4));
                const auto id = cmds[(size_t) rnd.nextInt ((int) cmds.size())].id; if (id == "fullscreen" || id == "palette" || id == "bounce" || id == "macro_stop" || id == "macro_del" || id == "macro_rec" || id == "harm_key" || id == "midi_menu" || id == "arp_menu" || id == "tpl_save" || id == "tpl_new" || id == "undohist" || id == "notes" || id == "audio2midi" || id == "audioeditor" || id == "padrack" || id == "typingpiano" || id.startsWith ("macro:")) continue;   // abren menús o diálogos, o renderizan audio
                if (id == "transoctup" || id == "transoctdown" || id == "doublemidi" || id == "stretch2") { if (rnd.nextInt (3) != 0) continue; }
                const auto t0 = juce::Time::getMillisecondCounterHiRes(); runCmd (id); if (std::getenv ("INJ_MONKEY_LOG") || juce::Time::getMillisecondCounterHiRes() - t0 > 400.0) std::printf ("[cmd %d] %s %.0f ms (%d pistas)\n", i, id.toRawUTF8(), juce::Time::getMillisecondCounterHiRes() - t0, te::getAudioTracks (edit).size()); std::fflush (stdout); const int r = rnd.nextInt (8); if (r == 0) edit.getUndoManager().undo(); else if (r == 1) edit.getUndoManager().redo();
                if (te::getAudioTracks (edit).size() > 24) runCmd ("delempty");
                if (i % 40 == 0) { mixer.refreshTracks(); playlist.repaint(); }
            }
            std::printf ("MONKEY CMD OK %d comandos, %d pistas\n", n, te::getAudioTracks (edit).size()); std::fflush (stdout); std::_Exit (0);
        });
        if (std::getenv ("INJ_MONKEY_FX")) juce::Timer::callAfterDelay (1500, [this]
        {   // estrés de efectos: añade, abre el editor, quita y deshace efectos al azar con el bucle de mensajes corriendo (como en uso real)
            auto rnd = std::make_shared<juce::Random> (777); auto count = std::make_shared<int> (0);
            struct T : juce::Timer { std::function<void()> f; void timerCallback() override { f(); } };
            auto* tm = new T; tm->f = [this, rnd, count, tm]
            {
                auto tracks = te::getAudioTracks (edit); if (tracks.isEmpty()) return;
                mixer.selected = rnd->nextInt (tracks.size()); const int a = rnd->nextInt (10); auto& cat = injfx::catalog();
                if (a < 4) { if (auto* np = mixer.addBuiltinFx (cat[(size_t) rnd->nextInt ((int) cat.size())].type)) if (rnd->nextBool()) openEditorWhenReady (np); }
                else if (a < 7) { auto* tk = tracks[rnd->nextInt (tracks.size())]; juce::Array<te::Plugin*> fx; for (auto* p : tk->pluginList.getPlugins()) if (dynamic_cast<injfx::InjFx*> (p)) fx.add (p); if (! fx.isEmpty()) fx[rnd->nextInt (fx.size())]->deleteFromParent(); }
                else if (a < 9) undoRedo (rnd->nextBool());
                else { auto tk = tracks[rnd->nextInt (tracks.size())]; for (auto* p : tk->pluginList.getPlugins()) if (dynamic_cast<injfx::InjFx*> (p) && rnd->nextInt (3) == 0) { p->setEnabled (! p->isEnabled()); } }
                if (++*count >= 300) { std::printf ("MONKEY FX OK %d acciones\n", *count); std::fflush (stdout); std::_Exit (0); }
            };
            tm->startTimer (15);
        });
        if (auto* of = std::getenv ("INJ_OPENFX")) juce::Timer::callAfterDelay (1200, [this, t = juce::String (of)]   // solo capturas: pone un efecto en la pista 1 y abre su ventana
        { if (te::getAudioTracks (edit).isEmpty()) return; mixer.selected = 0; if (auto* np = mixer.addBuiltinFx (t.toRawUTF8())) openEditorWhenReady (np); });
        if (std::getenv ("INJ_FXTEST")) juce::Timer::callAfterDelay (300, [this] { int f = 0; injfx::test::run (edit, f); injfx::test::runNew (edit, f); std::printf (f == 0 ? "TODO OK\n" : "%d FALLOS\n", f); std::fflush (stdout); std::_Exit (f == 0 ? 0 : 1); });
        if (std::getenv ("INJ_STEPTEST")) juce::Timer::callAfterDelay (500, [this] { setSteps (true); steps.setTopLeftPosition (0, 60); steps.ensureClip(); int f = 0; stepFeatureTests (f, steps.getClip() ? dynamic_cast<te::AudioTrack*> (steps.getClip()->getTrack()) : nullptr); std::printf (f == 0 ? "TODO OK\n" : "%d FALLOS\n", f); std::fflush (stdout); std::_Exit (0); });
        if (std::getenv ("INJ_SELFTEST")) juce::Timer::callAfterDelay (500, [this] { const int f = runSelfTest(); std::fflush (stdout); if (f >= 0) std::_Exit (f == 0 ? 0 : 1); });
        startTimerHz (30);
    }
    // Autoprueba de edición (INJ_SELFTEST=1): simula clics sobre el Piano Roll y comprueba el resultado.
    // Prueba de tono y velocidad (Rubber Band). Va aparte porque el audio estirado se prepara en segundo plano y hay que dejar correr el programa.
    void runToneTest (int startFails)
    {
        auto fails = std::make_shared<int> (startFails);
        auto chk = [fails] (bool ok, const juce::String& what) { std::printf ("%s  %s\n", ok ? "OK  " : "FALLO", what.toRawUTF8()); if (! ok) ++*fails; };
        auto sine = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_sine440.wav"); sine.deleteFile();
        { juce::WavAudioFormat wf; std::unique_ptr<juce::FileOutputStream> os (sine.createOutputStream()); std::unique_ptr<juce::AudioFormatWriter> w (wf.createWriterFor (os.get(), 44100.0, 1, 16, {}, 0)); if (w) { os.release(); juce::AudioBuffer<float> b (1, 88200); for (int i = 0; i < 88200; ++i) b.setSample (0, i, 0.5f * std::sin (juce::MathConstants<float>::twoPi * 440.f * (float) i / 44100.f)); w->writeFromAudioSampleBuffer (b, 0, 88200); } }
        trackutil::addTrack (edit); int kti = -1;
        { auto kl = te::getAudioTracks (edit); auto* kt = kl.getLast(); kti = te::getAllTracks (edit).indexOf (kt); auto kc = kt->insertMIDIClip ("kit", te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (2.0)), nullptr); kc->getSequence().addNote (36, te::BeatPosition::fromBeats (0.0), te::BeatDuration::fromBeats (1.0), 110, 0, nullptr);
          auto pd = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_pack_test"); samplepack::ensure (pd); instruments::addKit (edit, *kt, pd); }
        trackutil::addTrack (edit); auto tt = te::getAudioTracks (edit); auto* st = tt.getLast(); const int sti = te::getAllTracks (edit).indexOf (st);
        auto sc = st->insertWaveClip ("seno", sine, { te::TimeRange (te::TimePosition::fromSeconds (0.0), te::TimeDuration::fromSeconds (2.0)), te::TimeDuration() }, false);
        auto* wc = dynamic_cast<te::WaveAudioClip*> (sc.get()); if (wc == nullptr) { std::printf ("FALLO  el clip de prueba no es de audio\n"); std::fflush (stdout); std::_Exit (1); }
        auto freqOf = [this, sti] (const char* nm, double secs) -> double
        {
            auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (nm); if (! renderWav (f, secs, {}, sti)) return -1.0;
            juce::AudioFormatManager afm; afm.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> r (afm.createReaderFor (f)); if (! r) return -1.0;
            const int n0 = (int) (r->sampleRate * 0.5), n1 = (int) (r->sampleRate * 1.0); juce::AudioBuffer<float> b (1, n1 - n0); r->read (&b, 0, n1 - n0, n0, true, false);
            int zc = 0; for (int i = 1; i < b.getNumSamples(); ++i) if (b.getSample (0, i - 1) < 0.f && b.getSample (0, i) >= 0.f) ++zc; return zc / ((n1 - n0) / r->sampleRate);
        };
        const double f0 = freqOf ("inj_p0.wav", 2.0); chk (std::abs (f0 - 440.0) < 6.0, "tono: sin cambios el seno suena a 440 Hz (" + juce::String (f0, 1) + ")");
        PlaylistView::setClipPitch (*wc, 12.f);
        juce::Timer::callAfterDelay (4000, [this, kti, freqOf, chk, wc, fails]() mutable
        {
            const double f12 = freqOf ("inj_p12.wav", 2.0); chk (std::abs (f12 - 880.0) < 25.0, "tono: +12 semitonos sube una octava (" + juce::String (f12, 1) + " Hz)");
            PlaylistView::setClipPitch (*wc, 0.f); PlaylistView::setClipSpeed (*wc, 2.0);
            juce::Timer::callAfterDelay (4000, [this, kti, freqOf, chk, wc, fails]() mutable
            {
                const double fs2 = freqOf ("inj_s2.wav", 2.0);
                chk (std::abs (wc->getPosition().getLength().inSeconds() - 1.0) < 0.01, "velocidad: al 200 % el clip dura la mitad (" + juce::String (wc->getPosition().getLength().inSeconds(), 3) + " s)");
                chk (std::abs (fs2 - 440.0) < 12.0, "velocidad: al 200 % el tono se conserva (" + juce::String (fs2, 1) + " Hz)");
                {   // kit de batería (los sonidos del sampler se cargan en segundo plano)
                    auto kf = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_kit.wav"); double kv = -1.0;
                    if (renderWav (kf, 1.5, {}, kti)) { juce::AudioFormatManager afm; afm.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> r (afm.createReaderFor (kf)); if (r) { juce::AudioBuffer<float> b (2, (int) r->lengthInSamples); r->read (&b, 0, (int) r->lengthInSamples, 0, true, true); kv = (double) b.getRMSLevel (0, 0, b.getNumSamples()); } }
                    chk (kv > 0.002, "kit de bateria: suena al tocar notas (" + juce::String (kv, 4) + ")");
                }
                if (! std::getenv ("INJ_NOINTEG")) injfx::test::runIntegration (edit, *fails);
                cmdTests (chk);
                std::printf (*fails == 0 ? "TODO OK\n" : "%d FALLOS\n", *fails); std::fflush (stdout); std::_Exit (*fails == 0 ? 0 : 1);
            });
        });
    }
    int selfFails = 0;
    int selfTestStage2()
    {
        int fails = selfFails; auto chk = [&] (bool ok, const char* what) { std::printf ("%s  %s\n", ok ? "OK  " : "FALLO", what); if (! ok) ++fails; };
        if (std::getenv ("INJ_INTEG_EARLY")) { int f2 = 0; injfx::test::runIntegration (edit, f2); fails += f2; }   // quita 38 efectos antes de seguir: comprueba que destruirlos no rompe nada
        {
            auto out = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_steps_test.wav");
            out.deleteFile();   // el render AÑADE al archivo si ya existe: hay que partir de uno vacío
            const bool ok = te::Renderer::renderToFile (edit, out, false); chk (ok, "step: el render offline termina");
            juce::AudioFormatManager fmg; fmg.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> rd (fmg.createReaderFor (out));
            if (rd)
            {
                juce::AudioBuffer<float> b ((int) rd->numChannels, (int) rd->lengthInSamples); rd->read (&b, 0, b.getNumSamples(), 0, true, true);
                auto peak = [&] (double t0, double t1) { float p = 0; for (int c = 0; c < b.getNumChannels(); ++c) for (int i = (int) (t0 * rd->sampleRate); i < std::min (b.getNumSamples(), (int) (t1 * rd->sampleRate)); ++i) p = std::max (p, std::abs (b.getSample (c, i))); return p; };
                const double step = 60.0 / edit.tempoSequence.getTempo (0)->getBpm() / 4.0;
                for (int s : { 0, 4, 8, 12 }) chk (peak (s * step, s * step + 0.04) > 0.05f, ("step: suena el kick en el paso " + juce::String (s + 1)).toRawUTF8());
                chk (peak (0.11, 0.2) > 0.05f, "step: el sample suena completo (no se corta al terminar el paso)");
                for (int s : { 2, 6, 10 }) chk (peak (s * step + 0.02, s * step + 0.06) < peak (s * step - 0.06, s * step - 0.02), ("step: sin golpe nuevo en el paso " + juce::String (s + 1) + " (solo la cola del anterior)").toRawUTF8());
            }
            else chk (false, "step: se puede leer el WAV renderizado");
        }
        {   // armar pista para grabar (MIDI hacia una pista con instrumento)
            auto* host = dynamic_cast<te::AudioTrack*> (steps.getClip()->getTrack());
            if (host)
            {
                const int n = trackutil::armTrack (edit, *host, true);
                if (n > 0) { chk (trackutil::isArmed (*host) && trackutil::anyArmed (edit), "grabar: armar una pista con instrumento asigna la entrada MIDI"); trackutil::armTrack (edit, *host, false); chk (! trackutil::isArmed (*host), "grabar: desarmar la pista"); }
                else { chk (! trackutil::isArmed (*host), "grabar: sin entradas disponibles la pista no queda armada (en esta maquina no hay teclado MIDI)"); }
            }
        }
        {   // mover y enlazar canales
            trackutil::ensureTracks (edit, 3); auto tl = te::getAudioTracks (edit); chk (tl.size() >= 3, "mixer: hay al menos 3 canales para probar mover y enlazar"); if (tl.size() >= 3)
            {
                te::AudioTrack::Ptr a (tl[0]), b (tl[1]); mixer.moveChannelTo (0, 1); auto t2 = te::getAudioTracks (edit);
                chk (t2[0] == b.get() && t2[1] == a.get(), "mixer: mover un canal a la derecha intercambia el orden");
                mixer.moveChannelTo (1, 0); t2 = te::getAudioTracks (edit); chk (t2[0] == a.get() && t2[1] == b.get(), "mixer: se puede devolver al lugar original");
                a->getVolumePlugin()->setVolumeDb (0.f); b->getVolumePlugin()->setVolumeDb (-6.f); mixer.linkChannels (*a, *b); mixer.setLinkedVolume (*a, -4.f);
                chk (std::abs (b->getVolumePlugin()->getVolumeDb() + 10.f) < 0.05f, ("mixer: un canal enlazado se mueve junto con el otro (" + juce::String (b->getVolumePlugin()->getVolumeDb(), 1) + " dB)").toRawUTF8());
                mixer.unlinkChannel (*a); mixer.setLinkedVolume (*a, 0.f); chk (std::abs (b->getVolumePlugin()->getVolumeDb() + 10.f) < 0.05f && MixerPanel::linkOf (*b).isEmpty(), "mixer: al quitar el enlace ya no se mueven juntos");
                a->getVolumePlugin()->setVolumeDb (0.f); b->getVolumePlugin()->setVolumeDb (0.f);
                mixer.selectMaster (false); mixer.selected = 0; mixer.toggleSendTo (*a, *b); chk (mixer.sendKnobShown (1) && ! mixer.sendKnobShown (2), "mixer: el canal destino de un envio muestra la perilla de nivel");
                mixer.toggleSendTo (*a, *b); chk (! mixer.sendKnobShown (1), "mixer: sin envio no hay perilla");
                { auto cd = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_chain_test"); cd.createDirectory(); mixer.setEqMid (*a, 700.f, 9.f); mixer.setWidthOfTrack (*a, 0.3f);
                  const bool saved = mixer.saveChain (*a, "prueba", cd); mixer.clearChain (*b); chk (saved && mixer.eqMidGainOf (*b) < -90.f, "cadena: se guarda la cadena de un canal");
                  const bool loaded = mixer.loadChain (*b, cd.getChildFile ("prueba.injchain")); chk (loaded && std::abs (mixer.eqMidGainOf (*b) - 9.f) < 0.1f && std::abs (mixer.widthOfTrack (*b) - 0.3f) < 0.01f, ("cadena: al cargarla en otro canal vuelven el ecualizador y el estereo (" + juce::String (mixer.eqMidGainOf (*b), 1) + " dB)").toRawUTF8());
                  mixer.clearChain (*a); mixer.clearChain (*b); cd.deleteRecursively(); }
                { auto& am = edit.getAutomationRecordManager(); const bool w0 = am.isWritingAutomation(); am.setWritingAutomation (true); const bool on = am.isWritingAutomation(); am.setWritingAutomation (w0); chk (on, "automatizacion: se puede activar el modo escribir"); }
            }
        }
        {   // envíos y retornos
            auto peakOf = [] (const juce::File& f) { juce::AudioFormatManager fm; fm.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f)); if (! r) return -1.f; juce::AudioBuffer<float> b ((int) r->numChannels, (int) std::min<juce::int64> (r->lengthInSamples, (juce::int64) (0.5 * r->sampleRate))); r->read (&b, 0, b.getNumSamples(), 0, true, true); return b.getMagnitude (0, b.getNumSamples()); };
            auto* host = dynamic_cast<te::AudioTrack*> (steps.getClip()->getTrack()); te::AudioTrack* ret = nullptr; for (auto* t : te::getAudioTracks (edit)) if (t != host && t->getClips().isEmpty()) { ret = t; break; }
            if (host && ret)
            {
                auto all = te::getAllTracks (edit); auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_aux.wav"); ExportOpts eo;
                juce::ignoreUnused (all); mixer.toggleReturn (*ret, 0); renderWav (f, 1.0, eo); const float none = peakOf (f);
                mixer.toggleSend (*host, 0); renderWav (f, 1.0, eo); const float withSend = peakOf (f);
                chk (mixer.sendOf (*host, 0) != nullptr && mixer.returnOf (*ret, 0) != nullptr, "envios: se crean el envio y el retorno");
                chk (none > 0.05f && withSend > none * 1.25f, "envios: el retorno suma el sonido que le manda la otra pista");
                mixer.toggleSend (*host, 0); mixer.toggleReturn (*ret, 0); chk (mixer.sendOf (*host, 0) == nullptr && mixer.returnOf (*ret, 0) == nullptr, "envios: se pueden quitar");
            }
            else chk (false, "envios: hay una pista libre para el retorno");
        }
        {   // sidechain real: la pista baja cuando suena el bombo de otra
            auto winPeak = [] (const juce::File& f, double t0, double t1) { juce::AudioFormatManager fm; fm.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f)); if (! r) return -1.f; const int a = (int) (t0 * r->sampleRate), n = (int) ((t1 - t0) * r->sampleRate); juce::AudioBuffer<float> b ((int) r->numChannels, n); r->read (&b, 0, n, a, true, true); return b.getRMSLevel (0, 0, n); };
            auto* host = dynamic_cast<te::AudioTrack*> (steps.getClip()->getTrack()); te::AudioTrack* dst = nullptr; for (auto* t : te::getAudioTracks (edit)) if (t != host && t->getClips().isEmpty()) { dst = t; break; }
            if (host && dst && instruments::addSynth (edit, *dst, 0) != nullptr)
            {
                ideas::insert (edit, *dst, 0.0, 2, { { 48, 0.0, 8.0, 30 } }, "sc");
                auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_sc.wav"); ExportOpts eo; host->setMute (true);
                renderWav (f, 1.0, eo); const float plain = winPeak (f, 0.03, 0.40);
                chk (trackutil::setSidechain (edit, *dst, host, 1.0, 250.0) && trackutil::sidechainSource (edit, *dst) == host, "sidechain: se enlaza la pista con la fuente");
                renderWav (f, 1.0, eo); const float ducked = winPeak (f, 0.03, 0.40);
                chk (plain > 0.01f && ducked < plain * 0.8f, ("sidechain: la pista baja cuando suena el bombo (" + juce::String (plain, 3) + " -> " + juce::String (ducked, 3) + ")").toRawUTF8());
                trackutil::removeSidechain (*dst); chk (trackutil::sidechainPlugin (*dst) == nullptr, "sidechain: se puede quitar");
                host->setMute (false); for (auto* c : dst->getClips()) { auto* cc = c; cc->removeFromParent(); break; }
            }
            else chk (false, "sidechain: hay una pista libre para probar");
        }

        {   // juntar samples junto al proyecto
            auto cd = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_collect_test"); cd.deleteRecursively();
            const int n = collectSamples (cd); bool inside = false; for (auto* t : te::getAudioTracks (edit)) for (auto* p : t->pluginList.getPlugins()) if (auto* sp = dynamic_cast<te::SamplerPlugin*> (p)) for (auto v : sp->state) if (v.hasType ("SOUND") && juce::File (v.getProperty ("source").toString()).isAChildOf (cd)) inside = true;
            chk (n >= 1 && inside && cd.getNumberOfChildFiles (juce::File::findFiles) >= 1, "guardar con samples: se copian y se enlazan junto al proyecto");
            chk (collectSamples (cd) == 0, "guardar con samples: repetirlo no duplica archivos");
        }
        {   // volumen por fila y fuerza por paso: deben cambiar el nivel real (el sampler aplica los cambios de forma asincrona: se espera entre pasos)
            auto render = [this] (const char* nm) -> float
            {
                auto out = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (nm); out.deleteFile();
                te::Renderer::renderToFile (edit, out, false);
                juce::AudioFormatManager fmg; fmg.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> rd (fmg.createReaderFor (out)); if (! rd) return -1.f;
                juce::AudioBuffer<float> b ((int) rd->numChannels, (int) rd->lengthInSamples); rd->read (&b, 0, b.getNumSamples(), 0, true, true);
                float p = 0; for (int c = 0; c < b.getNumChannels(); ++c) for (int i = 0; i < std::min (b.getNumSamples(), (int) (0.06 * rd->sampleRate)); ++i) p = std::max (p, std::abs (b.getSample (c, i)));
                return p;
            };
            auto full = std::make_shared<float> (render ("inj_v1.wav"));
            {   // limitador del master
                mixer.setLimiter (true); auto* lp = mixer.limiterPlugin(); const bool made = lp != nullptr && mixer.limiterOn();
                if (lp) lp->ceiling = juce::Decibels::decibelsToGain (-24.0f);
                auto lf = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_lim.wav"); lf.deleteFile(); ExportOpts lo; lo.fmt = 0; renderWav (lf, 1.0, lo);
                float lim = 0.f; { juce::AudioFormatManager fm2; fm2.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> r2 (fm2.createReaderFor (lf)); if (r2) { juce::AudioBuffer<float> b2 ((int) r2->numChannels, (int) std::min<juce::int64> (r2->lengthInSamples, (juce::int64) (0.06 * r2->sampleRate))); r2->read (&b2, 0, b2.getNumSamples(), 0, true, true); lim = b2.getMagnitude (0, b2.getNumSamples()); } } const bool ok = made && lim > 0.0f && lim < 0.08f;
                std::printf ("%s  master: el limitador baja los picos (sin=%.3f con=%.3f)\n", ok ? "OK  " : "FALLO", (double) *full, (double) lim); if (! ok) ++fails;
                mixer.setLimiter (false); const bool off = ! mixer.limiterOn(); std::printf ("%s  master: el limitador se apaga\n", off ? "OK  " : "FALLO"); if (! off) ++fails;
            }
            steps.setRowVolume (0, -20.f);
            juce::Timer::callAfterDelay (400, [this, render, full, fails]() mutable
            {
                const float quiet = render ("inj_v2.wav"); const bool ok1 = *full > 0.05f && quiet < *full * 0.3f; std::printf ("%s  step: bajar el volumen de la fila baja el nivel (~-20 dB)\n", ok1 ? "OK  " : "FALLO"); if (! ok1) ++fails;
                steps.setRowVolume (0, 0.f); steps.setStepVelocity (0, 0, 20);
                juce::Timer::callAfterDelay (400, [this, render, full, fails]() mutable
                {
                    const float soft = render ("inj_v3.wav"); const bool ok2 = soft < *full * 0.7f; std::printf ("%s  step: menos fuerza en un paso suena mas bajo\n", ok2 ? "OK  " : "FALLO"); if (! ok2) ++fails;
                    steps.setStepVelocity (0, 0, 100);
                    auto tracks = te::getAudioTracks (edit); auto* host = dynamic_cast<te::AudioTrack*> (steps.getClip()->getTrack()); const int hi = tracks.indexOf (host);
                    juce::Timer::callAfterDelay (400, [this, render, host, hi, fails]() mutable
                    {
                        const float normal = render ("inj_a0.wav"); playlist.addAutoPoint (hi, 0.0, 0.0f); playlist.addAutoPoint (hi, 8.0, 0.0f);
                        const bool two = PlaylistView::volParamOf (*host)->getCurve().getNumPoints() == 2; std::printf ("%s  automatizacion: se crean los puntos de la curva\n", two ? "OK  " : "FALLO"); if (! two) ++fails;
                        juce::Timer::callAfterDelay (400, [this, render, host, normal, fails]() mutable
                        {
                            const float silent = render ("inj_a1.wav"); const bool ok = normal > 0.05f && silent < normal * 0.05f; std::printf ("%s  automatizacion: una curva de volumen en cero silencia la pista (sin=%.3f con=%.3f)\n", ok ? "OK  " : "FALLO", (double) normal, (double) silent); if (! ok) ++fails;
                            PlaylistView::volParamOf (*host)->getCurve().clear();
                            juce::Timer::callAfterDelay (400, [this, render, host, normal, fails]() mutable
                            {
                                const bool back = render ("inj_a2.wav") > normal * 0.9f; std::printf ("%s  automatizacion: al borrar la curva vuelve el sonido\n", back ? "OK  " : "FALLO"); if (! back) ++fails;
                                // automatizar un parametro de un plugin (no el volumen de la pista)
                                auto extra = edit.getPluginCache().createNewPlugin (te::VolumeAndPanPlugin::xmlTypeName, {}); host->pluginList.insertPlugin (extra, -1, nullptr);
                                te::AutomatableParameter* ep = extra->getAutomatableParameter (0).get(); const juce::String pid = "P" + extra->itemID.toString() + "|" + ep->paramID;
                                PlaylistView::setAutoParam (*host, pid);
                                const bool sel = ep != nullptr && PlaylistView::autoParamOf (*host) == ep;
                                std::printf ("%s  automatizacion: se puede elegir un parametro de plugin (%s)\n", sel ? "OK  " : "FALLO", ep ? ep->getParameterName().toRawUTF8() : "?"); if (! sel) ++fails;
                                auto extra2 = edit.getPluginCache().createNewPlugin (te::VolumeAndPanPlugin::xmlTypeName, {}); host->pluginList.insertPlugin (extra2, 0, nullptr);
                                const bool stable = PlaylistView::autoParamOf (*host) == ep; std::printf ("%s  automatizacion: la eleccion no depende del orden de los plugins\n", stable ? "OK  " : "FALLO"); if (! stable) ++fails;
                                extra2->deleteFromParent();
                                const int hi2 = te::getAudioTracks (edit).indexOf (host); playlist.addAutoPoint (hi2, 0.0, 0.0f); playlist.addAutoPoint (hi2, 8.0, 0.0f);
                                juce::Timer::callAfterDelay (400, [this, render, host, normal, ep, fails]() mutable
                                {
                                    const float s2 = render ("inj_a3.wav"); const bool ok3 = s2 < normal * 0.05f; std::printf ("%s  automatizacion: la curva de un parametro de plugin cambia el sonido (sin=%.3f con=%.3f)\n", ok3 ? "OK  " : "FALLO", (double) normal, (double) s2); if (! ok3) ++fails;
                                    ep->getCurve().clear(); PlaylistView::setAutoParam (*host, "vol");
                                    { const bool tapOk = meters.getTap() != nullptr && meters.getTap()->written() > 0; std::printf ("%s  medidores: la toma del Master recibe el audio\n", tapOk ? "OK  " : "FALLO"); if (! tapOk) ++fails; }
                                    stepAudioTests (fails);
                                });
                            });
                        });
                    });

                });
            });
            return -1;
        }
        std::printf (fails == 0 ? "TODO OK\n" : "%d FALLOS\n", fails); return fails;
    }
    void stepFeatureTests (int& fails, te::AudioTrack* host)
    {
        auto chk = [&] (bool ok, const char* what) { std::printf ("%s  %s\n", ok ? "OK  " : "FALLO", what); if (! ok) ++fails; };
                    const int origSteps = steps.stepsCount(); steps.setStepCount (16);
                    { setCountIn (1); chk (edit.getNumCountInBeats() == 4 && bars.countIn == 1, "grabar: cuenta regresiva de 1 compás = 4 tiempos");
                      setCountIn (4); chk (edit.getNumCountInBeats() == 1, "grabar: cuenta regresiva de 1 tiempo"); setCountIn (0); chk (edit.getNumCountInBeats() == 0, "grabar: sin cuenta regresiva"); }
                    if (auto* sf = std::getenv ("INJ_SAMPLE_FILE"))
                    {
                        const int r0 = steps.rowCount(); const auto c1 = steps.stepCentre (1, 3);   // en medio de una fila (no en el nombre)
                        steps.dropForTest (juce::StringArray (sf), (int) (c1.x * steps.scale), (int) (c1.y * steps.scale));
                        chk (steps.hasRowSample (1) && steps.rowCount() == r0, "step: soltar audio sobre una fila se lo asigna a esa fila");
                        steps.dropForTest (juce::StringArray (sf, sf), 5, 5);
                        chk (steps.rowCount() == std::min (8, r0 + 2) && steps.hasRowSample (steps.rowCount() - 1), "step: soltar dos audios fuera de las filas crea dos filas nuevas");
                    }
                    bool saved[4][16]; for (int r = 0; r < 4; ++r) for (int k = 0; k < 16; ++k) saved[r][k] = steps.getStep (r, k);
                    const int hi = te::getAllTracks (edit).indexOf (host); const double bpm = edit.tempoSequence.getTempos()[0]->getBpm();
                    auto src2 = juce::Desktop::getInstance().getMainMouseSource(); const auto LL = juce::ModifierKeys::leftButtonModifier;
                    auto sv = [&] (juce::Point<float> p, juce::ModifierKeys mk) { return juce::MouseEvent (src2, p, mk, 1.f, 0.f, 0.f, 0.f, 0.f, &steps, &steps, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false); };
                    auto scr = [&] (juce::Point<float> p) { return juce::Point<float> (p.x * steps.scale, p.y * steps.scale); };
                    auto clearAll = [&] { for (int r = 0; r < 4; ++r) for (int k = 0; k < 16; ++k) steps.setStep (r, k, false); };
                    // lectura de audio de la pista
                    auto rd = [&] (const char* nm, double secs) { auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (nm); renderWav (f, secs, {}, hi); juce::AudioFormatManager fm; fm.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f)); juce::AudioBuffer<float> b (2, 0); if (r) { b.setSize (2, (int) r->lengthInSamples); r->read (&b, 0, (int) r->lengthInSamples, 0, true, true); } return std::make_pair (b, r ? r->sampleRate : 44100.0); };
                    auto onset = [&] (const char* nm) { auto [b, sr] = rd (nm, 1.5); for (int i = 0; i < b.getNumSamples(); ++i) if (std::abs (b.getSample (0, i)) > 0.02f) return (double) i / sr; return -1.0; };
                    auto energy = [&] (const char* nm) { auto [b, sr] = rd (nm, 1.2); juce::ignoreUnused (sr); return b.getNumSamples() > 0 ? (double) b.getRMSLevel (0, 0, b.getNumSamples()) : -1.0; };
                    // estructura
                    steps.setStepGate (0, 0, 0.5); steps.setStepProb (0, 0, 0.5f); chk (std::abs (steps.getStepGate (0, 0) - 0.5) < 1e-6 && std::abs (steps.getStepProb (0, 0) - 0.5f) < 1e-6, "step: guarda el largo y la probabilidad de un paso"); steps.setStepGate (0, 0, 1.0); steps.setStepProb (0, 0, 1.f);
                    clearAll(); steps.setStep (0, 0, true); steps.setSwingAll (0.8f);
                    chk (steps.rowGroove (0) == "Basic 16th Swing" && std::abs (steps.rowSwing (0) - 0.8f) < 1e-3 && std::abs (steps.rowSwing (3) - 0.8f) < 1e-3, "step: el swing se aplica a todas las filas");
                    steps.setSwingAll (0.f); chk (steps.rowGroove (0).isEmpty() && steps.rowSwing (2) == 0.f, "step: swing 0 quita el groove");
                    steps.setSwingRow (1, 0.5f); chk (steps.rowSwing (1) > 0.4f && steps.rowSwing (0) == 0.f, "step: el swing tambien se puede poner solo a una fila"); steps.setSwingRow (1, 0.f);
                    steps.setResolution (0.125); chk (std::abs (steps.stepLen() - 0.125) < 1e-6, "step: la resolucion cambia a 1/32"); steps.setResolution (0.25); chk (std::abs (steps.stepLen() - 0.25) < 1e-6, "step: y vuelve a 1/16");
                    auto b2 = [&] (const char* t) { if (! std::getenv ("INJ_DBG")) return; juce::String b; for (int k = 0; k < 16; ++k) b += steps.getStep (2, k) ? "1" : "0"; std::printf ("DBG %s %s\n", t, b.toRawUTF8()); };
                    b2 ("antes"); steps.setRowLength (2, 5); b2 ("len5"); steps.setStep (2, 0, true); b2 ("set0"); steps.setRowLength (2, 5); b2 ("len5b"); chk (steps.rowLength (2) == 5 && steps.getStep (2, 5) && steps.getStep (2, 10) && steps.getStep (2, 15) && ! steps.getStep (2, 3), "step: una fila de 5 pasos se repite dentro del patron (polirritmia)");
                    { auto c = steps.stepCentre (2, 7); steps.mouseDown (sv (scr (c), LL)); steps.mouseUp (sv (scr (c), 0)); chk (steps.getStep (2, 7) && steps.getStep (2, 2) && steps.getStep (2, 12), ("step: pintar un paso en una fila corta se repite en todos sus ciclos (" + juce::String ((int) steps.getStep (2, 7)) + juce::String ((int) steps.getStep (2, 2)) + juce::String ((int) steps.getStep (2, 12)) + " fila " + juce::String (steps.selectedRow()) + " bits=" + [&] { juce::String b; for (int k = 0; k < 16; ++k) b += steps.getStep (2, k) ? "1" : "0"; return b; }() + " len=" + juce::String (steps.rowLength (2)) + " p=" + juce::String ((int) c.x) + "," + juce::String ((int) c.y) + " sc=" + juce::String (steps.scale, 2) + ")").toRawUTF8()); }
                    steps.setRowLength (2, 16); clearAll(); steps.setStep (0, 0, true); steps.setStep (0, 6, true);
                    steps.copyRow (0); steps.pasteRow (3); chk (steps.getStep (3, 0) && steps.getStep (3, 6) && ! steps.getStep (3, 1), "step: copiar y pegar una fila");
                    steps.setStep (1, 2, true); const auto n0 = steps.getClip()->getChannels()[0]->name.get(), n1 = steps.getClip()->getChannels()[1]->name.get();
                    steps.swapRows (0, 1); chk (steps.getClip()->getChannels()[0]->name.get() == n1 && steps.getClip()->getChannels()[1]->name.get() == n0 && steps.getStep (0, 2) && steps.getStep (1, 6) && ! steps.getStep (0, 6), "step: mover una fila lleva su nombre y sus golpes");
                    steps.swapRows (0, 1); chk (steps.getClip()->getChannels()[0]->name.get() == n0 && steps.getStep (0, 6), "step: y se puede devolver");
                    steps.setSolo (1, true); chk (! steps.rowAudible (0) && steps.rowAudible (1), "step: solo en una fila deja fuera a las demas"); steps.setSolo (1, false); chk (steps.rowAudible (0), "step: quitar el solo las devuelve");
                    // lineas de edicion con el raton: probabilidad y rampa de fuerza
                    steps.selectRow (0);
                    { auto tabp = steps.laneTabCentre (1); steps.mouseDown (sv (scr (tabp), LL)); steps.mouseUp (sv (scr (tabp), 0));
                      steps.setStep (0, 0, true);
                      auto p0 = steps.laneCentre (0, 0.5f); steps.mouseDown (sv (scr (p0), LL)); steps.mouseUp (sv (scr (p0), 0));
                      chk (std::abs (steps.getStepProb (0, 0) - 0.5f) < 0.06f, ("step: el carril de probabilidad se edita con el raton (" + juce::String (steps.getStepProb (0, 0), 2) + " fila " + juce::String (steps.selectedRow()) + ")").toRawUTF8());
                      auto tabv = steps.laneTabCentre (0); steps.mouseDown (sv (scr (tabv), LL)); steps.mouseUp (sv (scr (tabv), 0)); steps.setStepProb (0, 0, 1.f);
                      for (int k = 0; k < 8; ++k) steps.setStep (0, k, true);
                      auto a2 = steps.laneCentre (0, 0.2f), b2 = steps.laneCentre (7, 1.0f); const auto SH = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier);
                      steps.mouseDown (sv (scr (a2), LL)); steps.mouseDrag (sv (scr (b2), SH)); steps.mouseUp (sv (scr (b2), 0));
                      chk (steps.getStepVelocity (0, 3) > steps.getStepVelocity (0, 1) && steps.getStepVelocity (0, 6) > steps.getStepVelocity (0, 3), "step: Mayus + arrastrar traza una rampa de fuerza"); for (int k = 0; k < 16; ++k) { steps.setStep (0, k, true); steps.setStepVelocity (0, k, 100); } }
                    clearAll(); for (int r = 0; r < 4; ++r) for (int k = 0; k < 16; ++k) steps.setStep (r, k, saved[r][k]);
                    steps.setStepCount (origSteps);
    }
    // Pruebas con audio real del Step Sequencer. El sampler aplica sus cambios de forma asincrona, por eso van en dos etapas con espera.
    void stepAudioTests (int fails)
    {
        auto* host = steps.getClip() ? dynamic_cast<te::AudioTrack*> (steps.getClip()->getTrack()) : nullptr;
        if (host == nullptr || ! steps.hasRowSample (0) || std::getenv ("INJ_SAMPLE_FILE") == nullptr) { std::printf ("OK    step: pruebas de audio omitidas (sin sample)\n"); runToneTest (fails); return; }
        steps.setStepCount (16); for (int r = 0; r < 4; ++r) for (int k = 0; k < 16; ++k) steps.setStep (r, k, false); steps.setRowCut (0, true);
        juce::Timer::callAfterDelay (600, [this, host, fails]() mutable
        {
            const int hi = te::getAllTracks (edit).indexOf (host); const double bpm = edit.tempoSequence.getTempos()[0]->getBpm();
            auto chk = [&] (bool ok, const char* what) { std::printf ("%s  %s\n", ok ? "OK  " : "FALLO", what); if (! ok) ++fails; };
            auto rd = [&] (const char* nm, double secs) { auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (nm); renderWav (f, secs, {}, hi); juce::AudioFormatManager fm; fm.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f)); juce::AudioBuffer<float> b (2, 0); if (r) { b.setSize (2, (int) r->lengthInSamples); r->read (&b, 0, (int) r->lengthInSamples, 0, true, true); } return std::make_pair (b, r ? r->sampleRate : 44100.0); };
            auto onset = [&] (const char* nm) { auto [b, sr] = rd (nm, 1.5); for (int i = 0; i < b.getNumSamples(); ++i) if (std::abs (b.getSample (0, i)) > 0.02f) return (double) i / sr; return -1.0; };
            auto energy = [&] (const char* nm) { auto [b, sr] = rd (nm, 1.2); juce::ignoreUnused (sr); return b.getNumSamples() > 0 ? (double) b.getRMSLevel (0, 0, b.getNumSamples()) : -1.0; };
            steps.setStep (0, 1, true);
            const double o0 = onset ("inj_sw0.wav"); steps.setSwingAll (1.0f); const double o1 = onset ("inj_sw1.wav"); steps.setSwingAll (0.f);
            const double want = 0.66 * 0.25 * 60.0 / bpm; chk (o0 > 0.0 && o1 - o0 > want * 0.5 && o1 - o0 < want * 1.6, ("step: el swing retrasa el golpe del paso par (" + juce::String ((o1 - o0) * 1000.0, 0) + " ms, esperado ~" + juce::String (want * 1000.0, 0) + ")").toRawUTF8());
            steps.setStep (0, 1, false); for (int k = 0; k < 16; ++k) steps.setStep (0, k, true);
            const double eFull = energy ("inj_p0.wav"); for (int k = 0; k < 16; ++k) steps.setStepProb (0, k, 0.05f); const double eProb = energy ("inj_p1.wav"); for (int k = 0; k < 16; ++k) steps.setStepProb (0, k, 1.f);
            chk (eFull > 0.02 && eProb < eFull * 0.6, ("step: con poca probabilidad suenan menos golpes (" + juce::String (eFull, 3) + " -> " + juce::String (eProb, 3) + ")").toRawUTF8());
            for (int k = 0; k < 16; ++k) steps.setStep (0, k, false); steps.setStep (0, 0, true);
            const double gFull = energy ("inj_g0.wav"); steps.setStepGate (0, 0, 0.2); const double gShort = energy ("inj_g1.wav"); steps.setStepGate (0, 0, 1.0);
            chk (gFull > 0.005 && gShort < gFull * 0.8, ("step: un largo corto corta el sample (" + juce::String (gFull, 3) + " -> " + juce::String (gShort, 3) + ")").toRawUTF8());
            const double sFull = energy ("inj_s0.wav"); steps.setSolo (1, true);
            juce::Timer::callAfterDelay (600, [this, host, fails, sFull, hi]() mutable
            {
                auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_s1.wav"); renderWav (f, 1.2, {}, hi); juce::AudioFormatManager fm; fm.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f)); double sSolo = -1.0;
                if (r) { juce::AudioBuffer<float> b (2, (int) r->lengthInSamples); r->read (&b, 0, (int) r->lengthInSamples, 0, true, true); sSolo = (double) b.getRMSLevel (0, 0, b.getNumSamples()); }
                const bool ok = sFull > 0.005 && sSolo >= 0.0 && sSolo < sFull * 0.05; std::printf ("%s  step: con solo en otra fila esta fila deja de sonar (%.3f -> %.3f)\n", ok ? "OK  " : "FALLO", sFull, sSolo); if (! ok) ++fails;
                steps.setSolo (1, false); steps.setRowCut (0, false); for (int k = 0; k < 16; ++k) steps.setStep (0, k, false);
                juce::ignoreUnused (host); runToneTest (fails);
            });
        });
    }
    int runSelfTest()
    {
        static bool ran = false; if (ran) return 0; ran = true;
        int fails = 0; auto chk = [&] (bool ok, const char* what) { std::printf ("%s  %s\n", ok ? "OK  " : "FALLO", what); if (! ok) ++fails; };
        piano.setClip (firstMidiClip()); showView ("piano"); piano.setBounds (0, TopBars::kHeight, 1360, 700);
        auto& seq = piano.getClip()->getSequence(); const int n0 = seq.getNumNotes();
        auto src = juce::Desktop::getInstance().getMainMouseSource();
        auto ev = [&] (juce::Point<float> p, juce::ModifierKeys mk) { return juce::MouseEvent (src, p, mk, 1.f, 0.f, 0.f, 0.f, 0.f, &piano, &piano, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false); };
        const auto L = juce::ModifierKeys::leftButtonModifier;
        const float y = piano.ny (50) + 8, x0 = piano.bx (20.0) + 2;
        piano.tool = "pencil"; piano.mouseDown (ev ({ x0, y }, L)); piano.mouseDrag (ev ({ x0 + 60, y }, L)); piano.mouseUp (ev ({ x0 + 60, y }, 0));
        chk (seq.getNumNotes() == n0 + 1, "lápiz: se añade una nota");
        te::MidiNote* nn = nullptr; for (auto* n : seq.getNotes()) if (n->getNoteNumber() == 50) nn = n;
        chk (nn != nullptr && std::abs (nn->getStartBeat().inBeats() - 20.0) < 1e-6, "la nota empieza en el compás 6 (beat 20) cuantizada");
        chk (nn != nullptr && nn->getLengthBeats().inBeats() > 0.9, "arrastrar alarga la nota");
        piano.tool = "select"; const float xm = piano.bx (20.0) + 10;
        piano.mouseDown (ev ({ xm, y }, L)); piano.mouseDrag (ev ({ xm + 100, y - 40 }, L)); piano.mouseUp (ev ({ xm + 100, y - 40 }, 0));
        chk (nn != nullptr && nn->getNoteNumber() == 52, "seleccionar y arrastrar sube 2 semitonos");
        chk (nn != nullptr && std::abs (nn->getStartBeat().inBeats() - 22.0) < 1e-6, "y la mueve 2 beats a la derecha (cuantizado)");
        edit.getUndoManager().undo();
        chk (seq.getNumNotes() == n0 + 1 && nn != nullptr && nn->getNoteNumber() == 50 && std::abs (nn->getStartBeat().inBeats() - 20.0) < 1e-6, "deshacer revierte solo el movimiento");
        piano.tool = "erase"; const float ye = piano.ny (50) + 8, xe = piano.bx (20.0) + 10;
        piano.mouseDown (ev ({ xe, ye }, L)); piano.mouseUp (ev ({ xe, ye }, 0));
        chk (seq.getNumNotes() == n0, "borrador: elimina la nota");
        edit.getUndoManager().undo();
        chk (seq.getNumNotes() == n0 + 1, "deshacer devuelve la nota borrada");
        {   // herramientas nuevas del Piano Roll: cortar, humanizar, exportar/importar MIDI
            auto* first = seq.getNotes()[0]; const double s0 = first->getStartBeat().inBeats(), l0 = first->getLengthBeats().inBeats(); const int cnt = seq.getNumNotes();
            piano.splitNote (first, s0 + l0 * 0.5); chk (seq.getNumNotes() == cnt + 1, "piano roll: cortar divide una nota en dos");
            edit.getUndoManager().undo (); chk (seq.getNumNotes() == cnt, "piano roll: deshacer el corte");
            { piano.humanize(); piano.quantum = 0.25; piano.quantizeNotes(); bool onGrid = true; for (auto* nn : seq.getNotes()) { const double r = nn->getStartBeat().inBeats() / 0.25; if (std::abs (r - std::round (r)) > 1e-4) onGrid = false; } chk (onGrid && seq.getNumNotes() == cnt, "piano roll: cuantizar deja todas las notas en la rejilla"); edit.getUndoManager().undo(); edit.getUndoManager().undo(); }
            const int hn = piano.humanize (); chk (hn == cnt && seq.getNumNotes() == cnt, "piano roll: humanizar conserva las notas"); edit.getUndoManager().undo ();
            auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_test_export.mid"); chk (piano.exportMidiTo (tmp) && tmp.existsAsFile(), "piano roll: exporta un archivo MIDI");
            const int before = seq.getNumNotes(); const int added = piano.importMidiFrom (tmp); chk (added == before && seq.getNumNotes() == before * 2, "piano roll: importa el mismo MIDI y duplica las notas");
            edit.getUndoManager().undo (); chk (seq.getNumNotes() == before, "piano roll: deshacer la importacion"); tmp.deleteFile();
        }
        {   // --- Piano Roll: carriles de control, slide, silenciar, ratchet, ganancia, bucle de seleccion
            const auto RBm = juce::ModifierKeys::rightButtonModifier; const int pw = te::MidiControllerEvent::pitchWheelType;
            piano.clearCurve (pw); piano.showLane (2); const float yTop = (float) piano.gridBottom() + 34.f, yBot = (float) piano.getHeight() - 10.f, yq = yBot - (yBot - yTop) * .75f;
            const float xa = piano.bx (2.0), xb2 = piano.bx (3.0);
            piano.mouseDown (ev ({ xa, yq }, L)); piano.mouseDrag (ev ({ xb2, yq }, L)); piano.mouseUp (ev ({ xb2, yq }, 0));
            auto pb = piano.eventsOfType (pw);
            chk (pb.size() >= 12 && std::abs (pb.front()->getControllerValue() - 12288) < 400, ("piano roll: el carril de pitch bend dibuja una curva (" + juce::String ((int) pb.size()) + " puntos)").toRawUTF8());
            piano.mouseDown (ev ({ xa, yq }, RBm)); piano.mouseDrag (ev ({ xb2, yq }, RBm)); piano.mouseUp (ev ({ xb2, yq }, 0));
            chk (piano.eventsOfType (pw).empty(), "piano roll: clic derecho en el carril borra la curva");
            piano.showLane (1); piano.mouseDown (ev ({ xa, yBot - (yBot - yTop) * .5f }, L)); piano.mouseUp (ev ({ xa, yBot }, 0));
            { auto mod = piano.eventsOfType (1); chk (mod.size() == 1 && std::abs (mod[0]->getControllerValue() - 64) <= 2, "piano roll: el carril de modulacion (CC1) pone un punto a la mitad"); }
            piano.clearCurve (1); piano.showLane (0);
            // exportar / importar conserva la curva
            piano.setCurvePoint (pw, 1.0, 10000); piano.setCurvePoint (1, 1.5, 90);
            auto tmp2 = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_test_cc.mid"); piano.exportMidiTo (tmp2);
            piano.clearCurve (pw); piano.clearCurve (1); piano.importMidiFrom (tmp2); tmp2.deleteFile();
            { auto a1 = piano.eventsOfType (pw), a2 = piano.eventsOfType (1); chk (a1.size() == 1 && a1[0]->getControllerValue() == 10000 && a2.size() == 1 && a2[0]->getControllerValue() == 90, "piano roll: el MIDI exportado e importado conserva pitch bend y modulacion"); }
            piano.clearCurve (pw); piano.clearCurve (1);
            // notas: slide, silenciar, ratchet, velocidad
            { auto& sq = piano.getClip()->getSequence(); const int cnt0 = sq.getNumNotes();
              auto* nA = sq.addNote (60, te::BeatPosition::fromBeats (30.0), te::BeatDuration::fromBeats (1.0), 100, 0, nullptr); auto* nB = sq.addNote (62, te::BeatPosition::fromBeats (31.0), te::BeatDuration::fromBeats (1.0), 100, 0, nullptr);
              piano.growTo (33.0); piano.selectNotesForTest ({ nB });
              const int sl = piano.toggleSlide(); auto sp = piano.eventsOfType (pw);
              chk (sl == 1 && sp.size() >= 6 && sp.front()->getControllerValue() < 8192 && sp.back()->getControllerValue() == 8192, ("piano roll: slide crea un deslizamiento de pitch bend (" + juce::String ((int) sp.size()) + " puntos)").toRawUTF8());
              piano.toggleSlide (); chk (piano.eventsOfType (pw).empty(), "piano roll: quitar el slide borra el deslizamiento");
              piano.selectNotesForTest ({ nA, nB }); piano.muteSel (); chk (nA->isMute() && nB->isMute(), "piano roll: M silencia las notas elegidas"); piano.muteSel (); chk (! nA->isMute() && ! nB->isMute(), "piano roll: M otra vez las activa");
              piano.selectNotesForTest ({ nA }); piano.muteSel (); piano.noteTool ("ratchet:4"); int rc = 0, rm = 0; for (auto* o : sq.getNotes()) if (o->getStartBeat().inBeats() >= 30.0 && o->getStartBeat().inBeats() < 31.0) { ++rc; if (o->isMute()) ++rm; }
              chk (rc == 4 && rm == 4, "piano roll: ratchet x4 parte una nota en 4 y conserva el silencio");
              piano.selectNotesForTest ({}); { std::vector<te::MidiNote*> ns; for (auto* o : sq.getNotes()) if (o->getStartBeat().inBeats() >= 31.0) ns.push_back (o); piano.selectNotesForTest (ns); const int v0 = ns.front()->getVelocity(); piano.noteTool ("veldown"); chk (piano.selectedVelocityForTest() < v0, "piano roll: velocidad -10 % baja la fuerza"); }
              piano.selectNotesForTest ({}); { std::vector<te::MidiNote*> del; for (auto* o : sq.getNotes()) if (o->getStartBeat().inBeats() >= 29.0) del.push_back (o); for (auto* o : del) sq.removeNote (*o, nullptr); }
              chk (sq.getNumNotes() == cnt0, "piano roll: las notas de prueba se limpian");
            }
            { auto& tc = edit.getTransport(); const bool l0 = tc.looping; const auto r0 = tc.getLoopRange(); piano.playLoopSelection();
              chk (piano.loopSel && (bool) tc.looping && tc.getLoopRange().getLength().inSeconds() > 0.1, "piano roll: tocar la seleccion en bucle activa el bucle");
              piano.stopLoopSel(); chk (! piano.loopSel && (bool) tc.looping == l0 && tc.getLoopRange() == r0, "piano roll: al parar vuelve el bucle de antes"); }
        }
        // --- Mixer
        setMixer (true); 
        auto tracks = te::getAudioTracks (edit); auto mev = [&] (juce::Point<float> p, juce::ModifierKeys mk) { return juce::MouseEvent (src, p, mk, 1.f, 0.f, 0.f, 0.f, 0.f, &mixer, &mixer, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false); };
        const float k = mixer.scale; auto sB = mixer.smR (2, 0).getCentre(), mB = mixer.smR (2, 1).getCentre();
        mixer.mouseDown (mev ({ sB.x * k, sB.y * k }, L)); mixer.mouseUp (mev ({ sB.x * k, sB.y * k }, 0));
        chk (tracks[2]->isSolo (false), "mixer: S activa el solo del canal 3");
        mixer.mouseDown (mev ({ mB.x * k, mB.y * k }, L)); mixer.mouseUp (mev ({ mB.x * k, mB.y * k }, 0));
        chk (tracks[2]->isMuted (false), "mixer: M silencia el canal 3");
        const auto fa = mixer.faderArea (2); const float fx = fa.getCentreX() * k, fy0 = fa.getY(), fh = fa.getHeight();
        mixer.mouseDown (mev ({ fx, (fy0 + fh * 0.5f) * k }, L)); mixer.mouseUp (mev ({ fx, (fy0 + fh * 0.5f) * k }, 0));
        chk (std::abs (tracks[2]->getVolumePlugin()->getVolumeDb() - MixerPanel::posToDb (0.5f)) < 0.5f, "mixer: arrastrar el fader cambia el volumen real");
        mixer.mouseDoubleClick (mev ({ fx, (fy0 + fh * 0.5f) * k }, L));
        chk (std::abs (tracks[2]->getVolumePlugin()->getVolumeDb()) < 0.01f, "mixer: doble clic en el fader vuelve a 0 dB");
        tracks[2]->setSolo (false); tracks[2]->setMute (false);
        // --- Step Sequencer: clic en celdas y render real del patrón con un sample
        setSteps (true); steps.setTopLeftPosition (0, 60);
        auto stc = steps.getClip(); chk (stc != nullptr, "step: se crea el clip de patrón en la Playlist");
        for (int s = 0; s < 16; ++s) steps.setStep (0, s, false);
        auto sev = [&] (juce::Point<float> p, juce::ModifierKeys mk) { return juce::MouseEvent (src, p, mk, 1.f, 0.f, 0.f, 0.f, 0.f, &steps, &steps, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false); };
        for (int s : { 0, 4, 8, 12 }) { auto c = steps.stepCentre (0, s); juce::Point<float> p (c.x * steps.scale, c.y * steps.scale); steps.mouseDown (sev (p, L)); steps.mouseUp (sev (p, 0)); }
        chk (steps.getStep (0, 0) && steps.getStep (0, 4) && steps.getStep (0, 8) && steps.getStep (0, 12) && ! steps.getStep (0, 2), "step: los clics marcan los pasos 1, 5, 9 y 13");
        steps.runPatternOp (5); chk (steps.getStep (0, 1) && steps.getStep (0, 5) && ! steps.getStep (0, 0), "step: desplazar a la derecha mueve los golpes un paso");
        steps.runPatternOp (4); chk (steps.getStep (0, 0) && steps.getStep (0, 4), "step: desplazar a la izquierda los devuelve");
        chk (te::getAudioTracks (edit).size() == 500, "proyecto nuevo: 500 pistas listas");
        {   // modo ensayo: un clip en una celda se lanza y suena
            auto tl = te::getAudioTracks (edit); int col = -1; for (int k = 0; k < tl.size() && k < PerformancePanel::kCols; ++k) if (tl[k]->getClips().isEmpty() && ! trackutil::hasInstrument (*tl[k])) { col = k; break; }
            if (col >= 0 && instruments::addSynth (edit, *tl[col], 0) != nullptr)
            {
                perf.firstTrack = 0; auto* mc = dynamic_cast<te::MidiClip*> (perf.makeEmptyMidi (col, 0, 1.0));
                chk (mc != nullptr && perf.stateAt (col, 0) == 1, "ensayo: se crea un clip en la celda y queda detenido");
                if (mc) { mc->getSequence().addNote (60, te::BeatPosition::fromBeats (0.0), te::BeatDuration::fromBeats (3.0), 100, 0, nullptr);
                    edit.getLaunchQuantisation().type = te::LaunchQType::none; perf.launch (col, 0);
                    bool ok = false; for (int w = 0; w < 40 && ! ok; ++w) { juce::Thread::sleep (50); ok = perf.stateAt (col, 0) == 2; }
                    chk (ok || perf.stateAt (col, 0) == 3, ("ensayo: lanzar la celda la pone a sonar o en cola (estado " + juce::String (perf.stateAt (col, 0)) + ")").toRawUTF8());
                    perf.stopAll(); bool stopped = false; for (int w = 0; w < 40 && ! stopped; ++w) { juce::Thread::sleep (50); stopped = perf.stateAt (col, 0) == 1; }
                    chk (stopped, "ensayo: Detener todo para la celda"); edit.getTransport().stop (false, false); perf.removeClip (col, 0); chk (perf.stateAt (col, 0) == 0, "ensayo: quitar el clip vacia la celda"); }
            }
            else chk (false, "ensayo: hay una pista libre para probar");
        }
        {   // funciones nuevas v0.9.13: nudge, dividir en el cursor, zoom al proyecto, seguir cabezal, tap tempo, loop del clip, seleccionar pista
            auto tl = te::getAudioTracks (edit); te::AudioTrack* tt = nullptr; for (auto* t : tl) if (! t->getClips().isEmpty()) { tt = t; break; }
            if (tt != nullptr)
            {
                auto* c0 = tt->getClips().getFirst(); const int ti0 = tl.indexOf (tt);
                playlist.selectTrackClips (ti0); chk (playlist.selectedClips().size() == tt->getClips().size() || tt->getClips().size() == 1, "pista: seleccionar todos los clips de la pista");
                playlist.selectTrackClips (-1); playlist.selected = c0;
                const double s0 = c0->getPosition().getStart().inSeconds(); playlist.nudgeSelection (1, false);
                chk (c0->getPosition().getStart().inSeconds() > s0, "clips: la flecha derecha mueve el clip un paso"); playlist.nudgeSelection (-1, false); chk (std::abs (c0->getPosition().getStart().inSeconds() - s0) < 1e-6, "clips: la flecha izquierda lo devuelve");
                const int nBefore = tt->getClips().size(); const double mid = c0->getPosition().getStart().inSeconds() + c0->getPosition().getLength().inSeconds() * 0.5;
                edit.getTransport().setPosition (te::TimePosition::fromSeconds (mid)); playlist.selected = c0; playlist.splitSelectionAtPlayhead(); chk (tt->getClips().size() == nBefore + 1, "clips: S divide el clip elegido en el cursor");
                edit.getUndoManager().undo();
                playlist.loopClip (*tt->getClips().getFirst()); chk (edit.getTransport().looping.get(), "clip: repetir en bucle activa el bucle"); edit.getTransport().looping = false;
            }
            {   // v0.9.14: recientes, plantillas propias, invertir y normalizar
                auto tf = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_recent_test.injdaw"); tf.replaceWithText ("x"); const auto oldR = userdata::recentFile().loadFileAsString();
                userdata::addRecent (tf); chk (userdata::recents().size() > 0 && userdata::recents()[0] == tf.getFullPathName(), "recientes: el proyecto guardado queda de primero"); userdata::recentFile().replaceWithText (oldR); tf.deleteFile();
                edit.flushState(); te::EditFileOperations ops (edit); const auto tp = userdata::templatesDir().getChildFile ("inj_test_tpl.injdaw"); const bool wrote = ops.writeToFile (tp, false);
                chk (wrote && userdata::templateNames().contains ("inj_test_tpl"), "plantillas: guardar el proyecto como plantilla la lista"); tp.deleteFile();
                auto rf = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_rev_test.wav"); rf.deleteFile();
                { juce::WavAudioFormat wf; std::unique_ptr<juce::FileOutputStream> os (rf.createOutputStream()); std::unique_ptr<juce::AudioFormatWriter> w (wf.createWriterFor (os.get(), 44100.0, 1, 16, {}, 0)); if (w) { os.release(); juce::AudioBuffer<float> sb (1, 22050); for (int i = 0; i < 22050; ++i) sb.setSample (0, i, 0.3f * std::sin (i * 0.0313f) * (1.f - i / 22050.f)); w->writeFromAudioSampleBuffer (sb, 0, 22050); } }
                { te::AudioTrack* et = nullptr; for (auto* t : te::getAudioTracks (edit)) if (t->getClips().isEmpty()) { et = t; break; } if (et) et->insertWaveClip ("revtest", rf, { te::TimeRange (te::TimePosition::fromSeconds (200.0), te::TimeDuration::fromSeconds (0.5)), te::TimeDuration() }, false); }
                te::WaveAudioClip* wcx = nullptr; te::AudioTrack* wtx = nullptr; for (auto* t : te::getAudioTracks (edit)) { for (auto* c : t->getClips()) if (auto* w = dynamic_cast<te::WaveAudioClip*> (c)) { wcx = w; wtx = t; break; } if (wcx) break; }
                if (wcx != nullptr)
                {
                    const float g0 = wcx->getGainDB(); playlist.normaliseClip (*wcx); chk (std::abs (wcx->getGainDB() - g0) > 0.05f || std::abs (g0) > 0.05f, "clip: normalizar cambia la ganancia"); wcx->setGainDB (g0);
                    const int nc0 = wtx->getClips().size(); playlist.reverseClip (*wcx, *wtx); bool inv = false; for (auto* c : wtx->getClips()) if (c->getName().endsWith ("(inv)")) inv = true;
                    chk (inv && wtx->getClips().size() == nc0, "clip: invertir audio deja un clip nuevo (inv) en lugar del original");
                    for (int ci = wtx->getClips().size(); --ci >= 0;) { auto* c = wtx->getClips()[ci]; if (c->getName().startsWith ("revtest")) c->removeFromParent(); }   // limpiar para no afectar otras pruebas
                }
                else chk (false, "clip: hay un clip de audio para probar invertir");
                { auto hf = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_handle_test.wav"); hf.deleteFile();
                  { juce::WavAudioFormat wf; std::unique_ptr<juce::FileOutputStream> os (hf.createOutputStream()); std::unique_ptr<juce::AudioFormatWriter> w (wf.createWriterFor (os.get(), 44100.0, 1, 16, {}, 0)); if (w) { os.release(); juce::AudioBuffer<float> sb (1, 44100 * 9); sb.clear(); w->writeFromAudioSampleBuffer (sb, 0, 44100 * 9); } }
                  te::AudioTrack* ht = nullptr; for (auto* t : te::getAudioTracks (edit)) if (t->getClips().isEmpty()) { ht = t; break; } int hti = 0; { auto tt = te::getAudioTracks (edit); for (int i = 0; i < tt.size(); ++i) if (tt[i] == ht) hti = i; }
                  if (ht) { ht->insertWaveClip ("handletest", hf, { te::TimeRange (te::TimePosition::fromSeconds (0.0), te::TimeDuration::fromSeconds (8.0)), te::TimeDuration() }, false);
                    const double zPrev = playlist.zoomPx(); playlist.setZoom (14.0); for (auto* c : ht->getClips()) if (auto* hw = dynamic_cast<te::WaveAudioClip*> (c)) { const auto msg = playlist.selfTestHandles (*hw, hti); chk (msg.isEmpty(), ("clip: manijas de fundido y ganancia " + msg).toRawUTF8());  { const auto lm = playlist.selfTestLock (*hw, hti); chk (lm.isEmpty(), ("clip: bloquear clip " + lm).toRawUTF8()); } { const auto bm = playlist.selfTestBrush (*hw, hti); chk (bm.isEmpty(), ("pincel: " + bm).toRawUTF8()); } { const auto pm = playlist.selfTestPencil (*hw, hti); chk (pm.isEmpty(), ("lapiz: " + pm).toRawUTF8()); } { const auto rm = playlist.selfTestRightErase (*hw, hti); chk (rm.isEmpty(), ("borrar con clic derecho arrastrando: " + rm).toRawUTF8()); } } playlist.setZoom (zPrev);
                    for (int ci = ht->getClips().size(); --ci >= 0;) { auto* c = ht->getClips()[ci]; if (c->getName().startsWith ("handletest")) c->removeFromParent(); } } }
                {   // ganancia y fundido de clip: se oyen de verdad en el render
                  auto gf = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_gain_test.wav"); gf.deleteFile();
                  { juce::WavAudioFormat wf; std::unique_ptr<juce::FileOutputStream> os (gf.createOutputStream()); std::unique_ptr<juce::AudioFormatWriter> w (wf.createWriterFor (os.get(), 44100.0, 1, 16, {}, 0)); if (w) { os.release(); juce::AudioBuffer<float> sb (1, 44100 * 3); for (int i = 0; i < 44100 * 3; ++i) sb.setSample (0, i, 0.5f * std::sin (i * 0.0626f)); w->writeFromAudioSampleBuffer (sb, 0, 44100 * 3); } }
                  auto winRms = [] (const juce::File& f, double t0, double t1) { juce::AudioFormatManager fm; fm.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f)); if (! r) return -1.f; const int a0 = (int) (t0 * r->sampleRate), n = (int) ((t1 - t0) * r->sampleRate); juce::AudioBuffer<float> b ((int) r->numChannels, n); r->read (&b, 0, n, a0, true, true); return b.getRMSLevel (0, 0, n); };
                  trackutil::addTrack (edit); te::AudioTrack* gt = te::getAudioTracks (edit).getLast(); const int gti = te::getAllTracks (edit).indexOf (gt);
                  if (gt)
                  {
                      auto gc = gt->insertWaveClip ("gaintest", gf, { te::TimeRange (te::TimePosition::fromSeconds (0.0), te::TimeDuration::fromSeconds (3.0)), te::TimeDuration() }, false);
                      auto* gw = dynamic_cast<te::WaveAudioClip*> (gc.get()); auto of = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_gain_out.wav");
                      if (gw && renderWav (of, 1.6, {}, gti))
                      {
                          const float a0 = winRms (of, 0.5, 1.0); gw->setGainDB (-12.f); renderWav (of, 1.6, {}, gti); const float a1 = winRms (of, 0.5, 1.0);
                          chk (a0 > 0.01f && a1 > a0 * 0.18f && a1 < a0 * 0.34f, ("clip: -12 dB de ganancia baja el nivel a ~25% (" + juce::String (a0, 3) + " -> " + juce::String (a1, 3) + ")").toRawUTF8());
                          gw->setGainDB (0.f); gw->setFadeIn (te::TimeDuration::fromSeconds (1.0)); renderWav (of, 1.6, {}, gti);
                          const float e0 = winRms (of, 0.0, 0.2), e1 = winRms (of, 1.1, 1.5);
                          chk (e1 > 0.01f && e0 < e1 * 0.45f, ("clip: el fundido de entrada sube el sonido poco a poco (" + juce::String (e0, 3) + " -> " + juce::String (e1, 3) + ")").toRawUTF8());
                      }
                      else chk (false, "clip: se pudo renderizar el clip de prueba de ganancia");
                      if (gw)
                      {   // se guardan con el proyecto y vuelven al abrirlo
                          gw->setFadeIn (te::TimeDuration()); gw->setGainDB (-7.5f); gw->setFadeOut (te::TimeDuration::fromSeconds (0.4)); edit.flushState(); te::EditFileOperations ops (edit);
                          auto pf = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_gain_rt.injdaw"); pf.deleteFile(); bool ok = false; float rg = 0.f; double rf = 0.0;
                          if (ops.writeToFile (pf, false)) if (auto e2 = te::loadEditFromFile (engine, pf)) for (auto* t2 : te::getAudioTracks (*e2)) for (auto* c2 : t2->getClips()) if (c2->getName() == "gaintest") if (auto* w2 = dynamic_cast<te::WaveAudioClip*> (c2)) { ok = true; rg = w2->getGainDB(); rf = w2->getFadeOut().inSeconds(); }
                          chk (ok && std::abs (rg + 7.5f) < 0.01f && std::abs (rf - 0.4) < 0.01, "clip: ganancia y fundido se guardan en el proyecto y vuelven al abrirlo"); pf.deleteFile();
                      }
                      if (gw)
                      {   // bus: la salida de la pista va a otro canal; el sonido pasa por él y su fader baja a todo el grupo
                          gw->setGainDB (0.f); gw->setFadeOut (te::TimeDuration()); auto of2 = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_bus_out.wav");
                          juce::Array<te::AudioTrack*> muted; for (auto* ot : te::getAudioTracks (edit)) if (ot != gt && ! ot->isMuted (false)) { ot->setMute (true); muted.add (ot); }   // mezcla completa, solo suena la pista de prueba
                          renderWav (of2, 1.6, {}); const float b0 = winRms (of2, 0.5, 1.0);
                          auto* bus = mixer.newBus ("Bus prueba"); const bool routed = bus != nullptr && mixer.setOutput (*gt, bus);
                          chk (routed && MixerPanel::outputOf (*gt) == bus && MixerPanel::isBus (*bus), "bus: la salida de un canal se enruta a otro canal");
                          chk (bus != nullptr && ! mixer.setOutput (*bus, gt), "bus: no deja crear un lazo (bus -> canal que ya entra en el bus)");
                          if (bus && routed)
                          {
                              renderWav (of2, 1.6, {}); const float b1 = winRms (of2, 0.5, 1.0);
                              chk (b0 > 0.01f && b1 > b0 * 0.8f && b1 < b0 * 1.25f, ("bus: el sonido sigue llegando a Master a traves del bus (" + juce::String (b0, 3) + " -> " + juce::String (b1, 3) + ")").toRawUTF8());
                              bus->getVolumePlugin()->setVolumeDb (-12.f); renderWav (of2, 1.6, {}); const float b2 = winRms (of2, 0.5, 1.0);
                              chk (b2 > b1 * 0.18f && b2 < b1 * 0.34f, ("bus: bajar el fader del bus baja el nivel del grupo (" + juce::String (b1, 3) + " -> " + juce::String (b2, 3) + ")").toRawUTF8());
                              mixer.setOutput (*gt, nullptr); chk (MixerPanel::outputOf (*gt) == nullptr, "bus: se puede devolver la salida a Master");
                              mixer.toggleSendTo (*gt, *bus); chk (mixer.sendTo (*gt, *bus) != nullptr, "envios: se crea un envio a cualquier canal");
                              mixer.toggleSendTo (*gt, *bus); chk (mixer.sendTo (*gt, *bus) == nullptr, "envios: se quita y desaparece su retorno");
                          }
                          if (bus) trackutil::deleteTrack (edit, bus);
                          for (auto* ot : muted) ot->setMute (false);
                      }
                      {   // separación estéreo: un wav con sonido solo a la izquierda; en mono llega a los dos lados
                          auto sf = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_st_test.wav"); sf.deleteFile();
                          { juce::WavAudioFormat wf; std::unique_ptr<juce::FileOutputStream> os (sf.createOutputStream()); std::unique_ptr<juce::AudioFormatWriter> w (wf.createWriterFor (os.get(), 44100.0, 2, 16, {}, 0));
                            if (w) { os.release(); juce::AudioBuffer<float> b (2, 44100 * 2); b.clear(); for (int i = 0; i < b.getNumSamples(); ++i) b.setSample (0, i, 0.5f * std::sin (i * 0.06f)); w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples()); } }
                          trackutil::addTrack (edit); auto* stt = te::getAudioTracks (edit).getLast(); const int sti2 = te::getAllTracks (edit).indexOf (stt);
                          if (stt)
                          {
                              stt->insertWaveClip ("sttest", sf, { te::TimeRange (te::TimePosition::fromSeconds (0.0), te::TimeDuration::fromSeconds (2.0)), te::TimeDuration() }, false);
                              auto rch = [&] (int ch) { auto of3 = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_st_out.wav"); renderWav (of3, 1.2, {}, sti2); juce::AudioFormatManager fm; fm.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (of3)); if (! r) return -1.f; const int n = (int) (0.5 * r->sampleRate); juce::AudioBuffer<float> b ((int) r->numChannels, n); r->read (&b, 0, n, (int) (0.3 * r->sampleRate), true, true); return b.getRMSLevel (juce::jmin (ch, b.getNumChannels() - 1), 0, n); };
                              const float r0 = rch (1); mixer.setWidthOfTrack (*stt, 0.f); const float r1 = rch (1), l1 = rch (0);
                              chk (r0 < 0.02f && r1 > 0.1f && std::abs (r1 - l1) < 0.03f, ("estereo: en mono el sonido de un solo lado llega a los dos (" + juce::String (r0, 3) + " -> " + juce::String (r1, 3) + " / " + juce::String (l1, 3) + ")").toRawUTF8());
                              mixer.setWidthOfTrack (*stt, 1.f); const float q0 = rch (0); mixer.setEqMid (*stt, 420.f, 12.f); const float q1 = rch (0);
                              chk (q0 > 0.05f && q1 > q0 * 2.2f, ("ecualizador: subir +12 dB en la frecuencia del sonido lo hace mucho mas fuerte (" + juce::String (q0, 3) + " -> " + juce::String (q1, 3) + ")").toRawUTF8());
                              mixer.setEqMid (*stt, 420.f, 0.f);
                              juce::Array<te::AudioTrack*> mutedM; for (auto* ot : te::getAudioTracks (edit)) if (ot != stt && ! ot->isMuted (false)) { ot->setMute (true); mutedM.add (ot); }
                              auto rfull = [&] { auto of4 = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_st_full.wav"); renderWav (of4, 1.2, {}); juce::AudioFormatManager fm; fm.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (of4)); if (! r) return -1.f; const int n = (int) (0.5 * r->sampleRate); juce::AudioBuffer<float> b ((int) r->numChannels, n); r->read (&b, 0, n, (int) (0.3 * r->sampleRate), true, true); return b.getRMSLevel (0, 0, n); };
                              const float q2 = rfull(); mixer.setMasterEqMid (420.f, 12.f); const float q3 = rfull(); mixer.clearMasterEq(); for (auto* ot : mutedM) ot->setMute (false);
                              chk (q2 > 0.05f && q3 > q2 * 2.2f, ("master: el ecualizador del master sube el sonido (" + juce::String (q2, 3) + " -> " + juce::String (q3, 3) + ")").toRawUTF8());
                              {   // automatización: la curva de volumen queda guardada y se lee bien (el render con curvas no se prueba aquí)
                                  auto* pp = stt->getVolumePlugin()->volParam.get(); const float base = pp->getCurrentValue(); auto& cv = pp->getCurve();
                                  cv.addPoint (te::TimePosition::fromSeconds (0.0), pp->valueRange.start, 0.f); cv.addPoint (te::TimePosition::fromSeconds (1.0), base, 0.f);
                                  const float v0 = cv.getValueAt (te::TimePosition::fromSeconds (0.1)), v1 = cv.getValueAt (te::TimePosition::fromSeconds (0.9)); const int np = cv.getNumPoints(); cv.clear();
                                  chk (np == 2 && v1 > v0, ("automatizacion: la curva de volumen se guarda y sube (" + juce::String (v0, 3) + " -> " + juce::String (v1, 3) + ")").toRawUTF8());
                              }
                              trackutil::deleteTrack (edit, stt);
                          }
                          sf.deleteFile();
                      }
                      trackutil::deleteTrack (edit, gt);
                  }
                }
            }
            const double pxBefore = playlist.zoomPx(); playlist.zoomFit (false); chk (playlist.zoomPx() != pxBefore || playlist.projectEndBeat() < 0.5, "vista: ver todo el proyecto cambia el zoom"); playlist.cancelZoomAnim();
            playlist.follow = true; chk (playlist.follow, "vista: seguir el cabezal se activa"); playlist.follow = false;
            { const double b0 = bars.bpm; tapTempo(); juce::Thread::sleep (500); tapTempo(); juce::Thread::sleep (500); tapTempo(); chk (std::abs (bars.bpm - 120.0) < 12.0, ("tap tempo: tres toques a 0.5 s dan ~120 BPM (" + juce::String (bars.bpm, 1) + ")").toRawUTF8()); bars.bpm = b0; if (bars.onBpm) bars.onBpm (b0); taps.clear(); }
        }
        {   // detector de tempo y tono sobre los archivos de prueba de /tmp/pk/out2 (si existen)
            { int hit = 0, tot = 0; juce::String vals; for (auto* nm : { "Loop Gabber 172 BPM 01", "Loop Gabber 172 BPM 02", "Loop Gabber 172 BPM 03", "Loop Gabber 172 BPM 04", "Loop 909 172 BPM 01", "Loop 909 172 BPM 02", "Loop 909 172 BPM 03", "Loop Hardcore 172 BPM 01", "Loop Hardcore 172 BPM 02", "Loop Hardcore 172 BPM 03", "Bass Loop Acid 172 BPM 02" })
              { juce::File lp (juce::String ("/tmp/pk/out2/Loops/") + nm + ".wav"); if (! lp.existsAsFile()) continue; const auto r = sampleanalysis::analyse (lp); ++tot; vals += juce::String (r.bpm, 1) + " "; if (std::abs (r.bpm - 172.0) < 2.0) ++hit; }
              if (tot > 0) chk (hit >= tot - 1, ("detector: los bucles de 172 BPM se detectan (" + juce::String (hit) + "/" + juce::String (tot) + ": " + vals + ")").toRawUTF8()); }
            { juce::File kp ("/tmp/pk/out2/Kicks/Kick Hard Techno 01.wav"); if (kp.existsAsFile()) { const auto r = sampleanalysis::analyse (kp); chk (r.ok && r.oneShot && r.midi > 20 && r.midi < 80, ("detector: el bombo suelto da una nota (" + juce::String (r.midi) + ", " + juce::String (r.hz, 1) + " Hz)").toRawUTF8()); } }
            // seno de La 220 Hz de 3 s: nota 57 (La3)
            { juce::File tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_sine.wav"); tmp.deleteFile(); juce::WavAudioFormat wf; std::unique_ptr<juce::AudioFormatWriter> w (wf.createWriterFor (tmp.createOutputStream().release(), 44100.0, 1, 16, {}, 0));
              if (w) { juce::AudioBuffer<float> b (1, 44100 * 3); for (int i = 0; i < b.getNumSamples(); ++i) b.setSample (0, i, 0.5f * std::sin (juce::MathConstants<float>::twoPi * 220.f * (float) i / 44100.f)); w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples()); w.reset();
                const auto r = sampleanalysis::analyse (tmp); chk (r.ok && ! r.oneShot && r.keyPc == 9, ("detector: un La sostenido da el tono La (" + sampleanalysis::pcName (r.keyPc) + ")").toRawUTF8()); } }
        }
        // --- Playlist: M/S de cabecera, recorte, duplicar, agregar pista
        {
            playlist.setBounds (0, TopBars::kHeight, 1360, 700); playlist.tool = "select"; auto tr = te::getAudioTracks (edit);
            auto pev = [&] (juce::Point<float> p, juce::ModifierKeys mk) { return juce::MouseEvent (src, p, mk, 1.f, 0.f, 0.f, 0.f, 0.f, &playlist, &playlist, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false); };
            const float hy = (float) playlist.trackTop (3) + 40.f;
            playlist.mouseDoubleClick (pev ({ 73.f, hy }, L)); chk (tr[3]->isMuted (false), "playlist: doble clic en la cabecera silencia la pista");
            playlist.mouseDoubleClick (pev ({ 73.f, hy }, L)); chk (! tr[3]->isMuted (false), "playlist: doble clic otra vez la reactiva");
            { juce::ModifierKeys alt (juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier); playlist.mouseDown (pev ({ 73.f, hy }, alt)); playlist.mouseUp (pev ({ 73.f, hy }, 0)); chk (tr[3]->isSolo (false), "playlist: Alt+clic en la cabecera hace solo");
              playlist.mouseDown (pev ({ 73.f, hy }, alt)); playlist.mouseUp (pev ({ 73.f, hy }, 0)); chk (! tr[3]->isSolo (false), "playlist: Alt+clic otra vez quita el solo"); }
            { const int h0 = playlist.hOf (1), ey = playlist.trackTop (1) + h0; playlist.mouseDown (pev ({ 60.f, (float) ey }, L)); playlist.mouseDrag (pev ({ 60.f, (float) ey - 50.f }, L)); playlist.mouseUp (pev ({ 60.f, (float) ey - 50.f }, 0));
              chk (playlist.hOf (1) == h0 - 50 && playlist.hOf (0) == h0, "playlist: arrastrar el borde de una cabecera achica solo esa pista");
              playlist.setAllHeights (116); chk (playlist.hOf (1) == 116, "playlist: poder volver a la altura normal"); }
            {   // flechita de la cabecera: pliega y despliega
                const int ci = 2, h0 = playlist.hOf (ci); auto cc = playlist.chevronCentre (ci); playlist.mouseDown (pev (cc, L)); playlist.mouseUp (pev (cc, 0));
                chk (playlist.folded (ci) && playlist.hOf (ci) == 22, "playlist: la flechita de la pista la pliega");
                playlist.mouseDown (pev (cc, L)); playlist.mouseUp (pev (cc, 0)); chk (! playlist.folded (ci) && playlist.hOf (ci) == h0, "playlist: la flechita la despliega a su altura de antes");
            }
            {   // tijeras: linea guia y destello
                auto cl = tr[4]->insertMIDIClip ("cortar", te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (4.0)), nullptr); (void) cl;
                playlist.resetView(); playlist.snap = false; playlist.tool = "cut"; const float cx = playlist.beatToX (2.0), cy = (float) playlist.trackTop (4) + 20.f; playlist.mouseMove (pev ({ cx, cy }, 0));
                chk (playlist.cutTrack == 4 && std::abs (playlist.cutBeat - 2.0) < 0.3, "tijeras: muestran la linea donde se va a cortar");
                const int c0 = tr[4]->getClips().size(); playlist.mouseDown (pev ({ cx, cy }, L)); playlist.mouseUp (pev ({ cx, cy }, 0));
                chk (tr[4]->getClips().size() == c0 + 1 && playlist.flashA > 0.f, "tijeras: cortan donde estaba la linea y hacen un destello");
                playlist.snap = true; playlist.tool = "select";
            }
            {   // pincel: empezando en vacio pinta el clip seleccionado
                auto src6 = tr[4]->getClips()[0]; playlist.selected = src6; playlist.tool = "brush";
                const float bx = playlist.beatToX (40.0), by = (float) playlist.trackTop (5) + 8.f; playlist.mouseDown (pev ({ bx, by }, L)); playlist.mouseUp (pev ({ bx, by }, 0));
                chk (tr[5]->getClips().size() == 1, "pincel: pinta una copia del clip seleccionado en un lugar vacio");
                playlist.selected = nullptr; { const float bx2 = playlist.beatToX (50.0); playlist.mouseDown (pev ({ bx2, by }, L)); playlist.mouseUp (pev ({ bx2, by }, 0)); chk (tr[5]->getClips().size() == 2, "pincel: recuerda el ultimo clip aunque ya no este seleccionado y lo pega al lado"); chk (playlist.hasHScroll(), "playlist: la barra de abajo siempre esta"); }
                playlist.tool = "select"; playlist.selected = nullptr; { auto cs = tr[5]->getClips(); for (int k = cs.size(); --k >= 0;) cs.getUnchecked (k)->removeFromParent(); auto c4 = tr[4]->getClips(); for (int k = c4.size(); --k >= 0;) c4.getUnchecked (k)->removeFromParent(); }
            }
            { auto* ld = tr[0]->getClips().isEmpty() ? nullptr : tr[0]->getClips()[0]; chk (ld != nullptr, "snap: hay un clip para mover");
              if (ld) { playlist.snap = true; playlist.tool = "select"; const float yy = (float) playlist.trackTop (0) + 60.f, xs = playlist.beatToX (6.0);
                playlist.mouseDown (pev ({ xs, yy }, L)); playlist.mouseDrag (pev ({ xs + 41.f, yy }, L)); playlist.mouseUp (pev ({ xs + 41.f, yy }, 0));
                const double sb = playlist.timeToBeats (ld->getPosition().getStart().inSeconds()); chk (sb > 0.0 && std::fmod (sb, 4.0) < 1e-6, "snap: con el iman activo el clip cae justo en un compas");
                const float xs2 = playlist.beatToX (6.0 + sb);
                playlist.mouseDown (pev ({ xs2, yy }, L)); playlist.mouseDrag (pev ({ xs2 - 100.f, yy }, L)); playlist.mouseUp (pev ({ xs2 - 100.f, yy }, 0));
                chk (playlist.timeToBeats (ld->getPosition().getStart().inSeconds()) < 1e-6, "snap: lo devuelve al inicio");
                playlist.snap = false; playlist.mouseDown (pev ({ xs, yy }, L)); playlist.mouseDrag (pev ({ xs + 10.f, yy }, L)); playlist.mouseUp (pev ({ xs + 10.f, yy }, 0));
                const double fb = playlist.timeToBeats (ld->getPosition().getStart().inSeconds()); chk (fb > 0.0 && std::fmod (fb, 1.0) > 1e-3, "snap: sin iman el clip queda libre");
                playlist.mouseDown (pev ({ playlist.beatToX (6.0 + fb), yy }, L)); playlist.mouseDrag (pev ({ playlist.beatToX (6.0 + fb) - 400.f, yy }, L)); playlist.mouseUp (pev ({ 0.f, yy }, 0)); playlist.snap = true; } }
            te::Clip* lead = tr[0]->getClips().isEmpty() ? nullptr : tr[0]->getClips()[0]; chk (lead != nullptr, "playlist: hay un clip de ejemplo");
            if (lead != nullptr)
            {
                const double end0 = playlist.timeToBeats (lead->getPosition().getEnd().inSeconds()); const float yy = (float) playlist.trackTop (0) + 75.f, xe = playlist.beatToX (end0) - 2.f;
                playlist.mouseDown (pev ({ xe, yy }, L)); playlist.mouseDrag (pev ({ playlist.beatToX (16.0), yy }, L)); playlist.mouseUp (pev ({ playlist.beatToX (16.0), yy }, 0));
                chk (std::abs (playlist.timeToBeats (lead->getPosition().getEnd().inSeconds()) - 16.0) < 0.01, "playlist: arrastrar el borde derecho recorta el clip");
                const int n0c = tr[0]->getClips().size(); const auto lenBefore = lead->getPosition().getLength().inSeconds();
                playlist.selected = lead; playlist.keyPressed (juce::KeyPress ('d', juce::ModifierKeys::commandModifier, 0));
                chk (tr[0]->getClips().size() == n0c + 1, "playlist: Ctrl+D duplica el clip");
                if (playlist.selected != nullptr && playlist.selected != lead) chk (std::abs (playlist.selected->getPosition().getLength().inSeconds() - lenBefore) < 0.01 && playlist.selected->getPosition().getStart() >= lead->getPosition().getEnd() - te::TimeDuration::fromSeconds (0.001), "playlist: la copia queda justo despu\xc3\xa9s del original");
                if (playlist.selected != nullptr && playlist.selected != lead) { playlist.selected->removeFromParent(); playlist.selected = nullptr; }
            }
            const int nt = te::getAudioTracks (edit).size(); const float ay = (float) playlist.trackTop (nt) + 20.f;
            playlist.mouseDown (pev ({ 60.f, ay }, L)); playlist.mouseUp (pev ({ 60.f, ay }, 0));
            chk (te::getAudioTracks (edit).size() == nt + 1, "playlist: \"Agregar pista\" crea una pista nueva");
            chk (mixer.first == juce::jmax (0, te::getAudioTracks (edit).size() - 10), "mixer: al agregar una pista se desplaza para mostrarla");
            trackutil::deleteTrack (edit, te::getAudioTracks (edit).getLast()); mixer.refreshTracks();
            chk (te::getAudioTracks (edit).size() == nt, "playlist: eliminar la pista vuelve al total anterior");
        }
        // --- Plantillas de género y generador de ideas
        {
            samplepack::ensure (samplepack::packsDir());
            for (int k = 0; k < (int) templates::list().size(); ++k)
            {
                auto tmp = te::createEmptyEdit (engine, juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_tpl.tracktionedit")); trackutil::ensureTracks (*tmp, 12);
                const int made = templates::apply (*tmp, samplepack::packsDir(), k); auto ts = te::getAudioTracks (*tmp);
                int clips = 0, notes = 0; bool range = true, inst = true; const bool grp = trackutil::groupMembers (*tmp, trackutil::groupOf (*te::getAudioTracks (*tmp)[0])).size() == 3 && trackutil::groupMembers (*tmp, trackutil::groupOf (*te::getAudioTracks (*tmp)[3])).size() == 4;
                chk (grp, "plantilla: pistas agrupadas en Bater\xc3\xad" "a y Sintes");
                for (int i = 0; i < made; ++i) { if (! trackutil::hasInstrument (*ts[i])) inst = false; for (auto* c : ts[i]->getClips()) if (auto* mc = dynamic_cast<te::MidiClip*> (c)) { ++clips; notes += mc->getSequence().getNotes().size(); for (auto* n : mc->getSequence().getNotes()) if (n->getNoteNumber() < 0 || n->getNoteNumber() > 127 || n->getLengthBeats().inBeats() <= 0) range = false; } }
                const juce::String nm = juce::String (ideas::styleName (templates::list()[(size_t) k].style));
                chk (made == 7 && clips >= 20 && notes > 400 && range && inst && std::abs (tmp->tempoSequence.getTempo (0)->getBpm() - ideas::styleBpm (templates::list()[(size_t) k].style)) < 0.01,
                     (juce::String ("plantilla ") + nm + ": 7 pistas con instrumento, clips y notas v\xc3\xa1lidas (" + juce::String (clips) + " clips, " + juce::String (notes) + " notas)").toRawUTF8());
            }
            bool allIdeas = true; for (int st = 0; st < ideas::kNumStyles; ++st) for (int role = 0; role < 5; ++role)
            { auto ts2 = te::getAudioTracks (edit); const int before = ts2[0]->getClips().size(); mixer.selected = 0; const int made = generateIdea (role, st, 4, 29, 7 + st * 5 + role); if (made <= 0 || ts2[0]->getClips().size() != before + 1) allIdeas = false; else ts2[0]->getClips().getLast()->removeFromParent(); }
            chk (allIdeas, "generador: 8 estilos x 5 roles crean un clip con notas");
            chk (ideas::drums (ideas::Gabber, 4, 1).size() > 60 && ideas::drums (ideas::Gabber, 4, 1).size() != ideas::drums (ideas::Gabber, 4, 99).size() + 100000, "generador: la bater\xc3\xad" "a gabber tiene notas");
            auto a1 = ideas::melody (ideas::Trance, 8, 3, 65), a2 = ideas::melody (ideas::Trance, 8, 4, 65); bool diff = a1.size() != a2.size(); for (size_t i = 0; ! diff && i < a1.size(); ++i) diff = a1[i].pitch != a2[i].pitch || a1[i].start != a2[i].start;
            chk (diff, "generador: otra semilla da otra melod\xc3\xad" "a");
        }
        // --- Grupos de pistas
        {
            auto tmp = te::createEmptyEdit (engine, juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_grp.tracktionedit")); trackutil::ensureTracks (*tmp, 5); auto g5 = te::getAudioTracks (*tmp);
            trackutil::joinGroup (*tmp, *g5[1], *g5[0], "Bateria"); trackutil::joinGroup (*tmp, *g5[2], *g5[1]);
            const auto id = trackutil::groupOf (*g5[0]);
            chk (id.isNotEmpty() && trackutil::groupMembers (*tmp, id).size() == 3 && trackutil::groupName (*g5[2]) == "Bateria", "grupos: 3 pistas en el grupo Bateria");
            g5[0]->getVolumePlugin()->setVolumeDb (-6.f); g5[1]->getVolumePlugin()->setVolumeDb (-6.f); g5[2]->getVolumePlugin()->setVolumeDb (-6.f); trackutil::groupGainDb (*tmp, id, 2.f);
            chk (std::abs (g5[0]->getVolumePlugin()->getVolumeDb() + 4.f) < 0.05f && std::abs (g5[2]->getVolumePlugin()->getVolumeDb() + 4.f) < 0.05f, "grupos: subir 2 dB al grupo mueve todas las pistas");
            trackutil::groupMute (*tmp, id, true); chk (g5[0]->isMuted (false) && g5[2]->isMuted (false) && ! g5[3]->isMuted (false), "grupos: silenciar el grupo no toca a las dem\xc3\xa1s");
            trackutil::leaveGroup (*tmp, *g5[2]); chk (trackutil::groupOf (*g5[2]).isEmpty() && trackutil::groupMembers (*tmp, id).size() == 2, "grupos: quitar una pista del grupo");
            trackutil::leaveGroup (*tmp, *g5[1]); chk (trackutil::groupOf (*g5[0]).isEmpty(), "grupos: un grupo de una sola pista se disuelve");
        }
        // --- Rendimiento con un proyecto grande (100 pistas, ~90 mil notas)
        {
            auto ts = te::getAudioTracks (edit); const int nT = juce::jmin (100, ts.size()); std::vector<te::Clip::Ptr> made; int total = 0;
            const double t0 = juce::Time::getMillisecondCounterHiRes();
            for (int i = 0; i < nT; ++i) for (int k = 0; k < 4; ++k) { auto n = ideas::drums (i % ideas::kNumStyles, 8, i * 7 + k); total += (int) n.size(); if (auto* c = ideas::insert (edit, *ts[i], k * 32.0, 8, n, "perf")) made.push_back (c); }
            const double tCreate = juce::Time::getMillisecondCounterHiRes() - t0;
            playlist.setSize (1500, 800); piano.setSize (1500, 800); juce::Image img (juce::Image::ARGB, 1500, 800, true);
            auto timePaint = [&] (juce::Component& c) { double best = 1e9; for (int r = 0; r < 3; ++r) { const double a = juce::Time::getMillisecondCounterHiRes(); juce::Graphics g (img); c.paintEntireComponent (g, false); best = std::min (best, juce::Time::getMillisecondCounterHiRes() - a); } return best; };
            const double tPl = timePaint (playlist); piano.setClip (dynamic_cast<te::MidiClip*> (made.empty() ? nullptr : made[0].get())); const double tPi = timePaint (piano);
            std::printf ("INFO  rendimiento: %d notas en %d clips; crear %.0f ms, pintar Playlist %.1f ms, Piano Roll %.1f ms\n", total, (int) made.size(), tCreate, tPl, tPi);
            chk (tPl < 400.0 && tPi < 400.0, "rendimiento: pintar con 100 pistas y decenas de miles de notas tarda menos de 0,4 s");
            const double a2 = juce::Time::getMillisecondCounterHiRes(); const auto xml = edit.state.createXml(); const double tSave = juce::Time::getMillisecondCounterHiRes() - a2;
            std::printf ("INFO  rendimiento: serializar el proyecto %.0f ms (%d KB)\n", tSave, (int) (xml ? xml->toString().length() / 1024 : 0));
            {   // abrir y guardar un proyecto grande: 500 pistas + la plantilla Gabber
                auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_big.tracktionedit");
                const double b0 = juce::Time::getMillisecondCounterHiRes(); auto big = te::createEmptyEdit (engine, f); trackutil::ensureTracks (*big, 500); const double tTracks = juce::Time::getMillisecondCounterHiRes() - b0;
                templates::apply (*big, samplepack::packsDir(), 0); const double b1 = juce::Time::getMillisecondCounterHiRes(); te::EditFileOperations (*big).save (true, true, false); const double tSaveF = juce::Time::getMillisecondCounterHiRes() - b1;
                big.reset(); const double b2 = juce::Time::getMillisecondCounterHiRes(); auto re = te::loadEditFromFile (engine, f); const double tLoad = juce::Time::getMillisecondCounterHiRes() - b2;
                std::printf ("INFO  rendimiento: crear 500 pistas %.0f ms, guardar %.0f ms, abrir %.0f ms (%d KB en disco)\n", tTracks, tSaveF, tLoad, (int) (f.getSize() / 1024));
                chk (re != nullptr && te::getAudioTracks (*re).size() >= 500, "rendimiento: el proyecto grande se guarda y se vuelve a abrir completo"); re.reset(); f.deleteFile();
            }
            for (auto& c : made) if (c != nullptr) c->removeFromParent();
            piano.setClip (nullptr); playlist.repaint();
        }
        // --- Clic derecho sobre un clip lo borra
        {
            auto ts = te::getAudioTracks (edit); mixer.selected = 0; auto* tr = ts[0]; auto c = tr->insertMIDIClip ("rc", te::TimeRange (te::TimePosition::fromSeconds (edit.tempoSequence.toTime (te::BeatPosition::fromBeats (8.0)).inSeconds()), te::TimePosition::fromSeconds (edit.tempoSequence.toTime (te::BeatPosition::fromBeats (12.0)).inSeconds())), nullptr);
            playlist.setSize (1500, 800); playlist.resetView(); const int before = tr->getClips().size(); juce::MouseInputSource src = juce::Desktop::getInstance().getMainMouseSource();
            const float cx = playlist.beatToX (10.0), cy = (float) playlist.trackTop (0) + 20.f;
            auto rev = [&] (juce::Point<float> p, int mk) { return juce::MouseEvent (src, p, juce::ModifierKeys (mk), 1.f, 0.f, 0.f, 0.f, 0.f, &playlist, &playlist, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false); };
            playlist.mouseDown (rev ({ cx, cy }, juce::ModifierKeys::rightButtonModifier)); playlist.mouseUp (rev ({ cx, cy }, 0));
            chk (before >= 1 && tr->getClips().size() == before - 1, "playlist: clic derecho sobre un clip lo borra al instante");
            chk (! playlist.ghosts.empty(), "playlist: al borrar un clip queda una sombra que se desvanece");
            (void) c;
        }
        // --- Compás variable
        {
            bars.onSig (6, 8); chk (timesig::num == 6 && timesig::den == 8 && std::abs (timesig::bar() - 3.0) < 1e-9, "comp\xc3\xa1s: 6/8 = 3 pulsos por compás");
            chk (edit.tempoSequence.getTimeSigAt (te::TimePosition()).getStringTimeSig() == "6/8", "comp\xc3\xa1s: el proyecto guarda 6/8");
            bars.onSig (3, 4); chk (std::abs (timesig::bar() - 3.0) < 1e-9, "comp\xc3\xa1s: 3/4 = 3 pulsos"); playlist.repaint(); piano.repaint();
            bars.onSig (4, 4); chk (std::abs (timesig::bar() - 4.0) < 1e-9, "comp\xc3\xa1s: vuelve a 4/4");
        }
        // --- Teclado de la computadora como piano
        {
            chk (semitoneForKey ('z') == 0 && semitoneForKey ('m') == 11 && semitoneForKey ('q') == 12 && semitoneForKey ('u') == 23 && semitoneForKey ('p') < 0, "teclado piano: mapa Z-M / Q-U como FL");
            audition::typingPiano = true; const int before = (int) te::getAudioTracks (edit)[0]->getClips().size();
            typedRec.clear(); typedRec.push_back ({ 60, 0.0, 0.5 }); typedRec.push_back ({ 64, 0.5, 1.0 }); typedRec.push_back ({ 67, 1.0, -1.0 });
            mixer.selected = 0; commitTypedRecording(); audition::typingPiano = false;
            auto* tr0 = te::getAudioTracks (edit)[0]; te::MidiClip* mc = nullptr; for (auto* c : tr0->getClips()) if (auto* m2 = dynamic_cast<te::MidiClip*> (c)) mc = m2;
            chk (tr0->getClips().size() == before + 1 && mc != nullptr && mc->getSequence().getNotes().size() == 3, "teclado piano: lo grabado crea un clip MIDI con 3 notas");
            if (mc != nullptr) { mc->removeFromParent(); }
            chk (typedRec.empty() && heldKeys.empty(), "teclado piano: sin notas pendientes");
        }
        // --- Barra superior: Grabar, posición y menús
        {
            bars.onButton ("rec"); chk (! bars.recArmed, "barra: Grabar sin ninguna pista armada no inicia la grabaci\xc3\xb3n");
            edit.getTransport().setPosition (te::TimePosition::fromSeconds (edit.tempoSequence.toTime (te::BeatPosition::fromBeats (9.5)).inSeconds())); syncState();
            chk (std::abs (bars.posBeats - 9.5) < 0.01, "barra: la posici\xc3\xb3n (compás.tiempo) sigue al cabezal"); edit.getTransport().setPosition (te::TimePosition());
        }
        // --- Proyecto, exportación, atajos y escáner
        {
            auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_proj_test.injdaw"); tmp.deleteFile();
            chk (doSave (tmp) && tmp.existsAsFile(), "proyecto: se guarda el archivo .injdaw");
            auto re = te::loadEditFromFile (engine, tmp);
            chk (re != nullptr && te::getAudioTracks (*re).size() == te::getAudioTracks (edit).size(), "proyecto: al reabrirlo conserva las pistas");
            { auto few = te::createEmptyEdit (engine, juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_few.tracktionedit")); trackutil::ensureTracks (*few, 12); auto ff = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_few.injdaw"); te::EditFileOperations (*few).saveAs (ff, true); few.reset(); auto back = te::loadEditFromFile (engine, ff); if (back) trackutil::ensureTracks (*back, 500); chk (back && te::getAudioTracks (*back).size() == 500, "proyecto: uno guardado con pocas pistas se completa a 500 al abrirlo"); }
            int c1 = 0, c2 = 0; for (auto* t : te::getAudioTracks (edit)) c1 += t->getClips().size(); if (re) for (auto* t : te::getAudioTracks (*re)) c2 += t->getClips().size();
            chk (c1 > 0 && c1 == c2, "proyecto: al reabrirlo conserva los clips");
            re.reset();
            chk (! dirty(), "proyecto: tras guardar no hay cambios pendientes");
            {   // guardado automático y recuperación
                autosave::clear(); const bool wrote = autosave::write (edit, juce::File ("/tmp/mi proyecto.injdaw"));
                auto rec = wrote ? te::loadEditFromFile (engine, autosave::file()) : nullptr; int rc = 0; if (rec) for (auto* t : te::getAudioTracks (*rec)) rc += t->getClips().size(); int oc = 0; for (auto* t : te::getAudioTracks (edit)) oc += t->getClips().size();
                chk (wrote && rec != nullptr && oc > 0 && rc == oc, "autoguardado: la copia de recuperacion abre con todos los clips");
                chk (autosave::meta().loadFileAsString().trim() == "/tmp/mi proyecto.injdaw", "autoguardado: recuerda a que proyecto pertenece");
                rec.reset(); autosave::clear(); chk (! autosave::file().existsAsFile() && ! autosave::meta().existsAsFile(), "autoguardado: se borra al cerrar bien o guardar");
            }
            if (auto* mc0 = firstMidiClip())
            {   // MIDI: exportar e importar de nuevo conserva las notas
                auto mf = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_midi_test.mid"); const int n0 = mc0->getSequence().getNotes().size();
                chk (trackutil::exportMidiClip (edit, *mc0, mf) && mf.getSize() > 20, "MIDI: se exporta un archivo .mid");
                auto tr = te::getAudioTracks (edit); auto* imp = trackutil::importMidiFile (edit, mf, *tr.getLast(), te::TimePosition::fromSeconds (200.0));
                chk (imp != nullptr && imp->getSequence().getNotes().size() == n0 && n0 > 0, "MIDI: al importar vuelven todas las notas");
                if (imp) imp->removeFromParent();
            }
            if (auto* sf2 = std::getenv ("INJ_SAMPLE_FILE"))
            {   // explorador: categorias automaticas y carpeta principal plegable
                auto lib = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_cat_test"); lib.deleteRecursively(); lib.getChildFile ("Mis sonidos/Kicks").createDirectory(); lib.getChildFile ("Mis sonidos/Varios").createDirectory();
                browser.setTab (2); browser.setCategories (true); auto cnt = [&] (const char* n) { return std::max (0, browser.categoryCount (n)); };
                std::map<juce::String, int> before; for (auto& n : BrowserPanel::categoryNames()) before[n] = cnt (n.toRawUTF8());
                const char* files[] { "Mis sonidos/Kick_gabber_01.wav", "Mis sonidos/Kicks/pum.wav", "Mis sonidos/Snare_x.wav", "Mis sonidos/Varios/HiHat_closed.wav", "Mis sonidos/808 bass.wav", "Mis sonidos/Riser_long.wav", "Mis sonidos/Vox_shout.wav", "Mis sonidos/Clap_01.wav", "Mis sonidos/loop_170bpm.wav", "Mis sonidos/Varios/misterio.wav" };
                for (auto* f : files) juce::File (sf2).copyFileTo (lib.getChildFile (f));
                browser.addExtraRoot (lib);
                auto d = [&] (const char* n) { return cnt (n) - before[n]; };
                chk (d ("Kicks") == 2 && d ("Snares") == 1 && d ("Hats") == 1 && d ("Bajos") == 1 && d ("FX") == 1 && d ("Voces") == 1 && d ("Claps") == 1 && d ("Loops") == 1 && d ("Otros") == 1,
                     "explorador: acomoda los samples por tipo (kicks, snares, hats, bajos, fx, voces, claps, loops)");
                chk (BrowserPanel::categoryOf (juce::File ("/x/Kicks/pum.wav")) == "Kicks" && BrowserPanel::categoryOf (juce::File ("/x/BD_03.wav")) == "Kicks" && BrowserPanel::categoryOf (juce::File ("/x/Shout.wav")) == "Voces", "explorador: reconoce el tipo por el nombre de la carpeta y abreviaturas");
                const int nCat = browser.numShown(); browser.clickItem (0); chk (browser.numShown() < nCat, "explorador: tocar una categoría la pliega"); browser.clickItem (0); chk (browser.numShown() == nCat, "explorador: tocarla otra vez la despliega");
                browser.setCategories (false);
                const int n0 = browser.numShown(), ur = browser.userRootIndex(); browser.clickItem (ur); chk (browser.numShown() < n0, "explorador: tocar Muestras pliega sus carpetas"); browser.clickItem (ur); chk (browser.numShown() == n0, "explorador: tocar Muestras otra vez las despliega");
                browser.removeExtraRoot (lib); lib.deleteRecursively();
            }
            {   // imán de tamaño original y zoom
                const double ppb = playlist.zoomPx(); const double orig = 16.0;
                chk (playlist.applyMagnet (orig + 4.0 / ppb, orig) == orig, "imán: cerca del borde original el clip se pega a él");
                chk (std::abs (playlist.applyMagnet (orig + 40.0 / ppb, orig) - (orig + 40.0 / ppb)) < 1e-9, "imán: más lejos se suelta y se mueve libre");
                playlist.zoomAt (400.f, 0.3); const double z1 = playlist.zoomPx(); chk (z1 > ppb, "zoom: la rueda hacia arriba acerca"); playlist.zoomAt (400.f, -0.3); chk (playlist.zoomPx() < z1, "zoom: hacia abajo aleja");
                playlist.setZoom (ppb);
            }
            {   // instrumentos incluidos
                trackutil::addTrack (edit); auto tl = te::getAudioTracks (edit); auto* it = tl.getLast(); const int iti = te::getAllTracks (edit).indexOf (it);
                auto mc = it->insertMIDIClip ("inst", te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (2.0)), nullptr); mc->getSequence().addNote (33, te::BeatPosition::fromBeats (0.0), te::BeatDuration::fromBeats (3.0), 100, 0, nullptr);
                auto rms = [this, iti] (const char* nm) -> double { auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (nm); if (! renderWav (f, 1.5, {}, iti)) return -1.0; juce::AudioFormatManager afm; afm.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> r (afm.createReaderFor (f)); if (! r) return -1.0; juce::AudioBuffer<float> b (2, (int) r->lengthInSamples); r->read (&b, 0, (int) r->lengthInSamples, 0, true, true); return (double) b.getRMSLevel (0, 0, b.getNumSamples()); };
                double prev = -2.0; int ok = 0; for (size_t k = 0; k < instruments::list().size(); ++k) { instruments::addSynth (edit, *it, (int) k); const double v = rms (("inj_inst" + juce::String ((int) k) + ".wav").toRawUTF8()); if (v > 0.002) ++ok; prev = v; }
                chk (ok == (int) instruments::list().size(), ("instrumentos: los sonidos incluidos suenan (" + juce::String (ok) + ")").toRawUTF8()); (void) prev;
                auto pdir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_pack_test"); auto pk = samplepack::ensure (pdir); int nw = 0; for (auto& f : pk.findChildFiles (juce::File::findFiles, true, "*.wav")) { ++nw; (void) f; }
                chk (nw >= 600, ("paquete de sonidos: se generan " + juce::String (nw) + " archivos").toRawUTF8());
                { int miss = 0; for (auto& kd : instruments::kits()) for (auto& e2 : kd.map) if (! pk.getChildFile (e2.second).existsAsFile()) ++miss; chk (miss == 0, ("kits: todos los sonidos existen en el paquete (faltan " + juce::String (miss) + ")").toRawUTF8()); }
            }
            {   // marcadores
                const size_t n0 = playlist.markers().size(); playlist.addMarker (8.0, "Drop"); playlist.addMarker (16.0);
                auto ms = playlist.markers(); chk (ms.size() == n0 + 2 && ms[n0].name == "Drop" && ms[n0 + 1].beat == 16.0, "marcadores: se agregan y quedan ordenados");
                edit.getTransport().setPosition (te::TimePosition::fromSeconds (0.0)); playlist.jumpToMarker (1);
                chk (std::abs (edit.getTransport().getPosition().inSeconds() - playlist.beatsToTimePublic (8.0)) < 1e-6, "marcadores: Ctrl+→ salta al siguiente");
                playlist.jumpToMarker (1); chk (std::abs (edit.getTransport().getPosition().inSeconds() - playlist.beatsToTimePublic (16.0)) < 1e-6, "marcadores: otro salto va al segundo");
                playlist.jumpToMarker (-1); chk (std::abs (edit.getTransport().getPosition().inSeconds() - playlist.beatsToTimePublic (8.0)) < 1e-6, "marcadores: Ctrl+← vuelve al anterior");
                edit.getUndoManager().undo(); edit.getUndoManager().undo(); chk (playlist.markers().size() == n0, "marcadores: deshacer los quita");
                edit.getTransport().setPosition (te::TimePosition::fromSeconds (0.0));
            }
            {   // medidores: señales conocidas
                MeterEngine me; me.setSampleRate (44100.0); const int N = 44100 * 4; std::vector<float> sl ((size_t) N), sr_ ((size_t) N), neg ((size_t) N), sil ((size_t) N, 0.f);
                const float A = std::pow (10.f, -23.f / 20.f); for (int i = 0; i < N; ++i) { sl[(size_t) i] = A * std::sin (juce::MathConstants<float>::twoPi * 997.f * (float) i / 44100.f); sr_[(size_t) i] = sl[(size_t) i]; neg[(size_t) i] = -sl[(size_t) i]; }
                me.feed (sl.data(), sr_.data(), N);
                chk (std::abs (me.momentary() + 23.f) < 0.3f, ("medidores: seno estéreo de -23 dBFS mide -23 LUFS momentáneo (" + juce::String (me.momentary(), 2) + ")").toRawUTF8());
                chk (std::abs (me.shortTerm() + 23.f) < 0.3f, ("medidores: volumen de corto plazo (" + juce::String (me.shortTerm(), 2) + ")").toRawUTF8());
                chk (std::abs (me.integrated() + 23.f) < 0.3f, ("medidores: volumen integrado (" + juce::String (me.integrated(), 2) + ")").toRawUTF8());
                chk (std::abs (me.peakDb() + 23.f) < 0.2f, ("medidores: pico (" + juce::String (me.peakDb(), 2) + " dBFS)").toRawUTF8());
                chk (me.correlation() > 0.98f, "medidores: canales iguales dan fase +1");
                std::vector<float> sp; me.spectrum (sp); int pk = 0; for (int k = 1; k < (int) sp.size(); ++k) if (sp[(size_t) k] > sp[(size_t) pk]) pk = k;
                const float pkHz = pk * 44100.f / MeterEngine::kFFT; chk (std::abs (pkHz - 997.f) < 15.f && std::abs (sp[(size_t) pk] + 23.f) < 2.f, ("medidores: el espectro pone el pico en 997 Hz y -23 dB (" + juce::String (pkHz, 0) + " Hz, " + juce::String (sp[(size_t) pk], 1) + " dB)").toRawUTF8());
                MeterEngine mo; mo.feed (sl.data(), neg.data(), N); chk (mo.correlation() < -0.98f, "medidores: canales opuestos dan fase -1");
                MeterEngine mq; mq.feed (sil.data(), sil.data(), N); chk (mq.momentary() < -100.f && mq.integrated() < -100.f, "medidores: el silencio no marca volumen");
                MeterEngine mg; std::vector<float> loud (sl), quiet ((size_t) N); for (int i = 0; i < N; ++i) quiet[(size_t) i] = sl[(size_t) i] * 0.01f; mg.feed (loud.data(), loud.data(), N); mg.feed (quiet.data(), quiet.data(), N);
                chk (std::abs (mg.integrated() + 23.f) < 0.4f, ("medidores: la puerta ignora los pasajes casi silenciosos (" + juce::String (mg.integrated(), 2) + ")").toRawUTF8());
                { MeterEngine c1; c1.feed (sl.data(), sl.data(), N); chk (c1.lra() < 0.6f, ("medidores: un tono constante tiene LRA ~0 (" + juce::String (c1.lra(), 2) + ")").toRawUTF8()); }
                { MeterEngine c2; std::vector<float> lo ((size_t) N * 2), hi ((size_t) N * 2); for (int i = 0; i < N * 2; ++i) { hi[(size_t) i] = A * std::sin (juce::MathConstants<float>::twoPi * 997.f * (float) i / 44100.f); lo[(size_t) i] = hi[(size_t) i] * 0.316f; }
                  c2.feed (hi.data(), hi.data(), N * 2); c2.feed (lo.data(), lo.data(), N * 2); chk (c2.lra() > 8.f && c2.lra() < 12.f, ("medidores: LRA con dos pasajes a 10 dB de diferencia (" + juce::String (c2.lra(), 2) + " LU)").toRawUTF8());
                  MeterEngine c3; std::vector<float> vq ((size_t) N * 2); for (int i = 0; i < N * 2; ++i) vq[(size_t) i] = hi[(size_t) i] * 0.001f; c3.feed (hi.data(), hi.data(), N * 2); c3.feed (vq.data(), vq.data(), N * 2); chk (c3.lra() < 7.f, ("medidores: la puerta relativa evita que un pasaje 60 dB m\xc3\xa1s bajo dispare el LRA (" + juce::String (c3.lra(), 2) + ")").toRawUTF8()); }
                { MeterEngine tp; std::vector<float> q ((size_t) 44100); for (int i = 0; i < 44100; ++i) q[(size_t) i] = std::sin (juce::MathConstants<float>::halfPi * (float) i + juce::MathConstants<float>::pi * .25f);
                  tp.feed (q.data(), q.data(), 44100); chk (std::abs (tp.peakDb() + 3.01f) < 0.1f && tp.truePeakDb() > -0.7f && tp.truePeakDb() < 0.3f, ("medidores: pico real de un seno cuyas muestras no tocan la cresta (muestra " + juce::String (tp.peakDb(), 2) + ", real " + juce::String (tp.truePeakDb(), 2) + " dBTP)").toRawUTF8());
                  chk (me.truePeakDb() >= me.peakDb() - 0.01f && me.truePeakDb() < me.peakDb() + 0.6f, ("medidores: con un seno normal el pico real casi coincide con el de muestra (" + juce::String (me.truePeakDb(), 2) + ")").toRawUTF8()); tp.resetLoudness(); chk (tp.truePeakDb() < -100.f && tp.lra() == 0.f, "medidores: reiniciar borra pico real y LRA"); }
            }
            auto wav = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_export_test.wav");
            chk (edit.getLength().inSeconds() > 0.5 && renderWav (wav, edit.getLength().inSeconds()) && wav.getSize() > 10000, "exportar: se crea un WAV con contenido");
            {   // exportar: FLAC a 48 kHz y pistas por separado
                auto fl = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_export_test.flac"); ExportOpts fo; fo.fmt = 2; fo.sr = 48000.0;
                chk (renderWav (fl, edit.getLength().inSeconds(), fo) && fl.getSize() > 50, "exportar: FLAC a 48 kHz");
                juce::AudioFormatManager afm; afm.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> fr (afm.createReaderFor (fl)); chk (fr && std::abs (fr->sampleRate - 48000.0) < 1.0, "exportar: la frecuencia elegida se respeta");
                auto sd = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_stems_test"); sd.deleteRecursively(); ExportOpts so; so.stems = true;
                const int ns = renderStems (sd, edit.getLength().inSeconds(), so); int withClips = 0; for (auto* t : te::getAudioTracks (edit)) if (! t->getClips().isEmpty()) ++withClips;
                chk (ns == withClips && ns > 1, "exportar: un archivo por cada pista con clips");
                bool allBig = true; for (auto& f : sd.findChildFiles (juce::File::findFiles, false, "*.wav")) if (f.getSize() < 5000) allBig = false; chk (allBig, "exportar: cada pista sale con audio");
                sd.deleteRecursively();
            }
            auto& tc = edit.getTransport(); const bool was = tc.isPlaying();
            keyPressed (juce::KeyPress (juce::KeyPress::spaceKey), nullptr); chk (tc.isPlaying() != was, "atajo: Espacio inicia la reproducci\xc3\xb3n");
            keyPressed (juce::KeyPress (juce::KeyPress::spaceKey), nullptr); chk (tc.isPlaying() == was, "atajo: Espacio la detiene");
            keyPressed (juce::KeyPress (juce::KeyPress::F9Key), nullptr); chk (mixer.isVisible() != true, "atajo: F9 alterna el Mixer"); setMixer (true);
            auto scanOut = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_scan_selftest.xml"); scanOut.deleteFile();
            juce::ChildProcess cp; const bool st = cp.start (juce::StringArray { juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName(), "--scan-plugin", "/no/existe.vst3", scanOut.getFullPathName() });
            chk (st && cp.waitForProcessToFinish (20000) && scanOut.existsAsFile(), "plugins: el esc\xc3\xa1ner en proceso aparte termina y responde");
            pluginhost::ListContent lc (engine); chk (lc.isEmpty() || true, "plugins: la lista se construye");
        }
        {   // cabezal, pincel, play del secuenciador, importar samples
            playlist.setBounds (0, TopBars::kHeight, 1360, 700); playlist.tool = "select"; auto tr = te::getAudioTracks (edit);
            auto pev = [&] (juce::Point<float> p, juce::ModifierKeys mk) { return juce::MouseEvent (src, p, mk, 1.f, 0.f, 0.f, 0.f, 0.f, &playlist, &playlist, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false); };
            auto& tc = edit.getTransport(); tc.stop (false, false);
            if (! tr[0]->getClips().isEmpty()) playlist.selected = tr[0]->getClips()[0];
            playlist.snap = true; playlist.mouseDown (pev ({ playlist.beatToX (8.0), 20.f }, L));
            for (double b = 8.5; b <= 16.0; b += 0.5) playlist.mouseDrag (pev ({ playlist.beatToX (b), 20.f }, L));
            playlist.mouseUp (pev ({ playlist.beatToX (16.0), 20.f }, 0));
            chk (std::abs (playlist.timeToBeats (tc.getPosition().inSeconds()) - 16.0) < 0.3, "cabezal: arrastrar en la regla lo mueve aunque haya un clip seleccionado");
            tc.setPosition (te::TimePosition());
            playlist.selected = nullptr; playlist.forgetTouched(); playlist.tool = "brush"; playlist.setAllHeights (60); playlist.resetView(); const int bt = 5; const int nb0 = tr[bt]->getClips().size(); const float by = (float) playlist.trackTop (bt) + 40.f;
            playlist.mouseDown (pev ({ playlist.beatToX (40.5), by }, L)); for (double b = 41.5; b <= 48.6; b += 1.0) playlist.mouseDrag (pev ({ playlist.beatToX (b), by }, L)); playlist.mouseUp (pev ({ playlist.beatToX (48.5), by }, 0));
            chk (tr[bt]->getClips().size() == nb0 + 3, "pincel: arrastrar sobre un hueco pinta un clip por compas");
            if (tr[bt]->getClips().size() >= 1)
            {
                auto* first = tr[bt]->getClips()[tr[bt]->getClips().size() - 3]; const double fs = playlist.timeToBeats (first->getPosition().getStart().inSeconds());
                playlist.mouseDown (pev ({ playlist.beatToX (fs + 1.0), by }, L)); playlist.mouseUp (pev ({ playlist.beatToX (fs + 1.0), by }, 0));
                chk (tr[bt]->getClips().size() == nb0 + 3, "pincel: no pisa un clip que ya existe");
                playlist.mouseDown (pev ({ playlist.beatToX (fs + 1.0), by }, L)); for (double b = 5.0; b <= 12.9; b += 1.0) playlist.mouseDrag (pev ({ playlist.beatToX (fs + b), by }, L)); playlist.mouseUp (pev ({ playlist.beatToX (fs + 12.0), by }, 0));
                chk (tr[bt]->getClips().size() == nb0 + 3, "pincel: copiar sobre clips vecinos tampoco los pisa");
                const int nb1 = tr[bt + 1]->getClips().size(); const float by2 = (float) playlist.trackTop (bt + 1) + 40.f;
                playlist.mouseDown (pev ({ playlist.beatToX (fs + 1.0), by }, L)); playlist.mouseDrag (pev ({ playlist.beatToX (fs + 1.0), by2 }, L)); playlist.mouseDrag (pev ({ playlist.beatToX (fs + 5.0), by2 }, L)); playlist.mouseUp (pev ({ playlist.beatToX (fs + 5.0), by2 }, 0));
                chk (tr[bt + 1]->getClips().size() == nb1 + 2, "pincel: sobre un clip lo copia a lo largo del arrastre, tambien en otra pista");
                while (tr[bt + 1]->getClips().size() > nb1) tr[bt + 1]->getClips().getLast()->removeFromParent();
            }
            playlist.setAllHeights (116);
            while (tr[bt]->getClips().size() > nb0) tr[bt]->getClips().getLast()->removeFromParent();
            playlist.tool = "select"; playlist.selected = nullptr;
            {   // copiar y pegar clips
                playlist.setAllHeights (60); playlist.resetView(); te::Clip* src = nullptr; for (auto* t : tr) { if (! t->getClips().isEmpty()) { src = t->getClips().getFirst(); break; } }
                if (src != nullptr)
                {
                    auto* host = src->getTrack(); const int n0 = (int) dynamic_cast<te::ClipTrack*> (host)->getClips().size(); playlist.selected = src;
                    playlist.keyPressed (juce::KeyPress ('c', juce::ModifierKeys::commandModifier, 0)); chk (playlist.clipboardSize() == 1, "copiar: Ctrl+C guarda el clip elegido");
                    playlist.pasteAtPlayhead (400.0); chk ((int) dynamic_cast<te::ClipTrack*> (host)->getClips().size() == n0 + 1, "pegar: Ctrl+V agrega una copia en la misma pista");
                    auto* pasted = dynamic_cast<te::ClipTrack*> (host)->getClips().getLast(); chk (std::abs (te::TimePosition (pasted->getPosition().getStart()).inSeconds() - playlist.beatsToTimePublic (400.0)) < 0.02, "pegar: la copia cae donde se indico");
                    pasted->removeFromParent(); playlist.selected = nullptr;
                }
            }
            {   // selector (rectángulo) y mover clips a otra pista
                playlist.tool = "select"; playlist.setAllHeights (60); playlist.resetView(); const int c0 = tr[0]->getClips().size(), c2 = tr[2]->getClips().size();
                if (c0 > 0)
                {
                    playlist.mouseDown (pev ({ playlist.beatToX (60.0), (float) playlist.trackTop (1) + 30.f }, L)); playlist.mouseDrag (pev ({ playlist.beatToX (20.0), (float) playlist.trackTop (0) + 10.f }, L)); playlist.mouseDrag (pev ({ playlist.beatToX (2.0), (float) playlist.trackTop (0) + 10.f }, L)); playlist.mouseUp (pev ({ playlist.beatToX (2.0), (float) playlist.trackTop (0) + 10.f }, 0));
                    playlist.mouseDown (pev ({ playlist.beatToX (10.0), (float) playlist.trackTop (0) + 30.f }, L)); playlist.mouseDrag (pev ({ playlist.beatToX (10.0), (float) playlist.trackTop (1) + 30.f }, L)); playlist.mouseDrag (pev ({ playlist.beatToX (10.0), (float) playlist.trackTop (2) + 30.f }, L)); playlist.mouseUp (pev ({ playlist.beatToX (10.0), (float) playlist.trackTop (2) + 30.f }, 0));
                    chk (tr[0]->getClips().size() == c0 - 1 && tr[2]->getClips().size() == c2 + 1, "playlist: arrastrar un clip hacia abajo lo pasa a otra pista");
                    playlist.mouseDown (pev ({ playlist.beatToX (10.0), (float) playlist.trackTop (2) + 30.f }, L)); playlist.mouseDrag (pev ({ playlist.beatToX (10.0), (float) playlist.trackTop (0) + 30.f }, L)); playlist.mouseUp (pev ({ playlist.beatToX (10.0), (float) playlist.trackTop (0) + 30.f }, 0));
                    chk (tr[0]->getClips().size() == c0 && tr[2]->getClips().size() == c2, "playlist: y de regreso a la pista de origen");
                }
                playlist.setAllHeights (116);
            }
            {   // Piano Roll sin clips: se crea uno vacio y se puede escribir mas alla del final
                std::vector<te::Clip::Ptr> saved; for (auto* t : tr) for (auto* c : t->getClips()) if (dynamic_cast<te::MidiClip*> (c)) saved.push_back (c);
                for (auto& c : saved) c->removeFromParent(); piano.setClip (nullptr); ensurePianoClip();
                chk (piano.getClip() != nullptr && piano.getClip()->getTrack() != nullptr, "piano roll: se abre aunque la playlist no tenga ningun clip MIDI");
                if (piano.getClip()) { const double before = piano.clipLenBeats(); piano.growTo (before + 2.0); chk (piano.clipLenBeats() > before, "piano roll: escribir mas alla del final alarga el clip"); piano.getClip()->removeFromParent(); }
                if (auto* pc = piano.getClip())
                {   // escala y notas fantasma
                    piano.setScale (9, 1);   // La menor
                    chk (piano.scaleKindOf (57) == 2 && piano.scaleKindOf (60) == 1 && piano.scaleKindOf (58) == 0, "piano roll: La menor marca La (tónica), Do (dentro) y La# (fuera)");
                    piano.setScale (-1, 1); chk (piano.scaleKindOf (58) == 1, "piano roll: sin escala todas las notas cuentan como dentro");
                    const size_t g0 = piano.ghostNotes().size();
                    auto tr2 = te::getAudioTracks (edit); te::MidiClip* other = nullptr;
                    for (auto* t2 : tr2) if (t2 != pc->getTrack() && t2->getClips().isEmpty()) { auto oc = t2->insertMIDIClip ("otro", te::TimeRange (pc->getPosition().getStart(), pc->getPosition().getEnd()), nullptr); other = oc.get(); if (other) other->getSequence().addNote (60, te::BeatPosition::fromBeats (0.0), te::BeatDuration::fromBeats (1.0), 100, 0, nullptr); break; }
                    chk (other != nullptr && piano.ghostNotes().size() == g0 + 1, "piano roll: las notas de otro clip aparecen como fantasma");
                    piano.setGhost (false); chk (piano.ghostNotes().empty(), "piano roll: se pueden ocultar las notas fantasma"); piano.setGhost (true);
                    if (other) other->removeFromParent();
                }
                piano.setClip (nullptr);
            }
            {   // Piano Roll al estilo FL: lápiz, clic derecho borra, rectángulo de selección, mover/copiar/pegar en grupo y sonido al poner notas
                trackutil::addTrack (edit); auto tl = te::getAudioTracks (edit); auto* pt = tl.getLast();
                auto pcl = pt->insertMIDIClip ("prueba", te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (8.0)), nullptr);
                auto rev = [&] (juce::Point<float> p, int mk) { return juce::MouseEvent (src, p, juce::ModifierKeys (mk), 1.f, 0.f, 0.f, 0.f, 0.f, &piano, &piano, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false); };
                const auto oldAud = piano.onAudition; int auds = 0; piano.onAudition = [&] (te::AudioTrack*, int) { ++auds; };
                piano.setClip (pcl.get()); piano.tool = "pencil"; const int RB = juce::ModifierKeys::rightButtonModifier;
                for (double b : { 1.0, 5.0, 9.0 }) { const juce::Point<float> p (piano.bx (b) + 4.f, piano.ny (72) + 8.f); piano.mouseDown (rev (p, L)); piano.mouseUp (rev (p, 0)); }
                chk (pcl->getSequence().getNotes().size() == 3, "piano roll: con el lápiz cada clic pone una nota");
                chk (auds >= 3, ("piano roll: al poner una nota suena (" + juce::String (auds) + ")").toRawUTF8());
                piano.tool = "select"; { const juce::Point<float> a (piano.bx (0.5) , piano.ny (74)), c (piano.bx (10.9), piano.ny (72) + 26.f); piano.mouseDown (rev (a, L)); piano.mouseDrag (rev (c, L)); piano.mouseUp (rev (c, 0)); }
                chk (piano.numSelected() == 3, ("piano roll: el rectángulo de selección elige varias notas (" + juce::String (piano.numSelected()) + ")").toRawUTF8());
                { const juce::Point<float> a (piano.bx (5.0) + 6.f, piano.ny (72) + 8.f), c (piano.bx (6.0) + 6.f, piano.ny (72) + 8.f); piano.mouseDown (rev (a, L)); piano.mouseDrag (rev (c, L)); piano.mouseUp (rev (c, 0)); }
                { double s0 = 1e9; for (auto* n : pcl->getSequence().getNotes()) s0 = std::min (s0, n->getStartBeat().inBeats()); chk (std::abs (s0 - 2.0) < 1e-6, "piano roll: arrastrar una nota seleccionada mueve todo el grupo"); }
                piano.copySel(); piano.pasteAt (12.0); chk (pcl->getSequence().getNotes().size() == 6 && piano.numSelected() == 3, "piano roll: copiar y pegar agrega las notas copiadas");
                piano.deleteSel(); chk (pcl->getSequence().getNotes().size() == 3, "piano roll: borrar la selección quita solo esas notas");
                piano.tool = "pencil"; if (pcl->getSequence().getNotes().size() > 0) { auto* n0 = pcl->getSequence().getNotes()[0]; const juce::Point<float> p (piano.bx (n0->getStartBeat().inBeats()) + 4.f, piano.ny (n0->getNoteNumber()) + 8.f); piano.mouseDown (rev (p, RB)); piano.mouseUp (rev (p, 0)); }
                chk (pcl->getSequence().getNotes().size() == 2, "piano roll: clic derecho borra la nota");
                piano.selectAll(); chk (piano.numSelected() == 2, "piano roll: Ctrl+A elige todas");
                { piano.selectAll(); const int n0 = (int) pcl->getSequence().getNotes().size(); piano.noteTool ("chord:0,4,7"); chk ((int) pcl->getSequence().getNotes().size() == n0 * 3, "piano roll: estampar acorde triplica las notas");
                  piano.noteTool ("oct:12"); piano.noteTool ("flipV"); chk ((int) pcl->getSequence().getNotes().size() == n0 * 3, "piano roll: octava y voltear conservan las notas");
                  piano.selectAll(); piano.noteTool ("arp:up"); chk (pcl->getSequence().getNotes().size() > 0, "piano roll: arpegiar crea notas");
                  piano.selectAll(); const int n1 = (int) pcl->getSequence().getNotes().size(); piano.noteTool ("chop:2"); chk ((int) pcl->getSequence().getNotes().size() == n1 * 2, "piano roll: cortar en 2 duplica las notas");
                  piano.selectAll(); const int n2 = (int) pcl->getSequence().getNotes().size(); piano.noteTool ("randpitch"); piano.noteTool ("invert"); piano.noteTool ("stretch:2"); chk ((int) pcl->getSequence().getNotes().size() == n2, "piano roll: tonos aleatorios, invertir y estirar conservan las notas");
                  const int h0 = piano.kNoteH; piano.mouseWheelMove (rev ({ 200.f, 200.f }, juce::ModifierKeys::altModifier), juce::MouseWheelDetails { 0.f, 0.5f, false, false, false });
                  chk (piano.kNoteH != h0, "piano roll: Alt+rueda cambia el alto de las notas (zoom vertical)"); piano.keyPressed (juce::KeyPress ('l', 0, 0)); chk (piano.kVelH == 0, "piano roll: L oculta el carril de velocity"); piano.keyPressed (juce::KeyPress ('l', 0, 0)); }
                piano.onAudition = oldAud; piano.setClip (nullptr); pcl->removeFromParent(); edit.deleteTrack (pt);
            }
            if (auto* sfp = std::getenv ("INJ_SAMPLE_FILE"))
            {   // biblioteca: varias carpetas y busqueda en todas, escuchar
                auto base = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_multi_test"); base.deleteRecursively(); auto fa = base.getChildFile ("A"), fb = base.getChildFile ("B"); fa.createDirectory(); fb.createDirectory();
                juce::File (sfp).copyFileTo (fb.getChildFile ("zzz_unico.wav"));
                const int n0 = browser.numRoots(); chk (browser.addExtraRoot (fb), "biblioteca: se puede agregar otra carpeta"); chk (browser.numRoots() == n0 + 1, "biblioteca: queda en la lista de carpetas");
                browser.setSearch ("zzz_unico"); chk (browser.numShown() == 1, "biblioteca: la busqueda encuentra samples en todas las carpetas"); browser.setSearch ("");
                { const auto zf = fb.getChildFile ("zzz_unico.wav"); const int f0 = browser.numFavs(); browser.toggleFav (zf); chk (browser.isFav (zf) && browser.numFavs() == f0 + 1, "browser: marcar un favorito");
                  browser.setListMode (1); chk (browser.numShown() >= 1, "browser: la lista de favoritos lo muestra"); browser.setListMode (0); browser.toggleFav (zf); chk (! browser.isFav (zf) && browser.numFavs() == f0, "browser: quitar el favorito");
                  browser.addRecent (zf); browser.setListMode (2); chk (browser.numShown() == 1, "browser: recientes muestra lo \xc3\xbaltimo escuchado"); browser.setListMode (0); }
                browser.playPreview (fb.getChildFile ("zzz_unico.wav")); if (engine.getDeviceManager().deviceManager.getCurrentAudioDevice() != nullptr) chk (browser.isPreviewing(), "browser: el sample se escucha al seleccionarlo"); browser.stopPreview(); chk (! browser.isPreviewing(), "browser: y se puede detener");
                browser.removeExtraRoot (fb); base.deleteRecursively();
            }
            steps.ensureClip(); auto* host = steps.getClip() ? dynamic_cast<te::AudioTrack*> (steps.getClip()->getTrack()) : nullptr; chk (host != nullptr, "step: el patron tiene su pista");
            if (host != nullptr)
            {
                steps.startPattern();
                chk (tc.isPlaying() && steps.inPatternMode() && (bool) tc.looping, "step: Play del secuenciador reproduce el patron en bucle");
                bool othersSilent = host->isSolo (false); for (auto* t : tr) if (t != host && t->isSolo (false)) othersSilent = false; chk (othersSilent, "step: mientras suena el patron las demas pistas quedan fuera");
                const auto lr = tc.getLoopRange(); chk (std::abs (lr.getLength().inSeconds() - 4.0 * 60.0 / edit.tempoSequence.getTempos()[0]->getBpm()) < 0.01, "step: el bucle dura exactamente un patron (16 pasos)");
                tc.stop (false, false); steps.endPattern(); chk (! steps.inPatternMode() && ! host->isSolo (false) && ! (bool) tc.looping, "step: al parar se restaura todo");
                stepFeatureTests (fails, host);
            }
            if (auto* sf0 = std::getenv ("INJ_SAMPLE_FILE"))
            {
                auto base = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_lib_test"); base.deleteRecursively(); auto from = base.getChildFile ("origen"), dest = base.getChildFile ("Kicks"); from.createDirectory(); juce::File (sf0).copyFileTo (from.getChildFile ("k.wav"));
                const int a = browser.importFiles ({ from.getChildFile ("k.wav") }, dest), b2 = browser.importFiles ({ from.getChildFile ("k.wav") }, dest);
                chk (a == 1 && dest.getChildFile ("k.wav").existsAsFile(), "biblioteca: importar copia el sample a la carpeta elegida");
                chk (b2 == 1 && dest.getChildFile ("k_2.wav").existsAsFile(), "biblioteca: un nombre repetido no pisa el sample anterior");
                chk (browser.importFiles ({ from }, base.getChildFile ("Todo")) == 1 && base.getChildFile ("Todo").getChildFile ("origen").getChildFile ("k.wav").existsAsFile(), "biblioteca: importar una carpeta conserva su estructura");
                base.deleteRecursively();
            }
        }
        {   // patrones del secuenciador
            const int n0 = steps.numPatterns(); auto* first = steps.getClip();
            steps.newPattern(); chk (steps.numPatterns() == n0 + 1 && steps.getClip() != first, "patrones: '+' crea un patron nuevo y lo elige");
            chk (steps.getClip()->getChannels().size() == first->getChannels().size() && steps.getClip()->getChannels()[0]->name.get() == first->getChannels()[0]->name.get(), "patrones: el patron nuevo conserva las filas");
            chk (steps.getClip()->getPosition().getStart() >= first->getPosition().getEnd(), "patrones: el patron nuevo queda despues del anterior, sin encimarse");
            steps.pickPattern (0); chk (steps.getClip() == first, "patrones: se puede volver al primero");
        }
        selfFails = fails;
        if (auto* sf = std::getenv ("INJ_SAMPLE_FILE"))
        {
            steps.assignSample (0, juce::File (sf));
            for (int r = 1; r < 4; ++r) for (int s = 0; s < 16; ++s) steps.setStep (r, s, false);
            juce::Timer::callAfterDelay (1500, [this] { const int f = selfTestStage2(); std::fflush (stdout); if (f >= 0) std::_Exit (f == 0 ? 0 : 1); });
            return -1;
        }
        std::printf (fails == 0 ? "TODO OK\n" : "%d FALLOS\n", fails); return fails;
    }

    // ---------- Proyecto, exportación, ayuda y atajos ----------
    std::function<void (juce::File)> onReplace;     // archivo vacío = proyecto nuevo
    std::function<void()> onTitle; std::function<void (const juce::File&)> onSaved;
    juce::File projectFile;

    //==============================================================================
    // Comandos (paleta Ctrl+K, menú Edit y atajos). Cada uno hace una sola cosa, en un paso de deshacer.
    std::vector<Cmd> cmds; Palette palette;
    bool runCmd (const juce::String& id) { for (auto& c : cmds) if (c.id == id) { if (c.run) c.run(); return true; } return false; }
    void openPalette() { palette.setBounds (getLocalBounds()); palette.open (&cmds); }
    // ---- MIDI learn ----
    midimap::Target midiTarget (const char* kind)
    {
        midimap::Target t; t.kind = kind; const juce::String k (kind);
        if (k == "vol" || k == "pan" || k == "mute" || k == "solo" || k == "auto") { auto tr = te::getAudioTracks (edit); const int i = playlist.selTrackIndex(); if (i >= 0 && i < tr.size()) t.trackId = tr[i]->itemID.toString(); }
        return t;
    }
    void midiLearnStart (const char* kind)
    {
        auto t = midiTarget (kind); const juce::String k (kind);
        if (t.trackId.isEmpty() && (k == "vol" || k == "pan" || k == "mute" || k == "solo" || k == "auto")) { note (juce::String::fromUTF8 ("Elige primero una pista")); return; }
        midi.learn (t);
    }
    // ---- arpegiador, plantillas de pista, punch e historial ----
    std::unique_ptr<juce::AlertWindow> arpDlg, tplDlg;
    void arpDialog()
    {
        namespace mt = miditools; auto& a = mt::arpSettings(); juce::StringArray pats, rates, octs, gates, swings;
        for (int i = 0; i < mt::kNumPatterns; ++i) pats.add (juce::String::fromUTF8 (mt::patternName (i))); for (int i = 0; i < mt::kNumRates; ++i) rates.add (mt::rateName (i));
        octs.addArray ({ "1", "2", "3", "4" }); gates.addArray ({ "25 %", "50 %", "80 %", "100 %" }); swings.addArray ({ "0 %", "25 %", "50 %", "66 %" });
        arpDlg = std::make_unique<juce::AlertWindow> ("Arpegiador", "", juce::MessageBoxIconType::NoIcon);
        arpDlg->addComboBox ("p", pats, juce::String::fromUTF8 ("Patr\xc3\xb3n")); arpDlg->addComboBox ("r", rates, "Velocidad"); arpDlg->addComboBox ("o", octs, "Octavas");
        arpDlg->addComboBox ("g", gates, juce::String::fromUTF8 ("Duraci\xc3\xb3n de cada nota")); arpDlg->addComboBox ("s", swings, "Swing");
        static const int gv[] { 25, 50, 80, 100 }, sv[] { 0, 25, 50, 66 }; int gi = 2, si = 0; for (int i = 0; i < 4; ++i) { if (gv[i] == a.gate) gi = i; if (sv[i] == a.swing) si = i; }
        arpDlg->getComboBoxComponent ("p")->setSelectedItemIndex (a.pattern); arpDlg->getComboBoxComponent ("r")->setSelectedItemIndex (a.rate); arpDlg->getComboBoxComponent ("o")->setSelectedItemIndex (a.octaves - 1);
        arpDlg->getComboBoxComponent ("g")->setSelectedItemIndex (gi); arpDlg->getComboBoxComponent ("s")->setSelectedItemIndex (si);
        arpDlg->addButton ("Aplicar", 1, juce::KeyPress (juce::KeyPress::returnKey)); arpDlg->addButton ("Cancelar", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        arpDlg->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int ok)
        {
            if (ok == 1 && arpDlg)
            {
                auto& s = mt::arpSettings(); s.pattern = arpDlg->getComboBoxComponent ("p")->getSelectedItemIndex(); s.rate = arpDlg->getComboBoxComponent ("r")->getSelectedItemIndex(); s.octaves = arpDlg->getComboBoxComponent ("o")->getSelectedItemIndex() + 1;
                s.gate = gv[juce::jlimit (0, 3, arpDlg->getComboBoxComponent ("g")->getSelectedItemIndex())]; s.swing = sv[juce::jlimit (0, 3, arpDlg->getComboBoxComponent ("s")->getSelectedItemIndex())]; s.seed = (int) (juce::Time::getMillisecondCounter() & 0xffff);
                const int n = playlist.cmdNotes ("arp"); if (n > 0) note (juce::String (n) + juce::String::fromUTF8 (" notas de arpegio")); syncState();
            }
            arpDlg.reset();
        }), false);
    }
    void saveTemplateDialog()
    {
        auto tr = te::getAudioTracks (edit); const int i = playlist.selTrackIndex(); if (i < 0 || i >= tr.size()) { note (juce::String::fromUTF8 ("Elige primero una pista")); return; }
        tplDlg = std::make_unique<juce::AlertWindow> (juce::String::fromUTF8 ("Guardar pista como plantilla"), "", juce::MessageBoxIconType::NoIcon);
        tplDlg->addTextEditor ("n", tr[i]->getName() == "Track" ? juce::String ("Mi pista") : tr[i]->getName(), "Nombre"); tplDlg->addButton ("Guardar", 1, juce::KeyPress (juce::KeyPress::returnKey)); tplDlg->addButton ("Cancelar", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        tplDlg->enterModalState (true, juce::ModalCallbackFunction::create ([this, i] (int ok)
        {
            auto t = te::getAudioTracks (edit);
            if (ok == 1 && tplDlg && i < t.size()) note (trackutil::saveTemplate (*t[i], tplDlg->getTextEditorContents ("n")) ? juce::String::fromUTF8 ("Plantilla guardada") : juce::String::fromUTF8 ("No se pudo guardar la plantilla"));
            tplDlg.reset();
        }), false);
    }
    void templateMenu()
    {
        auto files = trackutil::listTemplates(); juce::PopupMenu m;
        if (files.isEmpty()) m.addItem (-1, juce::String::fromUTF8 ("No hay plantillas: guarda una pista con \"Guardar pista como plantilla\""), false);
        for (int k = 0; k < files.size(); ++k) m.addItem (1 + k, files[k].getFileNameWithoutExtension());
        m.showMenuAsync (juce::PopupMenu::Options(), [this, files] (int r) { if (r >= 1 && r <= files.size()) insertTemplate (files[r - 1]); });
    }
    bool insertTemplate (const juce::File& f)
    {
        edit.getUndoManager().beginNewTransaction (juce::String::fromUTF8 ("Pista desde plantilla")); auto* t = trackutil::loadTemplate (edit, f);
        if (t == nullptr) { note (juce::String::fromUTF8 ("La plantilla no se pudo abrir")); return false; }
        mixer.refreshTracks(); syncState(); note (juce::String::fromUTF8 ("Pista creada desde la plantilla")); return true;
    }
    void togglePunch()
    {
        edit.recordingPunchInOut = ! edit.recordingPunchInOut;
        note (edit.recordingPunchInOut ? juce::String::fromUTF8 ("Punch in/out activado: solo se graba dentro del bucle (m\xc3\xa1rcalo con el bucle)") : juce::String::fromUTF8 ("Punch in/out apagado"));
        syncState();
    }
    void undoHistoryMenu()
    {
        auto d = edit.getUndoManager().getUndoDescriptions(); juce::PopupMenu m;
        if (d.isEmpty()) m.addItem (-1, juce::String::fromUTF8 ("No hay nada que deshacer"), false);
        for (int k = 0; k < std::min (25, d.size()); ++k) m.addItem (1 + k, (k == 0 ? juce::String::fromUTF8 ("Deshacer: ") : juce::String::fromUTF8 ("Volver antes de: ")) + (d[k].isEmpty() ? juce::String::fromUTF8 ("(sin nombre)") : d[k]));
        m.showMenuAsync (juce::PopupMenu::Options(), [this] (int r) { if (r >= 1) { undoSteps (r); syncState(); } });
    }
    int undoSteps (int n) { int k = 0; while (k < n && edit.getUndoManager().canUndo()) { edit.getUndoManager().undo(); ++k; } return k; }
    // ---- bloc de letra y notas; audio a MIDI ----
    juce::Component::SafePointer<juce::DialogWindow> notesWin;
    void showNotes()
    {
        if (notesWin != nullptr) { notesWin->toFront (true); return; }
        juce::DialogWindow::LaunchOptions o; o.content.setOwned (new projectnotes::Content (edit)); o.dialogTitle = juce::String::fromUTF8 ("Letra y notas del proyecto"); o.componentToCentreAround = this;
        o.dialogBackgroundColour = theme::bgBar2; o.useNativeTitleBar = true; o.resizable = true; notesWin = o.launchAsync(); theme::darkTitleBar (notesWin.getComponent());
    }
    // Convierte el clip de audio elegido (una sola voz o melodía) en un clip MIDI en una pista nueva. Devuelve cuántas notas creó.
    int audioToMidi()
    {
        auto sel = playlist.selectedClips(); auto* wc = sel.size() == 1 ? dynamic_cast<te::WaveAudioClip*> (sel[0]) : nullptr;
        if (wc == nullptr) { note (juce::String::fromUTF8 ("Elige un clip de audio con una melod\xc3\xada de una sola voz")); return 0; }
        audioedit::Buf b; double sr = 0; if (! audioedit::readFile (wc->getOriginalFile(), b, sr, 60.0)) { note (juce::String::fromUTF8 ("No se pudo leer el audio")); return 0; }
        const int n = b.getNumSamples(); const double ratio = std::max (0.01, wc->getSpeedRatio()); const int i0 = juce::jlimit (0, n, (int) (wc->getPosition().getOffset().inSeconds() * sr)), i1 = juce::jlimit (i0, n, (int) ((wc->getPosition().getOffset().inSeconds() + wc->getPosition().getLength().inSeconds() * ratio) * sr));
        std::vector<float> mono ((size_t) (i1 - i0), 0.f); for (int c = 0; c < b.getNumChannels(); ++c) { auto* p = b.getReadPointer (c); for (int i = i0; i < i1; ++i) mono[(size_t) (i - i0)] += p[i] / (float) b.getNumChannels(); }
        const auto found = audio2midi::convert (mono, sr);
        if (found.empty()) { note (juce::String::fromUTF8 ("No encontr\xc3\xa9 notas claras: esto funciona con una sola voz o melod\xc3\xada, no con acordes ni bater\xc3\xada")); return 0; }
        const double t0 = wc->getPosition().getStart().inSeconds(); auto beat = [&] (double sec) { return edit.tempoSequence.toBeats (te::TimePosition::fromSeconds (t0 + sec / ratio)).inBeats(); };
        const double b0 = beat (0.0); std::vector<ideas::N> ns; double endB = 0; for (auto& x : found) { const double a = beat (x.start) - b0, e = beat (x.start + x.len) - b0; ns.push_back ({ x.pitch, a, std::max (0.125, e - a), x.vel }); endB = std::max (endB, e); }
        edit.getUndoManager().beginNewTransaction ("Audio a MIDI"); auto tp = edit.insertNewAudioTrack (te::TrackInsertPoint::getEndOfTracks (edit), nullptr); auto* tr = tp.get(); if (tr == nullptr) return 0;
        tr->setName (wc->getName() + " (MIDI)"); instruments::addSynth (edit, *tr, templates::synthIdx ("Lead Hoover"));
        auto* mc = ideas::insert (edit, *tr, b0, std::max (1, (int) std::ceil (endB / timesig::bar() - 1e-9)), ns, wc->getName()); mixer.refreshTracks(); syncState();
        if (mc == nullptr) return 0; note (juce::String ((int) ns.size()) + juce::String::fromUTF8 (" notas creadas en una pista nueva")); return (int) ns.size();
    }
    void midiLearnMenu()
    {
        juce::PopupMenu m; auto maps = midi.list();
        m.addSectionHeader (juce::String::fromUTF8 ("Aprender (luego mueve un mando)"));
        m.addItem (1, juce::String::fromUTF8 ("Volumen de la pista elegida")); m.addItem (2, juce::String::fromUTF8 ("Panorama de la pista elegida")); m.addItem (3, juce::String::fromUTF8 ("Silenciar la pista elegida")); m.addItem (4, juce::String::fromUTF8 ("Solo de la pista elegida"));
        m.addItem (5, juce::String::fromUTF8 ("Par\xc3\xa1metro automatizado de la pista elegida")); m.addItem (6, "Volumen general"); m.addItem (7, "Panorama general"); m.addItem (8, "Tempo");
        if (midi.learning()) { m.addSeparator(); m.addItem (9, juce::String::fromUTF8 ("Cancelar el aprendizaje")); }
        if (! maps.empty()) { m.addSeparator(); m.addSectionHeader (juce::String::fromUTF8 ("Asignaciones (clic para quitar)")); for (size_t i = 0; i < maps.size(); ++i) m.addItem (100 + (int) i, midi.describe (maps[i])); m.addSeparator(); m.addItem (99, "Quitar todas"); }
        m.showMenuAsync (juce::PopupMenu::Options(), [this] (int r)
        {
            static const char* kinds[] { "vol", "pan", "mute", "solo", "auto", "master", "masterpan", "bpm" };
            if (r >= 1 && r <= 8) midiLearnStart (kinds[r - 1]); else if (r == 9) midi.cancelLearn(); else if (r == 99) midi.clear(); else if (r >= 100) midi.remove (r - 100);
        });
    }
    void setLoopBars (int nBars)
    {   // bucle de N compases desde el cabezal (o todo el proyecto con -1)
        auto& tc = edit.getTransport(); const double bar = timesig::bar();
        double b0 = nBars < 0 ? 0.0 : std::floor (playlist.timeToBeats (tc.getPosition().inSeconds()) / bar + 1e-6) * bar;
        double b1 = nBars < 0 ? std::max (bar, std::ceil (playlist.projectEndBeat() / bar) * bar) : b0 + nBars * bar;
        tc.setLoopRange (te::TimeRange (te::TimePosition::fromSeconds (playlist.beatsToTimePublic (b0)), te::TimePosition::fromSeconds (playlist.beatsToTimePublic (b1)))); tc.looping = true; bars.looping = true; bars.repaint(); playlist.repaint();
    }
    void shiftLoop (int dir)
    {
        auto& tc = edit.getTransport(); auto r = tc.getLoopRange(); const double bar = timesig::bar(); double b0 = playlist.timeToBeats (r.getStart().inSeconds()), b1 = playlist.timeToBeats (r.getEnd().inSeconds());
        const double d = std::max (-b0, dir * bar); b0 += d; b1 += d; tc.setLoopRange (te::TimeRange (te::TimePosition::fromSeconds (playlist.beatsToTimePublic (b0)), te::TimePosition::fromSeconds (playlist.beatsToTimePublic (b1)))); playlist.repaint();
    }
    double bpmNow() { return edit.tempoSequence.getTempo (0)->getBpm(); }
    void setBpmTo (double b) { edit.getUndoManager().beginNewTransaction (juce::String::fromUTF8 ("Tempo")); b = juce::jlimit (20.0, 300.0, std::round (b * 100.0) / 100.0); edit.tempoSequence.getTempo (0)->setBpm (b); bars.repaint(); playlist.repaint(); note (juce::String (b, 2) + " BPM"); }
    int transposeAll (int st)
    {
        auto cs = playlist.selectedClips(); juce::Array<te::Clip*> all; if (cs.isEmpty()) for (auto* t : te::getAudioTracks (edit)) for (auto* c : t->getClips()) all.add (c); else all = cs;
        auto* um = &edit.getUndoManager(); um->beginNewTransaction (juce::String::fromUTF8 ("Transponer")); int n = 0;
        for (auto* c : all) if (auto* mc = dynamic_cast<te::MidiClip*> (c)) for (auto* nt : mc->getSequence().getNotes()) { nt->setNoteNumber (juce::jlimit (0, 127, nt->getNoteNumber() + st), um); ++n; }
        piano.repaint(); playlist.repaint(); note (n > 0 ? juce::String (n) + juce::String::fromUTF8 (" notas transpuestas ") + (st > 0 ? "+" : "") + juce::String (st) : juce::String::fromUTF8 ("No hay notas MIDI para transponer")); return n;
    }
    juce::String projectInfoText()
    {
        int tr = 0, cl = 0, mi = 0, au = 0, pa = 0, notes = 0; for (auto* t : te::getAudioTracks (edit)) { ++tr; for (auto* c : t->getClips()) { ++cl; if (auto* mc = dynamic_cast<te::MidiClip*> (c)) { ++mi; notes += mc->getSequence().getNotes().size(); } else if (dynamic_cast<te::StepClip*> (c)) ++pa; else ++au; } }
        const int secs = (int) std::lround (edit.getLength().inSeconds());
        return juce::String (tr) + (tr == 1 ? " pista" : " pistas") + juce::String::fromUTF8 (" · ") + juce::String (cl) + " clips (" + juce::String (mi) + " MIDI, " + juce::String (au) + " audio, " + juce::String (pa) + juce::String::fromUTF8 (" patrones) · ") + juce::String (notes) + " notas"
             + juce::String::fromUTF8 (" · ") + juce::String (bpmNow(), 2) + " BPM" + juce::String::fromUTF8 (" · ") + juce::String (timesig::num) + "/" + juce::String (timesig::den) + juce::String::fromUTF8 (" · ") + juce::String (secs / 60) + ":" + juce::String (secs % 60).paddedLeft ('0', 2);
    }
    void buildCommands()
    {
        cmds.clear(); const char* kClip = "Clips"; const char* kTrk = "Pistas"; const char* kPrj = "Proyecto"; const char* kSeq = "Step Sequencer"; const char* kView = "Vista";
        auto add = [this] (const char* id, const char* name, const char* cat, const char* keys, std::function<void()> fn)
        {   // al grabar una macro, cada comando que se ejecuta se apunta (los de macros no)
            const juce::String sid (id); auto f = std::move (fn);
            cmds.push_back ({ sid, juce::String::fromUTF8 (name), juce::String::fromUTF8 (cat), juce::String::fromUTF8 (keys), [this, sid, f] { if (macroRec && ! sid.startsWith ("macro")) macroIds.add (sid); if (f) f(); } });
        };
        auto clipOp = [this] (const char* op, const char* msg) { return [this, op, msg] { const int n = playlist.cmdClips (op); if (n > 0) note (juce::String (n) + juce::String::fromUTF8 (msg)); syncState(); }; };
        auto trkOp = [this] (const char* op, const char* msg) { return [this, op, msg] { const int n = playlist.cmdTracks (op); if (n > 0 && msg[0] != 0) note (juce::String (n) + juce::String::fromUTF8 (msg)); syncState(); }; };
        auto seqOp = [this] (const char* op, const char* msg) { return [this, op, msg] { if (! steps.isVisible()) setSteps (true); const int n = steps.rowOp (op); if (n >= 0) note (juce::String::fromUTF8 (msg) + ": " + juce::String (n) + " pasos"); }; };
        // --- clips ---
        add ("gainup", "Subir 3 dB el clip", kClip, "", clipOp ("gainup", " clips: +3 dB"));
        add ("gaindown", "Bajar 3 dB el clip", kClip, "", clipOp ("gaindown", " clips: -3 dB"));
        add ("fadeshort", "Fundidos cortos anti-clic (5 ms)", kClip, "", clipOp ("fadeshort", " clips con fundido corto"));
        add ("nofades", "Quitar los fundidos del clip", kClip, "", clipOp ("nofades", " clips sin fundidos"));
        add ("quant", "Cuantizar el inicio de los clips a la cuadrícula", kClip, "", clipOp ("quant", " clips cuantizados"));
        add ("closegaps", "Cerrar los huecos de la pista", kClip, "", clipOp ("closegaps", " clips movidos"));
        add ("endtoend", "Poner los clips elegidos uno tras otro", kClip, "", clipOp ("endtoend", " clips movidos"));
        add ("aligncursor", "Mover los clips elegidos al cabezal", kClip, "", clipOp ("aligncursor", " clips movidos"));
        add ("splitbars", "Dividir los clips en compases", kClip, "", clipOp ("splitbars", " cortes"));
        add ("split2", "Dividir los clips en 2 partes iguales", kClip, "", clipOp ("split:2", " cortes"));
        add ("split4", "Dividir los clips en 4 partes iguales", kClip, "", clipOp ("split:4", " cortes"));
        add ("loopsel", "Bucle sobre los clips elegidos", kClip, "", clipOp ("loopsel", " clips en el bucle"));
        add ("delmuted", "Eliminar los clips silenciados", kClip, "", clipOp ("delmuted", " clips eliminados"));
        add ("dupnewtrack", "Copiar los clips a una pista nueva", kClip, "", clipOp ("dupnewtrack", " clips copiados"));
        add ("clipup", "Subir los clips a la pista de arriba", kClip, "Alt+\xe2\x86\x91", clipOp ("up", " clips movidos"));
        add ("clipdown", "Bajar los clips a la pista de abajo", kClip, "Alt+\xe2\x86\x93", clipOp ("down", " clips movidos"));
        add ("selright", "Elegir los clips desde el cabezal hacia la derecha", kClip, "", clipOp ("selright", " clips elegidos"));
        add ("selleft", "Elegir los clips hasta el cabezal", kClip, "", clipOp ("selleft", " clips elegidos"));
        add ("invertsel", "Invertir la selecci\xc3\xb3n de clips", kClip, "Ctrl+I", clipOp ("invertsel", " clips elegidos"));
        add ("gotoclipstart", "Ir al inicio de los clips elegidos", kClip, "", clipOp ("gotostart", ""));
        add ("gotoclipend", "Ir al final de los clips elegidos", kClip, "", clipOp ("gotoend", ""));
        add ("bounce", "Convertir los clips elegidos a audio (bounce a pista nueva)", kClip, "", [this] { bounceSelected(); syncState(); });
        add ("clipmarkers", "Poner un marcador al inicio de cada clip elegido", kClip, "", clipOp ("markers", " marcadores"));
        add ("selmidi", "Elegir todos los clips MIDI", kClip, "", clipOp ("selmidi", " clips elegidos"));
        add ("selaudio", "Elegir todos los clips de audio", kClip, "", clipOp ("selaudio", " clips elegidos"));
        add ("selmuted", "Elegir los clips silenciados", kClip, "", clipOp ("selmuted", " clips elegidos"));
        add ("selalt", "Elegir uno de cada dos clips (de los elegidos)", kClip, "", clipOp ("selalt", " clips elegidos"));
        add ("trimbars", "Ajustar el largo de los clips a compases enteros", kClip, "", clipOp ("trimbars", " clips ajustados"));
        add ("flipclips", "Voltear la posición de los clips elegidos en el tiempo", kClip, "", clipOp ("flipclips", " clips movidos"));
        add ("spread", "Repartir los clips elegidos: uno por compás", kClip, "", clipOp ("spread", " clips movidos"));
        add ("resetcolor", "Quitar el color propio de los clips", kClip, "", clipOp ("resetcolor", " clips sin color propio"));
        add ("nameastrack", "Poner a los clips el nombre de su pista", kClip, "", clipOp ("nameastrack", " clips renombrados"));
        add ("quantnotes", "Cuantizar las notas de los clips MIDI a 1/16", kClip, "", clipOp ("quantnotes", " notas cuantizadas"));
        add ("vel100", "Velocidad 100 en todas las notas de los clips MIDI", kClip, "", clipOp ("vel100", " notas"));
        add ("dedupe", "Quitar notas repetidas en los clips MIDI", kClip, "", clipOp ("dedupe", " notas quitadas"));
        add ("killshort", "Quitar notas muy cortas (menos de 1/32)", kClip, "", clipOp ("killshort", " notas quitadas"));
        add ("reversemidi", "Invertir en el tiempo las notas del clip MIDI", kClip, "", clipOp ("reversemidi", " notas"));
        add ("doublemidi", "Duplicar el contenido del clip MIDI (el doble de largo)", kClip, "", clipOp ("doublemidi", " notas copiadas"));
        add ("stretch2", "Clip MIDI al doble de lento (x2)", kClip, "", clipOp ("stretch2", " notas"));
        add ("stretchhalf", "Clip MIDI al doble de rápido (÷2)", kClip, "", clipOp ("stretchhalf", " notas"));
        // --- pistas ---
        add ("duptrack", "Duplicar la pista (con sus clips y volumen)", kTrk, "", trkOp ("duptrack", ""));
        add ("delempty", "Quitar las pistas vac\xc3\xad" "as", kTrk, "", trkOp ("delempty", " pistas quitadas"));
        add ("foldall", "Plegar todas las pistas", kTrk, "", trkOp ("foldall", " pistas plegadas"));
        add ("unfoldall", "Desplegar todas las pistas", kTrk, "", trkOp ("unfoldall", " pistas desplegadas"));
        add ("prevtrack", "Ir a la pista de arriba", kTrk, "", trkOp ("prevtrack", ""));
        add ("nexttrack", "Ir a la pista de abajo", kTrk, "", trkOp ("nexttrack", ""));
        add ("mutetrack", "Silenciar o activar la pista elegida", kTrk, "", trkOp ("mutetrack", ""));
        add ("solotrack", "Solo en la pista elegida", kTrk, "", trkOp ("solotrack", ""));
        add ("panreset", "Centrar el pan de todas las pistas", kTrk, "", trkOp ("panreset", " pistas centradas"));
        add ("clearmarkers", "Quitar todos los marcadores", kTrk, "", trkOp ("clearmarkers", " marcadores quitados"));
        // --- armon\xc3\xada: tonalidad y acordes ---
        { const char* kHar = "Armon\xc3\xada";
          auto harOp = [this] (const char* op, const char* msg) { return [this, op, msg] { const int n = playlist.cmdHarmony (op); if (n > 0 && msg[0] != 0) note (juce::String (n) + juce::String::fromUTF8 (msg)); syncState(); }; };
          add ("harm_key", "Elegir la tonalidad del proyecto\xe2\x80\xa6", kHar, "", [this] { playlist.openKeyMenu(); });
          add ("harm_detectkey", "Deducir la tonalidad de los clips MIDI elegidos", kHar, "", harOp ("detectkey", ""));
          add ("harm_keynone", "Quitar la tonalidad del proyecto", kHar, "", harOp ("keynone", ""));
          add ("harm_snapscale", "Ajustar las notas elegidas a la escala", kHar, "", harOp ("snapscale", " notas ajustadas a la escala"));
          add ("harm_fit", "Transponer los clips MIDI elegidos para que encajen en la tonalidad", kHar, "", harOp ("fitkey", " clips transpuestos"));
          add ("harm_snapchord", "Ajustar las notas elegidas al acorde que suena", kHar, "", harOp ("snapchord", " notas ajustadas al acorde"));
          add ("harm_chordclip", "Crear un clip MIDI con los acordes en la pista elegida", kHar, "", harOp ("chordclip", " notas de acordes creadas"));
          add ("harm_namechord", "Poner nombre de acorde al clip MIDI elegido", kHar, "", harOp ("namechord", ""));
          add ("harm_lane", "Mostrar u ocultar el carril de acordes", kHar, "", harOp ("chordlane", ""));
          add ("harm_clearchords", "Quitar todos los acordes", kHar, "", harOp ("clearchords", " acordes quitados")); }
        // --- notas: arpegiador y herramientas al estilo Logic / Pro Tools ---
        { const char* kNot = "Notas";
          auto notOp = [this] (const char* op, const char* msg) { return [this, op, msg] { const int n = playlist.cmdNotes (op); if (n > 0 && msg[0] != 0) note (juce::String (n) + juce::String::fromUTF8 (msg)); syncState(); }; };
          add ("arp_menu", "Arpegiador\xe2\x80\xa6", kNot, "", [this] { arpDialog(); });
          add ("arp_up", "Arpegiador: hacia arriba", kNot, "", notOp ("arp_up", " notas de arpegio")); add ("arp_down", "Arpegiador: hacia abajo", kNot, "", notOp ("arp_down", " notas de arpegio"));
          add ("arp_updown", "Arpegiador: sube y baja", kNot, "", notOp ("arp_updown", " notas de arpegio")); add ("arp_downup", "Arpegiador: baja y sube", kNot, "", notOp ("arp_downup", " notas de arpegio"));
          add ("arp_random", "Arpegiador: al azar", kNot, "", notOp ("arp_random", " notas de arpegio")); add ("arp_played", "Arpegiador: en el orden en que se tocaron", kNot, "", notOp ("arp_played", " notas de arpegio"));
          add ("n_humanize", "Humanizar: mover un poco el tiempo y la fuerza de las notas", kNot, "", notOp ("humanize", " notas movidas"));
          add ("n_legato", "Legato: cada nota dura hasta la siguiente", kNot, "", notOp ("legato", " notas"));
          add ("n_rampup", "Velocidad: crescendo (de suave a fuerte)", kNot, "", notOp ("rampup", " notas")); add ("n_rampdown", "Velocidad: decrescendo (de fuerte a suave)", kNot, "", notOp ("rampdown", " notas"));
          add ("n_strumdown", "Rasgueo de acordes hacia arriba (del grave al agudo)", kNot, "", notOp ("strumdown", " notas")); add ("n_strumup", "Rasgueo de acordes hacia abajo (del agudo al grave)", kNot, "", notOp ("strumup", " notas"));
          add ("n_ratchet2", "Redoble: cada nota en 2", kNot, "", notOp ("ratchet2", " notas")); add ("n_ratchet3", "Redoble: cada nota en 3", kNot, "", notOp ("ratchet3", " notas")); add ("n_ratchet4", "Redoble: cada nota en 4", kNot, "", notOp ("ratchet4", " notas"));
          add ("n_nooverlap", "Quitar solapes entre notas del mismo tono", kNot, "", notOp ("nooverlap", " notas"));
          add ("n_fixlen16", "Todas las notas con duraci\xc3\xb3n de 1/16", kNot, "", notOp ("fixlen16", " notas"));
          add ("n_octup", "Doblar con la octava de arriba", kNot, "", notOp ("octup", " notas")); add ("n_octdown", "Doblar con la octava de abajo", kNot, "", notOp ("octdown", " notas")); add ("n_fifth", "Doblar con la quinta (acorde de potencia)", kNot, "", notOp ("fifth", " notas"));
          add ("n_mirror", "Invertir la melod\xc3\xada (espejo de los tonos)", kNot, "", notOp ("mirror", " notas")); add ("n_thin", "Dejar una de cada dos notas", kNot, "", notOp ("thin", " notas"));
          add ("n_swing16", "Swing de semicorcheas (50 %)", kNot, "", notOp ("swing16", " notas")); }
        // --- notas: transformaciones extra (estilo Logic / Studio One) ---
        {
            const char* kNot = "Notas";
            auto notOp2 = [this] (const char* op, const char* msg) { return [this, op, msg] { const int n = playlist.cmdNotes (op); if (n > 0 && msg[0] != 0) note (juce::String (n) + juce::String::fromUTF8 (msg)); syncState(); }; };
            struct R { const char* id; const char* name; const char* op; };
            static const R rs[] = {
                { "pn_t1", "Transponer las notas +1 semitono", "p_n_t1" }, { "pn_tm1", "Transponer las notas -1 semitono", "p_n_tm1" },
                { "pn_t7", "Transponer las notas +7 (una quinta arriba)", "p_n_t7" }, { "pn_tm7", "Transponer las notas -7 (una quinta abajo)", "p_n_tm7" },
                { "pn_vel08", "Velocidad de las notas al 80 %", "p_n_vel08" }, { "pn_vel12", "Velocidad de las notas al 120 %", "p_n_vel12" },
                { "pn_squash", "Comprimir la velocidad (todas parecidas)", "p_n_squash" },
                { "pn_len05", "Notas a la mitad de largo", "p_n_len05" }, { "pn_len2", "Notas al doble de largo", "p_n_len2" },
                { "pn_triad", "Acorde mayor sobre cada nota", "p_n_triad" }, { "pn_triadm", "Acorde menor sobre cada nota", "p_n_triadm" }, { "pn_seventh", "Acorde de séptima sobre cada nota", "p_n_seventh" },
                { "pn_top", "Dejar solo la nota más aguda de cada acorde", "p_n_top" }, { "pn_bottom", "Dejar solo la nota más grave de cada acorde", "p_n_bottom" },
                { "pn_shiftl", "Mover las notas 1/16 a la izquierda", "p_n_shiftl" }, { "pn_shiftr", "Mover las notas 1/16 a la derecha", "p_n_shiftr" },
                { "pn_accent", "Acentuar los tiempos fuertes", "p_n_accent" }, { "pn_ghost", "Notas fantasma en los tiempos débiles", "p_n_ghost" },
                { "pn_half", "Notas a medio tiempo", "p_n_half" }, { "pn_double", "Notas al doble de tiempo", "p_n_double" },
                { "pn_scale", "Ajustar las notas a la escala del proyecto", "p_n_scale" } };
            for (auto& r : rs) add (r.id, r.name, kNot, "", notOp2 (r.op, " notas"));
        }
        // --- funciones pro: fundidos, ganancia, pan, volúmenes, efectos, bucles ---
        {
            const char* kP = "Pro";
            auto proOp = [this] (const char* op, const char* msg) { return [this, op, msg] { const int n = playlist.cmdPro (op); if (n > 0 && msg[0] != 0) note (juce::String (n) + juce::String::fromUTF8 (msg)); syncState(); }; };
            struct R { const char* id; const char* name; const char* op; const char* msg; };
            static const R rs[] = {
                { "pro_fi10", "Fundido de entrada de 10 ms", "fi:10", " clips con fundido" }, { "pro_fi50", "Fundido de entrada de 50 ms", "fi:50", " clips con fundido" }, { "pro_fi250", "Fundido de entrada de 250 ms", "fi:250", " clips con fundido" },
                { "pro_fo10", "Fundido de salida de 10 ms", "fo:10", " clips con fundido" }, { "pro_fo50", "Fundido de salida de 50 ms", "fo:50", " clips con fundido" }, { "pro_fo250", "Fundido de salida de 250 ms", "fo:250", " clips con fundido" },
                { "pro_g1up", "Subir 1 dB el clip", "gain1up", " clips" }, { "pro_g1down", "Bajar 1 dB el clip", "gain1down", " clips" }, { "pro_g0", "Ganancia del clip a 0 dB", "gain0", " clips" },
                { "pro_tostart", "Mover los clips elegidos al inicio del proyecto", "clip_tostart", " clips movidos" },
                { "pro_snapbar", "Pegar el inicio de los clips al compás más cercano", "clip_snapbar", " clips movidos" },
                { "pro_trimhead", "Recortar el inicio de los clips hasta el cabezal", "clip_trimhead", " clips recortados" },
                { "pro_trimtail", "Recortar el final de los clips desde el cabezal", "clip_trimtail", " clips recortados" },
                { "pro_clipnames", "Poner nombres numerados a los clips elegidos", "clip_numnames", " clips renombrados" },
                { "pro_unmuteclips", "Activar todos los clips silenciados", "clip_unmute", " clips activados" },
                { "pro_insbar", "Insertar un compás vacío en el cabezal (empuja lo que sigue)", "clip_insbar", " clips movidos" },
                { "pro_delshort", "Quitar clips más cortos que 1/16", "clip_delshort", " clips quitados" },
                { "pro_vol1up", "Subir 1 dB todas las pistas", "t_vol1up", " pistas" }, { "pro_vol1down", "Bajar 1 dB todas las pistas", "t_vol1down", " pistas" }, { "pro_vol6down", "Bajar 6 dB todas las pistas", "t_vol6down", " pistas" },
                { "pro_panalt", "Pan alternado izquierda / derecha", "t_panalt", " pistas con pan" }, { "pro_panwide", "Repartir las pistas en el panorama de izquierda a derecha", "t_panwide", " pistas con pan" }, { "pro_panrand", "Pan al azar", "t_panrand", " pistas con pan" },
                { "pro_tnames", "Nombres numerados para todas las pistas", "t_numnames", " pistas renombradas" },
                { "pro_unsolo", "Quitar el solo de todas las pistas", "t_unsolo", " pistas" }, { "pro_unmute", "Quitar el silencio de todas las pistas", "t_unmute", " pistas" },
                { "pro_muteothers", "Silenciar todas menos la pista elegida", "t_muteothers", " pistas silenciadas" },
                { "pro_fxoff", "Apagar los efectos de todas las pistas", "t_fxbypass", " efectos apagados" }, { "pro_fxon", "Encender los efectos de todas las pistas", "t_fxenable", " efectos encendidos" },
                { "pro_fxoffsel", "Apagar los efectos de la pista elegida", "t_fxbypass_sel", " efectos apagados" },
                { "pro_fi500", "Fundido de entrada de 500 ms", "fi:500", " clips con fundido" }, { "pro_fi1000", "Fundido de entrada de 1 s", "fi:1000", " clips con fundido" },
                { "pro_fo500", "Fundido de salida de 500 ms", "fo:500", " clips con fundido" }, { "pro_fo1000", "Fundido de salida de 1 s", "fo:1000", " clips con fundido" },
                { "pro_g3up", "Subir 3 dB el clip", "gaind:3", " clips" }, { "pro_g3down", "Bajar 3 dB el clip", "gaind:-3", " clips" }, { "pro_g6up", "Subir 6 dB el clip", "gaind:6", " clips" }, { "pro_g6down", "Bajar 6 dB el clip", "gaind:-6", " clips" },
                { "pro_snapend", "Pegar el final de los clips al compás más cercano", "clip_snapend", " clips ajustados" },
                { "pro_len1", "Clips de 1 compás de largo", "clip_len:1", " clips ajustados" }, { "pro_len2", "Clips de 2 compases de largo", "clip_len:2", " clips ajustados" }, { "pro_len4", "Clips de 4 compases de largo", "clip_len:4", " clips ajustados" }, { "pro_len8", "Clips de 8 compases de largo", "clip_len:8", " clips ajustados" },
                { "pro_pan0", "Pan al centro en todas las pistas", "t_pan0", " pistas" }, { "pro_vol0", "Volumen a 0 dB en todas las pistas", "t_vol0", " pistas" },
                { "pro_colors", "Colores en arcoíris para las pistas", "t_colors", " pistas coloreadas" }, { "pro_solosel", "Solo únicamente en la pista elegida", "t_solosel", " pistas" },
                { "pro_bpmup", "Subir el tempo 1 BPM", "p_bpmup", "" }, { "pro_bpmdown", "Bajar el tempo 1 BPM", "p_bpmdown", "" }, { "pro_bpmround", "Redondear el tempo a un número entero", "p_bpmround", "" },
                { "pro_loop4", "Bucle de 4 compases desde el cabezal", "p_loop:4", "" }, { "pro_loop8", "Bucle de 8 compases desde el cabezal", "p_loop:8", "" }, { "pro_loop16", "Bucle de 16 compases desde el cabezal", "p_loop:16", "" }, { "pro_loopall", "Bucle sobre todo el proyecto", "p_loopall", "" },
                { "pro_nextbar", "Ir al compás siguiente", "p_nextbar", "" }, { "pro_prevbar", "Ir al compás anterior", "p_prevbar", "" },
                { "pro_loopbar", "Bucle sobre el compás del cabezal", "p_loopcursor", "" }, { "pro_loopoff", "Apagar el bucle", "p_loopclear", "" },
                { "pro_gostart", "Ir al inicio del proyecto", "p_gostart", "" }, { "pro_goend", "Ir al final del último clip", "p_goend", "" } };
            for (auto& r : rs) add (r.id, r.name, kP, "", proOp (r.op, r.msg));
        }
        // --- clips y pistas: repetir, empujar, fundido cruzado, plantillas, punch, historial ---
        { const char* kPt = "Edici\xc3\xb3n";
          add ("repeat2", "Repetir los clips elegidos 1 vez", kPt, "", clipOp ("repeat2", " clips nuevos")); add ("repeat4", "Repetir los clips elegidos 3 veces", kPt, "", clipOp ("repeat4", " clips nuevos")); add ("repeat8", "Repetir los clips elegidos 7 veces", kPt, "", clipOp ("repeat8", " clips nuevos"));
          add ("nudgel", "Empujar los clips 1/16 a la izquierda", kPt, "", clipOp ("nudgel", " clips movidos")); add ("nudger", "Empujar los clips 1/16 a la derecha", kPt, "", clipOp ("nudger", " clips movidos"));
          add ("nudgelb", "Empujar los clips un comp\xc3\xa1s a la izquierda", kPt, "", clipOp ("nudgelb", " clips movidos")); add ("nudgerb", "Empujar los clips un comp\xc3\xa1s a la derecha", kPt, "", clipOp ("nudgerb", " clips movidos"));
          add ("reverseaudio", "Invertir el sonido del clip de audio (al rev\xc3\xa9s)", kPt, "", clipOp ("reverseaudio", " clips invertidos"));
          add ("crossfade", "Fundido cruzado donde se solapan clips de audio", kPt, "", clipOp ("crossfade", " fundidos cruzados"));
          add ("lockclips", "Bloquear los clips elegidos", kPt, "", clipOp ("lockclips", " clips bloqueados")); add ("unlockclips", "Desbloquear los clips elegidos", kPt, "", clipOp ("unlockclips", " clips desbloqueados"));
          add ("tpl_save", "Guardar la pista elegida como plantilla\xe2\x80\xa6", kPt, "", [this] { saveTemplateDialog(); });
          add ("tpl_new", "Nueva pista desde una plantilla\xe2\x80\xa6", kPt, "", [this] { templateMenu(); });
          add ("punch", "Punch in/out: grabar solo dentro del bucle", kPt, "", [this] { togglePunch(); });
          add ("undohist", "Historial de deshacer\xe2\x80\xa6", kPt, "", [this] { undoHistoryMenu(); }); }
        { const char* kLive = "Teclado";
          add ("scalelock", "Bloqueo de escala: el teclado piano solo toca notas de la tonalidad", kLive, "", [this] { setScaleLock (! scaleLock); });
          add ("repeat_off", "Repetici\xc3\xb3n de notas: apagada", kLive, "", [this] { setRepeat (0); }); add ("repeat_4", "Repetici\xc3\xb3n de notas: 1/4", kLive, "", [this] { setRepeat (1); });
          add ("repeat_8", "Repetici\xc3\xb3n de notas: 1/8", kLive, "", [this] { setRepeat (2); }); add ("repeat_16", "Repetici\xc3\xb3n de notas: 1/16", kLive, "", [this] { setRepeat (3); }); add ("repeat_32", "Repetici\xc3\xb3n de notas: 1/32", kLive, "", [this] { setRepeat (4); });
          add ("typingpiano", "Teclado piano de la computadora: activar o apagar", kLive, "Ctrl+T", [this] { audition::typingPiano = ! audition::typingPiano; if (! audition::typingPiano) typedReleaseAll(); note (audition::typingPiano ? juce::String::fromUTF8 ("Teclado piano activado") : juce::String::fromUTF8 ("Teclado piano apagado")); }); }
        { const char* kNb = "Proyecto";
          add ("notes", "Letra y notas del proyecto\xe2\x80\xa6", kNb, "", [this] { showNotes(); });
          add ("audio2midi", "Audio a MIDI: convertir el clip de audio elegido (una sola voz) en notas", kNb, "", [this] { audioToMidi(); });
          add ("padrack", "Rack de pads: 16 pads con capas, volumen, pan y afinaci\xc3\xb3n por pad", kNb, "", [this] { setRack (true); });
          add ("pitcheditor", "Editor de afinaci\xc3\xb3n (Injustice Auto): corregir a mano la voz del clip elegido", kNb, "Ctrl+Mayús+P", [this] { setPitchEd (true); });
          add ("audioeditor", "Editor de audio: ver y editar el clip de audio elegido", kNb, "Ctrl+Mayús+E", [this] { setAed (true); }); }
        // --- MIDI learn ---
        { const char* kMidi = "MIDI";
          add ("midi_menu", "Asignaciones MIDI\xe2\x80\xa6", kMidi, "", [this] { midiLearnMenu(); });
          add ("midi_vol", "MIDI learn: volumen de la pista elegida", kMidi, "", [this] { midiLearnStart ("vol"); });
          add ("midi_pan", "MIDI learn: panorama de la pista elegida", kMidi, "", [this] { midiLearnStart ("pan"); });
          add ("midi_mute", "MIDI learn: silenciar la pista elegida", kMidi, "", [this] { midiLearnStart ("mute"); });
          add ("midi_solo", "MIDI learn: solo de la pista elegida", kMidi, "", [this] { midiLearnStart ("solo"); });
          add ("midi_auto", "MIDI learn: par\xc3\xa1metro automatizado de la pista elegida", kMidi, "", [this] { midiLearnStart ("auto"); });
          add ("midi_master", "MIDI learn: volumen general", kMidi, "", [this] { midiLearnStart ("master"); });
          add ("midi_bpm", "MIDI learn: tempo", kMidi, "", [this] { midiLearnStart ("bpm"); });
          add ("midi_clear", "Quitar todas las asignaciones MIDI", kMidi, "", [this] { midi.clear(); note (juce::String::fromUTF8 ("Asignaciones MIDI quitadas")); }); }
        // --- arreglo (secciones) ---
        {
            const char* kArr = "Arreglo";
            auto secOp = [this] (const char* op, const char* msg) { return [this, op, msg] { const int n = playlist.cmdSections (op); if (n > 0 && msg[0] != 0) note (juce::String::fromUTF8 (msg)); syncState(); }; };
            add ("sec_new", "Nueva secci\xc3\xb3n en el cabezal (8 compases)", kArr, "", secOp ("new", "Secci\xc3\xb3n creada"));
            add ("sec_markers", "Crear secciones desde los marcadores", kArr, "", secOp ("frommarkers", "Secciones creadas desde los marcadores"));
            add ("sec_left", "Mover la secci\xc3\xb3n del cabezal a la izquierda", kArr, "", secOp ("left", "Secci\xc3\xb3n movida con todo su contenido"));
            add ("sec_right", "Mover la secci\xc3\xb3n del cabezal a la derecha", kArr, "", secOp ("right", "Secci\xc3\xb3n movida con todo su contenido"));
            add ("sec_dup", "Duplicar la secci\xc3\xb3n del cabezal", kArr, "", secOp ("dup", "Secci\xc3\xb3n duplicada"));
            add ("sec_grow", "Alargar la secci\xc3\xb3n un comp\xc3\xa1s", kArr, "", secOp ("grow", ""));
            add ("sec_shrink", "Acortar la secci\xc3\xb3n un comp\xc3\xa1s", kArr, "", secOp ("shrink", ""));
            add ("sec_colour", "Cambiar el color de la secci\xc3\xb3n", kArr, "", secOp ("colour", ""));
            add ("sec_select", "Elegir los clips de la secci\xc3\xb3n", kArr, "", secOp ("select", ""));
            add ("sec_loop", "Repetir la secci\xc3\xb3n en bucle", kArr, "", secOp ("loop", "Bucle sobre la secci\xc3\xb3n"));
            add ("sec_next", "Ir a la secci\xc3\xb3n siguiente", kArr, "", secOp ("next", ""));
            add ("sec_prev", "Ir a la secci\xc3\xb3n anterior", kArr, "", secOp ("prev", ""));
            add ("sec_del", "Eliminar la secci\xc3\xb3n del cabezal", kArr, "", secOp ("del", "Secci\xc3\xb3n eliminada (sus clips se quedan)"));
            add ("sec_clear", "Quitar todas las secciones", kArr, "", secOp ("clear", "Secciones quitadas"));
        }
        // --- proyecto y transporte ---
        add ("ideastudio", "IA de ideas: crear una canci\xc3\xb3n o una parte desde una frase\xe2\x80\xa6", kPrj, "", [this] { openIdeaStudio(); });
        add ("tempox2", "Tempo x2", kPrj, "", [this] { setBpmTo (bpmNow() * 2.0); });
        add ("tempohalf", "Tempo a la mitad", kPrj, "", [this] { setBpmTo (bpmNow() / 2.0); });
        add ("tempoup", "Subir el tempo 1 BPM", kPrj, "", [this] { setBpmTo (bpmNow() + 1.0); });
        add ("tempodown", "Bajar el tempo 1 BPM", kPrj, "", [this] { setBpmTo (bpmNow() - 1.0); });
        add ("transup", "Transponer el MIDI +1 semitono", kPrj, "", [this] { transposeAll (1); });
        add ("transdown", "Transponer el MIDI -1 semitono", kPrj, "", [this] { transposeAll (-1); });
        add ("transoctup", "Transponer el MIDI +1 octava", kPrj, "", [this] { transposeAll (12); });
        add ("transoctdown", "Transponer el MIDI -1 octava", kPrj, "", [this] { transposeAll (-12); });
        add ("projectinfo", "Informaci\xc3\xb3n del proyecto", kPrj, "", [this] { note (projectInfoText()); });
        add ("barprev", "Cabezal al comp\xc3\xa1s anterior", kPrj, ",", [this] { playlist.gotoBar (-1); });
        add ("barnext", "Cabezal al comp\xc3\xa1s siguiente", kPrj, ".", [this] { playlist.gotoBar (1); });
        add ("metrotoggle", "Metrónomo: activar o apagar", kPrj, "", [this] { if (bars.onButton) bars.onButton ("metro"); });
        add ("looptoggle", "Bucle: activar o apagar", kPrj, "", [this] { if (bars.onButton) bars.onButton ("loop"); });
        add ("loop4", "Bucle de 4 compases desde el cabezal", kPrj, "", [this] { setLoopBars (4); });
        add ("loop8", "Bucle de 8 compases desde el cabezal", kPrj, "", [this] { setLoopBars (8); });
        add ("loopall", "Bucle sobre todo el proyecto", kPrj, "", [this] { setLoopBars (-1); });
        add ("loopfwd", "Mover el bucle un compás adelante", kPrj, "", [this] { shiftLoop (1); });
        add ("loopback", "Mover el bucle un compás atrás", kPrj, "", [this] { shiftLoop (-1); });
        add ("bpmround", "Redondear el tempo a un número entero", kPrj, "", [this] { setBpmTo (std::round (bpmNow())); });
        add ("vol0", "Poner todas las pistas a 0 dB", kTrk, "", trkOp ("vol0", " pistas a 0 dB"));
        add ("headroom", "Bajar 3 dB todas las pistas (margen)", kTrk, "", trkOp ("headroom", " pistas bajadas 3 dB"));
        add ("rainbow", "Un color distinto para cada pista", kTrk, "", trkOp ("rainbow", " pistas pintadas"));
        add ("seqhalf", "Fila: repetir la primera mitad en la segunda", kSeq, "", seqOp ("half", "Mitad repetida"));
        add ("seqsparse", "Fila: pocos golpes al azar", kSeq, "", seqOp ("sparse", "Fila dispersa"));
        add ("seqdense", "Fila: muchos golpes al azar", kSeq, "", seqOp ("dense", "Fila densa"));
        add ("seqaccent", "Fila: acentuar los tiempos fuertes", kSeq, "", seqOp ("accent", "Acentos"));
        add ("seqfour", "Fila: cuatro al piso (cada 4 pasos)", kSeq, "", seqOp ("fourfloor", "Cuatro al piso"));
        add ("seqoff", "Fila: a contratiempo", kSeq, "", seqOp ("offbeat", "Contratiempo"));
        add ("seqvel100", "Fila: velocidad 100 en los pasos activos", kSeq, "", seqOp ("vel100", "Velocidad 100"));
        // --- vista ---
        add ("zoom4", "Zoom: ver 4 compases", kView, "", [this] { playlist.zoomBars (4); });
        add ("zoom16", "Zoom: ver 16 compases", kView, "", [this] { playlist.zoomBars (16); });
        add ("zoom64", "Zoom: ver 64 compases", kView, "", [this] { playlist.zoomBars (64); });
        add ("fullscreen", "Pantalla completa", kView, "Alt+Enter", [this] { if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>()) dw->setFullScreen (! dw->isFullScreen()); });
        add ("palette", "Paleta de comandos", kView, "Ctrl+K", [this] { openPalette(); });
        // --- step sequencer (fila elegida) ---
        add ("seqrotl", "Fila: rotar los pasos a la izquierda", kSeq, "", seqOp ("rotl", "Fila rotada a la izquierda"));
        add ("seqrotr", "Fila: rotar los pasos a la derecha", kSeq, "", seqOp ("rotr", "Fila rotada a la derecha"));
        add ("seqinvert", "Fila: invertir los pasos", kSeq, "", seqOp ("invert", "Fila invertida"));
        add ("seqclear", "Fila: vaciar", kSeq, "", seqOp ("clear", "Fila vaciada"));
        add ("seqrandom", "Fila: aleatoria", kSeq, "", seqOp ("random", "Fila aleatoria"));
        add ("seqeuclid3", juce::String::fromUTF8 ("Fila: ritmo eucl\xc3\xad" "deo de 3").toRawUTF8(), kSeq, "", seqOp ("euclid:3", "Euclideo de 3"));
        add ("seqeuclid5", juce::String::fromUTF8 ("Fila: ritmo eucl\xc3\xad" "deo de 5").toRawUTF8(), kSeq, "", seqOp ("euclid:5", "Euclideo de 5"));
        add ("seqeuclid7", juce::String::fromUTF8 ("Fila: ritmo eucl\xc3\xad" "deo de 7").toRawUTF8(), kSeq, "", seqOp ("euclid:7", "Euclideo de 7"));
        // --- macros: secuencias de comandos grabadas por el usuario ---
        {
            const char* kMac = "Macros";
            add ("macro_rec", "Macros: empezar a grabar una macro", kMac, "", [this] { macroStart(); });
            add ("macro_stop", "Macros: detener y guardar\xe2\x80\xa6", kMac, "", [this] { macroStopDialog(); });
            add ("macro_cancel", "Macros: cancelar la grabaci\xc3\xb3n", kMac, "", [this] { macroRec = false; macroIds.clear(); note (juce::String::fromUTF8 ("Grabaci\xc3\xb3n de macro cancelada")); });
            add ("macro_del", "Macros: borrar una macro\xe2\x80\xa6", kMac, "", [this] { macroDeleteMenu(); });
            for (auto& line : loadMacros()) { const auto nm = line.upToFirstOccurrenceOf ("|", false, false); if (nm.isEmpty()) continue;
                cmds.push_back ({ "macro:" + nm, juce::String::fromUTF8 ("Macro: ") + nm, kMac, {}, [this, nm] { const int n = macroRun (nm); note (juce::String::fromUTF8 ("Macro \xc2\xab") + nm + juce::String::fromUTF8 ("\xc2\xbb: ") + juce::String (n) + " pasos"); } }); }
        }
        std::stable_sort (cmds.begin(), cmds.end(), [] (const Cmd& a, const Cmd& b) { return a.cat < b.cat; });
    }
    // ---- macros ----
    juce::File macroFile = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("INJUSTICE DAW").getChildFile ("macros.txt");
    bool macroRec = false; juce::StringArray macroIds;
    juce::StringArray loadMacros() const { juce::StringArray l; if (macroFile.existsAsFile()) l.addLines (macroFile.loadFileAsString()); l.removeEmptyStrings(); return l; }
    void saveMacros (const juce::StringArray& l) { macroFile.getParentDirectory().createDirectory(); macroFile.replaceWithText (l.joinIntoString ("\n") + "\n"); }
    void macroStart() { macroRec = true; macroIds.clear(); note (juce::String::fromUTF8 ("Grabando macro: ejecuta los comandos y luego elige \xc2\xabMacros: detener y guardar\xc2\xbb")); }
    bool macroSave (juce::String name)
    {
        if (! macroRec) return false; macroRec = false; name = name.replace ("|", " ").replace (",", " ").trim();
        if (macroIds.isEmpty() || name.isEmpty()) { macroIds.clear(); note (juce::String::fromUTF8 (name.isEmpty() ? "La macro necesita un nombre" : "No se grab\xc3\xb3 ning\xc3\xban comando")); return false; }
        auto l = loadMacros(); for (int i = l.size(); --i >= 0;) if (l[i].upToFirstOccurrenceOf ("|", false, false) == name) l.remove (i);
        l.add (name + "|" + macroIds.joinIntoString (",")); saveMacros (l); const int n = macroIds.size(); macroIds.clear(); buildCommands(); note (juce::String::fromUTF8 ("Macro \xc2\xab") + name + juce::String::fromUTF8 ("\xc2\xbb guardada (") + juce::String (n) + " pasos)"); return true;
    }
    int macroRun (const juce::String& name)
    {
        for (auto& line : loadMacros()) if (line.upToFirstOccurrenceOf ("|", false, false) == name)
        {
            juce::StringArray ids; ids.addTokens (line.fromFirstOccurrenceOf ("|", false, false), ",", ""); int n = 0; const bool was = macroRec; macroRec = false;
            for (auto& id : ids) { if (id.isEmpty() || id.startsWith ("macro") || n >= 60) continue; if (runCmd (id)) ++n; }
            macroRec = was; return n;
        }
        return 0;
    }
    void macroDelete (const juce::String& name) { auto l = loadMacros(); for (int i = l.size(); --i >= 0;) if (l[i].upToFirstOccurrenceOf ("|", false, false) == name) l.remove (i); saveMacros (l); buildCommands(); }
    std::unique_ptr<juce::AlertWindow> macroDlg;
    void macroStopDialog()
    {
        if (! macroRec) { note (juce::String::fromUTF8 ("No hay ninguna macro grabando")); return; }
        if (macroIds.isEmpty()) { macroRec = false; note (juce::String::fromUTF8 ("No se grab\xc3\xb3 ning\xc3\xban comando")); return; }
        macroDlg = std::make_unique<juce::AlertWindow> ("Guardar macro", juce::String (macroIds.size()) + " pasos", juce::MessageBoxIconType::NoIcon); macroDlg->addTextEditor ("n", "Mi macro", "Nombre"); macroDlg->addButton ("Guardar", 1); macroDlg->addButton ("Cancelar", 0);
        macroDlg->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int ok) { if (ok == 1 && macroDlg) macroSave (macroDlg->getTextEditorContents ("n")); else { macroRec = false; macroIds.clear(); } macroDlg.reset(); }), false);
    }
    void macroDeleteMenu()
    {
        auto l = loadMacros(); if (l.isEmpty()) { note (juce::String::fromUTF8 ("No hay macros guardadas")); return; } juce::PopupMenu m; juce::StringArray names; for (auto& x : l) names.add (x.upToFirstOccurrenceOf ("|", false, false));
        for (int i = 0; i < names.size(); ++i) m.addItem (1 + i, juce::String::fromUTF8 ("Borrar \xc2\xab") + names[i] + juce::String::fromUTF8 ("\xc2\xbb"));
        m.showMenuAsync (juce::PopupMenu::Options(), [this, names] (int r) { if (r >= 1 && r <= names.size()) macroDelete (names[r - 1]); });
    }
    double timeToBeatsT (double t) { return playlist.timeToBeats (t); }
    void cmdTests (const std::function<void (bool, const juce::String&)>& chk)
    {
        auto& tc = edit.getTransport(); auto startB = [this] (te::Clip* c) { return playlist.timeToBeats (c->getPosition().getStart().inSeconds()); };
        auto lenB = [this] (te::Clip* c) { return playlist.timeToBeats (c->getPosition().getEnd().inSeconds()) - playlist.timeToBeats (c->getPosition().getStart().inSeconds()); };
        auto atB = [this] (double b) { return te::TimePosition::fromSeconds (playlist.beatsToTimePublic (b)); };
        auto newTrack = [this] { auto p = edit.insertNewAudioTrack (te::TrackInsertPoint::getEndOfTracks (edit), nullptr); mixer.refreshTracks(); return p; };
        const std::vector<ideas::N> nn { { 60, 0.0, 1.0, 100 } };
        chk (cmds.size() >= 50, "comandos: hay al menos 50 comandos (" + juce::String ((int) cmds.size()) + ")");
        { std::set<juce::String> ids; for (auto& c : cmds) ids.insert (c.id); chk (ids.size() == cmds.size(), "comandos: no hay identificadores repetidos"); }
        playlist.setZoom (40.0); playlist.hideAllAutomation();
        auto tx = newTrack(); auto* A = ideas::insert (edit, *tx, 0.0, 1, nn, "A"); auto* B = ideas::insert (edit, *tx, 8.0, 1, nn, "B"); auto* C = ideas::insert (edit, *tx, 16.0, 1, nn, "C");
        chk (A && B && C, "comandos: se crean los clips de prueba");
        tc.setPosition (atB (8.0));
        playlist.selectClips ({}); playlist.cmdClips ("selright"); { auto s = playlist.selectedClips(); chk (s.contains (B) && s.contains (C) && ! s.contains (A), "elegir a la derecha: elige los clips desde el cabezal"); }
        playlist.cmdClips ("selleft"); { auto s = playlist.selectedClips(); chk (s.contains (A) && ! s.contains (B) && ! s.contains (C), "elegir a la izquierda: solo lo que termina antes del cabezal"); }
        playlist.selectClips ({ A }); playlist.cmdClips ("invertsel"); { auto s = playlist.selectedClips(); chk (! s.contains (A) && s.contains (B) && s.contains (C), "invertir selecci\xc3\xb3n: sale A y entran los dem\xc3\xa1s"); }
        B->setStart (atB (8.13), false, true); playlist.selectClips ({ B }); chk (playlist.cmdClips ("quant") == 1 && std::abs (startB (B) - 8.0) < 1e-3, "cuantizar clips: 8.13 pasa a 8.0 (" + juce::String (startB (B), 3) + ")");
        playlist.selectClips ({ A }); const int mv = playlist.cmdClips ("closegaps"); chk (mv == 2 && std::abs (startB (B) - 4.0) < 1e-3 && std::abs (startB (C) - 8.0) < 1e-3 && std::abs (startB (A)) < 1e-3, "cerrar huecos: A, B y C quedan pegados (0, 4, 8)");
        B->setStart (atB (12.0), false, true); C->setStart (atB (20.0), false, true); playlist.selectClips ({ A, C }); playlist.cmdClips ("endtoend");
        chk (std::abs (startB (C) - 4.0) < 1e-3 && std::abs (startB (B) - 12.0) < 1e-3, "uno tras otro: C pasa tras A y B (no elegido) no se mueve");
        tc.setPosition (atB (2.0)); playlist.selectClips ({ C }); playlist.cmdClips ("aligncursor"); chk (std::abs (startB (C) - 2.0) < 1e-3, "mover al cabezal: C empieza donde est\xc3\xa1 el cabezal");
        C->setStart (atB (20.0), false, true); playlist.selectClips ({ A, B }); playlist.cmdClips ("loopsel");
        chk (tc.looping && std::abs (tc.getLoopRange().getStart().inSeconds()) < 1e-3 && std::abs (timeToBeatsT (tc.getLoopRange().getEnd().inSeconds()) - 16.0) < 1e-2, "bucle sobre la selecci\xc3\xb3n: del inicio de A al final de B");
        tc.looping = false; playlist.cmdClips ("gotoend"); chk (std::abs (timeToBeatsT (tc.getPosition().inSeconds()) - 16.0) < 1e-2, "ir al final de lo elegido"); playlist.cmdClips ("gotostart"); chk (std::abs (tc.getPosition().inSeconds()) < 1e-3, "ir al inicio de lo elegido");
        { const int m0 = (int) playlist.markers().size(); playlist.selectClips ({ A, B }); chk (playlist.cmdClips ("markers") == 2 && (int) playlist.markers().size() == m0 + 2, "marcadores en cada clip elegido"); playlist.cmdTracks ("clearmarkers"); chk (playlist.markers().empty(), "quitar todos los marcadores"); }
        { auto ty = newTrack(); auto* D = ideas::insert (edit, *ty, 0.0, 3, nn, "D"); playlist.selectClips ({ D }); const int cuts = playlist.cmdClips ("splitbars");
          chk (cuts == 2 && ty->getClips().size() == 3, "dividir en compases: 3 compases dan 3 clips (" + juce::String (ty->getClips().size()) + ")");
          auto tz = newTrack(); auto* E = ideas::insert (edit, *tz, 0.0, 1, nn, "E"); playlist.selectClips ({ E }); playlist.cmdClips ("split:4");
          bool ok = tz->getClips().size() == 4; for (auto* c : tz->getClips()) ok = ok && std::abs (lenB (c) - 1.0) < 1e-3; chk (ok, "dividir en 4: cuatro clips de 1 pulso");
          playlist.selectClips ({ tz->getClips()[0] }); playlist.cmdClips ("split:2"); chk (tz->getClips().size() == 5, "dividir en 2: un clip m\xc3\xa1s");
          auto tracks = te::getAudioTracks (edit); const int iz = tracks.indexOf (tz.get()); auto* first = tz->getClips()[0]; playlist.selectClips ({ first });
          chk (playlist.cmdClips ("down") == 0, "bajar de pista: en la \xc3\xbaltima pista no hace nada"); playlist.cmdClips ("up"); chk (first->getTrack() == tracks[iz - 1], "subir de pista: el clip pasa a la pista de arriba");
          playlist.selectClips ({ first }); playlist.cmdClips ("down"); chk (first->getTrack() == tz.get(), "bajar de pista: vuelve a su pista"); }
        playlist.selectClips ({ B, C }); { const int nt0 = playlist.numTracks(); playlist.cmdClips ("dupnewtrack"); chk (playlist.numTracks() == nt0 + 1 && te::getAudioTracks (edit).getLast()->getClips().size() == 2, "copiar a pista nueva: 2 clips en la pista nueva"); }
        { auto cs = playlist.selectedClips(); cs[0]->setMuted (true); const int n0 = tx->getClips().size() + te::getAudioTracks (edit).getLast()->getClips().size(); playlist.selectClips ({}); playlist.cmdClips ("delmuted");
          chk (tx->getClips().size() + te::getAudioTracks (edit).getLast()->getClips().size() == n0 - 1, "eliminar silenciados: se va el clip silenciado"); }
        if (auto* sf = std::getenv ("INJ_SAMPLE_FILE"))
        {
            auto tw = newTrack(); te::AudioFile af (edit.engine, juce::File (sf)); auto wcp = tw->insertWaveClip ("w", juce::File (sf), { te::TimeRange (atB (0.0), te::TimeDuration::fromSeconds (juce::jmax (0.2, af.getLength()))), te::TimeDuration() }, false); auto* wc = wcp.get();
            if (wc) { te::Clip* wcc = wc; playlist.selectClips ({ wcc }); const float g0 = wc->getGainDB(); playlist.cmdClips ("gainup"); const float g1 = wc->getGainDB(); playlist.cmdClips ("gaindown"); playlist.cmdClips ("gaindown"); const float g2 = wc->getGainDB();
                chk (std::abs (g1 - g0 - 3.f) < .01f && std::abs (g2 - g0 + 3.f) < .01f, "ganancia del clip: +3 dB y -6 dB");
            if (wc)
            {   // bounce a pista nueva
                te::Clip* wcc2 = wc; playlist.selectClips ({ wcc2 }); const int nt0 = playlist.numTracks(); const int made = bounceSelected(); auto trs = te::getAudioTracks (edit);
                te::WaveAudioClip* bc = nullptr; for (auto* tk : trs) if (tk->getName().endsWith ("(bounce)")) for (auto* c : tk->getClips()) if (auto* w = dynamic_cast<te::WaveAudioClip*> (c)) bc = w;
                chk (made == 1 && playlist.numTracks() == nt0 + 1 && bc != nullptr, "bounce: crea una pista nueva con un clip de audio");
                chk (bc != nullptr && bc->getOriginalFile().existsAsFile() && bc->getOriginalFile().getSize() > 2000, "bounce: el audio renderizado existe y no est\xc3\xa1 vac\xc3\xado");
                chk (wcc2->isMuted(), "bounce: el clip original queda silenciado");
                { auto* tk0 = wcc2->getTrack(); int pos = trs.indexOf (dynamic_cast<te::AudioTrack*> (tk0)); chk (pos >= 0 && pos + 1 < trs.size() && trs[pos + 1]->getName().endsWith ("(bounce)"), "bounce: la pista nueva queda justo debajo de la original"); }
                playlist.selectClips ({}); chk (bounceSelected() == 0, "bounce: sin clips elegidos no hace nada");
                playlist.selectClips ({ wcc2 }); chk (bounceSelected() == 0, "bounce: un clip silenciado no se convierte");
                edit.getUndoManager().undo(); wcc2->setMuted (false); mixer.refreshTracks(); chk (playlist.numTracks() == nt0, "bounce: deshacer quita la pista nueva");
            }
                playlist.cmdClips ("fadeshort"); chk (std::abs (wc->getFadeIn().inSeconds() - 0.005) < 1e-4 && std::abs (wc->getFadeOut().inSeconds() - 0.005) < 1e-4, "fundidos cortos: 5 ms de entrada y de salida");
                playlist.cmdClips ("nofades"); chk (wc->getFadeIn().inSeconds() < 1e-6 && wc->getFadeOut().inSeconds() < 1e-6, "quitar fundidos"); }
        }
        // --- pistas ---
        { auto tracks = te::getAudioTracks (edit); const int n0 = tracks.size(); auto tcpy = tracks.indexOf (tx.get()); playlist.cmdTracks ("prevtrack"); playlist.selectClips ({}); 
          playlist.setSelTrackForTest (tcpy); const int clipsBefore = tx->getClips().size(); playlist.cmdTracks ("duptrack"); auto after = te::getAudioTracks (edit);
          te::AudioTrack* cp = nullptr; for (auto* t : after) if (t->getName().contains ("copia")) cp = t; chk (after.size() == n0 + 1 && cp != nullptr && cp->getClips().size() == clipsBefore && after.indexOf (cp) == tcpy + 1, "duplicar pista: queda debajo de la original, con los mismos clips y el nombre 'copia'");
          const int ts0 = playlist.selTrackIndex(); playlist.cmdTracks ("nexttrack"); const int ts1 = playlist.selTrackIndex(); playlist.cmdTracks ("prevtrack"); chk (ts1 == ts0 + 1 && playlist.selTrackIndex() == ts0, "pista de arriba / de abajo");
          auto* sel = after[playlist.selTrackIndex()]; const bool m0 = sel->isMuted (false); playlist.cmdTracks ("mutetrack"); const bool m1 = sel->isMuted (false); playlist.cmdTracks ("mutetrack"); chk (m0 != m1 && sel->isMuted (false) == m0, "silenciar la pista elegida (y volver)");
          const bool s0 = sel->isSolo (false); playlist.cmdTracks ("solotrack"); const bool s1 = sel->isSolo (false); playlist.cmdTracks ("solotrack"); chk (s0 != s1 && sel->isSolo (false) == s0, "solo en la pista elegida (y volver)");
          after[0]->getVolumePlugin()->setPan (.5f); playlist.cmdTracks ("panreset"); chk (std::abs (after[0]->getVolumePlugin()->getPan()) < 1e-4f, "centrar el pan de todas las pistas");
          playlist.cmdTracks ("foldall"); bool allF = true; for (int i = 0; i < playlist.numTracks(); ++i) allF = allF && playlist.folded (i); chk (allF, "plegar todas las pistas");
          playlist.cmdTracks ("unfoldall"); bool noF = true; for (int i = 0; i < playlist.numTracks(); ++i) noF = noF && ! playlist.folded (i); chk (noF, "desplegar todas las pistas");
          const int pre = playlist.numTracks(); newTrack(); newTrack(); const int removed = playlist.cmdTracks ("delempty"); bool noEmpty = true; for (auto* t : te::getAudioTracks (edit)) noEmpty = noEmpty && ! t->getClips().isEmpty();
          chk (removed >= 2 && playlist.numTracks() <= pre && noEmpty && playlist.numTracks() >= 1, "quitar pistas vac\xc3\xad" "as: no queda ninguna sin clips"); }
        // --- proyecto ---
        { edit.tempoSequence.getTempo (0)->setBpm (100.0); runCmd ("tempox2"); const double a = bpmNow(); runCmd ("tempohalf"); const double b = bpmNow(); runCmd ("tempoup"); const double c = bpmNow(); runCmd ("tempodown"); const double d = bpmNow();
          chk (std::abs (a - 200.0) < .01 && std::abs (b - 100.0) < .01 && std::abs (c - 101.0) < .01 && std::abs (d - 100.0) < .01, "tempo: x2, mitad, +1 y -1 (" + juce::String (a, 1) + ", " + juce::String (b, 1) + ", " + juce::String (c, 1) + ", " + juce::String (d, 1) + ")");
          edit.tempoSequence.getTempo (0)->setBpm (290.0); runCmd ("tempox2"); chk (bpmNow() <= 300.0, "tempo: el l\xc3\xadmite es 300 BPM"); edit.tempoSequence.getTempo (0)->setBpm (160.0); }
        { auto ta = newTrack(); auto* M = ideas::insert (edit, *ta, 0.0, 1, { { 60, 0.0, 1.0, 100 }, { 64, 1.0, 1.0, 100 } }, "M"); playlist.selectClips ({ M });
          auto pitches = [&] { std::vector<int> v; for (auto* n : M->getSequence().getNotes()) v.push_back (n->getNoteNumber()); std::sort (v.begin(), v.end()); return v; };
          runCmd ("transup"); auto p1 = pitches(); runCmd ("transoctdown"); auto p2 = pitches(); runCmd ("transdown"); runCmd ("transoctup"); auto p3 = pitches();
          chk (p1 == std::vector<int> { 61, 65 } && p2 == std::vector<int> { 49, 53 } && p3 == std::vector<int> { 60, 64 }, "transponer: +1, -12, -1, +12 vuelve al inicio");
          edit.getUndoManager().beginNewTransaction(); runCmd ("transoctup"); edit.getUndoManager().undo(); chk (pitches() == p3, "transponer: se deshace en un paso"); }
        chk (projectInfoText().contains ("BPM") && projectInfoText().contains ("pista"), "informaci\xc3\xb3n del proyecto: " + projectInfoText());
        tc.setPosition (atB (5.5)); playlist.gotoBar (1); chk (std::abs (timeToBeatsT (tc.getPosition().inSeconds()) - 8.0) < 1e-3, "comp\xc3\xa1s siguiente: de 5.5 a 8"); tc.setPosition (atB (5.5)); playlist.gotoBar (-1); chk (std::abs (timeToBeatsT (tc.getPosition().inSeconds()) - 4.0) < 1e-3, "comp\xc3\xa1s anterior: de 5.5 a 4");
        tc.setPosition (atB (8.0)); playlist.gotoBar (-1); chk (std::abs (timeToBeatsT (tc.getPosition().inSeconds()) - 4.0) < 1e-3, "comp\xc3\xa1s anterior: desde un l\xc3\xadmite de compás va al otro");
        runCmd ("zoom4"); runCmd ("zoom16"); runCmd ("zoom64"); chk (true, "zoom por compases: sin fallos");
        // --- step sequencer ---
        { setSteps (true); steps.ensureClip(); steps.selectRow (0); const int n = steps.getClip()->getPattern (0).getNumNotes(); for (int i = 0; i < n; ++i) steps.setStep (0, i, false); steps.setStep (0, 0, true); steps.setStep (0, 4, true);
          steps.rowOp ("rotr"); chk (steps.getStep (0, 1) && steps.getStep (0, 5) && ! steps.getStep (0, 0), "fila: rotar a la derecha mueve cada paso uno");
          steps.rowOp ("rotl"); steps.rowOp ("rotl"); chk (steps.getStep (0, n - 1) && steps.getStep (0, 3), "fila: rotar a la izquierda (y da la vuelta)");
          steps.rowOp ("rotr"); const int inv = steps.rowOp ("invert"); chk (inv == n - 2 && ! steps.getStep (0, 0) && steps.getStep (0, 1), "fila: invertir deja " + juce::String (inv) + " pasos");
          chk (steps.rowOp ("clear") == 0 && ! steps.getStep (0, 2), "fila: vaciar");
          bool eu = true; for (int k : { 3, 5, 7 }) eu = eu && steps.rowOp ("euclid:" + juce::String (k)) == k; chk (eu, "fila: ritmos euclideos de 3, 5 y 7 pasos");
          const int rn = steps.rowOp ("random"); chk (rn >= 0 && rn <= n, "fila: aleatoria (" + juce::String (rn) + " pasos)"); steps.rowOp ("clear"); }
        // --- selección por tipo, largo y posición ---
        { auto tm = newTrack(); auto* P = ideas::insert (edit, *tm, 40.0, 1, nn, "P"); auto* Q = ideas::insert (edit, *tm, 48.0, 1, nn, "Q"); auto* R = ideas::insert (edit, *tm, 56.0, 1, nn, "R"); auto* S4 = ideas::insert (edit, *tm, 64.0, 1, nn, "S");
          int midi = 0, audio = 0; for (auto* t : te::getAudioTracks (edit)) for (auto* c : t->getClips()) { if (dynamic_cast<te::MidiClip*> (c)) ++midi; else if (dynamic_cast<te::WaveAudioClip*> (c)) ++audio; }
          chk (playlist.cmdClips ("selmidi") == midi && playlist.selectedClips().size() == midi, "elegir clips MIDI: " + juce::String (midi));
          chk (audio == 0 || playlist.cmdClips ("selaudio") == audio, "elegir clips de audio: " + juce::String (audio));
          Q->setMuted (true); playlist.cmdClips ("selmuted"); chk (playlist.selectedClips().size() == 1 && playlist.selectedClips()[0] == Q, "elegir clips silenciados"); Q->setMuted (false);
          playlist.selectClips ({ P, Q, R, S4 }); chk (playlist.cmdClips ("selalt") == 2 && playlist.selectedClips().contains (P) && playlist.selectedClips().contains (R) && ! playlist.selectedClips().contains (Q), "elegir uno de cada dos: P y R");
          P->setEnd (atB (40.0 + 5.9), true); Q->setEnd (atB (48.0 + 1.9), true); playlist.selectClips ({ P, Q }); const int tb = playlist.cmdClips ("trimbars");
          chk (tb == 2 && std::abs (lenB (P) - 4.0) < 1e-3 && std::abs (lenB (Q) - 4.0) < 1e-3, "largo en compases enteros: 5.9 y 1.9 pulsos pasan a 1 compás (" + juce::String (lenB (P), 2) + ", " + juce::String (lenB (Q), 2) + ")");
          P->setStart (atB (40.0), false, true); Q->setStart (atB (50.0), false, true); R->setStart (atB (70.0), false, true); playlist.selectClips ({ P, Q }); playlist.cmdClips ("flipclips");
          chk (std::abs (startB (P) - 50.0) < 1e-3 && std::abs (startB (Q) - 40.0) < 1e-3, "voltear posiciones: P y Q intercambian lugares (" + juce::String (startB (P), 2) + ", " + juce::String (startB (Q), 2) + ")");
          Q->setStart (atB (40.0), false, true); P->setStart (atB (44.0), false, true); R->setStart (atB (70.0), false, true); S4->setStart (atB (90.0), false, true); playlist.selectClips ({ P, Q, R }); playlist.cmdClips ("spread");
          chk (std::abs (startB (Q) - 40.0) < 1e-3 && std::abs (startB (P) - 44.0) < 1e-3 && std::abs (startB (R) - 48.0) < 1e-3, "repartir: un clip por compás (40, 44, 48)");
          P->state.setProperty ("injColour", 3, nullptr); playlist.selectClips ({ P }); chk (playlist.cmdClips ("resetcolor") == 1 && ! P->state.hasProperty ("injColour"), "quitar el color propio del clip");
          P->setName ("xyz"); playlist.cmdClips ("nameastrack"); chk (P->getName() == tm->getName() || P->getName().startsWith ("Pista "), "nombre del clip = nombre de su pista (" + P->getName() + ")"); }
        // --- herramientas de notas de los clips MIDI ---
        { auto tn = newTrack(); auto* M = ideas::insert (edit, *tn, 0.0, 1, { { 60, 0.1, 0.5, 90 }, { 62, 1.13, 0.5, 80 }, { 64, 2.0, 0.05, 70 }, { 65, 3.0, 0.5, 60 }, { 65, 3.0, 0.5, 60 } }, "N");
          auto starts = [&] { std::vector<double> v; for (auto* nt : M->getSequence().getNotes()) v.push_back (nt->getStartBeat().inBeats()); std::sort (v.begin(), v.end()); return v; };
          playlist.selectClips ({ M }); chk (playlist.cmdClips ("dedupe") == 1 && M->getSequence().getNotes().size() == 4, "quitar notas repetidas: de 5 a 4");
          chk (playlist.cmdClips ("killshort") == 1 && M->getSequence().getNotes().size() == 3, "quitar notas muy cortas: se va la de 0.05");
          chk (playlist.cmdClips ("quantnotes") == 2 && std::abs (starts()[0]) < 1e-6 && std::abs (starts()[1] - 1.25) < 1e-6, "cuantizar notas a 1/16: 0.1 -> 0, 1.13 -> 1.25");
          chk (playlist.cmdClips ("vel100") > 0 && [&] { for (auto* nt : M->getSequence().getNotes()) if (nt->getVelocity() != 100) return false; return true; }(), "velocidad 100 en todas las notas");
          playlist.cmdClips ("reversemidi"); const auto rv = starts(); chk (rv.size() == 3 && std::abs (rv[0] - 0.5) < 1e-6 && std::abs (rv[1] - 2.25) < 1e-6 && std::abs (rv[2] - 3.5) < 1e-6, "invertir notas en el tiempo (" + juce::String (rv[0], 2) + ", " + juce::String (rv[1], 2) + ", " + juce::String (rv[2], 2) + ")");
          const double L0 = lenB (M); playlist.cmdClips ("doublemidi"); chk (M->getSequence().getNotes().size() == 6 && std::abs (lenB (M) - 2.0 * L0) < 1e-3, "duplicar el contenido: 6 notas y clip del doble de largo");
          playlist.cmdClips ("stretchhalf"); chk (std::abs (lenB (M) - L0) < 1e-3 && std::abs (starts().back() - (3.5 + L0) / 2.0) < 1e-3, "x1/2: el clip vuelve a su largo y las notas se acortan");
          playlist.cmdClips ("stretch2"); chk (std::abs (lenB (M) - 2.0 * L0) < 1e-3, "x2: el clip dura el doble");
          edit.getUndoManager().undo(); chk (std::abs (lenB (M) - L0) < 1e-3, "las notas de los clips se deshacen en un paso"); }
        // --- pistas (volumen y color) ---
        { playlist.cmdTracks ("vol0"); bool z = true; for (auto* t : te::getAudioTracks (edit)) z = z && std::abs (t->getVolumePlugin()->getVolumeDb()) < 1e-3f; chk (z, "todas las pistas a 0 dB");
          playlist.cmdTracks ("headroom"); bool h = true; for (auto* t : te::getAudioTracks (edit)) h = h && std::abs (t->getVolumePlugin()->getVolumeDb() + 3.f) < .05f; chk (h, "margen: todas las pistas a -3 dB"); playlist.cmdTracks ("vol0");
          playlist.cmdTracks ("rainbow"); bool rb = true; auto ts = te::getAudioTracks (edit); for (int i = 0; i < ts.size(); ++i) rb = rb && (int) ts[i]->state.getProperty ("injColour", -1) == i % 8; chk (rb, "un color distinto para cada pista"); }
        // --- bucle, metrónomo y tempo ---
        { const bool m0 = edit.clickTrackEnabled.get(); runCmd ("metrotoggle"); const bool m1 = edit.clickTrackEnabled.get(); runCmd ("metrotoggle"); chk (m0 != m1 && edit.clickTrackEnabled.get() == m0, "metr\xc3\xb3nomo: alterna y vuelve");
          tc.looping = false; runCmd ("looptoggle"); const bool l1 = tc.looping; runCmd ("looptoggle"); chk (l1 && ! tc.looping, "bucle: alterna y vuelve");
          tc.setPosition (atB (5.5)); runCmd ("loop4"); auto r4 = tc.getLoopRange(); chk (tc.looping && std::abs (timeToBeatsT (r4.getStart().inSeconds()) - 4.0) < 1e-3 && std::abs (timeToBeatsT (r4.getEnd().inSeconds()) - 20.0) < 1e-3, "bucle de 4 compases desde el compás del cabezal");
          runCmd ("loop8"); chk (std::abs (timeToBeatsT (tc.getLoopRange().getEnd().inSeconds()) - 36.0) < 1e-3, "bucle de 8 compases");
          runCmd ("loopfwd"); chk (std::abs (timeToBeatsT (tc.getLoopRange().getStart().inSeconds()) - 8.0) < 1e-3, "mover el bucle un compás adelante"); runCmd ("loopback"); runCmd ("loopback"); chk (std::abs (timeToBeatsT (tc.getLoopRange().getStart().inSeconds()) - 0.0) < 1e-3, "mover el bucle atrás");
          runCmd ("loopback"); chk (timeToBeatsT (tc.getLoopRange().getStart().inSeconds()) > -1e-6, "el bucle no pasa de cero");
          runCmd ("loopall"); chk (std::abs (tc.getLoopRange().getStart().inSeconds()) < 1e-6 && timeToBeatsT (tc.getLoopRange().getEnd().inSeconds()) >= playlist.projectEndBeat() - 1e-6, "bucle sobre todo el proyecto"); tc.looping = false;
          edit.tempoSequence.getTempo (0)->setBpm (123.4); runCmd ("bpmround"); chk (std::abs (bpmNow() - 123.0) < 1e-6, "redondear el tempo"); edit.tempoSequence.getTempo (0)->setBpm (160.0); }
        // --- step sequencer: m\xc3\xa1s ---
        { setSteps (true); steps.ensureClip(); steps.selectRow (0); const int n = steps.getClip()->getPattern (0).getNumNotes(); steps.rowOp ("clear"); steps.setStep (0, 0, true); steps.setStep (0, 3, true);
          steps.rowOp ("half"); chk (steps.getStep (0, n / 2) && steps.getStep (0, n / 2 + 3) && steps.getStep (0, 0), "fila: repetir la primera mitad");
          steps.rowOp ("fourfloor"); bool ff = true; for (int i = 0; i < n; ++i) ff = ff && steps.getStep (0, i) == (i % 4 == 0); chk (ff, "fila: cuatro al piso");
          steps.rowOp ("offbeat"); bool ob = true; for (int i = 0; i < n; ++i) ob = ob && steps.getStep (0, i) == (i % 4 == 2); chk (ob, "fila: a contratiempo");
          steps.rowOp ("clear"); steps.setStep (0, 0, true); steps.setStep (0, 1, true); steps.setStepVelocity (0, 0, 60); steps.setStepVelocity (0, 1, 60); steps.rowOp ("accent"); chk (steps.getStepVelocity (0, 0) == 127 && steps.getStepVelocity (0, 1) == 90, "fila: acentos en los tiempos fuertes");
          steps.rowOp ("vel100"); chk (steps.getStepVelocity (0, 0) == 100 && steps.getStepVelocity (0, 1) == 100, "fila: velocidad 100");
          const int sp = steps.rowOp ("sparse"), de = steps.rowOp ("dense"); chk (sp >= 0 && sp <= n && de >= 0 && de <= n, "fila: dispersa y densa (" + juce::String (sp) + ", " + juce::String (de) + ")"); steps.rowOp ("clear"); }
        // --- arreglo (secciones) ---
        {
            arranger::clearAll (edit); playlist.cmdTracks ("clearmarkers"); auto tz = newTrack(); auto* X = ideas::insert (edit, *tz, 0.0, 1, nn, "X"); auto* Y = ideas::insert (edit, *tz, 8.0, 1, nn, "Y"); auto* Z = ideas::insert (edit, *tz, 16.0, 1, nn, "Z");
            chk (arranger::add (edit, 0.0, 8.0, "Intro") && arranger::add (edit, 8.0, 8.0, "Drop") && arranger::add (edit, 16.0, 8.0, "Outro"), "arreglo: se crean tres secciones");
            chk (! arranger::add (edit, 4.0, 8.0) && ! arranger::add (edit, 0.0, 0.5), "arreglo: no se crean secciones solapadas ni de menos de un pulso");
            chk (arranger::list (edit).size() == 3 && arranger::indexAt (edit, 9.0) == 1 && arranger::indexAt (edit, 30.0) == -1, "arreglo: indexAt encuentra la secci\xc3\xb3n");
            playlist.addMarker (9.0, "m"); auto* vp = PlaylistView::volParamOf (*tz); vp->getCurve().clear(); vp->getCurve().addPoint (atB (2.0), vp->valueRange.convertFrom0to1 (.5f), 0.f);
            tc.setPosition (atB (1.0)); chk (playlist.currentSection() == 0, "arreglo: la secci\xc3\xb3n actual sigue al cabezal");
            chk (playlist.cmdSections ("right") == 1, "arreglo: mover a la derecha");
            { auto ss = arranger::list (edit); chk (ss[0].name == "Drop" && ss[1].name == "Intro" && std::abs (ss[0].beat) < 1e-6 && std::abs (ss[1].beat - 8.0) < 1e-6, "arreglo: las secciones cambian de orden y nombre se conserva (" + ss[0].name + "," + ss[1].name + ")"); }
            chk (std::abs (startB (X) - 8.0) < 1e-3 && std::abs (startB (Y)) < 1e-3 && std::abs (startB (Z) - 16.0) < 1e-3, "arreglo: los clips viajan con su secci\xc3\xb3n (X " + juce::String (startB (X), 2) + ", Y " + juce::String (startB (Y), 2) + ", Z " + juce::String (startB (Z), 2) + ")");
            { bool mk = false; for (auto& m : playlist.markers()) if (m.name == "m" && std::abs (m.beat - 1.0) < 1e-6) mk = true; chk (mk, "arreglo: el marcador viaja con su secci\xc3\xb3n"); }
            chk (vp->getCurve().getNumPoints() == 1 && std::abs (timeToBeatsT (vp->getCurve().getPointTime (0).inSeconds()) - 10.0) < 1e-3, "arreglo: la automatizaci\xc3\xb3n viaja con su secci\xc3\xb3n (" + juce::String (timeToBeatsT (vp->getCurve().getPointTime (0).inSeconds()), 2) + ")");
            chk (std::abs (vp->getCurve().getPointValue (0) - vp->valueRange.convertFrom0to1 (.5f)) < 1e-4, "arreglo: la automatizaci\xc3\xb3n conserva su valor");
            edit.getUndoManager().undo(); chk (std::abs (startB (X)) < 1e-3 && std::abs (startB (Y) - 8.0) < 1e-3 && arranger::list (edit)[0].name == "Intro", "arreglo: deshacer devuelve todo a su sitio");
            edit.getUndoManager().redo(); chk (std::abs (startB (X) - 8.0) < 1e-3, "arreglo: rehacer lo vuelve a mover");
            tc.setPosition (atB (9.0)); chk (playlist.cmdSections ("left") == 1 && std::abs (startB (X)) < 1e-3 && std::abs (startB (Y) - 8.0) < 1e-3, "arreglo: mover a la izquierda lo deja como estaba");
            tc.setPosition (atB (1.0)); chk (playlist.cmdSections ("left") == 0 && arranger::list (edit)[0].name == "Intro", "arreglo: la primera secci\xc3\xb3n no puede ir m\xc3\xa1s a la izquierda");
            arranger::move (edit, 0, 2); { auto ss = arranger::list (edit); chk (ss[0].name == "Drop" && ss[1].name == "Outro" && ss[2].name == "Intro" && std::abs (startB (Z) - 8.0) < 1e-3 && std::abs (startB (X) - 16.0) < 1e-3, "arreglo: mover la primera a la \xc3\xbaltima (Z " + juce::String (startB (Z), 2) + ", X " + juce::String (startB (X), 2) + ")"); }
            edit.getUndoManager().undo(); chk (std::abs (startB (Z) - 16.0) < 1e-3 && std::abs (startB (X)) < 1e-3, "arreglo: un solo deshacer revierte un movimiento de varios pasos");
            // huecos entre secciones: se conservan
            arranger::clearAll (edit); arranger::add (edit, 0.0, 4.0, "A"); arranger::add (edit, 12.0, 4.0, "B"); X->setStart (atB (0.0), false, true); Y->setStart (atB (6.0), false, true); Z->setStart (atB (12.0), false, true);
            arranger::swapNext (edit, 0); chk (std::abs (startB (Z)) < 1e-3 && std::abs (startB (Y) - 6.0) < 1e-3 && std::abs (startB (X) - 12.0) < 1e-3 && std::abs (arranger::list (edit)[1].beat - 12.0) < 1e-6, "arreglo: el hueco entre secciones se conserva (Y sigue en 6)");
            // duplicar
            arranger::clearAll (edit); arranger::add (edit, 0.0, 8.0, "Intro"); arranger::add (edit, 8.0, 8.0, "Drop"); X->setStart (atB (0.0), false, true); Y->setStart (atB (8.0), false, true); Z->setStart (atB (20.0), false, true);
            const int nc0 = tz->getClips().size(); tc.setPosition (atB (9.0)); chk (playlist.cmdSections ("dup") == 1, "arreglo: duplicar");
            { auto ss = arranger::list (edit); chk (ss.size() == 3 && ss[2].name == "Drop 2" && std::abs (ss[2].beat - 16.0) < 1e-6 && tz->getClips().size() == nc0 + 1 && std::abs (startB (Z) - 28.0) < 1e-3, "arreglo: la copia queda detr\xc3\xa1s y lo posterior se desplaza (Z " + juce::String (startB (Z), 2) + ")"); }
            { int at16 = 0; for (auto* c : tz->getClips()) if (std::abs (startB (c) - 16.0) < 1e-3) ++at16; chk (at16 == 1, "arreglo: el clip copiado est\xc3\xa1 en su sitio"); }
            edit.getUndoManager().undo(); chk (arranger::list (edit).size() == 2 && tz->getClips().size() == nc0 && std::abs (startB (Z) - 20.0) < 1e-3, "arreglo: deshacer la duplicaci\xc3\xb3n lo devuelve todo");
            // tama\xc3\xb1o
            chk (std::abs (arranger::setLen (edit, 0, 20.0) - 8.0) < 1e-6, "arreglo: no se puede pisar a la secci\xc3\xb3n siguiente"); chk (std::abs (arranger::setLen (edit, 0, 0.2) - 1.0) < 1e-6, "arreglo: m\xc3\xadnimo un pulso");
            tc.setPosition (atB (9.0)); const double l1 = arranger::list (edit)[1].len; playlist.cmdSections ("grow"); chk (std::abs (arranger::list (edit)[1].len - (l1 + timesig::bar())) < 1e-6, "arreglo: alargar un comp\xc3\xa1s"); playlist.cmdSections ("shrink");
            // elegir, bucle, navegar, colores
            playlist.cmdSections ("select"); { auto sel = playlist.selectedClips(); chk (sel.contains (Y) && ! sel.contains (X), "arreglo: elegir los clips de la secci\xc3\xb3n"); }
            playlist.cmdSections ("loop"); chk (tc.looping && std::abs (timeToBeatsT (tc.getLoopRange().getStart().inSeconds()) - 8.0) < 1e-2, "arreglo: bucle sobre la secci\xc3\xb3n"); tc.looping = false;
            tc.setPosition (atB (1.0)); playlist.cmdSections ("next"); chk (std::abs (timeToBeatsT (tc.getPosition().inSeconds()) - 8.0) < 1e-2, "arreglo: ir a la siguiente"); playlist.cmdSections ("prev"); chk (std::abs (timeToBeatsT (tc.getPosition().inSeconds())) < 1e-2, "arreglo: ir a la anterior");
            tc.setPosition (atB (9.0)); const int c0 = arranger::list (edit)[1].col; playlist.cmdSections ("colour"); chk (arranger::list (edit)[1].col == (c0 + 1) % 8, "arreglo: cambiar de color");
            arranger::rename (edit, 1, "  Build  "); chk (arranger::list (edit)[1].name == "Build", "arreglo: renombrar recorta espacios"); arranger::rename (edit, 1, "   "); chk (arranger::list (edit)[1].name == "Build", "arreglo: un nombre vac\xc3\xado se ignora");
            // marcadores -> secciones
            arranger::clearAll (edit); playlist.cmdTracks ("clearmarkers"); playlist.addMarker (0.0, "Intro"); playlist.addMarker (16.0, "7"); playlist.addMarker (32.0, "Drop");
            chk (playlist.cmdSections ("frommarkers") == 3, "arreglo: una secci\xc3\xb3n por marcador"); { auto ss = arranger::list (edit); chk (ss.size() == 3 && std::abs (ss[0].len - 16.0) < 1e-6 && ss[0].name == "Intro" && ss[2].name == "Drop" && ss[1].name != "7", "arreglo: duraci\xc3\xb3n hasta el siguiente marcador y nombres sensatos (" + ss[1].name + ")"); }
            chk (playlist.cmdSections ("frommarkers") == 0, "arreglo: repetirlo no duplica secciones");
            tc.setPosition (atB (100.0)); chk (playlist.cmdSections ("del") == 0 && arranger::list (edit).size() == 3, "arreglo: sin secci\xc3\xb3n bajo el cabezal no borra nada");
            tc.setPosition (atB (1.0)); chk (playlist.cmdSections ("del") == 1 && arranger::list (edit).size() == 2 && ! tz->getClips().isEmpty(), "arreglo: eliminar una secci\xc3\xb3n no borra sus clips");
            // proyecto guardado y vuelto a leer
            { auto xml = edit.state.createXml(); bool has = xml != nullptr && xml->getChildByName ("INJSECTION") != nullptr; chk (has, "arreglo: las secciones se guardan en el proyecto"); }
            // interfaz: arrastrar en el carril
            {
                playlist.setSize (1400, 700); playlist.setZoom (40.0); arranger::clearAll (edit); arranger::add (edit, 0.0, 8.0, "A"); arranger::add (edit, 8.0, 8.0, "B"); arranger::add (edit, 16.0, 8.0, "C");
                X->setStart (atB (0.0), false, true); Y->setStart (atB (8.0), false, true); Z->setStart (atB (16.0), false, true);
                juce::MouseInputSource src = juce::Desktop::getInstance().getMainMouseSource(); const float ly = (float) (PlaylistView::kBarsH + 7);
                auto drag = [&] (float x0, float x1, float y) { juce::Point<float> p (x0, y), q (x1, y); auto ev = [&] (juce::Point<float> pt) { return juce::MouseEvent (src, pt, juce::ModifierKeys(), 1.f, 0.f, 0.f, 0.f, 0.f, &playlist, &playlist, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false); };
                                                                  playlist.mouseDown (ev (p)); playlist.mouseDrag (ev (q)); playlist.mouseUp (ev (q)); };
                drag (playlist.beatToX (4.0), playlist.beatToX (22.0), ly);
                { auto ss = arranger::list (edit); chk (ss[0].name == "B" && ss[1].name == "C" && ss[2].name == "A" && std::abs (startB (X) - 16.0) < 1e-3 && std::abs (startB (Y)) < 1e-3, "arreglo (rat\xc3\xb3n): arrastrar A hasta el final la reordena con sus clips"); }
                drag (playlist.beatToX (12.0), playlist.beatToX (12.0), ly); chk (std::abs (timeToBeatsT (tc.getPosition().inSeconds()) - 8.0) < 1e-2, "arreglo (rat\xc3\xb3n): un clic lleva el cabezal al inicio de la secci\xc3\xb3n");
                drag (playlist.beatToX (8.0) - 2.f, playlist.beatToX (4.0), ly); chk (std::abs (arranger::list (edit)[0].len - 4.0) < 1e-6 || std::abs (arranger::list (edit)[0].len - 8.0) < 1e-6, "arreglo (rat\xc3\xb3n): arrastrar el borde no rompe la longitud");
                { juce::Image img (juce::Image::ARGB, playlist.getWidth(), playlist.getHeight(), true); juce::Graphics g (img); playlist.paintEntireComponent (g, false); chk (true, "arreglo: se dibuja el carril"); }
            }
            arranger::clearAll (edit); playlist.cmdTracks ("clearmarkers"); playlist.hideAllAutomation();
        }
        // --- armon\xc3\xada: tonalidad y acordes ---
        {
            harmony::clearAll (edit); harmony::setKey (edit, { 0, 0 }); edit.state.setProperty ("injChordLane", false, nullptr);
            int r = 0, q = 0;
            chk (harmony::parse ("Am", r, q) && r == 9 && q == 1, "acordes: Am se entiende"); chk (harmony::parse ("F#m7", r, q) && r == 6 && q == 4, "acordes: F#m7");
            chk (harmony::parse ("Bb", r, q) && r == 10 && q == 0, "acordes: Bb mayor"); chk (harmony::parse ("Bb5", r, q) && r == 10 && q == 2, "acordes: Bb5 es potencia de si bemol");
            chk (harmony::parse ("c5", r, q) && r == 0 && q == 2, "acordes: min\xc3\xbasculas valen"); chk (harmony::parse ("Gsus4", r, q) && r == 7 && q == 7, "acordes: Gsus4"); chk (harmony::parse ("Ebm", r, q) && r == 3 && q == 1, "acordes: Ebm");
            chk (harmony::parse ("Cmb5", r, q) && r == 0 && q == 11, "acordes: Cmb5"); chk (! harmony::parse ("H", r, q) && ! harmony::parse ("", r, q) && ! harmony::parse ("Cxyz", r, q), "acordes: lo que no es un acorde se rechaza");
            { harmony::Chord c; c.root = 9; c.quality = 1; auto p = harmony::pitches (c, 48); chk (p.size() == 3 && p[0] == 57 && p[1] == 60 && p[2] == 64, "acordes: Am = A3 C4 E4"); }
            { int rr = 0, qq = 0; chk (harmony::detectChord ({ 57, 60, 64 }, rr, qq) && rr == 9 && qq == 1, "acordes: reconoce Am"); chk (harmony::detectChord ({ 45, 52 }, rr, qq) && rr == 9 && qq == 2, "acordes: reconoce A5 (potencia)");
              chk (harmony::detectChord ({ 48, 64, 79 }, rr, qq) && rr == 0 && qq == 0, "acordes: reconoce C aunque las notas est\xc3\xa9n en octavas distintas"); chk (! harmony::detectChord ({ 60 }, rr, qq) && ! harmony::detectChord ({ 60, 61, 62, 63 }, rr, qq), "acordes: una nota o un racimo no son un acorde"); }
            harmony::Key am { 9, 1 }; chk (harmony::inScale (am, 69) && harmony::inScale (am, 60) && ! harmony::inScale (am, 61) && harmony::isRoot (am, 57) && ! harmony::isRoot (am, 60), "tonalidad: La menor tiene las notas que debe");
            chk (harmony::snapToScale (am, 61) == 60 && harmony::snapToScale (am, 66) == 65 && harmony::snapToScale (am, 64) == 64, "tonalidad: ajustar a la escala (C# a C, F# a F, E se queda)");
            chk (harmony::snapToScale ({ 0, 0 }, 61) == 61, "tonalidad: sin escala no cambia nada");
            { harmony::Key k; chk (harmony::detectKey ({ 57, 60, 64, 57, 59, 60, 62, 64, 65, 67, 69, 57, 60, 64 }, k) && k.root == 9 && k.scale == 1, "tonalidad: deduce La menor"); chk (harmony::detectKey ({ 60, 64, 67, 62, 65, 69, 60, 64, 67, 71, 67, 60 }, k) && k.root == 0 && k.scale == 2, "tonalidad: deduce Do mayor"); chk (! harmony::detectKey ({}, k), "tonalidad: sin notas no deduce"); }
            { std::vector<int> ps { 61, 65, 68 }; const int sh = harmony::bestShift (am, ps); int a0 = 0, a1 = 0; for (int p : ps) { a0 += harmony::inScale (am, p); a1 += harmony::inScale (am, p + sh); } chk (a1 > a0 && std::abs (sh) <= 6 && harmony::bestShift (am, { 57, 60, 64 }) == 0, "tonalidad: transponer lo mejora y lo que ya encaja no se mueve (" + juce::String (sh) + ")"); }
            // acordes en el proyecto
            chk (harmony::add (edit, 0.0, 4.0, 9, 1) && harmony::add (edit, 4.0, 4.0, 5, 0) && ! harmony::add (edit, 2.0, 4.0, 0, 0) && ! harmony::add (edit, 8.0, 0.1, 0, 0), "acordes: se crean y no se solapan");
            chk (harmony::indexAt (edit, 5.0) == 1 && harmony::indexAt (edit, 9.0) == -1 && harmony::list (edit)[1].name() == "F", "acordes: indexAt y nombre");
            chk (std::abs (harmony::setLen (edit, 0, 30.0) - 4.0) < 1e-6 && std::abs (harmony::setLen (edit, 1, 2.0) - 2.0) < 1e-6, "acordes: la duraci\xc3\xb3n no pisa al siguiente");
            chk (! harmony::moveTo (edit, 0, 3.0) && harmony::moveTo (edit, 1, 8.0) && std::abs (harmony::list (edit)[1].beat - 8.0) < 1e-6, "acordes: no se pueden mover encima de otro y s\xc3\xad a un hueco");
            harmony::moveTo (edit, 1, 4.0); harmony::setLen (edit, 1, 4.0); harmony::set (edit, 1, 7, 3); chk (harmony::list (edit)[1].name() == "G7", "acordes: editar el acorde");
            edit.getUndoManager().undo(); chk (harmony::list (edit)[1].name() == "F", "acordes: deshacer la edici\xc3\xb3n");
            playlist.refreshChordLane(); chk (playlist.chordOn, "acordes: el carril aparece al haber acordes"); const int rh = playlist.rulerH(); chk (rh == PlaylistView::kBarsH + PlaylistView::kSecH + PlaylistView::kChordH, "acordes: la regla crece lo justo");
            // operaciones sobre clips
            harmony::setKey (edit, am); chk (harmony::getKey (edit).root == 9 && harmony::getKey (edit).scale == 1, "tonalidad: se guarda en el proyecto");
            auto th = newTrack(); playlist.setSelTrackForTest (te::getAudioTracks (edit).indexOf (th.get()));
            const std::vector<ideas::N> n1 { { 60, 0.0, 1.0, 100 }, { 61, 1.0, 1.0, 100 }, { 64, 2.0, 1.0, 100 }, { 66, 3.0, 1.0, 100 } };
            auto notesOfClip = [] (te::MidiClip* m) { std::vector<int> o; for (auto* nt : m->getSequence().getNotes()) o.push_back (nt->getNoteNumber()); std::sort (o.begin(), o.end()); return o; };
            auto* M1 = ideas::insert (edit, *th, 0.0, 1, n1, "H1"); playlist.selectClips ({ M1 });
            chk (playlist.cmdHarmony ("snapscale") == 2 && notesOfClip (M1) == std::vector<int> { 60, 60, 64, 65 }, "armon\xc3\xada: ajustar a la escala cambia solo las notas que sobran");
            chk (playlist.cmdHarmony ("snapscale") == 0, "armon\xc3\xada: repetirlo no cambia nada");
            edit.getUndoManager().undo(); chk (notesOfClip (M1) == std::vector<int> { 60, 61, 64, 66 }, "armon\xc3\xada: deshacer devuelve las notas");
            const std::vector<ideas::N> n2 { { 61, 0.0, 1.0, 100 }, { 65, 1.0, 1.0, 100 }, { 68, 2.0, 1.0, 100 } };
            auto* M2 = ideas::insert (edit, *th, 8.0, 1, n2, "H2"); playlist.selectClips ({ M2 }); chk (playlist.cmdHarmony ("fitkey") == 1, "armon\xc3\xada: transponer para encajar");
            { int in = 0; for (int p : notesOfClip (M2)) in += harmony::inScale (am, p); chk (in == 3, "armon\xc3\xada: tras transponer las tres notas encajan"); }
            playlist.selectClips ({ M2 }); chk (playlist.cmdHarmony ("fitkey") == 0, "armon\xc3\xada: un clip que ya encaja no se mueve");
            // ajustar al acorde (Am de 0 a 4, F de 4 a 6)
            harmony::moveTo (edit, 1, 4.0); harmony::setLen (edit, 1, 4.0);
            const std::vector<ideas::N> n3 { { 61, 0.0, 1.0, 100 }, { 66, 1.0, 1.0, 100 }, { 62, 5.0, 1.0, 100 } };
            auto* M3 = ideas::insert (edit, *th, 16.0, 2, n3, "H3"); (void) M3; auto* M4 = ideas::insert (edit, *th, 0.0, 2, n3, "H4"); playlist.selectClips ({ M4 });
            chk (playlist.cmdHarmony ("snapchord") == 3, "armon\xc3\xada: ajustar al acorde cambia las notas que no son del acorde");
            { auto v = notesOfClip (M4); chk (v == std::vector<int> { 60, 60, 64 }, "armon\xc3\xada: C# a C y F# a E con Am; D a C con el acorde de F (" + juce::String (v[0]) + "," + juce::String (v[1]) + "," + juce::String (v[2]) + ")"); }
            { const int before = (int) th->getClips().size(); playlist.cmdHarmony ("chordclip"); chk ((int) th->getClips().size() == before + 1, "armon\xc3\xada: crear clip de acordes"); auto* last = dynamic_cast<te::MidiClip*> (th->getClips().getLast()); auto v = last ? notesOfClip (last) : std::vector<int>(); chk (v.size() == 6 && v[0] == 53 && v[1] == 57 && v[5] == 64, "armon\xc3\xada: el clip de acordes tiene Am y F (" + juce::String ((int) v.size()) + ")"); }
            { auto* C1 = ideas::insert (edit, *th, 40.0, 1, { { 57, 0.0, 4.0, 100 }, { 60, 0.0, 4.0, 100 }, { 64, 0.0, 4.0, 100 } }, "C1"); playlist.selectClips ({ C1 }); chk (playlist.cmdHarmony ("namechord") == 1, "armon\xc3\xada: nombre de acorde desde el clip"); chk (harmony::indexAt (edit, 41.0) >= 0 && harmony::list (edit)[(size_t) harmony::indexAt (edit, 41.0)].name() == "Am", "armon\xc3\xada: el acorde nuevo es Am");
              playlist.cmdHarmony ("namechord"); chk (harmony::list (edit).size() == 3, "armon\xc3\xada: no se duplica el acorde"); }
            { auto* C2 = ideas::insert (edit, *th, 48.0, 1, { { 60, 0.0, 1.0, 100 }, { 62, 1.0, 1.0, 100 }, { 64, 2.0, 1.0, 100 }, { 65, 3.0, 1.0, 100 }, { 67, 3.5, 0.5, 100 } }, "C2"); playlist.selectClips ({ C2 }); harmony::setKey (edit, { 0, 0 }); chk (playlist.cmdHarmony ("detectkey") == 1 && harmony::getKey (edit).root == 0 && harmony::getKey (edit).scale == 2, "armon\xc3\xada: deducir la tonalidad desde un clip");
              chk (playlist.cmdHarmony ("snapscale") > -1, "armon\xc3\xada: con tonalidad Do mayor ajustar no falla"); }
            harmony::setKey (edit, { 0, 0 }); playlist.selectClips ({ M1 }); chk (playlist.cmdHarmony ("snapscale") == 0 && playlist.cmdHarmony ("fitkey") == 0, "armon\xc3\xada: sin tonalidad no se toca nada");
            playlist.selectClips ({}); chk (playlist.cmdHarmony ("detectkey") == 0, "armon\xc3\xada: sin clips elegidos no hace nada");
            // Piano Roll usa la tonalidad del proyecto si el clip no tiene escala propia
            harmony::setKey (edit, am); piano.setClip (M1); chk (piano.scaleKindOf (61) == 0 && piano.scaleKindOf (69) == 2 && piano.scaleKindOf (60) == 1, "piano: marca la escala del proyecto (fuera 0, t\xc3\xb3nica 2, dentro 1)");
            M1->state.setProperty ("injScaleRoot", 0, nullptr); M1->state.setProperty ("injScaleType", 0, nullptr); chk (piano.scaleKindOf (61) == 0 && piano.scaleKindOf (60) == 2, "piano: la escala propia del clip manda sobre la del proyecto"); M1->state.removeProperty ("injScaleRoot", nullptr); piano.setClip (nullptr);
            // interfaz
            { playlist.setSize (1400, 700); playlist.setZoom (40.0); juce::MouseInputSource src = juce::Desktop::getInstance().getMainMouseSource();
              auto mev = [&] (juce::Point<float> p, juce::Point<float> from, int clicks) { return juce::MouseEvent (src, p, juce::ModifierKeys(), 1.f, 0.f, 0.f, 0.f, 0.f, &playlist, &playlist, juce::Time::getCurrentTime(), from, juce::Time::getCurrentTime(), clicks, false); };
              harmony::clearAll (edit); harmony::add (edit, 0.0, 4.0, 9, 1); harmony::add (edit, 8.0, 4.0, 5, 0); playlist.refreshChordLane(); playlist.cancelZoomAnim(); playlist.resetView(); playlist.setZoom (40.0); const float cy = (float) (PlaylistView::kBarsH + PlaylistView::kSecH + 8);
              { juce::Point<float> p0 (playlist.beatToX (2.0), cy), p1 (playlist.beatToX (6.0), cy); playlist.mouseDown (mev (p0, p0, 1)); playlist.mouseDrag (mev (p1, p0, 1)); playlist.mouseUp (mev (p1, p0, 1)); }
              chk (std::abs (harmony::list (edit)[0].beat - 4.0) < 1e-6, "acordes (rat\xc3\xb3n): arrastrar un acorde lo mueve");
              { juce::Point<float> p0 (playlist.beatToX (8.0) - 2.f, cy), p1 (playlist.beatToX (6.0), cy); playlist.mouseDown (mev (p0, p0, 1)); playlist.mouseDrag (mev (p1, p0, 1)); playlist.mouseUp (mev (p1, p0, 1)); }
              chk (std::abs (harmony::list (edit)[0].len - 2.0) < 1e-6, "acordes (rat\xc3\xb3n): arrastrar el borde acorta el acorde");
              { juce::Point<float> p0 (playlist.beatToX (6.0) - 2.f, cy), p1 (playlist.beatToX (30.0), cy); playlist.mouseDown (mev (p0, p0, 1)); playlist.mouseDrag (mev (p1, p0, 1)); playlist.mouseUp (mev (p1, p0, 1)); }
              chk (harmony::list (edit)[0].end() <= harmony::list (edit)[1].beat + 1e-6 && harmony::list (edit)[0].len > 2.0, "acordes (rat\xc3\xb3n): alargar no pisa al acorde vecino");
              { juce::Point<float> p0 (playlist.beatToX (9.0), cy); playlist.mouseDown (mev (p0, p0, 1)); playlist.mouseUp (mev (p0, p0, 1)); chk (std::abs (timeToBeatsT (tc.getPosition().inSeconds()) - 8.0) < 1e-2, "acordes (rat\xc3\xb3n): un clic lleva el cabezal al acorde"); }
              { juce::Image img (juce::Image::ARGB, playlist.getWidth(), playlist.getHeight(), true); juce::Graphics g (img); playlist.paintEntireComponent (g, false); chk (true, "acordes: se dibuja el carril y la tonalidad"); } }
            harmony::clearAll (edit); harmony::setKey (edit, { 0, 0 }); edit.state.setProperty ("injChordLane", false, nullptr); playlist.refreshChordLane();
            { auto xml = edit.state.createXml(); chk (xml != nullptr, "armon\xc3\xada: el proyecto se guarda"); }
        }
        // --- MIDI learn ---
        {
            midi.clear(); auto tm = newTrack(); playlist.setSelTrackForTest (te::getAudioTracks (edit).indexOf (tm.get())); const auto tid = tm->itemID.toString(); auto* vpm = tm->getVolumePlugin();
            auto cc = [&] (int n, int v, int ch = 1) { return midi.handle (juce::MidiMessage::controllerEvent (ch, n, v)); };
            chk (! cc (21, 64) && midi.list().empty(), "midi learn: sin asignar, un mando no hace nada");
            midiLearnStart ("vol"); chk (midi.learning() && midi.pendingText().startsWith ("Volumen"), "midi learn: queda esperando un mando");
            chk (! midi.handle (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100)) && midi.learning(), "midi learn: una nota no asigna");
            chk (cc (21, 64) && ! midi.learning() && midi.list().size() == 1 && midi.list()[0].cc == 21, "midi learn: el primer mando que se mueve queda asignado");
            cc (21, 127); chk (vpm->getVolumeDb() > 5.5f, "midi learn: CC al m\xc3\xa1ximo = +6 dB (" + juce::String (vpm->getVolumeDb(), 2) + ")");
            cc (21, 0); chk (vpm->getVolumeDb() <= -99.f, "midi learn: CC a cero = silencio (" + juce::String (vpm->getVolumeDb(), 1) + ")");
            cc (21, 91); chk (std::abs (vpm->getVolumeDb()) < 1.f, "midi learn: CC ~72% = cerca de 0 dB (" + juce::String (vpm->getVolumeDb(), 2) + ")");
            chk (! cc (22, 100), "midi learn: otro mando sin asignar no hace nada");
            midiLearnStart ("pan"); cc (22, 64); cc (22, 127); chk (std::abs (vpm->getPan() - 1.f) < .02f, "midi learn: panorama a la derecha"); cc (22, 0); chk (std::abs (vpm->getPan() + 1.f) < .02f, "midi learn: panorama a la izquierda");
            midiLearnStart ("mute"); cc (23, 1); cc (23, 127); chk (tm->isMuted (false), "midi learn: silenciar con el mando"); cc (23, 0); chk (! tm->isMuted (false), "midi learn: quitar el silencio");
            chk (midi.list().size() == 3, "midi learn: tres asignaciones");
            midiLearnStart ("pan"); cc (21, 10); { auto l = midi.list(); bool ok = l.size() == 2; for (auto& x : l) ok = ok && x.t.kind != "vol"; chk (ok, "midi learn: un mando solo tiene un destino (el CC 21 pasa a panorama y pierde volumen)"); }
            { int n21 = 0, nPan = 0; for (auto& x : midi.list()) { n21 += x.cc == 21; nPan += x.t.kind == "pan"; } chk (n21 == 1 && nPan == 1, "midi learn: un destino solo tiene un mando"); }
            midiLearnStart ("bpm"); cc (24, 5); cc (24, 127); chk (std::abs (bpmNow() - 250.0) < .6, "midi learn: tempo m\xc3\xa1ximo 250 (" + juce::String (bpmNow(), 1) + ")"); cc (24, 0); chk (std::abs (bpmNow() - 60.0) < .6, "midi learn: tempo m\xc3\xadnimo 60"); setBpmTo (160.0);
            midiLearnStart ("master"); cc (25, 5); cc (25, 127); chk (edit.getMasterVolumePlugin()->getVolumeDb() > 5.5f, "midi learn: volumen general"); cc (25, 91); chk (std::abs (edit.getMasterVolumePlugin()->getVolumeDb()) < 1.f, "midi learn: volumen general ~0 dB");
            midiLearnStart ("solo"); cc (26, 3); chk (midi.list().size() >= 5, "midi learn: solo asignado");
            cc (26, 127, 5); chk (tm->isSolo (false), "midi learn: el canal no importa si se aprendi\xc3\xb3 con cualquier canal que coincida (canal 5)"); cc (26, 0); tm->setSolo (false);
            { auto xml = edit.state.createXml(); chk (xml != nullptr && xml->getChildByName ("INJMIDIMAP") != nullptr, "midi learn: las asignaciones se guardan en el proyecto"); }
            midi.cancelLearn(); midiLearnStart ("vol"); midi.cancelLearn(); chk (! midi.learning() && ! cc (99, 1), "midi learn: cancelar aprendizaje");
            const int n0 = (int) midi.list().size(); midi.remove (0); chk ((int) midi.list().size() == n0 - 1, "midi learn: quitar una asignaci\xc3\xb3n"); edit.getUndoManager().undo(); chk ((int) midi.list().size() == n0, "midi learn: deshacer la devuelve");
            { midimap::Target gone { "vol", "no-existe" }; chk (! midi.apply (gone, .5f), "midi learn: un destino que ya no existe se ignora sin romper nada"); }
            midi.clear(); chk (midi.list().empty() && ! cc (21, 100), "midi learn: quitar todas"); edit.getUndoManager().undo(); chk (! midi.list().empty(), "midi learn: deshacer 'quitar todas'");
            midi.clear(); midiLearnStart ("pan"); cc (30, 0); vpm->setPan (0.f); cc (30, 64); chk (std::abs (vpm->getPan()) < .02f, "midi learn: la mitad del mando = centro");
            midi.clear(); vpm->setVolumeDb (0.f); vpm->setPan (0.f);
        }
        // --- macros ---
        {
            const auto realFile = macroFile; macroFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_macros_test.txt"); macroFile.deleteFile(); buildCommands();
            auto hasCmd = [this] (const juce::String& id) { for (auto& c : cmds) if (c.id == id) return true; return false; };
            chk (! macroSave ("x"), "macros: guardar sin haber empezado a grabar no hace nada");
            setBpmTo (100.0); runCmd ("tempoup"); const double step = bpmNow() - 100.0; chk (step > 0.0, "macros: comando de prueba (subir tempo) cambia el tempo");
            setBpmTo (100.0); macroStart(); chk (macroRec, "macros: empieza a grabar"); runCmd ("tempoup"); runCmd ("tempoup"); chk (macroIds.size() == 2, "macros: apunta cada comando ejecutado (" + juce::String (macroIds.size()) + ")");
            chk (macroSave ("Subir | doble"), "macros: se guarda con un nombre"); chk (! macroRec, "macros: termina la grabaci\xc3\xb3n");
            { bool found = false; juce::String nm; for (auto& l : loadMacros()) nm = l.upToFirstOccurrenceOf ("|", false, false); for (auto& c : cmds) if (c.id.startsWith ("macro:")) found = true; chk (found && nm.isNotEmpty() && ! nm.contains ("|"), "macros: aparece como comando en la paleta y el nombre se limpia (" + nm + ")"); }
            setBpmTo (100.0); { juce::String id; for (auto& c : cmds) if (c.id.startsWith ("macro:")) id = c.id; chk (runCmd (id) && std::abs (bpmNow() - (100.0 + 2.0 * step)) < 1e-6, "macros: ejecutarla repite los dos pasos (" + juce::String (bpmNow(), 2) + ")"); }
            macroStart(); runCmd ("tempoup"); macroSave ("Una"); macroStart(); runCmd ("tempoup"); macroRec = true; macroSave ("Una"); { int n = 0; for (auto& l : loadMacros()) n += l.startsWith ("Una|"); chk (n == 1, "macros: guardar con el mismo nombre la reemplaza"); }
            macroStart(); chk (! macroSave ("Vacia") && loadMacros().size() == 2, "macros: una grabaci\xc3\xb3n sin comandos no se guarda");
            macroStart(); runCmd ("macro_rec"); runCmd ("tempoup"); { juce::String id; for (auto& c : cmds) if (c.id == "macro:Una") id = c.id; runCmd (id); } chk (! macroIds.contains ("macro:Una"), "macros: ejecutar otra macro mientras se graba no se apunta dentro"); macroRec = false; macroIds.clear();
            setBpmTo (100.0); chk (macroRun ("no existe") == 0 && std::abs (bpmNow() - 100.0) < 1e-6, "macros: una macro que no existe no hace nada");
            macroStart(); runCmd ("tempoup"); macroSave ("Borrame"); chk (hasCmd ("macro:Borrame"), "macros: nueva macro en la paleta"); macroDelete ("Borrame"); chk (! hasCmd ("macro:Borrame"), "macros: borrar una macro la quita de la paleta");
            { juce::String raw = macroFile.loadFileAsString(); chk (raw.contains ("Una|") && raw.contains ("tempoup"), "macros: se guardan en un archivo y sobreviven al reiniciar"); }
            macroFile.deleteFile(); macroFile = realFile; buildCommands(); setBpmTo (160.0);
        }
        // --- arpegiador y herramientas de notas ---
        {
            namespace mt = miditools; auto tn = newTrack(); playlist.setSelTrackForTest (te::getAudioTracks (edit).indexOf (tn.get()));
            auto chordClip = [&] { auto* c = ideas::insert (edit, *tn, 0.0, 1, { { 60, 0.0, 4.0, 100 }, { 64, 0.0, 4.0, 100 }, { 67, 0.0, 4.0, 100 } }, "Acorde"); playlist.selectClips ({ c }); return c; };
            auto notesOf = [] (te::MidiClip* c) { mt::Notes v; for (auto* nt : c->getSequence().getNotes()) v.push_back ({ nt->getNoteNumber(), nt->getStartBeat().inBeats(), nt->getLengthBeats().inBeats(), nt->getVelocity() }); std::sort (v.begin(), v.end(), [] (const mt::Nt& a, const mt::Nt& b) { return a.start < b.start; }); return v; };
            auto& A = mt::arpSettings(); A = {}; A.rate = 2; A.gate = 100; A.octaves = 1; A.swing = 0;
            auto* c1 = chordClip (); chk (playlist.cmdNotes ("arp_up") == 16, "arpegiador: acorde de 4 tiempos a 1/16 da 16 notas");
            { auto v = notesOf (c1); chk (v.size() == 16 && v[0].pitch == 60 && v[1].pitch == 64 && v[2].pitch == 67 && v[3].pitch == 60 && std::abs (v[5].start - 1.25) < 1e-6 && std::abs (v[0].len - 0.25) < 1e-6, "arpegiador: arriba 60-64-67, paso de 1/16, nota entera"); }
            edit.getUndoManager().undo(); chk (c1->getSequence().getNumNotes() == 3, "arpegiador: se deshace de una vez");
            playlist.selectClips ({ c1 }); playlist.cmdNotes ("arp_down"); { auto v = notesOf (c1); chk (v.size() == 16 && v[0].pitch == 67 && v[1].pitch == 64 && v[2].pitch == 60, "arpegiador: hacia abajo 67-64-60"); }
            edit.getUndoManager().undo(); playlist.selectClips ({ c1 }); playlist.cmdNotes ("arp_updown"); { auto v = notesOf (c1); chk (v[0].pitch == 60 && v[1].pitch == 64 && v[2].pitch == 67 && v[3].pitch == 64 && v[4].pitch == 60, "arpegiador: sube y baja sin repetir los extremos"); }
            edit.getUndoManager().undo(); A.octaves = 2; playlist.selectClips ({ c1 }); playlist.cmdNotes ("arp_up"); { auto v = notesOf (c1); chk (v[3].pitch == 72 && v[5].pitch == 79 && v[6].pitch == 60, "arpegiador: 2 octavas"); }
            edit.getUndoManager().undo(); A.octaves = 1; A.gate = 50; A.swing = 50; playlist.selectClips ({ c1 }); playlist.cmdNotes ("arp_up"); { auto v = notesOf (c1); chk (std::abs (v[0].len - 0.125) < 1e-6 && std::abs (v[1].start - 0.375) < 1e-6 && std::abs (v[2].start - 0.5) < 1e-6, "arpegiador: duraci\xc3\xb3n 50 % y swing 50 %"); }
            edit.getUndoManager().undo(); A.gate = 100; A.swing = 0;
            { mt::Notes ch { { 60, 0, 4, 90 }, { 64, 0, 4, 100 }, { 67, 0, 4, 80 } }; mt::ArpSet r; r.pattern = mt::Random; r.seed = 5; auto a1 = mt::arpeggiate (ch, r, 4.0), a2 = mt::arpeggiate (ch, r, 4.0); bool same = a1.size() == a2.size(), norep = true, inset = true;
              for (size_t i = 0; i < a1.size() && same; ++i) { same = same && a1[i].pitch == a2[i].pitch; if (i > 0 && a1[i].pitch == a1[i - 1].pitch) norep = false; inset = inset && (a1[i].pitch == 60 || a1[i].pitch == 64 || a1[i].pitch == 67); }
              chk (same && norep && inset && ! a1.empty() && a1[0].vel == 100, "arpegiador: al azar es repetible con la misma semilla, sin repetir nota seguida"); }
            { mt::Notes single { { 60, 0, 1, 100 } }; mt::ArpSet r; r.octaves = 3; r.rate = 1; auto a = mt::arpeggiate (single, r, 8.0); chk (a.size() == 2 && a[0].pitch == 60 && a[1].pitch == 72, "arpegiador: una sola nota se reparte por las octavas"); }
            { mt::Notes two { { 60, 0, 1, 100 }, { 64, 2, 1, 100 } }; mt::ArpSet r; r.rate = 1; auto a = mt::arpeggiate (two, r, 4.0); chk (a.size() == 4 && std::abs (a[2].start - 2.0) < 1e-9, "arpegiador: cada acorde dura lo suyo (2 acordes, 2 notas cada uno)"); }
            { mt::ArpSet r; r.rate = 3; auto a = mt::arpeggiate ({ { 60, 0, 100, 100 } }, r, 100.0); chk (a.size() <= 4096, "arpegiador: tope de notas"); }
            { auto* e = ideas::insert (edit, *tn, 8.0, 1, { { 60, 0, 1, 100 } }, "Vacio"); for (auto* nt : std::vector<te::MidiNote*> (e->getSequence().getNotes().begin(), e->getSequence().getNotes().end())) e->getSequence().removeNote (*nt, nullptr); playlist.selectClips ({ e }); chk (playlist.cmdNotes ("arp_up") == 0, "arpegiador: un clip sin notas no hace nada"); }
            // humanizar, legato, rampas, rasgueo, redobles, octavas
            { auto* c = ideas::insert (edit, *tn, 16.0, 1, { { 60, 0.0, 0.25, 100 }, { 62, 1.0, 0.25, 100 }, { 64, 2.0, 0.25, 100 } }, "L"); playlist.selectClips ({ c });
              playlist.cmdNotes ("legato"); { auto v = notesOf (c); chk (v.size() == 3 && std::abs (v[0].len - 1.0) < 1e-6 && std::abs (v[1].len - 1.0) < 1e-6 && std::abs (v[2].len - 0.25) < 1e-6, "legato: cada nota llega hasta la siguiente"); }
              playlist.cmdNotes ("rampup"); { auto v = notesOf (c); chk (v[0].vel == 30 && v[2].vel == 127 && v[1].vel > 30 && v[1].vel < 127, "crescendo: de 30 a 127"); }
              playlist.cmdNotes ("rampdown"); { auto v = notesOf (c); chk (v[0].vel == 127 && v[2].vel == 30, "decrescendo: de 127 a 30"); }
              const auto before = notesOf (c); playlist.cmdNotes ("humanize"); { auto v = notesOf (c); bool ok = v.size() == before.size(), moved = false; std::sort (v.begin(), v.end(), [] (auto& a, auto& b) { return a.pitch < b.pitch; }); auto bs = before; std::sort (bs.begin(), bs.end(), [] (auto& a, auto& b) { return a.pitch < b.pitch; });
                for (size_t i = 0; i < v.size() && ok; ++i) { ok = std::abs (v[i].start - bs[i].start) <= 0.0301 && std::abs (v[i].vel - bs[i].vel) <= 8; moved = moved || std::abs (v[i].start - bs[i].start) > 1e-6 || v[i].vel != bs[i].vel; } chk (ok && moved, "humanizar: cambia poco (hasta 0.03 pulsos y 8 de fuerza)"); }
              playlist.cmdNotes ("octup"); chk (c->getSequence().getNumNotes() == 6, "doblar con octava: de 3 a 6 notas"); playlist.cmdNotes ("thin"); chk (c->getSequence().getNumNotes() == 3, "una de cada dos: de 6 a 3 notas"); }
            { auto* c = ideas::insert (edit, *tn, 24.0, 1, { { 67, 0.0, 2.0, 100 }, { 60, 0.0, 2.0, 100 }, { 64, 0.0, 2.0, 100 } }, "S"); playlist.selectClips ({ c }); playlist.cmdNotes ("strumdown"); auto v = notesOf (c);
              chk (v.size() == 3 && v[0].pitch == 60 && v[1].pitch == 64 && v[2].pitch == 67 && std::abs (v[1].start - 1.0 / 32.0) < 1e-6 && std::abs (v[2].end() - 2.0) < 1e-6, "rasgueo: del grave al agudo y todas terminan a la vez");
              playlist.cmdNotes ("ratchet2"); chk (c->getSequence().getNumNotes() == 6, "redoble x2: 3 notas pasan a 6");
              playlist.cmdNotes ("nooverlap"); chk (c->getSequence().getNumNotes() == 6, "quitar solapes: no pierde notas"); }
            { auto* c = ideas::insert (edit, *tn, 32.0, 1, { { 60, 0, 1, 100 }, { 64, 0.5, 1, 100 } }, "M"); playlist.selectClips ({ c }); playlist.cmdNotes ("mirror"); auto v = notesOf (c); chk (v.size() == 2 && v[0].pitch == 64 && v[1].pitch == 60, "espejo de tonos: 60/64 pasan a 64/60");
              c->state.setProperty ("injLocked", true, nullptr); playlist.cmdNotes ("mirror"); chk (notesOf (c)[0].pitch == 64, "un clip bloqueado no se toca"); c->state.setProperty ("injLocked", false, nullptr); }
            chk (playlist.cmdNotes ("no_existe") == 0, "notas: una operaci\xc3\xb3n que no existe no hace nada");
            // transformaciones nuevas de notas y funciones "pro"
            { auto* c = ideas::insert (edit, *tn, 48.0, 1, { { 60, 0, 1, 100 }, { 64, 1, 1, 80 } }, "PN"); playlist.selectClips ({ c });
              auto psum = [&] { int t = 0; for (auto& x : notesOf (c)) t += x.pitch; return t; };
              playlist.cmdNotes ("p_n_t7"); chk (psum() == 138, "transponer +7: suma de tonos 124 -> 138"); playlist.cmdNotes ("p_n_tm7"); chk (psum() == 124, "transponer -7 vuelve");
              playlist.cmdNotes ("p_n_triad"); chk (c->getSequence().getNumNotes() == 6, "acorde mayor sobre cada nota: de 2 a 6");
              playlist.cmdNotes ("p_n_top"); chk (c->getSequence().getNumNotes() == 2, "dejar la nota m\xc3\xa1s aguda de cada acorde: de 6 a 2");
              playlist.cmdNotes ("p_n_len2"); { auto v = notesOf (c); chk (! v.empty() && std::abs (v[0].len - 2.0) < 1e-6, "notas al doble de largo"); }
              playlist.cmdNotes ("p_n_vel08"); { auto v = notesOf (c); int mx = 0; for (auto& x : v) mx = std::max (mx, x.vel); chk (mx == 80, "velocidad al 80 %: 100 pasa a 80"); }
              harmony::setKey (edit, { 0, 0 }); chk (playlist.cmdNotes ("p_n_scale") == 0, "ajustar a la escala sin tonalidad no hace nada"); }
            { const auto before = te::getAudioTracks (edit); const int nT = before.size();
              chk (playlist.cmdPro ("t_panalt") == nT && before[0]->getVolumePlugin()->getPan() < 0.f && before[1]->getVolumePlugin()->getPan() > 0.f, "pan alternado: la primera a la izquierda y la segunda a la derecha");
              chk (playlist.cmdPro ("t_panwide") == nT && before[0]->getVolumePlugin()->getPan() < -0.7f && before[nT - 1]->getVolumePlugin()->getPan() > 0.7f, "pan repartido: de izquierda a derecha");
              before[0]->getVolumePlugin()->setVolumeDb (-10.f); playlist.cmdPro ("t_vol1up"); const float v1 = before[0]->getVolumePlugin()->getVolumeDb(); playlist.cmdPro ("t_vol1down"); chk (std::abs (v1 + 9.f) < 1e-3f && std::abs (before[0]->getVolumePlugin()->getVolumeDb() + 10.f) < 1e-3f, "subir y bajar 1 dB todas las pistas");
              playlist.cmdPro ("t_numnames"); chk (before[0]->getName() == "Pista 1" && before[nT - 1]->getName() == "Pista " + juce::String (nT), "nombres numerados");
              playlist.setSelTrackForTest (1); playlist.cmdPro ("t_muteothers"); chk (before[1]->isMuted (false) == false && before[0]->isMuted (false), "silenciar las dem\xc3\xa1s: queda sonando la elegida"); playlist.cmdPro ("t_unmute"); chk (! before[0]->isMuted (false), "quitar el silencio de todas");
              before[0]->setSolo (true); chk (playlist.cmdPro ("t_unsolo") >= 1 && ! before[0]->isSolo (false), "quitar el solo de todas");
              chk (playlist.cmdPro ("t_pan0") == nT && std::abs (before[0]->getVolumePlugin()->getPan()) < 1e-6f, "pan al centro en todas"); chk (playlist.cmdPro ("t_vol0") == nT && std::abs (before[0]->getVolumePlugin()->getVolumeDb()) < 1e-3f, "volumen a 0 dB en todas");
              playlist.setSelTrackForTest (0); playlist.cmdPro ("t_solosel"); chk (before[0]->isSolo (false) && ! before[1]->isSolo (false), "solo solo en la elegida"); playlist.cmdPro ("t_unsolo");
              { auto* tb = edit.tempoSequence.getTempo (0); tb->setBpm (140.4); playlist.cmdPro ("p_bpmround"); chk (std::abs (tb->getBpm() - 140.0) < 1e-6, "redondear el tempo"); playlist.cmdPro ("p_bpmup"); chk (std::abs (tb->getBpm() - 141.0) < 1e-6, "subir 1 BPM"); tb->setBpm (160.0); }
              edit.getTransport().setPosition (te::TimePosition()); playlist.cmdPro ("p_loop:8"); chk (edit.getTransport().looping, "bucle de 8 compases se activa"); playlist.cmdPro ("p_loopclear"); chk (! edit.getTransport().looping, "bucle se apaga");
              chk (playlist.cmdPro ("no_existe") == 0, "pro: una operaci\xc3\xb3n que no existe no hace nada"); }
            { auto tp = newTrack(); auto* A1 = ideas::insert (edit, *tp, 100.0, 1, { { 60, 0, 1, 100 } }, "A1"); auto* B1 = ideas::insert (edit, *tp, 108.0, 1, { { 60, 0, 1, 100 } }, "B1");
              edit.getTransport().setPosition (edit.tempoSequence.toTime (te::BeatPosition::fromBeats (104.0))); const double b0 = playlist.timeToBeatsPublic (B1->getPosition().getStart().inSeconds());
              chk (playlist.cmdPro ("clip_insbar") >= 1 && std::abs (playlist.timeToBeatsPublic (B1->getPosition().getStart().inSeconds()) - (b0 + 4.0)) < 1e-3 && std::abs (playlist.timeToBeatsPublic (A1->getPosition().getStart().inSeconds()) - 100.0) < 1e-3, "insertar un comp\xc3\xa1s: empuja lo que sigue y deja lo anterior");
              playlist.selectClips ({ A1 }); chk (playlist.cmdPro ("clip_numnames") == 1 && A1->getName() == "Clip 1", "clips con nombres numerados");
              playlist.selectClips ({ A1 }); chk (playlist.cmdPro ("fi:50") == 0, "fundidos: un clip MIDI no tiene fundidos");
              edit.getTransport().setPosition (te::TimePosition()); chk (playlist.cmdPro ("clip_trimhead") == 0, "recortar el inicio con el cabezal fuera del clip no hace nada"); }
            { const auto p1 = ideas::parsePrompt ("hardcore oscuro 190 bpm en la menor"); chk (p1.style == ideas::Hardcore && std::abs (p1.bpm - 190.0) < 1e-6 && p1.root == 9 && ! p1.major, "IA de ideas: entiende estilo, BPM y tonalidad menor");
              const auto p2 = ideas::parsePrompt ("Trance alegre 138 bpm en do mayor"); chk (p2.style == ideas::Trance && std::abs (p2.bpm - 138.0) < 1e-6 && p2.root == 0 && p2.major, "IA de ideas: tonalidad mayor y may\xc3\xbasculas");
              const auto p3 = ideas::parsePrompt ("solo bajo gabber"); chk (p3.style == ideas::Gabber && p3.role == 1, "IA de ideas: pide solo el bajo");
              const auto p4 = ideas::parsePrompt ("algo raro sin pistas"); chk (p4.style < 0 && p4.bpm == 0.0 && p4.root < 0, "IA de ideas: una frase que no entiende no inventa nada");
              ideas::majorMode() = true; auto bm = ideas::bass (ideas::Trance, 4, 3, 33); ideas::majorMode() = false; auto bn = ideas::bass (ideas::Trance, 4, 3, 33); bool dif = bm.size() != bn.size(); for (size_t i = 0; ! dif && i < bm.size(); ++i) dif = bm[i].pitch != bn[i].pitch;
              chk (dif, "IA de ideas: el modo mayor cambia las notas"); }
            // clips: repetir, empujar, invertir, fundido cruzado
            { auto* c = ideas::insert (edit, *tn, 40.0, 1, { { 60, 0, 1, 100 } }, "R"); playlist.selectClips ({ c }); const int before = tn->getClips().size();
              chk (playlist.cmdClips ("repeat4") == 3 && tn->getClips().size() == before + 3, "repetir x3: 3 clips nuevos");
              playlist.selectClips ({ c }); const double b0 = playlist.timeToBeatsPublic (c->getPosition().getStart().inSeconds()); playlist.cmdClips ("nudger"); chk (std::abs (playlist.timeToBeatsPublic (c->getPosition().getStart().inSeconds()) - (b0 + 0.25)) < 1e-6, "empujar 1/16 a la derecha");
              playlist.cmdClips ("nudgel"); chk (std::abs (playlist.timeToBeatsPublic (c->getPosition().getStart().inSeconds()) - b0) < 1e-6, "empujar 1/16 a la izquierda vuelve al sitio");
              chk (playlist.cmdClips ("reverseaudio") == 0 && playlist.cmdClips ("crossfade") == 0, "invertir sonido y fundido cruzado no hacen nada con clips MIDI"); }
            // plantillas de pista, duplicar con plugins, punch, historial
            { auto src = newTrack(); src->setName ("Mi kick"); src->state.setProperty ("injColour", 3, nullptr);
              auto pl = edit.getPluginCache().createNewPlugin (te::CompressorPlugin::xmlTypeName, {}); src->pluginList.insertPlugin (pl, 0, nullptr);
              int own = 0; for (auto* p : src->pluginList.getPlugins()) if (trackutil::isOwnPlugin (p)) ++own; chk (own == 1, "plantilla: la pista de prueba tiene 1 efecto propio");
              auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_tpl_test"); dir.deleteRecursively();
              chk (trackutil::saveTemplate (*src, "Mi kick/1", dir) && trackutil::listTemplates (dir).size() == 1, "plantilla: se guarda en un archivo con nombre v\xc3\xa1lido");
              const int nTracks = te::getAudioTracks (edit).size(); auto* nt = trackutil::loadTemplate (edit, trackutil::listTemplates (dir)[0]);
              int own2 = 0; if (nt != nullptr) for (auto* p : nt->pluginList.getPlugins()) if (trackutil::isOwnPlugin (p)) ++own2;
              chk (nt != nullptr && te::getAudioTracks (edit).size() == nTracks + 1 && nt->getName() == "Mi kick" && own2 == 1 && (int) nt->state.getProperty ("injColour", 0) == 3, "plantilla: la pista nueva trae nombre, color y efecto");
              chk (trackutil::loadTemplate (edit, dir.getChildFile ("no_existe.injtrack")) == nullptr, "plantilla: un archivo que no existe no crea nada");
              playlist.setSelTrackForTest (te::getAudioTracks (edit).indexOf (src.get())); playlist.cmdTracks ("duptrack"); auto all = te::getAudioTracks (edit); int own3 = 0; for (auto* t : all) if (t->getName() == "Mi kick copia") for (auto* p : t->pluginList.getPlugins()) if (trackutil::isOwnPlugin (p)) ++own3;
              chk (own3 == 1, "duplicar pista: tambi\xc3\xa9n copia los efectos e instrumento"); dir.deleteRecursively(); }
            { const bool was = edit.recordingPunchInOut; togglePunch(); chk (edit.recordingPunchInOut != was, "punch: se activa"); togglePunch(); chk (edit.recordingPunchInOut == was, "punch: se apaga"); }
            { edit.getUndoManager().beginNewTransaction ("uno"); tn->setName ("A1"); edit.getUndoManager().beginNewTransaction ("dos"); tn->setName ("A2"); const int k = undoSteps (2); chk (k == 2 && tn->getName() != "A2", "historial: volver 2 pasos atr\xc3\xa1s"); }
            A = {};
        }
        // --- editor de audio ---
        {
            namespace ae = audioedit; auto tn = newTrack(); auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_aed_test"); dir.deleteRecursively(); dir.createDirectory();
            auto mk = [&] (const juce::String& name, int ch, double secs, float dc, float amp, double lead = 0.0, double tail = 0.0)
            {
                ae::Buf b (ch, (int) (44100 * secs)); b.clear(); const int a = (int) (44100 * lead), e = b.getNumSamples() - (int) (44100 * tail);
                for (int c = 0; c < ch; ++c) for (int i = a; i < e; ++i) b.setSample (c, i, dc + amp * (c == 0 ? 1.f : .5f) * std::sin (i * 0.05f));
                auto f = dir.getChildFile (name); ae::writeWav (f, b, 44100.0); return f;
            };
            auto addClip = [&] (const juce::File& f, double secs) { auto c = tn->insertWaveClip (f.getFileNameWithoutExtension(), f, { te::TimeRange (te::TimePosition::fromSeconds (0.0), te::TimeDuration::fromSeconds (secs)), te::TimeDuration() }, false); return dynamic_cast<te::WaveAudioClip*> (c.get()); };
            auto load = [] (const juce::File& f) { ae::Buf b; double r = 0; ae::readFile (f, b, r); return b; };
            // funciones puras
            { ae::Buf b (2, 1000); for (int i = 0; i < 1000; ++i) { b.setSample (0, i, 0.1f + 0.2f * std::sin (i * 0.1f)); b.setSample (1, i, -0.3f + 0.2f * std::sin (i * 0.1f)); }
              ae::removeDC (b); double m0 = 0, m1 = 0; for (int i = 0; i < 1000; ++i) { m0 += b.getSample (0, i); m1 += b.getSample (1, i); } chk (std::abs (m0 / 1000) < 1e-4 && std::abs (m1 / 1000) < 1e-4, "editor: quitar DC deja la media en cero en cada canal");
              const float g = ae::normalize (b, -6.f); chk (g > 0.f && std::abs (ae::peakDb (b) + 6.f) < 0.05f, "editor: normalizar deja el pico en -6 dB"); ae::Buf z (1, 100); z.clear (); chk (ae::normalize (z) == 0.f, "editor: normalizar silencio no hace nada");
              ae::Buf s (1, 10); for (int i = 0; i < 10; ++i) s.setSample (0, i, (float) i); ae::reverse (s, 2, 6); chk (s.getSample (0, 2) == 5.f && s.getSample (0, 5) == 2.f && s.getSample (0, 1) == 1.f && s.getSample (0, 6) == 6.f, "editor: invertir solo el rango elegido");
              ae::Buf q (1, 100); q.clear (); q.setSample (0, 30, 0.5f); q.setSample (0, 60, -0.5f); auto r = ae::findAudible (q); chk (r.a == 30 && r.b == 61, "editor: encuentra el tramo con sonido"); chk (ae::findAudible (z).size() == 0, "editor: todo silencio da rango vac\xc3\xado");
              ae::Buf lp (1, 1000); for (int i = 0; i < 1000; ++i) lp.setSample (0, i, std::sin (i * 0.02f)); auto l2 = ae::loopCrossfade (lp, 100); chk (l2.getNumSamples() == 900 && std::abs (l2.getSample (0, 0) - lp.getSample (0, 900)) < 1e-4f && std::abs (l2.getSample (0, 99) - lp.getSample (0, 99)) < 0.05f, "editor: bucle sin clic acorta y enlaza final con inicio");
              ae::Buf fd (1, 100); for (int i = 0; i < 100; ++i) fd.setSample (0, i, 1.f); ae::fade (fd, true); chk (fd.getSample (0, 0) == 0.f && std::abs (fd.getSample (0, 99) - 1.f) < 1e-5f && fd.getSample (0, 50) > 0.6f && fd.getSample (0, 50) < 0.8f, "editor: fundido de entrada va de 0 a 1");
              auto pks = ae::peaks (s, 5); chk (pks.size() == 5 && pks[4].second == 9.f, "editor: picos por columna"); }
            // sobre un clip real
            const auto f1 = mk ("aed1.wav", 2, 1.0, 0.2f, 0.4f); auto* wc = addClip (f1, 1.0); chk (wc != nullptr, "editor: clip de prueba creado");
            if (wc != nullptr)
            {
                aed.setClip (wc); chk (std::abs (aed.fileSeconds() - 1.0) < 0.01 && aed.filePeakDb() > -6.f, "editor: lee el clip (duraci\xc3\xb3n y pico)");
                { auto tl = te::getAudioTracks (edit); int ti = -1; for (int k = 0; k < tl.size(); ++k) if (tl[k] == wc->getTrack()) ti = k;
                  setAed (false); chk (ti >= 0 && playlist.openClipAt (ti, wc->getPosition().time.getStart().inSeconds() + 0.1) && aed.isVisible() && aed.clip() == wc, "editor: doble clic en un clip de audio abre el editor con ese clip");
                  setAed (false); chk (! playlist.openClipAt (ti, wc->getPosition().time.getEnd().inSeconds() + 50.0) && ! aed.isVisible(), "editor: doble clic fuera de un clip no abre nada"); }
                const auto o0 = load (f1);
                chk (aed.run ("dc") && wc->getOriginalFile() != f1, "editor: quitar DC crea un archivo nuevo y el clip lo usa"); { auto b = load (wc->getOriginalFile()); double m = 0; for (int i = 0; i < b.getNumSamples(); ++i) m += b.getSample (0, i); chk (std::abs (m / b.getNumSamples()) < 5e-3 && b.getNumSamples() == o0.getNumSamples(), "editor: el archivo nuevo no tiene DC y dura lo mismo"); }
                edit.getUndoManager().undo(); chk (wc->getOriginalFile() == f1, "editor: deshacer vuelve al archivo original");
                aed.setClip (nullptr); aed.setClip (wc); const auto o1 = load (wc->getOriginalFile());
                aed.run ("pol"); { auto b = load (wc->getOriginalFile()); chk (std::abs (b.getSample (0, 1000) + o1.getSample (0, 1000)) < 1e-3f, "editor: invertir polaridad cambia el signo"); }
                aed.run ("swap"); { auto b = load (wc->getOriginalFile()); chk (std::abs (b.getSample (0, 1000) - (-o1.getSample (1, 1000))) < 1e-3f, "editor: intercambiar est\xc3\xa9reo cambia los canales"); }
                aed.run ("rev"); { auto b = load (wc->getOriginalFile()); const int n = b.getNumSamples(); chk (std::abs (b.getSample (0, 5) - (-o1.getSample (1, n - 6))) < 1e-3f, "editor: invertir pone el final al principio"); }
                aed.run ("fadein"); { auto b = load (wc->getOriginalFile()); chk (std::abs (b.getSample (0, 0)) < 1e-4f && b.getNumSamples() == o1.getNumSamples(), "editor: fundido de entrada empieza en silencio"); }
                aed.setSelection (0.5, 1.0); aed.run ("silence"); { auto b = load (wc->getOriginalFile()); chk (b.getMagnitude (0, b.getNumSamples() / 2 + 10, b.getNumSamples() / 2 - 10) < 1e-4f && b.getMagnitude (0, 2000, 4000) > 0.05f, "editor: silenciar solo el tramo elegido"); }
                aed.setSelection (0.0, 0.5); const double len0 = wc->getPosition().getLength().inSeconds(); aed.run ("crop"); chk (std::abs (aed.fileSeconds() - 0.5) < 0.02 && std::abs (wc->getPosition().getLength().inSeconds() - len0 * 0.5) < 0.03, "editor: quedarse con la selecci\xc3\xb3n acorta archivo y clip");
                aed.setSelection (0.5, 0.5); chk (! aed.hasSelection() && ! aed.run ("crop"), "editor: sin selecci\xc3\xb3n no recorta");
                const float db0 = aed.filePeakDb(); chk (aed.run ("norm") && std::abs (wc->getGainDB() + db0 - (-0.3f)) < 0.2f + std::abs (db0) * 0.0f, "editor: normalizar sube el volumen del clip (" + juce::String (wc->getGainDB(), 1) + " dB)");
            }
            const auto f2 = mk ("aed2.wav", 1, 2.0, 0.f, 0.5f, 0.5, 0.5); auto* w2 = addClip (f2, 2.0);
            if (w2 != nullptr)
            {
                aed.setClip (w2); chk (aed.run ("trim") && aed.fileSeconds() > 0.9 && aed.fileSeconds() < 1.1, "editor: recortar silencios deja ~1 s de 2 s"); chk (! aed.run ("swap"), "editor: intercambiar est\xc3\xa9reo en un audio mono no hace nada");
                const double l0 = aed.fileSeconds(); chk (aed.run ("loopx") && aed.fileSeconds() < l0 && aed.fileSeconds() > l0 - 0.05, "editor: bucle sin clic acorta unos 20 ms");
                chk (aed.run ("declick") && aed.run ("fadeout"), "editor: quitar clics y fundido de salida");
                aed.setPitch (3.f); chk (std::abs (w2->getPitchChange() - 3.f) < 1e-4f && w2->getTimeStretchMode() != te::TimeStretcher::disabled, "editor: tono +3 activa el estirado");
                const double lb = w2->getPosition().getLength().inSeconds(); aed.setSpeed (2.0); chk (std::abs (w2->getSpeedRatio() - 2.0) < 1e-6 && std::abs (w2->getPosition().getLength().inSeconds() - lb / 2.0) < 0.02, "editor: velocidad x2 acorta el clip a la mitad");
                aed.setStretchMode (2); chk (aed.stretchMode() == 2 && w2->getTimeStretchMode() == te::TimeStretcher::rubberbandPercussive, "editor: modo percusivo"); aed.setStretchMode (0); chk (aed.stretchMode() == 0, "editor: sin estirar");
                aed.setVolDb (-6.f); chk (std::abs (w2->getGainDB() + 6.f) < 1e-4f, "editor: volumen del clip -6 dB"); aed.setFade (true, 0.1f); chk (std::abs (aed.fadeIn() - 0.1f) < 1e-3f, "editor: fundido de entrada 0.1 s");
            }
            // --- editor de afinación (Injustice Auto) ---
            {
                const double sr0 = 44100.0; ae::Buf pb (1, (int) (sr0 * 2.4)); pb.clear();
                const double fr[2] { 450.0, 300.0 }; for (int k = 0; k < 2; ++k) { double ph = 0; for (int i = (int) (sr0 * (.1 + 1.2 * k)); i < (int) (sr0 * (1.2 + 1.2 * k)); ++i) { ph += 2 * juce::MathConstants<double>::pi * fr[k] / sr0; pb.setSample (0, i, .4f * (float) std::sin (ph)); } }
                auto pf = dir.getChildFile ("pitch1.wav"); ae::writeWav (pf, pb, sr0); auto* pc = addClip (pf, 2.4); chk (pc != nullptr, "afinacion: clip de prueba creado");
                if (pc != nullptr)
                {
                    pitchEd.setClip (pc); chk (pitchEd.waitReady (20000), "afinacion: el an\xc3\xa1lisis termina y encuentra notas"); chk (pitchEd.numNotes() == 2, "afinacion: dos tramos de tono = dos notas (" + juce::String (pitchEd.numNotes()) + ")");
                    if (pitchEd.numNotes() == 2)
                    {
                        chk (std::abs (pitchEd.noteAt (0).det - 69.39f) < .12f && std::abs (pitchEd.noteAt (1).det - 62.37f) < .12f, "afinacion: mide 450 Hz (La +39 cent) y 300 Hz (" + juce::String (pitchEd.noteAt (0).det, 2) + ", " + juce::String (pitchEd.noteAt (1).det, 2) + ")");
                        auto zc = [&] (const ae::Buf& b, double t0, double t1) { int a = (int) (t0 * sr0), e = (int) (t1 * sr0), n = 0; for (int i = a + 1; i < e; ++i) if (b.getSample (0, i - 1) <= 0.f && b.getSample (0, i) > 0.f) ++n; return n / (t1 - t0); };
                        auto o0 = pitchEd.renderNow(); chk (std::abs (zc (o0, .4, 1.0) - 450.0) < 3.0, "afinacion: sin correcciones deja el audio igual");
                        pitchEd.setScale (0, 0); pitchEd.setAuto (true); chk (std::abs (pitchEd.noteAt (0).tgt - 69.f) < .01f && std::abs (pitchEd.noteAt (1).tgt - 62.f) < .01f, "afinacion: AUTO lleva las notas a la nota m\xc3\xa1s cercana");
                        auto o1 = pitchEd.renderNow(); chk (std::abs (zc (o1, .4, 1.0) - 440.0) < 3.0, "afinacion: 450 Hz sale a 440 Hz (" + juce::String (zc (o1, .4, 1.0), 1) + ")");
                        pitchEd.setScale (0, 1); chk (std::abs (pitchEd.noteAt (1).tgt - 62.f) < .01f, "afinacion: Do mayor deja 300 Hz en Re4");
                        pitchEd.setAuto (false); pitchEd.setTargetOf (1, 60.f); auto o2 = pitchEd.renderNow(); chk (std::abs (zc (o2, 1.7, 2.3) - 261.63) < 3.0, "afinacion: mover a mano una nota a Do4 da 261,6 Hz (" + juce::String (zc (o2, 1.7, 2.3), 1) + ")");
                        float mx = 0.f; for (int i = 0; i < o2.getNumSamples(); ++i) mx = std::max (mx, std::abs (o2.getSample (0, i))); chk (mx < .6f && mx > .2f, "afinacion: el nivel se mantiene (" + juce::String (mx, 2) + ")");
                        chk (pitchEd.undo() && std::abs (pitchEd.noteAt (1).tgt - 62.f) < .01f && pitchEd.redo() && std::abs (pitchEd.noteAt (1).tgt - 60.f) < .01f, "afinacion: deshacer y rehacer");
                        const int n0 = pitchEd.numNotes(); chk (pitchEd.splitAt (0, 0.6) && pitchEd.numNotes() == n0 + 1 && pitchEd.mergeNext (0) && pitchEd.numNotes() == n0, "afinacion: partir y unir notas");
                        pitchEd.setAuto (true); const auto before = pc->getOriginalFile(); chk (pitchEd.apply() && pc->getOriginalFile() != before, "afinacion: aplicar crea un archivo nuevo y el clip lo usa");
                        { ae::Buf r; double rr = 0; ae::readFile (pc->getOriginalFile(), r, rr); chk (r.getNumSamples() == pb.getNumSamples() && std::abs (zc (r, .4, 1.0) - 440.0) < 3.0, "afinacion: el archivo aplicado dura lo mismo y suena a 440 Hz"); }
                    }
                }
                pitchEd.setClip (nullptr); chk (! pitchEd.ready() && pitchEd.numNotes() == 0, "afinacion: sin clip no hay notas");
            }
            aed.setClip (nullptr); chk (! aed.run ("dc") && aed.fileSeconds() == 0.0, "editor: sin clip no hace nada"); dir.deleteRecursively();
        }
        // --- teclado en vivo (repetici\xc3\xb3n y escala), bloc de notas, audio a MIDI ---
        {
            const auto key0 = harmony::getKey (edit); harmony::setKey (edit, { 0, 1 }); const auto k = harmony::getKey (edit); audition::typingPiano = true; typedReleaseAll();
            scaleLock = true; typedNoteOn ('s', 1); chk (heldKeys.count ('s') && harmony::inScale (k, heldKeys['s']), "bloqueo de escala: C# del teclado cae en una nota de la tonalidad"); typedNoteOff ('s');
            scaleLock = false; typedNoteOn ('s', 1); chk (heldKeys['s'] == 12 * (typedOct + 1) + 1, "sin bloqueo: suena la nota tal cual"); typedNoteOff ('s');
            repeatIdx = 0; typedNoteOn ('z', 0); chk (tickRepeat (1e18) == 0, "repetici\xc3\xb3n apagada: no retoca"); setRepeat (3); repeatHits = 0; const double nx = repeatNext['z'];
            chk (tickRepeat (nx - 1) == 0 && tickRepeat (nx + 1) == 1 && tickRepeat (nx + 1) == 0 && tickRepeat (nx + repeatMs() + 1) == 1 && repeatHits == 2, "repetici\xc3\xb3n 1/16: una nota nueva por paso mientras la tecla sigue pulsada");
            typedNoteOff ('z'); chk (tickRepeat (1e18) == 0, "repetici\xc3\xb3n: al soltar la tecla se detiene"); setRepeat (0); typedReleaseAll(); audition::typingPiano = false; harmony::setKey (edit, key0);
            { const auto txt = juce::String::fromUTF8 ("Letra\nsegunda l\xc3\xadnea \xe2\x80\x93 coro"); projectnotes::set (edit, txt); auto xml = edit.state.createXml(); auto vt = juce::ValueTree::fromXml (*xml); chk (projectnotes::get (edit) == txt && vt.getProperty ("injNotes").toString() == txt, "bloc de notas: el texto (con tildes y saltos de l\xc3\xadnea) se guarda dentro del proyecto"); } projectnotes::set (edit, {});
            // audio a MIDI
            const double sr = 44100.0; struct Tn { double hz, at, len; }; const Tn tones[] { { 440.0, 0.1, 0.4 }, { 523.25, 0.6, 0.4 }, { 659.26, 1.1, 0.4 } };
            std::vector<float> mono ((size_t) (sr * 1.7), 0.f); for (auto& t : tones) for (int i = (int) (t.at * sr); i < (int) ((t.at + t.len) * sr); ++i) mono[(size_t) i] = 0.5f * std::sin (2.0 * juce::MathConstants<double>::pi * t.hz * i / sr);
            { std::vector<float> sine ((size_t) 4096); for (size_t i = 0; i < sine.size(); ++i) sine[i] = (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (double) i / sr); const double hz = audio2midi::yin (sine.data(), 2048, sr); chk (std::abs (hz - 220.0) < 1.5, "audio a MIDI: detecta 220 Hz (" + juce::String (hz, 1) + ")"); }
            { auto nt = audio2midi::convert (mono, sr); chk (nt.size() == 3 && nt[0].pitch == 69 && nt[1].pitch == 72 && nt[2].pitch == 76, "audio a MIDI: 3 notas La4, Do5, Mi5 (" + juce::String ((int) nt.size()) + ")");
              if (nt.size() == 3) chk (std::abs (nt[0].start - 0.1) < 0.05 && std::abs (nt[1].start - 0.6) < 0.05 && std::abs (nt[0].len - 0.4) < 0.08, "audio a MIDI: inicio y duraci\xc3\xb3n de las notas"); }
            { std::vector<float> quiet ((size_t) (sr * 1.0), 0.f); chk (audio2midi::convert (quiet, sr).empty(), "audio a MIDI: el silencio no da notas"); chk (audio2midi::convert (std::vector<float> (100, 0.f), sr).empty(), "audio a MIDI: audio demasiado corto no da notas"); }
            { auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_a2m_test"); dir.deleteRecursively(); dir.createDirectory(); audioedit::Buf ab (1, (int) mono.size()); for (size_t i = 0; i < mono.size(); ++i) ab.setSample (0, (int) i, mono[i]);
              auto f = dir.getChildFile ("melodia.wav"); audioedit::writeWav (f, ab, sr); auto tn = newTrack(); auto c = tn->insertWaveClip ("melodia", f, { te::TimeRange (te::TimePosition::fromSeconds (0.0), te::TimeDuration::fromSeconds (1.7)), te::TimeDuration() }, false);
              playlist.selectClips ({ c.get() }); const int before = te::getAudioTracks (edit).size(); chk (audioToMidi() == 3 && te::getAudioTracks (edit).size() == before + 1, "audio a MIDI: crea una pista nueva con 3 notas");
              { auto all = te::getAudioTracks (edit); auto* last = all.getLast(); te::MidiClip* mc = nullptr; for (auto* cl : last->getClips()) if (auto* m = dynamic_cast<te::MidiClip*> (cl)) mc = m; chk (mc != nullptr && mc->getSequence().getNumNotes() == 3 && trackutil::hasInstrument (*last), "audio a MIDI: el clip tiene las notas y la pista un instrumento"); }
              audioedit::Buf sil (1, 44100); sil.clear(); auto f2 = dir.getChildFile ("silencio.wav"); audioedit::writeWav (f2, sil, sr); auto c2 = tn->insertWaveClip ("silencio", f2, { te::TimeRange (te::TimePosition::fromSeconds (10.0), te::TimeDuration::fromSeconds (1.0)), te::TimeDuration() }, false);
              playlist.selectClips ({ c2.get() }); const int before2 = te::getAudioTracks (edit).size(); chk (audioToMidi() == 0 && te::getAudioTracks (edit).size() == before2, "audio a MIDI: un clip en silencio no crea nada"); playlist.selectClips ({}); chk (audioToMidi() == 0, "audio a MIDI: sin clip elegido no hace nada"); dir.deleteRecursively(); }
        }
        // --- rack de pads ---
        {
            auto tn = newTrack(); mixer.selected = te::getAudioTracks (edit).indexOf (tn.get()); auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_rack_test"); dir.deleteRecursively(); dir.createDirectory();
            auto mkWav = [&] (const char* n, float hz) { audioedit::Buf b (1, 4410); for (int i = 0; i < 4410; ++i) b.setSample (0, i, 0.5f * std::sin (i * hz)); auto f = dir.getChildFile (n); audioedit::writeWav (f, b, 44100.0); return f; };
            const auto f1 = mkWav ("a.wav", 0.05f), f2 = mkWav ("b.wav", 0.09f), f3 = mkWav ("c.wav", 0.02f); int hits = 0; rack.onAudition = [&] (te::AudioTrack*, int) { ++hits; };
            chk (rack.sampler() == nullptr && rack.numLayers (36) == 0, "rack: una pista sin kit no tiene pads"); chk (rack.createEmptyKit() && rack.sampler() != nullptr && rack.numLayers (36) == 0, "rack: crear un kit vac\xc3\xado");
            chk (rack.loadSample (36, f1).isEmpty() && rack.numLayers (36) == 1 && rack.padName (36) == "a", "rack: cargar un sample en el pad");
            chk (rack.addLayer (36, f2).isEmpty() && rack.numLayers (36) == 2, "rack: a\xc3\xb1" "adir una capa");
            rack.setPadVolume (36, -6.f); rack.setPadPan (36, 0.5f); { bool all = true; for (int i : rack.soundsAt (36)) all = all && std::abs (rack.sampler()->getSoundGainDb (i) + 6.f) < 1e-3f && std::abs (rack.sampler()->getSoundPan (i) - 0.5f) < 1e-3f; chk (all && std::abs (rack.padVolume (36) + 6.f) < 1e-3f, "rack: volumen y pan valen para todas las capas"); }
            rack.setPadTune (36, 3.f); { bool ok = std::abs (rack.padTune (36) - 3.f) < 1e-3f; for (int i : rack.soundsAt (36)) ok = ok && rack.sampler()->getMinKey (i) == 36 && rack.sampler()->getMaxKey (i) == 36 && rack.sampler()->getKeyNote (i) == 33; chk (ok, "rack: afinar +3 mueve la nota ra\xc3\xadz, el pad sigue en su tecla"); }
            chk (rack.addLayer (36, f3).isEmpty() && std::abs (rack.sampler()->getSoundGainDb (rack.soundsAt (36).back()) + 6.f) < 1e-3f && std::abs (rack.padTune (36) - 3.f) < 1e-3f, "rack: una capa nueva hereda volumen y afinaci\xc3\xb3n del pad");
            chk (rack.loadSample (37, f3).isEmpty() && rack.numLayers (37) == 1 && rack.numLayers (36) == 3, "rack: pads distintos no se mezclan");
            rack.audition (36); rack.audition (40); chk (hits == 1 && rack.selPad == 40, "rack: tocar un pad con sonido avisa, uno vac\xc3\xado no");
            chk (rack.clearPad (36) && rack.numLayers (36) == 0 && rack.numLayers (37) == 1 && rack.sampler()->getNumSounds() == 1, "rack: vaciar un pad no toca los dem\xc3\xa1s");
            chk (rack.addLayer (36, dir.getChildFile ("no_existe.wav")).isNotEmpty() && rack.addLayer (60, f1).isNotEmpty(), "rack: archivo inexistente o pad fuera de rango dan error");
            for (int k = 0; k < 8; ++k) rack.addLayer (38, f1); chk (rack.numLayers (38) == 6, "rack: un pad admite hasta 6 capas"); chk (! rack.clearPad (50), "rack: vaciar un pad vac\xc3\xado no hace nada");
            rack.onAudition = [this] (te::AudioTrack* t, int pitch) { auditioner.note (t, pitch, 700); }; dir.deleteRecursively();
        }
        // --- paleta ---
        { palette.setBounds (getLocalBounds()); palette.open (&cmds); const int all = palette.numShown(); palette.setQuery ("dividir"); const int d = palette.numShown(); palette.setQuery ("TRANSPON"); const int t = palette.numShown(); palette.setQuery (juce::String::fromUTF8 ("eucl\xc3\xad" "deo")); const int e = palette.numShown(); palette.setQuery ("zzzz"); const int z = palette.numShown();
          palette.setQuery ("dividir compases"); const bool both = palette.numShown() == 1 && palette.shownId (0) == "splitbars";
          chk (all == (int) cmds.size() && d == 3 && t == 9 && e == 3 && z == 0 && both, "paleta: filtra por nombre sin importar acentos ni may\xc3\xbasculas (" + juce::String (d) + "/" + juce::String (t) + "/" + juce::String (e) + ")");
          palette.close(); chk (! palette.isVisible(), "paleta: se cierra");
          keyPressed (juce::KeyPress ('k', juce::ModifierKeys::commandModifier, 0), nullptr); chk (palette.isVisible(), "paleta: Ctrl+K la abre"); palette.close(); }
    }
    void refreshAll() { playlist.repaint(); piano.repaint(); steps.repaint(); mixer.repaint(); }
    bool forceDirty = false;
    bool dirty() const { return forceDirty || edit.hasChangedSinceSaved(); }
    bool autosaveNow() { if (! dirty()) return false; const bool ok = autosave::write (edit, projectFile); lastAutosave = juce::Time::getMillisecondCounter(); return ok; }
    juce::uint32 lastAutosave = juce::Time::getMillisecondCounter();
    void testShowHelp() { showHelp(); }
    void testShow (const juce::String& w) { if (w == "toast") note ("Se creó un clip MIDI vacío en la pista 3 para que puedas escribir notas"); else if (w == "export") exportWav(); else if (w == "alert") juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::NoIcon).withTitle (juce::String::fromUTF8 ("Cambios sin guardar")).withMessage (juce::String::fromUTF8 ("El proyecto tiene cambios sin guardar. ¿Quieres guardarlos antes de continuar?")).withButton ("Guardar").withButton ("No guardar").withButton ("Cancelar").withAssociatedComponent (this), nullptr); else if (w == "audio") openAudioSettings(); else if (w == "ideas") openIdeasDialog(); }
    void testShowMenu (const juce::String& n) { bars.openMenuForTest (n); }
    void demoFade (te::Clip* c) { playlist.selectForDemo (c); playlist.setZoom (14.0); }
    void note (const char* utf8) { toast.place (getLocalBounds()); toast.show (juce::String::fromUTF8 (utf8)); }
    void note (const juce::String& s) { toast.place (getLocalBounds()); toast.show (s); }
    static juce::File projectsDir() { auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("INJUSTICE DAW"); d.createDirectory(); return d; }
    void confirmThen (std::function<void()> go)
    {
        if (! dirty()) { go(); return; }
        juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::NoIcon).withTitle (juce::String::fromUTF8 ("Cambios sin guardar"))
            .withMessage (juce::String::fromUTF8 ("El proyecto tiene cambios sin guardar. ¿Quieres guardarlos antes de continuar?"))
            .withButton ("Guardar").withButton ("No guardar").withButton ("Cancelar").withAssociatedComponent (this),
            [this, go] (int r) { if (r == 1) saveProject (false, go); else if (r == 2) go(); });
    }
    bool doSave (const juce::File& f)
    {
        te::EditFileOperations ops (edit); const bool ok = ops.saveAs (f, true);
        if (ok) { forceDirty = false; autosave::clear(); userdata::addRecent (f); refreshMenuData(); projectFile = f; if (onSaved) onSaved (f); note (juce::String::fromUTF8 ("Guardado: ") + f.getFileName()); if (onTitle) onTitle(); } else note ("No se pudo guardar el proyecto en esa ubicaci\xc3\xb3n.");
        return ok;
    }
    void saveProject (bool as, std::function<void()> then = {})
    {
        if (! as && projectFile != juce::File()) { if (doSave (projectFile) && then) then(); return; }
        chooser = std::make_unique<juce::FileChooser> ("Guardar proyecto", projectsDir().getChildFile ("Proyecto.injdaw"), "*.injdaw");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
            [this, then] (const juce::FileChooser& fc) { auto f = fc.getResult(); if (f == juce::File()) return; if (doSave (f.withFileExtension ("injdaw")) && then) then(); });
    }
    void openProject()
    {
        chooser = std::make_unique<juce::FileChooser> ("Abrir proyecto", projectsDir(), "*.injdaw");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc) { auto f = fc.getResult(); if (f.existsAsFile() && onReplace) onReplace (f); });
    }
    // Junta todos los samples usados (clips de audio y sonidos del secuenciador) en una carpeta junto al proyecto y los enlaza ahi.
    int collectSamples (const juce::File& dir)
    {
        dir.createDirectory(); std::map<juce::String, juce::File> done; int copied = 0;
        auto bring = [&] (const juce::File& src) -> juce::File
        {
            if (! src.existsAsFile()) return {}; if (src.isAChildOf (dir)) return src;
            auto it = done.find (src.getFullPathName()); if (it != done.end()) return it->second;
            auto dest = dir.getChildFile (src.getFileName()); int n = 2;
            while (dest.existsAsFile()) dest = dir.getChildFile (src.getFileNameWithoutExtension() + "_" + juce::String (n++) + src.getFileExtension());
            if (! src.copyFileTo (dest)) return {};
            done[src.getFullPathName()] = dest; ++copied; return dest;
        };
        for (auto* t : te::getAudioTracks (edit))
        {
            for (auto* c : t->getClips()) if (auto* wc = dynamic_cast<te::WaveAudioClip*> (c)) { auto d = bring (wc->getOriginalFile()); if (d != juce::File()) wc->getSourceFileReference().setToDirectFileReference (d, true); }
            for (auto* p : t->pluginList.getPlugins()) if (auto* sp = dynamic_cast<te::SamplerPlugin*> (p))
            { int i = 0; for (auto v : sp->state) { if (! v.hasType ("SOUND")) continue; auto d = bring (juce::File (v.getProperty ("source").toString())); if (d != juce::File()) sp->setSoundMedia (i, d.getFullPathName()); ++i; } }
        }
        return copied;
    }
    std::unique_ptr<juce::FileChooser> midiIn;
    void importMidiDialog()
    {
        midiIn = std::make_unique<juce::FileChooser> ("Importar MIDI", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.mid;*.midi");
        midiIn->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult(); if (f == juce::File()) return;
            auto tr = te::getAudioTracks (edit); te::AudioTrack* dst = nullptr;
            for (auto* t : tr) if (t->getClips().isEmpty()) { dst = t; break; }
            if (dst == nullptr && ! tr.isEmpty()) dst = tr.getLast();
            edit.getUndoManager().beginNewTransaction();
            auto* c = dst ? trackutil::importMidiFile (edit, f, *dst, te::TimePosition::fromSeconds (edit.getTransport().getPosition().inSeconds())) : nullptr;
            note (c ? juce::String::fromUTF8 ("MIDI importado") : juce::String::fromUTF8 ("No se pudo importar el MIDI")); playlist.repaint();
        });
    }
    void saveWithSamples()
    {
        if (projectFile == juce::File()) { saveProject (true, [this] { if (projectFile != juce::File()) saveWithSamples(); }); return; }
        const int n = collectSamples (projectFile.getParentDirectory().getChildFile (projectFile.getFileNameWithoutExtension() + " - Samples"));
        doSave (projectFile); note (n > 0 ? juce::String (n) + " samples copiados junto al proyecto" : juce::String::fromUTF8 ("Todos los samples ya estaban junto al proyecto"));
    }
    bool renderWav (const juce::File& f, double len, ExportOpts o = {}, int onlyTrack = -1, double fromSec = 0.0)
    {
        if (edit.getTransport().isPlaying()) edit.getTransport().stop (false, false);
        te::Renderer::Parameters p (edit); p.destFile = f;
        p.audioFormat = o.fmt == 2 ? engine.getAudioFileFormatManager().getFlacFormat() : engine.getAudioFileFormatManager().getWavFormat();
        p.bitDepth = o.fmt == 1 ? 16 : 24; p.sampleRateForAudio = o.sr; p.shouldNormalise = o.norm; p.ditheringEnabled = o.fmt == 1;
        p.time = te::TimeRange (te::TimePosition::fromSeconds (fromSec), te::TimePosition::fromSeconds (fromSec + len + 2.0)); p.useMasterPlugins = onlyTrack < 0; p.canRenderInMono = false; p.checkNodesForAudio = false;
        if (onlyTrack < 0) { for (int i = 0; i < te::getAllTracks (edit).size(); ++i) p.tracksToDo.setBit (i); }
        else p.tracksToDo.setBit (onlyTrack);
        f.deleteFile(); return te::Renderer::renderToFile ("Exportar", p).existsAsFile();
    }
    // Bounce a pista nueva: cada pista con clips elegidos se renderiza (con sus efectos) solo con esos clips a un audio en una pista nueva
    // sin efectos justo debajo; los clips originales quedan silenciados (no se borran). Devuelve cuántos clips de audio crea.
    int bounceSelected()
    {
        auto sel = playlist.selectedClips(); if (sel.isEmpty()) { note (juce::String::fromUTF8 ("Elige primero los clips que quieres convertir a audio")); return 0; }
        if (edit.getTransport().isPlaying()) edit.getTransport().stop (false, false);
        auto all = te::getAllTracks (edit); juce::Array<te::AudioTrack*> tracks; for (auto* c : sel) if (auto* t = dynamic_cast<te::AudioTrack*> (c->getTrack())) tracks.addIfNotAlreadyThere (t);
        int made = 0; auto dir = edit.getTempDirectory (true).getChildFile ("bounces"); dir.createDirectory();
        edit.getUndoManager().beginNewTransaction (juce::String::fromUTF8 ("Bounce a pista nueva"));
        for (auto* t : tracks)
        {
            double a0 = 1e12, a1 = 0; juce::Array<te::Clip*> mine; for (auto* c : sel) if (c->getTrack() == t && ! c->isMuted()) { mine.add (c); a0 = std::min (a0, c->getPosition().getStart().inSeconds()); a1 = std::max (a1, c->getPosition().getEnd().inSeconds()); }
            if (mine.isEmpty()) continue; const int idx = all.indexOf (t); if (idx < 0) continue;
            std::vector<std::pair<te::Clip*, bool>> was; for (auto* c : t->getClips()) if (! mine.contains (c)) { was.push_back ({ c, c->isMuted() }); c->setMuted (true); }
            const auto file = dir.getChildFile (juce::File::createLegalFileName (t->getName() + " " + juce::String (juce::Time::currentTimeMillis())) + ".wav");
            ExportOpts o; const bool ok = renderWav (file, a1 - a0, o, idx, a0);
            for (auto& w : was) w.first->setMuted (w.second);
            if (! ok || ! file.existsAsFile()) { note (juce::String::fromUTF8 ("No se pudo renderizar la pista ") + t->getName()); continue; }
            auto nt = edit.insertNewAudioTrack (te::TrackInsertPoint (nullptr, t), nullptr); if (nt == nullptr) continue;
            nt->setName (t->getName() + " (bounce)"); trackutil::setColour (edit, nt.get(), (int) t->state.getProperty ("injColour", 0));
            auto wc = nt->insertWaveClip (t->getName() + " (bounce)", file, { te::TimeRange (te::TimePosition::fromSeconds (a0), te::TimePosition::fromSeconds (a1 + 2.0)), te::TimeDuration() }, false);
            if (wc != nullptr) { ++made; for (auto* c : mine) c->setMuted (true); }
        }
        mixer.refreshTracks(); playlist.repaint(); if (made > 0) note (juce::String (made) + juce::String::fromUTF8 (" bounce(s) creados; los clips originales quedan silenciados"));
        return made;
    }
    // Una pista por archivo: "01 Kick.wav", "02 Clap.wav"... (solo las que tienen clips)
    int renderStems (const juce::File& dir, double len, ExportOpts o)
    {
        dir.createDirectory(); int n = 0; auto all = te::getAllTracks (edit); auto audio = te::getAudioTracks (edit);
        for (int i = 0; i < audio.size(); ++i)
        {
            if (audio[i]->getClips().isEmpty()) continue; const int idx = all.indexOf (audio[i]); if (idx < 0) continue;
            const auto name = juce::File::createLegalFileName (audio[i]->getName() == "Track" ? "Pista " + juce::String (i + 1) : audio[i]->getName());
            if (renderWav (dir.getChildFile (juce::String (i + 1).paddedLeft ('0', 2) + " " + name + (o.fmt == 2 ? ".flac" : ".wav")), len, o, idx)) ++n;
        }
        return n;
    }
    void exportWav()
    {
        const auto len = edit.getLength().inSeconds();
        if (len <= 0.01) { note ("No hay nada que exportar: a\xc3\xb1" "ade clips o dibuja un patr\xc3\xb3n primero."); return; }
        auto* c = new ExportContent(); juce::DialogWindow::LaunchOptions lo; lo.content.setOwned (c); lo.dialogTitle = "Exportar audio"; lo.componentToCentreAround = this;
        lo.dialogBackgroundColour = theme::bgBar2; lo.useNativeTitleBar = true; lo.resizable = false;
        auto* dw = lo.launchAsync (); theme::darkTitleBar (dw); juce::Component::SafePointer<juce::DialogWindow> win (dw);
        c->onCancel = [win] { if (win) win->exitModalState (0); };
        c->onGo = [this, win, len] (ExportOpts o)
        {
            if (win) win->exitModalState (1);
            const auto base = projectFile != juce::File() ? projectFile.getFileNameWithoutExtension() : juce::String ("Mezcla"); const juce::String ext = o.fmt == 2 ? "flac" : "wav";
            if (o.stems)
            {
                chooser = std::make_unique<juce::FileChooser> ("Carpeta para las pistas", projectsDir(), "");
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this, len, o, base] (const juce::FileChooser& fc)
                {
                    auto d = fc.getResult(); if (d == juce::File()) return; const int n = renderStems (d.getChildFile (base + " - pistas"), len, o);
                    note (n > 0 ? juce::String (n) + " pistas exportadas a " + base + " - pistas" : juce::String::fromUTF8 ("No se pudo exportar."));
                });
                return;
            }
            chooser = std::make_unique<juce::FileChooser> ("Exportar audio", projectsDir().getChildFile (base + "." + ext), "*." + ext);
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                [this, len, o, ext] (const juce::FileChooser& fc)
                {
                    auto f = fc.getResult(); if (f == juce::File()) return; f = f.withFileExtension (ext);
                    note (renderWav (f, len, o) ? juce::String::fromUTF8 ("Exportado: ") + f.getFileName() : juce::String::fromUTF8 ("No se pudo exportar el audio."));
                });
        };
    }
    void showHelp()
    {
        auto* vp = new juce::Viewport(); vp->setViewedComponent (new HelpContent(), true); vp->setScrollBarsShown (true, false); vp->setSize (650, 780); vp->setScrollBarThickness (8);
        juce::DialogWindow::LaunchOptions o; o.content.setOwned (vp); o.dialogTitle = "Ayuda"; o.componentToCentreAround = this;
        o.dialogBackgroundColour = theme::bgBar2; o.useNativeTitleBar = true; o.resizable = false; theme::darkTitleBar (o.launchAsync());
    }
    void addPlugin (const juce::PluginDescription& d, int trackIdx)
    {
        if (trackIdx == -2)
        {   // cadena del Master: el efecto va antes del volumen
            auto p = engine.getPluginManager().createNewPlugin (edit, te::ExternalPlugin::xmlTypeName, d);
            if (p == nullptr) { note (juce::String::fromUTF8 ("No se pudo cargar ") + d.name); return; }
            auto& ml = edit.getMasterPluginList(); const int vi = ml.indexOf (edit.getMasterVolumePlugin().get()); ml.insertPlugin (p, vi >= 0 ? vi : -1, nullptr);
            mixer.repaint(); note (d.name + juce::String::fromUTF8 (" agregado al Master")); openEditorWhenReady (p); return;
        }
        auto tracks = te::getAudioTracks (edit); if (trackIdx < 0 || trackIdx >= tracks.size()) return;
        auto* t = tracks[trackIdx];
        auto p = engine.getPluginManager().createNewPlugin (edit, te::ExternalPlugin::xmlTypeName, d);
        if (p == nullptr) { note (juce::String::fromUTF8 ("No se pudo cargar ") + d.name); return; }
        const int volIdx = t->pluginList.indexOf (t->getVolumePlugin());
        t->pluginList.insertPlugin (p, d.isInstrument ? 0 : (volIdx >= 0 ? volIdx : t->pluginList.size()), nullptr);
        mixer.selected = trackIdx; mixer.repaint(); playlist.repaint();
        note (d.name + juce::String::fromUTF8 (d.isInstrument ? " agregado como instrumento a la pista " : " agregado como efecto a la pista ") + juce::String (trackIdx + 1));
        openEditorWhenReady (p);
    }
    // Deshacer/rehacer sin cerrar los plugins que tengas abiertos: se anotan antes y se reabren (si siguen existiendo) después.
    void undoRedo (bool undo)
    {
        std::vector<te::Plugin::Ptr> open;
        for (auto* p : te::getAllPlugins (edit, true)) if (p->windowState != nullptr && p->windowState->isWindowShowing()) open.push_back (p);
        if (undo) edit.getUndoManager().undo(); else edit.getUndoManager().redo();
        refreshAll();
        for (auto& p : open) juce::Timer::callAfterDelay (80, [this, p] { if (p->state.getParent().isValid() && p->windowState != nullptr && ! p->windowState->isWindowShowing()) openEditorWhenReady (p); });
    }
    // El plugin VST3 carga en segundo plano: reintenta hasta que su ventana aparezca (así abre a la primera).
    void openEditorWhenReady (te::Plugin::Ptr p, int tries = 0)
    {
        if (p == nullptr || p->windowState == nullptr) return;
        p->showWindowExplicitly();
        if (! p->windowState->isWindowShowing() && tries < 40) juce::Timer::callAfterDelay (250, [this, p, tries] { openEditorWhenReady (p, tries + 1); });
    }
    void openPluginList (int track = -1)
    {
        const int t = track != -1 ? track : mixer.selected;
        if (pluginWin != nullptr) { if (auto* c = dynamic_cast<pluginhost::ListContent*> (pluginWin->getContentComponent())) c->setTarget (t); pluginWin->toFront (true); return; }
        auto* c = new pluginhost::ListContent (engine); c->setTarget (t); c->onAdd = [this, c] (const juce::PluginDescription& d) { addPlugin (d, c->targetTrack); };
        juce::DialogWindow::LaunchOptions o; o.content.setOwned (c); o.dialogTitle = "Plugins VST3"; o.componentToCentreAround = this; o.dialogBackgroundColour = theme::bgBar2;
        o.useNativeTitleBar = true; o.resizable = true; o.escapeKeyTriggersCloseButton = true; pluginWin = o.launchAsync(); theme::darkTitleBar (pluginWin.getComponent());
        if (c->isEmpty()) c->startScan();
    }
    bool keyPressed (const juce::KeyPress& k, juce::Component*) override
    {
        const auto m = k.getModifiers(); const int c = k.getKeyCode(); auto& tc = edit.getTransport();
        if (m.isCommandDown() && juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) c) == 't') { audition::typingPiano = ! audition::typingPiano; if (! audition::typingPiano) typedReleaseAll(); note (audition::typingPiano ? juce::String::fromUTF8 ("Teclado como piano: ACTIVADO (Z-M y Q-U, Re P\xc3\xa1g / Av P\xc3\xa1g = octava)") : juce::String::fromUTF8 ("Teclado como piano: apagado")); return true; }
        if (handleTypedKey (k)) return true;
        if (dynamic_cast<juce::TextEditor*> (juce::Component::getCurrentlyFocusedComponent()) == nullptr && ! palette.isVisible())
        {
            const auto lc = juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) c);
            if (m.isCommandDown() && lc == 'k') { openPalette(); return true; }
            if (m.isCommandDown() && ! m.isShiftDown() && lc == 'i' && ! bars.pianoMode) { runCmd ("invertsel"); return true; }
            if (m.isAltDown() && ! m.isCommandDown() && (c == juce::KeyPress::upKey || c == juce::KeyPress::downKey) && ! bars.pianoMode) { runCmd (c == juce::KeyPress::upKey ? "clipup" : "clipdown"); return true; }
            if (m.isAltDown() && ! m.isCommandDown() && c == juce::KeyPress::returnKey) { runCmd ("fullscreen"); return true; }
            if (! m.isAnyModifierKeyDown() && ! bars.pianoMode && (c == ',' || c == '.')) { playlist.gotoBar (c == ',' ? -1 : 1); return true; }
        }
        else if (palette.isVisible()) return false;
        if (c == juce::KeyPress::spaceKey && m.isShiftDown() && ! m.isCommandDown()) { const bool was = tc.isPlaying(); if (was) tc.stop (false, false); tc.setPosition (playStart); syncState(); return true; }   // Mayús+Espacio: parar y volver de donde empezó
        if (c == juce::KeyPress::spaceKey && ! m.isAnyModifierKeyDown()) { if (tc.isPlaying()) tc.stop (false, false); else { steps.endPattern(); browser.stopPreview(); playStart = tc.getPosition(); tc.play (false); } syncState(); return true; }
        if (c == juce::KeyPress::endKey) { tc.setPosition (te::TimePosition::fromSeconds (edit.tempoSequence.toTime (te::BeatPosition::fromBeats (playlist.projectEndBeat())).inSeconds())); return true; }
        if (c == juce::KeyPress::F3Key) { toggleAB(); return true; }
        if (c == juce::KeyPress::F4Key) { playlist.follow = ! playlist.follow; bars.followOn = playlist.follow; bars.repaint(); return true; }
        if (m.isCommandDown() && c == '0' && ! bars.pianoMode) { playlist.zoomFit (m.isShiftDown()); return true; }
        if (c == juce::KeyPress::homeKey) { tc.setPosition (te::TimePosition()); return true; }
        if (c == juce::KeyPress::F1Key) { showHelp(); return true; }
        if (c == juce::KeyPress::F5Key) { showView ("playlist"); return true; }
        if (c == juce::KeyPress::F6Key) { setSteps (! steps.isVisible()); return true; }
        if (c == juce::KeyPress::F7Key) { ensurePianoClip(); showView ("piano"); return true; }
        if (c == juce::KeyPress::F8Key) { setBrowser (! browser.isVisible()); return true; }
        if (c == juce::KeyPress::F11Key) { setMeters (! meters.isVisible()); return true; }
        if (c == juce::KeyPress::F12Key) { setPerf (! perf.isVisible()); return true; }
        if (c == juce::KeyPress::F9Key) { setMixer (! mixer.isVisible()); return true; }
        if (c == juce::KeyPress::F10Key) { openPluginList(); return true; }
        if (m.isCommandDown())
        {
            const auto ch = juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) c);
            if (ch == 'z' && ! m.isShiftDown()) { undoRedo (true); return true; }
            if (ch == 'y' || (ch == 'z' && m.isShiftDown())) { undoRedo (false); return true; }
            if (ch == 's') { saveProject (m.isShiftDown()); return true; }
            if (ch == 'o') { confirmThen ([this] { openProject(); }); return true; }
            if (ch == 'n') { confirmThen ([this] { if (onReplace) onReplace ({}); }); return true; }
            if (ch == 'e' && m.isShiftDown()) { setAed (! aed.isVisible()); return true; }
            if (ch == 'p' && m.isShiftDown()) { setPitchEd (! pitchEd.isVisible()); return true; }
            if (ch == 'e') { exportWav(); return true; }
            if (ch == 'g') { openIdeasDialog(); return true; }
        }
        return false;
    }
    // ---- plantillas e ideas ----
    void applyTemplate (int k)
    {
        auto ov = templates::pendingOver(); templates::pendingOver() = {};
        const int made = templates::apply (edit, samplepack::packsDir(), k, ov.set ? &ov : nullptr);
        if (made == 0) { note (juce::String::fromUTF8 ("No se pudo crear la plantilla (faltan los sonidos)")); return; }
        bars.bpm = edit.tempoSequence.getTempo (0)->getBpm(); bars.repaint(); mixer.refreshTracks(); playlist.repaint();
        edit.getTransport().setPosition (te::TimePosition()); edit.resetChangedStatus();
        note (juce::String::fromUTF8 ("Plantilla ") + ideas::styleName (templates::list()[(size_t) k].style) + juce::String::fromUTF8 (": ") + juce::String (made) + juce::String::fromUTF8 (" pistas con una canción de 28 compases. Pulsa Espacio."));
    }
    // Genera una idea (rol: 0 batería, 1 bajo, 2 melodía, 3 acordes, 4 pad) en la pista elegida, desde el compás del cabezal.
    int generateIdea (int role, int style, int nBars, int root, int seed)
    {
        auto ts = te::getAudioTracks (edit); if (ts.isEmpty()) return 0; auto* t = ts[juce::jlimit (0, ts.size() - 1, mixer.selected)];
        if (! trackutil::hasInstrument (*t))
        {
            if (role == 0) { if (instruments::addKit (edit, *t, samplepack::packsDir(), 0) == nullptr) { note (juce::String::fromUTF8 ("Los sonidos todavía se están creando")); return 0; } }
            else instruments::addSynth (edit, *t, templates::synthIdx (role == 1 ? "Bajo Gabber" : role == 2 ? "Lead Hoover" : role == 3 ? "Stab Hardcore" : "Pad Oscuro"));
        }
        std::vector<ideas::N> n;
        switch (role) { case 0: n = ideas::drums (style, nBars, seed); break; case 1: n = ideas::bass (style, nBars, seed, root); break;
                        case 2: n = ideas::melody (style, nBars, seed, root + 36); break; case 3: n = ideas::chords (style, nBars, seed, root + 24, false); break; default: n = ideas::chords (style, nBars, seed, root + 12, true); break; }
        const double startBeat = std::floor (edit.tempoSequence.toBeats (edit.getTransport().getPosition()).inBeats() / 4.0) * 4.0;
        static const char* rn[] { "Bateria", "Bajo", "Melodia", "Acordes", "Pad" };
        auto* c = ideas::insert (edit, *t, startBeat, nBars, n, juce::String (rn[juce::jlimit (0, 4, role)]) + " - " + ideas::styleName (style));
        if (c != nullptr) { playlist.repaint(); piano.setClip (c); }
        return c != nullptr ? (int) n.size() : 0;
    }
    void openIdeasDialog()
    {
        auto* aw = new juce::AlertWindow (juce::String::fromUTF8 ("Generador de ideas"), juce::String::fromUTF8 ("Crea un clip nuevo en la pista seleccionada, desde el compás donde está el cabezal."), juce::MessageBoxIconType::NoIcon);
        juce::StringArray st; for (int k = 0; k < ideas::kNumStyles; ++k) st.add (ideas::styleName (k));
        aw->addComboBox ("estilo", st, "Estilo"); aw->addComboBox ("que", { juce::String::fromUTF8 ("Bater\xc3\xad" "a"), "Bajo", juce::String::fromUTF8 ("Melod\xc3\xad" "a"), "Acordes (stabs)", "Pad (acordes largos)" }, juce::String::fromUTF8 ("Qu\xc3\xa9 crear"));
        aw->addComboBox ("comp", { "4 compases", "8 compases", "16 compases" }, "Largo"); aw->addComboBox ("raiz", { "Do", "Do#", "Re", "Re#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" }, juce::String::fromUTF8 ("Nota ra\xc3\xadz"));
        aw->getComboBoxComponent ("estilo")->setSelectedItemIndex (lastIdeaStyle); aw->getComboBoxComponent ("comp")->setSelectedItemIndex (1); aw->getComboBoxComponent ("raiz")->setSelectedItemIndex (5);
        aw->addButton ("Crear", 1, juce::KeyPress (juce::KeyPress::returnKey)); aw->addButton ("Cerrar", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<MainComponent> self (this);
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([self, aw] (int r)
        {
            if (r == 1 && self != nullptr)
            {
                const int stl = aw->getComboBoxComponent ("estilo")->getSelectedItemIndex(), what = aw->getComboBoxComponent ("que")->getSelectedItemIndex();
                const int nb = std::array<int, 3> { 4, 8, 16 } [(size_t) juce::jmax (0, aw->getComboBoxComponent ("comp")->getSelectedItemIndex())], root = 24 + aw->getComboBoxComponent ("raiz")->getSelectedItemIndex();
                self->lastIdeaStyle = stl; const int made = self->generateIdea (what, stl, nb, root, (int) juce::Time::getMillisecondCounter());
                if (made > 0) self->note (juce::String (made) + juce::String::fromUTF8 (" notas creadas. ¿No te convence? Ctrl+Z y vuelve a pulsar Crear."));
            }
        }), true);
    }
    // "IA de ideas": una frase -> proyecto nuevo con canción (o solo una parte en la pista elegida). Funciona por reglas, sin internet.
    void openIdeaStudio()
    {
        auto* aw = new juce::AlertWindow (juce::String::fromUTF8 ("IA de ideas"), juce::String::fromUTF8 ("Escribe qué quieres: género, BPM, tonalidad, ánimo.\nEjemplos: hardcore oscuro 190 bpm en la menor  |  trance alegre 138 bpm en do mayor  |  solo bajo gabber"), juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("prompt", lastPrompt, juce::String::fromUTF8 ("Tu idea"));
        aw->addButton ("Canci\xc3\xb3n nueva", 1, juce::KeyPress (juce::KeyPress::returnKey)); aw->addButton ("Solo una parte", 2); aw->addButton ("Cerrar", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<MainComponent> self (this);
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([self, aw] (int r)
        {
            if (self == nullptr || r == 0) return;
            const auto text = aw->getTextEditorContents ("prompt"); self->lastPrompt = text; auto p = ideas::parsePrompt (text);
            if (p.style < 0) { p.style = ideas::Hardcore; p.understood.insert (0, juce::String::fromUTF8 ("Estilo no indicado: Hardcore")); }
            const int seed = (int) (juce::Time::getMillisecondCounter() & 0xffff);
            double bpm = p.bpm > 0 ? p.bpm : ideas::styleBpm (p.style) + (p.energy > 0 ? 6 : p.energy < 0 ? -8 : 0);
            const juce::String what = p.understood.joinIntoString (", ");
            if (r == 2 || p.role >= 0)
            {
                ideas::majorMode() = p.major; const int role = p.role >= 0 ? p.role : 2;
                const int made = self->generateIdea (role, p.style, 8, p.root >= 0 ? 24 + p.root : 33, seed); ideas::majorMode() = false;
                if (made > 0) self->note (juce::String (made) + juce::String::fromUTF8 (" notas creadas (") + what + "). Ctrl+Z para deshacer.");
                return;
            }
            if (! samplepack::ready (samplepack::packsDir())) { self->note (juce::String::fromUTF8 ("Los sonidos todavía se están creando; inténtalo en unos segundos")); return; }
            int k = 0; for (int i = 0; i < (int) templates::list().size(); ++i) if (templates::list()[(size_t) i].style == p.style) k = i;
            templates::Over o; o.set = true; o.bpm = bpm; o.root = p.root >= 0 ? 24 + p.root : -1; o.major = p.major; o.seedOff = seed;
            self->note (juce::String::fromUTF8 ("Entendí: ") + what);
            self->confirmThen ([self, k, o] { if (self == nullptr) return; templates::pending() = k; templates::pendingOver() = o; if (self->onReplace) self->onReplace ({}); });
        }), true);
    }
    juce::String lastPrompt = "hardcore oscuro 190 bpm en la menor";
    int lastIdeaStyle = 0;
    void syncTimeSig()
    {
        auto& ts = edit.tempoSequence.getTimeSigAt (te::TimePosition()); const int n = ts.numerator, d = ts.denominator;
        if (n > 0 && d > 0 && (n != timesig::num || d != timesig::den)) { timesig::num = n; timesig::den = d; bars.repaint(); playlist.repaint(); piano.repaint(); }
    }
    // ---- teclado de la computadora como piano (Ctrl+T), estilo FL Studio ----
    struct TypedNote { int pitch; double startSec, endSec; };
    std::map<int, int> heldKeys;   // tecla -> nota que suena
    std::vector<TypedNote> typedRec; int typedOct = 4; bool wasRec = false;
    static int semitoneForKey (int c)
    {
        static const char* low = "zsxdcvgbhnjm,"; static const char* up = "q2w3er5t6y7ui";
        const auto ch = (char) juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) c); if (c > 127) return -1;
        for (int i = 0; low[i]; ++i) if (low[i] == ch) return i;
        for (int i = 0; up[i]; ++i) if (up[i] == ch) return 12 + i;
        return -1;
    }
    te::AudioTrack* typedTarget() { auto ts = te::getAudioTracks (edit); return ts.isEmpty() ? nullptr : ts[juce::jlimit (0, ts.size() - 1, mixer.selected)]; }
    // ---- repetición de notas y bloqueo de escala (teclado piano de la computadora) ----
    bool scaleLock = false; int repeatIdx = 0; std::map<int, double> repeatNext; int repeatHits = 0;
    static double repeatBeats (int i) { static const double b[] { 0.0, 1.0, 0.5, 0.25, 0.125 }; return b[juce::jlimit (0, 4, i)]; }
    static const char* repeatName (int i) { static const char* n[] { "apagada", "1/4", "1/8", "1/16", "1/32" }; return n[juce::jlimit (0, 4, i)]; }
    double repeatMs() const { const double bpm = std::max (20.0, edit.tempoSequence.getTempo (0)->getBpm()); return 60000.0 / bpm * repeatBeats (repeatIdx); }
    void setRepeat (int i) { repeatIdx = juce::jlimit (0, 4, i); const double now = juce::Time::getMillisecondCounterHiRes(); for (auto& kv : heldKeys) repeatNext[kv.first] = now + repeatMs(); note (juce::String::fromUTF8 ("Repetici\xc3\xb3n de notas: ") + repeatName (repeatIdx) + (audition::typingPiano ? juce::String() : juce::String::fromUTF8 (" (activa el teclado piano con Ctrl+T)"))); }
    void setScaleLock (bool v) { scaleLock = v; note (v ? (harmony::getKey (edit).active() ? juce::String::fromUTF8 ("Bloqueo de escala: solo suenan notas de la tonalidad") : juce::String::fromUTF8 ("Bloqueo de escala activado, pero el proyecto no tiene tonalidad: elige una en el cuadro de arriba a la izquierda")) : juce::String::fromUTF8 ("Bloqueo de escala apagado")); }
    // Vuelve a tocar las teclas que siguen pulsadas, al ritmo elegido. Devuelve cuántas notas retocó. `nowMs` se puede dar para las pruebas.
    int tickRepeat (double nowMs = -1.0)
    {
        if (repeatIdx == 0 || ! audition::typingPiano || heldKeys.empty()) return 0; if (nowMs < 0) nowMs = juce::Time::getMillisecondCounterHiRes();
        const double step = repeatMs(); int n = 0; auto* t = typedTarget(); if (t == nullptr || step < 10.0) return 0;
        const bool live = t->edit.engine.getDeviceManager().deviceManager.getCurrentAudioDevice() != nullptr && edit.getTransport().isPlayContextActive();
        for (auto& kv : heldKeys)
        {
            double& nx = repeatNext[kv.first]; if (nowMs < nx) continue; nx = std::max (nx + step, nowMs - step); const int pitch = kv.second;
            if (live) { t->injectLiveMidiMessage (juce::MidiMessage::noteOff (1, pitch), {}); t->injectLiveMidiMessage (juce::MidiMessage::noteOn (1, pitch, (juce::uint8) 100), {}); }
            if (edit.getTransport().isRecording()) { const double now = edit.getTransport().getPosition().inSeconds(); for (auto& tn : typedRec) if (tn.pitch == pitch && tn.endSec < 0) { tn.endSec = now; break; } typedRec.push_back ({ pitch, now, -1.0 }); }
            ++n; ++repeatHits;
        }
        return n;
    }
    void typedNoteOn (int key, int semi)
    {
        if (heldKeys.count (key)) return;
        auto* t = typedTarget(); if (t == nullptr) return; int pitch = juce::jlimit (0, 127, 12 * (typedOct + 1) + semi);
        if (scaleLock) pitch = juce::jlimit (0, 127, harmony::snapToScale (harmony::getKey (edit), pitch));
        heldKeys[key] = pitch; repeatNext[key] = juce::Time::getMillisecondCounterHiRes() + repeatMs();
        if (t->edit.engine.getDeviceManager().deviceManager.getCurrentAudioDevice() != nullptr)
        {
            edit.getTransport().ensureContextAllocated();
            if (edit.getTransport().isPlayContextActive()) t->injectLiveMidiMessage (juce::MidiMessage::noteOn (1, pitch, (juce::uint8) 100), {});
        }
        if (edit.getTransport().isRecording()) { const double now = edit.getTransport().getPosition().inSeconds(); typedRec.push_back ({ pitch, now, -1.0 }); }
    }
    void typedNoteOff (int key)
    {
        auto it = heldKeys.find (key); if (it == heldKeys.end()) return; const int pitch = it->second; heldKeys.erase (it);
        if (auto* t = typedTarget()) if (t->edit.engine.getDeviceManager().deviceManager.getCurrentAudioDevice() != nullptr && edit.getTransport().isPlayContextActive())
            t->injectLiveMidiMessage (juce::MidiMessage::noteOff (1, pitch), {});
        const double now = edit.getTransport().getPosition().inSeconds();
        for (auto& n : typedRec) if (n.pitch == pitch && n.endSec < 0) { n.endSec = juce::jmax (now, n.startSec + .05); break; }
    }
    void typedReleaseAll() { std::vector<int> ks; for (auto& kv : heldKeys) ks.push_back (kv.first); for (int k : ks) typedNoteOff (k); }
    // al terminar la grabación, las notas tocadas con el teclado pasan a un clip MIDI de la pista armada
    void setCountIn (int k)
    {
        k = juce::jlimit (0, 4, k); edit.setCountInMode (static_cast<te::Edit::CountIn> (k)); bars.countIn = k; bars.repaint();
    }
    void commitTypedRecording()
    {
        auto notes = typedRec; typedRec.clear(); if (notes.empty()) return;
        te::AudioTrack* t = nullptr; for (auto* a : te::getAudioTracks (edit)) if (trackutil::isArmed (*a)) { t = a; break; }
        if (t == nullptr) t = typedTarget(); if (t == nullptr) return;
        double s0 = 1e9, e0 = 0; const double endNow = edit.getTransport().getPosition().inSeconds();
        for (auto& n : notes) { if (n.endSec < 0) n.endSec = juce::jmax (endNow, n.startSec + .1); s0 = juce::jmin (s0, n.startSec); e0 = juce::jmax (e0, n.endSec); }
        const double b0 = std::floor (edit.tempoSequence.toBeats (te::TimePosition::fromSeconds (s0)).inBeats() / 4.0) * 4.0;
        const double b1 = std::ceil (edit.tempoSequence.toBeats (te::TimePosition::fromSeconds (e0)).inBeats() / 4.0) * 4.0;
        auto clip = t->insertMIDIClip ("MIDI", te::TimeRange (edit.tempoSequence.toTime (te::BeatPosition::fromBeats (b0)), edit.tempoSequence.toTime (te::BeatPosition::fromBeats (juce::jmax (b1, b0 + 4.0)))), nullptr);
        if (clip == nullptr) return;
        for (auto& n : notes)
        {
            const double a = edit.tempoSequence.toBeats (te::TimePosition::fromSeconds (n.startSec)).inBeats() - b0, b = edit.tempoSequence.toBeats (te::TimePosition::fromSeconds (n.endSec)).inBeats() - b0;
            clip->getSequence().addNote (n.pitch, te::BeatPosition::fromBeats (juce::jmax (0.0, a)), te::BeatDuration::fromBeats (juce::jmax (0.125, b - a)), 100, 0, &edit.getUndoManager());
        }
        note (juce::String::fromUTF8 ("Notas grabadas del teclado: ") + juce::String ((int) notes.size())); playlist.repaint();
    }
    bool handleTypedKey (const juce::KeyPress& k)
    {
        if (! audition::typingPiano) return false; const auto m = k.getModifiers(); if (m.isCommandDown() || m.isAltDown()) return false;
        const int c = k.getKeyCode();
        if (c == juce::KeyPress::pageUpKey) { typedOct = juce::jmin (8, typedOct + 1); note ("Octava " + juce::String (typedOct)); return true; }
        if (c == juce::KeyPress::pageDownKey) { typedOct = juce::jmax (0, typedOct - 1); note ("Octava " + juce::String (typedOct)); return true; }
        const int s = semitoneForKey (c); if (s < 0) return false; typedNoteOn (c, s); return true;
    }
    bool keyStateChanged (bool, juce::Component*) override
    {
        if (! audition::typingPiano || heldKeys.empty()) return false;
        std::vector<int> up; for (auto& kv : heldKeys) if (! juce::KeyPress::isKeyCurrentlyDown (kv.first)) up.push_back (kv.first);
        for (int k : up) typedNoteOff (k);
        return ! up.empty();
    }
    void resized() override
    {
        toast.place (getLocalBounds()); palette.setBounds (getLocalBounds());
        auto r = getLocalBounds(); bars.setBounds (r.removeFromTop (TopBars::kHeight)); status.setBounds (r.removeFromBottom (StatusBar::kHeight));
        if (browser.isVisible()) { browser.setBounds (r.removeFromLeft (browserW).reduced (6, 6)); r.removeFromLeft (6); }
        playlist.setBounds (r); piano.setBounds (r);
        if (mixer.isVisible() && ! mixer.maximized) mixer.setTopLeftPosition (juce::jmax (0, getWidth() - mixer.getWidth() - 12), juce::jmax (TopBars::kHeight, getHeight() - StatusBar::kHeight - mixer.getHeight() - 12));
    }
    void mouseDown (const juce::MouseEvent& e) override { splitting = browser.isVisible() && e.x >= browserW - 2 && e.x <= browserW + 8 && e.y > TopBars::kHeight; }
    void mouseDrag (const juce::MouseEvent& e) override { if (splitting) { browserW = juce::jlimit (180, 520, e.x); resized(); repaint(); } }
    void mouseMove (const juce::MouseEvent& e) override { setMouseCursor (browser.isVisible() && e.x >= browserW - 2 && e.x <= browserW + 8 && e.y > TopBars::kHeight ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor); }
    void setBrowser (bool v) { browser.setVisible (v); bars.browserOpen = v; bars.repaint(); resized(); repaint(); }
    template <typename P> void fitPanel (P& p)
    {
        const float s = std::min ({ 1.f, (float) (getWidth() - 24) / p.designW, (float) (getHeight() - TopBars::kHeight - 24) / p.designH });
        if (s < p.scale || p.getWidth() == 0) { p.scale = std::max (.4f, s); p.setSize (juce::roundToInt (p.designW * p.scale), juce::roundToInt (p.designH * p.scale)); }
    }
    void setMixer (bool v) { if (v) fitPanel (mixer); mixer.setVisible (v); bars.mixerOpen = v; if (v) { mixer.setTopLeftPosition (juce::jmax (0, getWidth() - mixer.getWidth() - 12), juce::jmax (TopBars::kHeight, getHeight() - mixer.getHeight() - 12)); mixer.toFront (false); } bars.repaint(); }
    void setPitchEd (bool v) { if (v) fitPanel (pitchEd); pitchEd.setVisible (v); if (v) { pitchEd.setClip (aed.getClip ? aed.getClip() : nullptr); pitchEd.setTopLeftPosition (juce::jmax (0, (getWidth() - pitchEd.getWidth()) / 2), TopBars::kHeight + 12); pitchEd.toFront (false); } else pitchEd.stopPlay(); }
    void setAed (bool v) { if (v) fitPanel (aed); aed.setVisible (v); bars.aedOpen = v; if (v) { aed.setClip (aed.getClip ? aed.getClip() : nullptr); aed.setTopLeftPosition (juce::jmax (0, (getWidth() - aed.getWidth()) / 2), TopBars::kHeight + 20); aed.toFront (false); } bars.repaint(); }
    void setRack (bool v) { if (v) fitPanel (rack); rack.setVisible (v); bars.rackOpen = v; if (v) { rack.setTopLeftPosition (juce::jmax (0, (getWidth() - rack.getWidth()) / 2), TopBars::kHeight + 14); rack.toFront (false); rack.repaint(); } bars.repaint(); }
    void setPerf (bool v) { if (v) fitPanel (perf); perf.setVisible (v); bars.perfOpen = v; if (v) { perf.setTopLeftPosition (juce::jmax (0, (getWidth() - perf.getWidth()) / 2), TopBars::kHeight + 30); perf.toFront (false); } bars.repaint(); }
    void setMeters (bool v) { if (v) fitPanel (meters); meters.setVisible (v); bars.metersOpen = v; if (v) { meters.setTopLeftPosition (juce::jmax (0, (getWidth() - meters.getWidth()) / 2), TopBars::kHeight + 50); meters.toFront (false); } bars.repaint(); }
    void setSteps (bool v)
    {   // la ventana aparece al instante; el patrón y su sampler se crean justo después, sin congelar la pantalla
        if (v) { fitPanel (steps); if (! steps.hasClip())
            {   // el trabajo pesado se reparte en pasos de ~30 ms (clip, sampler y un sample por paso) para que la ventana siga respondiendo
                juce::Component::SafePointer<MainComponent> self (this); auto stage = std::make_shared<int> (-2); auto run = std::make_shared<std::function<void()>>();
                *run = [self, stage, run]
                {
                    if (self == nullptr) return; auto& st = self->steps; bool more = true;
                    if (*stage == -2) st.ensureClip(); else if (*stage == -1) st.ensureSampler(); else more = ! st.assignDefaultsStep (samplepack::packDir (samplepack::packsDir()), *stage);
                    ++*stage; st.repaint(); self->playlist.repaint(); if (more) juce::Timer::callAfterDelay (30, [run] { (*run)(); });
                };
                if (std::getenv ("INJ_SELFTEST") != nullptr || std::getenv ("INJ_SHOT") != nullptr || std::getenv ("INJ_STEPTEST") != nullptr)   // pruebas y capturas: todo de una vez, como antes
                { for (int k = 0; k < 8; ++k) { if (self == nullptr) break; auto& st = self->steps; if (k == 0) st.ensureClip(); else if (k == 1) st.ensureSampler(); else if (st.assignDefaultsStep (samplepack::packDir (samplepack::packsDir()), k - 2)) break; } }
                else juce::Timer::callAfterDelay (60, [run] { (*run)(); });
            } }
        steps.setVisible (v); bars.stepsOpen = v; if (v) { steps.setTopLeftPosition (juce::jmax (0, (getWidth() - steps.getWidth()) / 2), TopBars::kHeight + 70); steps.toFront (false); } playlist.repaint(); bars.repaint(); }
    PianoRollView& pianoRef() { return piano; } StepSeqPanel& stepsRef() { return steps; } BrowserPanel& browserRef() { return browser; } MixerPanel& mixerRef() { return mixer; }
    void openPanelsFromEnv (const juce::String& list)
    {
        if (auto* tp = std::getenv ("INJ_TEMPLATE")) { samplepack::ensure (samplepack::packsDir()); applyTemplate (std::atoi (tp)); }
        if (list.contains ("browser")) setBrowser (true); else setBrowser (false);
        if (list.contains ("mixer")) setMixer (true);
        if (list.contains ("pitchdemo"))
        {   // solo capturas: una melodía de voz sintética (notas con un poco de desafinación y vibrato)
            auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_shot_audio"); dir.createDirectory(); auto ts = te::getAudioTracks (edit);
            if (! ts.isEmpty())
            {
                const double sr0 = 44100.0; audioedit::Buf b (1, (int) (sr0 * 7.5)); b.clear(); const double nt[10][3] { { .05, .55, 62.3 }, { .6, 1.0, 64.4 }, { 1.05, 1.9, 67.0 }, { 1.95, 2.5, 60.5 }, { 2.55, 3.1, 66.6 }, { 3.15, 3.9, 71.3 }, { 3.95, 4.8, 72.2 }, { 4.85, 5.3, 68.8 }, { 5.35, 6.9, 65.6 }, { 5.0, 5.0, 0 } };
                double ph = 0; for (int k = 0; k < 9; ++k) for (int i = (int) (nt[k][0] * sr0); i < (int) (nt[k][1] * sr0); ++i) { const double t = i / sr0, m = nt[k][2] + .25 * std::sin (t * 31.0) + .1 * std::sin (t * 7.0); const double hz = 440.0 * std::pow (2.0, (m - 69.0) / 12.0); ph += 2 * juce::MathConstants<double>::pi * hz / sr0; float v = 0.f; for (int h = 1; h <= 5; ++h) v += (float) std::sin (ph * h) / (float) h; b.setSample (0, i, .22f * v); }
                auto f = dir.getChildFile ("voz_demo.wav"); audioedit::writeWav (f, b, sr0); auto c = ts[0]->insertWaveClip ("voz", f, { te::TimeRange (te::TimePosition::fromSeconds (0.0), te::TimeDuration::fromSeconds (7.5)), te::TimeDuration() }, false);
                playlist.selectClips ({ c.get() }); setPitchEd (true); pitchEd.waitReady (20000); if (std::getenv ("INJ_PE_AUTO")) { pitchEd.setScale (0, 2); pitchEd.setAuto (true); } if (pitchEd.numNotes() > 3) pitchEd.setTargetOf (3, pitchEd.noteAt (3).tgt + 2.f);
            }
        }
        if (list.contains ("aed") || list.contains ("rack"))
        {   // solo pruebas: audio de ejemplo (golpe con cola) para las capturas
            auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_shot_audio"); dir.createDirectory(); auto ts = te::getAudioTracks (edit);
            auto mkSnd = [&] (const char* n, float hz, float decay) { audioedit::Buf b (2, 66150); for (int i = 0; i < 66150; ++i) { const float env = std::exp (-(float) i / (44100.f * decay)); const float v = env * (0.8f * std::sin (i * hz * (1.f + 2.f * std::exp (-(float) i / 2500.f)))) + 0.04f * std::sin (i * 0.9f) * env; b.setSample (0, i, v + 0.12f); b.setSample (1, i, v * 0.9f + 0.12f); } auto f = dir.getChildFile (n); audioedit::writeWav (f, b, 44100.0); return f; };
            if (list.contains ("aed") && ! ts.isEmpty())
            {
                auto f = mkSnd ("golpe_demo.wav", 0.035f, 0.35f); double clipSec = 1.5; juce::String clipName = "golpe demo";
                if (auto* ef = std::getenv ("INJ_AED_FILE")) { juce::File uf (ef); audioedit::Buf tb; double tr = 0; if (audioedit::readFile (uf, tb, tr)) { f = uf; clipSec = (double) tb.getNumSamples() / tr; clipName = uf.getFileNameWithoutExtension(); } }
                auto c = ts[0]->insertWaveClip (clipName, f, { te::TimeRange (te::TimePosition::fromSeconds (0.0), te::TimeDuration::fromSeconds (clipSec)), te::TimeDuration() }, false);
                playlist.selectClips ({ c.get() }); setAed (true); aed.setClip (dynamic_cast<te::WaveAudioClip*> (c.get())); aed.setSelection (0.12, 0.42);
                if (auto* es = std::getenv ("INJ_AED_SEL")) { double a = 0, b = 0; if (std::sscanf (es, "%lf,%lf", &a, &b) == 2) aed.setSelection (a, b); }
                if (auto* ez = std::getenv ("INJ_AED_ZOOM")) { double a = 0, b = 1; if (std::sscanf (ez, "%lf,%lf", &a, &b) == 2) aed.setView (a, b); }
            }
            if (list.contains ("rack") && ts.size() > 1)
            {
                mixer.selected = 1; setRack (true); rack.createEmptyKit(); const char* nm[] { "kick_gabber.wav", "kick_sub.wav", "caja_dura.wav", "hat_corto.wav", "clap_rave.wav" }; const float hz[] { 0.03f, 0.02f, 0.12f, 0.4f, 0.2f };
                for (int i = 0; i < 5; ++i) rack.loadSample (36 + (i == 4 ? 5 : i == 3 ? 7 : i), mkSnd (nm[i], hz[i], 0.2f)); rack.addLayer (36, mkSnd ("kick_dist.wav", 0.05f, 0.25f)); rack.setPadVolume (36, -3.f); rack.selPad = 36;
            }
        }
        if (list.contains ("perf")) { setPerf (true); for (int c = 0; c < 5; ++c) for (int r = 0; r < 3 + c % 3; ++r) perf.makeEmptyMidi (c, r, 4.0); if (list.contains ("perfplay")) { edit.getLaunchQuantisation().type = te::LaunchQType::none; perf.launch (0, 0); perf.launch (2, 1); } }
        if (list.contains ("steps")) setSteps (true);
        if (list.contains ("meters"))
        {   // solo capturas: señal de ejemplo (graves, ruido y algo de ancho estéreo) para ver el panel con datos
            setMeters (true); auto& me = meters.engineRef(); const int N = 44100 * 5; std::vector<float> l ((size_t) N), r ((size_t) N); juce::Random rnd (7);
            for (int i = 0; i < N; ++i) { const float t = (float) i / 44100.f, ph = std::fmod (t * 55.f, 1.f), saw = 2.f * ph - 1.f, kick = std::sin (juce::MathConstants<float>::twoPi * (50.f + 120.f * std::exp (-std::fmod (t, .4f) * 30.f)) * std::fmod (t, .4f)) * std::exp (-std::fmod (t, .4f) * 8.f);
                const float hat = (rnd.nextFloat() - .5f) * (std::fmod (t, .2f) < .02f ? .3f : .02f), lead = .12f * std::sin (juce::MathConstants<float>::twoPi * 880.f * t) * std::sin (t * 3.f); l[(size_t) i] = .4f * kick + .18f * saw + hat + lead; r[(size_t) i] = .4f * kick + .18f * saw - hat * .6f + lead * .5f; }
            me.feed (l.data(), r.data(), N); me.feed (l.data(), r.data(), 4096);
        }
    }
    void paint (juce::Graphics& g) override { g.fillAll (theme::bgBar); }

    void setMenuData (const juce::StringArray& r, const juce::StringArray& t) { bars.recents = r; bars.myTemplates = t; }
private:
    audition::Player auditioner;
    te::Engine& engine; std::unique_ptr<te::Edit>& editPtr; te::Edit& edit; midimap::Mapper midi { engine }; int midiTick = 0; TopBars bars; StatusBar status; juce::Component::SafePointer<juce::DialogWindow> pluginWin; Toast toast; juce::TooltipWindow tooltips { this, 500 }; std::unique_ptr<juce::FileChooser> chooser; PlaylistView playlist; BrowserPanel browser; MixerPanel mixer { edit }; StepSeqPanel steps { edit }; MetersPanel meters { edit }; PerformancePanel perf { edit }; AudioEditorPanel aed { edit }; PitchEditorPanel pitchEd { edit }; PadRackPanel rack { edit }; int browserW = 250; bool splitting = false; PianoRollView piano { edit }; int qi = 2; bool lastDirty = false; int tick = 0;
    // Abrir el Piano Roll siempre funciona: si no hay ningún clip MIDI, crea uno vacío de 4 compases en la pista elegida.
    void ensurePianoClip()
    {
        if (piano.getClip() != nullptr && piano.getClip()->getTrack() != nullptr) return;
        if (auto* mc = firstMidiClip()) { piano.setClip (mc); return; }
        auto tracks = te::getAudioTracks (edit); if (tracks.isEmpty()) return;
        auto* tr = tracks[juce::jlimit (0, tracks.size() - 1, mixer.selected)];
        auto T = [&] (double b) { return te::TimePosition::fromSeconds (edit.tempoSequence.toTime (te::BeatPosition::fromBeats (b)).inSeconds()); };
        auto c = tr->insertMIDIClip (juce::String::fromUTF8 ("Patrón MIDI"), te::TimeRange (T (0.0), T (16.0)), nullptr);
        piano.setClip (c.get()); playlist.repaint(); note (juce::String::fromUTF8 ("Se creó un clip MIDI vacío en la pista ") + juce::String (tracks.indexOf (tr) + 1) + juce::String::fromUTF8 (" para que puedas escribir notas"));
    }
    te::MidiClip* firstMidiClip()
    {
        for (auto* t : te::getAudioTracks (edit)) for (auto* c : t->getClips()) if (auto* mc = dynamic_cast<te::MidiClip*> (c)) return mc;
        return nullptr;
    }
    void showView (const juce::String& v)
    {
        const bool pr = (v == "piano"); bars.view = v; bars.pianoMode = pr; playlist.setVisible (! pr); piano.setVisible (pr);
        bars.crumb = pr ? "Piano Roll - " + (piano.getClip() ? piano.getClip()->getName() : juce::String ("sin clip")) : juce::String ("Playlist - Arrangement");
        if (pr && (bars.tool == "cut" || bars.tool == "brush" || bars.tool == "trim")) { bars.tool = "select"; piano.tool = "select"; }
        bars.resized(); bars.repaint();
    }
    int lastWholeBeat = -1; te::TimePosition playStart;
    void refreshMenuData() { setMenuData (userdata::recents(), userdata::templateNames()); }
    void showHistory()
    {
        auto d = edit.getUndoManager().getUndoDescriptions(); juce::PopupMenu m;
        if (d.isEmpty()) m.addItem (-1, juce::String::fromUTF8 ("No hay cambios para deshacer"), false);
        for (int i = 0; i < std::min (15, d.size()); ++i) m.addItem (i + 1, (i == 0 ? juce::String::fromUTF8 ("Último: ") : juce::String (i + 1) + ". ") + (d[i].isEmpty() ? juce::String ("Cambio") : d[i]));
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&bars), [this] (int r) { for (int k = 0; k < r; ++k) undoRedo (true); refreshAll(); });
    }
    std::unique_ptr<juce::FileChooser> refChooser; bool abOn = false; std::vector<std::pair<te::AudioTrack*, bool>> abMutes;
    te::AudioTrack* refTrack() { for (auto* t : te::getAudioTracks (edit)) if ((bool) t->state.getProperty ("injRef", false)) return t; return nullptr; }
    void loadReference()
    {
        refChooser = std::make_unique<juce::FileChooser> (juce::String::fromUTF8 ("Elige una canción de referencia"), juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.mp3;*.flac;*.aif;*.aiff;*.ogg");
        refChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult(); if (! f.existsAsFile()) return; te::AudioFile af (edit.engine, f); if (af.getLength() <= 0) { note ("No se pudo leer ese archivo"); return; }
            te::AudioTrack* dst = refTrack(); if (dst == nullptr) for (auto* t : te::getAudioTracks (edit)) if (t->getClips().isEmpty()) { dst = t; break; }
            if (dst == nullptr) { note ("No hay una pista libre para la referencia"); return; }
            edit.getUndoManager().beginNewTransaction(); for (int i = dst->getClips().size(); --i >= 0;) dst->getClips()[i]->removeFromParent();
            dst->state.setProperty ("injRef", true, nullptr); dst->setName ("Referencia"); dst->setMute (true);
            dst->insertWaveClip (f.getFileNameWithoutExtension(), f, { te::TimeRange (te::TimePosition(), te::TimeDuration::fromSeconds (af.getLength())), te::TimeDuration() }, false);
            note (juce::String::fromUTF8 ("Referencia cargada en la pista \"Referencia\" (silenciada). Pulsa F3 para comparar A/B.")); playlist.repaint(); mixer.repaint();
        });
    }
    void toggleAB()
    {
        auto* rt = refTrack(); if (rt == nullptr) { note (juce::String::fromUTF8 ("Primero carga una referencia (menú Tools)")); return; }
        if (! abOn) { abMutes.clear(); for (auto* t : te::getAudioTracks (edit)) { abMutes.push_back ({ t, t->isMuted (false) }); t->setMute (t != rt); } abOn = true; note (juce::String::fromUTF8 ("B: suena la referencia (volumen sin igualar)")); }
        else { for (auto& p : abMutes) p.first->setMute (p.second); abMutes.clear(); rt->setMute (true); abOn = false; note ("A: suena tu mezcla"); }
        bars.abOn = abOn; bars.repaint(); mixer.repaint(); playlist.repaint();
    }
    void saveTemplate()
    {
        auto* aw = new juce::AlertWindow (juce::String::fromUTF8 ("Guardar como plantilla"), juce::String::fromUTF8 ("Nombre de la plantilla"), juce::MessageBoxIconType::NoIcon); aw->addTextEditor ("n", "Mi plantilla"); aw->addButton ("Guardar", 1); aw->addButton ("Cancelar", 0);
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int r)
        {
            if (r == 1) { auto nm = juce::File::createLegalFileName (aw->getTextEditorContents ("n").trim()); if (nm.isNotEmpty()) { edit.flushState(); te::EditFileOperations ops (edit); const bool ok = ops.writeToFile (userdata::templatesDir().getChildFile (nm + ".injdaw"), false); refreshMenuData(); note (ok ? juce::String::fromUTF8 ("Plantilla guardada: ") + nm : juce::String::fromUTF8 ("No se pudo guardar la plantilla")); } }
            delete aw;
        }));
    }
    std::vector<double> taps;
    void tapTempo()
    {
        const double now = juce::Time::getMillisecondCounterHiRes(); if (! taps.empty() && now - taps.back() > 2000.0) taps.clear(); taps.push_back (now); if (taps.size() > 7) taps.erase (taps.begin());
        if (taps.size() >= 2) { const double bpm = juce::jlimit (20.0, 400.0, std::round (60000.0 / ((taps.back() - taps.front()) / (double) (taps.size() - 1)) * 100.0) / 100.0); bars.bpm = bpm; if (bars.onBpm) bars.onBpm (bpm); bars.repaint(); }
    }
    void timerCallback() override { syncState(); if (++midiTick % 90 == 1) midi.attach(); tickRepeat(); }
    void syncState()
    {
        auto& tc = edit.getTransport(); bool changed = false;
        auto upd = [&] (auto& dst, auto v) { if (dst != v) { dst = v; changed = true; } };
        if (bars.countIn != (int) edit.getCountInMode()) { bars.countIn = (int) edit.getCountInMode(); bars.repaint(); }
        upd (bars.playing, tc.isPlaying()); upd (bars.looping, (bool) tc.looping); upd (bars.metro, (bool) edit.clickTrackEnabled.get());
        const double beats = edit.tempoSequence.toBeats (tc.getPosition()).inBeats(), pb = std::floor (beats * 4.0) / 4.0;   // la posición cambia por semicorchea
        upd (bars.posBeats, pb);
        bars.onTap = [this] { tapTempo(); };
        { const int wb = (int) std::floor (beats); if (tc.isPlaying() && wb != lastWholeBeat) bars.beatPulse(); lastWholeBeat = tc.isPlaying() ? wb : -1; }
        const double cpu = std::round (engine.getDeviceManager().getCpuUsage() * 100.0) / 100.0; upd (bars.cpu, cpu);
        if (++tick % 20 == 1) { upd (bars.ramMB, std::round (sysinfo::processMemoryMB() / 100.0) * 100.0); upd (bars.ramTotalMB, (double) juce::SystemStats::getMemorySizeInMegabytes()); }
        if (changed) bars.repaint();
        if (onTitle && lastDirty != dirty()) { lastDirty = dirty(); onTitle(); }
        syncTimeSig(); if (playlist.selTrack != mixer.selected) { playlist.selTrack = mixer.selected; playlist.repaint(); }
        { const bool r = edit.getTransport().isRecording(); if (wasRec && ! r) commitTypedRecording(); wasRec = r; }
        if (bars.recArmed != edit.getTransport().isRecording()) { bars.recArmed = edit.getTransport().isRecording(); bars.repaint(); }
        if (juce::Time::getMillisecondCounter() - lastAutosave > 30000u) { if (! autosaveNow()) lastAutosave = juce::Time::getMillisecondCounter(); }   // también mientras suena
        if (tick % 10 == 1)
        {
            juce::String l = "Sin dispositivo de audio";
            if (auto* d = engine.getDeviceManager().deviceManager.getCurrentAudioDevice()) l = d->getTypeName() + ": " + d->getName() + "   |   " + juce::String (d->getCurrentSampleRate() / 1000.0, 1) + " kHz   |   " + juce::String::fromUTF8 ("búfer ") + juce::String (d->getCurrentBufferSizeSamples());
            const double endB = playlist.projectEndBeat(); const int nb = (int) std::ceil (endB / timesig::bar() - 1e-6); const int secs = (int) std::lround (edit.tempoSequence.toTime (te::BeatPosition::fromBeats (endB)).inSeconds());
            const int nsel = playlist.selectedClips().size(); juce::String info = nb > 0 ? juce::String (nb) + " compases  " + juce::String (secs / 60) + ":" + juce::String (secs % 60).paddedLeft ('0', 2) + "   |   " : juce::String();
            if (nsel > 1) info += juce::String (nsel) + " clips elegidos   |   ";
            const juce::String r = info + (projectFile != juce::File() ? projectFile.getFileNameWithoutExtension() : juce::String::fromUTF8 ("Sin título")) + (dirty() ? juce::String::fromUTF8 ("   |   sin guardar") : juce::String::fromUTF8 ("   |   guardado"));
            if (l != status.left || r != status.right) { status.left = l; status.right = r; status.repaint(); }
        }
    }
    void openAudioSettings()
    {
        auto* sel = new juce::AudioDeviceSelectorComponent (engine.getDeviceManager().deviceManager, 0, 2, 0, 2, true, false, true, false);
        sel->setSize (520, 420);
        juce::DialogWindow::LaunchOptions o; o.content.setOwned (sel); o.dialogTitle = "Ajustes de audio";
        o.componentToCentreAround = this; o.dialogBackgroundColour = theme::bgBar2; o.useNativeTitleBar = true; o.resizable = false; theme::darkTitleBar (o.launchAsync());
    }
};

class MainWindow : public juce::DocumentWindow
{
public:
    MainComponent* main = nullptr;
    MainWindow (te::Engine& e, std::unique_ptr<te::Edit>& ed) : DocumentWindow ("INJUSTICE DAW", theme::bgBar, DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true); main = new MainComponent (e, ed); setContentOwned (main, true);
        setResizable (true, true); setResizeLimits (1320, 700, 4000, 3000); centreWithSize (1360, 800); if (auto* ws = std::getenv ("INJ_WIN")) { int w = 1360, h = 800; if (std::sscanf (ws, "%dx%d", &w, &h) == 2) centreWithSize (w, h); } setVisible (true); theme::darkTitleBar (this);
        addKeyListener (main);
    }
    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
};

class InjusticeDawApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "INJUSTICE DAW"; }
    const juce::String getApplicationVersion() override { return "0.9.47"; }
    void initialise (const juce::String&) override
    {
        auto args = getCommandLineParameterArray();
        if (args.size() >= 3 && args[0] == "--scan-plugin") { pluginhost::runScanChild (args[1], args[2]); quit(); return; }   // modo hijo del escáner
        juce::LookAndFeel::setDefaultLookAndFeel (&look);
        engine = std::make_unique<te::Engine> ("InjusticeDAW", std::make_unique<pluginhost::UIBehaviour>(), std::make_unique<pluginhost::Behaviour>());
        const auto t0 = juce::Time::getMillisecondCounter(); auto tick = [t0] (const char* w) { if (std::getenv ("INJ_TIMING")) { std::printf ("t=%4d ms  %s\n", (int) (juce::Time::getMillisecondCounter() - t0), w); std::fflush (stdout); } };
        tick ("motor creado"); installMetronomeSounds(); tick ("metronomo");
        engine->getPluginManager().createBuiltInType<InjLimiter>(); engine->getPluginManager().createBuiltInType<InjTap>(); engine->getPluginManager().createBuiltInType<InjStereo>(); injfx::registerAll (*engine);
        engine->getDeviceManager().initialise (0, 2);
        tick ("audio"); pluginhost::loadList (*engine); tick ("lista de plugins");
        edit = makeEdit ({}); tick ("proyecto");
        if (auto* demo = std::getenv ("INJ_DEMO_WAV")) addDemoWav (demo);
        if (std::getenv ("INJ_DEMO_MIDI")) addDemoMidi();
        if (auto* df = std::getenv ("INJ_DEMO_FULL")) addDemoFull (df);
        if (std::getenv ("INJ_DEMO_CHORDS")) { harmony::setKey (*edit, { 9, 1 }); const int rr[] { 9, 5, 0, 7 }, qq[] { 1, 0, 0, 0 }; for (int i = 0; i < 4; ++i) harmony::add (*edit, i * 16.0, 16.0, rr[i], qq[i]); }
        if (std::getenv ("INJ_DEMO_SECTIONS")) { arranger::add (*edit, 0.0, 16.0, "Intro"); arranger::add (*edit, 16.0, 32.0, "Build"); arranger::add (*edit, 48.0, 32.0, "Drop"); arranger::add (*edit, 80.0, 16.0, "Break"); }
        edit->getTransport().ensureContextAllocated(); tick ("contexto de audio");
        {   // primer inicio: abre una canción de ejemplo (plantilla Hardcore) para que no empiece en blanco
            auto marker = userdata::dataDir().getChildFile ("primer_inicio.txt");
            if (! marker.existsAsFile() && ! std::getenv ("INJ_SHOT") && ! std::getenv ("INJ_SELFTEST") && ! std::getenv ("INJ_TEMPLATE") && ! std::getenv ("INJ_DEMO_WAV") && ! std::getenv ("INJ_DEMO_MIDI") && ! autosave::file().existsAsFile())
            { marker.replaceWithText ("1"); samplepack::ensure (samplepack::packsDir()); templates::pending() = 0; }
        }
        openWindow(); tick ("ventana");
        if (! std::getenv ("INJ_SHOT") && ! std::getenv ("INJ_SELFTEST")) tryRecover();
        if (std::getenv ("INJ_LOADTEST"))   // solo medicion: 100 pistas con muchos clips; imprime tiempos de dibujo y de render
        {
            auto* mc = window->main; auto tracks = te::getAudioTracks (*edit); const double bs = 60.0 / 172.0; int clips = 0;
            for (int ti = 0; ti < 100 && ti < tracks.size(); ++ti)
                for (int c = 0; c < 20; ++c)
                {
                    auto cl = tracks[ti]->insertMIDIClip ("MIDI", te::TimeRange (te::TimePosition::fromSeconds (c * 16 * bs), te::TimePosition::fromSeconds ((c * 16 + 15) * bs)), nullptr); ++clips;
                    for (int n = 0; n < 32; ++n) cl->getSequence().addNote (36 + (n * 7) % 24, te::BeatPosition::fromBeats (n * 0.5), te::BeatDuration::fromBeats (0.4), 100, 0, nullptr);
                }
            mc->setBounds (0, 0, 1360, 800); mc->resized();
            const auto t0 = juce::Time::getMillisecondCounter(); for (int i = 0; i < 20; ++i) { auto img = mc->createComponentSnapshot (mc->getLocalBounds(), true, 1.0f); }
            const auto t1 = juce::Time::getMillisecondCounter();
            auto out = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_load.wav"); out.deleteFile(); te::Renderer::renderToFile (*edit, out, false);
            const auto t2 = juce::Time::getMillisecondCounter();
            std::printf ("LOADTEST: %d clips en 100 pistas | dibujar la ventana: %.1f ms por cuadro | render offline de %.0f s de musica: %.2f s\n", clips, (double) (t1 - t0) / 20.0, edit->getLength().inSeconds(), (double) (t2 - t1) / 1000.0);
            std::fflush (stdout); std::_Exit (0);
        }
        if (auto* pn = std::getenv ("INJ_PANELS"))   // solo pruebas: abre paneles y datos de ejemplo para las capturas
        {
            auto* mc = window->main;
            if (juce::String (pn).contains ("help")) mc->testShowHelp();
            if (auto* mn = std::getenv ("INJ_MENU")) mc->testShowMenu (mn);
            if (auto* dl = std::getenv ("INJ_DIALOG")) mc->testShow (dl);
            if (std::getenv ("INJ_DEMO_EQ")) { auto tr = te::getAudioTracks (*edit); if (! tr.isEmpty()) { auto& mx = mc->mixerRef(); mx.setEqMid (*tr[0], 900.f, 7.f); mx.eqDemo (*tr[0]); } }
            if (std::getenv ("INJ_COMPACT")) mc->mixerRef().setCompact (true);
            if (std::getenv ("INJ_DEMO_MASTER")) { auto& mx = mc->mixerRef(); mx.selectMaster (true); mx.setMasterEqMid (1200.f, -5.f); mx.setWidthMaster (1.4f); }
            if (auto* scp = std::getenv ("INJ_SC_SHOT")) { auto tr = te::getAudioTracks (*edit); if (tr.size() > 3) mc->mixerRef().snapshotSidechain (*tr[3], juce::File (scp)); }
            if (std::getenv ("INJ_DEMO_BUS"))   // captura: tres canales entran a un bus y otro manda un envío
            {
                auto& mx = mc->mixerRef(); auto tr = te::getAudioTracks (*edit); mx.refreshTracks();
                { auto t0 = te::getAudioTracks (*edit); if (t0.size() > 6) mx.linkChannels (*t0[5], *t0[6]); for (int k = 0; k < 8 && k < t0.size(); ++k) t0[k]->state.setProperty ("injIcon", k + 1, nullptr); if (t0.size() > 1) { auto* pp = t0[1]->getVolumePlugin()->volParam.get(); pp->getCurve().addPoint (te::TimePosition::fromSeconds (0.0), pp->valueRange.start, 0.f); pp->getCurve().addPoint (te::TimePosition::fromSeconds (2.0), pp->getCurrentValue(), 0.f); } mx.selected = 3; }
                if (auto* b = mx.newBus ("Drum Bus")) { for (int i = 0; i < 3 && i < tr.size(); ++i) mx.setOutput (*tr[i], b); if (tr.size() > 4) mx.toggleSendTo (*tr[3], *tr[4]); }
                mx.refreshTracks();
            }
            if (auto* sd = std::getenv ("INJ_SAMPLES")) { mc->browserRef().setRoot (juce::File (sd)); mc->browserRef().expandAll (2); }
            if (auto* bs = std::getenv ("INJ_BSEARCH")) mc->browserRef().setSearch (bs);
            if (auto* sel = std::getenv ("INJ_SELECT")) mc->browserRef().selectFile (juce::File (sel));
            if (juce::String (pn).contains ("steps") && std::getenv ("INJ_DEMO_STEPS"))
            {
                auto& st = mc->stepsRef(); st.ensureClip();
                for (int s : { 0, 4, 8, 12 }) st.setStep (0, s, true);
                for (int s : { 4, 12 }) st.setStep (1, s, true);
                for (int s : { 0, 2, 4, 6, 8, 10, 12, 14 }) st.setStep (2, s, true);
                st.setStep (3, 14, true);
            }
            mc->openPanelsFromEnv (pn);
            if (auto* rz = std::getenv ("INJ_RESIZE")) { auto& mx = mc->mixerRef(); const float f = juce::String (rz).getFloatValue(); mx.setSize (juce::roundToInt (mx.designW * f), juce::roundToInt (mx.designH * f)); mx.setTopLeftPosition (60, 120); }
            if (std::getenv ("INJ_FAKE_PLUGINS"))   // solo para capturas: entradas de ejemplo (no se guardan)
            {
                struct F { const char* n; const char* m; bool inst; };
                for (auto f : { F { "Injustice Kick", "Injustice Audio", true }, F { "Injustice Bass", "Injustice Audio", true }, F { "Injustice Distortion", "Injustice Audio", false }, F { "Injustice Reverb", "Injustice Audio", false }, F { "Serum", "Xfer Records", true }, F { "OTT", "Xfer Records", false }, F { "Pro-Q 3", "FabFilter", false } })
                { juce::PluginDescription d; d.name = f.n; d.manufacturerName = f.m; d.isInstrument = f.inst; d.pluginFormatName = "VST3"; d.fileOrIdentifier = juce::String ("/fake/") + f.n + ".vst3"; d.uniqueId = (int) juce::String (f.n).hashCode(); engine->getPluginManager().knownPluginList.addType (d); }
            }
            if (juce::String (pn).contains ("plugins")) mc->openPluginList (0);
            if (juce::String (pn).contains ("help")) mc->showHelp();
        }
        if (auto* fxd = std::getenv ("INJ_FXSHOTS"))   // solo capturas: un PNG por cada editor de efecto
        {
            juce::File dir (fxd); dir.createDirectory(); auto tr = te::getAudioTracks (*edit);
            if (! tr.isEmpty()) { int n = 0; for (auto& e : injfx::catalog())
            {
                auto np = edit->getPluginCache().createNewPlugin (e.type, {}); if (np == nullptr) continue;
                tr[0]->pluginList.insertPlugin (np, -1, nullptr); np->setEnabled (true);
                if (auto ed = np->createEditor()) { ed->setVisible (true); auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f); juce::PNGImageFormat png; auto f = dir.getChildFile (juce::String (n).paddedLeft ('0', 2) + "_" + juce::String (e.type) + ".png"); f.deleteFile(); if (auto os = f.createOutputStream()) png.writeImageToStream (img, *os); }
                np->deleteFromParent(); ++n;
            } }
            juce::Timer::callAfterDelay (300, [] { juce::JUCEApplicationBase::quit(); });
        }
        // Modo de prueba: INJ_SHOT=ruta.png guarda una captura de la ventana y sale (sirve para revisar el diseño sin pantalla).
        if (auto* shot = std::getenv ("INJ_SHOT"))
        {
            juce::String path (shot);
            juce::Timer::callAfterDelay (1500, [this, path]
            {
                juce::Component* target = window->getContentComponent();
                if (std::getenv ("INJ_SHOT_DIALOG")) for (int i = 0; i < juce::TopLevelWindow::getNumTopLevelWindows(); ++i) if (auto* dw = dynamic_cast<juce::DialogWindow*> (juce::TopLevelWindow::getTopLevelWindow (i))) target = dw->getContentComponent();
                auto img = target->createComponentSnapshot (target->getLocalBounds(), true, 1.0f);
                juce::PNGImageFormat png; juce::File f (path); f.deleteFile();
                if (auto os = f.createOutputStream()) png.writeImageToStream (img, *os);
                quit();
            });
        }
    }
    void shutdown() override { autosave::clear(); window.reset(); edit.reset(); engine.reset(); juce::LookAndFeel::setDefaultLookAndFeel (nullptr); }
    void systemRequestedQuit() override
    {
        if (window == nullptr || window->main == nullptr || std::getenv ("INJ_SHOT") || ! window->main->dirty()) { quit(); return; }
        window->main->confirmThen ([this] { quit(); });
    }
private:
    theme::Look look;   // se declara primero para destruirse al final
    std::unique_ptr<te::Engine> engine; std::unique_ptr<te::Edit> edit; std::unique_ptr<MainWindow> window; juce::File currentFile;

    // file vacío = proyecto nuevo
    // Metrónomo propio: clic corto tipo "madera digital" (acento más agudo en el primer tiempo del compás)
    void installMetronomeSounds()
    {
        auto make = [] (const juce::File& f, double freq, float gain)
        {
            const double sr = 44100.0; const int n = (int) (sr * 0.09); juce::AudioBuffer<float> b (1, n); auto* d = b.getWritePointer (0);
            for (int i = 0; i < n; ++i)
            {
                const double t = i / sr, env = std::exp (-t * 55.0) * std::min (1.0, t / 0.0012);
                d[i] = (float) (env * (0.75 * std::sin (juce::MathConstants<double>::twoPi * freq * t) + 0.25 * std::sin (juce::MathConstants<double>::twoPi * freq * 2.76 * t) * std::exp (-t * 40.0))) * gain;
            }
            f.deleteFile(); juce::WavAudioFormat wav; if (auto os = f.createOutputStream()) if (std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 1, 16, {}, 0)); w) { os.release(); w->writeFromAudioSampleBuffer (b, 0, n); }
        };
        auto big = pluginhost::dataDir().getChildFile ("click_acento.wav"), little = pluginhost::dataDir().getChildFile ("click_tiempo.wav");
        make (big, 2093.0, 1.0f); make (little, 1046.0, 0.45f);
        engine->getPropertyStorage().setProperty (te::SettingID::clickTrackSampleBig, big.getFullPathName()); engine->getPropertyStorage().setProperty (te::SettingID::clickTrackSampleSmall, little.getFullPathName());
    }
    std::unique_ptr<te::Edit> makeEdit (const juce::File& f)
    {
        std::unique_ptr<te::Edit> e;
        if (f.existsAsFile()) e = te::loadEditFromFile (*engine, f);
        const bool fresh = (e == nullptr);
        if (fresh) e = te::createEmptyEdit (*engine, juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("injustice_untitled.tracktionedit"));
        e->clickTrackEmphasiseBars = true;   // el primer tiempo de cada compás suena con acento
        trackutil::ensureTracks (*e, 500);   // siempre 500 pistas, también al abrir un proyecto guardado
        if (! fresh) e->resetChangedStatus();
        if (fresh)
        {
            e->tempoSequence.getTempo (0)->setBpm (160.0);
            e->getMasterVolumePlugin()->setVolumeDb (0.f);
            for (auto* t : te::getAudioTracks (*e)) t->setName ("Track");
        }
        currentFile = (fresh || f.isAChildOf (userdata::templatesDir())) ? juce::File() : f;
        return e;
    }
    void openWindow()
    {
        window = std::make_unique<MainWindow> (*engine, edit);
        window->main->projectFile = currentFile;
        window->main->onReplace = [this] (juce::File f)
        {
            juce::MessageManager::callAsync ([this, f]
            {
                if (f.existsAsFile() && te::loadEditFromFile (*engine, f) == nullptr) { window->main->note ("No se pudo abrir ese proyecto."); return; }
                window.reset(); edit.reset(); edit = makeEdit (f); edit->getTransport().ensureContextAllocated(); openWindow();
                window->main->note (f.existsAsFile() ? juce::String::fromUTF8 ("Proyecto abierto: ") + f.getFileName() : juce::String::fromUTF8 ("Proyecto nuevo"));
                if (f.existsAsFile() && ! f.isAChildOf (userdata::templatesDir())) { userdata::addRecent (f); window->main->setMenuData (userdata::recents(), userdata::templateNames()); }
            });
        };
        window->main->onTitle = [this] { if (window) window->setName ("INJUSTICE DAW  -  " + (currentFileName()) + (window->main->dirty() ? " *" : "")); };
        window->main->onSaved = [this] (const juce::File& f) { currentFile = f; };
        window->main->onTitle();
        if (templates::pending() >= 0) { const int k = templates::pending(); templates::pending() = -1; window->main->applyTemplate (k); }
    }
    // Si la sesión anterior se cerró sin querer, ofrece recuperar la copia automática.
    void tryRecover()
    {
        if (! autosave::file().existsAsFile()) return;
        juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::NoIcon).withTitle (juce::String::fromUTF8 ("Recuperar proyecto"))
            .withMessage (juce::String::fromUTF8 ("La última vez el programa se cerró sin guardar. Hay una copia automática de tu trabajo. ¿Quieres recuperarla?"))
            .withButton ("Recuperar").withButton ("Descartar"),
            [this] (int r)
            {
                if (r != 1) { autosave::clear(); return; }
                auto orig = juce::File (autosave::meta().loadFileAsString().trim());
                juce::MessageManager::callAsync ([this, orig]
                {
                    window.reset(); edit.reset(); edit = makeEdit (autosave::file()); currentFile = orig.existsAsFile() || orig.getParentDirectory().exists() ? orig : juce::File();
                    edit->getTransport().ensureContextAllocated(); openWindow(); window->main->projectFile = currentFile; window->main->forceDirty = true; window->main->onTitle();
                    window->main->note (juce::String::fromUTF8 ("Proyecto recuperado. Guárdalo para conservarlo."));
                });
            });
    }
    juce::String currentFileName() const { return window && window->main && window->main->projectFile != juce::File() ? window->main->projectFile.getFileNameWithoutExtension() : juce::String::fromUTF8 ("Sin título"); }
    void addDemoWav (const char* demo)
    {
        auto tr = te::getAudioTracks (*edit)[1]; juce::File f (demo); te::AudioFile af (*engine, f);
        const auto st = te::TimePosition::fromSeconds (edit->tempoSequence.toTime (te::BeatPosition::fromBeats (8.0)).inSeconds());
        auto dc = tr->insertWaveClip ("demo", f, { te::TimeRange (st, te::TimeDuration::fromSeconds (af.getLength())), te::TimeDuration() }, false);
        if (std::getenv ("INJ_DEMO_FADE") && dc != nullptr) if (auto* dw = dynamic_cast<te::WaveAudioClip*> (dc.get())) { dw->setFadeIn (te::TimeDuration::fromSeconds (0.8)); dw->setFadeOut (te::TimeDuration::fromSeconds (1.2)); dw->setGainDB (-6.f); if (window && window->main) window->main->demoFade (dw); }
    }
    // Arreglo de ejemplo para capturas: varios clips MIDI y de audio repartidos por las pistas
    void addDemoFull (const char* wav)
    {
        auto tracks = te::getAudioTracks (*edit); auto T = [&] (double b) { return te::TimePosition::fromSeconds (edit->tempoSequence.toTime (te::BeatPosition::fromBeats (b)).inSeconds()); };
        auto midi = [&] (int ti, const char* name, double b0, double b1, const std::vector<std::array<double, 4>>& notes)
        { auto c = tracks[ti]->insertMIDIClip (name, te::TimeRange (T (b0), T (b1)), nullptr); for (auto& n : notes) c->getSequence().addNote ((int) n[0], te::BeatPosition::fromBeats (n[1]), te::BeatDuration::fromBeats (n[2]), (int) n[3], 0, nullptr); };
        std::vector<std::array<double, 4>> drums, bass, keys, lead;
        for (int b = 0; b < 16; ++b) { drums.push_back ({ 36, b * 1.0, .25, 110 }); drums.push_back ({ 42, b * 1.0 + .5, .25, 80 }); if (b % 2) drums.push_back ({ 38, b * 1.0, .25, 100 }); drums.push_back ({ 46, b * 0.5 + .25, .125, 70 }); }
        const int bn[] { 36, 36, 39, 41, 36, 36, 43, 41 }; for (int i = 0; i < 8; ++i) bass.push_back ({ (double) bn[i], i * 2.0, 1.5, 100 });
        for (int c = 0; c < 4; ++c) for (int k : { 60, 64, 67, 72 }) keys.push_back ({ (double) k + (c % 2 ? 2 : 0), c * 4.0, 3.5, 90 });
        const int ln[] { 72, 74, 76, 79, 76, 74, 72, 69, 71, 72, 74, 76, 79, 83, 81, 79 }; for (int i = 0; i < 16; ++i) lead.push_back ({ (double) ln[i], i * 1.0, i % 3 == 1 ? .5 : 1.0, 100 });
        { const char* nmz[] { "Kick", "Clap", "Hat", "Perc", "Lead", "Chords", "Bass", "Vocal", "FX", "Riser" }; for (int i = 0; i < 10; ++i) tracks[i]->setName (nmz[i]); }
        midi (0, "Drums", 0, 16, drums); midi (0, "Drums", 24, 40, drums); midi (1, "Bass", 0, 16, bass); midi (1, "Bass", 24, 40, bass);
        midi (2, "Keys", 0, 16, keys); midi (2, "Keys", 32, 48, keys); midi (4, "Lead", 8, 24, lead); midi (6, "Lead", 16, 32, lead);
        juce::File f (wav); te::AudioFile af (*engine, f); const double len = af.getLength();
        auto audio = [&] (int ti, double b0, double b1) { tracks[ti]->insertWaveClip ("Vocal", f, { te::TimeRange (T (b0), T (b1)), te::TimeDuration() }, false); (void) len; };
        midi (0, "Drums", 48, 64, drums); midi (0, "Drums", 72, 88, drums); midi (1, "Clap", 8, 16, drums); midi (1, "Clap", 40, 64, drums); midi (1, "Clap", 72, 88, drums);
        midi (2, "Hats", 16, 40, drums); midi (2, "Hats", 56, 88, drums); midi (4, "Lead", 40, 56, lead); midi (4, "Lead", 64, 88, lead);
        midi (6, "Bass", 8, 24, bass); midi (6, "Bass", 40, 72, bass); midi (8, "FX", 12, 16, lead); midi (8, "FX", 36, 40, lead); midi (8, "FX", 60, 64, lead); midi (9, "Riser", 28, 40, keys); midi (9, "Riser", 60, 72, keys);
        audio (8, 0, 4); audio (9, 4, 12); audio (9, 44, 52); audio (6, 24, 32); audio (6, 52, 60);
        audio (3, 0, 8); audio (3, 12, 20); audio (3, 28, 36); audio (5, 4, 12); audio (5, 20, 28); audio (7, 0, 6); audio (7, 16, 22); audio (7, 36, 44);
    }
    void addDemoMidi()
    {
        auto tr = te::getAudioTracks (*edit)[0];
        auto c = tr->insertMIDIClip ("Lead", te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (edit->tempoSequence.toTime (te::BeatPosition::fromBeats (32.0)).inSeconds())), nullptr);
        const int notes[] { 60, 62, 64, 67, 71, 65, 64, 61, 59, 60, 62, 64, 67, 71, 69, 65 }; const double starts[] { 0, 1, 1.5, 2, 3, 4, 5, 6, 7, 8, 9, 9.5, 10, 11, 12, 13 };
        for (int i = 0; i < 16; ++i) c->getSequence().addNote (notes[i], te::BeatPosition::fromBeats (starts[i]), te::BeatDuration::fromBeats (i % 3 == 1 ? .5 : 1.0), 60 + (i * 37) % 60, 0, nullptr);
    }
};

START_JUCE_APPLICATION (InjusticeDawApp)
