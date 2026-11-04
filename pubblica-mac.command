#!/bin/bash
# Installer Mac firmato Developer ID + notarizzato, caricato sulla release di GitHub
# (quella a cui puntano i tasti del sito). Windows e Player li pubblica GitHub da solo.
cd "$(dirname "$0")" || exit 1
OUT="$HOME/CLAUDE/OPTIBOB/WEB/optibob.com"; PKG="$OUT/OptiBob-Mac.pkg"; REPO="DIELLE319/optibob-plugin"
SOLO_PLAYER=""; [ "$1" = player ] && SOLO_PLAYER=1     # «pubblica-mac.command player»: rifà solo il Player
if [ -z "$SOLO_PLAYER" ]; then
bash ./installer-mac.command || exit 1
if ! spctl -a -vv -t install "$PKG" 2>&1 | grep -q "accepted"; then
  echo "⚠️  Il Mac non accetta ancora il pacchetto (manca firma o notarizzazione):"
  spctl -a -vv -t install "$PKG" 2>&1 | head -3
  echo "    Lancia prima, nudo:  bash $(pwd)/prepara-firma.command"; exit 1
fi
echo "✓ Apple lo accetta. Carico su GitHub…"
fi
TOKEN=$(printf "protocol=https\nhost=github.com\n\n" | git credential fill 2>/dev/null | awk -F= '/^password=/{print $2}')
[ -z "$TOKEN" ] && { echo "⚠️  Non trovo l'accesso a GitHub"; exit 1; }
H=(-H "Authorization: Bearer $TOKEN" -H "Accept: application/vnd.github+json")
REL=$(curl -s "${H[@]}" "https://api.github.com/repos/$REPO/releases/latest")
RID=$(echo "$REL" | python3 -c "import sys,json;print(json.load(sys.stdin)['id'])")
carica () {   # carica $1 sulla release col nome $2, sostituendo quello che c'era
  local OLD=$(echo "$REL" | python3 -c "import sys,json;print(next((a['id'] for a in json.load(sys.stdin)['assets'] if a['name']=='$2'),''))")
  [ -n "$OLD" ] && curl -s -X DELETE "${H[@]}" "https://api.github.com/repos/$REPO/releases/assets/$OLD" >/dev/null
  curl -s "${H[@]}" -H "Content-Type: application/octet-stream" --data-binary @"$1" \
    "https://uploads.github.com/repos/$REPO/releases/$RID/assets?name=$2" | python3 -c "import sys,json;d=json.load(sys.stdin);print('✓ online:',d.get('browser_download_url',d))"
}
[ -z "$SOLO_PLAYER" ] && carica "$PKG" OptiBob-Mac.pkg

# OptiBob Player: firmato Developer ID + notarizzato anche lui (sennò il Mac avvisa)
ID=$(security find-identity -v -p codesigning | awk -F'"' '/Developer ID Application/{print $2; exit}')
bash ../OptiBobPlayer/costruisci.command >/dev/null 2>&1; killall "OptiBob Player" 2>/dev/null
T=$(mktemp -d); ditto "/Applications/OptiBob Player.app" "$T/OptiBob Player.app"
codesign --force --deep --options runtime --timestamp -s "$ID" "$T/OptiBob Player.app" \
  && ( cd "$T" && ditto -c -k --keepParent "OptiBob Player.app" n.zip ) \
  && echo "     Player: notarizzazione…" && { for t in 1 2 3; do xcrun notarytool submit "$T/n.zip" --keychain-profile optibob --wait 2>&1 | tail -1 | tee /tmp/ob-not.txt; grep -q "Accepted" /tmp/ob-not.txt && break; echo "     riprovo…"; sleep 20; done; grep -q "Accepted" /tmp/ob-not.txt; } \
  && xcrun stapler staple "$T/OptiBob Player.app" >/dev/null \
  && ( cd "$T" && rm -f n.zip && ditto -c -k --keepParent "OptiBob Player.app" OptiBobPlayer.zip ) \
  && cp -f "$T/OptiBobPlayer.zip" "$OUT/OptiBobPlayer.zip" && cp -f "$T/OptiBobPlayer.zip" ./OptiBobPlayer.zip \
  && carica "$T/OptiBobPlayer.zip" OptiBobPlayer.zip || echo "⚠️  Player non firmato"
rm -rf "$T"
