#!/usr/bin/env bash
#
# setup-apple-ci-signing.sh — verify + upload your PERSONAL Developer ID
# certificates for CI signing. This script never touches other keys: YOU
# export exactly the two certificates in Keychain Access, and the script
# inspects the file and refuses to upload if anything else is inside.
#
# One-time flow:
#   1. Script opens Keychain Access. In "My Certificates" (login keychain),
#      cmd-click to select EXACTLY these two entries:
#         Developer ID Application: Forrester Terry (6Y5SZ2K5XY)
#         Developer ID Installer:  Forrester Terry (6Y5SZ2K5XY)
#      File → Export Items… → save as sweetpapa-signing.p12 with a password.
#   2. Script verifies the file contains ONLY those two identities
#      (hard abort otherwise — nothing gets uploaded).
#   3. Sets GitHub secrets: APPLE_CERT_P12, APPLE_CERT_PASSWORD,
#      APPLE_TEAM_ID — and APPLE_ID + APPLE_APP_PASSWORD for notarization,
#      read from the 'spt-notary' keychain item (account = Apple ID email,
#      password = app-specific password). Then deletes the local .p12.
set -euo pipefail

TEAM_ID="6Y5SZ2K5XY"
NAME="Forrester Terry"
REPO="SweetPapa/FoFoPedalVST"
ALLOWED_APP="Developer ID Application: $NAME ($TEAM_ID)"
ALLOWED_INST="Developer ID Installer: $NAME ($TEAM_ID)"

command -v gh >/dev/null || { echo "needs gh CLI (brew install gh)"; exit 1; }

OPENSSL=openssl
for c in /opt/homebrew/opt/openssl@3/bin/openssl /usr/local/opt/openssl@3/bin/openssl; do
  [ -x "$c" ] && OPENSSL="$c" && break
done

cat <<EOF
── Step 1: export the two certificates yourself ────────────────────────────
Keychain Access will open. In the left sidebar pick the "login" keychain,
then the "My Certificates" tab. Cmd-click to select EXACTLY these two:

    $ALLOWED_APP
    $ALLOWED_INST

File → Export Items… → format .p12 → save anywhere (e.g. Desktop) with a
password. Do NOT select anything else — this script will verify and refuse
to upload if any other identity is in the file.
────────────────────────────────────────────────────────────────────────────
EOF
open -a "Keychain Access" || true

read -r -p "Path to the exported .p12 [~/Desktop/sweetpapa-signing.p12]: " P12_PATH
P12_PATH="${P12_PATH:-$HOME/Desktop/sweetpapa-signing.p12}"
P12_PATH="${P12_PATH/#\~/$HOME}"
[ -f "$P12_PATH" ] || { echo "not found: $P12_PATH"; exit 1; }

read -r -s -p "Password you set on the .p12: " P12_PW; echo ""

echo "── Step 2: verifying the export contains ONLY your personal identities…"
CERT_NAMES=$("$OPENSSL" pkcs12 -in "$P12_PATH" -passin "pass:$P12_PW" -nokeys -clcerts 2>/dev/null \
  | "$OPENSSL" x509 -noout -subject 2>/dev/null; \
  "$OPENSSL" pkcs12 -in "$P12_PATH" -passin "pass:$P12_PW" -nokeys 2>/dev/null \
  | awk '/friendlyName:/ {sub(/^ +friendlyName: /,""); print}')

if [ -z "$CERT_NAMES" ]; then
  echo "could not read the .p12 (wrong password?)"; exit 1
fi

echo "$CERT_NAMES" | sed 's/^/    /'

# subject lines describe the same certs; judge on friendlyName lines only
FRIENDLY=$(echo "$CERT_NAMES" | grep -v "^subject=" | grep -v "^$" || true)
EXTRA=$(echo "$FRIENDLY" | grep -vF "$ALLOWED_APP" | grep -vF "$ALLOWED_INST" || true)

if [ -n "$EXTRA" ]; then
  echo ""
  echo "✗ REFUSING TO UPLOAD — the export contains identities other than your"
  echo "  personal Developer ID pair:"
  echo "$EXTRA" | sed 's/^/      /'
  echo "  Re-export with ONLY the two '$NAME ($TEAM_ID)' entries selected."
  exit 1
fi
if ! echo "$FRIENDLY" | grep -qF "$ALLOWED_APP" || ! echo "$FRIENDLY" | grep -qF "$ALLOWED_INST"; then
  echo ""
  echo "✗ The export is missing one of the two required identities"
  echo "  (need both Application and Installer). Re-export with both selected."
  exit 1
fi
echo "  ✓ exactly the two personal Developer ID identities — safe to upload"

echo "── Step 3: setting GitHub secrets on $REPO…"
base64 -i "$P12_PATH"  | gh secret set APPLE_CERT_P12      -R "$REPO"
printf '%s' "$P12_PW"  | gh secret set APPLE_CERT_PASSWORD -R "$REPO"
printf '%s' "$TEAM_ID" | gh secret set APPLE_TEAM_ID       -R "$REPO"

echo ""
echo "── Step 4: notarization credentials from the 'spt-notary' keychain item…"
# The keychain item stores the app-specific password as the secret and the
# Apple ID email in its account field. (macOS may show one Allow prompt.)
APPLE_ID_IN=$(security find-generic-password -s spt-notary 2>/dev/null \
  | awk -F'"' '/"acct"/ {print $4}')
APP_PW_IN=$(security find-generic-password -s spt-notary -w 2>/dev/null || true)

if [ -z "$APPLE_ID_IN" ] || [ -z "$APP_PW_IN" ]; then
  echo "  ('spt-notary' item not found/readable — falling back to prompts)"
  read -r -p "Apple ID email (blank to skip notarization): " APPLE_ID_IN
  [ -n "$APPLE_ID_IN" ] && { read -r -s -p "App-specific password: " APP_PW_IN; echo ""; }
fi

if [ -n "$APPLE_ID_IN" ] && [ -n "$APP_PW_IN" ]; then
  printf '%s' "$APPLE_ID_IN" | gh secret set APPLE_ID           -R "$REPO"
  printf '%s' "$APP_PW_IN"   | gh secret set APPLE_APP_PASSWORD -R "$REPO"
  echo "  ✓ notarization secrets set (Apple ID: $APPLE_ID_IN)"
else
  echo "  (skipped notarization secrets — re-run any time)"
fi

rm -f "$P12_PATH"
echo ""
echo "Done — local .p12 deleted. Secrets now on the repo:"
gh secret list -R "$REPO"
