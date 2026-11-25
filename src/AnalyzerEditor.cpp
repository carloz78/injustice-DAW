#include "AnalyzerEditor.h"

using namespace juce;

namespace
{
    // ---- paleta (tomada de la referencia)
    const Colour kBg        (0xff030606);
    const Colour kPlotBg    (0xff010304);
    const Colour kPanelBg   (0xff0a0f11);
    const Colour kBorder    (0xff384348);
    const Colour kGrid      (0xff1b2327);
    const Colour kText      (0xffc9cfd1);
    const Colour kDim       (0xff8d979b);
    const Colour kGreen     (0xff3dff14);
    const Colour kBar       (0xff17f50d);
    const Colour kYellow    (0xffffee00);
    const Colour kBtn       (0xff12181b);
    const Colour kBtnBorder (0xff30393e);
    const Colour kSelBg     (0xff06210a);

    // ---- geometria del diseno (pixeles de 1752 x 897)
    constexpr float kPlotL = 105.f, kPlotR = 1387.f, kPlotT = 158.f, kPlotB = 590.f;
    constexpr float kMeterTop = 278.f, kMeterBottom = 585.f;
    constexpr int kSegments = 36;
    constexpr float kMeterL = 1495.f, kMeterR = 1646.f, kMeterW = 32.f;

    String fontName()
    {
       #if JUCE_WINDOWS
        return "Segoe UI";
       #else
        return Font::getDefaultSansSerifFontName();
       #endif
    }

    String dbText (float v)
    {
        if (v <= -150.f) return "-inf";
        if (std::abs (v) < 0.05f) return "0.0";
        return String (v, 1);
    }
}

