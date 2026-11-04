#!/bin/bash
# Mette i download su GitHub Releases (quelli che il sito linka):
# rifà il Player, carica il codice, e GitHub compila Mac + Windows e pubblica la release v1.0.0.
cd "$(dirname "$0")" || exit 1
echo "1/3  OptiBob Player…"
bash ../OptiBobPlayer/costruisci.command sito >/dev/null 2>&1 || { echo "⚠️  Player non compilato"; exit 1; }
cp -f "$HOME/CLAUDE/OPTIBOB/WEB/optibob.com/OptiBobPlayer.zip" ./OptiBobPlayer.zip
echo "2/3  carico su GitHub…"
git add .github/workflows/build.yml CMakeLists.txt Engine Source/*.h Source/*.cpp Source/*.mm \
        interfaccia-pro.html interfaccia-basic.html installer-mac.command installer-windows.iss \
        prova-desktop.command pubblica-download.command OptiBobPlayer.zip
git commit -q -m "OptiBob Live 1.0.0: interfaccia nuova, audio del Mac senza driver, installer Mac+Windows, release" || true
git push -q origin HEAD:main || { echo "⚠️  push non riuscito"; exit 1; }
echo "3/3  fatto. GitHub compila (circa 15 minuti):"
echo "     https://github.com/DIELLE319/optibob-plugin/actions"
