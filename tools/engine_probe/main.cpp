// INJUSTICE DAW - Etapa 0: prueba técnica del motor, sin interfaz.
// Verifica con datos (no a oído) que el motor:
//   1) reproduce un clip MIDI en un instrumento y lo exporta a WAV en los instantes correctos;
//   2) reproduce un StepClip (patrón de pasos) con un sampler y respeta la cuadrícula.
// El sinte y el sampler de Tracktion se usan SOLO como banco de pruebas.
#include <tracktion_engine/tracktion_engine.h>
#include <cmath>
#include <cstdio>

namespace te = tracktion;
using namespace std::literals;

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::printf ("%s  %s\n", ok ? "OK  " : "FALLO", what.toRawUTF8());
    if (! ok) ++failures;
}

struct Wav { juce::AudioBuffer<float> buf; double sr = 0; };

static Wav readWav (const juce::File& f)
{
    juce::AudioFormatManager m; m.registerBasicFormats();
    Wav w;
    if (auto r = std::unique_ptr<juce::AudioFormatReader> (m.createReaderFor (f)))
    {
        w.buf.setSize ((int) r->numChannels, (int) r->lengthInSamples);
        r->read (&w.buf, 0, w.buf.getNumSamples(), 0, true, true);
        w.sr = r->sampleRate;
    }
    return w;
}

static float peakIn (const Wav& w, double t0, double t1)
{
    int a = juce::jmax (0, (int) (t0 * w.sr)), b = juce::jmin (w.buf.getNumSamples(), (int) (t1 * w.sr));
    float p = 0;
    for (int c = 0; c < w.buf.getNumChannels(); ++c)
        for (int i = a; i < b; ++i) p = juce::jmax (p, std::abs (w.buf.getSample (c, i)));
    return p;
}

static void test1_midi (te::Engine& engine, const juce::File& dir)
{
    std::printf ("--- 1) Clip MIDI -> instrumento -> WAV\n");
    auto edit = te::createEmptyEdit (engine, dir.getChildFile ("t1.tracktionedit"));
    edit->tempoSequence.getTempo (0)->setBpm (120.0);   // 1 negra = 0.5 s
    edit->ensureNumberOfAudioTracks (1);
    auto* track = te::getAudioTracks (*edit)[0];

    auto synth = edit->getPluginCache().createNewPlugin (te::FourOscPlugin::xmlTypeName, {});
    check (synth != nullptr, "se crea el instrumento de pruebas");
    track->pluginList.insertPlugin (synth, 0, nullptr);

    auto clip = track->insertMIDIClip ("prueba", te::TimeRange (te::TimePosition::fromSeconds (0.0), te::TimeDuration::fromSeconds (4.0)), nullptr);
    // notas en negras 0 y 2 (t=0 s y t=1 s), cada una de 0.25 s de duración (media negra)
    clip->getSequence().addNote (69, te::BeatPosition::fromBeats (0.0), te::BeatDuration::fromBeats (0.5), 100, 0, nullptr);
    clip->getSequence().addNote (81, te::BeatPosition::fromBeats (2.0), te::BeatDuration::fromBeats (0.5), 100, 0, nullptr);

    auto out = dir.getChildFile ("t1.wav");
    check (te::Renderer::renderToFile (*edit, out, false), "render offline termina");
    auto w = readWav (out);
    check (w.buf.getNumSamples() > 0, "el WAV tiene muestras");
    std::printf ("      sr=%.0f  duración=%.3f s\n", w.sr, w.buf.getNumSamples() / juce::jmax (1.0, w.sr));

    check (peakIn (w, 0.02, 0.25) > 0.02f,  "hay audio en la nota 1 (0.00-0.25 s)");
    check (peakIn (w, 1.02, 1.25) > 0.02f,  "hay audio en la nota 2 (1.00-1.25 s)");
    check (peakIn (w, 0.55, 0.95) < 0.01f,  "silencio entre notas (0.55-0.95 s) tras el release");
    check (peakIn (w, 2.0, 3.5)  < 0.001f,  "silencio tras la última nota");
}

