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
# firma: Developer ID (per i clienti, con notarizzazione) se c'e', sennò Apple Development / ad hoc (solo prove)
ID=$(security find-identity -v -p codesigning 2>/dev/null | awk -F'"' '/Developer ID Application/{print $2; exit}')
[ -z "$ID" ] && ID=$(security find-identity -v -p codesigning 2>/dev/null | awk -F'"' '/Apple Development/{print $2; exit}')
IID=$(security find-identity -v 2>/dev/null | awk -F'"' '/Developer ID Installer/{print $2; exit}')
ENT="$T/diritti.plist"
cat > "$ENT" <<'PL'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>com.apple.security.device.audio-input</key><true/>
</dict></plist>
PL
firma () {   # firma un bundle: runtime rinforzato + marca temporale se Developer ID (serve alla notarizzazione)
  if [[ "$ID" == Developer\ ID* ]]; then
    codesign --force --deep --options runtime --timestamp --entitlements "$ENT" -s "$ID" "$1"
  else
    codesign --force --deep -s "${ID:--}" "$1" >/dev/null 2>&1 || true
  fi
}
# -- OptiBob Live (app) --
RD="$T/d/root"; mkdir -p "$RD/Applications"
APP="$RD/Applications/OptiBob Live.app"
ditto "$B/Standalone/OptiBob.app" "$APP"
/usr/libexec/PlistBuddy -c "Set :CFBundleName OptiBob Live" "$APP/Contents/Info.plist"
/usr/libexec/PlistBuddy -c "Add :CFBundleDisplayName string OptiBob Live" "$APP/Contents/Info.plist" 2>/dev/null \
  || /usr/libexec/PlistBuddy -c "Set :CFBundleDisplayName OptiBob Live" "$APP/Contents/Info.plist"
firma "$APP"
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
firma "$RP/Library/Audio/Plug-Ins/Components/OptiBob.component"
firma "$RP/Library/Audio/Plug-Ins/VST3/OptiBob.vst3"
cat > "$RP/Applications/OptiBob Plugin Uninstall.command" <<'UNI'
#!/bin/bash
echo "Removing OptiBob Plugin…"; sudo rm -rf "/Library/Audio/Plug-Ins/Components/OptiBob.component" "/Library/Audio/Plug-Ins/VST3/OptiBob.vst3"
sudo pkgutil --forget com.optibob.plugin >/dev/null 2>&1 || true; sudo rm -f "/Applications/OptiBob Plugin Uninstall.command"; echo Done.
UNI
chmod +x "$RP/Applications/OptiBob Plugin Uninstall.command"
pkgbuild --root "$RP" --identifier com.optibob.plugin --version "$VER" --install-location / "$T/OptiBob-Plugin.pkg" >/dev/null

# -- BlackHole 2ch (solo macOS prima del 14.4; liberatoria di Existential Audio) --
BH=""
if [ -f "$APP_DIR/BlackHole2ch.driver.zip" ]; then
  RB="$T/b/root/Library/Audio/Plug-Ins/HAL"; mkdir -p "$RB" "$T/b/script"
  ditto -x -k "$APP_DIR/BlackHole2ch.driver.zip" "$RB"
  printf '#!/bin/bash\nlaunchctl kickstart -k system/com.apple.audio.coreaudiod 2>/dev/null || killall coreaudiod 2>/dev/null\nexit 0\n' > "$T/b/script/postinstall"
  chmod +x "$T/b/script/postinstall"
  pkgbuild --root "$T/b/root" --scripts "$T/b/script" --identifier com.optibob.blackhole --version "$VER" --install-location / "$T/BlackHole.pkg" >/dev/null
  BH=1
fi

echo "3/3  un solo pacchetto con la scelta…"
cat > "$T/distribution.xml" <<'XML'
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>OptiBob</title>
  <options customize="always" rootVolumeOnly="true"/>
  <script>function ob_vecchio(){ return system.compareVersions(system.version.ProductVersion, '14.4') &lt; 0; }</script>
  <choices-outline>
    <line choice="desktop"/>
    <line choice="plugin"/>
    <!--BH_LINE-->
  </choices-outline>
  <choice id="desktop" title="OptiBob Live (app)" description="The standalone processor for your radio. It takes the sound your Mac plays, no extra drivers.">
    <pkg-ref id="com.optibob.desktop"/>
  </choice>
  <choice id="plugin" title="OptiBob Plugin (AU / VST3)" description="For music software: Logic, Ableton, Reaper…">
    <pkg-ref id="com.optibob.plugin"/>
  </choice>
  <!--BH_CHOICE-->
  <pkg-ref id="com.optibob.desktop" version="1.0.0">OptiBob-Live.pkg</pkg-ref>
  <pkg-ref id="com.optibob.plugin" version="1.0.0">OptiBob-Plugin.pkg</pkg-ref>
</installer-gui-script>
XML
if [ -n "$BH" ]; then
  /usr/bin/sed -i '' -e 's|<!--BH_LINE-->|<line choice="blackhole"/>|' \
    -e 's|<!--BH_CHOICE-->|<choice id="blackhole" title="BlackHole 2ch (macOS before 14.4)" description="Audio driver that lets OptiBob Live take the sound your Mac plays on macOS older than 14.4. By Existential Audio, included with permission." selected="ob_vecchio()"><pkg-ref id="com.optibob.blackhole"/></choice><pkg-ref id="com.optibob.blackhole" version="1.0.0">BlackHole.pkg</pkg-ref>|' "$T/distribution.xml"
fi
if [ -n "$IID" ]; then
  productbuild --distribution "$T/distribution.xml" --package-path "$T" --sign "$IID" --timestamp "$OUT/OptiBob-Mac.pkg" >/dev/null
else
  productbuild --distribution "$T/distribution.xml" --package-path "$T" "$OUT/OptiBob-Mac.pkg" >/dev/null
fi
# notarizzazione Apple (profilo "optibob" creato una volta con notarytool store-credentials)
if [ -n "$IID" ] && xcrun notarytool history --keychain-profile optibob >/dev/null 2>&1; then
  echo "     notarizzazione Apple (qualche minuto)…"
  xcrun notarytool submit "$OUT/OptiBob-Mac.pkg" --keychain-profile optibob --wait | tail -3
  xcrun stapler staple "$OUT/OptiBob-Mac.pkg" >/dev/null && echo "     notarizzato e graffettato ✓"
fi
rm -rf "$T"
echo ""; echo "PRONTO -> $OUT/OptiBob-Mac.pkg  (app: ${ID:-ad hoc} | pkg: ${IID:-non firmato})"
