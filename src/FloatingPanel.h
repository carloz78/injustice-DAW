#pragma once
#include "Icons.h"
#include <functional>
#include <cstdlib>

// Ventana flotante propia: se arrastra por la barra, se redimensiona desde bordes y esquinas (el contenido
// se escala), se maximiza y se cierra. El contenido se dibuja en coordenadas de diseño (designW x designH).

// Sombra de un panel que acaba de cerrarse: se encoge y se desvanece (como en FL Studio). No intercepta el ratón.
class CloseGhost : public juce::Component
{
public:
    CloseGhost (juce::Image i) : img (std::move (i)) { setInterceptsMouseClicks (false, false); setOpaque (false); }
    void start()
    {
        vb = std::make_unique<juce::VBlankAttachment> (this, [this]
        {
            t += 1.f / 8.f; if (t >= 1.f) { vb.reset(); juce::Component::SafePointer<CloseGhost> sp (this); juce::MessageManager::callAsync ([sp] { delete sp.getComponent(); }); return; }
            repaint();
        });
    }
    void paint (juce::Graphics& g) override
    {
        const float e = t * t, s = 1.f - .06f * e; g.setOpacity (1.f - e);
        g.drawImageTransformed (img, juce::AffineTransform::scale (s, s, getWidth() * .5f, getHeight() * .5f));
    }
private:
    juce::Image img; float t = 0.f; std::unique_ptr<juce::VBlankAttachment> vb;
};

class FloatingPanel : public juce::Component, public juce::TooltipClient
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
    // animación al abrir (aparece creciendo y desvaneciéndose) y al cerrar (sombra que se encoge); apagada en las pruebas automáticas
    void visibilityChanged() override
    {
        auto* par = getParentComponent(); if (par == nullptr || std::getenv ("INJ_SELFTEST") != nullptr || std::getenv ("INJ_SHOT") != nullptr) return;
        if (isVisible())
        {
            openT = 0.f; setAlpha (0.f);
            openVb = std::make_unique<juce::VBlankAttachment> (this, [this]
            {
                openT = std::min (1.f, openT + 1.f / 9.f); const float e = 1.f - std::pow (1.f - openT, 3.f);
                setAlpha (e); setTransform (juce::AffineTransform::scale (.95f + .05f * e, .95f + .05f * e, getWidth() * .5f, getHeight() * .5f));
                if (openT >= 1.f) { setAlpha (1.f); setTransform ({}); openVb.reset(); }
            });
        }
        else
        {
            if (openVb) { openVb.reset(); setAlpha (1.f); setTransform ({}); }
            auto img = createComponentSnapshot (getLocalBounds(), true, 1.f);
            if (img.isValid()) { auto* gh = new CloseGhost (img); par->addAndMakeVisible (gh); gh->setBounds (getBounds()); gh->toFront (false); gh->start(); }
        }
    }
    void resized() override { scale = (float) getWidth() / (float) designW; if (resizer) resizer->setBounds (getLocalBounds()); }

    void paint (juce::Graphics& g) override
    {
        using namespace theme;
        g.addTransform (juce::AffineTransform::scale (scale));
        auto r = juce::Rectangle<float> (0, 0, (float) designW, (float) designH).reduced (1);
        g.setColour (juce::Colours::black.withAlpha (.35f)); g.fillRoundedRectangle (r.expanded (1.f), 4.f);   // sombra de apoyo
        g.setColour (bgBar2); g.fillRoundedRectangle (r, 3.f);
        g.setColour (barLine.withAlpha (.8f)); g.drawRoundedRectangle (r.reduced (.5f), 3.f, 1.f);
        { juce::Path tb; tb.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), (float) kHead, 3.f, 3.f, true, true, false, false); g.setColour (bgBar); g.fillPath (tb); }
        g.setColour (seam); g.fillRect (r.getX(), (float) kHead, r.getWidth(), 1.f); g.setColour (edgeLight); g.fillRect (r.getX() + 3.f, r.getY() + 1.f, r.getWidth() - 6.f, 1.f);
        icons::draw (g, icon, { 12.f, 6.f, 36.f, 36.f }, iconIdle);
        g.setColour (text); g.setFont (fonts::ui (14.f, true)); g.drawText (title, 52, 0, 400, kHead, juce::Justification::centredLeft);
        const auto mx = maxR(), cx = closeR(); g.setColour (textDim.brighter (.25f));
        g.drawRect (mx.getCentreX() - 6.f, mx.getCentreY() - 6.f, 12.f, 12.f, 1.3f);
        g.drawLine (cx.getCentreX() - 6, cx.getCentreY() - 6, cx.getCentreX() + 6, cx.getCentreY() + 6, 1.4f); g.drawLine (cx.getCentreX() + 6, cx.getCentreY() - 6, cx.getCentreX() - 6, cx.getCentreY() + 6, 1.4f);
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
    void mouseMove (const juce::MouseEvent& e) override { hoverPos = sc (e).position; bodyMove (sc (e)); }
    void mouseExit (const juce::MouseEvent&) override { hoverPos = { -1.f, -1.f }; }
    juce::String getTooltip() override
    {
        if (closeR().contains (hoverPos)) return "Cerrar";
        if (maxR().contains (hoverPos)) return "Maximizar";
        return hoverPos.y >= kHead ? tipAt (hoverPos) : juce::String();
    }

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
        g.setColour (juce::Colour (0xff101010)); g.fillEllipse (c.x - 5, c.y - 5, 10, 10);
        g.setColour (juce::Colour (0xff3d3d3d)); g.fillEllipse (c.x - 3.5f, c.y - 3.5f, 7, 7);
    }
    static void screws (juce::Graphics&, juce::Rectangle<float>) { return; }
    static void screwsOld (juce::Graphics& g, juce::Rectangle<float> r) { for (auto p : { r.getTopLeft() + juce::Point<float> (10, 10), r.getTopRight() + juce::Point<float> (-10, 10), r.getBottomLeft() + juce::Point<float> (10, -10), r.getBottomRight() + juce::Point<float> (-10, -10) }) screw (g, p); }

protected:
    virtual void paintBody (juce::Graphics&) = 0;
    virtual void bodyDown (const juce::MouseEvent&) {}
    virtual void bodyDrag (const juce::MouseEvent&) {}
    virtual void bodyUp (const juce::MouseEvent&) {}
    virtual void bodyDouble (const juce::MouseEvent&) {}
    virtual void bodyMove (const juce::MouseEvent&) {}
    virtual void bodyWheel (const juce::MouseEvent&, const juce::MouseWheelDetails&) {}
    virtual juce::String tipAt (juce::Point<float>) { return {}; }
    juce::Point<float> hoverPos { -1.f, -1.f };
    juce::MouseEvent sc (const juce::MouseEvent& e) const { return e.withNewPosition (e.position / scale); }
    juce::Rectangle<int> toLocal (juce::Rectangle<float> r) const { return r.transformedBy (juce::AffineTransform::scale (scale)).toNearestInt(); }
private:
    float openT = 1.f; std::unique_ptr<juce::VBlankAttachment> openVb;
    bool dragging = false; juce::Point<int> dragOff; juce::Rectangle<int> normal;
    juce::ComponentBoundsConstrainer constrainer; std::unique_ptr<juce::ResizableBorderComponent> resizer;
};
