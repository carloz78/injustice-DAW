#pragma once
#include "FloatingPanel.h"
#include <tracktion_engine/tracktion_engine.h>
namespace te = tracktion;

// ---------------------------------------------------------------------------------------------
// Alojamiento de plugins VST3: ventana del editor, escaneo en proceso aparte, lista persistente.
// ---------------------------------------------------------------------------------------------
namespace pluginhost
{
    inline juce::File dataDir() { auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("INJUSTICE DAW"); d.createDirectory(); return d; }
    inline juce::File listFile()    { return dataDir().getChildFile ("plugins.xml"); }
    inline juce::File foldersFile() { return dataDir().getChildFile ("plugin_folders.txt"); }

    inline juce::StringArray defaultFolders()
    {
        juce::StringArray a;
       #if JUCE_WINDOWS
        a.add ("C:\\Program Files\\Common Files\\VST3");
        a.add (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getParentDirectory().getChildFile ("Local/Programs/Common/VST3").getFullPathName());
       #elif JUCE_MAC
        a.add ("/Library/Audio/Plug-Ins/VST3"); a.add ("~/Library/Audio/Plug-Ins/VST3");
       #else
        a.add ("/usr/lib/vst3"); a.add ("~/.vst3");
       #endif
        return a;
    }
    inline juce::StringArray customFolders() { juce::StringArray a; a.addLines (foldersFile().loadFileAsString()); a.removeEmptyStrings(); return a; }
    inline juce::File filesFile() { return dataDir().getChildFile ("plugin_files.txt"); }
    inline juce::StringArray customFiles() { juce::StringArray a; a.addLines (filesFile().loadFileAsString()); a.removeEmptyStrings(); return a; }
    inline void saveCustomFolders (const juce::StringArray& a) { foldersFile().replaceWithText (a.joinIntoString ("\n")); }

    inline void loadList (te::Engine& e)
    {
        if (auto xml = juce::XmlDocument::parse (listFile())) e.getPluginManager().knownPluginList.recreateFromXml (*xml);
    }
    inline void saveList (te::Engine& e)
    {
        if (auto xml = e.getPluginManager().knownPluginList.createXml()) xml->writeTo (listFile());
    }

    // Modo "hijo": analiza un solo plugin y escribe el resultado. Si el plugin se cuelga o falla, solo muere este proceso.
    inline void runScanChild (const juce::String& file, const juce::String& out)
    {
        juce::VST3PluginFormat fmt; juce::OwnedArray<juce::PluginDescription> found;
        fmt.findAllTypesForFile (found, file);
        juce::XmlElement root ("plugins");
        for (auto* d : found) root.addChildElement (d->createXml().release());
        root.writeTo (juce::File (out));
    }

    // ----- Ventana del editor de un plugin -----
    class EditorWindow : public juce::DocumentWindow
    {
    public:
        EditorWindow (te::Plugin::WindowState& s, std::unique_ptr<te::Plugin::EditorComponent> ed)
            : DocumentWindow (s.plugin.getName(), theme::bgBar2, DocumentWindow::closeButton | DocumentWindow::minimiseButton), state (s)
        {
            setUsingNativeTitleBar (true); const bool rs = ed->allowWindowResizing(); setContentOwned (ed.release(), true);
            setResizable (rs, false); centreAroundComponent (nullptr, getWidth(), getHeight()); setVisible (true); setAlwaysOnTop (false);
        }
        void closeButtonPressed() override { state.closeWindowExplicitly(); }
    private:
        te::Plugin::WindowState& state;
    };

    // Ejecuta una tarea de Tracktion (p. ej. exportar) con ventana de progreso y bot\xc3\xb3n Cancelar.
    struct TaskRunner : juce::ThreadWithProgressWindow
    {
        TaskRunner (te::ThreadPoolJobWithProgress& t) : juce::ThreadWithProgressWindow (juce::String::fromUTF8 ("Exportando…"), true, true), task (t) {}
        void run() override
        {
            while (! threadShouldExit())
            {
                const auto r = task.runJob(); setProgress (juce::jlimit (0.0, 1.0, (double) task.getCurrentTaskProgress()));
                if (r != juce::ThreadPoolJob::jobNeedsRunningAgain) break;
            }
        }
        te::ThreadPoolJobWithProgress& task;
    };

