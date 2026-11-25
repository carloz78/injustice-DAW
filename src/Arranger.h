#pragma once
#include <tracktion_engine/tracktion_engine.h>
#include "TrackUtil.h"
#include <algorithm>
#include <functional>
#include <vector>

// Pista de arreglo: secciones con nombre (Intro, Build, Drop...) que se guardan en el proyecto como hijos INJSECTION de edit.state.
// Reordenar una sección mueve con ella los clips, los marcadores y la automatización que contiene.
namespace arranger
{
    namespace te = tracktion;

    struct Sec { juce::ValueTree v; double beat = 0, len = 0; juce::String name; int col = 0; double end() const { return beat + len; } };

    inline const juce::Colour& colourOf (int i)
    {
        static const juce::Colour c[] { juce::Colour (0xfff08c22), juce::Colour (0xff5b9bd5), juce::Colour (0xff4cc38a), juce::Colour (0xffc678dd), juce::Colour (0xffe5484d), juce::Colour (0xff3fb8b0), juce::Colour (0xffd6b94a), juce::Colour (0xff8d909a) };
        return c[((i % 8) + 8) % 8];
    }
    inline juce::String defaultName (int n)
    {
        static const char* nm[] { "Intro", "Build", "Drop", "Break", "Drop 2", "Outro" };
        return n < 6 ? juce::String (nm[n]) : juce::String ("Secci\xc3\xb3n ") + juce::String (n + 1);
    }

    inline std::vector<Sec> list (te::Edit& e)
    {
        std::vector<Sec> out;
        for (auto c : e.state) if (c.hasType ("INJSECTION")) out.push_back ({ c, (double) c.getProperty ("beat", 0.0), (double) c.getProperty ("len", 4.0), c.getProperty ("name", "").toString(), (int) c.getProperty ("col", 0) });
        std::sort (out.begin(), out.end(), [] (const Sec& a, const Sec& b) { return a.beat < b.beat; });
        return out;
    }
    inline int indexAt (te::Edit& e, double beat) { auto s = list (e); for (int i = 0; i < (int) s.size(); ++i) if (beat >= s[(size_t) i].beat - 1e-9 && beat < s[(size_t) i].end() - 1e-9) return i; return -1; }

    inline double toBeats (te::Edit& e, te::TimePosition t) { return e.tempoSequence.toBeats (t).inBeats(); }
    inline te::TimePosition toTime (te::Edit& e, double b) { return e.tempoSequence.toTime (te::BeatPosition::fromBeats (b)); }

    // Aplica una función de tiempo (en pulsos) a clips, marcadores y puntos de automatización. Los clips se mueven por su inicio.
    inline int remap (te::Edit& e, const std::function<double (double)>& f)
    {
        int moved = 0; std::vector<std::pair<te::Clip*, double>> todo;
        for (auto* t : te::getAudioTracks (e)) for (auto* c : t->getClips()) { const double b = toBeats (e, c->getPosition().getStart()), nb = f (b); if (std::abs (nb - b) > 1e-9) todo.push_back ({ c, nb }); }
        for (auto& p : todo) { p.first->setStart (toTime (e, std::max (0.0, p.second)), false, true); ++moved; }
        for (auto c : e.state) if (c.hasType ("INJMARKER")) { const double b = (double) c.getProperty ("beat", 0.0), nb = f (b); if (std::abs (nb - b) > 1e-9) c.setProperty ("beat", std::max (0.0, nb), &e.getUndoManager()); }
        for (auto* t : te::getAudioTracks (e))
            for (auto* p : t->getAllAutomatableParams())
            {
                auto& cv = p->getCurve(); const int n = cv.getNumPoints(); if (n == 0) continue;
                struct P { double b; float v, c; }; std::vector<P> pts; bool changed = false;
                for (int i = 0; i < n; ++i) { const double b = toBeats (e, cv.getPointTime (i)), nb = f (b); changed = changed || std::abs (nb - b) > 1e-9; pts.push_back ({ std::max (0.0, nb), cv.getPointValue (i), cv.getPointCurve (i) }); }
                if (! changed) continue;
                cv.clear(); for (auto& q : pts) cv.addPoint (toTime (e, q.b), q.v, q.c);
            }
        return moved;
    }

