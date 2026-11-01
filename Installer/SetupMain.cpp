// ============================================================================
//  Changes 1.0 — Windows installer / uninstaller
//  The installer is a small JUCE app. The files to install are appended to the
//  executable as a zip archive followed by a 16-byte trailer:
//      [setup stub .exe][payload.zip][int64 zipOffset]["CHGSETUP"]
//  The uninstaller is the same stub without a payload.
// ============================================================================
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/LookAndFeel.h"

#if JUCE_WINDOWS
 #include <windows.h>
 #include <shlobj.h>
#endif

using namespace juce;
using LNF = ChangesLookAndFeel;

static bool makeShortcut (const File& target, const String& description, const File& link)
{
   #if JUCE_WINDOWS
    return target.createShortcut (description, link);
   #else
    ignoreUnused (target, description, link); return false;
   #endif
}

static const String appName    = "Changes";
static const String version    = "1.0.0";
static const String publisher  = "JazzyT Audio";
static const String uninstKey  = "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\JazzyT Audio Changes\\";
static const String settingsKey = "HKEY_LOCAL_MACHINE\\Software\\JazzyT Audio\\Changes\\";

//==============================================================================
struct Payload
{
    juce::int64 zipOffset = -1, zipLength = 0;
    File exe;

    bool locate()
    {
        exe = File::getSpecialLocation (File::currentExecutableFile);
        FileInputStream in (exe);
        if (! in.openedOk() || in.getTotalLength() < 32) return false;
        in.setPosition (in.getTotalLength() - 16);
        const juce::int64 off = in.readInt64();
        char magic[9] {}; in.read (magic, 8);
        if (String (magic) != "CHGSETUP") return false;
        if (off <= 0 || off >= in.getTotalLength() - 16) return false;
        zipOffset = off; zipLength = in.getTotalLength() - 16 - off;
        return true;
    }

    std::unique_ptr<ZipFile> open() const
    {
        auto* fin = new FileInputStream (exe);
        auto* sub = new SubregionStream (fin, zipOffset, zipLength, true);
        return std::make_unique<ZipFile> (sub, true);
    }
};

static File programFiles()
{
    return File::getSpecialLocation (File::globalApplicationsDirectory);   // C:\Program Files
}

static File defaultAppDir()   { return programFiles().getChildFile (publisher).getChildFile (appName); }
static File defaultVst3Dir()  { return programFiles().getChildFile ("Common Files").getChildFile ("VST3"); }

#if JUCE_WINDOWS
static File knownFolder (int csidl)
{
    WCHAR path[MAX_PATH] {};
    if (SUCCEEDED (SHGetFolderPathW (nullptr, csidl, nullptr, 0, path))) return File (String (path));
    return {};
}
#endif

static File desktopShortcut()
{
   #if JUCE_WINDOWS
    return knownFolder (CSIDL_COMMON_DESKTOPDIRECTORY).getChildFile (appName + ".lnk");
   #else
    return File::getSpecialLocation (File::userDesktopDirectory).getChildFile (appName + ".lnk");
   #endif
}

static File startMenuFolder()
{
   #if JUCE_WINDOWS
    return knownFolder (CSIDL_COMMON_PROGRAMS).getChildFile (appName);
   #else
    return File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile (appName);
   #endif
}

/** Launch a file un-elevated through Explorer (the installer itself runs as admin). */
static void openUnelevated (const File& f)
{
   #if JUCE_WINDOWS
    ShellExecuteW (nullptr, L"open", L"explorer.exe", ("\"" + f.getFullPathName() + "\"").toWideCharPointer(), nullptr, SW_SHOWNORMAL);
   #else
    f.startAsProcess();
   #endif
}

static bool isProcessRunning (const String& exeName)
{
   #if JUCE_WINDOWS
    ChildProcess p;
    if (p.start ("tasklist /FI \"IMAGENAME eq " + exeName + "\" /NH"))
        return p.readAllProcessOutput().containsIgnoreCase (exeName);
   #endif
    ignoreUnused (exeName);
    return false;
}

//==============================================================================
struct InstallOptions
{
    File appDir = defaultAppDir(), vst3Dir = defaultVst3Dir();
    bool standalone = true, vst3 = true, desktop = true, startMenu = true, manual = true;
};

