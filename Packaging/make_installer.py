"""Assembles Changes-1.0.0-Setup.exe and the distribution zip.
Setup.exe = setup stub + payload.zip + int64 offset + b'CHGSETUP'."""
import os, sys, struct, zipfile, shutil

ROOT = sys.argv[1]           # project root
BUILD = os.path.join(ROOT, "build-win")
DIST = os.path.join(ROOT, "dist")
VER = "1.0.0"

stub = os.path.join(BUILD, "ChangesSetup_artefacts", "Release", "ChangesSetup.exe")
app = os.path.join(BUILD, "Changes_artefacts", "Release", "Standalone", "Changes.exe")
vst = os.path.join(BUILD, "Changes_artefacts", "Release", "VST3", "Changes.vst3")
manual = os.path.join(DIST, "Changes Manual.pdf")

README = f"""CHANGES {VER} - Jazz Harmony Instrument (VST3 + Standalone for Windows 10/11 x64)
==========================================================================================

INSTALL
  1. Double-click  Changes-{VER}-Setup.exe
  2. Click "Yes" when Windows asks for permission (needed to write to Program Files).
     If "Windows protected your PC" appears: click "More info" -> "Run anyway"
     (the installer is not code-signed).
  3. NEXT -> choose components -> INSTALL -> FINISH.

  Installs:
    Standalone app : C:\\Program Files\\JazzyT Audio\\Changes\\Changes.exe
    VST3 plug-in   : C:\\Program Files\\Common Files\\VST3\\Changes.vst3
    Manual (PDF), Start-menu and desktop shortcuts, and an entry in Apps & features.

  Silent install : Changes-{VER}-Setup.exe /S        (from an administrator prompt)
  Manual install : copy Portable\\Changes.vst3 to C:\\Program Files\\Common Files\\VST3\\
                   and run Portable\\Changes.exe from anywhere.

USE IN YOUR DAW
  Rescan plug-ins, then load "Changes" (JazzyT Audio) as an instrument on a MIDI track.

QUICK START
  Left hand : MIDI notes C2-B3 (or computer keys Z S X D C V G B H N J M) = one chord per key.
  Right hand: notes above B3 (or keys Q 2 W 3 E R 5 T 6 Y 7 U I 9 O 0 P) = melody, snapped to the scale.
  TAB = second pad bank, SPACE = play progression, ESC = panic.
  Hover over any control - the help bar at the bottom explains it.

UNINSTALL
  Settings -> Apps -> Installed apps -> "Changes - Jazz Harmony Instrument" -> Uninstall
  (or Start menu -> Changes -> Uninstall Changes).

Full instructions: Changes Manual.pdf
"""

OFL_NOTICE = """THIRD-PARTY LICENCES
====================

JUCE
----
Changes is built with the JUCE framework (https://juce.com), (c) Raw Material Software Limited,
used under the JUCE 7 licence. The "Made with JUCE" splash screen is displayed as required.

VST3 SDK
--------
VST is a registered trademark of Steinberg Media Technologies GmbH. The VST3 SDK is used under
Steinberg's VST3 licence.

Poppins typeface
----------------
Copyright 2020 The Poppins Project Authors (https://github.com/itfoundry/Poppins)
(Indian Type Foundry). Licensed under the SIL Open Font License, Version 1.1:

"""

def main():
    for p in (stub, app, vst, manual):
        if not os.path.exists(p): sys.exit("missing " + p)
    os.makedirs(DIST, exist_ok=True)
    work = os.path.join(ROOT, "build-pkg"); shutil.rmtree(work, ignore_errors=True); os.makedirs(work)

    ofl = open("/root/.rustup/toolchains/stable-x86_64-unknown-linux-gnu/share/doc/rust/licenses/OFL-1.1.txt").read()
    licences = OFL_NOTICE + ofl
    open(os.path.join(work, "README.txt"), "w", newline="\r\n").write(README)
    open(os.path.join(work, "Third-Party Licenses.txt"), "w", newline="\r\n").write(licences)

    payload = os.path.join(work, "payload.zip")
    with zipfile.ZipFile(payload, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        z.write(app, "app/Changes.exe")
        z.write(manual, "app/Changes Manual.pdf")
        z.write(os.path.join(work, "README.txt"), "app/README.txt")
        z.write(os.path.join(work, "Third-Party Licenses.txt"), "app/Third-Party Licenses.txt")
        for base, dirs, files in os.walk(vst):
            for f in files:
                full = os.path.join(base, f)
                rel = os.path.relpath(full, os.path.dirname(vst)).replace(os.sep, "/")
                z.write(full, "vst3/" + rel)

    setup = os.path.join(DIST, f"Changes-{VER}-Setup.exe")
    stub_bytes = open(stub, "rb").read()
    with open(setup, "wb") as out:
        out.write(stub_bytes)
        out.write(open(payload, "rb").read())
        out.write(struct.pack("<q", len(stub_bytes)))
        out.write(b"CHGSETUP")

    # distribution zip
    dzip = os.path.join(DIST, f"Changes-{VER}-Windows.zip")
    top = f"Changes-{VER}-Windows"
    with zipfile.ZipFile(dzip, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        z.write(setup, f"{top}/Changes-{VER}-Setup.exe")
        z.write(manual, f"{top}/Changes Manual.pdf")
        z.write(os.path.join(work, "README.txt"), f"{top}/README.txt")
        z.write(os.path.join(work, "Third-Party Licenses.txt"), f"{top}/Third-Party Licenses.txt")
        z.write(app, f"{top}/Portable/Changes.exe")
        for base, dirs, files in os.walk(vst):
            for f in files:
                full = os.path.join(base, f)
                rel = os.path.relpath(full, os.path.dirname(vst)).replace(os.sep, "/")
                z.write(full, f"{top}/Portable/{rel}")
    for f in (setup, dzip): print(f, os.path.getsize(f) // 1024, "KB")

main()
