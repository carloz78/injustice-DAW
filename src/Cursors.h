#pragma once
#include "Icons.h"

// Cursores del ratón por herramienta (lápiz, pincel, tijeras, borrador, seleccionar), dibujados con los mismos iconos de la interfaz.
namespace cursors
{
    inline juce::MouseCursor make (icons::Id id, float hx, float hy)
    {
        constexpr int N = 40; juce::Image img (juce::Image::ARGB, N, N, true); juce::Graphics g (img);
        const float s = N * 0.62f, b0 = (N - s) * 0.5f; const juce::Rectangle<float> r (0.f, 0.f, (float) N, (float) N);
        for (int dx = -1; dx <= 1; ++dx) for (int dy = -1; dy <= 1; ++dy) if (dx != 0 || dy != 0) icons::draw (g, id, r.translated ((float) dx * 1.2f, (float) dy * 1.2f), juce::Colour (0xff141414));
        icons::draw (g, id, r, juce::Colours::white);
        return juce::MouseCursor (img, juce::roundToInt (b0 + hx * s), juce::roundToInt (b0 + hy * s));
    }
    // El nombre de herramienta es el mismo que usan la Playlist y el Piano Roll.
    inline juce::MouseCursor forTool (const juce::String& tool)
    {
        static const juce::MouseCursor pencil = make (icons::Pencil, .12f, .88f), brush = make (icons::Brush, .08f, .92f), cut = make (icons::Scissors, .5f, .5f),
                                       erase = make (icons::Eraser, .3f, .9f), select = make (icons::Select, .18f, .08f);
        if (tool == "pencil") return pencil; if (tool == "brush") return brush; if (tool == "cut") return cut; if (tool == "erase") return erase; if (tool == "select") return select;
        return juce::MouseCursor::NormalCursor;
    }
}
