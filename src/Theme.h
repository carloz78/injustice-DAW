#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Tokens de diseño medidos de los mockups (docs/DISENO_TOKENS.md)
namespace theme
{
    using juce::Colour;
    inline const Colour bgBar      { 0xff0c0b0d };   // barras superiores
    inline const Colour bgBar2     { 0xff100f12 };
    inline const Colour bgGrid     { 0xff212024 };
    inline const Colour bgGridAlt  { 0xff1f1f23 };
    inline const Colour gridLine   { 0xff2a292e };
    inline const Colour barLine    { 0xff333238 };
    inline const Colour rowLine    { 0xff0b0b0d };
    inline const Colour accent     { 0xffb404f3 };   // violeta/magenta principal
    inline const Colour accentHi   { 0xffdc02f3 };
    inline const Colour text       { 0xfff2f2f4 };
    inline const Colour textDim    { 0xff9a99a6 };
    inline const Colour iconIdle   { 0xffe4e3ea };

    // Colores de canal (muestreados del mockup 01)
    inline const Colour trackColours[] { Colour (0xffb232f7), Colour (0xffef06f3), Colour (0xfff955e4), Colour (0xff089cf4),
                                         Colour (0xff7a4cf0), Colour (0xffe03a8e), Colour (0xff2fd0c4), Colour (0xff8a5cf6) };
    // Texto sobre el color de pista: oscuro si el blanco no llega a contraste 4.5
    inline Colour textOn (Colour c)
    {
        auto lin = [] (float v) { return v <= 0.03928f ? v / 12.92f : std::pow ((v + 0.055f) / 1.055f, 2.4f); };
        float L = 0.2126f * lin (c.getFloatRed()) + 0.7152f * lin (c.getFloatGreen()) + 0.0722f * lin (c.getFloatBlue());
        float whiteContrast = 1.05f / (L + 0.05f);
        return whiteContrast >= 4.5f ? juce::Colours::white : Colour (0xff16101c);
    }
}
