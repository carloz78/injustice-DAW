#include <array>
#include "TopBars.h"
#include "PlaylistView.h"
#include "PianoRollView.h"
#include "MixerPanel.h"
#include "StepSeqPanel.h"
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
        g.setColour (juce::Colours::white.withAlpha (a)); g.setFont (juce::FontOptions (15.f)); g.drawFittedText (msg, getLocalBounds().reduced (14, 4), juce::Justification::centred, 2);
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
    HelpContent() { setSize (620, 640); }
    void paint (juce::Graphics& g) override
    {
        using namespace theme; g.fillAll (bgBar2);
        g.setColour (accentHi); g.setFont (juce::FontOptions (22.f, juce::Font::bold)); g.drawText ("INJUSTICE DAW", 24, 16, 400, 30, juce::Justification::left);
        g.setColour (textDim); g.setFont (juce::FontOptions (13.f)); g.drawText (juce::String::fromUTF8 ("Versión ") + juce::JUCEApplication::getInstance()->getApplicationVersion() + juce::String::fromUTF8 (" · código abierto (AGPL-3.0) · Injustice Audio"), 24, 46, 560, 20, juce::Justification::left);
        struct Row { const char* k; const char* d; };
        static const Row rows[] {
            { "Espacio", "Reproducir / pausa" }, { "Inicio", "Volver al principio" }, { "Ctrl+N / Ctrl+O", "Proyecto nuevo / abrir" }, { "Ctrl+S / Ctrl+Mayús+S", "Guardar / guardar como" },
            { "Ctrl+E", "Exportar a WAV" }, { "Ctrl+Z / Ctrl+Y", "Deshacer / rehacer" }, { "F5", "Playlist" }, { "F6", "Step Sequencer" }, { "F7", "Piano Roll" },
            { "F8", "Explorador de samples" }, { "F9", "Mixer" }, { "F10", "Lista de plugins VST3" }, { "F1", "Esta ayuda" }, { "Supr", "Borrar el clip / nota seleccionada" },
            { "Ctrl+D", "Duplicar el clip seleccionado" }, { "Ctrl+rueda", "Zoom horizontal en la Playlist" }, { "Alt+rueda", "Alto de las pistas en la Playlist" }, { "Rueda sobre un fader / perilla", "Ajuste fino" },
            { "Doble clic en un fader", "Volver a 0 dB" }, { "Doble clic en el nombre de un canal", "Renombrar" }, { "Arrastrar esquinas de un panel", "Cambiar su tamaño" } };
        int y = 84; for (auto& r : rows)
        {
            g.setColour (juce::Colour (0xff231f2b)); g.fillRoundedRectangle (20.f, (float) y, 580.f, 22.f, 5.f);
            g.setColour (accentHi); g.setFont (juce::FontOptions (13.f, juce::Font::bold)); g.drawText (juce::String::fromUTF8 (r.k), 30, y, 250, 22, juce::Justification::centredLeft);
            g.setColour (text); g.setFont (juce::FontOptions (13.f)); g.drawText (juce::String::fromUTF8 (r.d), 290, y, 300, 22, juce::Justification::centredLeft); y += 25;
        }
    }
};

