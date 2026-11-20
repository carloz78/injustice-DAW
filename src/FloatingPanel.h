#pragma once
#include "Icons.h"
#include <functional>

// Ventana flotante propia: se arrastra por la barra, se redimensiona desde bordes y esquinas (el contenido
// se escala), se maximiza y se cierra. El contenido se dibuja en coordenadas de diseño (designW x designH).
class FloatingPanel : public juce::Component
{
public:
    static constexpr int kHead = 48;
    juce::String title; icons::Id icon = icons::Mixer; std::function<void()> onClose; bool maximized = false; float scale = 1.f;
    int designW = 800, designH = 400;

    FloatingPanel()
    {
        resizer = std::make_unique<juce::ResizableBorderComponent> (this, &constrainer);
        resizer->setBorderThickness (juce::BorderSize<int> (7));
        addAndMakeVisible (resizer.get());
    }
    void setDesign (int w, int h)
    {
        designW = w; designH = h; constrainer.setFixedAspectRatio ((double) w / h); constrainer.setMinimumSize (juce::roundToInt (w * .45), juce::roundToInt (h * .45));
        setSize (juce::roundToInt (w * scale), juce::roundToInt (h * scale));
    }
    void resized() override { scale = (float) getWidth() / (float) designW; if (resizer) resizer->setBounds (getLocalBounds()); }

    void paint (juce::Graphics& g) override
    {
        using namespace theme;
        g.addTransform (juce::AffineTransform::scale (scale));
        auto r = juce::Rectangle<float> (0, 0, (float) designW, (float) designH).reduced (1);
        g.setColour (juce::Colour (0xff17181f)); g.fillRoundedRectangle (r.translated (0, 3), 12.f);
        g.setColour (juce::Colour (0xff252731)); g.fillRoundedRectangle (r, 10.f);
        g.setColour (barLine); g.drawRoundedRectangle (r, 10.f, 1.2f);
        icons::draw (g, icon, { 14.f, 4.f, 40.f, 40.f }, text);
        g.setColour (text); g.setFont (juce::FontOptions (19.f, juce::Font::bold)); g.drawText (title, 62, 0, 400, kHead, juce::Justification::centredLeft);
        const auto mx = maxR(), cx = closeR(); g.setColour (text);
        g.drawRect (mx.getCentreX() - 8.f, mx.getCentreY() - 8.f, 16.f, 16.f, 1.8f);
        g.drawLine (cx.getCentreX() - 8, cx.getCentreY() - 8, cx.getCentreX() + 8, cx.getCentreY() + 8, 1.8f); g.drawLine (cx.getCentreX() + 8, cx.getCentreY() - 8, cx.getCentreX() - 8, cx.getCentreY() + 8, 1.8f);
        paintBody (g);
    }
    void mouseDown (const juce::MouseEvent& e0) override
    {
        auto e = sc (e0);
        if (closeR().contains (e.position)) { if (onClose) onClose(); return; }
        if (maxR().contains (e.position)) { toggleMax(); return; }
        if (e.position.y < kHead) { dragging = true; dragOff = e0.getEventRelativeTo (getParentComponent()).getPosition() - getPosition(); return; }
        dragging = false; bodyDown (e);
    }
    void mouseDrag (const juce::MouseEvent& e0) override
    {
        if (dragging && getParentComponent())
        {
            if (maximized) { maximized = false; }
            auto p = e0.getEventRelativeTo (getParentComponent()).getPosition() - dragOff; auto pb = getParentComponent()->getLocalBounds();
            setTopLeftPosition (juce::jlimit (-getWidth() + 120, juce::jmax (0, pb.getWidth() - 120), p.x), juce::jlimit (0, juce::jmax (0, pb.getHeight() - kHead), p.y));
        }
        else if (! dragging) bodyDrag (sc (e0));
    }
    void mouseUp (const juce::MouseEvent& e) override { if (! dragging) bodyUp (sc (e)); dragging = false; }
    void mouseDoubleClick (const juce::MouseEvent& e0) override { auto e = sc (e0); if (e.position.y >= kHead) bodyDouble (e); else if (! closeR().contains (e.position) && ! maxR().contains (e.position)) toggleMax(); }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override { bodyWheel (sc (e), w); }
    void mouseMove (const juce::MouseEvent& e) override { bodyMove (sc (e)); }

    void toggleMax()
    {
        auto* p = getParentComponent(); if (p == nullptr) return;
        if (! maximized)
        {
            normal = getBounds(); auto pb = p->getLocalBounds().reduced (6); const float s = std::min ((float) pb.getWidth() / designW, (float) pb.getHeight() / designH);
            setBounds (juce::Rectangle<int> (juce::roundToInt (designW * s), juce::roundToInt (designH * s)).withCentre (pb.getCentre()));
        }
        else setBounds (normal);
        maximized = ! maximized; repaint();
    }
    juce::Rectangle<float> closeR() const { return { (float) designW - 56.f, 4.f, 40.f, 40.f }; }
    juce::Rectangle<float> maxR() const { return { (float) designW - 104.f, 4.f, 40.f, 40.f }; }

    // utilidades de dibujo compartidas
    static void screw (juce::Graphics& g, juce::Point<float> c)
    {
        g.setColour (juce::Colour (0xff0d0c10)); g.fillEllipse (c.x - 5, c.y - 5, 10, 10);
        g.setColour (juce::Colour (0xff3a3841)); g.fillEllipse (c.x - 3.5f, c.y - 3.5f, 7, 7);
    }
    static void screws (juce::Graphics& g, juce::Rectangle<float> r) { for (auto p : { r.getTopLeft() + juce::Point<float> (10, 10), r.getTopRight() + juce::Point<float> (-10, 10), r.getBottomLeft() + juce::Point<float> (10, -10), r.getBottomRight() + juce::Point<float> (-10, -10) }) screw (g, p); }

protected:
    virtual void paintBody (juce::Graphics&) = 0;
    virtual void bodyDown (const juce::MouseEvent&) {}
    virtual void bodyDrag (const juce::MouseEvent&) {}
    virtual void bodyUp (const juce::MouseEvent&) {}
    virtual void bodyDouble (const juce::MouseEvent&) {}
    virtual void bodyMove (const juce::MouseEvent&) {}
    virtual void bodyWheel (const juce::MouseEvent&, const juce::MouseWheelDetails&) {}
    juce::MouseEvent sc (const juce::MouseEvent& e) const { return e.withNewPosition (e.position / scale); }
    juce::Rectangle<int> toLocal (juce::Rectangle<float> r) const { return r.transformedBy (juce::AffineTransform::scale (scale)).toNearestInt(); }
private:
    bool dragging = false; juce::Point<int> dragOff; juce::Rectangle<int> normal;
    juce::ComponentBoundsConstrainer constrainer; std::unique_ptr<juce::ResizableBorderComponent> resizer;
};
