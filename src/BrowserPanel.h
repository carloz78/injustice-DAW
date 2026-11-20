#pragma once
#include "Icons.h"
#include <functional>
#include <set>
#include <map>
#include <vector>

// Browser lateral: categorías y árbol real de carpetas de samples con vista previa de forma de onda.
// Vocabulario propio (no copia el de otros DAW). 
class BrowserPanel : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    std::function<void()> onClose, onCollapse; std::function<void (const juce::String&)> onMessage;
    juce::File root;   // carpeta principal; además se pueden agregar otras carpetas (extras)

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
        extrasFile = rootFile.getSiblingFile ("samples_extras.txt");
        if (extrasFile.existsAsFile()) { juce::StringArray ls; ls.addLines (extrasFile.loadFileAsString()); for (auto& l : ls) if (l.trim().isNotEmpty() && juce::File (l.trim()).isDirectory()) extras.push_back (juce::File (l.trim())); }
        curDir = root; startTimerHz (20);
        addAndMakeVisible (search); search.setTextToShowWhenEmpty (juce::String::fromUTF8 ("Búsqueda de samples (todas tus carpetas)…"), textDimC()); search.onTextChange = [this] { scroll = 0; rebuild(); };
        rebuild();
    }
    ~BrowserPanel() override { stopTimer(); stopPreview(); if (dev && attached) dev->removeAudioCallback (&player); player.setSource (nullptr); }
    // ---- escuchar samples desde el Browser ----
    void setDeviceManager (juce::AudioDeviceManager* dm) { dev = dm; }
    bool isPreviewing() const { return transport.isPlaying(); }
    void playPreview (const juce::File& f, double fromFrac = 0.0)
    {
        stopPreview(); std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f)); if (! rd) return;
        const double sr = rd->sampleRate; const double len = (double) rd->lengthInSamples / sr;
        preSrc = std::make_unique<juce::AudioFormatReaderSource> (rd.release(), true);
        transport.setSource (preSrc.get(), 0, nullptr, sr);
        if (dev && ! attached) { player.setSource (&transport); dev->addAudioCallback (&player); attached = true; }
        transport.setGain (0.8f); transport.setPosition (juce::jlimit (0.0, juce::jmax (0.0, len - 0.01), fromFrac * len)); transport.start(); repaint();
    }
    void stopPreview() { transport.stop(); transport.setSource (nullptr); preSrc.reset(); }
    void removeExtraRoot (const juce::File& f) { for (size_t i = 0; i < extras.size(); ++i) if (extras[i] == f) { extras.erase (extras.begin() + (long) i); saveExtras(); rebuild(); return; } }
    int numRoots() const { return 1 + (int) extras.size(); }
    bool addExtraRoot (const juce::File& f)
    {
        if (! f.isDirectory() || f == root) return false; for (auto& e : extras) if (e == f) return false;
        extras.push_back (f); saveExtras(); rebuild(); return true;
    }
    static juce::File defaultRoot() { return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("INJUSTICE DAW").getChildFile ("Samples"); }
    static juce::Colour textDimC() { return juce::Colour (0xff9c9bad); }
    void resized() override { search.setBounds (10, kHead + 2, getWidth() - 20, 30); }
    juce::File getRoot() const { return root; }
    void reload() { rebuild(); }
    void setRoot (const juce::File& f) { root = f; curDir = f; expanded.clear(); selected = juce::File(); scroll = 0; rootFile.getParentDirectory().createDirectory(); rootFile.replaceWithText (f.getFullPathName()); rebuild(); }
    void expandAll (int maxDepth) { std::function<void (const juce::File&, int)> rec = [&] (const juce::File& d, int dep) { if (dep > maxDepth) return; for (auto& s : d.findChildFiles (juce::File::findDirectories, false)) { expanded.insert (s.getFullPathName()); rec (s, dep + 1); } }; expanded.insert (root.getFullPathName()); rec (root, 0); rebuild(); }
    void setCategories (bool on) { catMode = on; scroll = 0; rebuild(); }
    bool categoriesOn() const { return catMode; }
    int rootRows() const { int n = 0; for (auto& i : items) if (i.isRoot) ++n; return n; }
    int categoryCount (const juce::String& name) const { for (auto& i : items) if (i.label.isNotEmpty() && i.label.startsWith (name + "  (")) return i.label.fromLastOccurrenceOf ("  (", false, false).getIntValue(); return -1; }
    static const juce::StringArray& categoryNames() { static const juce::StringArray n { "Kicks", "Snares", "Claps", "Hats", "Toms", "Percusiones", "Bajos", "Sintes", "FX", "Voces", "Loops", "Otros" }; return n; }
    // Tipo de sample segun su nombre y el de sus carpetas (kick, snare, hat...).
    static juce::String categoryOf (const juce::File& f)
    {
        auto norm = [] (juce::String t) { t = t.toLowerCase(); juce::String o; for (auto c : t) o += juce::CharacterFunctions::isLetterOrDigit (c) ? juce::String::charToString (c) : juce::String (" "); return " " + o + " "; };
        const juce::String name = norm (f.getFileNameWithoutExtension()); juce::String dirs; auto d = f.getParentDirectory(); for (int i = 0; i < 3 && d.getFullPathName().length() > 3; ++i, d = d.getParentDirectory()) dirs += norm (d.getFileName()) + "|";
        struct Rule { const char* cat; std::vector<const char*> sub, word; };
        static const std::vector<Rule> rules {
            { "Loops", { "loop" }, {} }, { "Kicks", { "kick", "bombo", "kik" }, { "bd", "kck" } }, { "Snares", { "snare", "rimshot" }, { "snr", "sd" } },
            { "Claps", { "clap", "palma" }, { "clp" } }, { "Hats", { "hihat", "hi hat", "hat", "ohh", "chh" }, { "hh", "oh", "ch" } }, { "Toms", { "tom" }, {} },
            { "Percusiones", { "perc", "shaker", "conga", "bongo", "cowbell", "rim", "tambour", "clave" }, {} }, { "Bajos", { "bass", "808", "reese", "bajo" }, { "sub" } },
            { "FX", { "fx", "riser", "impact", "sweep", "uplift", "downlift", "crash", "noise", "explosion", "whoosh", "effect" }, {} },
            { "Voces", { "vocal", "vox", "voz", "acapella", "shout", "scream", "choir" }, {} }, { "Sintes", { "lead", "synth", "pad", "chord", "stab", "pluck", "arp", "sinte" }, {} } };
        for (const juce::String& hay : { name, dirs })
            for (auto& r : rules)
            {
                for (auto* k : r.sub) if (hay.contains (k)) return r.cat;
                for (auto* w : r.word) if (hay.contains (" " + juce::String (w) + " ")) return r.cat;
            }
        return "Otros";
    }
    void setSearch (const juce::String& t) { search.setText (t, false); scroll = 0; rebuild(); }
    int numShown() const { return (int) items.size(); }
    void selectFile (const juce::File& f) { selected = f; thumb.reset(); repaint(); }

    static constexpr int kTools = 130, kHead = 56, kCatRow = 34, kRow = 30, kPrev = 118;
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

        for (int b = 0; b < 4; ++b)
        {
            auto r = toolR (b); g.setColour (b == 0 ? accent : (r.contains (hover) ? panelHi : juce::Colour (0xff222027))); g.fillRoundedRectangle (r, 6.f); g.setColour (b == 0 ? juce::Colours::white : text); g.setFont (juce::FontOptions (14.f));
            g.drawText (b == 0 ? juce::String::fromUTF8 ("Importar samples") : b == 1 ? juce::String::fromUTF8 ("Nueva carpeta") : b == 2 ? juce::String::fromUTF8 ("Agregar carpeta…") : juce::String::fromUTF8 ("Abrir carpeta"), r.toNearestInt().reduced (6, 0), juce::Justification::centred, true);
        }
        if (searching()) { g.setColour (textDim); g.setFont (juce::FontOptions (13.f)); g.drawText (juce::String (found) + juce::String::fromUTF8 (found == 1 ? " sample encontrado en tus carpetas" : " samples encontrados en tus carpetas") + (found > 400 ? juce::String::fromUTF8 (" (se muestran 400)") : juce::String()), 12, kHead + 106, getWidth() - 24, 18, juce::Justification::centredLeft, true); }
        else { const juce::String dest = curDir == root ? juce::String::fromUTF8 ("Muestras (carpeta principal)") : curDir.getFileName(); g.setColour (textDim); g.setFont (juce::FontOptions (13.f));
          g.drawText (juce::String::fromUTF8 ("Se guardan en: ") + dest, 12, kHead + 106, getWidth() - 200, 18, juce::Justification::centredLeft, true); }
        { auto sg = segR(); g.setColour (juce::Colour (0xff222027)); g.fillRoundedRectangle (sg, 6.f); auto half = sg.withWidth (sg.getWidth() * .5f); if (catMode) half = half.translated (sg.getWidth() * .5f, 0);
          g.setColour (accent); g.fillRoundedRectangle (half.reduced (1.5f), 5.f); g.setFont (juce::FontOptions (12.f)); g.setColour (juce::Colours::white);
          g.drawText ("Carpetas", sg.withWidth (sg.getWidth() * .5f).toNearestInt(), juce::Justification::centred); g.drawText (juce::String::fromUTF8 ("Categorías"), sg.withTrimmedLeft (sg.getWidth() * .5f).toNearestInt(), juce::Justification::centred); }
        g.saveState(); g.reduceClipRegion (6, kHead + kTools, getWidth() - 12, getHeight() - kHead - kTools - kPrev);
        float y = (float) kHead + kTools + 6 - (float) scroll;
        for (size_t i = 0; i < items.size(); ++i, y += kRow)
        {
            auto& it = items[i]; if (y + kRow < kHead + kTools || y > getHeight()) continue;
            const bool sel = it.file == selected;
            if (it.isDir && it.file == curDir && ! searching()) { g.setColour (juce::Colour (0xff2c2640)); g.fillRoundedRectangle (juce::Rectangle<float> (8, y, (float) getWidth() - 16, (float) kRow), 5.f); g.setColour (accentHi); g.drawRoundedRectangle (juce::Rectangle<float> (8.5f, y + .5f, (float) getWidth() - 17, (float) kRow - 1), 5.f, 1.2f); }
            if (sel) { g.setColour (juce::Colour (0xff3b2a5e)); g.fillRoundedRectangle (juce::Rectangle<float> (8, y, (float) getWidth() - 16, (float) kRow), 5.f); }
            const float x = 14.f + it.depth * 18.f; g.setColour (text); g.setFont (juce::FontOptions (16.f));
            if (it.isDir) { const bool open = it.label.isNotEmpty() ? ! collapsedCats.count (it.label.upToLastOccurrenceOf ("  (", false, false)) : (it.isRoot ? ! collapsedRoots.count (it.file.getFullPathName()) : (searching() || expanded.count (it.file.getFullPathName()) > 0));
                g.saveState(); if (! open) g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, x + 12.f, y + kRow * .5f)); icons::draw (g, icons::Chevron, { x, y, 24.f, (float) kRow }, textDim); g.restoreState(); icons::draw (g, icons::OpenFile, { x + 22, y, 26.f, (float) kRow }, accent.brighter (.2f)); }
            else icons::draw (g, icons::Audio, { x + 22, y, 26.f, (float) kRow }, accentHi);
            g.setColour (it.isDir ? text : text.withAlpha (.92f)); g.drawText (it.label.isNotEmpty() ? it.label : it.isRoot ? (it.extra < 0 ? juce::String ("Muestras") : it.file.getFileName()) : (searching() ? it.file.getParentDirectory().getFileName() + "  /  " + it.file.getFileName() : it.file.getFileName()), (int) x + 54, (int) y, getWidth() - (int) x - 62, kRow, juce::Justification::centredLeft);
        }
        if (items.size() <= 1 && ! searching()) { g.setColour (textDim); g.setFont (juce::FontOptions (14.f)); g.drawFittedText (juce::String::fromUTF8 ("Aquí no hay audio todavía. Usa «Importar samples» o arrastra tus archivos aquí para guardarlos en tu biblioteca."), 16, (int) y, getWidth() - 32, 64, juce::Justification::topLeft, 4); }
        g.restoreState();
        drawPreview (g);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragFile = juce::File(); dragged = false;
        if (closeR().contains (e.position)) { if (onClose) onClose(); return; }
        if (collapseR().contains (e.position)) { if (onCollapse) onCollapse(); return; }
        if (toolR (0).contains (e.position)) { importDialog (curDir); return; }
        if (toolR (1).contains (e.position)) { newFolder (curDir); return; }
        if (toolR (2).contains (e.position)) { addFolderDialog(); return; }
        if (toolR (3).contains (e.position)) { root.createDirectory(); root.revealToUser(); return; }
        { auto pb = previewBox(); if (pb.contains (e.position) && selected.existsAsFile()) { if (transport.isPlaying() && e.mods.isShiftDown()) stopPreview(); else playPreview (selected, (e.position.x - pb.getX() - 8) / juce::jmax (1.f, pb.getWidth() - 16)); repaint(); return; } }
        { auto sg = segR(); if (sg.contains (e.position)) { setCategories (e.position.x > sg.getCentreX()); return; } }
        const int idx = itemAt (e.y); if (idx < 0) return; auto& it = items[(size_t) idx];
        if (e.mods.isPopupMenu()) { if (it.label.isEmpty()) itemMenu (it.file, it.isDir, it.isRoot, it.extra); return; }
        clickItem (idx);
    }
    // Clic en una fila: las carpetas y categorias se pliegan o se despliegan; un sample se selecciona y suena.
    void clickItem (int idx)
    {
        if (idx < 0 || idx >= (int) items.size()) return; auto& it = items[(size_t) idx];
        if (it.label.isNotEmpty()) { const auto k = it.label.upToLastOccurrenceOf ("  (", false, false); if (collapsedCats.count (k)) collapsedCats.erase (k); else collapsedCats.insert (k); rebuild(); }
        else if (it.isDir)
        {
            curDir = it.file; auto k = it.file.getFullPathName();
            if (it.isRoot) { if (collapsedRoots.count (k)) collapsedRoots.erase (k); else collapsedRoots.insert (k); }
            else if (expanded.count (k)) expanded.erase (k); else expanded.insert (k);
            rebuild();
        }
        else { selectFile (it.file); dragFile = it.file; playPreview (it.file); }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragFile.existsAsFile() && ! dragged && e.getDistanceFromDragStart() > 8) { dragged = true; juce::DragAndDropContainer::performExternalDragDropOfFiles ({ dragFile.getFullPathName() }, false); }
    }
    void mouseDoubleClick (const juce::MouseEvent&) override {}
    bool isInterestedInFileDrag (const juce::StringArray& f) override { return f.size() > 0; }
    void filesDropped (const juce::StringArray& f, int, int) override { juce::Array<juce::File> a; for (auto& s : f) a.add (juce::File (s)); importFiles (a, curDir); }
    // Copia samples (o carpetas enteras) a la biblioteca; no pisa archivos con el mismo nombre.
    int importFiles (const juce::Array<juce::File>& files, juce::File dest)
    {
        if (dest == juce::File() || (dest.exists() && ! dest.isDirectory())) dest = root; dest.createDirectory(); int n = 0;
        std::function<void (const juce::File&, const juce::File&)> copyDir = [&] (const juce::File& from, const juce::File& to)
        {
            to.createDirectory();
            for (auto& c : from.findChildFiles (juce::File::findFilesAndDirectories, false)) { if (c.getFileName().startsWith (".")) continue; if (c.isDirectory()) copyDir (c, to.getChildFile (c.getFileName())); else if (isAudio (c)) n += copyUnique (c, to) ? 1 : 0; }
        };
        for (auto& f : files) { if (f.isDirectory()) copyDir (f, dest.getChildFile (f.getFileName())); else if (isAudio (f)) n += copyUnique (f, dest) ? 1 : 0; }
        expanded.insert (dest.getFullPathName()); rebuild();
        if (onMessage) onMessage (n > 0 ? juce::String (n) + juce::String::fromUTF8 (n == 1 ? " sample guardado en «" : " samples guardados en «") + (dest == root ? juce::String::fromUTF8 ("Muestras") : dest.getFileName()) + juce::String::fromUTF8 ("»") : juce::String::fromUTF8 ("No hay audio que importar (WAV, AIFF, FLAC, MP3, OGG)"));
        return n;
    }
    void mouseMove (const juce::MouseEvent& e) override { hover = e.position; repaint (0, kHead + 36, getWidth(), 90); }
    void mouseExit (const juce::MouseEvent&) override { hover = { -1, -1 }; repaint(); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        const int content = (int) items.size() * kRow + 10, view = getHeight() - kHead - kTools - kPrev;
        scroll = juce::jlimit (0, juce::jmax (0, content - view), scroll - (int) (w.deltaY * 400)); repaint();
    }

