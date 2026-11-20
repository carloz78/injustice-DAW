#pragma once
#include "Icons.h"
#include <functional>
#include <set>

// Browser lateral: categorías y árbol real de carpetas de samples con vista previa de forma de onda.
// Vocabulario propio (no copia el de otros DAW). Las categorías aún sin función se muestran atenuadas.
class BrowserPanel : public juce::Component
{
public:
    std::function<void()> onClose, onCollapse;
    juce::File root;

    BrowserPanel() : thumbCache (8)
    {
        fm.registerBasicFormats();
        rootFile = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("INJUSTICE DAW").getChildFile ("samples_root.txt");
        if (rootFile.existsAsFile()) root = juce::File (rootFile.loadFileAsString().trim());
        if (! root.isDirectory())
        {   // carpeta propia por defecto: el usuario solo tiene que copiar sus samples aquí
            root = defaultRoot();
            for (auto* sub : { "Kicks", "Snares", "Claps", "Hats", "Percs", "FX", "Loops", "Vocales" }) root.getChildFile (sub).createDirectory();
        }
        addAndMakeVisible (search); search.setTextToShowWhenEmpty (juce::String::fromUTF8 ("Buscar samples…"), textDimC()); search.onTextChange = [this] { scroll = 0; rebuild(); };
        rebuild();
    }
    static juce::File defaultRoot() { return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("INJUSTICE DAW").getChildFile ("Samples"); }
    static juce::Colour textDimC() { return juce::Colour (0xff9c9bad); }
    void resized() override { search.setBounds (10, kHead + 2, getWidth() - 20, 30); }
    void setRoot (const juce::File& f) { root = f; expanded.clear(); selected = juce::File(); scroll = 0; rootFile.getParentDirectory().createDirectory(); rootFile.replaceWithText (f.getFullPathName()); rebuild(); }
    void expandAll (int maxDepth) { std::function<void (const juce::File&, int)> rec = [&] (const juce::File& d, int dep) { if (dep > maxDepth) return; for (auto& s : d.findChildFiles (juce::File::findDirectories, false)) { expanded.insert (s.getFullPathName()); rec (s, dep + 1); } }; expanded.insert (root.getFullPathName()); rec (root, 0); rebuild(); }
    void setSearch (const juce::String& t) { search.setText (t, true); }
    int numShown() const { return (int) items.size(); }
    void selectFile (const juce::File& f) { selected = f; thumb.reset(); repaint(); }

    static constexpr int kTools = 72, kHead = 56, kCatRow = 34, kRow = 30, kPrev = 118;
    static bool isAudio (const juce::File& f) { return f.hasFileExtension ("wav;aif;aiff;flac;mp3;ogg"); }

    void paint (juce::Graphics& g) override
    {
        using namespace theme;
        auto r = getLocalBounds().toFloat().reduced (1);
        g.setColour (juce::Colour (0xff17161b)); g.fillRoundedRectangle (r, 12.f); g.setColour (juce::Colour (0xff2c2a33)); g.drawRoundedRectangle (r, 12.f, 1.5f);
        icons::draw (g, icons::OpenFile, { 12.f, 8.f, 40.f, 40.f }, text);
        g.setColour (text); g.setFont (juce::FontOptions (19.f, juce::Font::bold)); g.drawText ("Browser", 56, 0, 160, kHead, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff222027)); g.fillRoundedRectangle (collapseR(), 6.f); g.fillRoundedRectangle (closeR(), 6.f);
        g.setColour (text); drawChevrons (g, collapseR()); drawX (g, closeR());

