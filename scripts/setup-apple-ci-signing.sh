#!/usr/bin/env bash
#
# setup-apple-ci-signing.sh — one-time: export your PERSONAL Developer ID
# certificates from the login keychain and install everything GitHub Actions
# needs to sign + notarize macOS releases.
#
#   ./scripts/setup-apple-ci-signing.sh
#
# What it does:
#   1. Exports your keychain identities (macOS will pop up an "Allow" dialog —
#      possibly several times; enter your login password / click Always Allow).
#   2. Extracts ONLY the two personal identities (team 6Y5SZ2K5XY):
#        Developer ID Application: Forrester Terry
#        Developer ID Installer:  Forrester Terry
#      Stanford / institutional certs are never uploaded.
#   3. Sets the GitHub secrets: APPLE_CERT_APP_P12, APPLE_CERT_INSTALLER_P12,
#      APPLE_CERT_PASSWORD, APPLE_TEAM_ID — plus APPLE_ID / APPLE_APP_PASSWORD
#      for notarization (you'll be prompted; create the app-specific password
#      at https://account.apple.com → Sign-In & Security first).
#   4. Shreds all temp files.
set -euo pipefail

TEAM_ID="6Y5SZ2K5XY"
MATCH_NAME="Forrester Terry"
REPO="SweetPapa/FoFoPedalVST"

command -v gh >/dev/null || { echo "needs gh CLI (brew install gh)"; exit 1; }

# Prefer Homebrew OpenSSL (handles every PKCS12 variant); fall back to system.
OPENSSL=openssl
for c in /opt/homebrew/opt/openssl@3/bin/openssl /usr/local/opt/openssl@3/bin/openssl; do
  [ -x "$c" ] && OPENSSL="$c" && break
done

WORK="$(mktemp -d)"
cleanup() { rm -rf "$WORK"; }
trap cleanup EXIT

EXPORT_PW="$(uuidgen)"
P12_PW="$(uuidgen)"

echo "→ Exporting identities from the login keychain."
echo "  macOS will ask permission for each private key — click Allow / Always Allow."
security export -t identities -f pkcs12 -P "$EXPORT_PW" -o "$WORK/all.p12"

echo "→ Unpacking and selecting personal Developer ID identities…"
"$OPENSSL" pkcs12 -in "$WORK/all.p12" -passin "pass:$EXPORT_PW" -nodes -out "$WORK/all.pem" 2>/dev/null

python3 - "$WORK" "$TEAM_ID" "$MATCH_NAME" <<'PYEOF'
import re, sys, os
work, team, name = sys.argv[1], sys.argv[2], sys.argv[3]
pem = open(os.path.join(work, 'all.pem')).read()

# Split into bagged blocks: each "Bag Attributes" header + following PEM body.
blocks = re.split(r'(?=Bag Attributes)', pem)
keys, certs = {}, {}
for b in blocks:
    m_id = re.search(r'localKeyID:\s*([0-9A-F \.]+)', b)
    if not m_id:
        continue
    kid = m_id.group(1).strip()
    if '-----BEGIN PRIVATE KEY-----' in b or '-----BEGIN RSA PRIVATE KEY-----' in b:
        keys[kid] = re.search(r'-----BEGIN [A-Z ]*PRIVATE KEY-----.*?-----END [A-Z ]*PRIVATE KEY-----', b, re.S).group(0)
    elif '-----BEGIN CERTIFICATE-----' in b:
        fn = re.search(r'friendlyName:\s*(.+)', b)
        certs.setdefault(kid, []).append((fn.group(1).strip() if fn else '',
            re.search(r'-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----', b, re.S).group(0)))

wanted = {'Application': None, 'Installer': None}
for kid, certlist in certs.items():
    for fname, cert in certlist:
        for kind in wanted:
            if f'Developer ID {kind}: {name} ({team})' in fname and kid in keys:
                wanted[kind] = (keys[kid], cert, fname)

missing = [k for k, v in wanted.items() if v is None]
if missing:
    print(f"ERROR: could not find personal Developer ID {missing} identity(ies) "
          f"for {name} ({team}) with private keys in the export.", file=sys.stderr)
    sys.exit(1)

for kind, (key, cert, fname) in wanted.items():
    base = os.path.join(work, kind.lower())
    open(base + '.key', 'w').write(key)
    open(base + '.crt', 'w').write(cert)
    print(f"  ✓ {fname}")
PYEOF

for kind in application installer; do
  "$OPENSSL" pkcs12 -export \
    -inkey "$WORK/$kind.key" -in "$WORK/$kind.crt" \
    -passout "pass:$P12_PW" -out "$WORK/$kind.p12"
done

echo "→ Setting GitHub secrets on $REPO…"
base64 -i "$WORK/application.p12" | gh secret set APPLE_CERT_APP_P12       -R "$REPO"
base64 -i "$WORK/installer.p12"   | gh secret set APPLE_CERT_INSTALLER_P12 -R "$REPO"
printf '%s' "$P12_PW"             | gh secret set APPLE_CERT_PASSWORD      -R "$REPO"
printf '%s' "$TEAM_ID"            | gh secret set APPLE_TEAM_ID            -R "$REPO"

echo ""
read -r -p "Apple ID email (for notarization): " APPLE_ID_IN
read -r -s -p "App-specific password (from account.apple.com, format xxxx-xxxx-xxxx-xxxx): " APP_PW_IN
echo ""
if [ -n "$APPLE_ID_IN" ] && [ -n "$APP_PW_IN" ]; then
  printf '%s' "$APPLE_ID_IN" | gh secret set APPLE_ID           -R "$REPO"
  printf '%s' "$APP_PW_IN"   | gh secret set APPLE_APP_PASSWORD -R "$REPO"
  echo "  ✓ notarization secrets set"
else
  echo "  (skipped notarization secrets — re-run any time to add them)"
fi

echo ""
echo "Done. CI can now sign + notarize macOS releases. Secrets on the repo:"
gh secret list -R "$REPO"
