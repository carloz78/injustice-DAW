#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "InjFontData.h"

// Sistema de diseño de INJUSTICE DAW (v0.9.43).
// Grafito con un solo acento (naranja), líneas finas, esquinas casi rectas, tipografía IBM Plex y números tabulares en Plex Mono.
namespace theme
{
    using juce::Colour;
    // ---- superficies, de más oscura a más clara ----
    inline const Colour bgBar      { 0xff141518 };   // barra superior (la más oscura)
    inline const Colour bgBar2     { 0xff1b1c20 };   // barra de vistas / paneles
    inline const Colour bgGrid     { 0xff222329 };   // carriles de la playlist
    inline const Colour bgGridAlt  { 0xff1f2025 };
    inline const Colour gridLine   { 0xff2c2e35 };
    inline const Colour barLine    { 0xff3c3f47 };
    inline const Colour rowLine    { 0xff121316 };
    inline const Colour panel      { 0xff25272d };   // paneles y cajas elevadas
    inline const Colour panelHi    { 0xff30333a };
    inline const Colour boxFill    { 0xff101114 };   // campos hundidos (BPM, compás, posición, búsqueda)
    inline const Colour boxLine    { 0xff2b2d34 };
    inline const Colour seam       { 0xff0b0b0d };   // junta oscura entre paneles
    inline const Colour edgeLight  { 0x0fffffff };   // filo claro en el borde superior de lo elevado
    // ---- color ----
    inline const Colour accent     { 0xfff08c22 };   // naranja de la marca: lo activo y lo seleccionado
    inline const Colour accentHi   { 0xffffa54a };   // texto e indicadores sobre fondo oscuro
    inline const Colour accentDim  { 0xff8a4f14 };
    inline const Colour pink       { 0xfff2a65a };   // marcadores
    inline const Colour record     { 0xffe5484d };
    inline const Colour ok         { 0xff4cc38a };
    inline const Colour text       { 0xffe6e8ec };
    inline const Colour textDim    { 0xff8d909a };
    inline const Colour textFaint  { 0xff5b5e68 };
    inline const Colour iconIdle   { 0xffc4c7ce };
    inline constexpr float rad = 2.f;                // radio estándar de esquinas

    // Colores de pista: apagados y con cuerpo (se leen bien con texto blanco y no cansan la vista)
    inline const Colour trackColours[] { Colour (0xff5b88ea), Colour (0xff31b5a2), Colour (0xffd5b144), Colour (0xffe27d46),
                                         Colour (0xffd2629c), Colour (0xff60b762), Colour (0xff9070d9), Colour (0xffd95b5b) };
    // Tinta clara para ondas, notas y rótulos sobre el cuerpo oscuro de un clip
    inline Colour inkOn (Colour) { return Colour (0xfff1f3f7); }

    // Texto sobre el color de pista: oscuro si el blanco no llega a contraste 4.5
    inline Colour textOn (Colour c)
    {
        auto lin = [] (float v) { return v <= 0.03928f ? v / 12.92f : std::pow ((v + 0.055f) / 1.055f, 2.4f); };
        float L = 0.2126f * lin (c.getFloatRed()) + 0.7152f * lin (c.getFloatGreen()) + 0.0722f * lin (c.getFloatBlue());
        float whiteContrast = 1.05f / (L + 0.05f);
        return whiteContrast >= 4.5f ? juce::Colours::white : Colour (0xff17181b);
    }

    // ---- tipografía ----
    // Barra de título nativa oscura (Windows 10/11); en otros sistemas no hace nada. Sin cabeceras de Windows: se carga dwmapi al vuelo.
    inline void darkTitleBar (juce::Component* c)
    {
       #if JUCE_WINDOWS
        if (c == nullptr) return; auto* p = c->getPeer(); if (p == nullptr) return;
        static juce::DynamicLibrary dwm ("dwmapi.dll");
        using Fn = long (__stdcall*) (void*, unsigned long, const void*, unsigned long);
        if (auto fn = (Fn) dwm.getFunction ("DwmSetWindowAttribute"))
        {
            void* h = p->getNativeHandle(); const unsigned long on = 1, cap = 0x00181514 /* BBGGRR de bgBar */, txt = 0x00f2e8e6;
            fn (h, 20, &on, sizeof on); fn (h, 19, &on, sizeof on); fn (h, 35, &cap, sizeof cap); fn (h, 36, &txt, sizeof txt);
        }
       #else
        (void) c;
       #endif
    }