        for (int b = 0; b < 2; ++b) { auto r = toolR (b); g.setColour (r.contains (hover) ? panelHi : juce::Colour (0xff222027)); g.fillRoundedRectangle (r, 6.f); g.setColour (text); g.setFont (juce::FontOptions (14.f)); g.drawText (b == 0 ? juce::String::fromUTF8 ("Elegir carpeta…") : juce::String ("Abrir carpeta"), r.toNearestInt(), juce::Justification::centred); }
        g.saveState(); g.reduceClipRegion (6, kHead + kTools, getWidth() - 12, getHeight() - kHead - kTools - kPrev);
        float y = (float) kHead + kTools + 6 - (float) scroll;
        if (! searching()) for (auto cat : { "Proyecto actual", "Archivos recientes", "Plugins instalados", "Presets de plugin", "Presets de canal", "Presets de mezcla" })
        { g.setColour (textDim.withAlpha (.55f)); g.setFont (juce::FontOptions (16.f)); g.drawText (cat, 40, (int) y, getWidth() - 50, kCatRow, juce::Justification::centredLeft);
          g.fillEllipse (18.f, y + kCatRow * .5f - 4, 8.f, 8.f); y += kCatRow; }
        if (! searching()) y += 4;
        for (size_t i = 0; i < items.size(); ++i, y += kRow)
        {
            auto& it = items[i]; if (y + kRow < kHead + kTools || y > getHeight()) continue;
            const bool sel = it.file == selected;
            if (sel) { g.setColour (juce::Colour (0xff3b2a5e)); g.fillRoundedRectangle (juce::Rectangle<float> (8, y, (float) getWidth() - 16, (float) kRow), 5.f); }
            const float x = 14.f + it.depth * 18.f; g.setColour (text); g.setFont (juce::FontOptions (16.f));
            if (it.isDir) { icons::draw (g, expanded.count (it.file.getFullPathName()) ? icons::Chevron : icons::Chevron, { x, y, 24.f, (float) kRow }, textDim); icons::draw (g, icons::OpenFile, { x + 22, y, 26.f, (float) kRow }, accent.brighter (.2f)); }
            else icons::draw (g, icons::Audio, { x + 22, y, 26.f, (float) kRow }, accentHi);
            g.setColour (it.isDir ? text : text.withAlpha (.92f)); g.drawText (it.isRoot ? "Muestras" : (searching() ? it.file.getParentDirectory().getFileName() + "  /  " + it.file.getFileName() : it.file.getFileName()), (int) x + 54, (int) y, getWidth() - (int) x - 62, kRow, juce::Justification::centredLeft);
        }
        if (items.size() <= 1 && ! searching()) { g.setColour (textDim); g.setFont (juce::FontOptions (14.f)); g.drawText (juce::String::fromUTF8 ("Aquí no hay audio todavía. Copia tus samples a la carpeta de abajo o elige otra con «Elegir carpeta»."), 16, (int) y, getWidth() - 32, 40, juce::Justification::centredLeft); }
        g.restoreState();
        drawPreview (g);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragFile = juce::File(); dragged = false;
        if (closeR().contains (e.position)) { if (onClose) onClose(); return; }
        if (collapseR().contains (e.position)) { if (onCollapse) onCollapse(); return; }
        if (toolR (0).contains (e.position)) { chooseRoot(); return; }
        if (toolR (1).contains (e.position)) { root.createDirectory(); root.revealToUser(); return; }
        const int idx = itemAt (e.y); if (idx < 0) return; auto& it = items[(size_t) idx];
        if (e.mods.isPopupMenu() && it.isRoot) { chooseRoot(); return; }
        if (it.isDir) { auto k = it.file.getFullPathName(); if (expanded.count (k)) expanded.erase (k); else expanded.insert (k); rebuild(); }
        else { selectFile (it.file); dragFile = it.file; }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragFile.existsAsFile() && ! dragged && e.getDistanceFromDragStart() > 8) { dragged = true; juce::DragAndDropContainer::performExternalDragDropOfFiles ({ dragFile.getFullPathName() }, false); }
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override { const int idx = itemAt (e.y); if (idx >= 0 && items[(size_t) idx].isRoot) chooseRoot(); }
    void mouseMove (const juce::MouseEvent& e) override { hover = e.position; repaint (0, kHead + 36, getWidth(), 36); }
    void mouseExit (const juce::MouseEvent&) override { hover = { -1, -1 }; repaint(); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        const int content = (int) items.size() * kRow + (searching() ? 0 : 6 * kCatRow) + 10, view = getHeight() - kHead - kTools - kPrev;
        scroll = juce::jlimit (0, juce::jmax (0, content - view), scroll - (int) (w.deltaY * 400)); repaint();
    }

