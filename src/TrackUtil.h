#pragma once
#include "Theme.h"
#include <tracktion_engine/tracktion_engine.h>
namespace te = tracktion;

// Utilidades compartidas por la Playlist y el Mixer.
namespace trackutil
{
    inline juce::Colour colourOf (te::AudioTrack* t, int index)
    {
        const int ci = t ? (int) t->state.getProperty ("injColour", index % 8) : index % 8;
        return theme::trackColours[((ci % 8) + 8) % 8];
    }
    inline void setColour (te::Edit& e, te::AudioTrack* t, int ci) { if (t) t->state.setProperty ("injColour", ci, &e.getUndoManager()); }

    inline void renameDialog (te::Edit& e, te::AudioTrack* t, std::function<void()> done)
    {
        if (t == nullptr) return;
        auto* aw = new juce::AlertWindow (juce::String::fromUTF8 ("Renombrar pista"), "", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("n", t->getName() == "Track" ? juce::String() : t->getName()); aw->addButton ("Aceptar", 1, juce::KeyPress (juce::KeyPress::returnKey)); aw->addButton ("Cancelar", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        te::AudioTrack::Ptr keep (t);
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([&e, keep, aw, done] (int r)
        {
            if (r == 1) { auto n = aw->getTextEditorContents ("n").trim(); keep->setName (n.isEmpty() ? juce::String ("Track") : n); if (done) done(); }
            delete aw; (void) e;
        }));
    }

    // Crea pistas hasta llegar a n. Si el motor deja de crear (límite), se detiene en lugar de quedarse en un bucle infinito.
    inline int ensureTracks (te::Edit& e, int n)
    {
        int count = te::getAudioTracks (e).size();
        while (count < n)
        {
            e.getTransport().stopIfRecording();
            if (e.insertNewAudioTrack (te::TrackInsertPoint::getEndOfTracks (e), nullptr) == nullptr) break;
            const int now = te::getAudioTracks (e).size(); if (now <= count) break; count = now;
        }
        return count;
    }
    inline te::AudioTrack* addTrack (te::Edit& e) { const int n = te::getAudioTracks (e).size(); if (ensureTracks (e, n + 1) <= n) return nullptr; auto t = te::getAudioTracks (e); return t[n]; }

    inline bool deleteTrack (te::Edit& e, te::AudioTrack* t)
    {
        if (t == nullptr || te::getAudioTracks (e).size() <= 1) return false;
        e.deleteTrack (t); return true;
    }

    // Copia un clip a otra pista (o a la misma) empezando en `start`.
    inline te::Clip* copyClipTo (te::Edit& e, te::Clip& c, te::ClipTrack& dest, te::TimePosition start)
    {
        auto xml = c.state.createXml(); if (xml == nullptr) return nullptr;
        te::EditItemID::remapIDs (*xml, e); auto vt = juce::ValueTree::fromXml (*xml);
        auto* nc = dest.insertClipWithState (vt); if (nc == nullptr) return nullptr;
        nc->setStart (start, false, true); return nc;
    }
    inline te::Clip* duplicateClip (te::Edit& e, te::Clip& c)
    {
        auto* track = dynamic_cast<te::ClipTrack*> (c.getTrack()); if (track == nullptr) return nullptr;
        auto xml = c.state.createXml(); if (xml == nullptr) return nullptr;
        te::EditItemID::remapIDs (*xml, e); auto vt = juce::ValueTree::fromXml (*xml);
        auto* nc = track->insertClipWithState (vt); if (nc == nullptr) return nullptr;
        nc->setStart (c.getPosition().getEnd(), false, true); return nc;
    }

    // ---- Grabación ----
    inline bool hasInstrument (te::AudioTrack& t)
    {
        for (auto* p : t.pluginList.getPlugins()) { if (dynamic_cast<te::SamplerPlugin*> (p)) return true; if (auto* ep = dynamic_cast<te::ExternalPlugin*> (p)) if (ep->isSynth()) return true; }
        return false;
    }
    inline bool isArmed (te::AudioTrack& t) { return (bool) t.state.getProperty ("injArm", false); }
    inline bool anyArmed (te::Edit& e) { for (auto* t : te::getAudioTracks (e)) if (isArmed (*t)) return true; return false; }
    // Arma o desarma una pista: con instrumento graba MIDI de todos los teclados conectados; sin instrumento, audio de la primera entrada.
    // Devuelve cuántas entradas quedaron asignadas a la pista.
    inline int armTrack (te::Edit& e, te::AudioTrack& t, bool arm)
    {
        e.getTransport().ensureContextAllocated (true);
        const bool wantMidi = hasInstrument (t); bool waveDone = false; int n = 0;
        for (auto* inst : e.getAllInputDevices())
        {
            auto& dev = inst->getInputDevice(); const auto type = dev.getDeviceType();
            const bool midi = type != te::InputDevice::waveDevice; if (midi != wantMidi) continue;
            if (! midi && waveDone && arm) continue;
            if (arm)
            {
                if (! dev.isEnabled()) dev.setEnabled (true);
                auto r = inst->setTarget (t.itemID, false, &e.getUndoManager(), 0); if (! r.has_value()) continue;
                inst->setRecordingEnabled (t.itemID, true); ++n; if (! midi) waveDone = true;
            }
            else if (inst->getTargets().contains (t.itemID)) { inst->setRecordingEnabled (t.itemID, false); (void) inst->removeTarget (t.itemID, &e.getUndoManager()); }
        }
        t.state.setProperty ("injArm", arm && n > 0, nullptr);
        return n;
    }

    // ---- MIDI: exportar un clip a .mid e importar un .mid como clip ----
    inline bool exportMidiClip (te::Edit& e, te::MidiClip& mc, const juce::File& f)
    {
        juce::MidiMessageSequence seq; const int tpq = 960;
        for (auto* n : mc.getSequence().getNotes())
        {
            const double s = n->getStartBeat().inBeats() * tpq, l = n->getLengthBeats().inBeats() * tpq;
            seq.addEvent (juce::MidiMessage::noteOn (1, n->getNoteNumber(), (juce::uint8) n->getVelocity()), s);
            seq.addEvent (juce::MidiMessage::noteOff (1, n->getNoteNumber()), s + std::max (1.0, l));
        }
        seq.updateMatchedPairs(); juce::MidiMessageSequence tempo; tempo.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / std::max (20.0, e.tempoSequence.getTempo (0)->getBpm()))), 0.0);
        juce::MidiFile mf; mf.setTicksPerQuarterNote (tpq); mf.addTrack (tempo); mf.addTrack (seq);
        f.deleteFile(); if (auto os = f.createOutputStream()) return mf.writeTo (*os); return false;
    }
    // Devuelve el clip creado (o nullptr). Los compases del archivo se respetan aunque su tempo sea otro.
    inline te::MidiClip* importMidiFile (te::Edit& e, const juce::File& f, te::AudioTrack& track, te::TimePosition start)
    {
        juce::FileInputStream in (f); if (! in.openedOk()) return nullptr; juce::MidiFile mf; if (! mf.readFrom (in)) return nullptr;
        const int tpq = mf.getTimeFormat(); if (tpq <= 0) return nullptr;
        juce::MidiMessageSequence all; for (int i = 0; i < mf.getNumTracks(); ++i) all.addSequence (*mf.getTrack (i), 0.0);
        juce::MidiMessageSequence conv;
        for (auto* ev : all) { auto m = ev->message; if (m.isNoteOnOrOff() || m.isController() || m.isPitchWheel()) { m.setTimeStamp (e.tempoSequence.toTime (te::BeatPosition::fromBeats (m.getTimeStamp() / (double) tpq)).inSeconds()); conv.addEvent (m); } }
        conv.updateMatchedPairs(); if (conv.getNumEvents() == 0) return nullptr;
        const double len = conv.getEndTime();
        auto clip = track.insertMIDIClip (f.getFileNameWithoutExtension(), te::TimeRange (start, te::TimePosition::fromSeconds (start.inSeconds() + std::max (1.0, len))), nullptr); if (clip == nullptr) return nullptr;
        te::mergeInMidiSequence (*clip, conv, te::TimeDuration::fromSeconds (start.inSeconds()), te::MidiList::NoteAutomationType::none);
        return clip.get();
    }
}
