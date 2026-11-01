"""Builds 'Changes Manual.pdf' - installation and operating instructions."""
import sys, os
from reportlab.lib.pagesizes import letter
from reportlab.lib.units import inch
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_LEFT, TA_CENTER
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Image,
                                Table, TableStyle, PageBreak, KeepTogether, NextPageTemplate)
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

RES = sys.argv[1]       # Resources dir (fonts)
SHOTS = sys.argv[2]     # screenshots dir
OUT = sys.argv[3]
MAC = len(sys.argv) > 4 and sys.argv[4].lower() == "mac"
COPYKEY = "Cmd" if MAC else "Ctrl"

pdfmetrics.registerFont(TTFont("Pop", os.path.join(RES, "Poppins-Regular.ttf")))
pdfmetrics.registerFont(TTFont("PopM", os.path.join(RES, "Poppins-Medium.ttf")))
pdfmetrics.registerFont(TTFont("PopB", os.path.join(RES, "Poppins-Bold.ttf")))
pdfmetrics.registerFontFamily("Pop", normal="Pop", bold="PopM", italic="Pop", boldItalic="PopM")

NAVY = colors.HexColor("#111827"); GOLD = colors.HexColor("#b07a22"); GOLDL = colors.HexColor("#e2a94b")
BLUE = colors.HexColor("#1b6fa8"); INK = colors.HexColor("#1d2333"); DIM = colors.HexColor("#5b6680"); PALE = colors.HexColor("#f4efe4")

body = ParagraphStyle("body", fontName="Pop", fontSize=9.6, leading=14.2, textColor=INK, spaceAfter=6)
small = ParagraphStyle("small", parent=body, fontSize=8.4, leading=12, textColor=DIM)
h1 = ParagraphStyle("h1", fontName="PopB", fontSize=20, leading=26, textColor=NAVY, spaceBefore=4, spaceAfter=10)
h2 = ParagraphStyle("h2", fontName="PopB", fontSize=13, leading=18, textColor=GOLD, spaceBefore=12, spaceAfter=5)
h3 = ParagraphStyle("h3", fontName="PopM", fontSize=10.5, leading=15, textColor=NAVY, spaceBefore=8, spaceAfter=3)
bullet = ParagraphStyle("bullet", parent=body, leftIndent=14, bulletIndent=4, spaceAfter=3)
cell = ParagraphStyle("cell", parent=body, fontSize=8.6, leading=12, spaceAfter=0)
cellb = ParagraphStyle("cellb", parent=cell, fontName="PopM")
note = ParagraphStyle("note", parent=body, backColor=colors.HexColor("#fbf3e3"), borderColor=GOLDL, borderWidth=0.8,
                      borderPadding=7, leftIndent=7, rightIndent=7, spaceBefore=6, spaceAfter=12)

def P(t, s=body): return Paragraph(t, s)
def B(items): return [Paragraph(i, bullet, bulletText="\u2022") for i in items]
def N(items): return [Paragraph(i, bullet, bulletText=f"{n}.") for n, i in enumerate(items, 1)]

def table(rows, widths, header=True):
    data = [[Paragraph(str(c), cellb if (header and r == 0) else cell) for c in row] for r, row in enumerate(rows)]
    t = Table(data, colWidths=widths, repeatRows=1 if header else 0)
    st = [("VALIGN", (0, 0), (-1, -1), "TOP"), ("GRID", (0, 0), (-1, -1), 0.4, colors.HexColor("#d9d3c4")),
          ("TOPPADDING", (0, 0), (-1, -1), 4), ("BOTTOMPADDING", (0, 0), (-1, -1), 4)]
    if header: st += [("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#efe6d2"))]
    for r in range(1 if header else 0, len(rows)):
        if r % 2 == 0: st.append(("BACKGROUND", (0, r), (-1, r), colors.HexColor("#faf8f3")))
    t.setStyle(TableStyle(st)); return t

def shot(name, w=7.0 * inch, caption=None):
    path = os.path.join(SHOTS, name)
    from PIL import Image as PI
    iw, ih = PI.open(path).size
    out = [Image(path, width=w, height=w * ih / iw)]
    if caption: out.append(P(caption, ParagraphStyle("cap", parent=small, alignment=TA_CENTER, spaceBefore=3)))
    return out

def on_page(c, doc):
    c.saveState()
    w, h = letter
    c.setFillColor(NAVY); c.rect(0, h - 0.42 * inch, w, 0.42 * inch, fill=1, stroke=0)
    c.setFillColor(GOLDL); c.setFont("PopB", 10); c.drawString(0.75 * inch, h - 0.28 * inch, "CHANGES")
    c.setFillColor(colors.white); c.setFont("Pop", 8.5); c.drawRightString(w - 0.75 * inch, h - 0.28 * inch, "Installation & Operating Instructions  |  v1.0")
    c.setFillColor(DIM); c.setFont("Pop", 8); c.drawCentredString(w / 2, 0.45 * inch, f"Page {doc.page}")
    c.restoreState()