static void makeClick (const juce::File& f, double sr)
{
    juce::WavAudioFormat fmt;
    std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
    std::unique_ptr<juce::AudioFormatWriter> wr (fmt.createWriterFor (os.get(), sr, 1, 16, {}, 0));
    if (wr) os.release();
    juce::AudioBuffer<float> b (1, (int) (0.05 * sr));
    for (int i = 0; i < b.getNumSamples(); ++i)
        b.setSample (0, i, 0.8f * std::exp (-i / (0.01f * (float) sr)) * std::sin (6.2831853f * 1000.f * (float) i / (float) sr));
    wr->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

static void test2_steps (te::Engine& engine, const juce::File& dir)
{
    std::printf ("--- 2) StepClip (patrón de pasos) + sampler -> WAV\n");
    auto click = dir.getChildFile ("click.wav");
    makeClick (click, 44100.0);
    { auto c = readWav (click); std::printf ("      click.wav: %d muestras, sr=%.0f, pico=%.3f\n", c.buf.getNumSamples(), c.sr, peakIn (c, 0, 1)); }

    auto edit = te::createEmptyEdit (engine, dir.getChildFile ("t2.tracktionedit"));
    edit->tempoSequence.getTempo (0)->setBpm (120.0);   // 16avo = 0.125 s
    edit->ensureNumberOfAudioTracks (1);
    auto* track = te::getAudioTracks (*edit)[0];

    te::Plugin::Ptr samplerPtr = edit->getPluginCache().createNewPlugin (te::SamplerPlugin::xmlTypeName, {});
    auto* sampler = dynamic_cast<te::SamplerPlugin*> (samplerPtr.get());
    check (sampler != nullptr, "se crea el sampler de pruebas");
    if (sampler == nullptr) return;
    track->pluginList.insertPlugin (sampler, 0, nullptr);
    auto err = sampler->addSound (click.getFullPathName(), "click", 0.0, 0.0, 1.0f);
    std::printf ("      addSound -> '%s'  sonidos=%d  archivo=%d bytes\n", err.toRawUTF8(), sampler->getNumSounds(), (int) click.getSize());
    sampler->setSoundParams (0, 36, 36, 36);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (800);   // el motor carga la info del archivo en segundo plano
    edit->getTransport().ensureContextAllocated();
    { auto af = sampler->getSoundFile (0);
      std::printf ("      archivo valido=%d  largo=%.3f s  sr=%.0f  nota=%d..%d\n", (int) af.isValid(), af.getLength(), af.getSampleRate(), sampler->getMinKey (0), sampler->getMaxKey (0)); }

    auto clipBase = track->insertNewClip (te::TrackItem::Type::step, "pasos", te::TimeRange (te::TimePosition(), te::TimeDuration::fromSeconds (2.0)), nullptr);
    auto* clip = dynamic_cast<te::StepClip*> (clipBase);
    check (clip != nullptr, "se crea el StepClip");
    if (clip == nullptr) return;

    check (clip->getChannels().size() > 0, "el StepClip trae canales");
    auto& chan = *clip->getChannels()[0];
    chan.noteNumber = 36;
    auto pat = clip->getPattern (0);
    pat.setNumNotes (16);
    for (int i : { 0, 4, 8, 12 }) pat.setNote (0, i, true);   // 4 por compás: golpes en 0, 0.5, 1.0, 1.5 s

    if (std::getenv ("PROBE_MIDI_SAMPLER"))   // diagnóstico: ¿suena el sampler con un clip MIDI?
    {
        auto mc = track->insertMIDIClip ("diag", te::TimeRange (te::TimePosition(), te::TimeDuration::fromSeconds (2.0)), nullptr);
        mc->getSequence().addNote (36, te::BeatPosition::fromBeats (0.0), te::BeatDuration::fromBeats (0.25), 100, 0, nullptr);
    }
    auto out = dir.getChildFile ("t2.wav");
    check (te::Renderer::renderToFile (*edit, out, false), "render offline termina");
    auto w = readWav (out);
    check (w.buf.getNumSamples() > 0, "el WAV tiene muestras");
    std::printf ("      pico global=%.4f  canales=%d  nota canal0=%d  paso0=%d\n", peakIn (w, 0, 10), clip->getChannels().size(), (int) chan.noteNumber.get(), (int) pat.getNote (0, 0));
    for (double t : { 0.0, 0.5, 1.0, 1.5 })
        check (peakIn (w, t, t + 0.04) > 0.1f, "golpe en t=" + juce::String (t, 2) + " s");
    for (double t : { 0.125, 0.25, 0.625, 1.25 })
        check (peakIn (w, t, t + 0.04) < 0.01f, "sin golpe en t=" + juce::String (t, 3) + " s");
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("injustice_probe");
    dir.deleteRecursively(); dir.createDirectory();

    te::Engine engine { "InjusticeProbe" };
    test1_midi (engine, dir);
    test2_steps (engine, dir);

    std::printf (failures == 0 ? "\nTODO OK\n" : "\n%d FALLOS\n", failures);
    return failures == 0 ? 0 : 1;
}
