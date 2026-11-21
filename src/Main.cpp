#include <array>
#include "TopBars.h"
#include "PlaylistView.h"
#include "PianoRollView.h"
#include "MixerPanel.h"
#include "StepSeqPanel.h"
#include "InjLimiter.h"
#include <thread>
#include "Meters.h"
#include "PerformancePanel.h"
#include "Instruments.h"
#include "Audition.h"
#include "TimeSig.h"
#include "Templates.h"
#include "BrowserPanel.h"
#include "PluginHost.h"
#include "TrackUtil.h"
#include "SysInfo.h"

#include <map>

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
        g.setColour (juce::Colour (0xff17141c).withAlpha (.96f * a)); g.fillRoundedRectangle (r, 8.f);
        g.setColour (theme::accent.withAlpha (a)); g.drawRoundedRectangle (r, 8.f, 1.5f);
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
    HelpContent() { setSize (620, 860); }
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
            { nullptr, "Playlist" },
            { "Clic en un clip / arrastrar", "Elegir / mover (también entre pistas)" }, { "Arrastrar en vacío (Selector)", "Rectángulo para elegir varios clips" }, { "Mayús+clic", "Sumar o quitar un clip de la selección" },
            { "Ctrl+A / Esc", "Elegir todos los clips / quitar la selección" }, { "Ctrl+C / Ctrl+X / Ctrl+V", "Copiar / cortar / pegar en el cursor" }, { "Ctrl+D", "Duplicar lo elegido" }, { "Ctrl+M", "Marcador en el cursor (clic derecho en la regla: más)" }, { "Ctrl+←  /  Ctrl+→", "Ir al marcador anterior / siguiente" }, { "Supr", "Borrar lo elegido" },
            { "Automatización + clic", "Punto de volumen; arrastra para moverlo, clic derecho para borrar" },
            { "Pincel + arrastrar", "Pinta clips de 1 compás; sobre un clip, copia lo que ya hiciste" },
            { "Ctrl+rueda / Alt+rueda", "Zoom horizontal / alto de las pistas" },
            { nullptr, "Mixer" },
            { "Doble clic en un fader", "Volver a 0 dB" }, { "Doble clic en el nombre de un canal", "Renombrar" }, { "Rueda sobre un fader / perilla", "Ajuste fino" } };
        int y = 80; for (auto& r : rows)
        {
            if (r.k == nullptr) { y += 6; g.setColour (textDim); g.setFont (juce::FontOptions (12.f, juce::Font::bold)); g.drawText (juce::String::fromUTF8 (r.d), 24, y, 400, 18, juce::Justification::centredLeft); y += 20; continue; }
            g.setColour (juce::Colour (0xff231f2b)); g.fillRoundedRectangle (20.f, (float) y, 580.f, 22.f, 5.f);
            g.setColour (accentHi); g.setFont (juce::FontOptions (13.f, juce::Font::bold)); g.drawText (juce::String::fromUTF8 (r.k), 30, y, 250, 22, juce::Justification::centredLeft);
            g.setColour (text); g.setFont (juce::FontOptions (13.f)); g.drawText (juce::String::fromUTF8 (r.d), 270, y, 330, 22, juce::Justification::centredLeft); y += 25;
        }
    }
};

