#!/bin/bash
# Installer Mac firmato Developer ID + notarizzato, caricato sulla release di GitHub
# (quella a cui puntano i tasti del sito). Windows e Player li pubblica GitHub da solo.
cd "$(dirname "$0")" || exit 1
OUT="$HOME/CLAUDE/OPTIBOB/WEB/optibob.com"; PKG="$OUT/OptiBob-Mac.pkg"; REPO="DIELLE319/optibob-plugin"
bash ./installer-mac.command || exit 1
if ! spctl -a -vv -t install "$PKG" 2>&1 | grep -q "accepted"; then
  echo "⚠️  Il Mac non accetta ancora il pacchetto (manca firma o notarizzazione):"
  spctl -a -vv -t install "$PKG" 2>&1 | head -3
  echo "    Lancia prima, nudo:  bash $(pwd)/prepara-firma.command"; exit 1
fi
echo "✓ Apple lo accetta. Carico su GitHub…"
TOKEN=$(printf "protocol=https\nhost=github.com\n\n" | git credential fill 2>/dev/null | awk -F= '/^password=/{print $2}')
[ -z "$TOKEN" ] && { echo "⚠️  Non trovo l'accesso a GitHub"; exit 1; }
H=(-H "Authorization: Bearer $TOKEN" -H "Accept: application/vnd.github+json")
REL=$(curl -s "${H[@]}" "https://api.github.com/repos/$REPO/releases/latest")
RID=$(echo "$REL" | python3 -c "import sys,json;print(json.load(sys.stdin)['id'])")
OLD=$(echo "$REL" | python3 -c "import sys,json;print(next((a['id'] for a in json.load(sys.stdin)['assets'] if a['name']=='OptiBob-Mac.pkg'),''))")
[ -n "$OLD" ] && curl -s -X DELETE "${H[@]}" "https://api.github.com/repos/$REPO/releases/assets/$OLD" >/dev/null
curl -s "${H[@]}" -H "Content-Type: application/octet-stream" --data-binary @"$PKG" \
  "https://uploads.github.com/repos/$REPO/releases/$RID/assets?name=OptiBob-Mac.pkg" | python3 -c "import sys,json;d=json.load(sys.stdin);print('✓ online:',d.get('browser_download_url',d))"
