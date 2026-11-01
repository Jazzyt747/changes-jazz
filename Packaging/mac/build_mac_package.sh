#!/bin/bash
# =============================================================================
#  Changes for macOS - builds Changes-<ver>-Mac.pkg and Changes-<ver>-Mac.dmg
#  Usage:  Packaging/mac/build_mac_package.sh <build-dir> <manual.pdf> <out-dir>
#  Requires macOS with Xcode command line tools (pkgbuild, productbuild, hdiutil).
# =============================================================================
set -euo pipefail

BUILD="$1"; MANUAL="$2"; OUT="$3"
VER="1.0.0"
HERE="$(cd "$(dirname "$0")" && pwd)"
ART="$BUILD/Changes_artefacts/Release"
WORK="$(mktemp -d)"
mkdir -p "$OUT"

APP="$ART/Standalone/Changes.app"
VST3="$ART/VST3/Changes.vst3"
AU="$ART/AU/Changes.component"
for p in "$APP" "$VST3" "$AU" "$MANUAL"; do [ -e "$p" ] || { echo "missing $p"; exit 1; }; done

echo "== Architectures"
lipo -info "$APP/Contents/MacOS/Changes"
lipo -info "$VST3/Contents/MacOS/Changes"
lipo -info "$AU/Contents/MacOS/Changes"

# Unsigned build: an ad-hoc signature is still required for code to run on Apple Silicon.
echo "== Ad-hoc signing"
for b in "$APP" "$VST3" "$AU"; do
    xattr -cr "$b"
    codesign --force --deep --sign - --timestamp=none "$b"
    codesign --verify --deep --strict "$b"
done

# ---- payload roots -----------------------------------------------------------
mkroot() { mkdir -p "$WORK/root_$1$2"; }
mkroot app  "/Applications";                       ditto "$APP"  "$WORK/root_app/Applications/Changes.app"
mkroot au   "/Library/Audio/Plug-Ins/Components";  ditto "$AU"   "$WORK/root_au/Library/Audio/Plug-Ins/Components/Changes.component"
mkroot vst3 "/Library/Audio/Plug-Ins/VST3";        ditto "$VST3" "$WORK/root_vst3/Library/Audio/Plug-Ins/VST3/Changes.vst3"
DOCS="$WORK/root_docs/Library/Application Support/JazzyT Audio/Changes"
mkdir -p "$DOCS"
cp "$MANUAL" "$DOCS/Changes Manual.pdf"
cp "$HERE/Uninstall Changes.command" "$DOCS/Uninstall Changes.command"
chmod 755 "$DOCS/Uninstall Changes.command"
cp "$HERE/Third-Party Licenses.txt" "$DOCS/"

# ---- component packages ------------------------------------------------------
mkdir -p "$WORK/pkgs" "$WORK/scripts_au"
cp "$HERE/postinstall_au" "$WORK/scripts_au/postinstall"; chmod 755 "$WORK/scripts_au/postinstall"

component() {   # name identifier [scripts]
    local name="$1" id="$2" scripts="${3:-}"
    pkgbuild --analyze --root "$WORK/root_$name" "$WORK/$name.plist"
    # never let the installer "relocate" an update into some other copy of the bundle
    /usr/libexec/PlistBuddy -c "Set :0:BundleIsRelocatable false" "$WORK/$name.plist" 2>/dev/null || true
    local extra=()
    [ -n "$scripts" ] && extra=(--scripts "$scripts")
    pkgbuild --root "$WORK/root_$name" --component-plist "$WORK/$name.plist" \
             --identifier "$id" --version "$VER" --install-location / "${extra[@]}" \
             "$WORK/pkgs/$name.pkg"
}
component app  com.jazzytaudio.changes.app
component au   com.jazzytaudio.changes.au   "$WORK/scripts_au"
component vst3 com.jazzytaudio.changes.vst3
pkgbuild --root "$WORK/root_docs" --identifier com.jazzytaudio.changes.docs --version "$VER" --install-location / "$WORK/pkgs/docs.pkg"

# ---- product archive ---------------------------------------------------------
cp -R "$HERE/resources" "$WORK/resources"
PKG="$OUT/Changes-$VER-Mac.pkg"
productbuild --distribution "$HERE/distribution.xml" --resources "$WORK/resources" \
             --package-path "$WORK/pkgs" "$PKG"
pkgutil --check-signature "$PKG" || true      # expected: unsigned
pkgutil --payload-files "$PKG" | head -40

# ---- disk image --------------------------------------------------------------
DMGROOT="$WORK/dmg/Changes $VER"
mkdir -p "$DMGROOT"
cp "$PKG" "$DMGROOT/Install Changes.pkg"
cp "$MANUAL" "$DMGROOT/Changes Manual.pdf"
cp "$HERE/README-Mac.txt" "$DMGROOT/README.txt"
cp "$HERE/Uninstall Changes.command" "$DMGROOT/Uninstall Changes.command"; chmod 755 "$DMGROOT/Uninstall Changes.command"
DMG="$OUT/Changes-$VER-Mac.dmg"
rm -f "$DMG"
hdiutil create -volname "Changes $VER" -srcfolder "$DMGROOT" -ov -format UDZO -imagekey zlib-level=9 "$DMG"
hdiutil verify "$DMG"

# portable zip of the raw bundles for manual installs
( cd "$WORK" && mkdir -p portable && ditto "$APP" portable/Changes.app && ditto "$VST3" portable/Changes.vst3 \
  && ditto "$AU" portable/Changes.component && ditto -c -k --sequesterRsrc --keepParent portable "$OUT/Changes-$VER-Mac-portable.zip" )

ls -la "$OUT"
echo "done"