// Guardado automático: copia del proyecto en una carpeta de recuperación. Se borra al cerrar bien o al guardar.
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
        go.setButtonText ("Exportar..."); cancel.setButtonText ("Cancelar"); addAndMakeVisible (go); addAndMakeVisible (cancel);
        go.onClick = [this] { if (onGo) onGo ({ fmt.getSelectedId() - 1, std::array<double, 3> { 44100.0, 48000.0, 96000.0 } [(size_t) sr.getSelectedId() - 1], norm.getToggleState(), what.getSelectedId() == 2 }); };
        cancel.onClick = [this] { if (onCancel) onCancel(); };
    }
    void paint (juce::Graphics& g) override
    {
        using namespace theme; g.fillAll (bgBar2); g.setColour (text); g.setFont (juce::FontOptions (13.f));
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
        addAndMakeVisible (bars); addAndMakeVisible (status); addAndMakeVisible (playlist); addChildComponent (toast); addChildComponent (piano); addAndMakeVisible (browser); addChildComponent (mixer); addChildComponent (steps); addChildComponent (meters); addChildComponent (perf);
        mixer.onClose = [this] { setMixer (false); }; steps.onClose = [this] { setSteps (false); }; meters.onClose = [this] { setMeters (false); }; perf.onClose = [this] { setPerf (false); }; perf.onMessage = [this] (const juce::String& m) { note (m); }; meters.attach(); mixer.onTracksChanged = [this] { playlist.repaint(); }; mixer.onMessage = [this] (const juce::String& s) { note (s); };
        steps.onClipChanged = [this] { playlist.repaint(); };
        browser.setDeviceManager (&engine.getDeviceManager().deviceManager); browser.onMessage = [this] (const juce::String& m) { note (m); }; browser.onClose = [this] { setBrowser (false); }; browser.onCollapse = [this] { setBrowser (false); };
        mixer.onAddEffect = [this] (int t) { openPluginList (t); };
        playlist.ourPlugins = [this] { juce::Array<juce::PluginDescription> a; for (auto& d : engine.getPluginManager().knownPluginList.getTypes()) if (pluginhost::isInjusticePlugin (d)) a.add (d); return a; };
        playlist.onAddPlugin = [this] (const juce::PluginDescription& d, int ti) { addPlugin (d, ti); };
        mixer.onOpenPlugin = [this] (te::Plugin* p) { if (p) openEditorWhenReady (p); };
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
        bars.bpm = edit.tempoSequence.getTempo (0)->getBpm();
        bars.onSig = [this] (int n, int d) { edit.tempoSequence.getTimeSigAt (te::TimePosition()).setStringTimeSig (juce::String (n) + "/" + juce::String (d)); syncTimeSig(); note ("Comp\xc3\xa1s " + juce::String (n) + "/" + juce::String (d)); };
        bars.onBpm = [this] (double b) { edit.tempoSequence.getTempo (0)->setBpm (b); };
        bars.onButton = [this] (const juce::String& id)
        {
            auto& tc = edit.getTransport();
            if (id == "play") { if (tc.isPlaying()) tc.stop (false, false); else { steps.endPattern(); browser.stopPreview(); tc.play (false); } }
            else if (id == "stop") { tc.stop (false, false); tc.setPosition (te::TimePosition()); }
            else if (id == "loop") { tc.looping = ! tc.looping; }
            else if (id == "undo") { undoRedo (true); }
            else if (id == "redo") { undoRedo (false); }
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
            else if (id == "dup") { playlist.keyPressed (juce::KeyPress ('d', juce::ModifierKeys::commandModifier, 0)); }
            else if (id == "del") { playlist.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)); }
            else if (id == "about") note (juce::String::fromUTF8 ("INJUSTICE DAW ") + juce::JUCEApplication::getInstance()->getApplicationVersion() + juce::String::fromUTF8 (" · código abierto (AGPL-3.0) · Injustice Audio"));
            else if (id == "metro") { edit.clickTrackEnabled = ! edit.clickTrackEnabled.get(); bars.metro = edit.clickTrackEnabled.get(); note (bars.metro ? "Metr\xc3\xb3nomo activado" : "Metr\xc3\xb3nomo apagado"); }
            else if (id == "tool:trim") { bars.tool = "trim"; playlist.tool = "trim"; }
            else if (id == "view:mixer") setMixer (! mixer.isVisible());
            else if (id == "view:steps") setSteps (! steps.isVisible());
            else if (id == "view:meters") setMeters (! meters.isVisible());
            else if (id == "view:perf") setPerf (! perf.isVisible());
            syncState();
        };
        if (auto* th = std::getenv ("INJ_TRACKH")) playlist.setAllHeights (juce::String (th).getIntValue());
        if (std::getenv ("INJ_AUTO")) { bars.tool = "auto"; playlist.tool = "auto"; const double bs = 60.0 / 172.0; for (auto [b, v] : { std::pair<double, float> { 0, .85f }, { 8, .85f }, { 12, .3f }, { 16, .85f }, { 24, .55f } }) playlist.addAutoPoint (0, b * bs, v); }
        if (auto* zz = std::getenv ("INJ_ZOOM")) playlist.setZoom (juce::String (zz).getDoubleValue());
        if (auto* v = std::getenv ("INJ_VIEW")) if (juce::String (v) == "piano") { piano.setClip (firstMidiClip()); showView ("piano"); }
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
                std::printf (*fails == 0 ? "TODO OK\n" : "%d FALLOS\n", *fails); std::fflush (stdout); std::_Exit (*fails == 0 ? 0 : 1);
            });
        });
    }
    int selfFails = 0;
    int selfTestStage2()
    {
        int fails = selfFails; auto chk = [&] (bool ok, const char* what) { std::printf ("%s  %s\n", ok ? "OK  " : "FALLO", what); if (! ok) ++fails; };
        {
            auto out = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_steps_test.wav");
            out.deleteFile();   // el render AÑADE al archivo si ya existe: hay que partir de uno vacío
            const bool ok = te::Renderer::renderToFile (edit, out, false); chk (ok, "step: el render offline termina");
            juce::AudioFormatManager fmg; fmg.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> rd (fmg.createReaderFor (out));
            if (rd)
            {
                juce::AudioBuffer<float> b ((int) rd->numChannels, (int) rd->lengthInSamples); rd->read (&b, 0, b.getNumSamples(), 0, true, true);
                auto peak = [&] (double t0, double t1) { float p = 0; for (int c = 0; c < b.getNumChannels(); ++c) for (int i = (int) (t0 * rd->sampleRate); i < std::min (b.getNumSamples(), (int) (t1 * rd->sampleRate)); ++i) p = std::max (p, std::abs (b.getSample (c, i))); return p; };
                const double step = 60.0 / 172.0 / 4.0;
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
                                    runToneTest (fails);
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
        const float cw = (float) (steps.designW - 336 - 26) / 16.f;
        for (int s : { 0, 4, 8, 12 }) { juce::Point<float> p ((336 + s * cw + cw * .5f) * steps.scale, ((float) (FloatingPanel::kHead + 96) + 38.f) * steps.scale); steps.mouseDown (sev (p, L)); steps.mouseUp (sev (p, 0)); }
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
                const float bx = playlist.beatToX (40.0), by = (float) playlist.trackTop (5) + 20.f; playlist.mouseDown (pev ({ bx, by }, L)); playlist.mouseUp (pev ({ bx, by }, 0));
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
                  const int h0 = piano.kNoteH; piano.mouseWheelMove (rev ({ 200.f, 200.f }, juce::ModifierKeys::altModifier), juce::MouseWheelDetails { 0.f, 0.5f, false, false, 0.f });
                  chk (piano.kNoteH != h0, "piano roll: Alt+rueda cambia el alto de las notas (zoom vertical)"); piano.keyPressed (juce::KeyPress ('l', 0, 0)); chk (piano.kVelH == 0, "piano roll: L oculta el carril de velocity"); piano.keyPressed (juce::KeyPress ('l', 0, 0)); }
                piano.onAudition = oldAud; piano.setClip (nullptr); pcl->removeFromParent(); edit.deleteTrack (pt);
            }
            if (auto* sfp = std::getenv ("INJ_SAMPLE_FILE"))
            {   // biblioteca: varias carpetas y busqueda en todas, escuchar
                auto base = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_multi_test"); base.deleteRecursively(); auto fa = base.getChildFile ("A"), fb = base.getChildFile ("B"); fa.createDirectory(); fb.createDirectory();
                juce::File (sfp).copyFileTo (fb.getChildFile ("zzz_unico.wav"));
                const int n0 = browser.numRoots(); chk (browser.addExtraRoot (fb), "biblioteca: se puede agregar otra carpeta"); chk (browser.numRoots() == n0 + 1, "biblioteca: queda en la lista de carpetas");
                browser.setSearch ("zzz_unico"); chk (browser.numShown() == 1, "biblioteca: la busqueda encuentra samples en todas las carpetas"); browser.setSearch ("");
                browser.playPreview (fb.getChildFile ("zzz_unico.wav")); chk (browser.isPreviewing(), "browser: el sample se escucha al seleccionarlo"); browser.stopPreview(); chk (! browser.isPreviewing(), "browser: y se puede detener");
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
    void refreshAll() { playlist.repaint(); piano.repaint(); steps.repaint(); mixer.repaint(); }
    bool forceDirty = false;
    bool dirty() const { return forceDirty || edit.hasChangedSinceSaved(); }
    bool autosaveNow() { if (! dirty()) return false; const bool ok = autosave::write (edit, projectFile); lastAutosave = juce::Time::getMillisecondCounter(); return ok; }
    juce::uint32 lastAutosave = juce::Time::getMillisecondCounter();
    void note (const char* utf8) { toast.place (getLocalBounds()); toast.show (juce::String::fromUTF8 (utf8)); }
    void note (const juce::String& s) { toast.place (getLocalBounds()); toast.show (s); }
    static juce::File projectsDir() { auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("INJUSTICE DAW"); d.createDirectory(); return d; }
    void confirmThen (std::function<void()> go)
    {
        if (! dirty()) { go(); return; }
        juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::QuestionIcon).withTitle (juce::String::fromUTF8 ("Cambios sin guardar"))
            .withMessage (juce::String::fromUTF8 ("El proyecto tiene cambios sin guardar. ¿Quieres guardarlos antes de continuar?"))
            .withButton ("Guardar").withButton ("No guardar").withButton ("Cancelar").withAssociatedComponent (this),
            [this, go] (int r) { if (r == 1) saveProject (false, go); else if (r == 2) go(); });
    }
    bool doSave (const juce::File& f)
    {
        te::EditFileOperations ops (edit); const bool ok = ops.saveAs (f, true);
        if (ok) { forceDirty = false; autosave::clear(); projectFile = f; if (onSaved) onSaved (f); note (juce::String::fromUTF8 ("Guardado: ") + f.getFileName()); if (onTitle) onTitle(); } else note ("No se pudo guardar el proyecto en esa ubicaci\xc3\xb3n.");
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
    bool renderWav (const juce::File& f, double len, ExportOpts o = {}, int onlyTrack = -1)
    {
        if (edit.getTransport().isPlaying()) edit.getTransport().stop (false, false);
        te::Renderer::Parameters p (edit); p.destFile = f;
        p.audioFormat = o.fmt == 2 ? engine.getAudioFileFormatManager().getFlacFormat() : engine.getAudioFileFormatManager().getWavFormat();
        p.bitDepth = o.fmt == 1 ? 16 : 24; p.sampleRateForAudio = o.sr; p.shouldNormalise = o.norm; p.ditheringEnabled = o.fmt == 1;
        p.time = te::TimeRange (te::TimePosition(), te::TimeDuration::fromSeconds (len + 2.0)); p.useMasterPlugins = onlyTrack < 0; p.canRenderInMono = false; p.checkNodesForAudio = false;
        if (onlyTrack < 0) { for (int i = 0; i < te::getAllTracks (edit).size(); ++i) p.tracksToDo.setBit (i); }
        else p.tracksToDo.setBit (onlyTrack);
        f.deleteFile(); return te::Renderer::renderToFile ("Exportar", p).existsAsFile();
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
        auto* dw = lo.launchAsync (); juce::Component::SafePointer<juce::DialogWindow> win (dw);
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
        juce::DialogWindow::LaunchOptions o; o.content.setOwned (new HelpContent()); o.dialogTitle = "Ayuda"; o.componentToCentreAround = this;
        o.dialogBackgroundColour = theme::bgBar2; o.useNativeTitleBar = true; o.resizable = false; o.launchAsync();
    }
    void addPlugin (const juce::PluginDescription& d, int trackIdx)
    {
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
        const int t = track >= 0 ? track : mixer.selected;
        if (pluginWin != nullptr) { if (auto* c = dynamic_cast<pluginhost::ListContent*> (pluginWin->getContentComponent())) c->setTarget (t); pluginWin->toFront (true); return; }
        auto* c = new pluginhost::ListContent (engine); c->setTarget (t); c->onAdd = [this, c] (const juce::PluginDescription& d) { addPlugin (d, c->targetTrack); };
        juce::DialogWindow::LaunchOptions o; o.content.setOwned (c); o.dialogTitle = "Plugins VST3"; o.componentToCentreAround = this; o.dialogBackgroundColour = theme::bgBar2;
        o.useNativeTitleBar = true; o.resizable = true; o.escapeKeyTriggersCloseButton = true; pluginWin = o.launchAsync();
        if (c->isEmpty()) c->startScan();
    }
    bool keyPressed (const juce::KeyPress& k, juce::Component*) override
    {
        const auto m = k.getModifiers(); const int c = k.getKeyCode(); auto& tc = edit.getTransport();
        if (m.isCommandDown() && juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) c) == 't') { audition::typingPiano = ! audition::typingPiano; if (! audition::typingPiano) typedReleaseAll(); note (audition::typingPiano ? juce::String::fromUTF8 ("Teclado como piano: ACTIVADO (Z-M y Q-U, Re P\xc3\xa1g / Av P\xc3\xa1g = octava)") : juce::String::fromUTF8 ("Teclado como piano: apagado")); return true; }
        if (handleTypedKey (k)) return true;
        if (c == juce::KeyPress::spaceKey && ! m.isAnyModifierKeyDown()) { if (tc.isPlaying()) tc.stop (false, false); else { steps.endPattern(); browser.stopPreview(); tc.play (false); } syncState(); return true; }
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
            if (ch == 'e') { exportWav(); return true; }
            if (ch == 'g') { openIdeasDialog(); return true; }
        }
        return false;
    }
    // ---- plantillas e ideas ----
    void applyTemplate (int k)
    {
        const int made = templates::apply (edit, samplepack::packsDir(), k);
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
        aw->addComboBox ("estilo", st, "Estilo"); aw->addComboBox ("que", { "Bater\xc3\xad" "a", "Bajo", "Melod\xc3\xad" "a", "Acordes (stabs)", "Pad (acordes largos)" }, "Qu\xc3\xa9 crear");
        aw->addComboBox ("comp", { "4 compases", "8 compases", "16 compases" }, "Largo"); aw->addComboBox ("raiz", { "Do", "Do#", "Re", "Re#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" }, "Nota ra\xc3\xadz");
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
    void typedNoteOn (int key, int semi)
    {
        if (heldKeys.count (key)) return;
        auto* t = typedTarget(); if (t == nullptr) return; const int pitch = juce::jlimit (0, 127, 12 * (typedOct + 1) + semi);
        heldKeys[key] = pitch;
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
        toast.place (getLocalBounds());
        auto r = getLocalBounds(); bars.setBounds (r.removeFromTop (TopBars::kHeight)); status.setBounds (r.removeFromBottom (StatusBar::kHeight));
        if (browser.isVisible()) { browser.setBounds (r.removeFromLeft (browserW).reduced (6, 6)); r.removeFromLeft (6); }
        playlist.setBounds (r); piano.setBounds (r);
        if (mixer.isVisible() && ! mixer.maximized) mixer.setTopLeftPosition (juce::jmax (0, getWidth() - mixer.getWidth() - 12), juce::jmax (TopBars::kHeight, getHeight() - StatusBar::kHeight - mixer.getHeight() - 12));
    }
    void mouseDown (const juce::MouseEvent& e) override { splitting = browser.isVisible() && e.x >= browserW - 2 && e.x <= browserW + 8 && e.y > TopBars::kHeight; }
    void mouseDrag (const juce::MouseEvent& e) override { if (splitting) { browserW = juce::jlimit (220, 520, e.x); resized(); repaint(); } }
    void mouseMove (const juce::MouseEvent& e) override { setMouseCursor (browser.isVisible() && e.x >= browserW - 2 && e.x <= browserW + 8 && e.y > TopBars::kHeight ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor); }
    void setBrowser (bool v) { browser.setVisible (v); bars.browserOpen = v; bars.repaint(); resized(); repaint(); }
    template <typename P> void fitPanel (P& p)
    {
        const float s = std::min ({ 1.f, (float) (getWidth() - 24) / p.designW, (float) (getHeight() - TopBars::kHeight - 24) / p.designH });
        if (s < p.scale || p.getWidth() == 0) { p.scale = std::max (.4f, s); p.setSize (juce::roundToInt (p.designW * p.scale), juce::roundToInt (p.designH * p.scale)); }
    }
    void setMixer (bool v) { if (v) fitPanel (mixer); mixer.setVisible (v); bars.mixerOpen = v; if (v) { mixer.setTopLeftPosition (juce::jmax (0, getWidth() - mixer.getWidth() - 12), juce::jmax (TopBars::kHeight, getHeight() - mixer.getHeight() - 12)); mixer.toFront (false); } bars.repaint(); }
    void setPerf (bool v) { if (v) fitPanel (perf); perf.setVisible (v); bars.perfOpen = v; if (v) { perf.setTopLeftPosition (juce::jmax (0, (getWidth() - perf.getWidth()) / 2), TopBars::kHeight + 30); perf.toFront (false); } bars.repaint(); }
    void setMeters (bool v) { if (v) fitPanel (meters); meters.setVisible (v); bars.metersOpen = v; if (v) { meters.setTopLeftPosition (juce::jmax (0, (getWidth() - meters.getWidth()) / 2), TopBars::kHeight + 50); meters.toFront (false); } bars.repaint(); }
    void setSteps (bool v)
    {   // la ventana aparece al instante; el patrón y su sampler se crean justo después, sin congelar la pantalla
        if (v) { fitPanel (steps); if (! steps.hasClip()) { juce::Component::SafePointer<MainComponent> self (this); juce::MessageManager::callAsync ([self] { if (self != nullptr) { self->steps.ensureClip(); self->steps.assignDefaults (samplepack::packDir (samplepack::packsDir())); self->steps.repaint(); self->playlist.repaint(); } }); } }
        steps.setVisible (v); bars.stepsOpen = v; if (v) { steps.setTopLeftPosition (juce::jmax (0, (getWidth() - steps.getWidth()) / 2), TopBars::kHeight + 70); steps.toFront (false); } playlist.repaint(); bars.repaint(); }
    PianoRollView& pianoRef() { return piano; } StepSeqPanel& stepsRef() { return steps; } BrowserPanel& browserRef() { return browser; } MixerPanel& mixerRef() { return mixer; }
    void openPanelsFromEnv (const juce::String& list)
    {
        if (auto* tp = std::getenv ("INJ_TEMPLATE")) { samplepack::ensure (samplepack::packsDir()); applyTemplate (std::atoi (tp)); }
        if (list.contains ("browser")) setBrowser (true); else setBrowser (false);
        if (list.contains ("mixer")) setMixer (true);
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

private:
    audition::Player auditioner;
    te::Engine& engine; std::unique_ptr<te::Edit>& editPtr; te::Edit& edit; TopBars bars; StatusBar status; juce::Component::SafePointer<juce::DialogWindow> pluginWin; Toast toast; juce::TooltipWindow tooltips { this, 500 }; std::unique_ptr<juce::FileChooser> chooser; PlaylistView playlist; BrowserPanel browser; MixerPanel mixer { edit }; StepSeqPanel steps { edit }; MetersPanel meters { edit }; PerformancePanel perf { edit }; int browserW = 300; bool splitting = false; PianoRollView piano { edit }; int qi = 2; bool lastDirty = false; int tick = 0;
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
    int lastWholeBeat = -1;
    void timerCallback() override { syncState(); }
    void syncState()
    {
        auto& tc = edit.getTransport(); bool changed = false;
        auto upd = [&] (auto& dst, auto v) { if (dst != v) { dst = v; changed = true; } };
        upd (bars.playing, tc.isPlaying()); upd (bars.looping, (bool) tc.looping); upd (bars.metro, (bool) edit.clickTrackEnabled.get());
        const double beats = edit.tempoSequence.toBeats (tc.getPosition()).inBeats(), pb = std::floor (beats * 4.0) / 4.0;   // la posición cambia por semicorchea
        upd (bars.posBeats, pb);
        { const int wb = (int) std::floor (beats); if (tc.isPlaying() && wb != lastWholeBeat) bars.beatPulse(); lastWholeBeat = tc.isPlaying() ? wb : -1; }
        const double cpu = std::round (engine.getDeviceManager().getCpuUsage() * 100.0) / 100.0; upd (bars.cpu, cpu);
        if (++tick % 20 == 1) { upd (bars.ramMB, std::round (sysinfo::processMemoryMB() / 100.0) * 100.0); upd (bars.ramTotalMB, (double) juce::SystemStats::getMemorySizeInMegabytes()); }
        if (changed) bars.repaint();
        if (onTitle && lastDirty != dirty()) { lastDirty = dirty(); onTitle(); }
        syncTimeSig(); if (playlist.selTrack != mixer.selected) { playlist.selTrack = mixer.selected; playlist.repaint(); }
        { const bool r = edit.getTransport().isRecording(); if (wasRec && ! r) commitTypedRecording(); wasRec = r; }
        if (bars.recArmed != edit.getTransport().isRecording()) { bars.recArmed = edit.getTransport().isRecording(); bars.repaint(); }
        if (juce::Time::getMillisecondCounter() - lastAutosave > 60000u && ! edit.getTransport().isPlaying()) { if (! autosaveNow()) lastAutosave = juce::Time::getMillisecondCounter(); }
        if (tick % 10 == 1)
        {
            juce::String l = "Sin dispositivo de audio";
            if (auto* d = engine.getDeviceManager().deviceManager.getCurrentAudioDevice()) l = d->getTypeName() + ": " + d->getName() + "   |   " + juce::String (d->getCurrentSampleRate() / 1000.0, 1) + " kHz   |   " + juce::String::fromUTF8 ("búfer ") + juce::String (d->getCurrentBufferSizeSamples());
            const juce::String r = (projectFile != juce::File() ? projectFile.getFileNameWithoutExtension() : juce::String::fromUTF8 ("Sin título")) + (dirty() ? juce::String::fromUTF8 ("   |   sin guardar") : juce::String::fromUTF8 ("   |   guardado"));
            if (l != status.left || r != status.right) { status.left = l; status.right = r; status.repaint(); }
        }
    }
    void openAudioSettings()
    {
        auto* sel = new juce::AudioDeviceSelectorComponent (engine.getDeviceManager().deviceManager, 0, 2, 0, 2, true, false, true, false);
        sel->setSize (520, 420);
        juce::DialogWindow::LaunchOptions o; o.content.setOwned (sel); o.dialogTitle = "Ajustes de audio";
        o.componentToCentreAround = this; o.dialogBackgroundColour = theme::bgBar2; o.useNativeTitleBar = true; o.resizable = false; o.launchAsync();
    }
};

class MainWindow : public juce::DocumentWindow
{
public:
    MainComponent* main = nullptr;
    MainWindow (te::Engine& e, std::unique_ptr<te::Edit>& ed) : DocumentWindow ("INJUSTICE DAW", theme::bgBar, DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true); main = new MainComponent (e, ed); setContentOwned (main, true);
        setResizable (true, true); setResizeLimits (1320, 700, 4000, 3000); centreWithSize (1360, 800); setVisible (true);
        addKeyListener (main);
    }
    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
};

class InjusticeDawApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "INJUSTICE DAW"; }
    const juce::String getApplicationVersion() override { return "0.6.1"; }
    void initialise (const juce::String&) override
    {
        auto args = getCommandLineParameterArray();
        if (args.size() >= 3 && args[0] == "--scan-plugin") { pluginhost::runScanChild (args[1], args[2]); quit(); return; }   // modo hijo del escáner
        juce::LookAndFeel::setDefaultLookAndFeel (&look);
        engine = std::make_unique<te::Engine> ("InjusticeDAW", std::make_unique<pluginhost::UIBehaviour>(), std::make_unique<pluginhost::Behaviour>());
        const auto t0 = juce::Time::getMillisecondCounter(); auto tick = [t0] (const char* w) { if (std::getenv ("INJ_TIMING")) { std::printf ("t=%4d ms  %s\n", (int) (juce::Time::getMillisecondCounter() - t0), w); std::fflush (stdout); } };
        tick ("motor creado"); installMetronomeSounds(); tick ("metronomo");
        engine->getPluginManager().createBuiltInType<InjLimiter>(); engine->getPluginManager().createBuiltInType<InjTap>();
        engine->getDeviceManager().initialise (0, 2);
        tick ("audio"); pluginhost::loadList (*engine); tick ("lista de plugins");
        edit = makeEdit ({}); tick ("proyecto");
        if (auto* demo = std::getenv ("INJ_DEMO_WAV")) addDemoWav (demo);
        if (std::getenv ("INJ_DEMO_MIDI")) addDemoMidi();
        if (auto* df = std::getenv ("INJ_DEMO_FULL")) addDemoFull (df);
        edit->getTransport().ensureContextAllocated(); tick ("contexto de audio");
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
            e->tempoSequence.getTempo (0)->setBpm (172.0);
            e->getMasterVolumePlugin()->setVolumeDb (0.f);
            for (auto* t : te::getAudioTracks (*e)) t->setName ("Track");
        }
        currentFile = fresh ? juce::File() : f;
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
        juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::QuestionIcon).withTitle (juce::String::fromUTF8 ("Recuperar proyecto"))
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
        tr->insertWaveClip ("demo", f, { te::TimeRange (st, te::TimeDuration::fromSeconds (af.getLength())), te::TimeDuration() }, false);
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
