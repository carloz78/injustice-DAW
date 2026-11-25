#pragma once
#include "Theme.h"
#include <functional>
#include <vector>

// Paleta de comandos (Ctrl+K): escribe parte del nombre y Enter lo ejecuta. Sirve para encontrar cualquier función sin buscar en menús.
struct Cmd { juce::String id, name, cat, keys; std::function<void()> run; };

class Palette : public juce::Component, private juce::TextEditor::Listener
{
public:
    std::function<void()> onClose;

    Palette()
    {
        setInterceptsMouseClicks (true, true); setWantsKeyboardFocus (true);
        ed.pre = [this] (const juce::KeyPress& k) { return handleKey (k); };
        ed.setFont (theme::fonts::ui (16.f)); ed.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        ed.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack); ed.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        ed.setColour (juce::TextEditor::textColourId, theme::text); ed.setColour (juce::CaretComponent::caretColourId, theme::accentHi);
        ed.setColour (juce::TextEditor::highlightColourId, theme::accentDim); ed.setIndents (0, 0); ed.setBorder ({ 0, 0, 0, 0 });
                ed.addListener (this); addAndMakeVisible (ed); setVisible (false);
    }
    void open (const std::vector<Cmd>* c)
    {
        cmds = c; ed.clear(); refilter(); setVisible (true); toFront (true); ed.grabKeyboardFocus(); repaint();
    }
    void close() { if (! isVisible()) return; setVisible (false); if (onClose) onClose(); }
    int numShown() const { return (int) shown.size(); }
    juce::String shownId (int i) const { return i >= 0 && i < (int) shown.size() ? (*cmds)[(size_t) shown[(size_t) i]].id : juce::String(); }
    void setQuery (const juce::String& q) { ed.setText (q, false); refilter(); }

    // sin acentos y en minúsculas, para que "transposicion" encuentre "Transposición"
    static juce::String fold (const juce::String& s)
    {
        juce::String o; for (auto ch : s.toLowerCase())
        {
            switch (ch) { case 0xe1: case 0xe0: case 0xe4: ch = 'a'; break; case 0xe9: case 0xe8: case 0xeb: ch = 'e'; break; case 0xed: case 0xec: case 0xef: ch = 'i'; break;
                          case 0xf3: case 0xf2: case 0xf6: ch = 'o'; break; case 0xfa: case 0xf9: case 0xfc: ch = 'u'; break; default: break; }
            o += ch;
        }
        return o;
    }

    void paint (juce::Graphics& g) override
    {
        using namespace theme;
        g.fillAll (juce::Colours::black.withAlpha (.28f));
        auto p = panelR().toFloat(); g.setColour (juce::Colours::black.withAlpha (.35f)); g.fillRoundedRectangle (p.translated (0, 3.f).expanded (1.f), 6.f);
        g.setColour (bgBar2); g.fillRoundedRectangle (p, 5.f); g.setColour (boxLine); g.drawRoundedRectangle (p.reduced (.5f), 5.f, 1.f);
        g.setColour (seam); g.fillRect (p.getX() + 1.f, p.getY() + kInput, p.getWidth() - 2.f, 1.f);
        { const float cx = p.getX() + 20.f, cy = p.getY() + 21.f; g.setColour (textDim); g.drawEllipse (cx - 5.f, cy - 5.f, 10.f, 10.f, 1.6f); g.drawLine (cx + 3.6f, cy + 3.6f, cx + 8.f, cy + 8.f, 1.8f); }
        if (ed.isEmpty()) { g.setColour (textFaint); g.setFont (fonts::ui (15.f)); g.drawText (juce::String::fromUTF8 ("Escribe un comando…  por ejemplo: dividir, tempo, transponer"), ed.getBounds().translated (3, 0), juce::Justification::centredLeft); }
        if (shown.empty()) { g.setColour (textFaint); g.setFont (fonts::ui (13.f)); g.drawText (juce::String::fromUTF8 ("No hay comandos con ese nombre"), (int) p.getX() + 16, (int) p.getY() + (int) kInput + 10, (int) p.getWidth() - 32, 22, juce::Justification::centredLeft); return; }
        g.saveState(); g.reduceClipRegion (p.toNearestInt().withTrimmedTop ((int) kInput + 1));
        for (int i = 0; i < kRows; ++i)
        {
            const int k = top + i; if (k >= (int) shown.size()) break; const auto& c = (*cmds)[(size_t) shown[(size_t) k]];
            auto r = juce::Rectangle<float> (p.getX() + 4.f, p.getY() + kInput + 4.f + (float) i * kRowH, p.getWidth() - 8.f, kRowH);
            if (k == sel) { g.setColour (accent.withAlpha (.16f)); g.fillRoundedRectangle (r, rad); g.setColour (accent.withAlpha (.55f)); g.drawRoundedRectangle (r.reduced (.5f), rad, 1.f); }
            else if (k == hot) { g.setColour (panelHi.withAlpha (.7f)); g.fillRoundedRectangle (r, rad); }
            g.setColour (k == sel ? text : text.withAlpha (.88f)); g.setFont (fonts::ui (13.5f)); g.drawText (c.name, r.toNearestInt().withTrimmedLeft (10).withTrimmedRight (170), juce::Justification::centredLeft, true);
            g.setColour (textFaint); g.setFont (fonts::label (10.5f)); g.drawText (c.cat, r.toNearestInt().withTrimmedRight (10).withLeft ((int) r.getRight() - 170).withWidth (c.keys.isEmpty() ? 160 : 70), juce::Justification::centredRight, true);
            if (c.keys.isNotEmpty()) { g.setColour (textDim); g.setFont (fonts::digits (11.f)); g.drawText (c.keys, r.toNearestInt().withTrimmedRight (10).withLeft ((int) r.getRight() - 100), juce::Justification::centredRight, true); }
        }
        g.restoreState();
    }
    void resized() override { auto p = panelR(); ed.setBounds (p.getX() + 44, p.getY() + 8, p.getWidth() - 60, 28); }
    void mouseMove (const juce::MouseEvent& e) override { const int h = rowAt (e.position); if (h != hot) { hot = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { if (hot != -1) { hot = -1; repaint(); } }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! panelR().contains (e.getPosition())) { close(); return; }
        const int k = rowAt (e.position); if (k >= 0) { sel = k; run(); }
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override { scrollTo (top + (w.deltaY > 0 ? -1 : 1) * 2); }
    void visibilityChanged() override { if (isVisible()) ed.grabKeyboardFocus(); }
    bool keyPressed (const juce::KeyPress& k) override { return handleKey (k); }

    void run()
    {
        if (sel < 0 || sel >= (int) shown.size()) return; auto fn = (*cmds)[(size_t) shown[(size_t) sel]].run;
        close(); if (fn) juce::MessageManager::callAsync (fn);
    }
    bool handleKey (const juce::KeyPress& k)
    {
        if (k == juce::KeyPress::escapeKey) { close(); return true; }
        if (k == juce::KeyPress::returnKey) { run(); return true; }
        if (k == juce::KeyPress::downKey) { move (1); return true; }
        if (k == juce::KeyPress::upKey) { move (-1); return true; }
        if (k == juce::KeyPress::pageDownKey) { move (kRows); return true; }
        if (k == juce::KeyPress::pageUpKey) { move (-kRows); return true; }
        return false;
    }
    void move (int d) { if (shown.empty()) return; sel = juce::jlimit (0, (int) shown.size() - 1, sel + d); scrollTo (sel < top ? sel : (sel >= top + kRows ? sel - kRows + 1 : top)); repaint(); }

private:
    static constexpr float kInput = 44.f, kRowH = 30.f; static constexpr int kRows = 10;
    struct Ed : juce::TextEditor { std::function<bool (const juce::KeyPress&)> pre; bool keyPressed (const juce::KeyPress& k) override { if (pre && pre (k)) return true; return juce::TextEditor::keyPressed (k); } };
    Ed ed; const std::vector<Cmd>* cmds = nullptr; std::vector<int> shown; int sel = 0, top = 0, hot = -1;

    juce::Rectangle<int> panelR() const { const int w = std::min (620, getWidth() - 40); const int rows = std::max (1, std::min (kRows, (int) shown.size())); const int h = (int) kInput + 8 + (int) (rows * kRowH) + (shown.empty() ? 12 : 0); return { (getWidth() - w) / 2, 64, w, h }; }
    int rowAt (juce::Point<float> p) const { auto r = panelR().toFloat(); if (! r.contains (p) || p.y < r.getY() + kInput + 4.f) return -1; const int i = (int) ((p.y - r.getY() - kInput - 4.f) / kRowH); const int k = top + i; return i < kRows && k < (int) shown.size() ? k : -1; }
    void scrollTo (int t) { top = juce::jlimit (0, std::max (0, (int) shown.size() - kRows), t); repaint(); }
    void textEditorTextChanged (juce::TextEditor&) override { refilter(); }
    void refilter()
    {
        shown.clear(); if (cmds == nullptr) return; juce::StringArray words; words.addTokens (fold (ed.getText()), " ", ""); words.removeEmptyStrings();
        for (int i = 0; i < (int) cmds->size(); ++i) { const auto hay = fold ((*cmds)[(size_t) i].name + " " + (*cmds)[(size_t) i].cat); bool ok = true; for (auto& w : words) if (! hay.contains (w)) { ok = false; break; } if (ok) shown.push_back (i); }
        if (! words.isEmpty()) std::stable_sort (shown.begin(), shown.end(), [&] (int a, int b) { return fold ((*cmds)[(size_t) a].name).startsWith (words[0]) && ! fold ((*cmds)[(size_t) b].name).startsWith (words[0]); });
        sel = 0; top = 0; setBounds (getBounds()); resized(); repaint();
    }
};
