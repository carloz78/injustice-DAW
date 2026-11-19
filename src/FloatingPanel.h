#pragma once
#include "Icons.h"
#include <functional>

// Ventana flotante propia (arrastrable, maximizable, cerrable) para Mixer y Step Sequencer.
class FloatingPanel : public juce::Component
{
public:
    static constexpr int kHead = 48;
    juce::String title; icons::Id icon = icons::Mixer; std::function<void()> onClose; bool maximized = false;

    void paint (juce::Graphics& g) override
    {
        using namespace theme;
        auto r = getLocalBounds().toFloat().reduced (1);
        g.setColour (juce::Colour (0xff17161b)); g.fillRoundedRectangle (r, 12.f);
        g.setColour (juce::Colour (0xff2c2a33)); g.drawRoundedRectangle (r, 12.f, 1.5f);
        g.setColour (juce::Colour (0xff222027)); g.fillRect (1, kHead, getWidth() - 2, 1);
        icons::draw (g, icon, { 14.f, 4.f, 40.f, 40.f }, text);
        g.setColour (text); g.setFont (juce::FontOptions (19.f, juce::Font::bold)); g.drawText (title, 62, 0, 400, kHead, juce::Justification::centredLeft);
        g.setColour (text); const auto mx = maxR(), cx = closeR();
        g.drawRect (mx.getCentreX() - 8.f, mx.getCentreY() - 8.f, 16.f, 16.f, 1.8f);
        g.drawLine (cx.getCentreX() - 8, cx.getCentreY() - 8, cx.getCentreX() + 8, cx.getCentreY() + 8, 1.8f); g.drawLine (cx.getCentreX() + 8, cx.getCentreY() - 8, cx.getCentreX() - 8, cx.getCentreY() + 8, 1.8f);
        paintBody (g);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (closeR().contains (e.position)) { if (onClose) onClose(); return; }
        if (maxR().contains (e.position)) { toggleMax(); return; }
        if (e.y < kHead) { dragging = true; dragStart = e.getPosition(); return; }
        dragging = false; bodyDown (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging && ! maximized && getParentComponent()) { auto p = getPosition() + e.getPosition() - dragStart; auto pb = getParentComponent()->getLocalBounds();
            setTopLeftPosition (juce::jlimit (0, juce::jmax (0, pb.getWidth() - getWidth()), p.x), juce::jlimit (0, juce::jmax (0, pb.getHeight() - getHeight()), p.y)); }
        else if (! dragging) bodyDrag (e);
    }
    void mouseUp (const juce::MouseEvent& e) override { if (! dragging) bodyUp (e); dragging = false; }
    void mouseDoubleClick (const juce::MouseEvent& e) override { if (e.y >= kHead) bodyDouble (e); }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override { bodyWheel (e, w); }

    void toggleMax()
    {
        if (auto* p = getParentComponent()) { if (! maximized) { normal = getBounds(); setBounds (p->getLocalBounds().reduced (6)); } else setBounds (normal); maximized = ! maximized; repaint(); }
    }
    juce::Rectangle<float> closeR() const { return { (float) getWidth() - 56.f, 4.f, 40.f, 40.f }; }
    juce::Rectangle<float> maxR() const { return { (float) getWidth() - 104.f, 4.f, 40.f, 40.f }; }

protected:
    virtual void paintBody (juce::Graphics&) = 0;
    virtual void bodyDown (const juce::MouseEvent&) {}
    virtual void bodyDrag (const juce::MouseEvent&) {}
    virtual void bodyUp (const juce::MouseEvent&) {}
    virtual void bodyDouble (const juce::MouseEvent&) {}
    virtual void bodyWheel (const juce::MouseEvent&, const juce::MouseWheelDetails&) {}
private:
    bool dragging = false; juce::Point<int> dragStart; juce::Rectangle<int> normal;
};
