#pragma once
#include "FxCore.h"
#include "InjSkinData.h"
// Ventanas con panel de equipo real: una imagen de fondo (con los LEDs apagados), tapas de perilla que giran y LEDs encendidos que se dibujan encima.
namespace injfx
{
inline juce::Image skinImage (const char* resource)
{
    int sz = 0; const char* d = InjSkinData::getNamedResource (resource, sz);
    return d != nullptr ? juce::ImageFileFormat::loadFrom (d, (size_t) sz) : juce::Image();
}

class SkinEditor : public FxEditor
{
public:
    struct Knob { int idx; float cx, cy, r; juce::Image cap; float a0 = -135.f, a1 = 135.f; float rot0 = 0.f; std::vector<float> stops; std::vector<float> cv, ca; };   // cv/ca: valores y ángulos de una perilla continua con marcas desiguales (se interpola entre ellos)   // rot0: ángulo (grados desde la vertical) al que apunta la tapa en la imagen original; stops: ángulo de cada posición de un selector            // coordenadas en píxeles de la imagen original
    struct Hot { juce::Rectangle<float> r; std::function<void()> click; };
    SkinEditor (InjFx& f, juce::Image bgImage, float scale, juce::Rectangle<int> crop = {})   // crop: parte de la imagen que se muestra (para quitar el borde negro); las coordenadas siguen siendo las de la imagen completa
        : FxEditor (f, (int) std::lround ((crop.isEmpty() ? bgImage.getWidth() : crop.getWidth()) * scale), (int) std::lround ((crop.isEmpty() ? bgImage.getHeight() : crop.getHeight()) * scale), "M3 12h18"),
          bg (crop.isEmpty() ? bgImage : bgImage.getClippedImage (crop)), sc (scale), org ((float) crop.getX(), (float) crop.getY()) { for (auto* c : getChildren()) c->setVisible (false); }   // la cabecera de la ventana base no se usa
    void resized() override {}   // sin cabecera: el panel ocupa toda la ventana
    void paint (juce::Graphics& g) override
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (bg, getLocalBounds().toFloat());
        g.addTransform (juce::AffineTransform::translation (-org.x * sc, -org.y * sc));
        paintSkin (g);
        for (auto& k : knobs)
        {
            const float n = juce::jlimit (0.f, 1.f, fx.param (k.idx)->getCurrentNormalisedValue()), h = k.cap.getWidth() * .5f; float deg = k.a0 + (k.a1 - k.a0) * n;
            if (! k.cv.empty()) { const float v = fx.param (k.idx)->getCurrentValue(); deg = k.ca.back(); for (size_t j = 0; j + 1 < k.cv.size(); ++j) if (v <= k.cv[j + 1]) { deg = v <= k.cv[j] ? k.ca[j] : k.ca[j] + (k.ca[j + 1] - k.ca[j]) * (v - k.cv[j]) / (k.cv[j + 1] - k.cv[j]); break; } }
            if (! k.stops.empty()) deg = k.stops[(size_t) juce::jlimit (0, (int) k.stops.size() - 1, juce::roundToInt (fx.param (k.idx)->getCurrentValue()))];
            const float a = (deg - k.rot0) * juce::MathConstants<float>::pi / 180.f;
            g.drawImageTransformed (k.cap, juce::AffineTransform::translation (-h, -h).rotated (a).scaled (sc).translated (k.cx * sc, k.cy * sc), false);
        }
        if (! fx.isEnabled()) { g.setColour (juce::Colours::black.withAlpha (.5f)); g.fillRect (getLocalBounds().translated ((int) (org.x * sc), (int) (org.y * sc))); }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto p = e.position / sc + org; active = -1;
        if (const int i = hitKnob (p); i >= 0) { active = i; startN = fx.param (knobs[(size_t) i].idx)->getCurrentNormalisedValue(); fx.param (knobs[(size_t) i].idx)->parameterChangeGestureBegin(); return; }
        for (auto& h : hots) if (h.r.contains (p) && h.click) { h.click(); repaint(); return; }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (active < 0) return;
        const float k = e.mods.isShiftDown() ? 800.f : 220.f; setN (knobs[(size_t) active].idx, startN - (float) e.getDistanceFromDragStartY() / k);
    }
    void mouseUp (const juce::MouseEvent&) override { if (active >= 0) fx.param (knobs[(size_t) active].idx)->parameterChangeGestureEnd(); active = -1; }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const auto p = e.position / sc + org;
        if (const int i = hitKnob (p); i >= 0) { const int ix = knobs[(size_t) i].idx; fx.param (ix)->parameterChangeGestureBegin(); fx.setValue (ix, fx.defaultOf (ix)); fx.param (ix)->parameterChangeGestureEnd(); repaint(); }
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const auto p = e.position / sc + org;
        if (const int i = hitKnob (p); i >= 0) { const int ix = knobs[(size_t) i].idx; setN (ix, fx.param (ix)->getCurrentNormalisedValue() + w.deltaY * (e.mods.isShiftDown() ? .02f : .08f)); }
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto p = e.position / sc + org; bool onKnob = false, onHot = false;
        onKnob = hitKnob (p) >= 0;
        for (auto& h : hots) if (h.r.contains (p)) onHot = true;
        setMouseCursor (onKnob ? juce::MouseCursor::UpDownResizeCursor : (onHot ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor));
    }
    int hitKnob (juce::Point<float> p) const   // la perilla más pequeña que contiene el punto (así una tapa gana a la falda que la rodea)
    {
        int best = -1; float br = 1e9f;
        for (size_t i = 0; i < knobs.size(); ++i) if (p.getDistanceFrom ({ knobs[i].cx, knobs[i].cy }) <= knobs[i].r * 1.25f && knobs[i].r < br) { best = (int) i; br = knobs[i].r; }
        return best;
    }
