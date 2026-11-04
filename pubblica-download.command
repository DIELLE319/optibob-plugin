#!/bin/bash
# Mette i download su GitHub Releases (quelli che il sito linka):
# rifà il Player, carica il codice, e GitHub compila Mac + Windows e pubblica la release v1.0.0.
cd "$(dirname "$0")" || exit 1
echo "1/3  OptiBob Player…"
bash ../OptiBobPlayer/costruisci.command sito >/dev/null 2>&1 || { echo "⚠️  Player non compilato"; exit 1; }
cp -f "$HOME/CLAUDE/OPTIBOB/WEB/optibob.com/OptiBobPlayer.zip" ./OptiBobPlayer.zip
# BlackHole 2ch (per macOS prima del 14.4): il driver si prende dal suo installer ufficiale in Download
P=$(ls -t "$HOME/Downloads"/BlackHole2ch*.pkg 2>/dev/null | head -1)
if [ -n "$P" ]; then
  X="$(mktemp -d)"; pkgutil --expand-full "$P" "$X/e" >/dev/null
  D=$(find "$X/e" -name "BlackHole2ch.driver" -type d | head -1)
  [ -n "$D" ] && ( cd "$(dirname "$D")" && ditto -c -k --keepParent BlackHole2ch.driver "$OLDPWD/BlackHole2ch.driver.zip" ) \
     && echo "     BlackHole 2ch dentro l'installer Mac ($(basename "$P"))"
  rm -rf "$X"
fi
[ -f BlackHole2ch.driver.zip ] || echo "⚠️  BlackHole2ch.pkg non trovato in Download: l'installer Mac esce senza"
echo "2/3  carico su GitHub…"
git add .github/workflows/build.yml CMakeLists.txt Engine Source/*.h Source/*.cpp Source/*.mm \
        interfaccia-pro.html interfaccia-basic.html installer-mac.command installer-windows.iss \
        prova-desktop.command pubblica-download.command pubblica-mac.command prepara-firma.command OptiBobPlayer.zip \
        $( [ -f BlackHole2ch.driver.zip ] && echo BlackHole2ch.driver.zip )
git commit -q -m "Windows: errori di compilazione visibili, prova automatica di installazione" || true
git push -q origin HEAD:main || { echo "⚠️  push non riuscito"; exit 1; }
echo "3/3  fatto. GitHub compila (circa 15 minuti):"
echo "     https://github.com/DIELLE319/optibob-plugin/actions"