//==============================================================================================
AnalyzerEditor::AnalyzerEditor (injfx::InjAnalyzer& p) : proc (p)
{
    logo = injfx::skinImage ("an_logo_png");
    setTooltip ({});

    constrainer.setMinimumSize (584, 299);
    constrainer.setMaximumSize (2628, 1346);
    ensureSpectrum();
    buildHits();
    applySize();
    lastTime = Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

AnalyzerEditor::~AnalyzerEditor() { stopTimer(); }

void AnalyzerEditor::applySize()
{
    const int h = designHeight();
    constrainer.setFixedAspectRatio ((double) kW / (double) h);
    const int w = getWidth() > 0 ? getWidth() : 1226;
    setSize (w, roundToInt (w * (float) h / (float) kW));
}

void AnalyzerEditor::resized() {}

float AnalyzerEditor::yOf (float db) const
{
    const float top = anopt::topValues[proc.choice ("top")];
    const float range = anopt::rangeValues[proc.choice ("range")];
    return kPlotT + (top - db) / range * (kPlotB - kPlotT);
}

float AnalyzerEditor::xOf (double hz) const
{
    return kPlotL + (float) (std::log (hz / an::SpectrumEngine::kFLo) / std::log (an::SpectrumEngine::kFHi / an::SpectrumEngine::kFLo)) * (kPlotR - kPlotL);
}

//==============================================================================================
void AnalyzerEditor::text (Graphics& g, const String& t, float x, float y, float w, float h, Justification j,
                           Colour c, float size, bool bold, float kern)
{
    Font f (FontOptions (fontName(), size, bold ? Font::bold : Font::plain));
    if (! exactlyEqual (kern, 0.f)) f = f.withExtraKerningFactor (kern);
    g.setFont (f);
    g.setColour (c);
    g.drawText (t, Rectangle<float> (x, y, w, h), j, false);
}

void AnalyzerEditor::segButtons (Graphics& g, int y0, int y1, const int* xs, const StringArray& labels, int count, int selected)
{
    const Rectangle<float> all ((float) xs[0], (float) y0, (float) (xs[count] - xs[0]), (float) (y1 - y0));
    g.setColour (kBtn);
    g.fillRoundedRectangle (all, 10.f);
    g.setColour (kBtnBorder);
    g.drawRoundedRectangle (all.reduced (1.f), 10.f, 2.f);
    for (int i = 1; i < count; ++i)
        g.fillRect ((float) xs[i] - 1.f, (float) y0 + 2.f, 2.f, (float) (y1 - y0) - 4.f);
    if (selected >= 0)
    {
        const Rectangle<float> r ((float) xs[selected], (float) y0, (float) (xs[selected + 1] - xs[selected]), (float) (y1 - y0));
        g.setColour (kSelBg);
        g.fillRoundedRectangle (r.reduced (2.f), 9.f);
        g.setColour (kGreen.withAlpha (0.85f));
        g.drawRoundedRectangle (r.reduced (2.f), 9.f, 2.2f);
    }
    for (int i = 0; i < count; ++i)
        text (g, labels[i], (float) xs[i], (float) y0, (float) (xs[i + 1] - xs[i]), (float) (y1 - y0), Justification::centred,
              i == selected ? kGreen : kText, 30.f);
}

void AnalyzerEditor::plainButton (Graphics& g, Rectangle<int> r, const String& label, bool active)
{
    const auto f = r.toFloat();
    g.setColour (active ? kSelBg : Colour (0xff171d20));
    g.fillRoundedRectangle (f, 10.f);
    g.setColour (active ? kGreen.withAlpha (0.85f) : kBtnBorder);
    g.drawRoundedRectangle (f.reduced (1.f), 10.f, active ? 2.2f : 2.f);
    text (g, label, f.getX(), f.getY(), f.getWidth(), f.getHeight(), Justification::centred, active ? kGreen : kText, 30.f);
}

void AnalyzerEditor::stepper (Graphics& g, const String& value, float cx, float cy, float arrowX)
{
    text (g, value, cx - 110.f, cy - 24.f, 220.f, 48.f, Justification::centred, kGreen, 33.f);
    g.setColour (kText);
    Path up, dn;
    up.addTriangle (arrowX - 6.5f, cy - 5.f, arrowX + 6.5f, cy - 5.f, arrowX, cy - 15.f);
    dn.addTriangle (arrowX - 6.5f, cy + 5.f, arrowX + 6.5f, cy + 5.f, arrowX, cy + 15.f);
    g.fillPath (up);
    g.fillPath (dn);
}

//==============================================================================================
void AnalyzerEditor::paint (Graphics& g)
{
    g.fillAll (kBg);
    g.addTransform (AffineTransform::scale (scale()));
    paintHeader (g);
    paintPlot (g);
    paintRight (g);
    if (! proc.compactUi.load()) paintControls (g);
}

void AnalyzerEditor::paintHeader (Graphics& g)
{
    if (logo.isValid())
        g.drawImage (logo, Rectangle<float> (44.f, 2.f, 296.f, 92.f), RectanglePlacement::stretchToFit);

    g.setColour (Colour (0xffa9b2b6));
    g.fillRect (384.f, 14.f, 2.f, 64.f);

    {
        Font f (FontOptions (fontName(), 56.f, Font::bold));
        f = f.withExtraKerningFactor (0.03f);
        g.setFont (f);
        g.setColour (kGreen);
        g.drawFittedText ("ANALYZER", Rectangle<int> (441, -2, 245, 62), Justification::centredLeft, 1, 0.6f);
    }
    {
        Font f (FontOptions (fontName(), 24.f, Font::plain));
        f = f.withExtraKerningFactor (0.55f);
        g.setFont (f);
        g.setColour (Colour (0xff9aa3a7));
        g.drawFittedText ("MULTIMETER", Rectangle<int> (444, 56, 236, 26), Justification::centredLeft, 1, 0.6f);
    }

    // botones del encabezado
    auto box = [&] (float x) { const Rectangle<float> r (x, 19.f, 52.f, 52.f);
                               g.setColour (Colour (0xff0d1214)); g.fillRoundedRectangle (r, 11.f);
                               g.setColour (Colour (0xff9aa3a7)); g.drawRoundedRectangle (r.reduced (1.f), 11.f, 2.f); };
    box (1590.f);
    box (1666.f);

    // engranaje
    {
        Path gear;
        const float cx = 1616.f, cy = 45.f;
        for (int i = 0; i < 8; ++i)
        {
            Path tooth;
            tooth.addRoundedRectangle (-4.2f, -15.5f, 8.4f, 9.f, 1.5f);
            gear.addPath (tooth, AffineTransform::rotation (MathConstants<float>::twoPi * (float) i / 8.f).translated (cx, cy));
        }
        gear.addEllipse (cx - 11.5f, cy - 11.5f, 23.f, 23.f);
        gear.setUsingNonZeroWinding (true);
        Path hole; hole.addEllipse (cx - 5.f, cy - 5.f, 10.f, 10.f);
        g.setColour (Colour (0xffeef1f2));
        g.fillPath (gear);
        g.setColour (Colour (0xff0d1214));
        g.fillPath (hole);
    }
    g.setColour (Colour (0xffeef1f2));
    g.fillRoundedRectangle (1680.f, 41.f, 24.f, 5.f, 2.f);
}

void AnalyzerEditor::paintPlot (Graphics& g)
{
    const Rectangle<float> panel (12.f, 97.f, 1403.f, 553.f);
    g.setColour (kPlotBg);
    g.fillRoundedRectangle (panel, 16.f);
    g.setColour (kBorder);
    g.drawRoundedRectangle (panel.reduced (1.f), 16.f, 2.f);

    const float top = anopt::topValues[proc.choice ("top")];
    const float range = anopt::rangeValues[proc.choice ("range")];
    const float bottom = top - range;

    text (g, "dB", 40.f, 112.f, 60.f, 36.f, Justification::centredLeft, kText, 29.f);
    text (g, "Top:", 140.f, 112.f, 62.f, 36.f, Justification::centredLeft, kDim, 29.f);
    text (g, anopt::tops[proc.choice ("top")], 200.f, 112.f, 100.f, 36.f, Justification::centredLeft, kGreen, 29.f);
    text (g, "Range:", 312.f, 112.f, 90.f, 36.f, Justification::centredLeft, kDim, 29.f);
    text (g, anopt::ranges[proc.choice ("range")], 401.f, 112.f, 100.f, 36.f, Justification::centredLeft, kGreen, 29.f);

    // cuadricula y etiquetas de amplitud
    const float step = range <= 70.f ? 5.f : 10.f;
    for (float v = top; v >= bottom - 0.01f; v -= step)
    {
        const float y = yOf (v);
        g.setColour (kGrid);
        g.fillRect (kPlotL, std::floor (y), kPlotR - kPlotL, 1.f);
        text (g, String (roundToInt (v)), 20.f, y - 14.f, 66.f, 28.f, Justification::centredRight, kText, 27.f);
    }
    // cuadricula y etiquetas de frecuencia (octavas)
    static const char* fl[] = { "16", "31", "62", "125", "250", "500", "1 k", "2 k", "4 k", "8 k", "16 k" };
    for (int i = 0; i < 11; ++i)
    {
        const double hz = 15.625 * std::pow (2.0, i);
        const float x = xOf (std::max (hz, 16.0));
        g.setColour (kGrid);
        g.fillRect (std::floor (x), kPlotT, 1.f, kPlotB - kPlotT);
        text (g, fl[i], x - 40.f, 598.f, 80.f, 30.f, Justification::centred, kText, 27.f);
    }
    text (g, "Hz", 1383.f, 598.f, 36.f, 30.f, Justification::centredLeft, kText, 27.f);
    g.setColour (kGrid.brighter (0.25f));
    g.drawRect (Rectangle<float> (kPlotL, kPlotT, kPlotR - kPlotL, kPlotB - kPlotT), 1.f);

    // barras del espectro (verde solido) e indicadores de pico (amarillo)
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (Rectangle<int> ((int) kPlotL, (int) kPlotT, (int) (kPlotR - kPlotL), (int) (kPlotB - kPlotT)));
        const auto& lv = spec.levels();
        const auto& pk = spec.peaks();
        const auto& e = spec.bandEdges();
        const int nBars = (int) std::min ({ (size_t) spec.numBands(), lv.size(), pk.size(), e.size() > 0 ? e.size() - 1 : (size_t) 0 });
        for (int b = 0; b < nBars; ++b)
        {
            const float x0 = xOf (e[(size_t) b]), x1 = xOf (e[(size_t) b + 1]);
            const float w = x1 - x0;
            const float gap = jlimit (1.f, 3.f, w * 0.16f);
            const float bx = x0 + gap * 0.5f, bw = w - gap;
            const float db = lv[(size_t) b];
            if (db > bottom)
            {
                const float y = jlimit (kPlotT, kPlotB, yOf (db));
                g.setColour (kBar);
                g.fillRect (bx, y, bw, kPlotB - y);
            }
            const float pdb = pk[(size_t) b];
            if (pdb > bottom)
            {
                const float y = jlimit (kPlotT, kPlotB, yOf (pdb));
                g.setColour (kYellow);
                g.fillRect (bx, std::max (kPlotT, y - 6.f), bw, 3.f);
            }
        }
    }
}