def on_cover(c, doc):
    w, h = letter
    c.saveState()
    c.setFillColor(NAVY); c.rect(0, 0, w, h, fill=1, stroke=0)
    c.setFillColor(colors.HexColor("#1a2438")); c.rect(0, 0, w, h * 0.42, fill=1, stroke=0)
    for i, (x, hh) in enumerate([(0.9, 0.7), (1.55, 1.05), (2.2, 1.4)]):
        c.setFillColor([GOLDL, GOLD, colors.HexColor("#8f5f1c")][i]); c.roundRect(x * inch, h * 0.42 + 0.0, 0.5 * inch, hh * inch, 6, fill=1, stroke=0)
    c.setFillColor(GOLDL); c.setFont("PopB", 54); c.drawString(0.85 * inch, h - 2.6 * inch, "CHANGES")
    c.setFillColor(colors.HexColor("#c9cfdb")); c.setFont("PopM", 13); c.drawString(0.9 * inch, h - 3.0 * inch, ("JAZZ HARMONY INSTRUMENT  -  AU, VST3 & APP FOR macOS" if MAC else "JAZZ HARMONY INSTRUMENT  -  VST3 & STANDALONE FOR WINDOWS"))
    c.setFillColor(colors.white); c.setFont("PopB", 22); c.drawString(0.9 * inch, h - 4.0 * inch, "Installation & Operating Instructions")
    c.setFont("Pop", 11); c.setFillColor(colors.HexColor("#c9cfdb"))
    c.drawString(0.9 * inch, h - 4.35 * inch, "Version 1.0.0  |  JazzyT Audio")
    c.drawImage(os.path.join(SHOTS, "gui_intermediate.png"), 3.1 * inch, 0.7 * inch, width=4.9 * inch, height=4.9 * inch * 860 / 1280)
    c.restoreState()

doc = BaseDocTemplate(OUT, pagesize=letter, leftMargin=0.75 * inch, rightMargin=0.75 * inch, topMargin=0.75 * inch,
                      bottomMargin=0.7 * inch, title="Changes - Installation & Operating Instructions", author="JazzyT Audio",
                      subject="Changes jazz harmony instrument manual")
frame = Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="f")
doc.addPageTemplates([PageTemplate(id="cover", frames=[frame], onPage=on_cover),
                      PageTemplate(id="body", frames=[frame], onPage=on_page)])
S = [NextPageTemplate("body"), PageBreak()]

# ---------------------------------------------------------------- contents
S += [P("Contents", h1)]
toc = [("1", "Welcome"), ("2", "System requirements"), ("3", "Installing Changes"), ("4", "Using the plug-in in your DAW"),
       ("5", "The standalone application"), ("6", "Quick start - your first ii-V-I"), ("7", "The interface, section by section"),
       ("8", "Theory levels"), ("9", "Voicing styles"), ("10", "The right hand: melody modes & harmonizing"),
       ("11", "Progressions, comping and MIDI export"), ("12", "Editing and saving palettes"), ("13", "Keyboard shortcuts"),
       ("14", "Troubleshooting"), ("15", "Uninstalling"), ("16", "Credits & licences")]
S += [table([[n, t] for n, t in toc], [0.5 * inch, 6.4 * inch], header=False), PageBreak()]

# ---------------------------------------------------------------- 1
S += [P("1  Welcome", h1),
      P("<b>Changes</b> is a playable jazz harmony instrument. Your <b>left hand plays one key per chord</b> - each of the 24 chord pads "
        "holds a complete, professionally voiced jazz chord - while your <b>right hand plays melody that is automatically kept in the "
        "right scale</b> for whatever chord is sounding. Four theory levels take you from simple seventh chords to the upper-structure "
        "triads, altered dominants and Coltrane relationships taught in conservatories, and every feature can be switched on or off."),
      P("Everything you hear is generated by a real music-theory engine and synthesized in real time - nothing is a fixed recording - "
        "so the voicings follow your key, level, style and voice-leading settings exactly.")]
S += B(["24 chord pads per palette, 8 built-in teaching palettes, fully editable (drag to swap, right-click to edit, save/load).",
        "Voice leading that moves each note the shortest distance, like a real pianist.",
        "Right-hand melody snapped to the chord-scale (Dorian, Mixolydian, Altered, Lydian Dominant ...), pentatonic, blues or bebop scales.",
        "'NEXT' suggestions that light up strong follow-on chords, and a modulation helper with pivot chords.",
        "Progression strip with record, loop playback in six comping rhythms, MIDI export and drag-and-drop MIDI into your DAW.",
        "Five built-in instruments (electric piano, grand, organ, vibraphone, pad), strum/cascade, humanize, chorus, reverb, tremolo.",
        "MIDI output of every voiced chord and melody note, so Changes can drive any other instrument."])

