#!/bin/bash
# UN solo installer Mac: OptiBob-Mac.pkg. All'apertura si sceglie cosa installare
# (OptiBob Live = app, OptiBob Plugin = AU+VST3), con disinstallazione.
# Gira sul Mac di Luca e anche su GitHub (CI): B, OUT e SALTA_COMPILA si possono passare da fuori.
set -e
APP_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD="${BUILD:-$HOME/.optibob-kvr-build}"
B="$BUILD/OptiBob_artefacts/Release"
OUT="${OUT:-$HOME/CLAUDE/OPTIBOB/WEB/optibob.com}"
VER="1.0.0"; T="$(mktemp -d)"; mkdir -p "$OUT"

if [ -z "$SALTA_COMPILA" ]; then
  echo "1/3  compilo (app + AU + VST3)…"
  cmake -S "$APP_DIR" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release >/dev/null
  cmake --build "$BUILD" --target OptiBob_All -j8 >/dev/null
fi

echo "2/3  componenti…"
ID=$(security find-identity -v -p codesigning 2>/dev/null | awk -F'"' '/Developer ID Application|Apple Development/{print $2; exit}')
# -- OptiBob Live (app) --
RD="$T/d/root"; mkdir -p "$RD/Applications"
APP="$RD/Applications/OptiBob Live.app"
ditto "$B/Standalone/OptiBob.app" "$APP"
/usr/libexec/PlistBuddy -c "Set :CFBundleName OptiBob Live" "$APP/Contents/Info.plist"
/usr/libexec/PlistBuddy -c "Add :CFBundleDisplayName string OptiBob Live" "$APP/Contents/Info.plist" 2>/dev/null \
  || /usr/libexec/PlistBuddy -c "Set :CFBundleDisplayName OptiBob Live" "$APP/Contents/Info.plist"
codesign --force --deep -s "${ID:--}" "$APP" >/dev/null 2>&1 || true
cat > "$RD/Applications/OptiBob Live Uninstall.command" <<'UNI'
#!/bin/bash
echo "Removing OptiBob Live…"; sudo rm -rf "/Applications/OptiBob Live.app"
sudo pkgutil --forget com.optibob.desktop >/dev/null 2>&1 || true; sudo rm -f "/Applications/OptiBob Live Uninstall.command"; echo Done.
UNI
chmod +x "$RD/Applications/OptiBob Live Uninstall.command"
pkgbuild --root "$RD" --identifier com.optibob.desktop --version "$VER" --install-location / "$T/OptiBob-Live.pkg" >/dev/null
# -- OptiBob Plugin --
RP="$T/p/root"; mkdir -p "$RP/Library/Audio/Plug-Ins/Components" "$RP/Library/Audio/Plug-Ins/VST3" "$RP/Applications"
ditto "$B/AU/OptiBob.component" "$RP/Library/Audio/Plug-Ins/Components/OptiBob.component"
ditto "$B/VST3/OptiBob.vst3"    "$RP/Library/Audio/Plug-Ins/VST3/OptiBob.vst3"
codesign --force --deep -s "${ID:--}" "$RP/Library/Audio/Plug-Ins/Components/OptiBob.component" >/dev/null 2>&1 || true
codesign --force --deep -s "${ID:--}" "$RP/Library/Audio/Plug-Ins/VST3/OptiBob.vst3" >/dev/null 2>&1 || true
cat > "$RP/Applications/OptiBob Plugin Uninstall.command" <<'UNI'
#!/bin/bash
echo "Removing OptiBob Plugin…"; sudo rm -rf "/Library/Audio/Plug-Ins/Components/OptiBob.component" "/Library/Audio/Plug-Ins/VST3/OptiBob.vst3"
sudo pkgutil --forget com.optibob.plugin >/dev/null 2>&1 || true; sudo rm -f "/Applications/OptiBob Plugin Uninstall.command"; echo Done.
UNI
chmod +x "$RP/Applications/OptiBob Plugin Uninstall.command"
pkgbuild --root "$RP" --identifier com.optibob.plugin --version "$VER" --install-location / "$T/OptiBob-Plugin.pkg" >/dev/null

echo "3/3  un solo pacchetto con la scelta…"
cat > "$T/distribution.xml" <<'XML'
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>OptiBob</title>
  <options customize="always" rootVolumeOnly="true"/>
  <choices-outline>
    <line choice="desktop"/>
    <line choice="plugin"/>
  </choices-outline>
  <choice id="desktop" title="OptiBob Live (app)" description="The standalone processor for your radio. It takes the sound your Mac plays, no extra drivers.">
    <pkg-ref id="com.optibob.desktop"/>
  </choice>
  <choice id="plugin" title="OptiBob Plugin (AU / VST3)" description="For music software: Logic, Ableton, Reaper…">
    <pkg-ref id="com.optibob.plugin"/>
  </choice>
  <pkg-ref id="com.optibob.desktop" version="1.0.0">OptiBob-Live.pkg</pkg-ref>
  <pkg-ref id="com.optibob.plugin" version="1.0.0">OptiBob-Plugin.pkg</pkg-ref>
</installer-gui-script>
XML
productbuild --distribution "$T/distribution.xml" --package-path "$T" "$OUT/OptiBob-Mac.pkg" >/dev/null
rm -rf "$T"
echo ""; echo "PRONTO -> $OUT/OptiBob-Mac.pkg  (firma: ${ID:-ad hoc})"
