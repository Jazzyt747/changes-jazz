# Changes - Jazz Harmony Instrument

A playable jazz harmony instrument: one key per chord in the left hand, an always-in-key melody in the right,
and music theory from beginner to conservatory level. Built with JUCE 7.

| Platform | Formats | How it is built |
|---|---|---|
| macOS 10.13+ (universal: Apple Silicon + Intel) | Audio Unit, VST3, Standalone app | GitHub Actions (`.github/workflows/build-mac.yml`) -> `.pkg` installer + `.dmg` |
| Windows 10/11 x64 | VST3, Standalone app | Cross-compiled with llvm-mingw -> `Changes-1.0.0-Setup.exe` |

## Get the Mac build
Open the **Actions** tab -> latest **Build macOS** run -> download the **Changes-1.0.0-Mac** artifact.

## Layout
- `Source/` - theory engine, synth, plug-in processor and GUI
- `Tests/` - theory unit tests and the render/GUI test harness
- `Installer/` - Windows setup program
- `Packaging/` - manual generator, Windows packager, `mac/` installer scripts