if not MAC:
    # ---------------------------------------------------------------- 2
    S += [P("2  System requirements", h1),
          table([["Item", "Requirement"],
                 ["Operating system", "Windows 10 or Windows 11, 64-bit"],
                 ["Processor", "Any x64 CPU from the last ten years (Changes uses about 1-3% of one core)"],
                 ["Plug-in host", "Any 64-bit DAW that loads VST3 instruments: FL Studio, Ableton Live 10+, Reaper, Cubase, Studio One, Bitwig, Cakewalk, etc."],
                 ["Disk space", "About 40 MB"],
                 ["Optional", "A USB/MIDI keyboard (49 keys or more is ideal). The computer keyboard and mouse also work."]],
                [1.6 * inch, 5.3 * inch])]

    # ---------------------------------------------------------------- 3
    S += [P("3  Installing Changes", h1), P("What is in the package", h2),
          table([["File", "What it is"],
                 ["Changes-1.0.0-Setup.exe", "The installer. Installs the standalone app, the VST3 plug-in, shortcuts and this manual."],
                 ["Changes Manual.pdf", "This document."],
                 ["README.txt", "Short installation notes."],
                 ["Portable\\Changes.exe", "The standalone app, runnable without installing."],
                 ["Portable\\Changes.vst3", "The VST3 plug-in bundle, for manual installation."]],
                [2.1 * inch, 4.8 * inch]),
          P("Running the installer", h2)]
    S += N(["Double-click <b>Changes-1.0.0-Setup.exe</b>.",
            "Windows asks <i>'Do you want to allow this app to make changes to your device?'</i> - click <b>Yes</b>. Administrator rights are needed to write to Program Files.",
            "If a blue <b>'Windows protected your PC'</b> SmartScreen box appears, click <b>More info</b> and then <b>Run anyway</b>. This appears for any new program that is not code-signed with a paid certificate.",
            "On the welcome page click <b>NEXT</b>.",
            "Choose components: <b>Standalone application</b>, <b>VST3 plug-in</b>, <b>Desktop shortcut</b>, <b>Start menu shortcuts</b> and <b>Manual</b> are all ticked by default. The folders can be changed with <b>BROWSE</b>, but the defaults are recommended.",
            "Click <b>INSTALL</b>. When the bar is full click <b>FINISH</b> (optionally ticking <i>Launch Changes now</i>)."])
    S += [P("Where files are installed", h2),
          table([["Component", "Location"],
                 ["Standalone app", "C:\\Program Files\\JazzyT Audio\\Changes\\Changes.exe"],
                 ["VST3 plug-in", "C:\\Program Files\\Common Files\\VST3\\Changes.vst3"],
                 ["Manual & uninstaller", "C:\\Program Files\\JazzyT Audio\\Changes\\"],
                 ["Shortcuts", "Desktop, and Start menu > Changes"],
                 ["Your settings", "%APPDATA%\\JazzyT Audio\\Changes\\"]],
                [1.6 * inch, 5.3 * inch]),
          P("Silent install (for advanced users)", h3),
          P("<font name='PopM'>Changes-1.0.0-Setup.exe /S</font> installs with all defaults and no windows (run from an administrator command prompt). "
            "Add <font name='PopM'>/D=C:\\Path\\To\\Folder</font> to choose the application folder. The result is written to "
            "<font name='PopM'>%TEMP%\\ChangesSetup.log</font>."),
          P("Manual installation (no installer)", h3),
          P("Copy the <b>Portable\\Changes.vst3</b> folder into <font name='PopM'>C:\\Program Files\\Common Files\\VST3\\</font>, and run "
            "<b>Portable\\Changes.exe</b> from anywhere. Nothing else is required.")]

    # ---------------------------------------------------------------- 4
    S += [P("4  Using the plug-in in your DAW", h1),
          P("Changes is a <b>VST3 instrument</b> (vendor: JazzyT Audio). After installing, let your DAW rescan its plug-ins, then add Changes "
            "to an instrument / MIDI track and arm the track so your MIDI keyboard reaches it."),
          table([["DAW", "How to find Changes"],
                 ["FL Studio", "Options > Manage plugins > Find installed plugins (Start scan). Then Channel rack > + > Changes."],
                 ["Ableton Live", "Settings > Plug-Ins > enable 'Use VST3 Plug-In System Folders' > Rescan. Browser > Plug-Ins > VST3 > JazzyT Audio > Changes."],
                 ["Reaper", "Options > Preferences > Plug-ins > VST > Re-scan. Insert > Virtual instrument on new track > Changes."],
                 ["Cubase / Nuendo", "Studio > VST Plug-in Manager > Update. Add an Instrument Track and choose JazzyT Audio > Changes."],
                 ["Studio One", "Options > Locations > VST Plug-ins > Reset Blocklist / rescan. Browser > Instruments > Changes."],
                 ["Bitwig Studio", "Settings > Locations - the VST3 system folder is scanned automatically. Device browser > Changes."]],
                [1.3 * inch, 5.6 * inch]),
          P("Recording what Changes plays (MIDI out)", h2),
          P("With <b>MIDI Out</b> on, Changes sends every voiced chord on <b>MIDI channel 1</b> and every melody note on <b>channel 2</b> to "
            "its MIDI output. Hosts that support VST3 MIDI output (Reaper, Cubase, Bitwig, Studio One, FL Studio via a MIDI-out port) can record "
            "it or route it to another instrument. Turn <b>Internal</b> off to use Changes purely as a chord generator."),
          P("The simplest way to get the voiced chords into any DAW (including Ableton Live, whose VST3 hosting has no MIDI output) is the "
            "progression strip: build a progression and <b>drag the DRAG MIDI handle onto a MIDI track</b> - see section 11.", note),
          P("Tempo", h3),
          P("Inside a DAW the progression player follows the host tempo automatically (a teal <b>HOST SYNC</b> label appears above the Tempo knob).")]

    # ---------------------------------------------------------------- 5
    S += [P("5  The standalone application", h1),
          P("Start <b>Changes</b> from the desktop or Start menu. The first time, open <b>Options</b> (top-left of the window) > "
            "<b>Audio/MIDI Settings</b>:")]
    S += B(["<b>Audio device type</b>: choose <i>Windows Audio (Exclusive Mode)</i> or <i>Windows Audio (Low Latency Mode)</i> for the lowest latency, or <i>DirectSound</i> for maximum compatibility.",
            "<b>Output</b>: your speakers / headphones / audio interface.",
            "<b>Audio buffer size</b>: 256 samples (about 5 ms) is a good start; raise it if you hear crackles.",
            "<b>Active MIDI inputs</b>: tick your MIDI keyboard."])
    S += [P("No MIDI keyboard? Use the computer keyboard (Z S X D C V G B H N J M for chords, Q 2 W 3 E R 5 T 6 Y 7 U I 9 O 0 P for "
            "melody) or click the pads and the on-screen keyboard. Click once on the Changes window so it has keyboard focus.", note),
          P("The window can be resized by dragging its bottom-right corner; the whole interface scales smoothly. Settings are remembered "
            "between sessions (Options > Save current state to a file also works).")]

