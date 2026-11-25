#pragma once

#include "FxAnalyzer.h"
#include "FxSkin.h"
#include <functional>

// Pantalla de INJUSTICE ANALYZER. Todo se dibuja en codigo sobre un lienzo de diseno de 1752 x 897
// y se escala a la ventana (redimensionable, proporcion fija).
class AnalyzerEditor : public te::Plugin::EditorComponent,
                       public juce::SettableTooltipClient,
                       private juce::Timer
{
public:
    explicit AnalyzerEditor (injfx::InjAnalyzer&);
    bool allowWindowResizing() override { return true; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return &constrainer; }
    ~AnalyzerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int kW = 1752, kH = 897, kHCompact = 665;

    // para las pruebas de captura
    void tickForTest (double dt) { step (dt); }

private:
    struct Hit
    {
        juce::Rectangle<int> r;
        std::function<void (juce::Point<int>)> click;
        std::function<void (int)> wheel;
        juce::String tip;
        bool controlsPanel = false;
    };

    struct Chan { float barPeak = -200.f, rms = -200.f, hold = -200.f, holdLeft = 0.f; };

    void timerCallback() override;
    void step (double dt);
    float scale() const { return (float) getWidth() / (float) kW; }
    int designHeight() const { return proc.compactUi.load() ? kHCompact : kH; }
    void applySize();
    void buildHits();
    void ensureSpectrum();

    // dibujo
    void paintHeader (juce::Graphics&);
    void paintPlot (juce::Graphics&);
    void paintRight (juce::Graphics&);
    void paintControls (juce::Graphics&);
    void text (juce::Graphics&, const juce::String&, float x, float y, float w, float h, juce::Justification,
               juce::Colour, float size, bool bold = false, float kern = 0.f);
    void segButtons (juce::Graphics&, int y0, int y1, const int* xs, const juce::StringArray& labels, int count, int selected);
    void stepper (juce::Graphics&, const juce::String& value, float cx, float cy, float arrowX);
    void plainButton (juce::Graphics&, juce::Rectangle<int>, const juce::String&, bool active);

    float yOf (float db) const;
    float xOf (double hz) const;

    // acciones
    void cycleChoice (const char* id, int dir, int count);
    void popupChoice (const char* id, const juce::StringArray& items, juce::Rectangle<int> designArea);
    void showGearMenu();
    void resetSpectrumPeaks();
    void resetLevelPeaks();

    injfx::InjAnalyzer& proc;
    juce::ComponentBoundsConstrainer constrainer;
    juce::TooltipWindow tooltipWindow { this, 600 };
    juce::Image logo;
    std::vector<Hit> hits;

    an::SpectrumEngine spec;
    double specSr = 0.0; int specOrder = 0, specBands = 0;
    double lastTime = 0.0, lastTextTime = 0.0;
    Chan ch[2];
    float shownRms[2] { -200.f, -200.f };
    float lufsI = -200.f, lufsS = -200.f, lra = 0.f;
    int ctrlHash = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalyzerEditor)
};