    inline bool add (te::Edit& e, double beat, double len, const juce::String& name = {}, int col = -1)
    {
        beat = std::max (0.0, beat); if (len < 1.0) return false; auto s = list (e); if (indexAt (e, beat) >= 0) return false;
        for (auto& x : s) if (x.beat > beat + 1e-9) { len = std::min (len, x.beat - beat); break; }
        if (len < 1.0 - 1e-9) return false;
        juce::ValueTree v ("INJSECTION"); v.setProperty ("beat", beat, nullptr); v.setProperty ("len", len, nullptr);
        v.setProperty ("name", name.isEmpty() ? defaultName ((int) s.size()) : name, nullptr); v.setProperty ("col", col >= 0 ? col : (int) s.size(), nullptr);
        e.getUndoManager().beginNewTransaction ("Nueva secci\xc3\xb3n"); e.state.addChild (v, -1, &e.getUndoManager()); return true;
    }
    inline void remove (te::Edit& e, int i) { auto s = list (e); if (i < 0 || i >= (int) s.size()) return; e.getUndoManager().beginNewTransaction ("Quitar secci\xc3\xb3n"); e.state.removeChild (s[(size_t) i].v, &e.getUndoManager()); }
    inline void clearAll (te::Edit& e) { auto s = list (e); if (s.empty()) return; e.getUndoManager().beginNewTransaction ("Quitar secciones"); for (auto& x : s) e.state.removeChild (x.v, &e.getUndoManager()); }
    inline void rename (te::Edit& e, int i, const juce::String& n) { auto s = list (e); if (i < 0 || i >= (int) s.size() || n.trim().isEmpty()) return; e.getUndoManager().beginNewTransaction ("Renombrar secci\xc3\xb3n"); s[(size_t) i].v.setProperty ("name", n.trim(), &e.getUndoManager()); }
    inline void cycleColour (te::Edit& e, int i) { auto s = list (e); if (i < 0 || i >= (int) s.size()) return; e.getUndoManager().beginNewTransaction ("Color de secci\xc3\xb3n"); s[(size_t) i].v.setProperty ("col", (s[(size_t) i].col + 1) % 8, &e.getUndoManager()); }
    // Cambia la duración (en pulsos) sin pisar a la siguiente. Devuelve la duración final.
    inline double setLen (te::Edit& e, int i, double len)
    {
        auto s = list (e); if (i < 0 || i >= (int) s.size()) return 0; len = std::max (1.0, len);
        if (i + 1 < (int) s.size()) len = std::min (len, s[(size_t) i + 1].beat - s[(size_t) i].beat);
        e.getUndoManager().beginNewTransaction ("Duraci\xc3\xb3n de secci\xc3\xb3n"); s[(size_t) i].v.setProperty ("len", len, &e.getUndoManager()); return len;
    }