else:
    # ---------------------------------------------------------------- 2 (mac)
    S += [P("2  System requirements", h1),
          table([["Item", "Requirement"],
                 ["Operating system", "macOS 10.13 High Sierra or later, including macOS 14 Sonoma and macOS 15 Sequoia"],
                 ["Processor", "Apple Silicon (M1 and later) or Intel - Changes is a universal app and runs natively on both"],
                 ["Plug-in host", "Audio Unit: Logic Pro, GarageBand, MainStage. VST3: Ableton Live 10+, Cubase, Reaper, Studio One, Bitwig, FL Studio"],
                 ["Disk space", "About 60 MB"],
                 ["Optional", "A USB/MIDI keyboard (49 keys or more is ideal). The computer keyboard and trackpad/mouse also work."]],
                [1.6 * inch, 5.3 * inch])]

    # ---------------------------------------------------------------- 3 (mac)
    S += [P("3  Installing Changes", h1), P("What is in the disk image", h2),
          table([["File", "What it is"],
                 ["Install Changes.pkg", "The installer. Installs the app, the Audio Unit, the VST3 plug-in, this manual and an uninstaller."],
                 ["Changes Manual.pdf", "This document."],
                 ["README.txt", "Short installation notes."],
                 ["Uninstall Changes.command", "Removes everything the installer added."]],
                [2.1 * inch, 4.8 * inch]),
          P("Running the installer", h2)]
    S += N(["Double-click <b>Changes-1.0.0-Mac.dmg</b> to open it, then double-click <b>Install Changes.pkg</b>.",
            "Changes is not notarized by Apple (that needs a paid developer account), so macOS may say it <i>cannot be opened because Apple cannot check it for malicious software</i>. Click <b>Done</b>, open <b>System Settings > Privacy &amp; Security</b>, scroll down and click <b>Open Anyway</b> next to <i>Install Changes.pkg</i>, then confirm with your password or Touch ID. "
            "(On macOS 14 and earlier you can instead Control-click the package, choose <b>Open</b>, then <b>Open</b> again.)",
            "Click <b>Continue</b> through the introduction, read and <b>Agree</b> to the licence.",
            "On <i>Installation Type</i> click <b>Install</b>, or <b>Customize</b> to choose components (App, Audio Unit, VST3, Manual).",
            "Enter your Mac password when asked - plug-ins are installed for all users. When it says <i>The installation was successful</i>, click <b>Close</b>."])
    S += [P("Where files are installed", h2),
          table([["Component", "Location"],
                 ["Standalone app", "/Applications/Changes.app"],
                 ["Audio Unit", "/Library/Audio/Plug-Ins/Components/Changes.component"],
                 ["VST3 plug-in", "/Library/Audio/Plug-Ins/VST3/Changes.vst3"],
                 ["Manual &amp; uninstaller", "/Library/Application Support/JazzyT Audio/Changes/"],
                 ["Your settings", "~/Library/Application Support/JazzyT Audio/Changes/"]],
                [1.6 * inch, 5.3 * inch]),
          P("If the app itself is blocked the first time", h3),
          P("Because Changes is unsigned, the first launch of <b>Changes.app</b> may also be blocked. Use the same <b>System Settings > Privacy &amp; Security > Open Anyway</b> button once; after that it opens normally. Plug-ins loaded by your DAW are not affected.")]

    # ---------------------------------------------------------------- 4 (mac)
    S += [P("4  Using the plug-in in your DAW", h1),
          P("Changes is installed as an <b>Audio Unit instrument</b> and a <b>VST3 instrument</b> (manufacturer: JazzyT Audio). Add it to an "
            "instrument track and arm the track so your MIDI keyboard reaches it."),
          table([["DAW", "How to find Changes"],
                 ["Logic Pro", "New Software Instrument track > Instrument slot > AU Instruments > JazzyT Audio > Changes. If it is missing: Logic Pro > Settings > Plug-in Manager > Reset &amp; Rescan Selection."],
                 ["GarageBand", "New Software Instrument track > Smart Controls (B) > Plug-ins > Instrument slot > AU Instruments > JazzyT Audio > Changes."],
                 ["MainStage", "Software Instrument channel strip > Instrument slot > AU Instruments > JazzyT Audio > Changes."],
                 ["Ableton Live", "Settings > Plug-Ins > turn on 'Use Audio Units v2' or 'Use VST3 Plug-in System Folders' > Rescan. Browser > Plug-Ins > JazzyT Audio > Changes."],
                 ["Reaper", "Settings > Plug-ins > VST/AU > Re-scan. Insert > Virtual instrument on new track > Changes."],
                 ["Cubase / Studio One / Bitwig", "Rescan plug-ins in the plug-in manager, then add Changes (VST3) on an instrument track."],
                 ["FL Studio (Mac)", "Options > Manage plugins > Find installed plugins. Channel rack > + > Changes."]],
                [1.6 * inch, 5.3 * inch]),
          P("Recording what Changes plays (MIDI out)", h2),
          P("With <b>MIDI Out</b> on, Changes sends every voiced chord on <b>MIDI channel 1</b> and every melody note on <b>channel 2</b>. "
            "Hosts that accept MIDI from instrument plug-ins (Reaper, Cubase, Bitwig, Studio One, Logic via the MIDI FX / instrument output where supported) can record it. "
            "Turn <b>Internal</b> off to use Changes purely as a chord generator."),
          P("The simplest way to get the voiced chords into any DAW, including Logic, GarageBand and Ableton Live, is the progression strip: "
            "build a progression and <b>drag the DRAG MIDI handle onto a track or region</b> - see section 11.", note),
          P("Tempo", h3),
          P("Inside a DAW the progression player follows the host tempo automatically (a teal <b>HOST SYNC</b> label appears above the Tempo knob).")]

    # ---------------------------------------------------------------- 5 (mac)
    S += [P("5  The standalone application", h1),
          P("Open <b>Changes</b> from Applications or Launchpad. The first time, open <b>Options</b> (top-left of the window) > "
            "<b>Audio/MIDI Settings</b>:")]
    S += B(["<b>Output</b>: your Mac's speakers, headphones or audio interface (Core Audio).",
            "<b>Audio buffer size</b>: 256 samples (about 5 ms) is a good start; raise it if you hear crackles.",
            "<b>Active MIDI inputs</b>: tick your MIDI keyboard. Bluetooth MIDI keyboards are paired in Audio MIDI Setup > Window > Show MIDI Studio > Bluetooth."])
    S += [P("No MIDI keyboard? Use the computer keyboard (Z S X D C V G B H N J M for chords, Q 2 W 3 E R 5 T 6 Y 7 U I 9 O 0 P for "
            "melody) or click the pads and the on-screen keyboard. Click once on the Changes window so it has keyboard focus.", note),
          P("The window can be resized by dragging its bottom-right corner; the whole interface scales smoothly and looks sharp on Retina "
            "displays. Settings are remembered between sessions. On a trackpad, a two-finger click (or Control-click) works wherever this manual says right-click.")]