    namespace fonts
    {
        inline juce::Typeface::Ptr load (const char* d, int n) { return juce::Typeface::createSystemTypefaceFor (d, (size_t) n); }
        inline juce::Typeface::Ptr regular()  { static auto t = load (InjFontData::IBMPlexSansRegular_ttf,  InjFontData::IBMPlexSansRegular_ttfSize);  return t; }
        inline juce::Typeface::Ptr medium()   { static auto t = load (InjFontData::IBMPlexSansMedium_ttf,   InjFontData::IBMPlexSansMedium_ttfSize);   return t; }
        inline juce::Typeface::Ptr semibold() { static auto t = load (InjFontData::IBMPlexSansSemiBold_ttf, InjFontData::IBMPlexSansSemiBold_ttfSize); return t; }
        inline juce::Typeface::Ptr mono()     { static auto t = load (InjFontData::IBMPlexMonoMedium_ttf,   InjFontData::IBMPlexMonoMedium_ttfSize);   return t; }
        // Texto de interfaz y cifras (tabulares, no bailan al cambiar)
        inline juce::Font ui (float h, bool strong = false) { return juce::Font (juce::FontOptions (strong ? semibold() : regular()).withHeight (h)); }
        inline juce::Font label (float h) { return juce::Font (juce::FontOptions (medium()).withHeight (h)); }
        inline juce::Font digits (float h) { return juce::Font (juce::FontOptions (mono()).withHeight (h)); }
    }

    // ---- piezas de dibujo comunes ----
    // Panel elevado: relleno, filo claro arriba y junta oscura abajo
    inline void raised (juce::Graphics& g, juce::Rectangle<float> r, Colour fill = panel, float rr = rad)
    {
        g.setColour (fill); g.fillRoundedRectangle (r, rr);
        g.setColour (edgeLight); g.fillRect (r.getX() + rr, r.getY(), r.getWidth() - 2 * rr, 1.f);
        g.setColour (seam.withAlpha (.7f)); g.drawRoundedRectangle (r.reduced (.5f), rr, 1.f);
    }
    // Campo hundido: más oscuro que lo que lo rodea, con sombra interior arriba
    inline void well (juce::Graphics& g, juce::Rectangle<float> r, bool focus = false, float rr = rad)
    {
        g.setColour (boxFill); g.fillRoundedRectangle (r, rr);
        g.setColour (juce::Colours::black.withAlpha (.18f)); g.fillRect (r.getX() + rr, r.getY(), r.getWidth() - 2 * rr, 1.f);
        g.setColour (focus ? accent : boxLine); g.drawRoundedRectangle (r.reduced (.5f), rr, 1.f);
    }
    // Botón plano: reposo, encima, activo (naranja)
    inline void button (juce::Graphics& g, juce::Rectangle<float> r, bool on, bool hover, bool down = false, float rr = rad)
    {
        if (on) { g.setColour (accent); g.fillRoundedRectangle (r, rr); g.setColour (juce::Colours::white.withAlpha (.08f)); g.fillRect (r.getX() + rr, r.getY(), r.getWidth() - 2 * rr, 1.f); g.setColour (accentDim.darker (.4f)); g.drawRoundedRectangle (r.reduced (.5f), rr, 1.f); return; }
        raised (g, r, down ? bgBar2 : (hover ? panelHi : panel), rr);
    }

