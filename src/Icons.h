#pragma once
#include "Theme.h"

// Iconos vectoriales propios (dibujados con trazos; sin recursos de terceros).
namespace icons
{
    enum Id { NewFile, OpenFile, Layout, Undo, Redo, Play, Stop, Rec, Loop, Select, Pencil, Scissors, Stretch,
              Speaker, Metro, Magnet, Chevron, Playlist, Mixer, PianoRoll, Audio, Plugins, Export, Help, Eraser, Steps, Power, Sliders, Plus, Dots, Save, Trash, Plug, Brush, Curve };

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
        const float lt = juce::jmax (1.25f, s * 0.075f); PathStrokeType st2 (lt, PathStrokeType::curved, PathStrokeType::rounded);
        auto R = [&] (float x, float y, float w, float h) { return juce::Rectangle<float> (P (x, y).x, P (x, y).y, w * s, h * s); };
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
            case Select: { Path d; d.addRoundedRectangle (R (.08f,.08f,.62f,.62f).getX(), R (.08f,.08f,.62f,.62f).getY(), .62f * s, .62f * s, 1.5f); Path dd; const float dl[] { 3.f, 3.f }; PathStrokeType (lt).createDashedStroke (dd, d, dl, 2); g.fillPath (dd);
                       p.startNewSubPath (P (.46f,.4f)); p.lineTo (P (.46f,.94f)); p.lineTo (P (.6f,.8f)); p.lineTo (P (.7f,.98f)); p.lineTo (P (.8f,.93f)); p.lineTo (P (.7f,.76f)); p.lineTo (P (.88f,.76f)); p.closeSubPath(); g.fillPath (p); } break;
            case Pencil: p.startNewSubPath (P (.14f,.88f)); p.lineTo (P (.2f,.64f)); p.lineTo (P (.66f,.18f)); p.lineTo (P (.84f,.36f)); p.lineTo (P (.38f,.82f)); p.closeSubPath(); g.strokePath (p, st2); g.drawLine ({ P (.56f,.28f), P (.74f,.46f) }, lt); g.drawLine ({ P (.2f,.64f), P (.38f,.82f) }, lt); break;
            case Scissors: g.drawEllipse (R (.06f,.58f,.3f,.3f), lt); g.drawEllipse (R (.06f,.12f,.3f,.3f), lt); g.drawLine ({ P (.3f,.38f), P (.94f,.78f) }, lt); g.drawLine ({ P (.3f,.62f), P (.94f,.22f) }, lt); break;
            case Stretch: g.drawLine ({ P (.14f,.5f), P (.86f,.5f) }, lt); p.startNewSubPath (P (.32f,.3f)); p.lineTo (P (.12f,.5f)); p.lineTo (P (.32f,.7f)); p.startNewSubPath (P (.68f,.3f)); p.lineTo (P (.88f,.5f)); p.lineTo (P (.68f,.7f)); g.strokePath (p, st2); break;
            case Metro: p.startNewSubPath (P (.3f,.92f)); p.lineTo (P (.7f,.92f)); p.lineTo (P (.6f,.12f)); p.lineTo (P (.4f,.12f)); p.closeSubPath(); g.strokePath (p, st);
                        g.drawLine ({ P (.5f,.72f), P (.68f,.3f) }, lw); g.fillEllipse (P (.62f,.28f).x - s * .05f, P (.62f,.28f).y - s * .05f, s * .12f, s * .12f); break;
            case Speaker: p.startNewSubPath (P (.05f,.38f)); p.lineTo (P (.3f,.38f)); p.lineTo (P (.58f,.12f)); p.lineTo (P (.58f,.88f)); p.lineTo (P (.3f,.62f)); p.lineTo (P (.05f,.62f)); p.closeSubPath(); g.fillPath (p);
                          { Path a; a.addCentredArc (P (.58f,.5f).x, P (.58f,.5f).y, s * .3f, s * .3f, 0, 0.6f, 2.5f, true); a.addCentredArc (P (.58f,.5f).x, P (.58f,.5f).y, s * .5f, s * .5f, 0, 0.6f, 2.5f, true); g.strokePath (a, st); } break;
            case Magnet: p.startNewSubPath (P (.2f,.92f)); p.lineTo (P (.2f,.46f)); p.cubicTo (P (.2f,-.04f), P (.8f,-.04f), P (.8f,.46f)); p.lineTo (P (.8f,.92f)); g.strokePath (p, juce::PathStrokeType (lt * 2.2f, PathStrokeType::mitered, PathStrokeType::butt)); g.drawLine ({ P (.1f,.74f), P (.3f,.74f) }, lt); g.drawLine ({ P (.7f,.74f), P (.9f,.74f) }, lt); break;
            case Chevron: p.startNewSubPath (P (.15f,.35f)); p.lineTo (P (.5f,.7f)); p.lineTo (P (.85f,.35f)); g.strokePath (p, st); break;
            case Playlist: g.drawLine ({ P (.06f,.14f), P (.94f,.14f) }, lt); for (float x : { .06f, .38f, .7f }) g.drawLine ({ P (x,.06f), P (x,.14f) }, lt);
                       g.drawRoundedRectangle (R (.06f,.3f,.46f,.24f).reduced (lt * .5f), 1.5f, lt); g.drawRoundedRectangle (R (.34f,.66f,.6f,.24f).reduced (lt * .5f), 1.5f, lt); g.fillRect (R (.7f,.14f,.07f,.78f)); break;
            case Mixer: for (int i = 0; i < 3; ++i) { const float x = .2f + i * .3f, y = i == 0 ? .55f : (i == 1 ? .25f : .45f); g.drawLine ({ P (x,.08f), P (x,.92f) }, lt); g.fillRoundedRectangle (R (x - .12f, y, .24f, .17f), 1.5f); } break;
            case PianoRoll: g.drawRect (R (.06f,.08f,.26f,.84f), lt); for (float y : { .25f, .46f, .67f }) g.fillRect (R (.06f, y, .16f, .1f)); g.fillRoundedRectangle (R (.42f,.18f,.3f,.15f), 1.5f); g.fillRoundedRectangle (R (.58f,.42f,.34f,.15f), 1.5f); g.fillRoundedRectangle (R (.44f,.66f,.3f,.15f), 1.5f); break;
            case Audio: g.drawEllipse (b.reduced (s * .12f), lw); g.drawEllipse (b.reduced (s * .36f), lw); g.drawLine ({ P (.5f,.5f), P (.78f,.2f) }, lw); break;
            case Plugins: for (int i = 0; i < 9; ++i) g.fillEllipse (P (.12f + (i % 3) * .3f, .12f + (i / 3) * .3f).x, P (0, .12f + (i / 3) * .3f).y, s * .15f, s * .15f); break;
            case Export: p.startNewSubPath (P (.5f,.65f)); p.lineTo (P (.5f,.08f)); p.startNewSubPath (P (.25f,.32f)); p.lineTo (P (.5f,.08f)); p.lineTo (P (.75f,.32f));
                         p.startNewSubPath (P (.1f,.6f)); p.lineTo (P (.1f,.92f)); p.lineTo (P (.9f,.92f)); p.lineTo (P (.9f,.6f)); g.strokePath (p, st); break;
            case Eraser: p.startNewSubPath (P (.1f,.62f)); p.lineTo (P (.55f,.14f)); p.lineTo (P (.9f,.44f)); p.lineTo (P (.5f,.88f)); p.lineTo (P (.3f,.88f)); p.closeSubPath(); g.strokePath (p, st2); g.drawLine ({ P (.3f,.88f), P (.94f,.88f) }, lt); g.drawLine ({ P (.3f,.4f), P (.74f,.78f) }, lt); break;
            case Steps: for (int r = 0; r < 2; ++r) for (int k = 0; k < 4; ++k) { auto q = R (.04f + k * .24f, .26f + r * .3f, .19f, .2f); if (((k + r) & 1) == 0) g.fillRoundedRectangle (q, 1.2f); else g.drawRoundedRectangle (q.reduced (lt * .5f), 1.2f, lt); } break;
            case Power: { Path a; a.addCentredArc (b.getCentreX(), b.getCentreY() + s * .04f, s * .36f, s * .36f, 0, 0.62f, 5.66f, true); g.strokePath (a, st); g.drawLine ({ P (.5f,.04f), P (.5f,.46f) }, lw); } break;
            case Sliders: for (int k = 0; k < 3; ++k) { const float y = .2f + k * .3f; g.drawLine ({ P (.05f, y), P (.95f, y) }, lw * .8f); g.fillEllipse (P (k == 1 ? .62f : .28f, y).x - s * .09f, P (0, y).y - s * .09f, s * .18f, s * .18f); } break;
            case Plus: g.drawLine ({ P (.1f,.5f), P (.9f,.5f) }, lw * 1.15f); g.drawLine ({ P (.5f,.1f), P (.5f,.9f) }, lw * 1.15f); break;
            case Dots: for (int k = 0; k < 3; ++k) g.fillEllipse (P (.5f, .15f + k * .35f).x - s * .07f, P (0, .15f + k * .35f).y - s * .07f, s * .14f, s * .14f); break;
            case Save: p.startNewSubPath (P (.1f,.1f)); p.lineTo (P (.78f,.1f)); p.lineTo (P (.92f,.24f)); p.lineTo (P (.92f,.92f)); p.lineTo (P (.1f,.92f)); p.closeSubPath(); g.strokePath (p, st); g.drawRect (P (.28f,.1f).x, P (.28f,.1f).y, s * .36f, s * .26f, lw * .8f); g.drawRect (P (.25f,.58f).x, P (.25f,.58f).y, s * .5f, s * .34f, lw * .8f); break;
            case Trash: g.drawLine ({ P (.1f,.25f), P (.9f,.25f) }, lw); p.startNewSubPath (P (.22f,.25f)); p.lineTo (P (.28f,.92f)); p.lineTo (P (.72f,.92f)); p.lineTo (P (.78f,.25f)); g.strokePath (p, st); g.drawLine ({ P (.38f,.25f), P (.38f,.1f) }, lw); g.drawLine ({ P (.62f,.25f), P (.62f,.1f) }, lw); g.drawLine ({ P (.38f,.1f), P (.62f,.1f) }, lw); break;
            case Plug: p.startNewSubPath (P (.3f,.05f)); p.lineTo (P (.3f,.3f)); p.startNewSubPath (P (.7f,.05f)); p.lineTo (P (.7f,.3f)); g.strokePath (p, st); { Path q; q.addRoundedRectangle (P (.15f,.3f).x, P (.15f,.3f).y, s * .7f, s * .35f, 3.f); g.strokePath (q, st); } g.drawLine ({ P (.5f,.65f), P (.5f,.95f) }, lw); break;
            case Brush: { Path h; h.addRoundedRectangle (R (.43f,.0f,.14f,.42f).getX(), R (.43f,.0f,.14f,.42f).getY(), .14f * s, .42f * s, 1.5f); Path f; f.addRectangle (R (.4f,.44f,.2f,.1f)); Path bs; bs.startNewSubPath (P (.4f,.58f)); bs.lineTo (P (.6f,.58f)); bs.cubicTo (P (.62f,.78f), P (.56f,.92f), P (.5f,1.0f)); bs.cubicTo (P (.44f,.92f), P (.38f,.78f), P (.4f,.58f)); bs.closeSubPath();
                         const auto rot = juce::AffineTransform::rotation (.8f, P (.5f,.5f).x, P (.5f,.5f).y).translated (s * .06f, -s * .06f); h.applyTransform (rot); f.applyTransform (rot); bs.applyTransform (rot);
                         g.strokePath (h, st2); g.fillPath (f); g.fillPath (bs); } break;
            case Curve: p.startNewSubPath (P (.1f,.76f)); p.cubicTo (P (.34f,.76f), P (.3f,.26f), P (.52f,.3f)); p.cubicTo (P (.74f,.34f), P (.7f,.76f), P (.9f,.2f)); g.strokePath (p, st2); for (auto q : { P (.1f,.76f), P (.52f,.3f), P (.9f,.2f) }) { g.setColour (c); g.fillEllipse (q.x - s * .09f, q.y - s * .09f, s * .18f, s * .18f); } break;
            case Help: g.drawEllipse (b.reduced (s * .06f), lw); g.setFont (juce::FontOptions (s * .7f, juce::Font::bold)); g.drawText ("?", b.toNearestInt(), juce::Justification::centred); break;
        }
    }
}