# ---------------------------------------------------------------- 6
S += [P("6  Quick start - your first ii-V-I", h1)]
S += N(["Set <b>KEY</b> to C, <b>MAJOR</b>, and level <b>INTERMEDIATE</b>.",
        "Play the pad labelled <b>Dm9</b> (MIDI note D2, computer key X). The display shows <i>Dm9 - ii7 - Pre-dominant</i>, its notes and degrees.",
        "Notice the teal <b>NEXT</b> outlines: <b>G9</b> lights up. Play it (G2, key B), then <b>Cmaj9</b> (C2, key Z). Listen to how little each note moves - that is voice leading.",
        "Hold a chord and play melody anywhere above B3 (or keys Q-P). Every note fits - the right hand follows <i>D Dorian</i>, then <i>G Mixolydian</i>, then <i>C Ionian</i>.",
        "Switch the level to <b>CONSERVATORY</b> and play the same three pads: you now hear quartal 'So What' voicings, upper-structure triads and drop-2 spreads.",
        "Arm <b>REC</b>, play a few chords, press <b>PLAY</b> and improvise over your own loop."])
S += shot("gui_intermediate.png", caption="Changes at Intermediate level, playing Dm9 (ii7) in C major.")

# ---------------------------------------------------------------- 7
S += [PageBreak(), P("7  The interface, section by section", h1),
      P("Hover over any control and the <b>help bar</b> at the bottom of the window explains what it does. Pop-up tooltips can be turned "
        "on and off with the <b>?</b> button.")]
S += [P("Text size", h2),
      P("Open <b>MENU > Text size</b> and choose <b>Small</b> (the original compact look), <b>Medium</b> (default) or <b>Large</b>. "
        "Labels, buttons, menus, tooltips, the help bar and the quick start guide all grow; the layout adjusts so nothing overlaps. "
        "The choice is remembered for both the standalone app and the plug-in. You can also make the whole window bigger by dragging "
        "its bottom-right corner (60% to 150%)."),
      P("Header", h2), table([["Control", "Function"],
        ["KEY", "The home key (C to B). Pads are stored relative to the key, so changing it transposes everything."],
        ["MAJOR / MINOR", "Selects the major or minor palette and key scale."],
        ["THEORY LEVEL", "Beginner, Intermediate, Advanced, Conservatory - changes the chords on the pads and how they are voiced (section 8)."],
        ["MODULATE", "Lists related keys (relative minor, up a fourth, Coltrane major-thirds, truck-driver half step ...) with the pivot V7 chord. Choosing one changes the key."],
        ["?", "Pop-up tooltips on/off."],
        ["MENU", "Quick start guide, save/load palette, export MIDI, <b>Text size</b> (Small / Medium / Large), panic, about."]], [1.4 * inch, 5.5 * inch])]
S += [P("Harmony panel (left)", h2), table([["Control", "Function"],
        ["Voicing style", "Auto (chosen by level) or a fixed style: Close, Shell, Rootless A/B, Drop 2, Quartal, Upper-structure, Spread (section 9)."],
        ["Chord register", "Low / Middle / High - moves voicings by an octave."],
        ["Bass Note", "Adds the root (or slash bass) low in the left hand."],
        ["Extensions", "Adds 9ths, 11ths and 13ths allowed by the chord-scale. Off = plain seventh chords at any level."],
        ["Alterations", "Colours dominants: b9/b13 when resolving to minor; fully altered at Conservatory."],
        ["Voice Leading", "Picks the inversion that moves least from the previous chord."],
        ["Auto Reharm", "Replaces every dominant with its tritone substitute (G7 becomes Db7#11)."],
        ["Latch", "Chords keep sounding after you release the key, until the next chord."],
        ["Suggest Next", "Shows teal NEXT outlines on strong follow-on chords."],
        ["Lock Palette", "Keeps your edited pads when changing level or major/minor."],
        ["Palette preset / SAVE / LOAD / RESET", "Load a teaching palette, save or load your own .chgpal files, or restore the preset."]], [1.6 * inch, 5.3 * inch])]
S += [P("Chord display", h2),
      P("Shows the chord name, its Roman numeral in the key, its harmonic function in plain English (e.g. <i>Secondary dominant V7/ii - resolves down a fifth</i>), "
        "the exact notes and their scale degrees (R, 3, b7, 9, #11, 13 ...), the current right-hand scale, and up to four suggested next chords.")]
S += [P("Chord pads", h2),
      P("Two piano-style rows: the lower row is pads 1-12 (MIDI C2-B3 by default), the upper row pads 13-24. White pads hold the "
        "diatonic chords, black pads the colour and substitute chords. Each pad shows the chord name, Roman numeral, computer key and MIDI note. "
        "The playing pad glows gold; suggested pads have a teal outline and dot.")]
S += [P("Progression panel", h2), table([["Control", "Function"],
        ["REC", "While lit, every chord you play from the pads is appended to the progression."],
        ["PLAY", "Loops the progression with the selected comp pattern (Space bar)."],
        ["CLEAR", "Empties the progression."],
        ["EXPORT .MID", "Saves the progression as a standard MIDI file, voiced exactly as you hear it."],
        ["DRAG MIDI", "Drag this handle into a DAW track or a folder to drop the progression as a .mid file."],
        ["Beats", "Length given to newly added chords (1, 2, 3, 4 or 8 beats)."],
        ["Comp pattern", "Sustain, Charleston, 2 & 4 stabs, Push, Freddie Green 4s, Ballad halves."],
        ["TEMPO / SWING", "Player tempo (host tempo in a DAW) and swing ratio of off-beat hits (50% straight - 66% triplet)."]], [1.4 * inch, 5.5 * inch])]
