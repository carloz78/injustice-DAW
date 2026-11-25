#pragma once
#include <tracktion_engine/tracktion_engine.h>
#include <functional>
#include <memory>
#include <vector>

// MIDI learn: asigna un mando (CC) de cualquier controlador a un parámetro. Las asignaciones se guardan en el proyecto
// (hijos INJMIDIMAP de edit.state). Escucha los dispositivos MIDI que ya maneja el motor.
namespace midimap
{
    namespace te = tracktion;

    struct Target { juce::String kind; juce::String trackId; };   // kind: vol, pan, mute, solo, auto, master, masterpan, bpm
    struct Map { juce::ValueTree v; int cc = 0, ch = 0; Target t; };

    class Mapper : private te::PhysicalMidiInputDevice::Listener
    {
    public:
        std::function<te::Edit*()> getEdit;
        std::function<te::AutomatableParameter* (te::AudioTrack&)> autoParam;   // parámetro "automatizado" de la pista (puede ser de un plugin)
        std::function<void (const juce::String&)> onMessage;
        std::function<void()> onChanged;

        explicit Mapper (te::Engine& e) : engine (e), alive (std::make_shared<bool> (true)) {}
        ~Mapper() override { *alive = false; detach(); }

        // --- estado de aprendizaje ---
        bool learning() const { return pending; }
        juce::String pendingText() const { return pending ? describe (pendingTarget) : juce::String(); }
        void learn (const Target& t) { pendingTarget = t; pending = true; if (onMessage) onMessage (juce::String::fromUTF8 ("Mueve un mando del controlador para asignarlo a: ") + describe (t)); }
        void cancelLearn() { pending = false; }

        // --- enlaces ---
        std::vector<Map> list() const
        {
            std::vector<Map> out; auto* e = getEdit ? getEdit() : nullptr; if (e == nullptr) return out;
            for (auto c : e->state) if (c.hasType ("INJMIDIMAP")) out.push_back ({ c, (int) c.getProperty ("cc", 0), (int) c.getProperty ("ch", 0), { c.getProperty ("kind", "").toString(), c.getProperty ("track", "").toString() } });
            return out;
        }
        void remove (int i) { auto e = getEdit(); auto m = list(); if (i < 0 || i >= (int) m.size()) return; e->getUndoManager().beginNewTransaction ("Quitar asignaci\xc3\xb3n MIDI"); e->state.removeChild (m[(size_t) i].v, &e->getUndoManager()); if (onChanged) onChanged(); }
        void clear() { auto e = getEdit(); auto m = list(); if (m.empty()) return; e->getUndoManager().beginNewTransaction ("Quitar asignaciones MIDI"); for (auto& x : m) e->state.removeChild (x.v, &e->getUndoManager()); if (onChanged) onChanged(); }
        static bool same (const Target& a, const Target& b) { return a.kind == b.kind && a.trackId == b.trackId; }

        juce::String describe (const Target& t) const
        {
            auto* e = getEdit ? getEdit() : nullptr; juce::String nm;
            if (e != nullptr && t.trackId.isNotEmpty()) for (auto* tr : te::getAudioTracks (*e)) if (tr->itemID.toString() == t.trackId) nm = tr->getName();
            const juce::String tn = nm.isNotEmpty() ? juce::String::fromUTF8 (" \xc2\xb7 ") + nm : juce::String();
            if (t.kind == "vol") return "Volumen" + tn; if (t.kind == "pan") return "Panorama" + tn; if (t.kind == "mute") return "Silenciar" + tn; if (t.kind == "solo") return "Solo" + tn;
            if (t.kind == "auto") return juce::String::fromUTF8 ("Par\xc3\xa1metro automatizado") + tn; if (t.kind == "master") return "Volumen general"; if (t.kind == "masterpan") return "Panorama general"; if (t.kind == "bpm") return "Tempo";
            return t.kind;
        }
        juce::String describe (const Map& m) const { return "CC " + juce::String (m.cc) + (m.ch > 0 ? " (canal " + juce::String (m.ch) + ")" : juce::String()) + juce::String::fromUTF8 (" \xe2\x86\x92 ") + describe (m.t); }