private:
    struct Item { juce::File file; int depth = 0; bool isDir = false, isRoot = false; int extra = -1; juce::String label; };
    std::set<juce::String> collapsedRoots, collapsedCats; bool catMode = false;
    std::vector<Item> items; std::set<juce::String> expanded; juce::File selected, dragFile, rootFile, curDir, extrasFile; std::vector<juce::File> extras; int found = 0; bool wasPlaying = false, attached = false;
    juce::AudioDeviceManager* dev = nullptr; juce::AudioTransportSource transport; juce::AudioSourcePlayer player; std::unique_ptr<juce::AudioFormatReaderSource> preSrc; int scroll = 0; bool dragged = false;
    juce::AudioFormatManager fm; juce::AudioThumbnailCache thumbCache; std::unique_ptr<juce::AudioThumbnail> thumb; std::unique_ptr<juce::FileChooser> chooser; juce::TextEditor search; juce::Point<float> hover { -1, -1 };
    bool searching() const { return search.getText().trim().isNotEmpty(); }
    juce::Rectangle<float> segR() const { return { (float) getWidth() - 184.f, (float) kHead + 105.f, 174.f, 20.f }; }
    juce::Rectangle<float> toolR (int b) const { const float w = ((float) getWidth() - 28.f) * .5f; return { 10.f + (b % 2) * (w + 8.f), (float) kHead + 36.f + (b / 2) * 34.f, w, 30.f }; }

    juce::Rectangle<float> closeR() const { return { (float) getWidth() - 52.f, 10.f, 36.f, 36.f }; }
    juce::Rectangle<float> collapseR() const { return { (float) getWidth() - 94.f, 10.f, 36.f, 36.f }; }
    static void drawX (juce::Graphics& g, juce::Rectangle<float> r) { auto c = r.getCentre(); g.drawLine (c.x - 6, c.y - 6, c.x + 6, c.y + 6, 1.8f); g.drawLine (c.x + 6, c.y - 6, c.x - 6, c.y + 6, 1.8f); }
    static void drawChevrons (juce::Graphics& g, juce::Rectangle<float> r) { auto c = r.getCentre(); for (float o : { -3.f, 4.f }) { juce::Path p; p.startNewSubPath (c.x + o + 3, c.y - 6); p.lineTo (c.x + o - 3, c.y); p.lineTo (c.x + o + 3, c.y + 6); g.strokePath (p, juce::PathStrokeType (1.8f)); } }
    int itemAt (int y) const { float yy = (float) kHead + kTools + 6 - (float) scroll; if (y < kHead + kTools || y > getHeight() - kPrev) return -1; const int i = (int) std::floor ((y - yy) / kRow); return (i >= 0 && i < (int) items.size()) ? i : -1; }

    void buildCategories (const juce::Array<juce::File>& roots)
    {
        std::map<juce::String, juce::Array<juce::File>> by; int total = 0;
        for (auto& r : roots) if (r.isDirectory()) for (auto& f : r.findChildFiles (juce::File::findFiles, true, "*")) if (isAudio (f) && ! f.getFileName().startsWith (".") && total < 30000) { by[categoryOf (f)].addIfNotAlreadyThere (f); ++total; }
        struct Cmp { static int compareElements (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()); } } cmp;
        for (auto& cn : categoryNames())
        {
            auto& arr = by[cn]; if (arr.isEmpty()) continue; arr.sort (cmp);
            items.push_back ({ juce::File ("/categoria/" + cn), 0, true, false, -1, cn + "  (" + juce::String (arr.size()) + ")" });
            if (! collapsedCats.count (cn)) for (auto& f : arr) items.push_back ({ f, 1, false, false });
        }
    }
    void rebuild()
    {
        items.clear(); juce::Array<juce::File> roots; roots.add (root); for (auto& e : extras) roots.add (e);
        if (searching())
        {
            const auto q = search.getText().trim(); juce::Array<juce::File> all;
            for (auto& r : roots) if (r.isDirectory()) for (auto& f : r.findChildFiles (juce::File::findFiles, true, "*")) all.addIfNotAlreadyThere (f);
            struct Cmp { static int compareElements (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()); } } cmp; all.sort (cmp);
            found = 0; for (auto& f : all) if (isAudio (f) && f.getFileName().containsIgnoreCase (q)) { ++found; if (items.size() < 400) items.push_back ({ f, 1, false, false }); }
            repaint(); return;
        }
        if (catMode) { buildCategories (roots); repaint(); return; }
        std::function<void (const juce::File&, int)> rec = [&] (const juce::File& d, int dep)
        {
            juce::Array<juce::File> subs = d.findChildFiles (juce::File::findDirectories, false); subs.sort();
            for (auto& s : subs) { if (s.getFileName().startsWith (".")) continue; items.push_back ({ s, dep, true, false }); if (expanded.count (s.getFullPathName())) rec (s, dep + 1); }
            juce::Array<juce::File> fl = d.findChildFiles (juce::File::findFiles, false); fl.sort();
            for (auto& f : fl) if (isAudio (f)) items.push_back ({ f, dep, false, false });
        };
        for (int k = 0; k < roots.size(); ++k)
        {
            if (! roots[k].isDirectory()) continue;
            items.push_back ({ roots[k], 0, true, true, k - 1 }); if (! collapsedRoots.count (roots[k].getFullPathName())) { expanded.insert (roots[k].getFullPathName()); rec (roots[k], 1); }
        }
        repaint();
    }
    void saveExtras() { juce::StringArray ls; for (auto& e : extras) ls.add (e.getFullPathName()); extrasFile.getParentDirectory().createDirectory(); extrasFile.replaceWithText (ls.joinIntoString ("\n")); }
    void timerCallback() override { if (transport.isPlaying() || wasPlaying) { wasPlaying = transport.isPlaying(); repaint (0, getHeight() - kPrev, getWidth(), kPrev); } }
    static bool copyUnique (const juce::File& src, const juce::File& dir)
    {
        if (src.getParentDirectory() == dir) return false;
        auto dst = dir.getChildFile (src.getFileName());
        for (int k = 2; dst.exists() && k < 1000; ++k) dst = dir.getChildFile (src.getFileNameWithoutExtension() + "_" + juce::String (k) + src.getFileExtension());
        return src.copyFileTo (dst);
    }
    void importDialog (const juce::File& dest)
    {
        chooser = std::make_unique<juce::FileChooser> (juce::String::fromUTF8 ("Elige los samples que quieres guardar en tu biblioteca"), juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems, [this, dest] (const juce::FileChooser& fc) { if (fc.getResults().size() > 0) importFiles (fc.getResults(), dest); });
    }
    void newFolder (const juce::File& parent)
    {
        auto* aw = new juce::AlertWindow (juce::String::fromUTF8 ("Nueva carpeta"), juce::String::fromUTF8 ("Se crea dentro de «") + (parent == root ? juce::String::fromUTF8 ("Muestras") : parent.getFileName()) + juce::String::fromUTF8 ("»"), juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("n", "", juce::String::fromUTF8 ("Nombre")); aw->addButton ("Crear", 1); aw->addButton ("Cancelar", 0);
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, parent, aw] (int res)
        {
            auto name = juce::File::createLegalFileName (aw->getTextEditorContents ("n").trim());
            if (res == 1 && name.isNotEmpty()) { auto d = parent.getChildFile (name); d.createDirectory(); expanded.insert (parent.getFullPathName()); curDir = d; rebuild(); }
            delete aw;
        }));
    }
    void itemMenu (const juce::File& f, bool isDir, bool isRoot, int extra)
    {
        juce::PopupMenu m; const auto dir = isDir ? f : f.getParentDirectory();
        m.addItem (1, juce::String::fromUTF8 ("Importar samples aquí…")); m.addItem (2, juce::String::fromUTF8 ("Nueva subcarpeta…")); m.addSeparator(); m.addItem (3, juce::String::fromUTF8 ("Mostrar en el Explorador")); if (isRoot && extra < 0) m.addItem (4, juce::String::fromUTF8 ("Cambiar carpeta principal…")); if (isRoot && extra >= 0) m.addItem (5, juce::String::fromUTF8 ("Quitar de la lista (no borra nada)"));
        m.showMenuAsync ({}, [this, f, dir, isRoot, extra] (int id) { if (id == 5 && extra >= 0 && extra < (int) extras.size()) { extras.erase (extras.begin() + extra); saveExtras(); rebuild(); return; } if (id == 1) importDialog (dir); else if (id == 2) newFolder (dir); else if (id == 3) f.revealToUser(); else if (id == 4 && isRoot) chooseRoot(); });
    }
    void addFolderDialog()
    {
        chooser = std::make_unique<juce::FileChooser> (juce::String::fromUTF8 ("Agrega una carpeta con samples a tu biblioteca"), root);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
        { if (fc.getResult().isDirectory()) { if (addExtraRoot (fc.getResult())) { if (onMessage) onMessage (juce::String::fromUTF8 ("Carpeta agregada: ") + fc.getResult().getFileName() + juce::String::fromUTF8 (". La búsqueda ya la incluye")); } } });
    }
    juce::Rectangle<float> previewBox() const { return { 8.f, (float) getHeight() - kPrev, (float) getWidth() - 16.f, kPrev - 8.f }; }
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
        if (transport.getLengthInSeconds() > 0.0 && (transport.isPlaying() || transport.getCurrentPosition() > 0.0)) { auto wr = b.reduced (8, 8).withTrimmedBottom (22); const float px = wr.getX() + wr.getWidth() * (float) (transport.getCurrentPosition() / transport.getLengthInSeconds()); g.setColour (juce::Colours::white); g.fillRect (px - .75f, wr.getY(), 1.5f, wr.getHeight()); }
        g.setColour (text); g.setFont (juce::FontOptions (13.f)); g.drawText (info, b.toNearestInt().withTrimmedRight (12), juce::Justification::bottomRight);
        g.setColour (textDim); g.setFont (juce::FontOptions (12.f)); g.drawText (transport.isPlaying() ? juce::String::fromUTF8 ("Sonando · clic para repetir desde ahí") : juce::String::fromUTF8 ("Clic aquí para escuchar"), b.toNearestInt().withTrimmedLeft (12), juce::Justification::bottomLeft);
    }
};