    // Límites del proyecto: el motor trae 400 pistas y 16 plugins por pista; aquí se amplían.
    struct Behaviour : public te::EngineBehaviour
    {
        te::EditLimits getEditLimits() override { te::EditLimits l; l.maxNumTracks = 1000; l.maxPluginsOnTrack = 32; l.maxClipsInTrack = 5000; return l; }
    };

    class UIBehaviour : public te::UIBehaviour
    {
    public:
        void runTaskWithProgressBar (te::ThreadPoolJobWithProgress& t) override
        {
            if (std::getenv ("INJ_SELFTEST")) { while (t.runJob() == juce::ThreadPoolJob::jobNeedsRunningAgain) {} return; }   // sin ventana en las pruebas
            TaskRunner r (t); r.runThread();
        }
        std::unique_ptr<juce::Component> createPluginWindow (te::PluginWindowState& s) override
        {
            if (auto* ws = dynamic_cast<te::Plugin::WindowState*> (&s))
                if (auto ed = ws->plugin.createEditor()) return std::make_unique<EditorWindow> (*ws, std::move (ed));
            return {};
        }
    };

    // ----- Escáner (hilo que lanza un proceso hijo por plugin) -----
    class Scanner : public juce::Thread
    {
    public:
        std::function<void (const juce::String&, double)> onProgress;   // se llama en el hilo de mensajes
        std::function<void (int nuevos, int fallidos)> onDone;
        Scanner (te::Engine& e) : juce::Thread ("Escaneo de plugins"), engine (e) {}
        ~Scanner() override { stopThread (60000); }
        void run() override
        {
            auto& list = engine.getPluginManager().knownPluginList; juce::VST3PluginFormat fmt;
            juce::FileSearchPath sp; for (auto& f : defaultFolders()) sp.add (juce::File (f.replace ("~", juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName()))); for (auto& f : customFolders()) sp.add (juce::File (f));
            auto files = fmt.searchPathsForPlugins (sp, true, false); for (auto& cf : customFiles()) if (juce::File (cf).exists()) files.addIfNotAlreadyThere (cf);
            const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName();
            int added = 0, failed = 0;
            for (int i = 0; i < files.size() && ! threadShouldExit(); ++i)
            {
                const auto f = files[i]; const double p = (double) i / juce::jmax (1, files.size());
                juce::MessageManager::callAsync ([this, f, p, alive = std::weak_ptr<int> (token)] { if (alive.lock() && onProgress) onProgress (juce::File (f).getFileNameWithoutExtension(), p); });
                if (list.isListingUpToDate (f, fmt)) continue;
                bool blocked = false; for (auto& b : list.getBlacklistedFiles()) if (b == f) blocked = true;
                if (blocked) continue;
                auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("inj_scan_" + juce::String (i) + ".xml"); tmp.deleteFile();
                juce::ChildProcess cp; bool ok = false;
                if (cp.start (juce::StringArray { exe, "--scan-plugin", f, tmp.getFullPathName() }, 0))
                {
                    const auto t0 = juce::Time::getMillisecondCounter();
                    while (cp.isRunning() && juce::Time::getMillisecondCounter() - t0 < 60000 && ! threadShouldExit()) wait (100);
                    if (cp.isRunning()) cp.kill(); else ok = tmp.existsAsFile();
                }
                if (ok)
                {
                    if (auto xml = juce::XmlDocument::parse (tmp)) for (auto* c : xml->getChildIterator()) { juce::PluginDescription d; if (d.loadFromXml (*c)) { if (list.addType (d)) ++added; } }
                }
                else { list.addToBlacklist (f); ++failed; }
                tmp.deleteFile();
            }
            saveList (engine);
            juce::MessageManager::callAsync ([this, added, failed, alive = std::weak_ptr<int> (token)] { if (alive.lock() && onDone) onDone (added, failed); });
        }
        std::shared_ptr<int> token = std::make_shared<int> (0);
    private:
        te::Engine& engine;
    };

    // ----- Panel "Plugins" -----
    class ListContent : public juce::Component, private juce::TableListBoxModel
    {
    public:
        std::function<void (const juce::PluginDescription&)> onAdd;   // agrega a la pista de destino
        int targetTrack = 0;

