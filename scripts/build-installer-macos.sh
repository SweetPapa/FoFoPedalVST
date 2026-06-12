#!/usr/bin/env bash
#
# build-installer-macos.sh — package all six pedals into a GUI .pkg installer.
#
# Usage:
#   ./scripts/build-installer-macos.sh                # unsigned pkg (right-click → Open to install)
#   SIGN_ID="Developer ID Installer: Name (TEAMID)" \
#   NOTARY_PROFILE="my-notary-profile" \
#   ./scripts/build-installer-macos.sh                # signed + notarized pkg
#
# Prereqs: the plugins must already be built Release:
#   cmake --build build --config Release
#
# Output: dist/SweetPapaPedals-<version>.pkg
#   Installs AU components → /Library/Audio/Plug-Ins/Components
#            VST3 bundles  → /Library/Audio/Plug-Ins/VST3
#   (system-wide so every DAW and every user account sees them)
#
# Signing:
#   Personal (non-Stanford) Developer ID certs are AUTO-DETECTED from the
#   keychain — "Developer ID Installer" signs the pkg, "Developer ID
#   Application" signs each plugin bundle (hardened runtime + timestamp,
#   notarization-ready). Override or disable via env:
#     SIGN_ID=none            — force-unsigned build
#     SIGN_ID="..."           — explicit installer identity
#     APP_SIGN_ID="..."       — explicit bundle-signing identity
#     NOTARY_PROFILE=profile  — notarize+staple (xcrun notarytool store-credentials)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build}"
VERSION="${VERSION:-0.1.0}"
IDENTIFIER="com.sweetpapa.pedals"
DIST_DIR="$REPO_ROOT/dist"
STAGE="$(mktemp -d /tmp/sweetpapa-pkg.XXXXXX)"
trap 'rm -rf "$STAGE"' EXIT

PLUGINS=(VROOM DAYDREAM FOFOPEDAL DOUBLE BACKPORCH SWAY)

# ── auto-detect personal signing identities ─────────────────────────────────
# Sweet Papa releases are personal — never sign with institutional (Stanford)
# certs. Picks the first matching personal identity unless overridden.
detect_identity() { # $1 = identity-type prefix
  security find-identity -v 2>/dev/null \
    | grep -F "$1" \
    | grep -viE "stanford" \
    | head -1 \
    | sed -E 's/^[^"]*"([^"]+)".*$/\1/'
}

if [[ "${SIGN_ID:-}" == "none" ]]; then
  SIGN_ID=""
  APP_SIGN_ID=""
else
  SIGN_ID="${SIGN_ID:-$(detect_identity "Developer ID Installer")}"
  APP_SIGN_ID="${APP_SIGN_ID:-$(detect_identity "Developer ID Application")}"
fi
[[ -n "$SIGN_ID"     ]] && echo "installer identity: $SIGN_ID"
[[ -n "$APP_SIGN_ID" ]] && echo "bundle identity:    $APP_SIGN_ID"

echo "── staging plugins ─────────────────────────────────────"
mkdir -p "$STAGE/root/Library/Audio/Plug-Ins/Components" \
         "$STAGE/root/Library/Audio/Plug-Ins/VST3"

for p in "${PLUGINS[@]}"; do
  AU="$BUILD_DIR/${p}_artefacts/Release/AU/${p}.component"
  V3="$BUILD_DIR/${p}_artefacts/Release/VST3/${p}.vst3"
  [[ -d "$AU" ]] || { echo "missing $AU — build Release first"; exit 1; }
  [[ -d "$V3" ]] || { echo "missing $V3 — build Release first"; exit 1; }
  cp -R "$AU" "$STAGE/root/Library/Audio/Plug-Ins/Components/"
  cp -R "$V3" "$STAGE/root/Library/Audio/Plug-Ins/VST3/"

  if [[ -n "$APP_SIGN_ID" ]]; then
    # Hardened runtime + secure timestamp = notarization-ready bundles.
    codesign --force --deep --options runtime --timestamp \
             --sign "$APP_SIGN_ID" \
             "$STAGE/root/Library/Audio/Plug-Ins/Components/${p}.component"
    codesign --force --deep --options runtime --timestamp \
             --sign "$APP_SIGN_ID" \
             "$STAGE/root/Library/Audio/Plug-Ins/VST3/${p}.vst3"
  fi
  echo "  ✓ $p"
done

echo "── building component pkg ──────────────────────────────"
mkdir -p "$DIST_DIR"
COMPONENT_PKG="$STAGE/SweetPapaPedals-component.pkg"
pkgbuild \
  --root "$STAGE/root" \
  --identifier "$IDENTIFIER" \
  --version "$VERSION" \
  --install-location "/" \
  "$COMPONENT_PKG"

echo "── building GUI installer ──────────────────────────────"
cat > "$STAGE/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Sweet Papa Pedals ${VERSION}</title>
    <welcome mime-type="text/plain"><![CDATA[
Six free pedals for indie-rock production:

  DOUBLE     — every take you didn't record
  BACKPORCH  — sounds produced, not wet
  SWAY       — makes static tracks move like a band
  VROOM      — the dirt pedal that lands in the mix
  DAYDREAM   — one knob, warm tape to dream
  FOFOPEDAL  — twelve characters, one MIX knob

This installs the AU and VST3 plug-ins for all users
(/Library/Audio/Plug-Ins). Restart your DAW afterwards.
]]></welcome>
    <options customize="never" require-scripts="false" rootVolumeOnly="true"/>
    <domains enable_localSystem="true"/>
    <pkg-ref id="$IDENTIFIER" version="$VERSION">SweetPapaPedals-component.pkg</pkg-ref>
    <choices-outline>
        <line choice="default"/>
    </choices-outline>
    <choice id="default" title="Sweet Papa Pedals">
        <pkg-ref id="$IDENTIFIER"/>
    </choice>
</installer-gui-script>
EOF

OUT_PKG="$DIST_DIR/SweetPapaPedals-$VERSION.pkg"
PRODUCT_ARGS=(
  --distribution "$STAGE/distribution.xml"
  --package-path "$STAGE"
)
if [[ -n "${SIGN_ID:-}" ]]; then
  PRODUCT_ARGS+=(--sign "$SIGN_ID")
fi
productbuild "${PRODUCT_ARGS[@]}" "$OUT_PKG"

if [[ -n "${NOTARY_PROFILE:-}" ]]; then
  echo "── notarizing ──────────────────────────────────────────"
  xcrun notarytool submit "$OUT_PKG" --keychain-profile "$NOTARY_PROFILE" --wait
  xcrun stapler staple "$OUT_PKG"
fi

echo ""
echo "done → $OUT_PKG"
if [[ -z "$SIGN_ID" ]]; then
  echo "note: unsigned — recipients must right-click → Open the pkg."
elif [[ -z "${NOTARY_PROFILE:-}" ]]; then
  echo "note: signed but NOT notarized — Gatekeeper may still warn on download."
  echo "      One-time setup:  xcrun notarytool store-credentials sweetpapa \\"
  echo "                         --apple-id <appleid> --team-id 6Y5SZ2K5XY \\"
  echo "                         --password <app-specific-password>"
  echo "      Then:            NOTARY_PROFILE=sweetpapa $0"
fi
