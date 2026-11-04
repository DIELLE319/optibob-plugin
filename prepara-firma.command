#!/bin/bash
# UNA VOLTA SOLA: certificati Developer ID + profilo di notarizzazione "optibob".
# Va lanciato NUDO (senza cc): alla fine chiede la password specifica per app.
TEAM="Y96CN7UB7C"; MAIL="luca.pedretti@tutor81.com"
K="$HOME/CLAUDE/_chiavi"; KC="$HOME/Library/Keychains/login.keychain-db"
# certificato intermedio Apple (Developer ID G2), serve a costruire la catena della firma
curl -fsSL https://www.apple.com/certificateauthority/DeveloperIDG2CA.cer -o /tmp/DeveloperIDG2CA.cer && \
  security import /tmp/DeveloperIDG2CA.cer -k "$KC" >/dev/null 2>&1
# i due certificati creati su developer.apple.com + le loro chiavi (in CLAUDE/_chiavi)
PW="ob$RANDOM$RANDOM"
for n in app installer; do
  if [ -f "$K/devid-$n.key" ] && [ -f "$K/devid-$n.cer" ]; then
    /usr/bin/openssl x509 -inform der -in "$K/devid-$n.cer" -out "/tmp/devid-$n.pem"
    /usr/bin/openssl pkcs12 -export -inkey "$K/devid-$n.key" -in "/tmp/devid-$n.pem" -name "devid-$n" \
        -out "/tmp/devid-$n.p12" -passout "pass:$PW" || echo "⚠️  p12 $n non creato"
    security import "/tmp/devid-$n.p12" -k "$KC" -P "$PW" -T /usr/bin/codesign -T /usr/bin/productbuild \
        -T /usr/bin/productsign -T /usr/bin/pkgbuild 2>&1 | grep -v "already exists" | sed 's/^/   /'
    rm -f "/tmp/devid-$n.p12" "/tmp/devid-$n.pem"
  fi
done
echo "── nel portachiavi:"; security find-identity -p codesigning | grep -i "developer id" | sed 's/^/   /'
security find-identity -p basic | grep -i "developer id installer" | sed 's/^/   /'
A=$(security find-identity -v -p codesigning | grep -c "Developer ID Application")
I=$(security find-identity -v | grep -c "Developer ID Installer")
echo "Developer ID Application: $([ $A -gt 0 ] && echo presente || echo MANCA)"
echo "Developer ID Installer:   $([ $I -gt 0 ] && echo presente || echo MANCA)"
if [ $A -eq 0 ] || [ $I -eq 0 ]; then
  echo
  echo "Crea quelli che mancano: Xcode → Impostazioni → Account → $MAIL → Gestisci certificati"
  echo "→ + → «Developer ID Application» e poi + → «Developer ID Installer». Poi rilancia questo."
  open -a Xcode 2>/dev/null; exit 1
fi
if xcrun notarytool history --keychain-profile optibob >/dev/null 2>&1; then
  echo "Profilo di notarizzazione: presente. Tutto pronto."; exit 0
fi
echo
echo "Ora serve una password specifica per app (non quella dell'Apple ID):"
echo "account.apple.com → Accesso e sicurezza → Password specifiche per app → + → nome «optibob»"
open "https://account.apple.com/account/manage" 2>/dev/null
echo
xcrun notarytool store-credentials optibob --apple-id "$MAIL" --team-id "$TEAM"