        ListContent (te::Engine& e) : engine (e)
        {
            setSize (780, 540);
            addAndMakeVisible (search); search.setTextToShowWhenEmpty (juce::String::fromUTF8 ("Buscar por nombre o fabricante…"), juce::Colours::grey); search.onTextChange = [this] { refresh(); };
            addAndMakeVisible (scanBtn); scanBtn.setButtonText ("Buscar plugins"); scanBtn.onClick = [this] { startScan(); };
            addAndMakeVisible (folderBtn); folderBtn.setButtonText (juce::String::fromUTF8 ("Añadir carpeta…")); folderBtn.onClick = [this] { addFolder(); };
            addAndMakeVisible (fileBtn); fileBtn.setButtonText (juce::String::fromUTF8 ("Añadir plugin…")); fileBtn.onClick = [this] { addFile(); };
            addAndMakeVisible (addBtn); addBtn.onClick = [this] { addSelected(); };
            addAndMakeVisible (table); table.setModel (this); table.setRowHeight (30); table.setColour (juce::ListBox::backgroundColourId, theme::bgBar);
            auto& h = table.getHeader(); h.addColumn ("Nombre", 1, 300); h.addColumn ("Fabricante", 2, 200); h.addColumn ("Tipo", 3, 110); h.addColumn ("Formato", 4, 90); h.setStretchToFitActive (true);
            table.setMultipleSelectionEnabled (false);
            h.setColour (juce::TableHeaderComponent::backgroundColourId, juce::Colour (0xff1d1b23)); h.setColour (juce::TableHeaderComponent::textColourId, theme::text); h.setColour (juce::TableHeaderComponent::outlineColourId, juce::Colour (0xff2e2c34)); h.setColour (juce::TableHeaderComponent::highlightColourId, theme::accent.withAlpha (.4f));
            engine.getPluginManager().knownPluginList.addChangeListener (&relay); relay.owner = this;
            refresh();
        }
        ~ListContent() override { engine.getPluginManager().knownPluginList.removeChangeListener (&relay); scanner.reset(); }
        bool isEmpty() const { return shown.isEmpty(); }
        void setTarget (int t) { targetTrack = t; refresh(); }
        void startScan()
        {
            if (scanner) return;
            scanner = std::make_unique<Scanner> (engine); scanBtn.setEnabled (false); status = juce::String::fromUTF8 ("Buscando…"); repaint();
            scanner->onProgress = [this] (const juce::String& n, double p) { status = juce::String::fromUTF8 ("Analizando: ") + n + " (" + juce::String (juce::roundToInt (p * 100)) + " %)"; repaint(); };
            scanner->onDone = [this] (int a, int f)
            {
                status = juce::String (a) + juce::String::fromUTF8 (" plugin(s) nuevo(s)") + (f > 0 ? juce::String (", ") + juce::String (f) + juce::String::fromUTF8 (" no se pudieron cargar (se omitirán)") : juce::String());
                scanBtn.setEnabled (true); juce::MessageManager::callAsync ([this] { scanner.reset(); refresh(); repaint(); }); repaint();
            };
            scanner->startThread();
        }
        void paint (juce::Graphics& g) override
        {
            g.fillAll (theme::bgBar2); g.setColour (theme::text); g.setFont (juce::FontOptions (13.f)); g.drawText (status, 16, getHeight() - 36, getWidth() - 360, 28, juce::Justification::centredLeft);
            if (shown.isEmpty())
            {
                g.setColour (theme::textDim); g.setFont (juce::FontOptions (14.f));
                g.drawFittedText (juce::String::fromUTF8 ("Aún no hay plugins en la lista.\nPulsa «Buscar plugins» para analizar tus carpetas VST3,\no «Añadir carpeta…» si tus plugins están en otra ubicación."), table.getBounds().reduced (40), juce::Justification::centred, 4);
            }
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (12); auto top = r.removeFromTop (34);
            scanBtn.setBounds (top.removeFromRight (130)); top.removeFromRight (8); folderBtn.setBounds (top.removeFromRight (140)); top.removeFromRight (8); fileBtn.setBounds (top.removeFromRight (130)); top.removeFromRight (8); search.setBounds (top);
            auto bot = r.removeFromBottom (36); addBtn.setBounds (bot.removeFromRight (330)); r.removeFromTop (8); table.setBounds (r.withTrimmedBottom (4));
        }
    private:
        te::Engine& engine; juce::TextEditor search; juce::TextButton scanBtn, folderBtn, fileBtn, addBtn; juce::TableListBox table; juce::String status;
        std::unique_ptr<Scanner> scanner; juce::Array<juce::PluginDescription> shown; std::unique_ptr<juce::FileChooser> chooser;
        struct Relay : juce::ChangeListener { ListContent* owner = nullptr; void changeListenerCallback (juce::ChangeBroadcaster*) override { if (owner) owner->refresh(); } } relay;

