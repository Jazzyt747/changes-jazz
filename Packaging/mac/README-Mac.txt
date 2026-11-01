CHANGES 1.0.0 - Jazz Harmony Instrument for macOS (Apple Silicon + Intel, macOS 10.13+)
==================================================================================

INSTALL
  1. Double-click "Install Changes.pkg".
  2. macOS may say it "cannot be opened because Apple cannot check it for malicious
     software" (Changes is not notarized by Apple). To allow it:
       - Click Done / OK, then open  System Settings > Privacy & Security,
         scroll down and click  "Open Anyway"  next to "Install Changes.pkg",
         then confirm with your password or Touch ID.
       - On macOS 14 and older you can instead Control-click (right-click) the
         package, choose Open, then click Open.
  3. Follow the installer: Continue > Agree > Install.

  Installs:
    App          /Applications/Changes.app
    Audio Unit   /Library/Audio/Plug-Ins/Components/Changes.component
    VST3         /Library/Audio/Plug-Ins/VST3/Changes.vst3
    Manual       /Library/Application Support/JazzyT Audio/Changes/

USE
  Logic / GarageBand : Software Instrument track > AU Instruments > JazzyT Audio > Changes
  Other DAWs         : rescan plug-ins, load "Changes" (VST3) on an instrument track
  Standalone         : open Changes from Applications; Options > Audio/MIDI Settings

QUICK START
  Left hand : MIDI notes C2-B3 (or keys Z S X D C V G B H N J M) = one chord per key
  Right hand: notes above B3 (or keys Q 2 W 3 E R 5 T 6 Y 7 U I 9 O 0 P) = melody in the scale
  TAB = second pad bank, SPACE = play progression, ESC = panic, MENU > Text size

UNINSTALL
  Double-click "Uninstall Changes.command" (also installed in the Manual folder above).
  If macOS blocks it, open Terminal and run:
      bash "/Library/Application Support/JazzyT Audio/Changes/Uninstall Changes.command"

Full instructions: Changes Manual.pdf
