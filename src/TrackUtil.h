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
}