void AnalyzerEditor::paintRight (Graphics& g)
{
    const Rectangle<float> panel (1432.f, 97.f, 308.f, 553.f);
    g.setColour (kPlotBg);
    g.fillRoundedRectangle (panel, 16.f);
    g.setColour (kBorder);
    g.drawRoundedRectangle (panel.reduced (1.f), 16.f, 2.f);

    text (g, "LUFS", 1452.f, 112.f, 100.f, 36.f, Justification::centredLeft, kText, 29.f);
    text (g, "L", 1576.f, 112.f, 50.f, 36.f, Justification::centred, kText, 29.f);
    text (g, "R", 1663.f, 112.f, 50.f, 36.f, Justification::centred, kText, 29.f);
    text (g, "I", 1452.f, 148.f, 40.f, 30.f, Justification::centredLeft, kText, 25.f);
    text (g, "S", 1452.f, 177.f, 40.f, 30.f, Justification::centredLeft, kText, 25.f);
    text (g, "LRA", 1452.f, 206.f, 70.f, 30.f, Justification::centredLeft, kText, 25.f);

    text (g, dbText (lufsI), 1470.f, 148.f, 74.f, 30.f, Justification::centredRight, kGreen, 25.f);
    text (g, dbText (lufsS), 1470.f, 177.f, 74.f, 30.f, Justification::centredRight, kGreen, 25.f);
    text (g, lra > 0.f ? String (lra, 1) : String ("0.0"), 1546.f, 206.f, 80.f, 30.f, Justification::centredRight, kGreen, 25.f);

    text (g, dbText (ch[0].hold), 1556.f, 148.f, 72.f, 30.f, Justification::centredRight, kGreen, 25.f);
    text (g, dbText (ch[1].hold), 1644.f, 148.f, 72.f, 30.f, Justification::centredRight, kGreen, 25.f);
    text (g, dbText (shownRms[0]), 1556.f, 177.f, 72.f, 30.f, Justification::centredRight, kGreen, 25.f);
    text (g, dbText (shownRms[1]), 1644.f, 177.f, 72.f, 30.f, Justification::centredRight, kGreen, 25.f);

    text (g, "L", 1485.f, 240.f, 50.f, 32.f, Justification::centred, kText, 27.f);
    text (g, "R", 1636.f, 240.f, 50.f, 32.f, Justification::centred, kText, 27.f);

    // escala de los medidores
    for (int v = 0; v >= -60; v -= 5)
    {
        const float y = 279.f + (float) (-v) * (299.f / 60.f);
        text (g, String (v), 1560.f, y - 14.f, 60.f, 28.f, Justification::centred, kText, 24.f);
    }

    // medidores segmentados
    const int mode = proc.choice ("levelMode");     // 0 Peak, 1 RMS, 2 Peak & RMS
    const float segH = (kMeterBottom - kMeterTop) / (float) kSegments;
    for (int c = 0; c < 2; ++c)
    {
        const float x = c == 0 ? kMeterL : kMeterR;
        const float fill = mode == 0 ? ch[c].barPeak : ch[c].rms;
        const bool showHold = mode != 1;
        for (int i = 0; i < kSegments; ++i)
        {
            const float lowDb = -60.f + (float) i * (60.f / (float) kSegments);
            const float y = kMeterBottom - (float) (i + 1) * segH;
            const Rectangle<float> r (x, y + 0.8f, kMeterW, segH - 1.6f);
            const bool lit = fill > lowDb;
            const bool holdSeg = showHold && ch[c].hold > lowDb && ch[c].hold <= lowDb + 60.f / (float) kSegments;
            g.setColour (holdSeg ? kYellow : (lit ? kBar : Colour (0xff17f50d).withAlpha (0.10f)));
            g.fillRect (r);
        }
    }
}