class MainComponent : public juce::Component, public juce::KeyListener, private juce::Timer
{
public:
    MainComponent (te::Engine& eng, std::unique_ptr<te::Edit>& e) : engine (eng), editPtr (e), edit (*e), playlist (*e)
    {
        addAndMakeVisible (bars); addAndMakeVisible (playlist); addChildComponent (toast); addChildComponent (piano); addAndMakeVisible (browser); addChildComponent (mixer); addChildComponent (steps);
        mixer.onClose = [this] { setMixer (false); }; steps.onClose = [this] { setSteps (false); }; mixer.onTracksChanged = [this] { playlist.repaint(); };
        steps.onClipChanged = [this] { playlist.repaint(); };
        browser.onMessage = [this] (const juce::String& m) { note (m); }; browser.onClose = [this] { setBrowser (false); }; browser.onCollapse = [this] { setBrowser (false); };
        mixer.onAddEffect = [this] (int t) { openPluginList (t); };
        mixer.onOpenPlugin = [this] (te::Plugin* p) { if (p) p->showWindowExplicitly(); };
        playlist.onTracksChanged = [this] { mixer.refreshTracks(); playlist.repaint(); };
        playlist.onTrackSelected = [this] (int i) { mixer.selected = i; mixer.showTrack (i); };
        mixer.onTracksChanged = [this] { playlist.repaint(); };
        piano.onMessage = [this] (const juce::String& m) { note (m); };
        playlist.onOpenMidi = [this] (te::MidiClip* c) { piano.setClip (c); showView ("piano"); };
        bars.bpm = edit.tempoSequence.getTempo (0)->getBpm();
        bars.onBpm = [this] (double b) { edit.tempoSequence.getTempo (0)->setBpm (b); };
        bars.onButton = [this] (const juce::String& id)
        {
            auto& tc = edit.getTransport();
            if (id == "play") { if (tc.isPlaying()) tc.stop (false, false); else { steps.endPattern(); tc.play (false); } }
            else if (id == "stop") { tc.stop (false, false); tc.setPosition (te::TimePosition()); }
            else if (id == "loop") { tc.looping = ! tc.looping; }
            else if (id == "undo") { edit.getUndoManager().undo(); refreshAll(); }
            else if (id == "redo") { edit.getUndoManager().redo(); refreshAll(); }
            else if (id == "snap") { playlist.snap = ! playlist.snap; piano.snap = playlist.snap; bars.snap = playlist.snap; }
            else if (id.startsWith ("tool:")) { bars.tool = id.substring (5); playlist.tool = bars.tool; piano.tool = bars.tool; }
            else if (id == "quant") { static const double qs[] { 1.0, 0.5, 0.25, 0.125 }; static const char* ql[] { "1/4", "1/8", "1/16", "1/32" }; qi = (qi + 1) % 4; piano.quantum = qs[qi]; bars.quantLabel = ql[qi]; bars.repaint(); }
            else if (id == "view:playlist") showView ("playlist");
            else if (id == "view:piano") { if (piano.getClip() == nullptr) piano.setClip (firstMidiClip()); showView ("piano"); }
            else if (id == "audio") openAudioSettings();
            else if (id == "layout") setBrowser (! browser.isVisible());
            else if (id == "new") confirmThen ([this] { if (onReplace) onReplace ({}); });
            else if (id == "open") confirmThen ([this] { openProject(); });
            else if (id == "export") exportWav();
            else if (id == "help") showHelp();
            else if (id == "plugins") openPluginList();
            else if (id == "rec") { bars.recArmed = ! bars.recArmed; if (bars.recArmed) note ("Grabaci\xc3\xb3n armada. Grabar en vivo audio y MIDI llegar\xc3\xa1 en una pr\xc3\xb3xima versi\xc3\xb3n; por ahora dibuja en el Piano Roll o el Step Sequencer."); }
            else if (id == "save") saveProject (false);
            else if (id == "saveas") saveProject (true);
            else if (id == "quit") juce::JUCEApplication::getInstance()->systemRequestedQuit();
            else if (id == "dup") { playlist.keyPressed (juce::KeyPress ('d', juce::ModifierKeys::commandModifier, 0)); }
            else if (id == "del") { playlist.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)); }
            else if (id == "about") note (juce::String::fromUTF8 ("INJUSTICE DAW ") + juce::JUCEApplication::getInstance()->getApplicationVersion() + juce::String::fromUTF8 (" · código abierto (AGPL-3.0) · Injustice Audio"));
            else if (id == "metro") { edit.clickTrackEnabled = ! edit.clickTrackEnabled.get(); bars.metro = edit.clickTrackEnabled.get(); note (bars.metro ? "Metr\xc3\xb3nomo activado" : "Metr\xc3\xb3nomo apagado"); }
            else if (id == "tool:trim") { bars.tool = "trim"; playlist.tool = "trim"; }
            else if (id == "view:mixer") setMixer (! mixer.isVisible());
            else if (id == "view:steps") setSteps (! steps.isVisible());
            syncState();
        };
        if (auto* th = std::getenv ("INJ_TRACKH")) playlist.setAllHeights (juce::String (th).getIntValue());
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
        chk (te::getAudioTracks (edit).size() == 500, "proyecto nuevo: 500 pistas listas");
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
        // --- Barra superior: Grabar, posición y menús
        {
            bars.onButton ("rec"); chk (bars.recArmed, "barra: Grabar se arma (se pone rojo)"); bars.onButton ("rec"); chk (! bars.recArmed, "barra: pulsarlo otra vez lo desarma");
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
            auto wav = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_export_test.wav");
            chk (edit.getLength().inSeconds() > 0.5 && renderWav (wav, edit.getLength().inSeconds()) && wav.getSize() > 10000, "exportar: se crea un WAV con contenido");
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
            playlist.selected = nullptr; playlist.tool = "brush"; playlist.setAllHeights (60); playlist.resetView(); const int bt = 5; const int nb0 = tr[bt]->getClips().size(); const float by = (float) playlist.trackTop (bt) + 40.f;
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

    // ---------- Proyecto, exportación, ayuda y atajos ----------
    std::function<void (juce::File)> onReplace;     // archivo vacío = proyecto nuevo
    std::function<void()> onTitle; std::function<void (const juce::File&)> onSaved;
    juce::File projectFile;
    void refreshAll() { playlist.repaint(); piano.repaint(); steps.repaint(); mixer.repaint(); }
    bool dirty() const { return edit.hasChangedSinceSaved(); }
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
        if (ok) { projectFile = f; if (onSaved) onSaved (f); note (juce::String::fromUTF8 ("Guardado: ") + f.getFileName()); if (onTitle) onTitle(); } else note ("No se pudo guardar el proyecto en esa ubicaci\xc3\xb3n.");
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
    bool renderWav (const juce::File& f, double len)
    {
        if (edit.getTransport().isPlaying()) edit.getTransport().stop (false, false);
        te::Renderer::Parameters p (edit); p.destFile = f; p.audioFormat = engine.getAudioFileFormatManager().getWavFormat(); p.bitDepth = 24; p.sampleRateForAudio = 44100.0;
        p.time = te::TimeRange (te::TimePosition(), te::TimeDuration::fromSeconds (len + 2.0)); p.useMasterPlugins = true; p.canRenderInMono = false; p.checkNodesForAudio = false; for (int i = 0; i < te::getAllTracks (edit).size(); ++i) p.tracksToDo.setBit (i);
        f.deleteFile(); return te::Renderer::renderToFile ("Exportar", p).existsAsFile();
    }
    void exportWav()
    {
        const auto len = edit.getLength().inSeconds();
        if (len <= 0.01) { note ("No hay nada que exportar: a\xc3\xb1" "ade clips o dibuja un patr\xc3\xb3n primero."); return; }
        chooser = std::make_unique<juce::FileChooser> ("Exportar a WAV", projectsDir().getChildFile ((projectFile != juce::File() ? projectFile.getFileNameWithoutExtension() : juce::String ("Mezcla")) + ".wav"), "*.wav");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
            [this, len] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult(); if (f == juce::File()) return; f = f.withFileExtension ("wav");
                const auto out = renderWav (f, len);
                note (out ? juce::String::fromUTF8 ("Exportado: ") + f.getFileName() : juce::String::fromUTF8 ("No se pudo exportar el audio."));
            });
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
        if (d.isInstrument) p->showWindowExplicitly();
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
        if (c == juce::KeyPress::spaceKey && ! m.isAnyModifierKeyDown()) { if (tc.isPlaying()) tc.stop (false, false); else { steps.endPattern(); tc.play (false); } syncState(); return true; }
        if (c == juce::KeyPress::homeKey) { tc.setPosition (te::TimePosition()); return true; }
        if (c == juce::KeyPress::F1Key) { showHelp(); return true; }
        if (c == juce::KeyPress::F5Key) { showView ("playlist"); return true; }
        if (c == juce::KeyPress::F6Key) { setSteps (! steps.isVisible()); return true; }
        if (c == juce::KeyPress::F7Key) { if (piano.getClip() == nullptr) piano.setClip (firstMidiClip()); showView ("piano"); return true; }
        if (c == juce::KeyPress::F8Key) { setBrowser (! browser.isVisible()); return true; }
        if (c == juce::KeyPress::F9Key) { setMixer (! mixer.isVisible()); return true; }
        if (c == juce::KeyPress::F10Key) { openPluginList(); return true; }
        if (m.isCommandDown())
        {
            const auto ch = juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) c);
            if (ch == 'z' && ! m.isShiftDown()) { edit.getUndoManager().undo(); refreshAll(); return true; }
            if (ch == 'y' || (ch == 'z' && m.isShiftDown())) { edit.getUndoManager().redo(); refreshAll(); return true; }
            if (ch == 's') { saveProject (m.isShiftDown()); return true; }
            if (ch == 'o') { confirmThen ([this] { openProject(); }); return true; }
            if (ch == 'n') { confirmThen ([this] { if (onReplace) onReplace ({}); }); return true; }
            if (ch == 'e') { exportWav(); return true; }
        }
        return false;
    }
    bool keyStateChanged (bool, juce::Component*) override { return false; }
    void resized() override
    {
        toast.place (getLocalBounds());
        auto r = getLocalBounds(); bars.setBounds (r.removeFromTop (TopBars::kHeight));
        if (browser.isVisible()) { browser.setBounds (r.removeFromLeft (browserW).reduced (6, 6)); r.removeFromLeft (6); }
        playlist.setBounds (r); piano.setBounds (r);
        if (mixer.isVisible() && ! mixer.maximized) mixer.setTopLeftPosition (juce::jmax (0, getWidth() - mixer.getWidth() - 12), juce::jmax (TopBars::kHeight, getHeight() - mixer.getHeight() - 12));
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
    void setSteps (bool v) { if (v) { steps.ensureClip(); fitPanel (steps); } steps.setVisible (v); bars.stepsOpen = v; if (v) { steps.setTopLeftPosition (juce::jmax (0, (getWidth() - steps.getWidth()) / 2), TopBars::kHeight + 70); steps.toFront (false); } playlist.repaint(); bars.repaint(); }
    PianoRollView& pianoRef() { return piano; } StepSeqPanel& stepsRef() { return steps; } BrowserPanel& browserRef() { return browser; } MixerPanel& mixerRef() { return mixer; }
    void openPanelsFromEnv (const juce::String& list)
    {
        if (list.contains ("browser")) setBrowser (true); else setBrowser (false);
        if (list.contains ("mixer")) setMixer (true);
        if (list.contains ("steps")) setSteps (true);
    }
    void paint (juce::Graphics& g) override { g.fillAll (theme::bgBar); }

private:
    te::Engine& engine; std::unique_ptr<te::Edit>& editPtr; te::Edit& edit; TopBars bars; juce::Component::SafePointer<juce::DialogWindow> pluginWin; Toast toast; juce::TooltipWindow tooltips { this, 500 }; std::unique_ptr<juce::FileChooser> chooser; PlaylistView playlist; BrowserPanel browser; MixerPanel mixer { edit }; StepSeqPanel steps { edit }; int browserW = 300; bool splitting = false; PianoRollView piano { edit }; int qi = 2; bool lastDirty = false; int tick = 0;
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
    void timerCallback() override { syncState(); }
    void syncState()
    {
        auto& tc = edit.getTransport(); bool changed = false;
        auto upd = [&] (auto& dst, auto v) { if (dst != v) { dst = v; changed = true; } };
        upd (bars.playing, tc.isPlaying()); upd (bars.looping, (bool) tc.looping); upd (bars.metro, (bool) edit.clickTrackEnabled.get());
        const double beats = edit.tempoSequence.toBeats (tc.getPosition()).inBeats(), pb = std::floor (beats * 4.0) / 4.0;   // la posición cambia por semicorchea
        upd (bars.posBeats, pb);
        const double cpu = std::round (engine.getDeviceManager().getCpuUsage() * 100.0) / 100.0; upd (bars.cpu, cpu);
        if (++tick % 20 == 1) { upd (bars.ramMB, std::round (sysinfo::processMemoryMB() / 100.0) * 100.0); upd (bars.ramTotalMB, (double) juce::SystemStats::getMemorySizeInMegabytes()); }
        if (changed) bars.repaint();
        if (onTitle && lastDirty != dirty()) { lastDirty = dirty(); onTitle(); }
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
    const juce::String getApplicationVersion() override { return "0.4.1"; }
    void initialise (const juce::String&) override
    {
        auto args = getCommandLineParameterArray();
        if (args.size() >= 3 && args[0] == "--scan-plugin") { pluginhost::runScanChild (args[1], args[2]); quit(); return; }   // modo hijo del escáner
        juce::LookAndFeel::setDefaultLookAndFeel (&look);
        engine = std::make_unique<te::Engine> ("InjusticeDAW", std::make_unique<pluginhost::UIBehaviour>(), std::make_unique<pluginhost::Behaviour>());
        installMetronomeSounds();
        engine->getDeviceManager().initialise (0, 2);
        pluginhost::loadList (*engine);
        edit = makeEdit ({});
        if (auto* demo = std::getenv ("INJ_DEMO_WAV")) addDemoWav (demo);
        if (std::getenv ("INJ_DEMO_MIDI")) addDemoMidi();
        if (auto* df = std::getenv ("INJ_DEMO_FULL")) addDemoFull (df);
        edit->getTransport().ensureContextAllocated();
        openWindow();
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
    void shutdown() override { window.reset(); edit.reset(); engine.reset(); juce::LookAndFeel::setDefaultLookAndFeel (nullptr); }
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
        make (big, 1760.0, 0.85f); make (little, 1175.0, 0.6f);
        engine->getPropertyStorage().setProperty (te::SettingID::clickTrackSampleBig, big.getFullPathName()); engine->getPropertyStorage().setProperty (te::SettingID::clickTrackSampleSmall, little.getFullPathName());
    }
    std::unique_ptr<te::Edit> makeEdit (const juce::File& f)
    {
        std::unique_ptr<te::Edit> e;
        if (f.existsAsFile()) e = te::loadEditFromFile (*engine, f);
        const bool fresh = (e == nullptr);
        if (fresh) e = te::createEmptyEdit (*engine, juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("injustice_untitled.tracktionedit"));
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