    // Intercambia la sección i con la siguiente: lo que hay en medio (huecos) se conserva entre las dos.
    inline bool swapNext (te::Edit& e, int i)
    {
        auto s = list (e); if (i < 0 || i + 1 >= (int) s.size()) return false;
        const double a0 = s[(size_t) i].beat, la = s[(size_t) i].len, b0 = s[(size_t) i + 1].beat, lb = s[(size_t) i + 1].len, g = b0 - (a0 + la), span = b0 + lb;
        auto f = [=] (double t) { if (t < a0 - 1e-9 || t >= span - 1e-9) return t; if (t < a0 + la - 1e-9) return t + lb + g; if (t < b0 - 1e-9) return t + (lb - la); return t - (b0 - a0); };
        remap (e, f);
        s[(size_t) i + 1].v.setProperty ("beat", a0, &e.getUndoManager()); s[(size_t) i].v.setProperty ("beat", a0 + lb + g, &e.getUndoManager());
        return true;
    }
    // Lleva la sección `from` a la posición `to` (índices en orden de aparición) con una sola acción de deshacer.
    inline bool move (te::Edit& e, int from, int to)
    {
        const int n = (int) list (e).size(); if (from < 0 || from >= n || to < 0 || to >= n || from == to) return false;
        e.getUndoManager().beginNewTransaction ("Mover secci\xc3\xb3n");
        for (int i = from; i < to; ++i) swapNext (e, i);
        for (int i = from - 1; i >= to; --i) swapNext (e, i);
        return true;
    }
    // Copia la sección justo detrás de sí misma; todo lo posterior se desplaza para hacerle sitio.
    inline bool duplicate (te::Edit& e, int i)
    {
        auto s = list (e); if (i < 0 || i >= (int) s.size()) return false;
        const auto sec = s[(size_t) i]; const double a = sec.beat, b = sec.end(), L = sec.len;
        { int total = 0, inSec = 0; for (auto* t : te::getAudioTracks (e)) for (auto* c : t->getClips()) { ++total; const double cb = toBeats (e, c->getPosition().getStart()); if (cb >= a - 1e-9 && cb < b - 1e-9) ++inSec; } if (total + inSec > 3000) return false; }   // tope: evita que duplicar una y otra vez agote la memoria
        e.getUndoManager().beginNewTransaction ("Duplicar secci\xc3\xb3n");
        std::vector<std::pair<te::Clip*, te::ClipTrack*>> src;
        for (auto* t : te::getAudioTracks (e)) for (auto* c : t->getClips()) { const double cb = toBeats (e, c->getPosition().getStart()); if (cb >= a - 1e-9 && cb < b - 1e-9) src.push_back ({ c, t }); }
        std::vector<te::Clip*> srcClips; for (auto& p : src) srcClips.push_back (p.first);
        std::vector<std::pair<te::ClipTrack*, std::pair<juce::ValueTree, double>>> copies;
        for (auto& p : src) { auto xml = p.first->state.createXml(); if (! xml) continue; te::EditItemID::remapIDs (*xml, e); copies.push_back ({ p.second, { juce::ValueTree::fromXml (*xml), toBeats (e, p.first->getPosition().getStart()) + L } }); }
        std::vector<std::pair<double, juce::String>> mk; for (auto c : e.state) if (c.hasType ("INJMARKER")) { const double mb = (double) c.getProperty ("beat", 0.0); if (mb >= a - 1e-9 && mb < b - 1e-9) mk.push_back ({ mb + L, c.getProperty ("name", "").toString() }); }
        remap (e, [=] (double t) { return t >= b - 1e-9 ? t + L : t; });
        for (auto x : list (e)) if (x.beat >= b - 1e-9 && x.v != sec.v) x.v.setProperty ("beat", x.beat + L, &e.getUndoManager());
        for (auto& c : copies) if (auto* nc = c.first->insertClipWithState (c.second.first)) nc->setStart (toTime (e, c.second.second), false, true);
        for (auto& m : mk) { juce::ValueTree v ("INJMARKER"); v.setProperty ("beat", m.first, nullptr); v.setProperty ("name", m.second, nullptr); e.state.addChild (v, -1, &e.getUndoManager()); }
        juce::ValueTree v ("INJSECTION"); v.setProperty ("beat", b, nullptr); v.setProperty ("len", L, nullptr); v.setProperty ("name", sec.name + " 2", nullptr); v.setProperty ("col", sec.col, nullptr);
        e.state.addChild (v, -1, &e.getUndoManager()); return true;
    }
    // Una sección por cada marcador (hasta el siguiente); la última dura `lastLen` pulsos.
    inline int fromMarkers (te::Edit& e, double lastLen)
    {
        std::vector<std::pair<double, juce::String>> ms; for (auto c : e.state) if (c.hasType ("INJMARKER")) ms.push_back ({ (double) c.getProperty ("beat", 0.0), c.getProperty ("name", "").toString() });
        std::sort (ms.begin(), ms.end(), [] (auto& a, auto& b) { return a.first < b.first; }); int n = 0;
        for (size_t k = 0; k < ms.size(); ++k) { const double len = k + 1 < ms.size() ? ms[k + 1].first - ms[k].first : lastLen; if (add (e, ms[k].first, len, ms[k].second.containsOnly ("0123456789") ? juce::String() : ms[k].second)) ++n; }
        return n;
    }
}
