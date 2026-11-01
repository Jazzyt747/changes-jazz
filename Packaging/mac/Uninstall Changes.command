#!/bin/bash
# Removes Changes (app, Audio Unit, VST3 plug-in and documentation) from this Mac.
# Your saved palettes and exported MIDI files are not touched.
echo ""
echo "  Uninstall Changes - Jazz Harmony Instrument"
echo "  -------------------------------------------"
echo "  This removes:"
echo "    /Applications/Changes.app"
echo "    /Library/Audio/Plug-Ins/Components/Changes.component"
echo "    /Library/Audio/Plug-Ins/VST3/Changes.vst3"
echo "    /Library/Application Support/JazzyT Audio/Changes"
echo ""
read -r -p "  Continue? Type y and press Return: " answer
if [ "$answer" != "y" ] && [ "$answer" != "Y" ]; then echo "  Cancelled."; exit 0; fi

echo "  Your Mac password is needed to remove files from /Applications and /Library."
sudo /bin/rm -rf "/Applications/Changes.app" \
                 "/Library/Audio/Plug-Ins/Components/Changes.component" \
                 "/Library/Audio/Plug-Ins/VST3/Changes.vst3" \
                 "/Library/Application Support/JazzyT Audio/Changes"
sudo /bin/rmdir "/Library/Application Support/JazzyT Audio" 2>/dev/null
for id in app au vst3 docs; do sudo /usr/sbin/pkgutil --forget "com.jazzytaudio.changes.$id" >/dev/null 2>&1; done
killall -9 AudioComponentRegistrar >/dev/null 2>&1
echo ""
echo "  Changes has been removed. You can close this window."
