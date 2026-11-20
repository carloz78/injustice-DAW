#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Tokens de diseño (ronda 3: paleta pastel, muestreada de los diseños nuevos).
namespace theme
{
    using juce::Colour;
    inline const Colour bgBar      { 0xff1b1b25 };   // barra superior
    inline const Colour bgBar2     { 0xff20202d };   // barra de vistas / paneles
    inline const Colour bgGrid     { 0xff25252f };
    inline const Colour bgGridAlt  { 0xff22222b };
    inline const Colour gridLine   { 0xff2d2d39 };
    inline const Colour barLine    { 0xff3a3a48 };
    inline const Colour rowLine    { 0xff1a1a22 };
    inline const Colour panel      { 0xff292935 };   // paneles y cajas
    inline const Colour panelHi    { 0xff343541 };
    inline const Colour boxFill    { 0xff181821 };   // cajas de BPM / compás / posición
    inline const Colour boxLine    { 0xff2e2e3b };
    inline const Colour accent     { 0xffa96ee0 };   // lila pastel principal
    inline const Colour accentHi   { 0xffd1a2f5 };   // lila rosado (texto resaltado)
    inline const Colour pink       { 0xfff08ac8 };   // rosa pastel
    inline const Colour record     { 0xffe8563a };
    inline const Colour text       { 0xfff0eff5 };
    inline const Colour textDim    { 0xff9c9bad };
    inline const Colour iconIdle   { 0xffd9d8e3 };

    // Colores de pista: pasteles suaves (rosas y lilas primero)
    // Colores medidos de la referencia de Ableton (morado y rosa primero)
    inline const Colour trackColours[] { Colour (0xffd985ff), Colour (0xffff9db8), Colour (0xff9fb6ff), Colour (0xffffb27d),
                                         Colour (0xfffff08c), Colour (0xff8cffe0), Colour (0xffb9ff8c), Colour (0xffff9fe6) };
    // Tinta casi negra para ondas y notas sobre un clip pastel (como Ableton)
    inline Colour inkOn (Colour) { return Colour (0xff15151d); }

    // Texto sobre el color de pista: oscuro si el blanco no llega a contraste 4.5
    inline Colour textOn (Colour c)
    {
        auto lin = [] (float v) { return v <= 0.03928f ? v / 12.92f : std::pow ((v + 0.055f) / 1.055f, 2.4f); };
        float L = 0.2126f * lin (c.getFloatRed()) + 0.7152f * lin (c.getFloatGreen()) + 0.0722f * lin (c.getFloatBlue());
        float whiteContrast = 1.05f / (L + 0.05f);
        return whiteContrast >= 4.5f ? juce::Colours::white : Colour (0xff1a1522);
    }

    // Aspecto oscuro/pastel para menús, diálogos y campos de texto
    struct Look : public juce::LookAndFeel_V4
    {
        Look()
        {
            setColour (juce::PopupMenu::backgroundColourId, panel); setColour (juce::PopupMenu::textColourId, text);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, accent); setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
            setColour (juce::AlertWindow::backgroundColourId, bgBar2); setColour (juce::AlertWindow::textColourId, text); setColour (juce::AlertWindow::outlineColourId, barLine);
            setColour (juce::TextEditor::backgroundColourId, boxFill); setColour (juce::TextEditor::textColourId, text); setColour (juce::TextEditor::outlineColourId, boxLine);
            setColour (juce::TextEditor::focusedOutlineColourId, accent); setColour (juce::CaretComponent::caretColourId, accentHi);
            setColour (juce::TextButton::buttonColourId, panelHi); setColour (juce::TextButton::textColourOffId, text); setColour (juce::TextButton::buttonOnColourId, accent);
            setColour (juce::ComboBox::backgroundColourId, boxFill); setColour (juce::ComboBox::textColourId, text); setColour (juce::ComboBox::outlineColourId, boxLine);
            setColour (juce::ListBox::backgroundColourId, bgBar); setColour (juce::ListBox::textColourId, text);
            setColour (juce::Label::textColourId, text); setColour (juce::ScrollBar::thumbColourId, accent.withAlpha (.6f));
            setColour (juce::ResizableWindow::backgroundColourId, bgBar2); setColour (juce::DocumentWindow::textColourId, text);
            setColour (juce::TooltipWindow::backgroundColourId, panel); setColour (juce::TooltipWindow::textColourId, text); setColour (juce::TooltipWindow::outlineColourId, barLine);
            setColour (juce::TableHeaderComponent::backgroundColourId, boxFill); setColour (juce::TableHeaderComponent::textColourId, text);
        }
    };
}
