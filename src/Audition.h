#pragma once
#include <tracktion_engine/tracktion_engine.h>
#include <vector>

// Escuchar notas al momento (piano roll, instrumentos, secuenciador): inyecta MIDI en vivo en una pista y apaga la nota después.
namespace audition
{
    namespace te = tracktion;
    inline bool typingPiano = false;   // teclado de la computadora como piano (Ctrl+T)

    class Player : private juce::Timer
    {
    public:
        struct Ev { int pitch, on, off; bool started = false, stopped = false; };   // milisegundos desde el inicio
        ~Player() override { stopAll(); }

        void play (te::AudioTrack* t, std::vector<Ev> evs, int vel = 100)
        {
            stopAll(); if (t == nullptr || evs.empty()) return;
            if (t->edit.engine.getDeviceManager().deviceManager.getCurrentAudioDevice() == nullptr) return;   // sin tarjeta de audio no hay nada que escuchar
            t->edit.getTransport().ensureContextAllocated(); if (! t->edit.getTransport().isPlayContextActive()) return;
            track = t; events = std::move (evs); velocity = vel; t0 = juce::Time::getMillisecondCounter(); startTimerHz (100); tick();
        }
        void note (te::AudioTrack* t, int pitch, int ms = 400) { play (t, { { pitch, 0, ms } }); }
        void stopAll()
        {
            stopTimer();
            if (auto* at = dynamic_cast<te::AudioTrack*> (track.get()))
                for (auto& e : events) if (e.started && ! e.stopped) at->injectLiveMidiMessage (juce::MidiMessage::noteOff (1, e.pitch), {});
            events.clear(); track = nullptr; sent = 0;
        }
        int notesSent() const { return sent; }   // para las pruebas
    private:
        te::Track::Ptr track; std::vector<Ev> events; int velocity = 100; juce::uint32 t0 = 0; int sent = 0;
        void timerCallback() override { tick(); }
        void tick()
        {
            auto* at = dynamic_cast<te::AudioTrack*> (track.get()); if (at == nullptr) { stopTimer(); return; }
            const int now = (int) (juce::Time::getMillisecondCounter() - t0); bool pending = false;
            for (auto& e : events)
            {
                if (! e.started && now >= e.on) { at->injectLiveMidiMessage (juce::MidiMessage::noteOn (1, e.pitch, (juce::uint8) velocity), {}); e.started = true; ++sent; }
                if (e.started && ! e.stopped && now >= e.off) { at->injectLiveMidiMessage (juce::MidiMessage::noteOff (1, e.pitch), {}); e.stopped = true; }
                if (! e.stopped) pending = true;
            }
            if (! pending) { stopTimer(); events.clear(); track = nullptr; }
        }
    };
}