void AnalyzerEditor::paintControls (Graphics& g)
{
    const Rectangle<float> panel (12.f, 665.f, 1728.f, 215.f);
    g.setColour (kPanelBg);
    g.fillRoundedRectangle (panel, 16.f);
    g.setColour (kBorder);
    g.drawRoundedRectangle (panel.reduced (1.f), 16.f, 2.f);

    g.setColour (Colour (0xff3a444a));
    for (int x : { 641, 965, 1220, 1518 }) g.fillRect ((float) x - 1.f, 685.f, 2.f, 167.f);

    auto title = [&] (const String& t, float cx, float cy) { text (g, t, cx - 130.f, cy - 18.f, 260.f, 36.f, Justification::centred, kText, 30.f); };

    // Detection / Mode
    title ("Detection", 323.f, 690.f);
    title ("Mode", 319.f, 783.f);
    { const int xs[] = { 66, 192, 318, 447, 575 };
      segButtons (g, 716, 758, xs, anopt::detection, 4, proc.choice ("detection")); }
    { const int xs[] = { 66, 243, 415, 575 };
      segButtons (g, 808, 852, xs, anopt::mode, 3, proc.choice ("mode")); }

    // Analyzer Bands + Return Rate
    title ("Analyzer Bands", 809.f, 690.f);
    stepper (g, anopt::bands[proc.choice ("bands")], 803.f, 733.f, 845.f);
    title ("Return Rate", 803.f, 787.f);
    stepper (g, anopt::rates[proc.choice ("specRate")], 793.f, 826.f, 871.f);

    // Peak (espectro)
    title ("Peak", 1090.f, 690.f);
    stepper (g, anopt::holds[proc.choice ("specHold")], 1086.f, 729.f, 1126.f);
    plainButton (g, { 1019, 759, 146, 41 }, "Hold", proc.flag ("specHoldOn"));
    plainButton (g, { 1019, 809, 146, 43 }, "Reset", false);

    // Level + Return Rate
    title ("Level", 1377.f, 690.f);
    stepper (g, anopt::levelMode[proc.choice ("levelMode")], 1378.f, 728.f, 1472.f);
    title ("Return Rate", 1380.f, 787.f);
    stepper (g, anopt::rates[proc.choice ("levelRate")], 1370.f, 826.f, 1449.f);

    // Peak (medidores)
    title ("Peak", 1631.f, 690.f);
    stepper (g, anopt::holds[proc.choice ("levelHold")], 1626.f, 729.f, 1670.f);
    plainButton (g, { 1566, 759, 144, 41 }, "Hold", proc.flag ("levelHoldOn"));
    plainButton (g, { 1566, 809, 144, 43 }, "Reset", false);
}