        // Procesa un mensaje (hilo de mensajes). Devuelve true si lo consumió (aprendió o aplicó).
        bool handle (const juce::MidiMessage& m)
        {
            if (! m.isController()) return false; auto* e = getEdit ? getEdit() : nullptr; if (e == nullptr) return false;
            const int cc = m.getControllerNumber(), ch = m.getChannel(), val = m.getControllerValue();
            if (pending)
            {
                pending = false; auto all = list(); e->getUndoManager().beginNewTransaction ("Asignaci\xc3\xb3n MIDI");
                for (auto& x : all) if (same (x.t, pendingTarget) || (x.cc == cc && x.ch == 0 && same (x.t, pendingTarget))) e->state.removeChild (x.v, &e->getUndoManager());   // un destino = un mando
                for (auto& x : list()) if (x.cc == cc && (x.ch == ch || x.ch == 0)) e->state.removeChild (x.v, &e->getUndoManager());              // un mando = un destino
                juce::ValueTree v ("INJMIDIMAP"); v.setProperty ("cc", cc, nullptr); v.setProperty ("ch", 0, nullptr); v.setProperty ("kind", pendingTarget.kind, nullptr); v.setProperty ("track", pendingTarget.trackId, nullptr);
                e->state.addChild (v, -1, &e->getUndoManager());
                if (onMessage) onMessage ("CC " + juce::String (cc) + juce::String::fromUTF8 (" \xe2\x86\x92 ") + describe (pendingTarget)); if (onChanged) onChanged(); return true;
            }
            bool used = false;
            for (auto& x : list()) if (x.cc == cc && (x.ch == 0 || x.ch == ch)) { apply (x.t, (float) val / 127.f); used = true; }
            return used;
        }

        // Aplica un valor 0..1 al destino. Devuelve false si el destino ya no existe.
        bool apply (const Target& t, float v01)
        {
            auto* e = getEdit ? getEdit() : nullptr; if (e == nullptr) return false; v01 = juce::jlimit (0.f, 1.f, v01);
            auto dbOf = [] (float v) { return v <= 0.001f ? -100.f : juce::jlimit (-100.f, 6.f, juce::Decibels::gainToDecibels (v * v * 1.995f)); };
            if (t.kind == "master") { e->getMasterVolumePlugin()->setVolumeDb (dbOf (v01)); return true; }
            if (t.kind == "masterpan") { e->getMasterVolumePlugin()->setPan (v01 * 2.f - 1.f); return true; }
            if (t.kind == "bpm") { e->tempoSequence.getTempo (0)->setBpm (60.0 + (double) v01 * 190.0); return true; }
            te::AudioTrack* tr = nullptr; for (auto* x : te::getAudioTracks (*e)) if (x->itemID.toString() == t.trackId) tr = x; if (tr == nullptr) return false;
            if (t.kind == "vol") { if (auto* vp = tr->getVolumePlugin()) vp->setVolumeDb (dbOf (v01)); return true; }
            if (t.kind == "pan") { if (auto* vp = tr->getVolumePlugin()) vp->setPan (v01 * 2.f - 1.f); return true; }
            if (t.kind == "mute") { tr->setMute (v01 >= .5f); return true; }
            if (t.kind == "solo") { tr->setSolo (v01 >= .5f); return true; }
            if (t.kind == "auto") { if (autoParam) if (auto* p = autoParam (*tr)) { p->setNormalisedParameter (v01, juce::sendNotification); return true; } return false; }
            return false;
        }

        // Escucha los dispositivos MIDI de entrada del motor (se vuelve a llamar de vez en cuando por si se conecta uno nuevo).
        void attach()
        {
            for (auto& d : engine.getDeviceManager().getMidiInDevices())
                if (auto* p = dynamic_cast<te::PhysicalMidiInputDevice*> (d.get()))
                {
                    bool have = false; for (auto& a : attached) have = have || a.get() == d.get(); if (have) continue;
                    p->addListener (this); attached.push_back (d);
                    if (! p->isEnabled()) p->setEnabled (true);
                }
        }
        int numDevices() const { return (int) attached.size(); }

    private:
        te::Engine& engine; std::shared_ptr<bool> alive; bool pending = false; Target pendingTarget; std::vector<std::shared_ptr<te::MidiInputDevice>> attached;
        void detach() { for (auto& d : attached) if (auto* p = dynamic_cast<te::PhysicalMidiInputDevice*> (d.get())) p->removeListener (this); attached.clear(); }
        void handleIncomingMidiMessage (const juce::MidiMessage& m) override   // hilo MIDI: se pasa al hilo de mensajes
        {
            if (! m.isController()) return; auto flag = alive; juce::MidiMessage copy (m);
            juce::MessageManager::callAsync ([this, flag, copy] { if (*flag) handle (copy); });
        }
    };
}