protected:
    virtual void paintSkin (juce::Graphics&) {}
    void tick() override { repaint(); }
    // LED encendido: sprite número idx del atlas (cuadros cuadrados uno junto a otro) centrado en (cx,cy) de la imagen original
    void drawLed (juce::Graphics& g, const juce::Image& atlas, int idx, float cx, float cy)
    {
        const int s = atlas.getHeight(); const float d = s * sc;
        g.drawImage (atlas, juce::roundToInt (cx * sc - d * .5f), juce::roundToInt (cy * sc - d * .5f), juce::roundToInt (d), juce::roundToInt (d), idx * s, 0, s, s, false);
    }
    void drawSprite (juce::Graphics& g, const juce::Image& im, float cx, float cy)
    {
        const float w = im.getWidth() * sc, h = im.getHeight() * sc; g.drawImage (im, juce::Rectangle<float> (cx * sc - w * .5f, cy * sc - h * .5f, w, h));
    }
    // aguja de un VU: parte de (px,py) (centro del arco, en píxeles de la imagen), apunta al ángulo (grados desde la vertical) y se recorta a la caja de la cara del medidor
    void drawNeedle (juce::Graphics& g, float px, float py, float deg, float r0, float r1, juce::Rectangle<float> face, juce::Colour col, float widthPx)
    {
        const float a = deg * juce::MathConstants<float>::pi / 180.f, ux = std::sin (a), uy = -std::cos (a);
        juce::Graphics::ScopedSaveState ss (g); g.reduceClipRegion ((face * sc).toNearestInt());
        const juce::Line<float> ln ((px + ux * r0) * sc, (py + uy * r0) * sc, (px + ux * r1) * sc, (py + uy * r1) * sc);
        g.setColour (juce::Colours::black.withAlpha (.28f)); g.drawLine (juce::Line<float> (ln.getStart() + juce::Point<float> (3.f * sc, 3.f * sc), ln.getEnd() + juce::Point<float> (3.f * sc, 3.f * sc)), widthPx * sc * 1.4f);
        g.setColour (col); g.drawLine (ln, widthPx * sc);
    }
    void setN (int idx, float n)
    {
        n = juce::jlimit (0.f, 1.f, n); auto* p = fx.param (idx); const auto r = p->valueRange; p->setParameter (r.snapToLegalValue (r.convertFrom0to1 (n)), juce::sendNotificationSync); repaint();
    }
    void choose (int idx, int v) { fx.setValue (idx, (float) v); }
    juce::Image bg; float sc; juce::Point<float> org; std::vector<Knob> knobs; std::vector<Hot> hots; int active = -1; float startN = 0.f;
};
} // namespace injfx