/** Does the actual work. Reports progress (0..1) and log lines via callbacks. */
static Result performInstall (const Payload& payload, const InstallOptions& o,
                              std::function<void (double, const String&)> progress)
{
    auto zip = payload.open();
    if (zip == nullptr || zip->getNumEntries() == 0) return Result::fail ("The installer payload is missing or damaged.");

    if (o.standalone && isProcessRunning ("Changes.exe"))
        return Result::fail ("Changes is currently running. Please close it and run Setup again.");

    if (! o.appDir.createDirectory()) return Result::fail ("Could not create " + o.appDir.getFullPathName());
    if (o.vst3 && ! o.vst3Dir.createDirectory()) return Result::fail ("Could not create " + o.vst3Dir.getFullPathName());

    const int n = zip->getNumEntries();
    for (int i = 0; i < n; ++i)
    {
        const auto* e = zip->getEntry (i);
        const String name = e->filename.replaceCharacter ('\\', '/');
        File target;
        if (name.startsWith ("app/"))
        {
            const String rel = name.fromFirstOccurrenceOf ("app/", false, false);
            if (rel == "Changes.exe" && ! o.standalone) continue;
            if (rel.endsWithIgnoreCase (".pdf") && ! o.manual) continue;
            target = o.appDir.getChildFile (rel);
        }
        else if (name.startsWith ("vst3/"))
        {
            if (! o.vst3) continue;
            target = o.vst3Dir.getChildFile (name.fromFirstOccurrenceOf ("vst3/", false, false));
        }
        else continue;

        if (name.endsWithChar ('/')) { target.createDirectory(); continue; }
        progress ((double) i / n, "Installing " + target.getFullPathName());
        target.getParentDirectory().createDirectory();
        if (target.existsAsFile() && ! target.deleteFile())
            return Result::fail ("Could not replace " + target.getFullPathName() + " (is it in use by a DAW?)");
        std::unique_ptr<InputStream> in (zip->createStreamForEntry (i));
        if (in == nullptr) return Result::fail ("Could not read " + name + " from the installer.");
        FileOutputStream out (target);
        if (! out.openedOk()) return Result::fail ("Could not write " + target.getFullPathName());
        out.writeFromInputStream (*in, -1);
        out.flush();
        if (out.getStatus().failed()) return Result::fail (out.getStatus().getErrorMessage());
    }

    // Mark the VST3 bundle folder so Explorer shows the plugin icon (desktop.ini needs the System attribute)
   #if JUCE_WINDOWS
    if (o.vst3)
    {
        const File bundle = o.vst3Dir.getChildFile ("Changes.vst3");
        SetFileAttributesW (bundle.getFullPathName().toWideCharPointer(), FILE_ATTRIBUTE_SYSTEM);
        const File ini = bundle.getChildFile ("desktop.ini");
        if (ini.existsAsFile()) SetFileAttributesW (ini.getFullPathName().toWideCharPointer(), FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN);
    }
   #endif

    // Uninstaller: the stub part of this executable (no payload)
    progress (0.92, "Writing uninstaller");
    const File uninstaller = o.appDir.getChildFile ("Uninstall Changes.exe");
    {
        uninstaller.deleteFile();
        FileInputStream in (payload.exe);
        FileOutputStream out (uninstaller);
        if (in.openedOk() && out.openedOk()) out.writeFromInputStream (in, payload.zipOffset);
    }

    // Shortcuts
    progress (0.95, "Creating shortcuts");
    const File exe = o.appDir.getChildFile ("Changes.exe");
    if (o.standalone && o.desktop) makeShortcut (exe, "Changes - Jazz Harmony Instrument", desktopShortcut());
    if (o.startMenu)
    {
        const File sm = startMenuFolder();
        sm.createDirectory();
        if (o.standalone) makeShortcut (exe, "Changes - Jazz Harmony Instrument", sm.getChildFile ("Changes.lnk"));
        const File man = o.appDir.getChildFile ("Changes Manual.pdf");
        if (man.existsAsFile()) makeShortcut (man, "Changes installation and operating instructions", sm.getChildFile ("Changes Manual.lnk"));
        makeShortcut (uninstaller, "Uninstall Changes", sm.getChildFile ("Uninstall Changes.lnk"));
    }

    // Registry: Apps & features entry + remembered paths
   #if JUCE_WINDOWS
    progress (0.98, "Registering with Windows");
    WindowsRegistry::setValue (uninstKey + "DisplayName", "Changes - Jazz Harmony Instrument");
    WindowsRegistry::setValue (uninstKey + "DisplayVersion", version);
    WindowsRegistry::setValue (uninstKey + "Publisher", publisher);
    WindowsRegistry::setValue (uninstKey + "InstallLocation", o.appDir.getFullPathName());
    WindowsRegistry::setValue (uninstKey + "DisplayIcon", (o.standalone ? exe : uninstaller).getFullPathName());
    WindowsRegistry::setValue (uninstKey + "UninstallString", "\"" + uninstaller.getFullPathName() + "\" /uninstall");
    WindowsRegistry::setValue (uninstKey + "QuietUninstallString", "\"" + uninstaller.getFullPathName() + "\" /uninstall /S");
    WindowsRegistry::setValue (uninstKey + "NoModify", (uint32) 1);
    WindowsRegistry::setValue (uninstKey + "NoRepair", (uint32) 1);
    WindowsRegistry::setValue (uninstKey + "EstimatedSize", (uint32) (zip->getNumEntries() > 0 ? payload.zipLength * 3 / 1024 : 0));
    WindowsRegistry::setValue (settingsKey + "InstallDir", o.appDir.getFullPathName());
    WindowsRegistry::setValue (settingsKey + "Vst3Dir", o.vst3 ? o.vst3Dir.getFullPathName() : String());
    WindowsRegistry::setValue (settingsKey + "Version", version);
   #endif

    progress (1.0, "Done");
    return Result::ok();
}