S += [P("Right hand - melody panel", h2), table([["Control", "Function"],
        ["Snap to Scale", "Notes outside the scale move to the nearest scale note."],
        ["White-Key Map", "Every white key plays a scale degree (C = root of the key), so any key can be played on white keys only. Black keys play chord tones."],
        ["Show Scale", "Dots on the on-screen keyboard show the scale (gold dots = chord tones)."],
        ["Scale", "Key scale, Chord scale, Chord tones, Pentatonic, Blues, Bebop (section 10)."],
        ["Harmonize", "Off, Octaves, Diatonic 3rds, Block chords (George Shearing style)."],
        ["Octave", "Transposes the melody by -2 to +2 octaves (also Up/Down arrow keys)."],
        ["Chord zone", "Where the 24 chord keys start: C1, C2 (default) or C3. Notes above the zone are melody."]], [1.4 * inch, 5.5 * inch])]
S += [P("Performance, Sound and Mixer (right)", h2), table([["Control", "Function"],
        ["Strum", "Cascade time: 0 ms strikes all notes together, up to 250 ms for a slow harp-like roll."],
        ["Strum direction", "Up, Down, or Alternate each chord."],
        ["Humanize", "Small natural timing and velocity variations."],
        ["Dynamics", "How much your key velocity matters (0% = even touch)."],
        ["Chords / Melody sound", "Suitcase Electric Piano, Concert Grand, Jazz Organ, Vibraphone, Velvet Pad - chosen separately for each hand."],
        ["Tone, Release", "Brightness and ring-out time of both instruments."],
        ["Chorus, Reverb, Tremolo", "Stereo chorus, room reverb and suitcase-style stereo tremolo."],
        ["Internal / MIDI Out", "Built-in sound on/off; MIDI output on/off."],
        ["Chords / Melody / Master faders", "Volumes with gradient LED meters (green - yellow - red with peak hold)."],
        ["PANIC", "Stops all notes and the player (Esc)."]], [1.6 * inch, 5.3 * inch])]
S += [P("On-screen keyboard and help bar", h2),
      P("The keyboard spans C1-C7. The coloured bars above it mark the chord zone (gold) and the melody zone (blue). Keys light gold for "
        "sounding chord notes and blue for melody notes. Click or drag on it to play. The help bar along the bottom always explains the "
        "control under the mouse.")]

# ---------------------------------------------------------------- 8
S += [PageBreak(), P("8  Theory levels", h1),
      P("The level decides <b>which chords are on the pads</b> (unless Lock Palette is on) and <b>how rich the voicings are</b>. "
        "These are the pads of the default major palette in C:"),
      table([["Pad (white)", "Beginner", "Intermediate", "Advanced", "Conservatory"],
             ["I", "Cmaj7", "Cmaj9", "Cmaj13", "Cmaj13 (drop 2)"],
             ["ii", "Dm7", "Dm9", "Dm7(11) - So What quartal", "Dm7(11) - So What quartal"],
             ["iii", "Em7", "Em7(11)", "Em7(11)", "Em7(11)"],
             ["IV", "Fmaj7", "Fmaj9", "Fmaj13", "Fmaj9#11 / Fmaj13#11 (lydian)"],
             ["V", "G7", "G9", "G13", "G13#11 / G7alt (upper structure)"],
             ["vi", "Am7", "Am9", "Am11", "Am11"],
             ["vii", "Bm7b5", "Bm7b5(11)", "Bm7b5(11)", "Bm9b5 / Bm11b5 (locrian #2)"]],
            [1.0 * inch, 1.2 * inch, 1.3 * inch, 1.5 * inch, 1.9 * inch]),
      Spacer(1, 6),
      table([["Level", "What it teaches", "Colour (black-pad) chords"],
             ["Beginner", "Four-note seventh chords in close position, root on the bottom. Diatonic harmony and the ii-V-I.", "I6, II7 (V/V), iv m7, VI7 (V/ii), bVII7, I7, bIIImaj7, #iv dim7, bVImaj7, iv m6"],
             ["Intermediate", "Bill Evans rootless A/B voicings with 9ths. Secondary dominants, tritone substitute, minor iv.", "bII7 (subV), II7, #iv m7b5, VI7, bVII7, I6/9, I7, III7, iv m6, VII7"],
             ["Advanced", "11ths and 13ths, b9 and altered dominants, quartal minor chords, lydian IV, modal interchange.", "bII13#11, VI7b9, III7b9, IVmaj7#11, V7alt, iv m6/9"],
             ["Conservatory", "Upper-structure triads, drop-2, lydian & altered colours, locrian #2, Neapolitan, Coltrane relations, slash chords.", "bII13#11, VI7alt, bVII13#11 (backdoor), bIImaj7, bIIImaj7#11, III7alt, bVI13#11, iv m(maj7)"]],
            [1.0 * inch, 3.3 * inch, 2.6 * inch]),
      P("Minor keys", h2),
      P("The minor palette uses jazz-minor harmony: i m6 / i m(maj7), ii m7b5, bIIImaj7, iv m7, V7b9 (V7alt at higher levels), bVImaj7, "
        "vii dim7, plus subV, bVII7, v m7 and II7 on the black pads."),
      P("Palette presets", h2),
      table([["Preset", "Contents"],
             ["Level Default", "The palettes above, rebuilt for each level."],
             ["ii-V-I Workshop", "Eight major ii-V-I cadences around the cycle of fourths - play pads in groups of three."],
             ["Minor ii-V-i Workshop", "Eight minor cadences: m7b5 - altered dominant - minor tonic."],
             ["Rhythm Changes", "I-vi-ii-V turnarounds, I7-IV7-#iv dim, and the III7-VI7-II7-V7 bridge."],
             ["Coltrane Changes", "Three tonal centres a major third apart, each with its ii, V, subV and altered V."],
             ["Modal / So What", "Quartal minor 11th chords on every root (pads 1-12) and quartal sus chords (13-24)."],
             ["Jazz & Bird Blues", "Dominant blues chords, #iv diminished and the descending ii-V chains of 'Bird' blues."],
             ["Neo-Soul Colours", "maj9, m11 and 9sus4 colours with quartal options and borrowed chords."]],
            [1.7 * inch, 5.2 * inch])]

