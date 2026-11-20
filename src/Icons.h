#pragma once
#include "Theme.h"

// Iconos vectoriales propios (dibujados con trazos; sin recursos de terceros).
namespace icons
{
    enum Id { NewFile, OpenFile, Layout, Undo, Redo, Play, Stop, Rec, Loop, Select, Pencil, Scissors, Stretch,
              Speaker, Metro, Magnet, Chevron, Playlist, Mixer, PianoRoll, Audio, Plugins, Export, Help, Eraser, Steps, Power, Sliders, Plus, Dots, Save, Trash, Plug, Brush };

    inline void draw (juce::Graphics& g, Id id, juce::Rectangle<float> r, juce::Colour c)
    {
        using juce::Path; using juce::PathStrokeType;
        const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.62f;
        auto b = juce::Rectangle<float> (s, s).withCentre (r.getCentre());
        auto P = [&] (float x, float y) { return juce::Point<float> (b.getX() + x * s, b.getY() + y * s); };
        g.setColour (c);
        const float lw = juce::jmax (1.6f, s * 0.11f);
        PathStrokeType st (lw, PathStrokeType::curved, PathStrokeType::rounded);
        Path p;
        switch (id)
        {
            case NewFile: p.startNewSubPath (P (.2f,.05f)); p.lineTo (P (.65f,.05f)); p.lineTo (P (.85f,.25f)); p.lineTo (P (.85f,.95f)); p.lineTo (P (.2f,.95f)); p.closeSubPath(); g.strokePath (p, st); break;
            case OpenFile: p.startNewSubPath (P (.05f,.25f)); p.lineTo (P (.4f,.25f)); p.lineTo (P (.5f,.38f)); p.lineTo (P (.95f,.38f)); p.lineTo (P (.95f,.85f)); p.lineTo (P (.05f,.85f)); p.closeSubPath(); g.strokePath (p, st); break;
            case Layout: for (int i = 0; i < 4; ++i) g.drawRoundedRectangle (P (i % 2 ? .55f : .08f, i / 2 ? .55f : .08f).x, P (0, i / 2 ? .55f : .08f).y, s * .37f, s * .37f, 2.f, lw); break;
            case Undo: case Redo: {
                const bool u = id == Undo; auto X = [&] (float x) { return u ? x : 1.f - x; };
                p.startNewSubPath (P (X (.2f), .35f)); p.lineTo (P (X (.2f), .12f)); p.startNewSubPath (P (X (.2f), .35f)); p.lineTo (P (X (.43f), .35f));
                p.startNewSubPath (P (X (.2f), .35f)); p.cubicTo (P (X (.45f), .05f), P (X (.95f), .15f), P (X (.9f), .6f)); p.cubicTo (P (X (.88f), .85f), P (X (.6f), .92f), P (X (.4f), .85f));
                g.strokePath (p, st); break; }
            case Play: p.addTriangle (P (.2f,.1f), P (.2f,.9f), P (.92f,.5f)); g.fillPath (p); break;
            case Stop: g.fillRoundedRectangle (b.reduced (s * .12f), 2.f); break;
            case Rec:  g.fillEllipse (b.reduced (s * .08f)); break;
            case Loop: p.addCentredArc (b.getCentreX(), b.getCentreY(), s * .4f, s * .4f, 0, 0.6f, 6.0f, true); g.strokePath (p, st);
                       { Path a; a.addTriangle (P (.8f,.02f), P (.8f,.4f), P (.5f,.2f)); g.fillPath (a); } break;
            case Select: p.addTriangle (P (.18f,.08f), P (.18f,.85f), P (.9f,.55f)); p.addTriangle (P (.45f,.55f), P (.6f,.55f), P (.5f,.95f)); g.fillPath (p); break;
            case Pencil: p.startNewSubPath (P (.12f,.88f)); p.lineTo (P (.18f,.62f)); p.lineTo (P (.72f,.08f)); p.lineTo (P (.92f,.28f)); p.lineTo (P (.38f,.82f)); p.closeSubPath(); g.strokePath (p, st); break;
            case Scissors: g.drawEllipse (P (.1f,.62f).x, P (.1f,.62f).y, s * .26f, s * .26f, lw); g.drawEllipse (P (.1f,.1f).x, P (.1f,.1f).y, s * .26f, s * .26f, lw);
                           g.drawLine ({ P (.3f,.3f), P (.92f,.8f) }, lw); g.drawLine ({ P (.3f,.7f), P (.92f,.2f) }, lw); break;
            case Stretch: g.drawLine ({ P (.1f,.5f), P (.9f,.5f) }, lw); { Path a; a.addTriangle (P (.02f,.5f), P (.3f,.28f), P (.3f,.72f)); a.addTriangle (P (.98f,.5f), P (.7f,.28f), P (.7f,.72f)); g.fillPath (a); } break;
            case Metro: p.startNewSubPath (P (.3f,.92f)); p.lineTo (P (.7f,.92f)); p.lineTo (P (.6f,.12f)); p.lineTo (P (.4f,.12f)); p.closeSubPath(); g.strokePath (p, st);
                        g.drawLine ({ P (.5f,.72f), P (.68f,.3f) }, lw); g.fillEllipse (P (.62f,.28f).x - s * .05f, P (.62f,.28f).y - s * .05f, s * .12f, s * .12f); break;
            case Speaker: p.startNewSubPath (P (.05f,.38f)); p.lineTo (P (.3f,.38f)); p.lineTo (P (.58f,.12f)); p.lineTo (P (.58f,.88f)); p.lineTo (P (.3f,.62f)); p.lineTo (P (.05f,.62f)); p.closeSubPath(); g.fillPath (p);
                          { Path a; a.addCentredArc (P (.58f,.5f).x, P (.58f,.5f).y, s * .3f, s * .3f, 0, 0.6f, 2.5f, true); a.addCentredArc (P (.58f,.5f).x, P (.58f,.5f).y, s * .5f, s * .5f, 0, 0.6f, 2.5f, true); g.strokePath (a, st); } break;
            case Magnet: p.startNewSubPath (P (.2f,.9f)); p.lineTo (P (.2f,.45f)); p.addCentredArc (P (.5f,.45f).x, P (.5f,.45f).y, s * .3f, s * .35f, 0, -1.5708f, 1.5708f, false);
                         p = Path(); p.startNewSubPath (P (.18f,.92f)); p.lineTo (P (.18f,.45f)); p.cubicTo (P (.18f,-.05f), P (.82f,-.05f), P (.82f,.45f)); p.lineTo (P (.82f,.92f)); g.strokePath (p, PathStrokeType (lw * 1.6f)); break;
            case Chevron: p.startNewSubPath (P (.15f,.35f)); p.lineTo (P (.5f,.7f)); p.lineTo (P (.85f,.35f)); g.strokePath (p, st); break;
            case Playlist: for (int i = 0; i < 3; ++i) g.drawRoundedRectangle (P (.08f, .1f + i * .3f).x, P (.08f, .1f + i * .3f).y, s * .84f, s * .2f, 2.f, lw * .8f); break;
            case Mixer: for (int i = 0; i < 3; ++i) { g.drawLine ({ P (.2f + i * .3f,.08f), P (.2f + i * .3f,.92f) }, lw * .9f); g.fillRoundedRectangle (P (.08f + i * .3f, .2f + (i == 1 ? .4f : i * .15f)).x, P (0, .2f + (i == 1 ? .4f : i * .15f)).y, s * .24f, s * .16f, 2.f); } break;
            case PianoRoll: for (int i = 0; i < 3; ++i) g.fillRoundedRectangle (P (.08f + i * .22f, .1f + i * .3f).x, P (0, .1f + i * .3f).y, s * (.5f - i * .08f), s * .2f, 2.f); break;
            case Audio: g.drawEllipse (b.reduced (s * .12f), lw); g.drawEllipse (b.reduced (s * .36f), lw); g.drawLine ({ P (.5f,.5f), P (.78f,.2f) }, lw); break;
            case Plugins: for (int i = 0; i < 9; ++i) g.fillEllipse (P (.12f + (i % 3) * .3f, .12f + (i / 3) * .3f).x, P (0, .12f + (i / 3) * .3f).y, s * .15f, s * .15f); break;
            case Export: p.startNewSubPath (P (.5f,.65f)); p.lineTo (P (.5f,.08f)); p.startNewSubPath (P (.25f,.32f)); p.lineTo (P (.5f,.08f)); p.lineTo (P (.75f,.32f));
                         p.startNewSubPath (P (.1f,.6f)); p.lineTo (P (.1f,.92f)); p.lineTo (P (.9f,.92f)); p.lineTo (P (.9f,.6f)); g.strokePath (p, st); break;
            case Eraser: p.startNewSubPath (P (.1f,.62f)); p.lineTo (P (.55f,.12f)); p.lineTo (P (.92f,.42f)); p.lineTo (P (.5f,.9f)); p.lineTo (P (.3f,.9f)); p.closeSubPath(); g.strokePath (p, st); g.drawLine ({ P (.32f,.38f), P (.72f,.72f) }, lw); g.drawLine ({ P (.5f,.92f), P (.92f,.92f) }, lw); break;
            case Steps: for (int r = 0; r < 2; ++r) for (int c = 0; c < 3; ++c) g.fillRoundedRectangle (P (.04f + c * .34f, .2f + r * .38f).x, P (0, .2f + r * .38f).y, s * .26f, s * .26f, 2.f); break;
            case Power: { Path a; a.addCentredArc (b.getCentreX(), b.getCentreY() + s * .04f, s * .36f, s * .36f, 0, 0.62f, 5.66f, true); g.strokePath (a, st); g.drawLine ({ P (.5f,.04f), P (.5f,.46f) }, lw); } break;
            case Sliders: for (int k = 0; k < 3; ++k) { const float y = .2f + k * .3f; g.drawLine ({ P (.05f, y), P (.95f, y) }, lw * .8f); g.fillEllipse (P (k == 1 ? .62f : .28f, y).x - s * .09f, P (0, y).y - s * .09f, s * .18f, s * .18f); } break;
            case Plus: g.drawLine ({ P (.1f,.5f), P (.9f,.5f) }, lw * 1.15f); g.drawLine ({ P (.5f,.1f), P (.5f,.9f) }, lw * 1.15f); break;
            case Dots: for (int k = 0; k < 3; ++k) g.fillEllipse (P (.5f, .15f + k * .35f).x - s * .07f, P (0, .15f + k * .35f).y - s * .07f, s * .14f, s * .14f); break;
            case Save: p.startNewSubPath (P (.1f,.1f)); p.lineTo (P (.78f,.1f)); p.lineTo (P (.92f,.24f)); p.lineTo (P (.92f,.92f)); p.lineTo (P (.1f,.92f)); p.closeSubPath(); g.strokePath (p, st); g.drawRect (P (.28f,.1f).x, P (.28f,.1f).y, s * .36f, s * .26f, lw * .8f); g.drawRect (P (.25f,.58f).x, P (.25f,.58f).y, s * .5f, s * .34f, lw * .8f); break;
            case Trash: g.drawLine ({ P (.1f,.25f), P (.9f,.25f) }, lw); p.startNewSubPath (P (.22f,.25f)); p.lineTo (P (.28f,.92f)); p.lineTo (P (.72f,.92f)); p.lineTo (P (.78f,.25f)); g.strokePath (p, st); g.drawLine ({ P (.38f,.25f), P (.38f,.1f) }, lw); g.drawLine ({ P (.62f,.25f), P (.62f,.1f) }, lw); g.drawLine ({ P (.38f,.1f), P (.62f,.1f) }, lw); break;
            case Plug: p.startNewSubPath (P (.3f,.05f)); p.lineTo (P (.3f,.3f)); p.startNewSubPath (P (.7f,.05f)); p.lineTo (P (.7f,.3f)); g.strokePath (p, st); { Path q; q.addRoundedRectangle (P (.15f,.3f).x, P (.15f,.3f).y, s * .7f, s * .35f, 3.f); g.strokePath (q, st); } g.drawLine ({ P (.5f,.65f), P (.5f,.95f) }, lw); break;
            case Brush: g.drawLine ({ P (.86f,.1f), P (.44f,.56f) }, lw * 1.7f);
                        p.startNewSubPath (P (.4f,.5f)); p.cubicTo (P (.18f,.46f), P (.2f,.74f), P (.08f,.92f)); p.cubicTo (P (.34f,.94f), P (.58f,.88f), P (.6f,.66f)); p.closeSubPath(); g.fillPath (p); break;
            case Help: g.drawEllipse (b.reduced (s * .06f), lw); g.setFont (juce::FontOptions (s * .7f, juce::Font::bold)); g.drawText ("?", b.toNearestInt(), juce::Justification::centred); break;
        }
    }
}