static Result performUninstall (std::function<void (const String&)> log)
{
    File appDir = defaultAppDir(), vst3Dir = defaultVst3Dir();
   #if JUCE_WINDOWS
    const String a = WindowsRegistry::getValue (settingsKey + "InstallDir");
    const String v = WindowsRegistry::getValue (settingsKey + "Vst3Dir");
    if (a.isNotEmpty()) appDir = File (a);
    if (v.isNotEmpty()) vst3Dir = File (v);
   #endif
    if (isProcessRunning ("Changes.exe")) return Result::fail ("Changes is running. Please close it (and any DAW using the plugin) first.");

    const File bundle = vst3Dir.getChildFile ("Changes.vst3");
    if (bundle.isDirectory())
    {
       #if JUCE_WINDOWS
        SetFileAttributesW (bundle.getFullPathName().toWideCharPointer(), FILE_ATTRIBUTE_NORMAL);
        SetFileAttributesW (bundle.getChildFile ("desktop.ini").getFullPathName().toWideCharPointer(), FILE_ATTRIBUTE_NORMAL);
       #endif
        log ("Removing " + bundle.getFullPathName());
        if (! bundle.deleteRecursively()) return Result::fail ("Could not remove the VST3 plug-in (close your DAW and try again).");
    }
    for (auto f : { desktopShortcut(), startMenuFolder() })
    {
        log ("Removing " + f.getFullPathName());
        if (f.isDirectory()) f.deleteRecursively(); else f.deleteFile();
    }
    const File self = File::getSpecialLocation (File::currentExecutableFile);
    for (const auto& f : appDir.findChildFiles (File::findFilesAndDirectories, false))
        if (f != self) { log ("Removing " + f.getFullPathName()); if (f.isDirectory()) f.deleteRecursively(); else f.deleteFile(); }

   #if JUCE_WINDOWS
    WindowsRegistry::deleteKey (uninstKey.dropLastCharacters (1));
    WindowsRegistry::deleteKey (settingsKey.dropLastCharacters (1));
    // delete the uninstaller and the (now empty) folders once this process has exited
    const String cmd = "cmd.exe /C ping 127.0.0.1 -n 3 > nul & del /F /Q \"" + self.getFullPathName() + "\" & rmdir \""
                       + appDir.getFullPathName() + "\" & rmdir \"" + appDir.getParentDirectory().getFullPathName() + "\"";
    STARTUPINFOW si {}; si.cb = sizeof (si); si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi {};
    std::wstring w (cmd.toWideCharPointer());
    if (CreateProcessW (nullptr, w.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    { CloseHandle (pi.hThread); CloseHandle (pi.hProcess); }
   #endif
    return Result::ok();
}

//==============================================================================
class SetupComponent : public Component, private Timer
{
public:
    SetupComponent (const Payload& p, bool uninstallMode) : payload (p), uninstall (uninstallMode)
    {
        setLookAndFeel (&lnf);
        for (auto* b : { &standaloneTgl, &vstTgl, &desktopTgl, &startTgl, &manualTgl, &launchTgl, &openManualTgl })
        {
            b->setToggleState (true, dontSendNotification);
            addChildComponent (*b);
        }
        openManualTgl.setToggleState (false, dontSendNotification);
        for (auto* e : { &appDirEd, &vstDirEd })
        {
            e->setFont (LNF::font (13.0f));
            e->setIndents (8, 6);
            e->setColour (TextEditor::backgroundColourId, colours::bg0);
            e->setColour (TextEditor::outlineColourId, colours::border);
            e->setColour (TextEditor::focusedOutlineColourId, colours::gold);
            e->setColour (TextEditor::textColourId, colours::text);
            addChildComponent (*e);
        }
        appDirEd.setText (opts.appDir.getFullPathName());
        vstDirEd.setText (opts.vst3Dir.getFullPathName());
        for (auto* b : { &browseApp, &browseVst, &backBtn, &nextBtn, &cancelBtn }) addChildComponent (*b);
        nextBtn.getProperties().set ("accent", colours::gold.toString());
        nextBtn.setToggleState (true, dontSendNotification);
        browseApp.onClick = [this] { browse (appDirEd); };
        browseVst.onClick = [this] { browse (vstDirEd); };
        backBtn.onClick = [this] { if (page == 1) setPage (0); };
        nextBtn.onClick = [this] { next(); };
        cancelBtn.onClick = [] { JUCEApplicationBase::quit(); };
        vstTgl.onClick = [this] { vstDirEd.setEnabled (vstTgl.getToggleState()); browseVst.setEnabled (vstTgl.getToggleState()); };
        standaloneTgl.onClick = [this] { desktopTgl.setEnabled (standaloneTgl.getToggleState()); };
        setSize (880, 620);
        setPage (0);
    }

    ~SetupComponent() override { setLookAndFeel (nullptr); }

    void paint (Graphics& g) override
    {
        ColourGradient bg (Colour (0xff141c2e), 0, 0, Colour (0xff070a12), 0, (float) getHeight(), false);
        g.setGradientFill (bg); g.fillAll();
        // left banner
        auto banner = juce::Rectangle<float> (0, 0, 220, (float) getHeight());
        ColourGradient bb (Colour (0xff1b2540), 0, 0, Colour (0xff0a0e18), 0, banner.getBottom(), false);
        g.setGradientFill (bb); g.fillRect (banner);
        g.setColour (colours::gold.withAlpha (0.4f)); g.drawLine (220, 0, 220, (float) getHeight(), 1.0f);
        Font f = LNF::displayFont (30.0f); f.setExtraKerningFactor (0.1f); g.setFont (f);
        ColourGradient lg (colours::goldLight, 0, 40, colours::goldDark, 0, 76, false);
        g.setGradientFill (lg);
        g.drawText ("CHANGES", juce::Rectangle<int> (24, 36, 190, 40), Justification::centredLeft);
        g.setFont (LNF::font (9.5f, true)); g.setColour (colours::textDim);
        g.drawText ("JAZZ HARMONY INSTRUMENT", juce::Rectangle<int> (25, 74, 190, 14), Justification::centredLeft);
        // decorative pads
        for (int i = 0; i < 3; ++i)
        {
            auto r = juce::Rectangle<float> (36.0f + i * 52.0f, 330.0f - i * 40.0f, 40.0f, 110.0f + i * 40.0f);
            ColourGradient pg (colours::goldLight, r.getX(), r.getY(), colours::goldDark, r.getX(), r.getBottom(), false);
            g.setGradientFill (pg); g.fillRoundedRectangle (r, 6.0f);
        }
        g.setFont (LNF::font (11.0f)); g.setColour (colours::textFaint);
        g.drawText ("Version " + version, juce::Rectangle<int> (24, getHeight() - 34, 180, 16), Justification::centredLeft);

        auto content = juce::Rectangle<int> (250, 28, getWidth() - 280, getHeight() - 100);
        g.setFont (LNF::displayFont (22.0f)); g.setColour (colours::text);
        g.drawText (title, content.removeFromTop (38), Justification::centredLeft);
        content.removeFromTop (8);
        AttributedString as; as.append (body, LNF::font (13.5f), colours::text.withAlpha (0.85f));
        TextLayout tl; tl.createLayout (as, (float) content.getWidth());
        tl.draw (g, content.removeFromTop ((int) tl.getHeight() + 4).toFloat());

        if (page == 1)
        {
            g.setFont (LNF::font (10.5f, true)); g.setColour (colours::textDim);
            g.drawText ("APPLICATION FOLDER", 252, 352, 400, 18, Justification::centredLeft);
            g.drawText ("VST3 PLUG-IN FOLDER", 252, 426, 400, 18, Justification::centredLeft);
        }
        if (page == 2 || page == 3)
        {
            const ScopedLock sl (statusLock);
            auto bar = juce::Rectangle<float> (250, 190, (float) getWidth() - 280, 22);
            LNF::drawRecess (g, bar, 9.0f);
            auto fill = bar.reduced (3).withWidth ((bar.getWidth() - 6) * (float) progress);
            ColourGradient pg (colours::goldLight, fill.getX(), fill.getY(), colours::goldDark, fill.getX(), fill.getBottom(), false);
            g.setGradientFill (pg); g.fillRoundedRectangle (fill, 6.0f);
            g.setFont (LNF::font (12.0f)); g.setColour (colours::textDim);
            g.drawFittedText (status, juce::Rectangle<int> (250, 222, getWidth() - 280, 50), Justification::topLeft, 2);
        }
        g.setColour (colours::border); g.drawLine (230, (float) getHeight() - 72, (float) getWidth() - 20, (float) getHeight() - 72, 1.0f);
    }

    void resized() override
    {
        const int h = 36, colB = 250 + (getWidth() - 280) / 2;
        standaloneTgl.setBounds (250, 214, colB - 260, h);
        vstTgl.setBounds (colB, 214, getWidth() - 30 - colB, h);
        desktopTgl.setBounds (250, 258, colB - 260, h);
        startTgl.setBounds (colB, 258, getWidth() - 30 - colB, h);
        manualTgl.setBounds (250, 302, colB - 260, h);
        appDirEd.setBounds (250, 374, getWidth() - 400, h);
        browseApp.setBounds (getWidth() - 140, 374, 110, h);
        vstDirEd.setBounds (250, 448, getWidth() - 400, h);
        browseVst.setBounds (getWidth() - 140, 448, 110, h);
        launchTgl.setBounds (250, 330, 360, h);
        openManualTgl.setBounds (250, 374, 360, h);
        cancelBtn.setBounds (getWidth() - 410, getHeight() - 56, 120, 40);
        backBtn.setBounds (getWidth() - 280, getHeight() - 56, 120, 40);
        nextBtn.setBounds (getWidth() - 150, getHeight() - 56, 130, 40);
    }

#if CHANGES_SETUP_SNAPSHOT
public:
    void showPageForTest (int p, bool ok)
    {
        result = ok ? Result::ok() : Result::fail ("Could not replace C:\\Program Files\\Common Files\\VST3\\Changes.vst3 (is it in use by a DAW?)");
        progress = p == 2 ? 0.45 : 1.0;
        status = p == 2 ? "Installing C:\\Program Files\\JazzyT Audio\\Changes\\Changes.exe" : "Done";
        setPage (p);
    }
#endif

private:
    ChangesLookAndFeel lnf;
    const Payload& payload;
    bool uninstall;
    InstallOptions opts;
    int page = 0;
    String title, body, status;
    double progress = 0;
    std::unique_ptr<FileChooser> chooser;
    ToggleButton standaloneTgl { "Standalone application" }, vstTgl { "VST3 plug-in" }, desktopTgl { "Desktop shortcut" },
                 startTgl { "Start menu shortcuts" }, manualTgl { "Manual (PDF)" }, launchTgl { "Launch Changes now" },
                 openManualTgl { "Open the manual" };
    TextEditor appDirEd, vstDirEd;
    TextButton browseApp { "BROWSE" }, browseVst { "BROWSE" }, backBtn { "BACK" }, nextBtn { "NEXT" }, cancelBtn { "CANCEL" };
    std::unique_ptr<Thread> worker;
    Result result = Result::ok();
    std::atomic<bool> finished { false };
    CriticalSection statusLock;
    void setStatus (double p, const String& m) { const ScopedLock sl (statusLock); progress = p; status = m; }
public:
    bool isBusy() const { return page == 2; }
private:

    void browse (TextEditor& ed)
    {
        chooser = std::make_unique<FileChooser> ("Choose a folder", File (ed.getText()));
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
                              [&ed] (const FileChooser& fc) { if (fc.getResult() != File()) ed.setText (fc.getResult().getFullPathName()); });
    }

    void setPage (int p)
    {
        page = p;
        for (auto* c : getChildren()) c->setVisible (false);
        cancelBtn.setVisible (p < 2);
        nextBtn.setVisible (true);
        backBtn.setVisible (p == 1);
        if (uninstall)
        {
            if (p == 0) { title = "Uninstall Changes"; body = "This removes the Changes application, the VST3 plug-in, shortcuts and the Apps & features entry. Your saved palettes and exported MIDI files in Documents are not touched.\n\nClose any DAW that is using the plug-in before continuing."; nextBtn.setButtonText ("UNINSTALL"); }
            else if (p == 2) { title = "Removing Changes"; body = {}; nextBtn.setVisible (false); }
            else { title = result.wasOk() ? "Changes has been removed" : "Uninstall problem"; body = result.wasOk() ? "Thank you for playing Changes." : result.getErrorMessage(); nextBtn.setButtonText ("CLOSE"); }
        }
        else if (p == 0)
        {
            title = "Welcome to Changes " + version;
            body = "Changes is a playable jazz harmony instrument: one key per chord in the left hand, an always-in-key melody in the right, and music theory from beginner to conservatory level.\n\n"
                   "Setup will install the standalone application and the VST3 plug-in for your DAW. It needs administrator rights to write to Program Files.\n\n"
                   "By installing you agree that this software is provided as-is, without warranty of any kind.";
            nextBtn.setButtonText ("NEXT");
        }
        else if (p == 1)
        {
            title = "Choose components";
            body = "Select what to install. The VST3 plug-in goes into the standard folder that every Windows DAW scans.";
            for (auto* c : std::initializer_list<Component*> { &standaloneTgl, &vstTgl, &desktopTgl, &startTgl, &manualTgl, &appDirEd, &vstDirEd, &browseApp, &browseVst })
                c->setVisible (true);
            nextBtn.setButtonText ("INSTALL");
        }
        else if (p == 2) { title = "Installing"; body = {}; nextBtn.setVisible (false); }
        else
        {
            title = result.wasOk() ? "Installation complete" : "Installation failed";
            body = result.wasOk() ? "Changes is ready.\n\nIn your DAW, rescan plug-ins and load \"Changes\" from JazzyT Audio as an instrument. The standalone app is in the Start menu."
                                  : result.getErrorMessage();
            launchTgl.setVisible (result.wasOk() && opts.standalone);
            openManualTgl.setVisible (result.wasOk() && opts.manual);
            nextBtn.setButtonText ("FINISH");
        }
        resized();
        repaint();
    }

    void next()
    {
        if (page == 0 && ! uninstall) { setPage (1); return; }
        if ((page == 1 && ! uninstall) || (page == 0 && uninstall))
        {
            opts.appDir = File (appDirEd.getText().trim());
            opts.vst3Dir = File (vstDirEd.getText().trim());
            opts.standalone = standaloneTgl.getToggleState(); opts.vst3 = vstTgl.getToggleState();
            opts.desktop = desktopTgl.getToggleState(); opts.startMenu = startTgl.getToggleState(); opts.manual = manualTgl.getToggleState();
            setPage (2);
            struct Worker : Thread
            {
                SetupComponent& s; Worker (SetupComponent& o) : Thread ("setup"), s (o) {}
                void run() override
                {
                    if (s.uninstall) s.result = performUninstall ([this] (const String& m) { s.setStatus (jmin (0.95, s.progress + 0.15), m); });
                    else s.result = performInstall (s.payload, s.opts, [this] (double p, const String& m) { s.setStatus (p, m); });
                    s.setStatus (1.0, s.result.wasOk() ? "Done" : s.result.getErrorMessage());
                    s.finished = true;
                }
            };
            worker = std::make_unique<Worker> (*this);
            worker->startThread();
            startTimerHz (30);
            return;
        }
        if (page == 3)
        {
            if (! uninstall && result.wasOk())
            {
                if (launchTgl.isVisible() && launchTgl.getToggleState()) openUnelevated (opts.appDir.getChildFile ("Changes.exe"));
                if (openManualTgl.isVisible() && openManualTgl.getToggleState()) openUnelevated (opts.appDir.getChildFile ("Changes Manual.pdf"));
            }
            JUCEApplicationBase::quit();
        }
    }

    void timerCallback() override
    {
        repaint();
        if (finished.load())
        {
            stopTimer();
            worker->stopThread (1000);
            setPage (3);
        }
    }
};

//==============================================================================
class SetupApp : public JUCEApplication
{
public:
    const String getApplicationName() override { return "Changes Setup"; }
    const String getApplicationVersion() override { return version; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const String& cmd) override
    {
        ChangesLookAndFeel::setTextSize (ChangesLookAndFeel::textLarge);
        const bool hasPayload = payload.locate();
        const bool uninstall = cmd.containsIgnoreCase ("/uninstall") || ! hasPayload;
        const bool silent = cmd.contains ("/S");

        if (silent)
        {
            Result r = Result::ok();
            if (uninstall) r = performUninstall ([] (const String&) {});
            else
            {
                InstallOptions o;
                if (cmd.contains ("/D="))
                    o.appDir = File (cmd.fromFirstOccurrenceOf ("/D=", false, false).unquoted().trim());
                r = performInstall (payload, o, [] (double, const String&) {});
            }
            File::getSpecialLocation (File::tempDirectory).getChildFile ("ChangesSetup.log")
                .replaceWithText ((uninstall ? "uninstall: " : "install: ") + (r.wasOk() ? String ("OK") : r.getErrorMessage()));
            setApplicationReturnValue (r.wasOk() ? 0 : 1);
            quit();
            return;
        }
        window = std::make_unique<Window> (payload, uninstall);
    }

    void shutdown() override { window.reset(); }
    void systemRequestedQuit() override { quit(); }

private:
    struct Window : DocumentWindow
    {
        Window (const Payload& p, bool uninstall)
            : DocumentWindow (uninstall ? "Uninstall Changes" : "Changes " + version + " Setup", colours::bg0, DocumentWindow::closeButton)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new SetupComponent (p, uninstall), true);
            centreWithSize (getWidth(), getHeight());
            setResizable (false, false);
            setVisible (true);
        }
        void closeButtonPressed() override
        {
            if (auto* c = dynamic_cast<SetupComponent*> (getContentComponent()); c != nullptr && c->isBusy()) return;
            JUCEApplicationBase::quit();
        }
    };
    Payload payload;
    std::unique_ptr<Window> window;
};

#if CHANGES_SETUP_SNAPSHOT
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    ChangesLookAndFeel::setTextSize (ChangesLookAndFeel::textLarge);
    const File out (argc > 1 ? String (argv[1]) : String ("."));
    Payload p;
    struct Shot { bool uninstall; int page; bool ok; const char* name; };
    for (auto s : { Shot { false, 0, true, "setup_0_welcome.png" }, Shot { false, 1, true, "setup_1_components.png" },
                    Shot { false, 2, true, "setup_2_installing.png" }, Shot { false, 3, true, "setup_3_done.png" },
                    Shot { false, 3, false, "setup_3_failed.png" }, Shot { true, 0, true, "setup_uninstall.png" } })
    {
        SetupComponent c (p, s.uninstall);
        if (s.page > 0 || s.uninstall) c.showPageForTest (s.page, s.ok);
        Image img = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
        File f = out.getChildFile (s.name); f.deleteFile();
        FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os);
    }
    return 0;
}
#else
START_JUCE_APPLICATION (SetupApp)
#endif
