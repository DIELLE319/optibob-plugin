#!/bin/bash
# Compila OptiBob Desktop (standalone) e lo apre. Il log completo resta in ultima-build.log
A="$HOME/CLAUDE/OPTIBOB/APP/optibob-kvr"; B="$HOME/.optibob-kvr-build"; LOG="$A/ultima-build.log"
{ cmake -S "$A" -B "$B" -DCMAKE_BUILD_TYPE=Release && cmake --build "$B" --target OptiBob_Standalone -j8; } > "$LOG" 2>&1
if [ $? -ne 0 ]; then
  echo "▸ ERRORE di compilazione. Righe chiave:"
  grep -E "error:|Error [0-9]|undefined|ld: " "$LOG" | head -15
  echo "(log completo in ultima-build.log: lo legge Claude)"
  exit 1
fi
killall OptiBob "OptiBob Live" 2>/dev/null || true
sleep 1
# aperta come app (non dal Terminale): cosi' il permesso "audio di sistema" lo chiede OptiBob
APP="/Applications/OptiBob Live.app"
rm -rf /Applications/OptiBob.app "$APP"
ditto "$B/OptiBob_artefacts/Release/Standalone/OptiBob.app" "$APP"
# il nome dell'app sul Mac: OptiBob Live
/usr/libexec/PlistBuddy -c "Set :CFBundleName OptiBob Live" "$APP/Contents/Info.plist"
/usr/libexec/PlistBuddy -c "Add :CFBundleDisplayName string OptiBob Live" "$APP/Contents/Info.plist" 2>/dev/null \
  || /usr/libexec/PlistBuddy -c "Set :CFBundleDisplayName OptiBob Live" "$APP/Contents/Info.plist"
# firma: con un certificato sviluppatore se c'e' (il permesso audio resta fra una build e l'altra), sennò ad hoc
ID=$(security find-identity -v -p codesigning 2>/dev/null | awk -F'"' '/Apple Development|Developer ID Application/{print $2; exit}')
codesign --force --deep -s "${ID:--}" "$APP" >/dev/null 2>&1
echo "▸ firma: ${ID:-ad hoc}"
open -n "$APP"
echo "▸ Compilato e aperto."