        static bool isOurs (const juce::PluginDescription& d)
        {
            const auto m = d.manufacturerName.toLowerCase(), n = d.name.toLowerCase();
            if (m.contains ("injustice")) return true;
            for (auto* k : { "injustice", "ravecore", "lowcore", "comp core", "kick forge", "gabber drive" }) if (n.contains (k)) return true;
            return false;
        }
        void refresh()
        {
            shown.clear(); const auto q = search.getText().trim();
            for (auto& d : engine.getPluginManager().knownPluginList.getTypes())
                if (q.isEmpty() || d.name.containsIgnoreCase (q) || d.manufacturerName.containsIgnoreCase (q)) shown.add (d);
            struct Cmp { static int compareElements (const juce::PluginDescription& a, const juce::PluginDescription& b) { const bool oa = isOurs (a), ob = isOurs (b); if (oa != ob) return oa ? -1 : 1; return a.name.compareIgnoreCase (b.name); } } cmp; shown.sort (cmp);
            table.updateContent(); table.repaint(); updateAddLabel(); repaint();
        }
        void updateAddLabel()
        {
            const int r = table.getSelectedRow(); const bool ok = r >= 0 && r < shown.size();
            addBtn.setEnabled (ok); addBtn.setButtonText (juce::String::fromUTF8 ("Agregar a la pista ") + juce::String (targetTrack + 1) + (ok ? (shown[r].isInstrument ? juce::String::fromUTF8 (" como instrumento") : juce::String::fromUTF8 (" como efecto")) : juce::String()));
        }
        void addSelected() { const int r = table.getSelectedRow(); if (r >= 0 && r < shown.size() && onAdd) onAdd (shown[r]); }
        void addFile()
        {
            chooser = std::make_unique<juce::FileChooser> (juce::String::fromUTF8 ("Elige tu plugin (.vst3)"), juce::File(), "*.vst3");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
            { auto f = fc.getResult(); if (f.exists() && f.hasFileExtension ("vst3")) { auto a = customFiles(); a.addIfNotAlreadyThere (f.getFullPathName()); filesFile().replaceWithText (a.joinIntoString ("\n")); startScan(); } });
        }
        void addFolder()
        {
            chooser = std::make_unique<juce::FileChooser> ("Carpeta con plugins VST3", juce::File(), "");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
            { auto d = fc.getResult(); if (d.isDirectory()) { auto a = customFolders(); a.addIfNotAlreadyThere (d.getFullPathName()); saveCustomFolders (a); startScan(); } });
        }
        int getNumRows() override { return shown.size(); }
        void paintRowBackground (juce::Graphics& g, int row, int w, int h, bool sel) override
        {
            const bool ours = row < shown.size() && isOurs (shown.getReference (row));
            g.fillAll (sel ? theme::accent : ours ? juce::Colour (0xff231a33) : (row % 2 ? juce::Colour (0xff141218) : theme::bgBar));
            if (ours && ! sel) { g.setColour (theme::accentHi); g.fillRect (0, 0, 3, h); }
            if (ours && (row + 1 >= shown.size() || ! isOurs (shown.getReference (row + 1)))) { g.setColour (juce::Colour (0xff4a3a6e)); g.fillRect (0, h - 1, w, 1); }
        }
        void paintCell (juce::Graphics& g, int row, int col, int w, int h, bool) override
        {
            if (row >= shown.size()) return; auto& d = shown.getReference (row); g.setColour (juce::Colours::white); g.setFont (juce::FontOptions (14.f));
            const bool ours = isOurs (d); if (ours) g.setFont (juce::FontOptions (14.f, juce::Font::bold));
            const juce::String t = col == 1 ? d.name : col == 2 ? (ours ? juce::String ("INJUSTICE AUDIO") : d.manufacturerName) : col == 3 ? (d.isInstrument ? juce::String ("Instrumento") : juce::String ("Efecto")) : d.pluginFormatName;
            g.drawText (t, 10, 0, w - 14, h, juce::Justification::centredLeft, true);
        }
        void selectedRowsChanged (int) override { updateAddLabel(); }
        void cellDoubleClicked (int row, int, const juce::MouseEvent&) override { table.selectRow (row); addSelected(); }
        void returnKeyPressed (int) override { addSelected(); }
    };
}
