#pragma once
#include "Theme.h"
#include <tracktion_engine/tracktion_engine.h>

// Bloc de letra y notas del proyecto: el texto se guarda dentro del proyecto (edit.state, propiedad injNotes).
namespace projectnotes
{
    namespace te = tracktion;
    inline juce::String get (te::Edit& e) { return e.state.getProperty ("injNotes", "").toString(); }
    inline void set (te::Edit& e, const juce::String& t) { if (t != get (e)) e.state.setProperty ("injNotes", t, nullptr); }

    class Content : public juce::Component, private juce::TextEditor::Listener
    {
    public:
        explicit Content (te::Edit& e) : edit (e)
        {
            ed.setMultiLine (true, true); ed.setReturnKeyStartsNewLine (true); ed.setFont (theme::fonts::ui (16.f)); ed.setText (get (e), false);
            ed.setColour (juce::TextEditor::backgroundColourId, theme::bgBar); ed.setColour (juce::TextEditor::textColourId, theme::text); ed.setColour (juce::TextEditor::outlineColourId, theme::boxLine);
            ed.setColour (juce::TextEditor::focusedOutlineColourId, theme::accent); ed.setColour (juce::CaretComponent::caretColourId, theme::accentHi); ed.setColour (juce::TextEditor::highlightColourId, theme::accentDim);
            ed.setIndents (10, 8); ed.addListener (this); addAndMakeVisible (ed); setSize (560, 460);
        }
        void resized() override { ed.setBounds (getLocalBounds().reduced (10)); }
    private:
        te::Edit& edit; juce::TextEditor ed;
        void textEditorTextChanged (juce::TextEditor&) override { set (edit, ed.getText()); }
    };
}