# ---------------------------------------------------------------- 9
S += [P("9  Voicing styles", h1),
      table([["Style", "Description", "Example (G7 family)"],
             ["Close position", "Chord tones stacked within an octave. Beginner default.", "B D F G"],
             ["Shell (Bud Powell)", "Root with 3rd and 7th only - the leanest bebop left hand.", "G B F / G F B"],
             ["Rootless A/B (Bill Evans)", "Four notes starting on the 3rd (A) or 7th (B), adding 9 and 13; root left to the bass.", "B E F A / F A B E"],
             ["Drop 2", "Close voicing with the second voice from the top dropped an octave - a wider, guitar-like sound.", "F B E A"],
             ["Quartal / So What", "Stacked fourths, as in Miles Davis's 'So What' and McCoy Tyner.", "F B E A (G13)"],
             ["Upper-structure triads", "Guide tones (3 & 7) under a triad that supplies the tensions.", "F B + A C# E (A/G7 = 13#11)"],
             ["Spread (two-hand)", "Root low, 7th and 10th, tensions on top - a lush two-handed spread.", "G F B A E"]],
            [1.6 * inch, 3.4 * inch, 1.9 * inch]),
      P("Each pad also has four <b>voicing variations</b> (right-click > Voicing variation): 'auto' picks the best voice-leading, the "
        "alternatives give other inversions or register choices, just like a pianist re-voicing the same chord."),
      P("The chord name always describes what is actually sounding - if an upper-structure voicing adds a #11, the name becomes "
        "G13#11 and the right-hand scale switches to Lydian Dominant so melody never clashes.", note)]

# ---------------------------------------------------------------- 10
S += [P("10  The right hand: melody modes & harmonizing", h1),
      table([["Scale mode", "Notes used", "Example over G7 in C"],
             ["Key scale", "The 7 notes of the key.", "C D E F G A B"],
             ["Chord scale", "The jazz chord-scale of the current chord (follows alterations).", "G Mixolydian; G7alt = G Altered"],
             ["Chord tones", "Only the notes of the chord incl. extensions - arpeggios that cannot miss.", "G B D F A E"],
             ["Pentatonic", "A pentatonic suited to the chord quality (minor pent on minor chords, pent from b5 on alt).", "G A B D E"],
             ["Blues", "The blues scale of the key.", "C Eb F Gb G Bb"],
             ["Bebop", "Chord scale plus the bebop passing tone (maj7 on dominants, #5 on major, 3 on minor).", "G A B C D E F F#"]],
            [1.2 * inch, 3.6 * inch, 2.1 * inch]),
      P("<b>White-Key Map</b> makes every key playable on white keys: C plays the root of the key, D the 2nd degree and so on. "
        "<b>Harmonize</b> adds an octave below, a diatonic third below, or Shearing-style block chords (melody on top, chord tones "
        "underneath, melody doubled an octave lower).")]

# ---------------------------------------------------------------- 11
S += [P("11  Progressions, comping and MIDI export", h1)]
S += B(["<b>Add chords</b>: drag a pad onto the strip (drop between chords to insert), double-click a pad, right-click a pad > Add to progression, or arm <b>REC</b> and play.",
        "<b>Edit</b>: click a chord to hear it, drag to reorder, drag a chord onto a pad to store it there, hover and click the red x to delete, right-click to change length (1-8 beats), duplicate or remove.",
        "<b>Play</b>: PLAY (or Space) loops the progression with the comp pattern; keep improvising in the right hand.",
        "<b>Export</b>: EXPORT .MID saves a file; DRAG MIDI drags the same file straight into your DAW. The file contains the exact voicings, strum and comp rhythm at the current tempo (960 PPQ, channel 1)."])
S += [table([["Comp pattern", "Rhythm (per 4/4 bar)"],
             ["Sustain (whole)", "One long chord for the full length."],
             ["Charleston", "Beat 1 short, then the 'and' of 2 held - the classic swing comp."],
             ["2 & 4 stabs", "Short chords on beats 2 and 4."],
             ["Push (anticipation)", "Beat 1, then an anticipation on the 'and' of 3."],
             ["Freddie Green 4s", "Four short quarter notes, big-band guitar style."],
             ["Ballad (half notes)", "Two half notes per bar."]], [1.7 * inch, 5.2 * inch])]

# ---------------------------------------------------------------- 12
S += [P("12  Editing and saving palettes", h1)]
S += B(["<b>Swap</b> two pads by dragging one onto the other. Hold <b>"+COPYKEY+"</b> (or "+("Option" if MAC else "Alt")+") while dropping to <b>copy</b> instead.",
        "<b>Right-click a pad</b> to set its Root (as a scale degree), Quality (maj7, 6, maj7#11, maj7#5, 7, 7sus4, 7alt, 7b9, 7#11, m7, m6, m(maj7), m7b5, dim7), Voicing variation, Voicing style, or Slash bass; copy/paste chords; or reset the pad.",
        "Turn on <b>Lock Palette</b> before changing level if you want to keep your edits.",
        "<b>SAVE</b> writes a .chgpal file; <b>LOAD</b> reads one - you can also drag a .chgpal file onto the window.",
        "Inside a DAW the palette, progression and all settings are saved with your project automatically."])

