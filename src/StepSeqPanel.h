#pragma once
#include "FloatingPanel.h"
#include "TrackUtil.h"
#include <tracktion_engine/tracktion_engine.h>
namespace te = tracktion;

// Step Sequencer: edita un StepClip real de la Playlist (clip de patrón). Cada fila reproduce un sample con el sampler del motor.
class StepSeqPanel : public FloatingPanel, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    std::function<void()> onClipChanged; std::function<void (te::AudioTrack*, int)> onAudition;
    explicit StepSeqPanel (te::Edit& e) : edit (e)
    {
        title = "Step Sequencer"; icon = icons::Steps; scale = .84f; setDesign (1130, designHeight (4)); startTimerHz (60); edit.engine.getGrooveTemplateManager().useParameterizedGrooves (true);
    }

    // Crea (si hace falta) el clip de patrón en la primera pista libre, con su sampler.
    // Los samples del secuenciador suenan completos (no se cortan al terminar el paso).
    void openAll() { if (sampler) { for (int i = 0; i < sampler->getNumSounds(); ++i) if (! sampler->isSoundOpenEnded (i)) sampler->setSoundOpenEnded (i, true); applyAudible(); } }
    // Filas 1-4 con sonidos 909 del paquete la primera vez que se crea el patrón (una sola vez por clip)
    void assignDefaults (const juce::File& pack)
    {
        ensureClip(); if (! clip || (bool) clip->state.getProperty ("injDefaults", false) || ! pack.isDirectory()) return;
        static const char* f[] { "Kicks/Kick Dash 01.wav", "Claps/Clap 909 01.wav", "Hats/Hat Corto 01.wav", "Hats/Hat Abierto 01.wav" };
        for (int r = 0; r < 4 && r < clip->getChannels().size(); ++r) if (! hasSample (r) && pack.getChildFile (f[r]).existsAsFile()) assignSample (r, pack.getChildFile (f[r]));
        clip->state.setProperty ("injDefaults", true, nullptr); repaint();
    }
    // Version por pasos de assignDefaults (una fila por llamada) para no congelar la ventana al abrir: devuelve true cuando ya no quedan filas.
    bool assignDefaultsStep (const juce::File& pack, int r)
    {
        if (! clip || (bool) clip->state.getProperty ("injDefaults", false) || ! pack.isDirectory()) return true;
        static const char* f[] { "Kicks/Kick Dash 01.wav", "Claps/Clap 909 01.wav", "Hats/Hat Corto 01.wav", "Hats/Hat Abierto 01.wav" };
        if (r < 4 && r < clip->getChannels().size()) { if (! hasSample (r) && pack.getChildFile (f[r]).existsAsFile()) assignSample (r, pack.getChildFile (f[r])); return false; }
        clip->state.setProperty ("injDefaults", true, nullptr); repaint(); return true;
    }
    bool hasClip() { validate(); return clip != nullptr; }
    bool inPatternMode() const { return patternMode; }
    // Reproduce solo el patrón en bucle: silencia las demás pistas mientras dura y lo restaura al parar.
    void startPattern()
    {
        ensureClip(); if (! clip) return; auto& tc = edit.getTransport();
        endPattern(); if (tc.isPlaying()) tc.stop (false, false);
        auto* host = dynamic_cast<te::AudioTrack*> (clip->getTrack()); if (host == nullptr) return;
        oldLoop = (bool) tc.looping; oldRange = tc.getLoopRange(); oldSolo.clear();
        for (auto* t : te::getAudioTracks (edit)) { oldSolo.push_back ({ t, t->isSolo (false) }); t->setSolo (t == host); }
        const auto s = clip->getPosition().getStart();
        tc.setLoopRange (te::TimeRange (s, edit.tempoSequence.toTime (edit.tempoSequence.toBeats (s) + te::BeatDuration::fromBeats (stepCount() * stepLen()))));
        tc.looping = true; patternMode = true; tc.setPosition (s); tc.play (false);
    }
    void endPattern()
    {
        if (! patternMode) return; patternMode = false; auto& tc = edit.getTransport();
        for (auto& [t, so] : oldSolo) for (auto* a : te::getAudioTracks (edit)) if (a == t) a->setSolo (so);
        oldSolo.clear(); tc.looping = oldLoop; tc.setLoopRange (oldRange);
    }
    // El sampler se crea solo cuando hace falta (al poner el primer sample): crear un plugin reconstruye el audio y tardaba al abrir.
    void ensureSampler()
    {
        ensureClip(); if (! clip || sampler != nullptr) return; auto* host = dynamic_cast<te::AudioTrack*> (clip->getTrack()); if (host == nullptr) return;
        sampler = findSampler (host); if (sampler != nullptr) return;
        samplerPtr = edit.getPluginCache().createNewPlugin (te::SamplerPlugin::xmlTypeName, {});
        host->pluginList.insertPlugin (samplerPtr, 0, nullptr); sampler = dynamic_cast<te::SamplerPlugin*> (samplerPtr.get());
    }
    void validate() { if (clip != nullptr && clip->getTrack() == nullptr) { clip = nullptr; sampler = nullptr; samplerPtr = nullptr; repaint(); } }   // el clip pudo borrarse con Deshacer
    void ensureClip()
    {
        validate();
        if (clip != nullptr) return;
        auto tracks = te::getAudioTracks (edit);
        for (auto* t : tracks) for (auto* c : t->getClips()) if (auto* sc = dynamic_cast<te::StepClip*> (c)) { clip = sc; sampler = findSampler (t); openAll(); return; }
        te::AudioTrack* host = nullptr; for (auto* t : tracks) if (t->getClips().isEmpty()) { host = t; break; }
        if (host == nullptr) { if (tracks.isEmpty()) return; host = tracks.getLast(); }
        auto len = edit.tempoSequence.toTime (te::BeatPosition::fromBeats (16.0));
        clip = dynamic_cast<te::StepClip*> (host->insertNewClip (te::TrackItem::Type::step, juce::String::fromUTF8 ("Patrón"), te::TimeRange (te::TimePosition(), len), nullptr));
        if (clip == nullptr) return;
        static const char* names[] { "Kick", "Clap", "Hat", "Open Hat", "Perc 5", "Perc 6", "Perc 7", "Perc 8" };
        for (int i = 0; i < clip->getChannels().size() && i < 8; ++i) { auto& ch = *clip->getChannels()[i]; ch.name = names[i]; ch.noteNumber = 36 + i; }
        clip->getPattern (0).setNumNotes (16); rows = 4;
        if (onClipChanged) onClipChanged();
    }
    // Para la captura de pantalla y pruebas
    void setStep (int row, int step, bool on) { ensureClip(); if (clip) clip->getPattern (0).setNote (row, step, on); }
    bool getStep (int row, int step) { ensureClip(); return clip && clip->getPattern (0).getNote (row, step); }
    te::StepClip* getClip() { ensureClip(); return clip.get(); }
    int numPatterns() { ensureClip(); return patterns().size(); }
    void newPattern() { ensureClip(); addPattern (false); }
    void runPatternOp (int id) { ensureClip(); patternOp (id); }
    void pickPattern (int i) { ensureClip(); selectPattern (i); }
    void setRowVolume (int r, float db) { setVol (r, db); }
    void setRowPan (int r, float pn) { setPan (r, pn); }
    float rowVolume (int r) { const int i = soundForRow (r); return i >= 0 && sampler ? sampler->getSoundGainDb (i) : 0.f; }
    void setStepVelocity (int r, int st, int v) { ensureClip(); if (clip) clip->getPattern (0).setVelocity (r, st, v); }
    void assignSample (int row, const juce::File& f)
    {
        ensureSampler(); if (! clip || ! sampler) return;
        int existing = soundForRow (row); if (existing >= 0) sampler->removeSound (existing);
        const auto err = sampler->addSound (f.getFullPathName(), f.getFileNameWithoutExtension(), 0.0, 0.0, 1.0f);
        if (err.isEmpty()) { const int note = clip->getChannels()[row]->noteNumber; const int idx = sampler->getNumSounds() - 1; sampler->setSoundParams (idx, note, note, note); sampler->setSoundOpenEnded (idx, true); }   // abierto: el sample suena completo aunque el paso sea corto
        applyAudible(); repaint();
    }


    // ---- API pública para las pruebas y las capturas ----
    juce::Point<float> stepCentre (int row, int s) { return stepR (row, s).getCentre(); }
    juce::Point<float> laneTabCentre (int i) { return laneTabR (i).getCentre(); }
    juce::Point<float> laneCentre (int s, float frac) { auto r = laneR(); return { r.getX() + (s + .5f) * r.getWidth() / (float) stepCount(), r.getBottom() - 3.f - frac * (r.getHeight() - 6.f) }; }
    void setStepGate (int r, int st, double g) { ensureClip(); if (clip) clip->getPattern (0).setGate (r, st, g); }
    double getStepGate (int r, int st) { ensureClip(); return clip ? clip->getPattern (0).getGate (r, st) : 1.0; }
    void setStepProb (int r, int st, float p) { ensureClip(); if (clip) clip->getPattern (0).setProbability (r, st, p); }
    float getStepProb (int r, int st) { ensureClip(); return clip ? clip->getPattern (0).getProbability (r, st) : 1.f; }
    int getStepVelocity (int r, int st) { ensureClip(); return clip ? clip->getPattern (0).getVelocity (r, st) : 0; }
    void setSwingAll (float v) { ensureClip(); if (! clip) return; clip->state.setProperty ("injSwing", v, nullptr); for (int r = 0; r < rowCount(); ++r) setSwingRow (r, v); repaint(); }
    void setSwingRow (int r, float v) { for (auto* c : patterns()) if (r < c->getChannels().size()) applySwing (*c->getChannels()[r], v); }
    float rowSwing (int r) { return clip && r < clip->getChannels().size() ? ((clip->getChannels()[r]->grooveTemplate.get().isNotEmpty()) ? (float) clip->getChannels()[r]->grooveStrength : 0.f) : 0.f; }
    juce::String rowGroove (int r) { return clip && r < clip->getChannels().size() ? clip->getChannels()[r]->grooveTemplate.get() : juce::String(); }
    void setSolo (int r, bool on) { ensureClip(); if (clip && clip->getTrack()) { clip->getTrack()->state.setProperty ("injSolo" + juce::String (r), on, nullptr); applyAudible(); repaint(); } }
    bool isSolo (int r) { return clip && clip->getTrack() && (bool) clip->getTrack()->state.getProperty ("injSolo" + juce::String (r), false); }
    bool rowAudible (int r) { return audible (r); }
    bool rowCut (int r) { return clip && clip->getTrack() && (bool) clip->getTrack()->state.getProperty ("injCut" + juce::String (r), false); }
    void setRowCut (int r, bool on) { ensureClip(); if (clip && clip->getTrack()) { clip->getTrack()->state.setProperty ("injCut" + juce::String (r), on, nullptr); applyAudible(); repaint(); } }
    void setRowLength (int r, int n) { ensureClip(); if (! clip) return; clip->state.setProperty ("injRowLen" + juce::String (r), n >= stepCount() ? 0 : n, nullptr); applyRowLen (r); repaint(); }
    int rowLength (int r) { return rowLen (r); }
    void copyRow (int r) { ensureClip(); if (! clip) return; auto pat = clip->getPattern (0); rowClip.notes = pat.getChannel (r); rowClip.vel = pat.getVelocities (r); rowClip.gate = pat.getGates (r); rowClip.prob = pat.getProbabilities (r); rowClip.n = stepCount(); rowClip.valid = true; }
    void pasteRow (int r)
    {
        ensureClip(); if (! clip || ! rowClip.valid) return; edit.getUndoManager().beginNewTransaction ("Pegar fila"); auto pat = clip->getPattern (0); const int n = stepCount();
        juce::BigInteger bits; juce::Array<int> vel; juce::Array<double> gate; juce::Array<float> prob;
        for (int k = 0; k < n; ++k) { const int sk = k % std::max (1, rowClip.n); bits.setBit (k, rowClip.notes[sk]); vel.add (sk < rowClip.vel.size() ? rowClip.vel[sk] : 100); gate.add (sk < rowClip.gate.size() ? rowClip.gate[sk] : 1.0); prob.add (sk < rowClip.prob.size() ? rowClip.prob[sk] : 1.f); }
        pat.setChannel (r, bits); pat.setVelocities (r, vel); pat.setGates (r, gate); pat.setProbabilities (r, prob);
        if (onClipChanged) onClipChanged(); repaint();
    }
    void swapRows (int a, int b)
    {
        ensureClip(); if (! clip) return; const int nch = clip->getChannels().size(); if (a == b || a < 0 || b < 0 || a >= nch || b >= nch) return;
        for (auto* c : patterns())
        {
            auto& A = *c->getChannels()[a]; auto& B = *c->getChannels()[b]; auto pat = c->getPattern (0);
            { const auto t = A.name.get(); A.name = B.name.get(); B.name = t; } { const int t = A.noteNumber; A.noteNumber = (int) B.noteNumber; B.noteNumber = t; }
            { const auto t = A.grooveTemplate.get(); A.grooveTemplate = B.grooveTemplate.get(); B.grooveTemplate = t; } { const float t = A.grooveStrength; A.grooveStrength = (float) B.grooveStrength; B.grooveStrength = t; }
            { const auto na = pat.getChannel (a), nb = pat.getChannel (b); pat.setChannel (a, nb); pat.setChannel (b, na); }
            { const auto va = pat.getVelocities (a), vb = pat.getVelocities (b); pat.setVelocities (a, vb); pat.setVelocities (b, va); }
            { const auto ga = pat.getGates (a), gb = pat.getGates (b); pat.setGates (a, gb); pat.setGates (b, ga); }
            { const auto pa = pat.getProbabilities (a), pb = pat.getProbabilities (b); pat.setProbabilities (a, pb); pat.setProbabilities (b, pa); }
            for (const char* k : { "injRowLen" }) { const auto ka = juce::String (k) + juce::String (a), kb = juce::String (k) + juce::String (b); const auto va = c->state.getProperty (ka, 0), vb = c->state.getProperty (kb, 0); c->state.setProperty (ka, vb, nullptr); c->state.setProperty (kb, va, nullptr); }
        }
        if (auto* tr = clip->getTrack()) for (const char* k : { "injOff", "injSolo", "injCut" }) { const auto ka = juce::String (k) + juce::String (a), kb = juce::String (k) + juce::String (b); const auto va = tr->state.getProperty (ka, false), vb = tr->state.getProperty (kb, false); tr->state.setProperty (ka, vb, nullptr); tr->state.setProperty (kb, va, nullptr); }
        applyAudible(); if (onClipChanged) onClipChanged(); repaint();
    }
    double stepLen() { if (! clip) return 0.25; const double l = clip->getPattern (0).getNoteLength().inBeats(); return l > 1e-3 ? l : 0.25; }
    void setResolution (double beats)
    {
        ensureClip(); if (! clip) return; for (auto* c : patterns()) c->getPattern (0).setNoteLength (te::BeatDuration::fromBeats (beats));
        const float sw = (float) (double) clip->state.getProperty ("injSwing", 0.0); for (int r = 0; r < rowCount(); ++r) if (rowSwing (r) > .01f) setSwingRow (r, rowSwing (r)); juce::ignoreUnused (sw);
        if (patternMode) { edit.getTransport().stop (false, false); endPattern(); } if (onClipChanged) onClipChanged(); repaint();
    }
    int rowCount() const { return rows; }
    void dropForTest (const juce::StringArray& f, int x, int y) { filesDropped (f, x, y); }
    int selectedRow() const { return selRow; }
    void selectRow (int r) { selRow = juce::jlimit (0, rows - 1, r); repaint(); }
    // Comandos de la fila elegida (paleta Ctrl+K): rotar a izquierda/derecha, invertir pasos, vaciar, aleatoria y euclídea. Devuelve cuántos pasos quedan activos.
    int rowOp (const juce::String& op)
    {
        ensureClip(); if (! clip) return -1; const int n = stepCount(), r = juce::jlimit (0, rows - 1, selRow); if (n <= 0) return -1;
        struct S { bool on; int vel; double gate; float prob; }; std::vector<S> old ((size_t) n);
        auto pat = clip->getPattern (0);
        for (int i = 0; i < n; ++i) old[(size_t) i] = { pat.getNote (r, i), pat.getVelocity (r, i), pat.getGate (r, i), pat.getProbability (r, i) };
        edit.getUndoManager().beginNewTransaction (juce::String::fromUTF8 ("Fila: ") + op); std::vector<S> out = old; juce::Random rnd (juce::Time::getMillisecondCounter());
        if (op == "rotl" || op == "rotr") { const int d = op == "rotl" ? -1 : 1; for (int i = 0; i < n; ++i) out[(size_t) ((i + d + n) % n)] = old[(size_t) i]; }
        else if (op == "invert") { for (auto& o : out) { o.on = ! o.on; if (o.on && o.vel <= 0) o.vel = 100; } }
        else if (op == "clear") { for (auto& o : out) o.on = false; }
        else if (op == "half") { const int h = n / 2; if (h < 1) return -1; for (int i = 0; i < h && i + h < n; ++i) out[(size_t) (i + h)] = old[(size_t) i]; }
        else if (op == "sparse" || op == "dense") { const float pr = op == "sparse" ? .2f : .5f; for (auto& o : out) { o.on = rnd.nextFloat() < pr; if (o.on && o.vel <= 0) o.vel = 70 + rnd.nextInt (58); } }
        else if (op == "accent") { for (int i = 0; i < n; ++i) if (out[(size_t) i].on) out[(size_t) i].vel = i % 4 == 0 ? 127 : 90; }
        else if (op == "fourfloor" || op == "offbeat") { const int ph = op == "fourfloor" ? 0 : 2; for (int i = 0; i < n; ++i) { out[(size_t) i].on = i % 4 == ph; if (out[(size_t) i].on && out[(size_t) i].vel <= 0) out[(size_t) i].vel = 110; } }
        else if (op == "vel100") { for (auto& o : out) if (o.on) o.vel = 100; }
        else if (op == "random") { for (auto& o : out) { o.on = rnd.nextFloat() < .35f; if (o.on) o.vel = 70 + rnd.nextInt (58); } }
        else if (op.startsWith ("euclid:")) { const int k = juce::jlimit (0, n, op.fromFirstOccurrenceOf (":", false, false).getIntValue()); for (int i = 0; i < n; ++i) { out[(size_t) i].on = k > 0 && ((i * k) % n) < k; if (out[(size_t) i].on && out[(size_t) i].vel <= 0) out[(size_t) i].vel = 100; } }
        else return -1;
        int on = 0; for (int i = 0; i < n; ++i) { pat.setNote (r, i, out[(size_t) i].on); if (out[(size_t) i].on) { ++on; pat.setVelocity (r, i, out[(size_t) i].vel); pat.setGate (r, i, out[(size_t) i].gate); pat.setProbability (r, i, out[(size_t) i].prob); } }
        if (onClipChanged) onClipChanged(); repaint(); return on;
    }
    bool hasRowSample (int r) { ensureClip(); return hasSample (r); }
    int stepsCount() { ensureClip(); return stepCount(); }
    void setStepCount (int n) { ensureClip(); if (clip) setSteps (n); }

