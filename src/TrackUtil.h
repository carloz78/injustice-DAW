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
        for (auto* p : t.pluginList.getPlugins()) { if (dynamic_cast<te::SamplerPlugin*> (p) || dynamic_cast<te::FourOscPlugin*> (p)) return true; if (auto* ep = dynamic_cast<te::ExternalPlugin*> (p)) if (ep->isSynth()) return true; }
        return false;
    }
    // ---- grupos de pistas (se guardan en el proyecto como propiedades de la pista) ----
    inline juce::String groupOf (te::AudioTrack& t) { return t.state.getProperty ("injGroup", "").toString(); }
    inline juce::String groupName (te::AudioTrack& t) { return t.state.getProperty ("injGroupName", "Grupo").toString(); }
    inline juce::Array<te::AudioTrack*> groupMembers (te::Edit& e, const juce::String& id)
    {
        juce::Array<te::AudioTrack*> v; if (id.isEmpty()) return v;
        for (auto* t : te::getAudioTracks (e)) if (groupOf (*t) == id) v.add (t);
        return v;
    }
    inline int groupColour (const juce::String& id) { return id.isEmpty() ? 0 : (int) (((unsigned) id.hashCode()) % 8u); }
    // une t al grupo de other (o crea uno nuevo con las dos)
    inline void joinGroup (te::Edit& e, te::AudioTrack& t, te::AudioTrack& other, const juce::String& newName = {})
    {
        auto id = groupOf (other); auto* um = &e.getUndoManager();
        if (id.isEmpty())
        {
            int n = 1; juce::StringArray used; for (auto* x : te::getAudioTracks (e)) if (groupOf (*x).isNotEmpty()) used.addIfNotAlreadyThere (groupOf (*x)); n = used.size() + 1;
            id = "g" + juce::String (juce::Time::currentTimeMillis()) + "_" + juce::String (n);
            other.state.setProperty ("injGroup", id, um); other.state.setProperty ("injGroupName", newName.isNotEmpty() ? newName : "Grupo " + juce::String (n), um);
        }
        t.state.setProperty ("injGroup", id, um); t.state.setProperty ("injGroupName", groupName (other), um);
    }
    inline void leaveGroup (te::Edit& e, te::AudioTrack& t)
    {
        auto id = groupOf (t); auto* um = &e.getUndoManager(); t.state.removeProperty ("injGroup", um); t.state.removeProperty ("injGroupName", um);
        auto rest = groupMembers (e, id); if (rest.size() == 1) { rest[0]->state.removeProperty ("injGroup", um); rest[0]->state.removeProperty ("injGroupName", um); }   // un grupo de una sola pista se disuelve
    }
    inline void renameGroup (te::Edit& e, const juce::String& id, const juce::String& name) { for (auto* t : groupMembers (e, id)) t->state.setProperty ("injGroupName", name, &e.getUndoManager()); }
    inline void groupGainDb (te::Edit& e, const juce::String& id, float db) { for (auto* t : groupMembers (e, id)) if (auto* vp = t->getVolumePlugin()) vp->setVolumeDb (juce::jlimit (-60.f, 6.f, vp->getVolumeDb() + db)); }
    inline bool groupMuted (te::Edit& e, const juce::String& id) { auto m = groupMembers (e, id); return m.size() > 0 && m[0]->isMuted (false); }
    inline void groupMute (te::Edit& e, const juce::String& id, bool mute) { for (auto* t : groupMembers (e, id)) t->setMute (mute); }

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

    // ---- Sidechain real: un compresor interno baja esta pista cada vez que suena otra (típico: el bombo baja el bajo) ----
    inline te::CompressorPlugin* sidechainPlugin (te::AudioTrack& t)
    {
        for (auto* p : t.pluginList.getPlugins()) if (auto* c = dynamic_cast<te::CompressorPlugin*> (p)) if ((bool) c->state.getProperty ("injSidechain", false)) return c;
        return nullptr;
    }
    inline te::AudioTrack* sidechainSource (te::Edit& e, te::AudioTrack& t)
    {
        if (auto* c = sidechainPlugin (t)) { const auto id = c->getSidechainSourceID(); for (auto* o : te::getAudioTracks (e)) if (o->itemID == id) return o; }
        return nullptr;
    }
    inline double sidechainAmount (te::AudioTrack& t) { if (auto* c = sidechainPlugin (t)) return (double) c->state.getProperty ("injSCAmount", 0.7); return 0.0; }
    inline double sidechainRelease (te::AudioTrack& t) { if (auto* c = sidechainPlugin (t)) return (double) c->state.getProperty ("injSCRelease", 120.0); return 0.0; }
    inline void removeSidechain (te::AudioTrack& t) { if (auto* c = sidechainPlugin (t)) c->deleteFromParent(); }
    // amount 0..1 = cuánto baja (1 = casi silencio); releaseMs = qué tan rápido vuelve el volumen
    inline bool setSidechain (te::Edit& e, te::AudioTrack& dest, te::AudioTrack* src, double amount = 0.7, double releaseMs = 120.0)
    {
        if (src == nullptr || src == &dest) return false;
        auto* c = sidechainPlugin (dest);
        if (c == nullptr)
        {
            auto p = e.getPluginCache().createNewPlugin (te::CompressorPlugin::xmlTypeName, {}); c = dynamic_cast<te::CompressorPlugin*> (p.get()); if (c == nullptr) return false;
            int at = 0; auto pl = dest.pluginList.getPlugins(); for (int k = 0; k < pl.size(); ++k) if (dynamic_cast<te::SamplerPlugin*> (pl[k].get()) || dynamic_cast<te::FourOscPlugin*> (pl[k].get()) || dynamic_cast<te::ExternalPlugin*> (pl[k].get())) at = k + 1;
            dest.pluginList.insertPlugin (p, at, nullptr); c->state.setProperty ("injSidechain", true, nullptr);
        }
        amount = juce::jlimit (0.0, 1.0, amount); releaseMs = juce::jlimit (10.0, 300.0, releaseMs);
        c->useSidechainTrigger = true; c->setSidechainSourceID (src->itemID); c->setThreshold (0.02f); c->setRatio ((float) (1.0 - 0.95 * amount));
        c->attackMs.setParameter (0.5f, juce::sendNotification); c->releaseMs.setParameter ((float) releaseMs, juce::sendNotification); c->outputDb.setParameter (0.f, juce::sendNotification);
        c->state.setProperty ("injSCAmount", amount, nullptr); c->state.setProperty ("injSCRelease", releaseMs, nullptr);
        return true;
    }
}