# ---------------------------------------------------------------- 13
S += [P("13  Keyboard shortcuts", h1),
      table([["Keys", "Action"],
             ["Z S X D C V G B H N J M", "Chord pads 1-12 (or 13-24 after TAB) - laid out like a piano octave"],
             ["Q 2 W 3 E R 5 T 6 Y 7 U I 9 O 0 P", "Melody notes (chromatic from the bottom of the melody zone)"],
             ["TAB", "Switch the computer-keyboard pad bank (1-12 / 13-24)"],
             ["SPACE", "Play / stop the progression"],
             ["ESC", "Panic - all notes off"],
             ["Up / Down arrows", "Melody octave up / down"]], [2.6 * inch, 4.3 * inch]),
      P("In a DAW, computer-keyboard input only reaches Changes when its window has focus and the host passes keystrokes to plug-ins.", small)]

if not MAC:
    # ---------------------------------------------------------------- 14
    S += [P("14  Troubleshooting", h1),
          table([["Problem", "Solution"],
                 ["Windows says 'Windows protected your PC'", "Click More info > Run anyway. The installer is safe but not code-signed."],
                 ["The DAW does not list Changes", "Rescan plug-ins (section 4). Check that C:\\Program Files\\Common Files\\VST3\\Changes.vst3 exists and that your DAW is 64-bit with VST3 enabled."],
                 ["No sound in the standalone app", "Options > Audio/MIDI Settings: choose an output device and press 'Test'. Check that Internal is on and the faders are up."],
                 ["Pads do nothing from my MIDI keyboard", "Tick the keyboard under Active MIDI inputs (standalone) or arm the track (DAW). Check CHORD ZONE matches where you are playing (default C2-B3)."],
                 ["Crackles or dropouts", "Increase the audio buffer size; close other audio apps; prefer Windows Audio (Low Latency) or your interface's ASIO driver in the DAW."],
                 ["Computer keys do not play", "Click on the Changes window first. In a DAW some hosts keep the keyboard for themselves."],
                 ["Stuck notes", "Press PANIC (or Esc)."],
                 ["Setup says Changes is running / a file is in use", "Close the standalone app and any DAW that has the plug-in loaded, then run Setup again."]],
                [2.2 * inch, 4.7 * inch])]

    # ---------------------------------------------------------------- 15
    S += [P("15  Uninstalling", h1)]
    S += N(["Close Changes and any DAW using it.",
            "Open <b>Settings > Apps > Installed apps</b> (or Control Panel > Programs and Features), find <b>Changes - Jazz Harmony Instrument</b> and click <b>Uninstall</b> - or use Start menu > Changes > Uninstall Changes.",
            "Confirm with <b>UNINSTALL</b>. The app, plug-in, shortcuts and registry entries are removed. Palettes and MIDI files you saved in Documents are kept."])

else:
    # ---------------------------------------------------------------- 14 (mac)
    S += [P("14  Troubleshooting", h1),
          table([["Problem", "Solution"],
                 ["'Apple cannot check it for malicious software'", "System Settings > Privacy &amp; Security > Open Anyway (section 3). Changes is safe but not notarized."],
                 ["Logic / GarageBand does not list Changes", "Logic Pro > Settings > Plug-in Manager > select Changes > Reset &amp; Rescan Selection. Make sure /Library/Audio/Plug-Ins/Components/Changes.component exists. Restarting the Mac also refreshes the Audio Unit list."],
                 ["Another DAW does not list Changes", "Rescan plug-ins (section 4). Check that /Library/Audio/Plug-Ins/VST3/Changes.vst3 exists."],
                 ["No sound in the standalone app", "Options > Audio/MIDI Settings: choose an output device and press 'Test'. Check that Internal is on and the faders are up, and that the Mac is not muted."],
                 ["Pads do nothing from my MIDI keyboard", "Tick the keyboard under Active MIDI inputs (standalone) or arm the track (DAW). Check CHORD ZONE matches where you are playing (default C2-B3)."],
                 ["Crackles or dropouts", "Increase the audio buffer size and close other audio apps."],
                 ["Computer keys do not play", "Click on the Changes window first. Logic's own Musical Typing may capture the keys - close it (Window > Show Musical Typing) or click the Changes window."],
                 ["Stuck notes", "Press PANIC (or Esc)."]],
                [2.2 * inch, 4.7 * inch])]

    # ---------------------------------------------------------------- 15 (mac)
    S += [P("15  Uninstalling", h1)]
    S += N(["Quit Changes and any DAW using it.",
            "Double-click <b>Uninstall Changes.command</b> (in the disk image, or in /Library/Application Support/JazzyT Audio/Changes/). Terminal opens; type <b>y</b>, press Return and enter your Mac password.",
            "If macOS blocks the script, open <b>Terminal</b> and run: <font name='PopM'>bash \"/Library/Application Support/JazzyT Audio/Changes/Uninstall Changes.command\"</font>",
            "The app, Audio Unit, VST3 plug-in and manual are removed. Palettes and MIDI files you saved in Documents are kept."])

# ---------------------------------------------------------------- 16
S += [P("16  Credits & licences", h1),
      P("Changes 1.0 by JazzyT Audio. Built with the JUCE framework (JUCE 7). VST is a registered trademark of Steinberg Media "
        "Technologies GmbH. The interface uses the Poppins typeface by Indian Type Foundry, licensed under the SIL Open Font License 1.1 "
        "(full text in <i>Third-Party Licenses.txt</i>). The JUCE 'Made with JUCE' splash appears briefly when the window opens, as "
        "required by the JUCE licence."),
      P("This software is provided as-is, without warranty of any kind.", small)]

doc.build(S)
print("wrote", OUT)