    // Aspecto oscuro para menús, diálogos y campos de texto
    struct Look : public juce::LookAndFeel_V4
    {
        Look()
        {
            setColour (juce::PopupMenu::backgroundColourId, panel); setColour (juce::PopupMenu::textColourId, text);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, accent); setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff17181b));
            setColour (juce::AlertWindow::backgroundColourId, bgBar2); setColour (juce::AlertWindow::textColourId, text); setColour (juce::AlertWindow::outlineColourId, barLine);
            setColour (juce::TextEditor::backgroundColourId, boxFill); setColour (juce::TextEditor::textColourId, text); setColour (juce::TextEditor::outlineColourId, boxLine);
            setColour (juce::TextEditor::focusedOutlineColourId, accent); setColour (juce::CaretComponent::caretColourId, accentHi);
            setColour (juce::TextEditor::highlightColourId, accent.withAlpha (.35f)); setColour (juce::TextEditor::highlightedTextColourId, text);
            setColour (juce::TextButton::buttonColourId, panelHi); setColour (juce::TextButton::textColourOffId, text); setColour (juce::TextButton::buttonOnColourId, accent);
            setColour (juce::ComboBox::backgroundColourId, boxFill); setColour (juce::ComboBox::textColourId, text); setColour (juce::ComboBox::outlineColourId, boxLine);
            setColour (juce::ListBox::backgroundColourId, bgBar); setColour (juce::ListBox::textColourId, text);
            setColour (juce::Label::textColourId, text); setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xff6a6d77).withAlpha (.7f));
            setColour (juce::ResizableWindow::backgroundColourId, bgBar2); setColour (juce::DocumentWindow::textColourId, text);
            setColour (juce::TooltipWindow::backgroundColourId, panelHi); setColour (juce::TooltipWindow::textColourId, text); setColour (juce::TooltipWindow::outlineColourId, barLine);
            setColour (juce::TableHeaderComponent::backgroundColourId, boxFill); setColour (juce::TableHeaderComponent::textColourId, text);
            setColour (juce::Slider::thumbColourId, accent); setColour (juce::Slider::trackColourId, accent); setColour (juce::Slider::backgroundColourId, boxFill);
        }
        // Todo el texto usa IBM Plex: normal, y semi-negrita donde el código pide negrita
        juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override
        {
            const auto n = f.getTypefaceName();
            if (n == juce::Font::getDefaultMonospacedFontName()) return fonts::mono();
            if (n == juce::Font::getDefaultSansSerifFontName() || n == juce::Font::getDefaultSerifFontName() || n.isEmpty()) return f.isBold() ? fonts::semibold() : fonts::regular();
            return juce::LookAndFeel_V4::getTypefaceForFont (f);
        }
        juce::Font getPopupMenuFont() override { return fonts::ui (13.5f); }
        juce::Font getTextButtonFont (juce::TextButton&, int) override { return fonts::ui (13.f, true); }
        juce::Font getLabelFont (juce::Label&) override { return fonts::ui (13.f); }
        juce::Font getComboBoxFont (juce::ComboBox&) override { return fonts::ui (13.f); }
        juce::Font getAlertWindowMessageFont() override { return fonts::ui (14.f); }
        juce::Font getAlertWindowTitleFont() override { return fonts::ui (17.f, true); }

        void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
        {
            g.fillAll (panel); g.setColour (barLine); g.drawRect (0, 0, w, h, 1);
        }
        void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                                const juce::String& txt, const juce::String& shortcut, const juce::Drawable* icon, const juce::Colour* textCol) override
        {
            if (isSeparator) { auto r = area.reduced (10, 0); g.setColour (barLine.withAlpha (.8f)); g.fillRect (r.getX(), r.getCentreY(), r.getWidth(), 1); return; }
            auto r = area.reduced (4, 1); const bool hot = isHighlighted && isActive;
            if (hot) { g.setColour (accent); g.fillRoundedRectangle (r.toFloat(), rad); }
            auto col = hot ? Colour (0xff17181b) : (textCol != nullptr ? *textCol : text); if (! isActive) col = textFaint;
            g.setColour (col); g.setFont (getPopupMenuFont()); auto tr = r.reduced (10, 0);
            if (isTicked) { juce::Path p; p.startNewSubPath (0.f, .5f); p.lineTo (.35f, 1.f); p.lineTo (1.f, 0.f); g.strokePath (p, juce::PathStrokeType (1.8f), juce::AffineTransform::scale (9.f, 8.f).translated ((float) r.getX() + 8.f, (float) r.getCentreY() - 4.f)); tr.removeFromLeft (14); }
            if (icon != nullptr) { icon->drawWithin (g, tr.removeFromLeft (18).toFloat(), juce::RectanglePlacement::centred, 1.f); tr.removeFromLeft (6); }
            if (hasSubMenu) { juce::Path p; p.startNewSubPath (0.f, 0.f); p.lineTo (.6f, .5f); p.lineTo (0.f, 1.f); g.strokePath (p, juce::PathStrokeType (1.5f), juce::AffineTransform::scale (5.f, 9.f).translated ((float) r.getRight() - 14.f, (float) r.getCentreY() - 4.5f)); tr.removeFromRight (12); }
            if (shortcut.isNotEmpty()) { g.setColour (hot ? Colour (0xff17181b).withAlpha (.7f) : textDim); g.setFont (fonts::ui (12.f)); g.drawText (shortcut, tr, juce::Justification::centredRight); tr.removeFromRight (shortcut.length() * 7); g.setColour (col); g.setFont (getPopupMenuFont()); }
            g.drawFittedText (txt, tr, juce::Justification::centredLeft, 1);
        }
        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down) override
        {
            button (g, b.getLocalBounds().toFloat().reduced (.5f), b.getToggleState(), hover, down);
        }
        void drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize, bool over, bool down) override
        {
            g.setColour (juce::Colours::black.withAlpha (.18f)); g.fillRect (x, y, w, h);
            const auto thumb = vertical ? juce::Rectangle<float> ((float) x + 2.f, (float) (y + thumbStart), (float) w - 4.f, (float) thumbSize) : juce::Rectangle<float> ((float) (x + thumbStart), (float) y + 2.f, (float) thumbSize, (float) h - 4.f);
            g.setColour (juce::Colour (0xff6a6d77).withAlpha (down ? .95f : over ? .85f : .6f)); g.fillRoundedRectangle (thumb, 2.f);
        }
        int getScrollbarButtonSize (juce::ScrollBar&) override { return 0; }
        void drawTooltip (juce::Graphics& g, const juce::String& txt, int w, int h) override
        {
            g.fillAll (panelHi); g.setColour (barLine); g.drawRect (0, 0, w, h, 1);
            g.setColour (text); g.setFont (fonts::ui (12.5f)); g.drawFittedText (txt, 6, 3, w - 12, h - 6, juce::Justification::centredLeft, 3);
        }
    };
}