protected:
    static constexpr int kRowH = 58, kRows0 = 80, kNameX = 96, kNameW = 214, kStepX0 = 326, kPanel = 164, kMaxPat = 8, kBar = 48;
    static constexpr int designHeight (int nr) { return kHead + kRows0 + nr * kRowH + 8 + kPanel + 18; }
    int rowsY() const { return kHead + kRows0; }
    int panelY() { return rowsY() + rows * kRowH + 8; }
    int topY() const { return kHead + 12; }
    juce::Rectangle<float> playR() { return { 16.f, (float) topY(), 64.f, (float) kBar }; }
    juce::Rectangle<float> stopR() { return { 88.f, (float) topY(), 48.f, (float) kBar }; }
    juce::Rectangle<float> stepsR() { return { 150.f, (float) topY(), 118.f, (float) kBar }; }
    juce::Rectangle<float> resR() { return { 276.f, (float) topY(), 112.f, (float) kBar }; }
    juce::Rectangle<float> swGR() { return { 410.f, (float) topY() + 28.f, 150.f, 16.f }; }
    juce::Rectangle<float> tabR (int i) { return { 604.f + i * 46.f, (float) topY(), 42.f, (float) kBar }; }
    juce::Rectangle<float> addR() { return { (float) designW - 76.f, (float) topY(), 54.f, (float) kBar }; }
    juce::Rectangle<float> laneTabR (int i) { static const float x[] { 0.f, 80.f, 198.f }, w[] { 74.f, 112.f, 66.f }; return { kStepX0 + x[i], (float) panelY() + 6.f, w[i], 26.f }; }
    juce::Rectangle<float> laneR() { return { (float) kStepX0, (float) panelY() + 38.f, (float) designW - kStepX0 - 26.f, 112.f }; }
    juce::Rectangle<float> volR() { return { 16.f, (float) panelY() + 50.f, 270.f, 16.f }; }
    juce::Rectangle<float> panR() { return { 16.f, (float) panelY() + 92.f, 270.f, 16.f }; }
    juce::Rectangle<float> swR() { return { 16.f, (float) panelY() + 134.f, 270.f, 16.f }; }
    int stepCount() { return clip ? clip->getPattern (0).getNumNotes() : 16; }
    float stepW() { return (float) (designW - kStepX0 - 26) / (float) stepCount(); }
    juce::Rectangle<float> rowR (int row) { return { 10.f, (float) (rowsY() + row * kRowH) + 3.f, (float) designW - 20.f, kRowH - 6.f }; }
    juce::Rectangle<float> stepR (int row, int s) { const float w = stepW(); return { kStepX0 + s * w + 2, (float) (rowsY() + row * kRowH) + 8.f, w - 5.f, kRowH - 16.f }; }
    juce::Rectangle<float> powerR (int row) { return { 18.f, (float) (rowsY() + row * kRowH) + 9.f, 34.f, kRowH - 18.f }; }
    juce::Rectangle<float> soloR (int row) { return { 56.f, (float) (rowsY() + row * kRowH) + 9.f, 34.f, kRowH - 18.f }; }
    juce::Rectangle<float> nameR (int row) { return { (float) kNameX, (float) (rowsY() + row * kRowH) + 9.f, (float) kNameW, kRowH - 18.f }; }

    // ---- filas: silencio, solo, largo propio ----
    int offNote (int r) { return clip ? (int) clip->getChannels()[r]->noteNumber + 64 : 100; }
    bool isOn (int r) { return clip == nullptr || clip->getTrack() == nullptr || ! (bool) clip->getTrack()->state.getProperty ("injOff" + juce::String (r), false); }
    bool anySolo() { for (int r = 0; r < rows; ++r) if (isSolo (r)) return true; return false; }
    bool audible (int r) { return isOn (r) && (! anySolo() || isSolo (r)); }
    void setOn (int r, bool on) { if (clip->getTrack()) clip->getTrack()->state.setProperty ("injOff" + juce::String (r), ! on, nullptr); applyAudible(); repaint(); }
    void applyAudible()   // un sample que no debe sonar deja de responder a su nota
    {
        if (! clip || ! sampler) return;
        for (int r = 0; r < clip->getChannels().size(); ++r)
        {
            const int note = clip->getChannels()[r]->noteNumber; const int i = soundForRow (r); if (i < 0) continue;
            const int k = audible (r) ? note : offNote (r); sampler->setSoundParams (i, k, k, k); sampler->setSoundOpenEnded (i, ! rowCut (r));
        }
    }
    int rowLen (int r) { if (! clip) return 16; const int n = stepCount(), v = (int) clip->state.getProperty ("injRowLen" + juce::String (r), 0); return (v >= 2 && v < n) ? v : n; }
    template <class F> void mirror (int r, int s, F fn) { const int n = rowLen (r), total = stepCount(); for (int k = s % n; k < total; k += n) fn (k); }
    void setCell (int r, int s, bool on) { auto pat = clip->getPattern (0); mirror (r, s, [&] (int k) { pat.setNote (r, k, on); }); }
    void setCellVel (int r, int s, int v) { auto pat = clip->getPattern (0); mirror (r, s, [&] (int k) { if (pat.getNote (r, k)) pat.setVelocity (r, k, v); }); }
    void setCellGate (int r, int s, double v) { auto pat = clip->getPattern (0); mirror (r, s, [&] (int k) { if (pat.getNote (r, k)) pat.setGate (r, k, v); }); }
    void setCellProb (int r, int s, float v) { auto pat = clip->getPattern (0); mirror (r, s, [&] (int k) { if (pat.getNote (r, k)) pat.setProbability (r, k, v); }); }
    void applyRowLen (int r)   // repite los primeros pasos de la fila hasta el final del patrón
    {
        if (! clip) return; auto pat = clip->getPattern (0); const int n = rowLen (r), total = stepCount();
        for (int k = n; k < total; ++k) { const int s = k % n; const bool on = pat.getNote (r, s); pat.setNote (r, k, on); if (on) { pat.setVelocity (r, k, pat.getVelocity (r, s)); pat.setGate (r, k, pat.getGate (r, s)); pat.setProbability (r, k, pat.getProbability (r, s)); } }
    }
    void applySwing (te::StepClip::Channel& ch, float v)
    {
        if (v < .01f) { ch.grooveTemplate = juce::String(); ch.grooveStrength = 0.f; }
        else { ch.grooveTemplate = juce::String (stepLen() >= .49 ? "Basic 8th Swing" : "Basic 16th Swing"); ch.grooveStrength = juce::jlimit (0.f, 1.f, v); }
    }

    // ---- dibujo ----
    juce::Colour padCol() const { if (clip) if (auto* at = dynamic_cast<te::AudioTrack*> (clip->getTrack())) { int i = 0; for (auto* t : te::getAudioTracks (edit)) { if (t == at) break; ++i; } return trackutil::colourOf (at, i); } return juce::Colour (0xfff59735); }
    void drawBtn (juce::Graphics& g, juce::Rectangle<float> r, bool on, bool hot = false)
    {
        using namespace theme; g.setColour (on ? accent.withAlpha (.2f) : (hot ? panelHi : panel)); g.fillRoundedRectangle (r, 3.f);
        g.setColour (on ? accent : seam); g.drawRoundedRectangle (r.reduced (.5f), 3.f, 1.f);
        if (! on) { g.setColour (edgeLight); g.fillRect (r.getX() + 2.f, r.getY() + 1.f, r.getWidth() - 4.f, 1.f); }
    }
    void drawSlider (juce::Graphics& g, juce::Rectangle<float> r, float frac, const juce::String& label, bool centred, bool enabled)
    {
        using namespace theme;
        g.setColour (enabled ? textDim : textFaint); g.setFont (theme::fonts::label (12.f)); g.drawText (label, (int) r.getX(), (int) r.getY() - 22, 270, 18, juce::Justification::centredLeft);
        { auto tk = r.withSizeKeepingCentre (r.getWidth(), 4.f); g.setColour (boxFill); g.fillRoundedRectangle (tk, 1.f); g.setColour (seam); g.drawRoundedRectangle (tk.expanded (.5f), 1.f, 1.f); }
        g.setColour (enabled ? accent : textDim.withAlpha (.4f)); const float x = r.getX() + frac * r.getWidth();
        if (centred) g.fillRect (juce::Rectangle<float> (std::min (x, r.getCentreX()), r.getCentreY() - 2, std::abs (x - r.getCentreX()), 4.f)); else g.fillRect (juce::Rectangle<float> (r.getX(), r.getCentreY() - 2, x - r.getX(), 4.f));
        { juce::Rectangle<float> th (x - 4.f, r.getCentreY() - 8.f, 8.f, 16.f); g.setColour (enabled ? juce::Colour (0xffc9ccd3) : textFaint); g.fillRoundedRectangle (th, 1.5f); g.setColour (seam); g.drawRoundedRectangle (th.reduced (.5f), 1.5f, 1.f); g.setColour (enabled ? accent : textDim); g.fillRect (x - .5f, th.getY() + 3.f, 1.f, th.getHeight() - 6.f); }
    }
    juce::String resLabel() { const double l = stepLen(); return l >= .49 ? "1/8" : (l > .2 ? "1/16" : (l > .15 ? juce::String::fromUTF8 ("1/16 tresillo") : "1/32")); }
    void paintBody (juce::Graphics& g) override
    {
        using namespace theme; validate();
        auto& tc = edit.getTransport(); const bool playing = tc.isPlaying();
        drawBtn (g, playR(), playing && patternMode); icons::draw (g, icons::Play, playR(), (playing && patternMode) ? theme::accentHi : juce::Colours::white);
        drawBtn (g, stopR(), false); icons::draw (g, icons::Stop, stopR(), juce::Colours::white);
        for (auto r : { stepsR(), resR() }) { drawBtn (g, r, false); icons::draw (g, icons::Chevron, r.removeFromRight (34.f), textDim); }
        g.setColour (text); g.setFont (theme::fonts::ui (15.f)); g.drawText (juce::String (stepCount()) + " pasos", stepsR().toNearestInt().withTrimmedLeft (16).withTrimmedRight (30), juce::Justification::centredLeft);
        g.drawText (resLabel(), resR().toNearestInt().withTrimmedLeft (16).withTrimmedRight (30), juce::Justification::centredLeft);
        { const float sw = clip ? (float) (double) clip->state.getProperty ("injSwing", 0.0) : 0.f; drawSlider (g, swGR(), sw, "Swing   " + juce::String (juce::roundToInt (sw * 100)) + " %", false, true); }
        drawBtn (g, addR(), false); g.setColour (rows < 8 ? text : textDim); g.drawLine (addR().getCentreX() - 10, addR().getCentreY(), addR().getCentreX() + 10, addR().getCentreY(), 2.2f); g.drawLine (addR().getCentreX(), addR().getCentreY() - 10, addR().getCentreX(), addR().getCentreY() + 10, 2.2f);
        if (clip == nullptr) return;
        const double sl = stepLen();
        int cur = -1; if (playing) { const double b = edit.tempoSequence.toBeats (tc.getPosition()).inBeats() - edit.tempoSequence.toBeats (clip->getPosition().getStart()).inBeats(); if (b >= 0) cur = (int) std::floor (std::fmod (b, stepCount() * sl) / sl); }
        g.setFont (theme::fonts::ui (10.5f)); g.setColour (textDim);
        for (int s = 0; s < stepCount(); s += 4) g.drawText (juce::String (s + 1), juce::Rectangle<float> (stepR (0, s).getX(), (float) rowsY() - 17.f, 30.f, 14.f).toNearestInt(), juce::Justification::centredLeft);
        auto pat = clip->getPattern (0);
        for (int r = 0; r < rows; ++r)
        {
            const bool on = isOn (r), aud = audible (r), solo = isSolo (r), selR = r == selRow;
            g.setColour (selR ? panelHi : bgGrid); g.fillRoundedRectangle (rowR (r), 3.f); g.setColour (seam); g.drawRoundedRectangle (rowR (r).reduced (.5f), 3.f, 1.f);
            if (selR) { g.setColour (accent); g.fillRect (rowR (r).getX() + 1.f, rowR (r).getY() + 1.f, 2.f, rowR (r).getHeight() - 2.f); }
            drawBtn (g, powerR (r), false); g.setColour (on ? accentHi : textDim);
            { auto pr = powerR (r).withSizeKeepingCentre (18, 18); g.drawEllipse (pr.reduced (1.5f), 1.6f); g.fillRect (pr.getCentreX() - .8f, pr.getY() - 2, 1.6f, 9.f); }
            drawBtn (g, soloR (r), solo); g.setColour (solo ? theme::accentHi : textDim); g.setFont (theme::fonts::ui (15.f, true)); g.drawText ("S", soloR (r).toNearestInt(), juce::Justification::centred);
            drawBtn (g, nameR (r), false);
            auto tr = nameR (r).toNearestInt().withTrimmedLeft (14).withTrimmedRight (40);
            g.setColour (aud ? text : textDim); g.setFont (theme::fonts::ui (15.f, true)); g.drawText (clip->getChannels()[r]->name.get(), tr.withTrimmedBottom (16), juce::Justification::bottomLeft);
            g.setColour (hasSample (r) ? accentHi.withAlpha (aud ? 1.f : .5f) : textDim.withAlpha (.7f)); g.setFont (theme::fonts::ui (11.f));
            g.drawText (hasSample (r) ? sampleName (r) : juce::String ("sin sample"), tr.withTop (tr.getBottom() - 15).withHeight (14), juce::Justification::centredLeft);
            icons::draw (g, icons::Chevron, nameR (r).removeFromRight (34), text);
            const int rl = rowLen (r);
            for (int s = 0; s < stepCount(); ++s)
            {
                const bool v = pat.getNote (r, s); auto sr = stepR (r, s); const bool echo = s >= rl; const float al = echo ? .5f : 1.f;
                const float fl = (r < 8 && s < 64) ? flash[r][s] : 0.f;
                if (fl > 0.f) { g.setColour (accentHi.withAlpha (fl * .55f)); g.drawRoundedRectangle (sr.expanded (1.f + fl * 2.f), 3.f, 1.5f); }
                const auto rr = fl > 0.f ? sr.expanded (fl * 1.5f) : sr;
                if (v)
                {
                    const int vel = pat.getVelocity (r, s); auto base = (aud ? padCol() : textFaint).interpolatedWith (bgBar, (1.f - vel / 127.f) * .55f).interpolatedWith (juce::Colours::white, fl * .5f);
                    g.setColour (base.withMultipliedAlpha (al)); g.fillRoundedRectangle (rr, 2.f);
                    g.setColour (juce::Colours::white.withAlpha (.22f * al)); g.fillRect (rr.getX() + 1.f, rr.getY() + 1.f, rr.getWidth() - 2.f, 1.f);
                    g.setColour (juce::Colours::black.withAlpha (.5f * al)); g.drawRoundedRectangle (rr.reduced (.5f), 2.f, 1.f);
                    const double gt = pat.getGate (r, s); if (gt < .98) { g.setColour (juce::Colours::black.withAlpha (.55f * al)); g.fillRect (rr.getX() + 4.f, rr.getBottom() - 7.f, (rr.getWidth() - 8.f) * (float) gt, 2.f); }
                    const float pb = pat.getProbability (r, s); if (pb < .99f) { g.setColour (theme::accentHi.withAlpha (.85f * al)); g.setFont (theme::fonts::ui (10.f, true)); g.drawText (juce::String (juce::roundToInt (pb * 100)), rr.toNearestInt().withTrimmedLeft (4).withHeight (14), juce::Justification::centredLeft); }
                }
                else
                {
                    g.setColour ((((s / 4) & 1) ? juce::Colour (0xff33353c) : juce::Colour (0xff2b2d33)).withMultipliedAlpha (al)); g.fillRoundedRectangle (rr, 2.f);
                    g.setColour (juce::Colour (0xff1a1b1f)); g.drawRoundedRectangle (rr.reduced (.5f), 2.f, 1.f);
                }
                if (s == cur) { g.setColour (juce::Colours::white.withAlpha (.85f)); g.drawRoundedRectangle (sr.reduced (.5f), 2.f, 1.5f); }
            }
            if (rl < stepCount()) { auto b = stepR (r, rl); g.setColour (accentHi.withAlpha (.7f)); g.fillRect (b.getX() - 3.5f, b.getY() - 2.f, 2.f, b.getHeight() + 4.f); }
        }
        paintTabsAndPanel (g);
    }
    float laneFrac (int r, int s)
    {
        auto pat = clip->getPattern (0);
        if (! pat.getNote (r, s)) return 0.f;
        return lane == 0 ? pat.getVelocity (r, s) / 127.f : (lane == 1 ? pat.getProbability (r, s) : (float) pat.getGate (r, s));
    }
    void setLaneFrac (int r, int s, float f)
    {
        f = juce::jlimit (0.f, 1.f, f);
        if (lane == 0) setCellVel (r, s, juce::jlimit (4, 127, (int) std::lround (f * 127.f))); else if (lane == 1) setCellProb (r, s, juce::jlimit (0.05f, 1.f, f)); else setCellGate (r, s, juce::jlimit (0.1, 1.0, (double) f));
    }
    void paintTabsAndPanel (juce::Graphics& g)
    {
        using namespace theme; const auto pats = patterns();
        for (int i = 0; i <= pats.size() && i <= kMaxPat; ++i)
        {
            auto r = tabR (i); const bool plus = i == pats.size(), cur = ! plus && pats[i] == clip.get();
            if (plus && pats.size() >= kMaxPat) break;
            drawBtn (g, r, cur); g.setColour (cur ? theme::accentHi : (plus ? textDim : text)); g.setFont (theme::fonts::ui (plus ? 24.f : 18.f)); g.drawText (plus ? "+" : juce::String (i + 1), r.toNearestInt(), juce::Justification::centred);
        }
        auto box = juce::Rectangle<float> (10.f, (float) panelY() - 2, (float) designW - 20.f, (float) kPanel + 4); g.setColour (bgBar); g.fillRoundedRectangle (box, 3.f); g.setColour (seam); g.drawRoundedRectangle (box.reduced (.5f), 3.f, 1.f);
        selRow = juce::jlimit (0, rows - 1, selRow); const bool has = hasSample (selRow); const int si = soundForRow (selRow);
        g.setColour (text); g.setFont (theme::fonts::ui (15.f, true)); g.drawText (clip->getChannels()[selRow]->name.get(), 20, panelY() + 6, 270, 22, juce::Justification::centredLeft);
        const float db = si >= 0 ? sampler->getSoundGainDb (si) : 0.f, pn = si >= 0 ? sampler->getSoundPan (si) : 0.f;
        drawSlider (g, volR(), juce::jmap (db, -40.f, 6.f, 0.f, 1.f), has ? "Volumen   " + juce::String (db, 1) + " dB" : juce::String ("Volumen"), false, has);
        drawSlider (g, panR(), (pn + 1.f) * .5f, has ? juce::String ("Pan   ") + (std::abs (pn) < .02f ? juce::String ("centro") : juce::String (juce::roundToInt (std::abs (pn) * 100)) + (pn < 0 ? "% izq." : "% der.")) : juce::String ("Pan"), true, has);
        const float sw = rowSwing (selRow); drawSlider (g, swR(), sw, "Swing de la fila   " + juce::String (juce::roundToInt (sw * 100)) + " %", false, true);
        static const char* ln[] { "Fuerza", "Probabilidad", "Largo" };
        for (int i = 0; i < 3; ++i) { auto r = laneTabR (i); drawBtn (g, r, lane == i); g.setColour (lane == i ? theme::accentHi : text); g.setFont (theme::fonts::ui (13.5f)); g.drawText (juce::String::fromUTF8 (ln[i]), r.toNearestInt(), juce::Justification::centred); }
        auto lr = laneR(); g.setColour (boxFill); g.fillRoundedRectangle (lr, 2.f); g.setColour (seam); g.drawRoundedRectangle (lr.reduced (.5f), 2.f, 1.f);
        const float w = lr.getWidth() / (float) stepCount(); auto pat = clip->getPattern (0);
        for (int s = 0; s < stepCount(); ++s)
        {
            if ((s / 4) & 1) { g.setColour (juce::Colours::white.withAlpha (.025f)); g.fillRect (lr.getX() + s * w, lr.getY(), w, lr.getHeight()); }
            const float f = laneFrac (selRow, s), h = (lr.getHeight() - 6.f) * f; const bool on = pat.getNote (selRow, s);
            g.setColour (on ? padCol() : juce::Colour (0xff3a3c43)); g.fillRect (lr.getX() + s * w + 3, lr.getBottom() - 3 - h, w - 6, std::max (2.f, h));
        }
        if (lane == 2 && ! rowCut (selRow)) { g.setColour (textDim.withAlpha (.8f)); g.setFont (theme::fonts::ui (12.f)); g.drawText (juce::String::fromUTF8 ("El largo solo se oye si la fila corta el sample (menú de la fila)"), lr.toNearestInt().withTrimmedLeft (10).withHeight (20), juce::Justification::centredLeft); }
        for (float f : { .25f, .5f, .75f }) { g.setColour (juce::Colours::white.withAlpha (.045f)); g.fillRect (lr.getX(), lr.getBottom() - 3.f - f * (lr.getHeight() - 6.f), lr.getWidth(), 1.f); }
    }
    void bodyDown (const juce::MouseEvent& e) override
    {
        ensureClip(); if (! clip) return; auto p = e.position; auto& tc = edit.getTransport();
        if (playR().contains (p)) { if (tc.isPlaying() && patternMode) { tc.stop (false, false); endPattern(); } else startPattern(); repaint(); return; }
        if (stopR().contains (p)) { tc.stop (false, false); endPattern(); tc.setPosition (clip->getPosition().getStart()); repaint(); return; }
        if (stepsR().contains (p)) { stepsMenu(); return; }
        if (resR().contains (p)) { resMenu(); return; }
        if (swGR().expanded (0, 10).contains (p)) { drag = Drag::swingAll; bodyDrag (e); return; }
        if (addR().contains (p)) { if (rows < 8) { ++rows; setDesign (designW, designHeight (rows)); } repaint(); return; }
        for (int i = 0; i < patterns().size() + 1 && i < kMaxPat + 1; ++i) if (tabR (i).contains (p))
        { if (i < patterns().size()) { if (e.mods.isPopupMenu()) patternMenu (i); else selectPattern (i); } else if (patterns().size() < kMaxPat) addPattern (false); return; }
        for (int i = 0; i < 3; ++i) if (laneTabR (i).contains (p)) { lane = i; repaint(); return; }
        if (volR().expanded (0, 10).contains (p)) { drag = Drag::vol; bodyDrag (e); return; }
        if (panR().expanded (0, 10).contains (p)) { drag = Drag::pan; bodyDrag (e); return; }
        if (swR().expanded (0, 10).contains (p)) { drag = Drag::swingRow; bodyDrag (e); return; }
        if (laneR().contains (p)) { drag = Drag::lane; edit.getUndoManager().beginNewTransaction ("Editar pasos"); rampStep = -1; bodyDrag (e); return; }
        for (int r = 0; r < rows; ++r)
        {
            if (powerR (r).contains (p)) { setOn (r, ! isOn (r)); return; }
            if (soloR (r).contains (p)) { setSolo (r, ! isSolo (r)); return; }
            if (nameR (r).contains (p)) { selRow = r; if (p.x > nameR (r).getRight() - 44 || e.mods.isPopupMenu()) rowMenu (r); else drag = Drag::move; repaint(); return; }
            for (int s = 0; s < stepCount(); ++s) if (stepR (r, s).expanded (3).contains (p))
            {
                selRow = r; auto pat = clip->getPattern (0); paint_ = ! pat.getNote (r, s); edit.getUndoManager().beginNewTransaction ("Pasos"); setCell (r, s, paint_); if (std::getenv ("INJ_DBG")) std::printf ("DBG pad r=%d s=%d paint=%d len=%d steps=%d\n", r, s, (int) paint_, rowLen (r), stepCount());
                if (paint_ && (e.mods.isShiftDown() || e.mods.isAltDown())) setCellVel (r, s, e.mods.isShiftDown() ? 127 : 55);
                if (paint_) { hear (r); if (r < 8 && s < 64) flash[r][s] = 1.f; } lastCell = { r, s }; painting = true; if (onClipChanged) onClipChanged(); repaint(); return;
            }
        }
    }
    juce::String tipAt (juce::Point<float> p) override
    {
        if (playR().contains (p)) return juce::String::fromUTF8 ("Reproducir patrón");
        if (stopR().contains (p)) return "Detener";
        if (stepsR().contains (p)) return "Cantidad de pasos";
        if (resR().contains (p)) return juce::String::fromUTF8 ("Duración de cada paso (1/32 para rolls, tresillo para ritmo de 3)");
        if (swGR().expanded (0, 10).contains (p)) return juce::String::fromUTF8 ("Swing de todas las filas (doble clic = 0)");
        if (addR().contains (p)) return "Agregar fila";
        for (int i = 0; i <= patterns().size() && i < kMaxPat + 1; ++i) if (tabR (i).contains (p)) return i == patterns().size() ? juce::String::fromUTF8 ("Patrón nuevo") : juce::String::fromUTF8 ("Patrón ") + juce::String (i + 1) + juce::String::fromUTF8 ("  (clic derecho: más opciones)");
        for (int r = 0; r < rows; ++r)
        {
            if (powerR (r).contains (p)) return "Activar fila";
            if (soloR (r).contains (p)) return "Solo: escuchar solo esta fila";
            if (nameR (r).contains (p)) return p.x > nameR (r).getRight() - 44 ? juce::String ("Sample y opciones de la fila") : juce::String::fromUTF8 ("Arrastra para mover la fila");
        }
        if (volR().expanded (0, 10).contains (p)) return "Volumen";
        if (panR().expanded (0, 10).contains (p)) return "Pan";
        if (swR().expanded (0, 10).contains (p)) return juce::String::fromUTF8 ("Swing de esta fila (doble clic = 0)");
        if (laneR().contains (p)) return juce::String::fromUTF8 ("Arrastra para editar. Mayús + arrastrar = rampa");
        return {};
    }
    void bodyDouble (const juce::MouseEvent& e) override
    {
        if (volR().expanded (0, 10).contains (e.position)) { setVol (selRow, 0.f); repaint(); }
        else if (panR().expanded (0, 10).contains (e.position)) { setPan (selRow, 0.f); repaint(); }
        else if (swR().expanded (0, 10).contains (e.position)) { setSwingRow (selRow, 0.f); repaint(); }
        else if (swGR().expanded (0, 10).contains (e.position)) { setSwingAll (0.f); }
    }
    void bodyDrag (const juce::MouseEvent& e) override   // arrastrar sobre varias celdas pinta/borra
    {
        if (! clip) return;
        if (drag == Drag::vol) { setVol (selRow, juce::jmap (juce::jlimit (0.f, 1.f, (e.position.x - volR().getX()) / volR().getWidth()), -40.f, 6.f)); repaint(); return; }
        if (drag == Drag::pan) { float v = juce::jlimit (-1.f, 1.f, (e.position.x - panR().getX()) / panR().getWidth() * 2.f - 1.f); setPan (selRow, std::abs (v) < .04f ? 0.f : v); repaint(); return; }
        if (drag == Drag::swingAll) { setSwingAll (juce::jlimit (0.f, 1.f, (e.position.x - swGR().getX()) / swGR().getWidth())); return; }
        if (drag == Drag::swingRow) { setSwingRow (selRow, juce::jlimit (0.f, 1.f, (e.position.x - swR().getX()) / swR().getWidth())); repaint(); return; }
        if (drag == Drag::move) { for (int r = 0; r < rows; ++r) if (r != selRow && e.position.y >= rowR (r).getY() && e.position.y < rowR (r).getBottom()) { swapRows (selRow, r); selRow = r; break; } return; }
        if (drag == Drag::lane)
        {
            const int st = juce::jlimit (0, stepCount() - 1, (int) ((e.position.x - laneR().getX()) / (laneR().getWidth() / (float) stepCount())));
            const float f = (laneR().getBottom() - 3.f - e.position.y) / (laneR().getHeight() - 6.f);
            if (e.mods.isShiftDown() && rampStep >= 0)   // rampa recta desde el punto donde empezó el arrastre
            {
                const int a = std::min (rampStep, st), b = std::max (rampStep, st);
                for (int k = a; k <= b; ++k) { const float t = b == a ? 1.f : (float) (rampStep <= st ? k - rampStep : rampStep - k) / (float) std::abs (st - rampStep); setLaneFrac (selRow, k, rampF + (f - rampF) * t); }
            }
            else { setLaneFrac (selRow, st, f); if (! e.mods.isShiftDown()) { rampStep = st; rampF = juce::jlimit (0.f, 1.f, f); } }
            repaint(); return;
        }
        if (! painting) return;
        for (int r = 0; r < rows; ++r) for (int s = 0; s < stepCount(); ++s)
            if (stepR (r, s).expanded (3).contains (e.position) && lastCell != juce::Point<int> (r, s)) { setCell (r, s, paint_); if (paint_) { hear (r); if (r < 8 && s < 64) flash[r][s] = 1.f; } lastCell = { r, s }; repaint(); return; }
    }
    void bodyUp (const juce::MouseEvent&) override { painting = false; if (drag == Drag::lane || drag == Drag::move || drag == Drag::swingAll) { if (onClipChanged) onClipChanged(); } drag = Drag::none; }

    bool isInterestedInFileDrag (const juce::StringArray& f) override { return f.size() > 0; }
    void filesDropped (const juce::StringArray& f, int x0, int y0) override
    {
        const float x = x0 / scale, y = y0 / scale;
        juce::Array<juce::File> audio;
        for (auto& s : f) { juce::File fl (s); if (fl.existsAsFile() && fl.hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) audio.add (fl); }
        if (audio.isEmpty()) return;
        int target = -1;   // sobre cualquier punto de una fila: esa fila; fuera de las filas: filas nuevas
        for (int r = 0; r < rows; ++r) if (y >= rowsY() + r * kRowH && y < rowsY() + (r + 1) * kRowH) target = r;
        if (target >= 0) { assignSample (target, audio[0]); selRow = target; }
        for (int i = (target >= 0 ? 1 : 0); i < audio.size() && rows < 8; ++i)
        {
            ++rows; setDesign (designW, designHeight (rows));
            assignSample (rows - 1, audio[i]); selRow = rows - 1;
        }
        if (onClipChanged) onClipChanged();
        repaint();
    }