//==============================================================================================
void AnalyzerEditor::cycleChoice (const char* id, int dir, int count)
{
    proc.setChoice (id, jlimit (0, count - 1, proc.choice (id) + dir));
}

void AnalyzerEditor::popupChoice (const char* id, const StringArray& items, Rectangle<int> area)
{
    PopupMenu m;
    const int cur = proc.choice (id);
    for (int i = 0; i < items.size(); ++i) m.addItem (i + 1, items[i], true, i == cur);
    const float s = scale();
    const auto screen = localAreaToGlobal (Rectangle<int> (roundToInt (area.getX() * s), roundToInt (area.getY() * s),
                                                          roundToInt (area.getWidth() * s), roundToInt (area.getHeight() * s)));
    Component::SafePointer<AnalyzerEditor> safe (this);
    const String sid (id);
    m.showMenuAsync (PopupMenu::Options().withTargetScreenArea (screen),
                     [safe, sid] (int r) { if (safe != nullptr && r > 0) safe->proc.setChoice (sid.toRawUTF8(), r - 1); });
}

void AnalyzerEditor::resetSpectrumPeaks() { spec.resetPeaks(); }

void AnalyzerEditor::resetLevelPeaks()
{
    for (auto& c : ch) { c.hold = c.barPeak; c.holdLeft = 0.f; }
}

