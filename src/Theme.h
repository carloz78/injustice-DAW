#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Tokens de diseño (ronda 3: paleta pastel, muestreada de los diseños nuevos).
namespace theme
{
    using juce::Colour;
    inline const Colour bgBar      { 0xff1f2024 };   // barra superior
    inline const Colour bgBar2     { 0xff26272b };   // barra de vistas / paneles
    inline const Colour bgGrid     { 0xff2a2b30 };
    inline const Colour bgGridAlt  { 0xff27282d };
    inline const Colour gridLine   { 0xff35363c };
    inline const Colour barLine    { 0xff4a4b52 };
    inline const Colour rowLine    { 0xff1d1e22 };
    inline const Colour panel      { 0xff2f3035 };   // paneles y cajas
    inline const Colour panelHi    { 0xff3b3c42 };
    inline const Colour boxFill    { 0xff1c1c1c };   // cajas de BPM / compás / posición
    inline const Colour boxLine    { 0xff323232 };
    inline const Colour accent     { 0xffe0974c };   // lila pastel principal
    inline const Colour accentHi   { 0xfff5ba7d };   // lila rosado (texto resaltado)
    inline const Colour pink       { 0xfff0ac65 };   // rosa pastel
    inline const Colour record     { 0xffe8563a };
    inline const Colour text       { 0xfff0f0f0 };
    inline const Colour textDim    { 0xff9d9d9d };
    inline const Colour iconIdle   { 0xffdadada };

    // Colores de pista: pasteles suaves (rosas y lilas primero)
    // Colores medidos de la referencia de Ableton (morado y rosa primero)
    inline const Colour trackColours[] { Colour (0xff90a8ff), Colour (0xff2fe3cf), Colour (0xfffaee6a), Colour (0xffffa070),
                                         Colour (0xffff8fc8), Colour (0xff8ff07a), Colour (0xffc89cff), Colour (0xffff7a70) };
    // Tinta casi negra para ondas y notas sobre un clip pastel (como Ableton)
    inline Colour inkOn (Colour) { return Colour (0xff191919); }

    // Texto sobre el color de pista: oscuro si el blanco no llega a contraste 4.5
    inline Colour textOn (Colour c)
    {
        auto lin = [] (float v) { return v <= 0.03928f ? v / 12.92f : std::pow ((v + 0.055f) / 1.055f, 2.4f); };
        float L = 0.2126f * lin (c.getFloatRed()) + 0.7152f * lin (c.getFloatGreen()) + 0.0722f * lin (c.getFloatBlue());
        float whiteContrast = 1.05f / (L + 0.05f);
        return whiteContrast >= 4.5f ? juce::Colours::white : Colour (0xff1b1b1b);
    }

    // Aspecto oscuro/pastel para menús, diálogos y campos de texto
    struct Look : public juce::LookAndFeel_V4
    {
        Look()
        {
            setColour (juce::PopupMenu::backgroundColourId, panel); setColour (juce::PopupMenu::textColourId, text);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, accent); setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff1a1a1a));
            setColour (juce::AlertWindow::backgroundColourId, bgBar2); setColour (juce::AlertWindow::textColourId, text); setColour (juce::AlertWindow::outlineColourId, barLine);
            setColour (juce::TextEditor::backgroundColourId, boxFill); setColour (juce::TextEditor::textColourId, text); setColour (juce::TextEditor::outlineColourId, boxLine);
            setColour (juce::TextEditor::focusedOutlineColourId, accent); setColour (juce::CaretComponent::caretColourId, accentHi);
            setColour (juce::TextButton::buttonColourId, panelHi); setColour (juce::TextButton::textColourOffId, text); setColour (juce::TextButton::buttonOnColourId, accent);
            setColour (juce::ComboBox::backgroundColourId, boxFill); setColour (juce::ComboBox::textColourId, text); setColour (juce::ComboBox::outlineColourId, boxLine);
            setColour (juce::ListBox::backgroundColourId, bgBar); setColour (juce::ListBox::textColourId, text);
            setColour (juce::Label::textColourId, text); setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xff74757d).withAlpha (.75f));
            setColour (juce::ResizableWindow::backgroundColourId, bgBar2); setColour (juce::DocumentWindow::textColourId, text);
            setColour (juce::TooltipWindow::backgroundColourId, panel); setColour (juce::TooltipWindow::textColourId, text); setColour (juce::TooltipWindow::outlineColourId, barLine);
            setColour (juce::TableHeaderComponent::backgroundColourId, boxFill); setColour (juce::TableHeaderComponent::textColourId, text);
        }
    };
}
