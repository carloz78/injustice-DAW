#pragma once
#include <complex>
// Núcleo común de los efectos integrados de INJUSTICE DAW: piezas de DSP (filtros, retardos, suavizado,
// sobremuestreo), la base de plugin con parámetros automatizables y presets, y los controles de la ventana
// (perilla, botones de tipo, "Activo") con el estilo de los diseños aprobados.
#include <tracktion_engine/tracktion_engine.h>
#include <juce_dsp/juce_dsp.h>
#include "Theme.h"
#include <atomic>
#include <cmath>
#include <functional>
#include <vector>
namespace te = tracktion;

namespace injfx
{
constexpr float kPi = 3.14159265358979f;
inline float db2lin (float d) { return std::pow (10.f, d * .05f); }
inline float lin2db (float l) { return 20.f * std::log10 (std::max (l, 1e-9f)); }

//---- suavizado exponencial de un valor (sin "zipper noise" al mover perillas)
struct Smooth
{
    float cur = 0.f, a = 1.f; bool init = false;   // la primera llamada salta al valor pedido (sin barrido desde cero)
    void prep (double sr, float ms) { a = 1.f - std::exp (-1.f / (std::max (.01f, ms) * .001f * (float) sr)); init = false; }
    void snap (float v) { cur = v; init = true; }
    float next (float target) { if (! init) { cur = target; init = true; } cur += a * (target - cur); return cur; }
};
// suavizado por canal (evita el "zipper" al mover una perilla)
struct SmoothSt { Smooth s[2]; void prep (double sr, float ms) { for (auto& x : s) x.prep (sr, ms); } float next (int c, float t) { return s[c & 1].next (t); } };

//---- filtro de dos polos y dos ceros (cookbook de RBJ), estado en doble precisión
struct BQ
{
    enum Type { LP, HP, BP, Notch, Peak, LowShelf, HighShelf, AllPass };
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1[2] {}, z2[2] {};
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
    void set (Type t, double sr, double f, double q, double gdb = 0)
    {
        f = juce::jlimit (10.0, sr * .49, f); q = std::max (.05, q);
        const double w = 2 * juce::MathConstants<double>::pi * f / sr, c = std::cos (w), s = std::sin (w), al = s / (2 * q), A = std::pow (10.0, gdb / 40.0);
        double B0 = 1, B1 = 0, B2 = 0, A0 = 1, A1 = 0, A2 = 0;
        switch (t)
        {
            case LP:  B0 = (1 - c) / 2; B1 = 1 - c; B2 = B0; A0 = 1 + al; A1 = -2 * c; A2 = 1 - al; break;
            case HP:  B0 = (1 + c) / 2; B1 = -(1 + c); B2 = B0; A0 = 1 + al; A1 = -2 * c; A2 = 1 - al; break;
            case BP:  B0 = al; B1 = 0; B2 = -al; A0 = 1 + al; A1 = -2 * c; A2 = 1 - al; break;
            case Notch: B0 = 1; B1 = -2 * c; B2 = 1; A0 = 1 + al; A1 = -2 * c; A2 = 1 - al; break;
            case Peak: B0 = 1 + al * A; B1 = -2 * c; B2 = 1 - al * A; A0 = 1 + al / A; A1 = -2 * c; A2 = 1 - al / A; break;
            case AllPass: B0 = 1 - al; B1 = -2 * c; B2 = 1 + al; A0 = 1 + al; A1 = -2 * c; A2 = 1 - al; break;
            case LowShelf: { const double be = 2 * std::sqrt (A) * al;
                B0 = A * ((A + 1) - (A - 1) * c + be); B1 = 2 * A * ((A - 1) - (A + 1) * c); B2 = A * ((A + 1) - (A - 1) * c - be);
                A0 = (A + 1) + (A - 1) * c + be; A1 = -2 * ((A - 1) + (A + 1) * c); A2 = (A + 1) + (A - 1) * c - be; break; }
            case HighShelf: { const double be = 2 * std::sqrt (A) * al;
                B0 = A * ((A + 1) + (A - 1) * c + be); B1 = -2 * A * ((A - 1) + (A + 1) * c); B2 = A * ((A + 1) + (A - 1) * c - be);
                A0 = (A + 1) - (A - 1) * c + be; A1 = 2 * ((A - 1) - (A + 1) * c); A2 = (A + 1) - (A - 1) * c - be; break; }
        }
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    inline float run (int ch, float x) { const double y = b0 * x + z1[ch]; z1[ch] = b1 * x - a1 * y + z2[ch]; z2[ch] = b2 * x - a2 * y; return (float) y; }
    double magDb (double f, double sr) const
    {
        const double w = 2 * juce::MathConstants<double>::pi * f / sr; const std::complex<double> z1i = std::polar (1.0, -w), z2i = std::polar (1.0, -2 * w);
        const auto n = b0 + b1 * z1i + b2 * z2i, d = 1.0 + a1 * z1i + a2 * z2i; return 20 * std::log10 (std::max (1e-9, std::abs (n / d)));
    }
};

//---- línea de retardo con lectura fraccionaria (Hermite de 4 puntos)
struct Delay
{
    std::vector<float> b; int mask = 0, w = 0;
    void prep (int maxSamples) { int n = 8; while (n < maxSamples + 8) n <<= 1; b.assign ((size_t) n, 0.f); mask = n - 1; w = 0; }
    void clear() { std::fill (b.begin(), b.end(), 0.f); w = 0; }
    inline void push (float x) { b[(size_t) w] = x; w = (w + 1) & mask; }
    inline float at (int k) const { return b[(size_t) ((w - 1 - k) & mask)]; }
    // d = muestras hacia atrás (0 = la última que entró); mínimo 1 para la interpolación
    inline float read (float d) const
    {
        d = std::max (1.f, d); const int di = (int) d; const float f = d - (float) di;
        const float xm1 = at (di - 1), x0 = at (di), x1 = at (di + 1), x2 = at (di + 2);
        const float c1 = .5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.f * x1 - .5f * x2, c3 = .5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
};

//---- pasa-todo de primer orden (fases) y de retardo (difusión)
struct Allpass1 { float z = 0; inline float run (float x, float a) { const float y = a * x + z; z = x - a * y; return y; } };

//---- recorre un bloque en trozos de tamaño acotado
template <typename F> inline void forChunks (int total, int maxN, F&& f) { for (int s = 0; s < total; s += maxN) f (s, std::min (maxN, total - s)); }

//==============================================================================
struct Spec { const char* id; const char* name; float lo, hi, def; float mid; int fmt; const char* choices; };
enum Fmt { fPct, fDb, fHz, fMs, fSec, fRatio, fQ, fNum, fChoice, fKill, fVowel, fSemi, fNote, fBeats, fLufs, fRate, fInt, fOct, fCutLo, fCutHi };
struct Preset { const char* name; std::vector<float> v; };

inline juce::String fmtValue (const Spec& s, float v)
{
    switch (s.fmt)
    {
        case fPct:   return juce::String (juce::roundToInt (v * 100.f)) + " %";
        case fDb:    return (v > .049f ? "+" : "") + juce::String (v, 1) + " dB";
        case fHz:    return v >= 1000.f ? juce::String (v / 1000.f, v >= 10000.f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
        case fMs:    return v < 10.f ? juce::String (v, 1) + " ms" : juce::String (juce::roundToInt (v)) + " ms";
        case fSec:   return juce::String (v, 1) + " s";
        case fRatio: return juce::String (v, 1) + " : 1";
        case fQ:     return juce::String (v, 1);
        case fChoice: { auto n = juce::StringArray::fromTokens (juce::String::fromUTF8 (s.choices), "|", ""); return n[juce::jlimit (0, juce::jmax (0, n.size() - 1), juce::roundToInt (v))]; }
        case fKill:  return v <= s.lo + .5f ? juce::String ("Apagado") : (v > .049f ? "+" : "") + juce::String (v, 1) + " dB";
        case fVowel: { static const char* n[] { "A", "E", "I", "O", "U" }; return n[juce::jlimit (0, 4, juce::roundToInt (v))]; }
        case fSemi:  return (v > .05f ? "+" : "") + juce::String (v, 1) + " st";
        case fNote:  { static const char* n[] { "Do", "Do#", "Re", "Re#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" }; return n[((juce::roundToInt (v) % 12) + 12) % 12]; }
        case fCutLo: return v <= s.lo + 1.f ? juce::String ("Apagado") : juce::String (juce::roundToInt (v)) + " Hz";
        case fCutHi: return v >= s.hi - 100.f ? juce::String ("Apagado") : (v >= 1000.f ? juce::String (v / 1000.f, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz");
        case fRate:  return juce::String (v, v < 10.f ? 2 : 1) + " Hz";
        case fInt:   return juce::String (juce::roundToInt (v));
        case fOct:   return juce::String (v, 1) + " dB/oct";
        case fLufs:  return juce::String (v, 1) + " LUFS";
        case fBeats: return juce::String (v, 2) + " pulsos";
        default:     return juce::String (v, 2);
    }
}

//==============================================================================
// Base de los efectos: parámetros automatizables declarados con Spec, presets y medidores atómicos para la ventana.
class InjFx : public te::Plugin
{
public:
    InjFx (te::PluginCreationInfo info) : te::Plugin (info) {}
    ~InjFx() override { for (auto* p : params) p->detachFromCurrentValue(); params.clear(); cvs.clear(); notifyListenersOfDeletion(); }   // primero se sueltan los parámetros: notifyListenersOfDeletion() los borra
    juce::String getSelectableDescription() override { return getName(); }
    int getNumOutputChannelsGivenInputs (int n) override { return juce::jmin (n, 2); }
    void deinitialise() override {}
    virtual const Spec* specs() const = 0;
    virtual int numSpecs() const = 0;
    virtual std::vector<Preset> presets() const { return {}; }
    float v (int i) const { return params[(size_t) i]->getCurrentValue(); }
    int vi (int i) const { return juce::roundToInt (v (i)); }
    te::AutomatableParameter* param (int i) const { return params[(size_t) i]; }
    float defaultOf (int i) const { return specs()[i].def; }
    void setValue (int i, float x) { param (i)->setParameter (juce::jlimit (specs()[i].lo, specs()[i].hi, x), juce::sendNotificationSync); }
    juce::String presetName() const { return state.getProperty ("injPreset", "Inicial").toString(); }
    void applyPreset (int idx)
    {
        auto ps = presets(); if (idx < 0 || idx >= (int) ps.size()) return;
        for (size_t i = 0; i < ps[(size_t) idx].v.size() && (int) i < numSpecs(); ++i) setValue ((int) i, ps[(size_t) idx].v[i]);
        state.setProperty ("injPreset", juce::String::fromUTF8 (ps[(size_t) idx].name), getUndoManager());
    }
    void resetAll() { for (int i = 0; i < numSpecs(); ++i) setValue (i, specs()[i].def); state.setProperty ("injPreset", "Inicial", getUndoManager()); }
    // medidores para la ventana (se escriben desde el hilo de audio)
    std::atomic<float> meterA { 0.f }, meterB { 0.f }, meterC { 0.f };
    // ventana automática (los efectos nuevos): qué mostrar y cómo
    struct Layout { int pills = -1; std::vector<int> knobs; std::vector<std::vector<int>> rows; int visualH = 0; float dia = 56.f; int perRow = 5; const char* icon = "M3 12h18"; };
    virtual Layout layout() const { return {}; }
    virtual void paintVisual (juce::Graphics&, juce::Rectangle<float>) {}   // hilo de la interfaz
    virtual void visualClick() {}
    virtual void visualClickAt (juce::Point<float>, juce::Rectangle<float>) { visualClick(); }
    // tempo: posición en pulsos (negras) y BPM al inicio del bloque
    double beatAt (const te::PluginRenderContext& fc) const { return edit.tempoSequence.toBeats (fc.editTime.getStart()).inBeats(); }
    double bpmAt (const te::PluginRenderContext& fc) const { return std::max (30.0, edit.tempoSequence.getBpmAt (fc.editTime.getStart())); }
protected:
    void buildParams()
    {
        for (int i = 0; i < numSpecs(); ++i)
        {
            const Spec s = specs()[i]; juce::NormalisableRange<float> r (s.lo, s.hi); if (s.mid > s.lo && s.mid < s.hi) r.setSkewForCentre (s.mid);
            if (s.fmt == fChoice) r.interval = 1.f;
            auto* p = addParam (s.id, juce::String::fromUTF8 (s.name).upToFirstOccurrenceOf ("|", false, false), r, [s] (float x) { return fmtValue (s, x); }, [] (const juce::String& t) { return t.getFloatValue(); });
            auto cv = std::make_unique<juce::CachedValue<float>>(); cv->referTo (state, juce::Identifier (juce::String ("p_") + s.id), getUndoManager(), s.def);   // el valor vive en el estado del plugin: se guarda con el proyecto y se deshace
            p->attachToCurrentValue (*cv); cvs.push_back (std::move (cv)); params.push_back (p);
        }
    }
    std::vector<te::AutomatableParameter*> params; std::vector<std::unique_ptr<juce::CachedValue<float>>> cvs;
};

// Cuerpo común de cada efecto nuevo (nombre, tipo, parámetros y ventana automática)
#define INJ_FX_BODY(Cls, TYPE, NAME_UTF8, SHORT, N) \
    static inline const char* xmlTypeName = TYPE; static const char* getPluginName() { return TYPE; } \
    Cls (te::PluginCreationInfo i) : InjFx (i) { buildParams(); } \
    juce::String getName() const override { return juce::String::fromUTF8 (NAME_UTF8); } \
    juce::String getPluginType() override { return xmlTypeName; } juce::String getShortName (int) override { return SHORT; } \
    const Spec* specs() const override { return S; } int numSpecs() const override { return N; } \
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override { return std::make_unique<AutoEditor> (*this); }
// FFT de 2048 puntos (radix-2) y espectro en dB de un anillo de muestras (0 dB = senoide a plena escala, ventana Hann)
inline void fft2048 (std::complex<float>* x)
{
    constexpr int N = 2048; static const std::vector<std::complex<float>> T = [] { std::vector<std::complex<float>> v ((size_t) N / 2); for (int k = 0; k < N / 2; ++k) v[(size_t) k] = std::polar (1.f, (float) (-2.0 * 3.14159265358979 * k / N)); return v; }();
    for (int i = 1, j = 0; i < N; ++i) { int bit = N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (x[i], x[j]); }
    for (int len = 2; len <= N; len <<= 1) { const int st = N / len; for (int i = 0; i < N; i += len) for (int k = 0; k < len / 2; ++k) { const auto u = x[i + k], t = x[i + k + len / 2] * T[(size_t) (k * st)]; x[i + k] = u + t; x[i + k + len / 2] = u - t; } }
}
inline void ringSpectrum (const float* ring, int mask, int end, std::vector<float>& outDb)
{
    constexpr int N = 2048; static thread_local std::vector<std::complex<float>> x; x.assign ((size_t) N, {});
    for (int i = 0; i < N; ++i) x[(size_t) i] = { ring[(end - N + i) & mask] * (.5f - .5f * std::cos (2.f * 3.14159265f * (float) i / (float) N)), 0.f };
    fft2048 (x.data()); outDb.assign ((size_t) N / 2, -140.f); for (int k = 1; k < N / 2; ++k) outDb[(size_t) k] = 20.f * std::log10 (std::abs (x[(size_t) k]) / ((float) N / 4.f) + 1e-9f);
}
inline juce::String dbText (float db) { if (db < -99.f) return "-inf"; if (db > -.05f && db < .05f) return "0.0"; return (db > 0.f ? "+" : "") + juce::String (db, 1); }
inline juce::String grText (float reduction) { return dbText (-std::abs (reduction)); }
inline float coefMs (float ms, double sr) { return std::exp (-1.f / (std::max (.01f, ms) * .001f * (float) sr)); }

//==============================================================================
// Controles de la ventana (estilo de los diseños: tablero #26272b, perillas con arco naranja)
inline juce::Font uiFont (float h, bool bold = false) { return theme::fonts::ui (h, bold); }

class FxButton : public juce::Component
{
public:
    juce::String text; bool on = false, filled = false; std::function<void()> onClick; float fontH = 13.f; bool bold = true;
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat(); const bool hot = isMouseOver();
        if (on || filled) { g.setColour (theme::accent.withAlpha (hot ? .28f : .2f)); g.fillRoundedRectangle (r, 3.f); g.setColour (theme::accent); g.drawRoundedRectangle (r.reduced (.5f), 3.f, 1.f); g.setColour (theme::accentHi); }
        else { g.setColour (hot ? theme::panelHi : theme::panel); g.fillRoundedRectangle (r, 3.f); g.setColour (theme::seam); g.drawRoundedRectangle (r.reduced (.5f), 3.f, 1.f); g.setColour (theme::edgeLight); g.fillRect (r.getX() + 2.f, r.getY() + 1.f, r.getWidth() - 4.f, 1.f); g.setColour (theme::text); }
        g.setFont (uiFont (fontH, bold)); g.drawText (text, getLocalBounds(), juce::Justification::centred);
    }
    void mouseUp (const juce::MouseEvent& e) override { if (getLocalBounds().contains (e.getPosition()) && onClick) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
};

class FxKnob : public juce::Component, public juce::SettableTooltipClient
{
public:
    juce::String labelOverride;
    FxKnob (InjFx& f, int idx, float size = 56.f) : fx (f), i (idx), dia (size) { setMouseCursor (juce::MouseCursor::UpDownResizeCursor); }
    void paint (juce::Graphics& g) override
    {
        const auto& s = fx.specs()[i]; const float v = fx.v (i), n = juce::jlimit (0.f, 1.f, fx.param (i)->getCurrentNormalisedValue());
        const float cx = getWidth() * .5f, cy = dia * .5f, R = dia * .5f * .78f, sw = dia * .0785f, a0 = -kPi * .75f, a1 = a0 + kPi * 1.5f * n;
        const float aw = std::max (2.f, sw * .8f);
        juce::Path tr; tr.addCentredArc (cx, cy, R, R, 0.f, a0, kPi * .75f, true); g.setColour (theme::boxFill); g.strokePath (tr, juce::PathStrokeType (aw + 2.f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        g.setColour (theme::panelHi); g.strokePath (tr, juce::PathStrokeType (aw, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        if (n > .015f) { juce::Path va; va.addCentredArc (cx, cy, R, R, 0.f, a0, a1, true); g.setColour (theme::accent); g.strokePath (va, juce::PathStrokeType (aw, juce::PathStrokeType::curved, juce::PathStrokeType::butt)); }
        { const float br = R - aw * .5f - 3.f; g.setColour (theme::seam); g.fillEllipse (cx - br - 1.f, cy - br - .5f, (br + 1.f) * 2.f, (br + 1.f) * 2.f);
          g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3d45), cx, cy - br, juce::Colour (0xff25272d), cx, cy + br, false)); g.fillEllipse (cx - br, cy - br, br * 2.f, br * 2.f);
          g.setColour (theme::edgeLight.withAlpha (.12f)); g.drawEllipse (cx - br + .5f, cy - br + .5f, br * 2.f - 1.f, br * 2.f - 1.f, 1.f);
          g.setColour (theme::text); g.drawLine (cx + std::sin (a1) * br * .35f, cy - std::cos (a1) * br * .35f, cx + std::sin (a1) * br * .92f, cy - std::cos (a1) * br * .92f, std::max (1.6f, sw * .55f)); }
        const bool sm = dia < 48.f;
        g.setColour (theme::text); g.setFont (theme::fonts::digits (sm ? 10.5f : 12.f)); g.drawText (fmtValue (s, v), juce::Rectangle<int> (0, (int) dia + 2, getWidth(), sm ? 14 : 17), juce::Justification::centred);
        g.setColour (theme::textDim); g.setFont (uiFont (sm ? 10.f : 11.f)); juce::String nm = juce::String::fromUTF8 (s.name); if (nm.contains ("|")) nm = sm ? nm.fromLastOccurrenceOf ("|", false, false) : nm.upToFirstOccurrenceOf ("|", false, false);
        g.drawText (labelOverride.isNotEmpty() ? labelOverride : nm, juce::Rectangle<int> (0, (int) dia + (sm ? 16 : 20), getWidth(), 14), juce::Justification::centred);
    }
    void mouseDown (const juce::MouseEvent&) override { startN = fx.param (i)->getCurrentNormalisedValue(); fx.param (i)->parameterChangeGestureBegin(); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float k = e.mods.isShiftDown() ? 800.f : 220.f; setN (startN - (float) e.getDistanceFromDragStartY() / k);
    }
    void mouseUp (const juce::MouseEvent&) override { fx.param (i)->parameterChangeGestureEnd(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { fx.param (i)->parameterChangeGestureBegin(); fx.setValue (i, fx.defaultOf (i)); fx.param (i)->parameterChangeGestureEnd(); repaint(); }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        auto* p = fx.param (i); setN (p->getCurrentNormalisedValue() + w.deltaY * (e.mods.isShiftDown() ? .02f : .08f)); (void) p;
    }
private:
    void setN (float n)
    {
        n = juce::jlimit (0.f, 1.f, n); auto* p = fx.param (i); const auto r = p->valueRange; float v = r.convertFrom0to1 (n); v = r.snapToLegalValue (v);
        p->setParameter (v, juce::sendNotificationSync); repaint();
    }
    InjFx& fx; int i; float dia, startN = 0.f;
};

// Fila de botones para un parámetro de tipo "elección"
class FxPills : public juce::Component
{
public:
    FxPills (InjFx& f, int idx, int gap = 8) : fx (f), i (idx)
    {
        auto names = juce::StringArray::fromTokens (juce::String::fromUTF8 (fx.specs()[idx].choices), "|", "");
        for (int k = 0; k < names.size(); ++k)
        {
            auto* b = bs.add (new FxButton()); b->text = names[k]; addAndMakeVisible (b);
            b->onClick = [this, k] { fx.param (i)->parameterChangeGestureBegin(); fx.setValue (i, (float) k); fx.param (i)->parameterChangeGestureEnd(); refresh(); };
        }
        g_ = gap; refresh();
    }
    void refresh() { for (int k = 0; k < bs.size(); ++k) { const bool o = k == fx.vi (i); if (bs[k]->on != o) { bs[k]->on = o; bs[k]->repaint(); } } }
    void resized() override
    {
        const int n = bs.size(); if (n == 0) return; const int w = (getWidth() - g_ * (n - 1)) / n;
        for (int k = 0; k < n; ++k) bs[k]->setBounds (k * (w + g_), 0, k == n - 1 ? getWidth() - k * (w + g_) : w, getHeight());
    }
private:
    InjFx& fx; int i, g_ = 8; juce::OwnedArray<FxButton> bs;
};

//==============================================================================
// Ventana del efecto: cabecera (icono, título, preajustes, Activo) + cuerpo que dibuja cada efecto
class FxEditor : public te::Plugin::EditorComponent, private juce::Timer
{
public:
    FxEditor (InjFx& f, int w, int h, const char* iconSvg) : fx (f)
    {
        setSize (w, h); icon = juce::Drawable::parseSVGPath (juce::String (iconSvg));
        presetBtn.bold = true; presetBtn.fontH = 12.f; addAndMakeVisible (presetBtn); activeBtn.text = "Activo"; activeBtn.fontH = 12.f; addAndMakeVisible (activeBtn);
        presetBtn.onClick = [this] { showPresets(); };
        activeBtn.onClick = [this] { fx.setEnabled (! fx.isEnabled()); syncHeader(); };
        syncHeader(); startTimerHz (30);
    }
    ~FxEditor() override { stopTimer(); }
    bool allowWindowResizing() override { return false; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }
    void resized() override { presetBtn.setBounds (getWidth() - 14 - 60 - 10 - 200, 10, 200, 28); activeBtn.setBounds (getWidth() - 14 - 60, 10, 60, 28); }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (theme::bgBar2); g.setColour (theme::bgBar); g.fillRect (0, 0, getWidth(), 48); g.setColour (theme::seam); g.fillRect (0, 47, getWidth(), 1); g.setColour (theme::edgeLight); g.fillRect (0, 0, getWidth(), 1);
        g.setColour (theme::accent); g.fillRect (0, 0, 3, 48);
        if (! icon.isEmpty()) { g.setColour (theme::iconIdle); g.strokePath (icon, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::butt), juce::AffineTransform::scale (22.f / 24.f).translated (16.f, 13.f)); }
        g.setColour (theme::text); g.setFont (uiFont (15.f, true)); g.drawText (fx.getName(), 50, 0, getWidth() - 320, 48, juce::Justification::centredLeft);
        paintBody (g);
    }
protected:
    virtual void paintBody (juce::Graphics&) {}
    virtual void tick() {}
    template <typename T> T* own (T* c) { addAndMakeVisible (c); owned.add (c); return c; }
    FxKnob* knob (int idx, int x, int y, int w, float dia = 56.f) { auto* k = new FxKnob (fx, idx, dia); k->setBounds (x, y, w, (int) dia + 38); return own (k); }
    FxPills* pills (int idx, juce::Rectangle<int> r) { auto* p = new FxPills (fx, idx); p->setBounds (r); pillList.add (p); return own (p); }
    // fila de perillas repartidas en el ancho del cuerpo (margen 16)
    void knobRow (std::initializer_list<int> idxs, int y, float dia = 56.f)
    {
        const int n = (int) idxs.size(), cw = (getWidth() - 32) / n; int k = 0; for (int i : idxs) knob (i, 16 + k++ * cw, y, cw, dia);
    }
    InjFx& fx;
private:
    void timerCallback() override { syncHeader(); for (auto* p : pillList) p->refresh(); tick(); for (auto* c : owned) if (dynamic_cast<FxKnob*> (c)) c->repaint(); repaint (getLocalBounds().withTrimmedTop (48)); }
    void syncHeader()
    {
        const bool on = fx.isEnabled(); if (activeBtn.on != on) { activeBtn.on = on; activeBtn.repaint(); }
        const auto t = juce::String::fromUTF8 ("Preajuste: ") + fx.presetName(); if (presetBtn.text != t) { presetBtn.text = t; presetBtn.repaint(); }
    }
    void showPresets()
    {
        juce::PopupMenu m; auto ps = fx.presets(); m.addItem (1, "Inicial");
        if (! ps.empty()) m.addSeparator();
        for (size_t i = 0; i < ps.size(); ++i) m.addItem (100 + (int) i, juce::String::fromUTF8 (ps[i].name));
        juce::Component::SafePointer<FxEditor> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn), [sp] (int r) { if (sp == nullptr) return; if (r == 1) sp->fx.resetAll(); else if (r >= 100) sp->fx.applyPreset (r - 100); sp->syncHeader(); });
    }
    juce::Path icon; FxButton presetBtn, activeBtn; juce::OwnedArray<juce::Component> owned; juce::Array<FxPills*> pillList;
};

// Sobremuestreo x2 reutilizable para los efectos que distorsionan (sin aliasing). Compensa la latencia de la señal seca.
struct OS2
{
    static constexpr int kMax = 2048;
    juce::dsp::Oversampling<float> os { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };
    float lat = 0.f; int latInt = 0; Delay dry[2]; juce::AudioBuffer<float> sc;
    OS2() { os.initProcessing (kMax); lat = os.getLatencyInSamples(); latInt = (int) std::lround (lat); }
    void prep() { os.reset(); lat = os.getLatencyInSamples(); latInt = (int) std::lround (lat); for (auto& d : dry) { d.prep (latInt + kMax + 8); d.clear(); } sc.setSize (2, kMax); }
    bool ready() const { return sc.getNumSamples() >= kMax; }
    // pre(c,x): ganancia previa en la frecuencia normal; nl(c,x): no linealidad en 2x; post(c,y,seco): salida final
    template <typename Pre, typename NL, typename Post> void run (const te::PluginRenderContext& fc, Pre pre, NL nl, Post post)
    {
        const int nc = std::min (2, fc.destBuffer->getNumChannels());
        forChunks (fc.bufferNumSamples, kMax, [&] (int s0, int n)
        {
            float* io[2]; for (int c = 0; c < 2; ++c) io[c] = fc.destBuffer->getWritePointer (juce::jmin (c, nc - 1), fc.bufferStartSample + s0);
            for (int c = 0; c < 2; ++c) { float* w = sc.getWritePointer (c); for (int i = 0; i < n; ++i) { dry[c].push (io[c][i]); w[i] = pre (c, io[c][i]); } }
            juce::dsp::AudioBlock<float> blk (sc); auto sub = blk.getSubBlock (0, (size_t) n); auto up = os.processSamplesUp (sub);
            for (size_t c = 0; c < 2; ++c) { float* u = up.getChannelPointer (c); for (size_t i = 0; i < up.getNumSamples(); ++i) u[i] = nl ((int) c, u[i]); }
            os.processSamplesDown (sub);
            for (int c = 0; c < 2; ++c) { const float* w = sc.getReadPointer (c); for (int i = 0; i < n; ++i) { const float o = post (c, w[i], dry[c].at (latInt + n - 1 - i)); if (c < nc) io[c][i] = o; } }
        });
    }
};

inline void barH (juce::Graphics& g, juce::Rectangle<float> r, float v01, juce::Colour c) { g.setColour (theme::boxFill); g.fillRoundedRectangle (r, 1.5f); const float w = juce::jlimit (0.f, 1.f, v01) * r.getWidth(); if (w > 1.f) { g.setColour (c); g.fillRoundedRectangle (r.withWidth (std::max (w, 2.f)), 1.5f); } }
inline void barV (juce::Graphics& g, juce::Rectangle<float> r, float v01, juce::Colour c, bool fromTop = false) { g.setColour (theme::boxFill); g.fillRoundedRectangle (r, 2.f); const float h = juce::jlimit (0.f, 1.f, v01) * r.getHeight(); if (h > 1.f) { g.setColour (c); g.fillRoundedRectangle (fromTop ? r.withHeight (h) : r.withTop (r.getBottom() - h), 2.f); } }
inline void label (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Justification j = juce::Justification::centredLeft, float h = 11.f, bool bold = false, juce::Colour c = theme::textDim) { g.setColour (c); g.setFont (uiFont (h, bold)); g.drawText (t, r.toNearestInt(), j); }

// Caja oscura de gráficos (#1c1c1c, esquinas redondas) para dibujar curvas y medidores
inline void plotBox (juce::Graphics& g, juce::Rectangle<float> r) { g.setColour (theme::boxFill); g.fillRoundedRectangle (r, 3.f); g.setColour (theme::seam); g.drawRoundedRectangle (r.reduced (.5f), 3.f, 1.f); }


// Ventana automática para los efectos nuevos: tipo en botones, zona visual opcional y perillas en filas
class AutoEditor : public FxEditor
{
public:
    static int heightFor (const InjFx::Layout& L)
    {
        int y = 64; if (L.pills >= 0) y += 48; if (L.visualH > 0) y += L.visualH + 14;
        const int n = (int) L.knobs.size(), per = std::max (1, L.perRow), rows = L.rows.empty() ? (n + per - 1) / per : (int) L.rows.size(); return y + rows * ((int) L.dia + 46) + 10;
    }
    AutoEditor (InjFx& f) : FxEditor (f, 600, heightFor (f.layout()), f.layout().icon), L (f.layout())
    {
        int y = 64; if (L.pills >= 0) { pills (L.pills, { 16, y, 568, 34 }); y += 48; }
        if (L.visualH > 0) { vis = { 16.f, (float) y, 568.f, (float) L.visualH }; y += L.visualH + 14; }
        const int n = (int) L.knobs.size(), per = std::max (1, L.perRow);
        if (! L.rows.empty()) { int r = 0; for (auto& row : L.rows) { const int cw = 568 / (int) row.size(); int c = 0; for (int id : row) knob (id, 16 + c++ * cw, y + r * ((int) L.dia + 46), cw, L.dia); ++r; } }
        else for (int k = 0; k < n; ++k) { const int row = k / per, col = k % per, cw = 568 / per; knob (L.knobs[(size_t) k], 16 + col * cw, y + row * ((int) L.dia + 46), cw, L.dia); }
    }
protected:
    void paintBody (juce::Graphics& g) override
    {
        if (L.visualH <= 0) return; plotBox (g, vis); juce::Graphics::ScopedSaveState ss (g); juce::Path clip; clip.addRoundedRectangle (vis, 3.f); g.reduceClipRegion (clip); fx.paintVisual (g, vis);
    }
    void mouseDown (const juce::MouseEvent& e) override { if (L.visualH > 0 && vis.contains (e.position)) fx.visualClickAt (e.position, vis); }
private:
    InjFx::Layout L; juce::Rectangle<float> vis;
};

} // namespace injfx