void AnalyzerEditor::showGearMenu()
{
    PopupMenu t, r, f, m;
    for (int i = 0; i < anopt::tops.size(); ++i)    t.addItem (100 + i, anopt::tops[i], true, i == proc.choice ("top"));
    for (int i = 0; i < anopt::ranges.size(); ++i)  r.addItem (200 + i, anopt::ranges[i], true, i == proc.choice ("range"));
    for (int i = 0; i < anopt::fftSizes.size(); ++i) f.addItem (300 + i, anopt::fftSizes[i] + " puntos", true, i == proc.choice ("fft"));
    m.addSubMenu ("Top", t);
    m.addSubMenu ("Range", r);
    m.addSubMenu ("FFT", f);
    m.addSeparator();
    m.addItem (400, "Reiniciar LUFS Integrated y LRA");
    const float s = scale();
    const auto screen = localAreaToGlobal (Rectangle<int> (roundToInt (1590 * s), roundToInt (19 * s), roundToInt (52 * s), roundToInt (52 * s)));
    Component::SafePointer<AnalyzerEditor> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetScreenArea (screen), [safe] (int id)
    {
        if (safe == nullptr || id <= 0) return;
        if (id >= 100 && id < 200) safe->proc.setChoice ("top", id - 100);
        else if (id >= 200 && id < 300) safe->proc.setChoice ("range", id - 200);
        else if (id >= 300 && id < 400) safe->proc.setChoice ("fft", id - 300);
        else if (id == 400) safe->proc.loudness().requestReset();
        safe->repaint();
    });
}