private:
    struct Item { juce::File file; int depth = 0; bool isDir = false, isRoot = false; };
    std::vector<Item> items; std::set<juce::String> expanded; juce::File selected, dragFile, rootFile; int scroll = 0; bool dragged = false;
    juce::AudioFormatManager fm; juce::AudioThumbnailCache thumbCache; std::unique_ptr<juce::AudioThumbnail> thumb; std::unique_ptr<juce::FileChooser> chooser; juce::TextEditor search; juce::Point<float> hover { -1, -1 };
    bool searching() const { return search.getText().trim().isNotEmpty(); }
    juce::Rectangle<float> toolR (int b) const { const float w = ((float) getWidth() - 28.f) * .5f; return { 10.f + b * (w + 8.f), (float) kHead + 36.f, w, 30.f }; }

    juce::Rectangle<float> closeR() const { return { (float) getWidth() - 52.f, 10.f, 36.f, 36.f }; }
    juce::Rectangle<float> collapseR() const { return { (float) getWidth() - 94.f, 10.f, 36.f, 36.f }; }
    static void drawX (juce::Graphics& g, juce::Rectangle<float> r) { auto c = r.getCentre(); g.drawLine (c.x - 6, c.y - 6, c.x + 6, c.y + 6, 1.8f); g.drawLine (c.x + 6, c.y - 6, c.x - 6, c.y + 6, 1.8f); }
    static void drawChevrons (juce::Graphics& g, juce::Rectangle<float> r) { auto c = r.getCentre(); for (float o : { -3.f, 4.f }) { juce::Path p; p.startNewSubPath (c.x + o + 3, c.y - 6); p.lineTo (c.x + o - 3, c.y); p.lineTo (c.x + o + 3, c.y + 6); g.strokePath (p, juce::PathStrokeType (1.8f)); } }
    int itemAt (int y) const { float yy = (float) kHead + kTools + 6 - (float) scroll + (searching() ? 0 : 6 * kCatRow + 4); if (y < kHead + kTools || y > getHeight() - kPrev) return -1; const int i = (int) std::floor ((y - yy) / kRow); return (i >= 0 && i < (int) items.size()) ? i : -1; }

    void rebuild()
    {
        items.clear(); if (! root.isDirectory()) { repaint(); return; }
        if (searching())
        {
            const auto q = search.getText().trim(); juce::Array<juce::File> all = root.findChildFiles (juce::File::findFiles, true, "*");
            struct Cmp { static int compareElements (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()); } } cmp; all.sort (cmp);
            for (auto& f : all) if (isAudio (f) && f.getFileName().containsIgnoreCase (q) && items.size() < 400) items.push_back ({ f, 1, false, false });
            repaint(); return;
        }
        items.push_back ({ root, 0, true, true }); expanded.insert (root.getFullPathName());
        std::function<void (const juce::File&, int)> rec = [&] (const juce::File& d, int dep)
        {
            juce::Array<juce::File> subs = d.findChildFiles (juce::File::findDirectories, false); subs.sort();
            for (auto& s : subs) { if (s.getFileName().startsWith (".")) continue; items.push_back ({ s, dep, true, false }); if (expanded.count (s.getFullPathName())) rec (s, dep + 1); }
            juce::Array<juce::File> fl = d.findChildFiles (juce::File::findFiles, false); fl.sort();
            for (auto& f : fl) if (isAudio (f)) items.push_back ({ f, dep, false, false });
        };
        rec (root, 1); repaint();
    }
    void chooseRoot()
    {
        chooser = std::make_unique<juce::FileChooser> ("Carpeta de samples", root);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc) { if (fc.getResult().isDirectory()) setRoot (fc.getResult()); });
    }
    void drawPreview (juce::Graphics& g)
    {
        using namespace theme; auto b = juce::Rectangle<float> (8, (float) getHeight() - kPrev, (float) getWidth() - 16, kPrev - 8.f);
        g.setColour (juce::Colour (0xff111015)); g.fillRoundedRectangle (b, 8.f);
        if (! selected.existsAsFile()) { g.setColour (textDim); g.setFont (juce::FontOptions (13.f)); g.drawText ("Selecciona un sample para ver su forma de onda", b.toNearestInt(), juce::Justification::centred); return; }
        if (! thumb) { thumb = std::make_unique<juce::AudioThumbnail> (128, fm, thumbCache); thumb->setSource (new juce::FileInputSource (selected)); }
        g.setColour (accentHi); thumb->drawChannel (g, b.reduced (8, 8).withTrimmedBottom (22).toNearestInt(), 0.0, thumb->getTotalLength(), 0, 1.f);
        juce::String info; if (std::unique_ptr<juce::AudioFormatReader> rd { fm.createReaderFor (selected) }) info = juce::String (rd->sampleRate / 1000.0, 1) + " kHz  " + juce::String ((int) rd->bitsPerSample) + "-bit  " + selected.getFileExtension().substring (1).toUpperCase();
        g.setColour (text); g.setFont (juce::FontOptions (13.f)); g.drawText (info, b.toNearestInt().withTrimmedRight (12), juce::Justification::bottomRight);
    }
};