private:
    te::Edit& edit; te::StepClip::Ptr clip; te::Plugin::Ptr samplerPtr; te::SamplerPlugin* sampler = nullptr; bool patternMode = false, oldLoop = false; te::TimeRange oldRange; std::vector<std::pair<te::AudioTrack*, bool>> oldSolo; int rows = 4, selRow = 0, lane = 0, rampStep = -1; float rampF = 0.f;
    enum class Drag { none, vol, pan, lane, move, swingAll, swingRow }; Drag drag = Drag::none; bool painting = false, paint_ = true; juce::Point<int> lastCell;
    struct RowClip { juce::BigInteger notes; juce::Array<int> vel; juce::Array<double> gate; juce::Array<float> prob; int n = 16; bool valid = false; } rowClip;
    juce::StringArray dummy;
    std::unique_ptr<juce::FileChooser> chooser;

    void timerCallback() override
    {
        validate(); auto& tc = edit.getTransport(); if (patternMode && ! tc.isPlaying()) endPattern();
        if (! isVisible()) return;
        bool dirty = false; const bool playing = tc.isPlaying();
        int cur = -1; if (playing && clip != nullptr) { const double b = edit.tempoSequence.toBeats (tc.getPosition()).inBeats() - edit.tempoSequence.toBeats (clip->getPosition().getStart()).inBeats(); if (b >= 0) cur = (int) std::floor (std::fmod (b, stepCount() * stepLen()) / stepLen()); }
        if (cur != lastCur) { lastCur = cur; if (cur >= 0 && clip != nullptr) { auto pat = clip->getPattern (0); for (int r = 0; r < rows && r < 8; ++r) if (cur < 64 && pat.getNote (r, cur) && audible (r)) flash[r][cur] = 1.f; } dirty = true; }
        for (auto& row : flash) for (auto& f : row) if (f > 0.f) { f = f < .03f ? 0.f : f * .88f; dirty = true; }
        if (dirty || playing) repaint();
    }
    int lastCur = -1; float flash[8][64] {};
    te::SamplerPlugin* findSampler (te::AudioTrack* t) { for (auto* p : t->pluginList.getPlugins()) if (auto* s = dynamic_cast<te::SamplerPlugin*> (p)) return s; return nullptr; }
    void hear (int r) { if (onAudition && clip) if (auto* t = dynamic_cast<te::AudioTrack*> (clip->getTrack())) onAudition (t, clip->getChannels()[r]->noteNumber.get()); }
    bool hasSample (int r) { return soundForRow (r) >= 0; }
    juce::String sampleName (int r) { const int i = soundForRow (r); return i >= 0 && sampler ? sampler->getSoundName (i) : juce::String(); }
    int soundForRow (int row)
    {
        if (! sampler || ! clip || row < 0 || row >= clip->getChannels().size()) return -1; const int note = clip->getChannels()[row]->noteNumber, off = note + 64;
        for (int i = 0; i < sampler->getNumSounds(); ++i) { const int a = sampler->getMinKey (i), b = sampler->getMaxKey (i); if ((a == note && b == note) || (a == off && b == off)) return i; }
        for (int i = 0; i < sampler->getNumSounds(); ++i) if (sampler->getMinKey (i) == 0 && sampler->getMaxKey (i) == 0) return i;   // proyectos viejos: el sample apagado quedaba en la nota 0
        return -1;
    }
    juce::Array<te::StepClip*> patterns()
    {
        juce::Array<te::StepClip*> out; if (clip == nullptr || clip->getTrack() == nullptr) return out;
        if (auto* ct = dynamic_cast<te::ClipTrack*> (clip->getTrack())) for (auto* c : ct->getClips()) if (auto* sc = dynamic_cast<te::StepClip*> (c)) out.add (sc);
        std::sort (out.begin(), out.end(), [] (te::StepClip* a, te::StepClip* b) { return a->getPosition().getStart() < b->getPosition().getStart(); });
        return out;
    }
    void selectPattern (int i)
    {
        auto pats = patterns(); if (i < 0 || i >= pats.size() || pats[i] == clip.get()) return;
        const bool was = patternMode; if (was) { edit.getTransport().stop (false, false); endPattern(); }
        clip = pats[i]; if (was) startPattern(); repaint(); if (onClipChanged) onClipChanged();
    }
    void addPattern (bool duplicate)
    {
        auto pats = patterns(); if (pats.isEmpty() || ! clip) return; auto* host = dynamic_cast<te::AudioTrack*> (clip->getTrack()); if (host == nullptr) return;
        const double endB = edit.tempoSequence.toBeats (pats.getLast()->getPosition().getEnd()).inBeats(); const double lenB = edit.tempoSequence.toBeats (clip->getPosition().getEnd()).inBeats() - edit.tempoSequence.toBeats (clip->getPosition().getStart()).inBeats();
        const auto st = edit.tempoSequence.toTime (te::BeatPosition::fromBeats (std::ceil (endB / 4.0) * 4.0));
        te::StepClip::Ptr n;
        if (duplicate) { if (auto* c = trackutil::copyClipTo (edit, *clip, *host, st)) n = dynamic_cast<te::StepClip*> (c); }
        else
        {
            n = dynamic_cast<te::StepClip*> (host->insertNewClip (te::TrackItem::Type::step, juce::String::fromUTF8 ("Patrón ") + juce::String (pats.size() + 1), te::TimeRange (st, edit.tempoSequence.toTime (te::BeatPosition::fromBeats (std::ceil (endB / 4.0) * 4.0 + lenB))), nullptr));
            if (n)
            {
                for (int i = 0; i < n->getChannels().size() && i < clip->getChannels().size(); ++i) { auto& a = *n->getChannels()[i]; auto& b = *clip->getChannels()[i]; a.name = b.name.get(); a.noteNumber = (int) b.noteNumber; a.grooveTemplate = b.grooveTemplate.get(); a.grooveStrength = (float) b.grooveStrength; }
                n->getPattern (0).setNumNotes (stepCount()); n->getPattern (0).setNoteLength (clip->getPattern (0).getNoteLength());
                if (clip->state.hasProperty ("injSwing")) n->state.setProperty ("injSwing", clip->state.getProperty ("injSwing"), nullptr);
            }
        }
        if (n) { clip = n; if (onClipChanged) onClipChanged(); } repaint();
    }
    // desplazar, aleatorizar, invertir, repetir mitad y humanizar sobre el patrón actual (herramientas al estilo FL Studio)
    void patternOp (int id)
    {
        if (! clip) return; auto pat = clip->getPattern (0); const int ns = pat.getNumNotes(), nr = clip->getChannels().size(); juce::Random rnd;
        edit.getUndoManager().beginNewTransaction (juce::String::fromUTF8 ("Patrón"));
        for (int r = 0; r < nr; ++r)
        {
            std::vector<char> on ((size_t) ns); std::vector<int> vel ((size_t) ns); for (int k = 0; k < ns; ++k) { on[(size_t) k] = pat.getNote (r, k) ? 1 : 0; vel[(size_t) k] = pat.getVelocity (r, k); }
            std::vector<char> o2 = on; std::vector<int> v2 = vel;
            if (id == 4 || id == 5) for (int k = 0; k < ns; ++k) { const int to = ((k + (id == 4 ? -1 : 1)) % ns + ns) % ns; o2[(size_t) to] = on[(size_t) k]; v2[(size_t) to] = vel[(size_t) k]; }
            else if (id == 6) for (int k = 0; k < ns; ++k) { o2[(size_t) k] = rnd.nextFloat() < (r == 0 ? (k % 4 == 0 ? 0.9f : 0.08f) : (k % 2 == 0 ? 0.3f : 0.18f)) ? 1 : 0; v2[(size_t) k] = 70 + rnd.nextInt (50); }
            else if (id == 7) for (int k = 0; k < ns; ++k) o2[(size_t) k] = on[(size_t) k] ? 0 : 1;
            else if (id == 8) for (int k = ns / 2; k < ns; ++k) { o2[(size_t) k] = on[(size_t) (k - ns / 2)]; v2[(size_t) k] = vel[(size_t) (k - ns / 2)]; }
            else if (id == 9) for (int k = 0; k < ns; ++k) v2[(size_t) k] = juce::jlimit (20, 127, vel[(size_t) k] + rnd.nextInt (31) - 15);
            for (int k = 0; k < ns; ++k) { pat.setNote (r, k, o2[(size_t) k] != 0); if (o2[(size_t) k] && v2[(size_t) k] >= 4) pat.setVelocity (r, k, juce::jmin (127, v2[(size_t) k])); }
        }
        if (onClipChanged) onClipChanged(); repaint();
    }
    void patternMenu (int i)
    {
        juce::PopupMenu m; m.addItem (1, juce::String::fromUTF8 ("Duplicar este patrón")); m.addItem (2, juce::String::fromUTF8 ("Eliminar este patrón"), patterns().size() > 1); m.addItem (3, juce::String::fromUTF8 ("Vaciar este patrón (borra todos sus golpes)")); m.addSeparator(); m.addItem (4, juce::String::fromUTF8 ("Desplazar a la izquierda")); m.addItem (5, juce::String::fromUTF8 ("Desplazar a la derecha")); m.addItem (6, juce::String::fromUTF8 ("Aleatorizar golpes")); m.addItem (7, juce::String::fromUTF8 ("Invertir golpes")); m.addItem (8, juce::String::fromUTF8 ("Repetir la primera mitad")); m.addItem (9, juce::String::fromUTF8 ("Variar la fuerza (humanizar)"));
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (toLocal (tabR (i)))), [this, i] (int id)
        {
            auto pats = patterns(); if (i >= pats.size()) return; selectPattern (i);
            if (id == 1 && pats.size() < kMaxPat) addPattern (true);
            else if (id == 2 && pats.size() > 1) { te::StepClip::Ptr dead = pats[i]; clip = pats[i == 0 ? 1 : i - 1]; dead->removeFromParent(); if (onClipChanged) onClipChanged(); repaint(); }
            else if (id == 3) { clip->getPattern (0).clear(); repaint(); }
            else if (id >= 4 && id <= 9) patternOp (id);
        });
    }
    void setVol (int r, float db) { const int i = soundForRow (r); if (i >= 0 && sampler) sampler->setSoundGains (i, juce::jlimit (-40.f, 6.f, db), sampler->getSoundPan (i)); }
    void setPan (int r, float pn) { const int i = soundForRow (r); if (i >= 0 && sampler) sampler->setSoundGains (i, sampler->getSoundGainDb (i), juce::jlimit (-1.f, 1.f, pn)); }
    void setSteps (int n)
    {
        clip->getPattern (0).setNumNotes (n); for (int r = 0; r < rows; ++r) if (rowLen (r) < n) applyRowLen (r); repaint();
    }
    void stepsMenu()
    {
        juce::PopupMenu m; for (int n : { 8, 12, 16, 24, 32 }) m.addItem (n, juce::String (n) + " pasos", true, stepCount() == n);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (toLocal (stepsR()))), [this] (int id) { if (id > 0 && clip) setSteps (id); });
    }
    void resMenu()
    {
        juce::PopupMenu m; const double l = stepLen();
        m.addItem (1, juce::String::fromUTF8 ("1/8"), true, l >= .49); m.addItem (2, "1/16", true, l > .2 && l < .49); m.addItem (3, juce::String::fromUTF8 ("1/16 tresillo"), true, l > .15 && l <= .2); m.addItem (4, "1/32", true, l <= .15);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (toLocal (resR()))), [this] (int id) { static const double v[] { 0, .5, .25, 1.0 / 6.0, .125 }; if (id >= 1 && id <= 4) setResolution (v[id]); });
    }
    void rowMenu (int r)
    {
        juce::PopupMenu m, lens; m.addItem (1, "Cargar sample..."); m.addItem (2, "Quitar sample", hasSample (r)); m.addItem (3, "Renombrar fila..."); m.addSeparator();
        m.addItem (15, juce::String::fromUTF8 ("Cortar el sample al terminar el paso (usa el Largo)"), true, rowCut (r)); m.addSeparator(); m.addItem (10, "Copiar fila"); m.addItem (11, "Pegar en esta fila", rowClip.valid); m.addItem (12, "Vaciar fila"); m.addSeparator(); m.addItem (13, "Subir fila", r > 0); m.addItem (14, "Bajar fila", r < rows - 1);
        lens.addItem (200, juce::String::fromUTF8 ("Igual al patrón"), true, rowLen (r) == stepCount()); for (int n = 3; n < stepCount(); ++n) if (n <= 15) lens.addItem (200 + n, juce::String (n) + " pasos", true, rowLen (r) == n);
        m.addSubMenu (juce::String::fromUTF8 ("Largo de la fila (polirritmia)"), lens);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (toLocal (nameR (r)))), [this, r] (int id)
        {
            if (id == 1) { chooser = std::make_unique<juce::FileChooser> ("Elige un sample", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg"); chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, r] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) assignSample (r, fc.getResult()); }); }
            else if (id == 2) { const int i = soundForRow (r); if (i >= 0 && sampler) sampler->removeSound (i); repaint(); }
            else if (id == 3) { auto* aw = new juce::AlertWindow ("Renombrar fila", "", juce::MessageBoxIconType::NoIcon); aw->addTextEditor ("n", clip->getChannels()[r]->name.get()); aw->addButton ("Aceptar", 1); aw->addButton ("Cancelar", 0);
                aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, r, aw] (int res) { if (res == 1) { clip->getChannels()[r]->name = aw->getTextEditorContents ("n"); repaint(); } delete aw; })); }
            else if (id == 10) copyRow (r); else if (id == 11) pasteRow (r); else if (id == 15) setRowCut (r, ! rowCut (r));
            else if (id == 12) { edit.getUndoManager().beginNewTransaction ("Vaciar fila"); clip->getPattern (0).clearChannel (r); if (onClipChanged) onClipChanged(); repaint(); }
            else if (id == 13) { swapRows (r, r - 1); selRow = r - 1; } else if (id == 14) { swapRows (r, r + 1); selRow = r + 1; }
            else if (id >= 200) setRowLength (r, id - 200);
        });
    }
};