void AnalyzerEditor::buildHits()
{
    hits.clear();
    auto add = [this] (Rectangle<int> r, String tip, std::function<void (Point<int>)> click, std::function<void (int)> wheel = {}, bool ctrl = false)
    {
        Hit h; h.r = r; h.tip = std::move (tip); h.click = std::move (click); h.wheel = std::move (wheel); h.controlsPanel = ctrl;
        hits.push_back (std::move (h));
    };

    add ({ 1590, 19, 52, 52 }, "Ajustes: Top, Range y tamano de FFT", [this] (Point<int>) { showGearMenu(); });
    add ({ 1666, 19, 52, 52 }, "Vista compacta (oculta los controles inferiores)", [this] (Point<int>)
    {
        proc.compactUi.store (! proc.compactUi.load());
        applySize();
        repaint();
    });
    add ({ 140, 108, 160, 44 }, "Top: nivel maximo de la escala", [this] (Point<int> p) { popupChoice ("top", anopt::tops, { 200, 112, 100, 36 }); (void) p; },
         [this] (int d) { cycleChoice ("top", d < 0 ? 1 : -1, anopt::tops.size()); });
    add ({ 312, 108, 200, 44 }, "Range: margen dinamico mostrado", [this] (Point<int>) { popupChoice ("range", anopt::ranges, { 401, 112, 100, 36 }); },
         [this] (int d) { cycleChoice ("range", d, anopt::ranges.size()); });
    add ({ 1446, 108, 100, 44 }, "Clic: reinicia LUFS Integrated y LRA", [this] (Point<int>) { proc.loudness().requestReset(); });
    add ({ 1446, 144, 110, 66 }, "LUFS: I = Integrated, S = Short-term (3 s)", [] (Point<int>) {});
    add ({ 1446, 204, 190, 34 }, "LRA: rango de sonoridad (EBU Tech 3342), en LU", [] (Point<int>) {});
    add ({ 1556, 144, 170, 34 }, "Pico (con retencion), en dBFS", [] (Point<int>) {});
    add ({ 1556, 174, 170, 34 }, "RMS (ventana de 300 ms), en dBFS", [] (Point<int>) {});

    // Detection / Mode
    {
        static const int dx[] = { 66, 192, 318, 447, 575 };
        for (int i = 0; i < 4; ++i)
            add ({ dx[i], 716, dx[i + 1] - dx[i], 42 }, "Detection: canal que se analiza", [this, i] (Point<int>) { proc.setChoice ("detection", i); }, {}, true);
        static const int mx[] = { 66, 243, 415, 575 };
        const char* tips[] = { "RMS Slow: promedio de 1 s", "RMS Fast: promedio de 125 ms", "Peak: valor instantaneo de cada trama" };
        for (int i = 0; i < 3; ++i)
            add ({ mx[i], 808, mx[i + 1] - mx[i], 44 }, tips[i], [this, i] (Point<int>) { proc.setChoice ("mode", i); }, {}, true);
    }

    // Steppers
    struct S { const char* id; const StringArray* items; Rectangle<int> area; float arrowX, cy; const char* tip; };
    const S ss[] = {
        { "bands",     &anopt::bands,     { 733, 708, 130, 52 },  845.f,  733.f, "Analyzer Bands: numero de barras del espectro" },
        { "specRate",  &anopt::rates,     { 700, 800, 190, 52 },  871.f,  826.f, "Return Rate: velocidad maxima de caida de las barras" },
        { "specHold",  &anopt::holds,     { 1036, 706, 110, 46 }, 1126.f, 729.f, "Tiempo de retencion de los indicadores de pico" },
        { "levelMode", &anopt::levelMode, { 1280, 704, 220, 50 }, 1472.f, 728.f, "Level: lo que muestran los medidores L / R" },
        { "levelRate", &anopt::rates,     { 1280, 800, 190, 52 }, 1449.f, 826.f, "Return Rate: velocidad maxima de caida de los medidores" },
        { "levelHold", &anopt::holds,     { 1580, 706, 110, 46 }, 1670.f, 729.f, "Tiempo de retencion del pico de los medidores" },
    };
    for (const auto& s : ss)
    {
        const char* id = s.id; const StringArray* items = s.items; const float ax = s.arrowX, cy = s.cy; const Rectangle<int> area = s.area;
        add (area.withRight ((int) ax + 18), s.tip,
             [this, id, items, ax, cy, area] (Point<int> p)
             {
                 if ((float) p.x >= ax - 14.f) cycleChoice (id, (float) p.y < cy ? 1 : -1, items->size());
                 else popupChoice (id, *items, area);
             },
             [this, id, items] (int d) { cycleChoice (id, d, items->size()); }, true);
    }

    add ({ 1019, 759, 146, 41 }, "Hold: mantiene los picos del espectro hasta Reset", [this] (Point<int>) { proc.setFlag ("specHoldOn", ! proc.flag ("specHoldOn")); }, {}, true);
    add ({ 1019, 809, 146, 43 }, "Reset: reinicia los picos del espectro", [this] (Point<int>) { resetSpectrumPeaks(); }, {}, true);
    add ({ 1566, 759, 144, 41 }, "Hold: mantiene los picos de L / R hasta Reset", [this] (Point<int>) { proc.setFlag ("levelHoldOn", ! proc.flag ("levelHoldOn")); }, {}, true);
    add ({ 1566, 809, 144, 43 }, "Reset: reinicia los picos de L / R", [this] (Point<int>) { resetLevelPeaks(); }, {}, true);
}

void AnalyzerEditor::mouseDown (const MouseEvent& e)
{
    const float s = scale();
    const Point<int> p (roundToInt (e.position.x / s), roundToInt (e.position.y / s));
    const bool compact = proc.compactUi.load();
    for (auto it = hits.rbegin(); it != hits.rend(); ++it)
        if (it->r.contains (p) && ! (compact && it->controlsPanel))
        {
            if (it->click) it->click (p);
            repaint();
            return;
        }
}

void AnalyzerEditor::mouseMove (const MouseEvent& e)
{
    const float s = scale();
    const Point<int> p (roundToInt (e.position.x / s), roundToInt (e.position.y / s));
    const bool compact = proc.compactUi.load();
    for (auto it = hits.rbegin(); it != hits.rend(); ++it)
        if (it->r.contains (p) && ! (compact && it->controlsPanel)) { setTooltip (it->tip); return; }
    setTooltip ({});
}

