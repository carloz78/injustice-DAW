#include "TopBars.h"
#include "PlaylistView.h"
#include "PianoRollView.h"
#include "MixerPanel.h"
#include "StepSeqPanel.h"
#include "BrowserPanel.h"

class MainComponent : public juce::Component, private juce::Timer
{
public:
    MainComponent (te::Engine& eng, std::unique_ptr<te::Edit>& e) : engine (eng), editPtr (e), edit (*e), playlist (*e)
    {
        addAndMakeVisible (bars); addAndMakeVisible (playlist); addChildComponent (piano); addAndMakeVisible (browser); addChildComponent (mixer); addChildComponent (steps);
        mixer.onClose = [this] { setMixer (false); }; steps.onClose = [this] { setSteps (false); }; mixer.onTracksChanged = [this] { playlist.repaint(); };
        steps.onClipChanged = [this] { playlist.repaint(); };
        browser.onClose = [this] { setBrowser (false); }; browser.onCollapse = [this] { setBrowser (false); };
        playlist.onOpenMidi = [this] (te::MidiClip* c) { piano.setClip (c); showView ("piano"); };
        bars.bpm = edit.tempoSequence.getTempo (0)->getBpm();
        bars.onBpm = [this] (double b) { edit.tempoSequence.getTempo (0)->setBpm (b); };
        bars.onButton = [this] (const juce::String& id)
        {
            auto& tc = edit.getTransport();
            if (id == "play") { if (tc.isPlaying()) tc.stop (false, false); else tc.play (false); }
            else if (id == "stop") { tc.stop (false, false); tc.setPosition (te::TimePosition()); }
            else if (id == "loop") { tc.looping = ! tc.looping; }
            else if (id == "undo") edit.getUndoManager().undo();
            else if (id == "redo") edit.getUndoManager().redo();
            else if (id == "snap") { playlist.snap = ! playlist.snap; piano.snap = playlist.snap; bars.snap = playlist.snap; }
            else if (id.startsWith ("tool:")) { bars.tool = id.substring (5); playlist.tool = bars.tool; piano.tool = bars.tool; }
            else if (id == "quant") { static const double qs[] { 1.0, 0.5, 0.25, 0.125 }; static const char* ql[] { "1/4", "1/8", "1/16", "1/32" }; qi = (qi + 1) % 4; piano.quantum = qs[qi]; bars.quantLabel = ql[qi]; bars.repaint(); }
            else if (id == "view:playlist") showView ("playlist");
            else if (id == "view:piano") { if (piano.getClip() == nullptr) piano.setClip (firstMidiClip()); showView ("piano"); }
            else if (id == "audio") openAudioSettings();
            else if (id == "layout") setBrowser (! browser.isVisible());
            else if (id == "view:mixer") setMixer (! mixer.isVisible());
            else if (id == "view:steps") setSteps (! steps.isVisible());
            syncState();
        };
        if (auto* v = std::getenv ("INJ_VIEW")) if (juce::String (v) == "piano") { piano.setClip (firstMidiClip()); showView ("piano"); }
        if (std::getenv ("INJ_SELFTEST")) juce::Timer::callAfterDelay (500, [this] { const int f = runSelfTest(); std::fflush (stdout); if (f >= 0) std::_Exit (f == 0 ? 0 : 1); });
        startTimerHz (20);
    }
    // Autoprueba de edición (INJ_SELFTEST=1): simula clics sobre el Piano Roll y comprueba el resultado.
    int selfFails = 0;
    int selfTestStage2()
    {
        int fails = selfFails; auto chk = [&] (bool ok, const char* what) { std::printf ("%s  %s\n", ok ? "OK  " : "FALLO", what); if (! ok) ++fails; };
        {
            auto out = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_steps_test.wav");
            const bool ok = te::Renderer::renderToFile (edit, out, false); chk (ok, "step: el render offline termina");
            juce::AudioFormatManager fmg; fmg.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> rd (fmg.createReaderFor (out));
            if (rd)
            {
                juce::AudioBuffer<float> b ((int) rd->numChannels, (int) rd->lengthInSamples); rd->read (&b, 0, b.getNumSamples(), 0, true, true);
                auto peak = [&] (double t0, double t1) { float p = 0; for (int c = 0; c < b.getNumChannels(); ++c) for (int i = (int) (t0 * rd->sampleRate); i < std::min (b.getNumSamples(), (int) (t1 * rd->sampleRate)); ++i) p = std::max (p, std::abs (b.getSample (c, i))); return p; };
                const double step = 60.0 / 172.0 / 4.0;
                for (int s : { 0, 4, 8, 12 }) chk (peak (s * step, s * step + 0.04) > 0.05f, ("step: suena el kick en el paso " + juce::String (s + 1)).toRawUTF8());
                for (int s : { 2, 6, 10 }) chk (peak (s * step + 0.02, s * step + 0.06) < 0.01f, ("step: silencio en el paso " + juce::String (s + 1)).toRawUTF8());
            }
            else chk (false, "step: se puede leer el WAV renderizado");
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
        // --- Mixer
        setMixer (true); mixer.setSize (mixer.getWidth(), mixer.getHeight());
        auto tracks = te::getAudioTracks (edit); auto mev = [&] (juce::Point<float> p, juce::ModifierKeys mk) { return juce::MouseEvent (src, p, mk, 1.f, 0.f, 0.f, 0.f, 0.f, &mixer, &mixer, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false); };
        const float sx = (float) (MixerPanel::kX0 + 2 * MixerPanel::kStride) + 38.f;   // canal 3
        mixer.mouseDown (mev ({ sx - 4, (float) MixerPanel::kTop + MixerPanel::kStripH - 26.f }, L)); mixer.mouseUp (mev ({ sx, 0 }, 0));
        chk (tracks[2]->isSolo (false), "mixer: S activa el solo del canal 3");
        mixer.mouseDown (mev ({ sx + 31, (float) MixerPanel::kTop + MixerPanel::kStripH - 26.f }, L)); mixer.mouseUp (mev ({ sx, 0 }, 0));
        chk (tracks[2]->isMuted (false), "mixer: M silencia el canal 3");
        const float fy0 = (float) MixerPanel::kTop + 112.f, fh = (float) MixerPanel::kStripH - 112.f - 58.f;
        mixer.mouseDown (mev ({ sx, fy0 + fh * 0.5f }, L)); mixer.mouseUp (mev ({ sx, fy0 + fh * 0.5f }, 0));
        chk (std::abs (tracks[2]->getVolumePlugin()->getVolumeDb() - te::volumeFaderPositionToDB (0.5f)) < 0.5f, "mixer: arrastrar el fader cambia el volumen real");
        mixer.mouseDoubleClick (mev ({ sx, fy0 + fh * 0.5f }, L));
        chk (std::abs (tracks[2]->getVolumePlugin()->getVolumeDb()) < 0.01f, "mixer: doble clic en el fader vuelve a 0 dB");
        tracks[2]->setSolo (false); tracks[2]->setMute (false);
        // --- Step Sequencer: clic en celdas y render real del patrón con un sample
        setSteps (true); steps.setBounds (0, 60, steps.getWidth(), steps.getHeight());
        auto stc = steps.getClip(); chk (stc != nullptr, "step: se crea el clip de patrón en la Playlist");
        for (int s = 0; s < 16; ++s) steps.setStep (0, s, false);
        auto sev = [&] (juce::Point<float> p, juce::ModifierKeys mk) { return juce::MouseEvent (src, p, mk, 1.f, 0.f, 0.f, 0.f, 0.f, &steps, &steps, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false); };
        const float cw = (float) (steps.getWidth() - 336 - 26) / 16.f;
        for (int s : { 0, 4, 8, 12 }) { juce::Point<float> p (336 + s * cw + cw * .5f, (float) (FloatingPanel::kHead + 96) + 38.f); steps.mouseDown (sev (p, L)); steps.mouseUp (sev (p, 0)); }
        chk (steps.getStep (0, 0) && steps.getStep (0, 4) && steps.getStep (0, 8) && steps.getStep (0, 12) && ! steps.getStep (0, 2), "step: los clics marcan los pasos 1, 5, 9 y 13");
        selfFails = fails;
        if (auto* sf = std::getenv ("INJ_SAMPLE_FILE"))
        {
            steps.assignSample (0, juce::File (sf));
            for (int r = 1; r < 4; ++r) for (int s = 0; s < 16; ++s) steps.setStep (r, s, false);
            juce::Timer::callAfterDelay (1500, [this] { const int f = selfTestStage2(); std::fflush (stdout); std::_Exit (f == 0 ? 0 : 1); });
            return -1;
        }
        std::printf (fails == 0 ? "TODO OK\n" : "%d FALLOS\n", fails); return fails;
    }
    void resized() override
    {
        auto r = getLocalBounds(); bars.setBounds (r.removeFromTop (TopBars::kHeight));
        if (browser.isVisible()) { browser.setBounds (r.removeFromLeft (browserW).reduced (6, 6)); r.removeFromLeft (6); }
        playlist.setBounds (r); piano.setBounds (r);
        if (mixer.isVisible() && ! mixer.maximized) mixer.setTopLeftPosition (juce::jmax (0, getWidth() - mixer.getWidth() - 12), juce::jmax (TopBars::kHeight, getHeight() - mixer.getHeight() - 12));
    }
    void mouseDown (const juce::MouseEvent& e) override { splitting = browser.isVisible() && e.x >= browserW - 2 && e.x <= browserW + 8 && e.y > TopBars::kHeight; }
    void mouseDrag (const juce::MouseEvent& e) override { if (splitting) { browserW = juce::jlimit (220, 520, e.x); resized(); repaint(); } }
    void mouseMove (const juce::MouseEvent& e) override { setMouseCursor (browser.isVisible() && e.x >= browserW - 2 && e.x <= browserW + 8 && e.y > TopBars::kHeight ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor); }
    void setBrowser (bool v) { browser.setVisible (v); bars.browserOpen = v; bars.repaint(); resized(); repaint(); }
    void setMixer (bool v) { mixer.setVisible (v); bars.mixerOpen = v; if (v) { mixer.setTopLeftPosition (juce::jmax (0, getWidth() - mixer.getWidth() - 12), juce::jmax (TopBars::kHeight, getHeight() - mixer.getHeight() - 12)); mixer.toFront (false); } bars.repaint(); }
    void setSteps (bool v) { if (v) steps.ensureClip(); steps.setVisible (v); bars.stepsOpen = v; if (v) { steps.setTopLeftPosition (juce::jmax (0, (getWidth() - steps.getWidth()) / 2), TopBars::kHeight + 70); steps.toFront (false); } playlist.repaint(); bars.repaint(); }
    PianoRollView& pianoRef() { return piano; } StepSeqPanel& stepsRef() { return steps; } BrowserPanel& browserRef() { return browser; } MixerPanel& mixerRef() { return mixer; }
    void openPanelsFromEnv (const juce::String& list)
    {
        if (list.contains ("browser")) setBrowser (true); else setBrowser (false);
        if (list.contains ("mixer")) setMixer (true);
        if (list.contains ("steps")) setSteps (true);
    }
    void paint (juce::Graphics& g) override { g.fillAll (theme::bgBar); }

private:
    te::Engine& engine; std::unique_ptr<te::Edit>& editPtr; te::Edit& edit; TopBars bars; PlaylistView playlist; BrowserPanel browser; MixerPanel mixer { edit }; StepSeqPanel steps { edit }; int browserW = 300; bool splitting = false; PianoRollView piano { edit }; int qi = 2;
    te::MidiClip* firstMidiClip()
    {
        for (auto* t : te::getAudioTracks (edit)) for (auto* c : t->getClips()) if (auto* mc = dynamic_cast<te::MidiClip*> (c)) return mc;
        return nullptr;
    }
    void showView (const juce::String& v)
    {
        const bool pr = (v == "piano"); bars.view = v; bars.pianoMode = pr; playlist.setVisible (! pr); piano.setVisible (pr);
        bars.crumb = pr ? "Piano Roll - " + (piano.getClip() ? piano.getClip()->getName() : juce::String ("sin clip")) : juce::String ("Playlist - Arrangement");
        if (pr && bars.tool == "cut") { bars.tool = "select"; piano.tool = "select"; }
        bars.resized(); bars.repaint();
    }
    void timerCallback() override { syncState(); }
    void syncState()
    {
        auto& tc = edit.getTransport();
        if (bars.playing != tc.isPlaying() || bars.looping != (bool) tc.looping) { bars.playing = tc.isPlaying(); bars.looping = tc.looping; bars.repaint(); }
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
    }
    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
};

class InjusticeDawApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "INJUSTICE DAW"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    void initialise (const juce::String&) override
    {
        engine = std::make_unique<te::Engine> ("InjusticeDAW");
        engine->getDeviceManager().initialise (0, 2);
        edit = te::createEmptyEdit (*engine, juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("injustice_untitled.tracktionedit"));
        edit->tempoSequence.getTempo (0)->setBpm (172.0);
        edit->ensureNumberOfAudioTracks (10);
        edit->getMasterVolumePlugin()->setVolumeDb (0.f);
        int i = 1; for (auto* t : te::getAudioTracks (*edit)) t->setName ("Track");
        (void) i;
        if (auto* demo = std::getenv ("INJ_DEMO_WAV"))   // solo pruebas: coloca un clip en la pista 2, compás 3
        {
            auto tr = te::getAudioTracks (*edit)[1]; juce::File f (demo); te::AudioFile af (*engine, f);
            const auto st = te::TimePosition::fromSeconds (edit->tempoSequence.toTime (te::BeatPosition::fromBeats (8.0)).inSeconds());
            tr->insertWaveClip ("demo", f, { te::TimeRange (st, te::TimeDuration::fromSeconds (af.getLength())), te::TimeDuration() }, false);
        }
        if (std::getenv ("INJ_DEMO_MIDI"))   // solo pruebas: melodía de ejemplo en la pista 1
        {
            auto tr = te::getAudioTracks (*edit)[0];
            auto c = tr->insertMIDIClip ("Lead", te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (edit->tempoSequence.toTime (te::BeatPosition::fromBeats (32.0)).inSeconds())), nullptr);
            const int notes[] { 60, 62, 64, 67, 71, 65, 64, 61, 59, 60, 62, 64, 67, 71, 69, 65 }; const double starts[] { 0, 1, 1.5, 2, 3, 4, 5, 6, 7, 8, 9, 9.5, 10, 11, 12, 13 };
            for (int i = 0; i < 16; ++i) c->getSequence().addNote (notes[i], te::BeatPosition::fromBeats (starts[i]), te::BeatDuration::fromBeats (i % 3 == 1 ? .5 : 1.0), 60 + (i * 37) % 60, 0, nullptr);
        }
        edit->getTransport().ensureContextAllocated();
        window = std::make_unique<MainWindow> (*engine, edit);
        if (auto* pn = std::getenv ("INJ_PANELS"))   // solo pruebas: abre paneles y datos de ejemplo para las capturas
        {
            auto* mc = window->main;
            if (auto* sd = std::getenv ("INJ_SAMPLES")) { mc->browserRef().setRoot (juce::File (sd)); mc->browserRef().expandAll (2); }
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
        }
        // Modo de prueba: INJ_SHOT=ruta.png guarda una captura de la ventana y sale (sirve para revisar el diseño sin pantalla).
        if (auto* shot = std::getenv ("INJ_SHOT"))
        {
            juce::String path (shot);
            juce::Timer::callAfterDelay (1500, [this, path]
            {
                auto img = window->getContentComponent()->createComponentSnapshot (window->getContentComponent()->getLocalBounds(), true, 1.0f);
                juce::PNGImageFormat png; juce::File f (path); f.deleteFile();
                if (auto os = f.createOutputStream()) png.writeImageToStream (img, *os);
                quit();
            });
        }
    }
    void shutdown() override { window.reset(); edit.reset(); engine.reset(); }
    void systemRequestedQuit() override { quit(); }
private:
    std::unique_ptr<te::Engine> engine; std::unique_ptr<te::Edit> edit; std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION (InjusticeDawApp)