void AnalyzerEditor::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    const float s = scale();
    const Point<int> p (roundToInt (e.position.x / s), roundToInt (e.position.y / s));
    const bool compact = proc.compactUi.load();
    const int dir = w.deltaY > 0.f ? 1 : (w.deltaY < 0.f ? -1 : 0);
    if (dir == 0) return;
    for (auto it = hits.rbegin(); it != hits.rend(); ++it)
        if (it->r.contains (p) && it->wheel && ! (compact && it->controlsPanel)) { it->wheel (dir); repaint(); return; }
}

//==============================================================================================
void AnalyzerEditor::ensureSpectrum()
{
    const double sr = proc.getCurrentSampleRate();
    int order = 12 + proc.choice ("fft");
    if (sr >= 88000.0) ++order;
    if (sr >= 176000.0) ++order;
    order = jlimit (11, 16, order);
    const int nb = anopt::bandCounts[proc.choice ("bands")];
    if (! exactlyEqual (sr, specSr) || order != specOrder || nb != specBands)
    {
        specSr = sr; specOrder = order; specBands = nb;
        spec.prepare (sr, order, nb);
    }
}

void AnalyzerEditor::step (double dt)
{
    ensureSpectrum();
    spec.update (proc.ringL(), proc.ringR(), proc.choice ("detection"), proc.choice ("mode"), dt,
                 anopt::rateValues[proc.choice ("specRate")], anopt::holdSeconds[proc.choice ("specHold")], proc.flag ("specHoldOn"));

    const float rate = anopt::rateValues[proc.choice ("levelRate")];
    const float fall = (float) (rate * dt);
    const double holdSec = anopt::holdSeconds[proc.choice ("levelHold")];
    const bool latched = proc.flag ("levelHoldOn");
    auto& lv = proc.level();
    for (int c = 0; c < 2; ++c)
    {
        const float pk = lv.takePeak (c);
        const float pkDb = pk > 1e-10f ? 20.f * std::log10 (pk) : an::kFloorDb;
        const float rmsDb = an::powerToDb ((double) lv.meanSquare (c));
        auto& s = ch[c];
        s.barPeak = std::max (pkDb, s.barPeak - fall);
        s.rms = std::max (rmsDb, s.rms - fall);
        if (latched)               { if (pkDb > s.hold) s.hold = pkDb; }
        else if (pkDb >= s.hold)   { s.hold = pkDb; s.holdLeft = (float) holdSec; }
        else if (s.holdLeft > 0.f) s.holdLeft -= (float) dt;
        else                       s.hold = std::max (pkDb, s.hold - fall);
        s.barPeak = std::max (s.barPeak, an::kFloorDb);
        s.rms = std::max (s.rms, an::kFloorDb);
        s.hold = std::max (s.hold, an::kFloorDb);
    }

    // las cifras se refrescan a 10 Hz para que se puedan leer
    lastTextTime += dt;
    if (lastTextTime >= 0.1)
    {
        lastTextTime = 0.0;
        shownRms[0] = ch[0].rms; shownRms[1] = ch[1].rms;
        lufsI = proc.loudness().integrated();
        lufsS = proc.loudness().shortTerm();
        lra = proc.loudness().range();
    }
}

void AnalyzerEditor::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes();
    const double dt = jlimit (0.001, 0.25, (now - lastTime) / 1000.0);
    lastTime = now;
    step (dt);

    int h = 0;
    for (const char* id : { "detection", "mode", "bands", "specHold", "specRate", "levelMode", "levelHold", "levelRate", "top", "range", "fft" })
        h = h * 31 + proc.choice (id) + 1;
    h = h * 31 + (proc.flag ("specHoldOn") ? 7 : 3) + (proc.flag ("levelHoldOn") ? 13 : 5);
    if (h != ctrlHash) { ctrlHash = h; repaint(); return; }

    const float s = scale();
    repaint (Rectangle<int> (0, 0, getWidth(), roundToInt (655.f * s)));
}

std::unique_ptr<te::Plugin::EditorComponent> injfx::InjAnalyzer::createEditor() { return std::make_unique<AnalyzerEditor> (*this); }
