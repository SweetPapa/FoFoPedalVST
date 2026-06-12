#!/usr/bin/env bash
#
# install-au.sh — build + validate + open in GarageBand.
#
# Usage:
#   ./scripts/install-au.sh                   # builds VROOM
#   ./scripts/install-au.sh DAYDREAM          # builds DAYDREAM
#   PLUGIN=DAYDREAM ./scripts/install-au.sh   # same thing
#   LAUNCH_GB=0 ./scripts/install-au.sh       # skip GarageBand launch
#   NO_BUILD=1  ./scripts/install-au.sh       # skip rebuild
#   CONFIG=Release ./scripts/install-au.sh    # release build
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build}"
CONFIG="${CONFIG:-Debug}"

# Plugin selection — arg1 wins over $PLUGIN, defaults to VROOM.
PLUGIN="${1:-${PLUGIN:-VROOM}}"
PLUGIN_UC="$(echo "$PLUGIN" | tr '[:lower:]' '[:upper:]')"

case "$PLUGIN_UC" in
  VROOM)     AU_SUBTYPE="Vrm1" ;;
  DAYDREAM)  AU_SUBTYPE="Dydr" ;;
  FOFOPEDAL) AU_SUBTYPE="FoFo" ;;
  *)
    echo "Unknown plugin '$PLUGIN'. Pass VROOM, DAYDREAM, or FOFOPEDAL." >&2
    exit 2
    ;;
esac

AU_MANUFACTURER="SwPa"
AU_TYPE="aufx"
AU_BUNDLE_NAME="${PLUGIN_UC}.component"
AU_INSTALL_PATH="$HOME/Library/Audio/Plug-Ins/Components/$AU_BUNDLE_NAME"

bold()   { printf "\033[1m%s\033[0m\n" "$*"; }
green()  { printf "\033[32m%s\033[0m\n" "$*"; }
yellow() { printf "\033[33m%s\033[0m\n" "$*"; }
red()    { printf "\033[31m%s\033[0m\n" "$*" >&2; }

# ── 1. Build ────────────────────────────────────────────────────────────────
if [ "${NO_BUILD:-0}" != "1" ]; then
  bold "▶ Building ${PLUGIN_UC} AU ($CONFIG)…"
  if [ ! -d "$BUILD_DIR" ]; then
    red "Build directory $BUILD_DIR not found — configure CMake first."
    exit 1
  fi
  cmake --build "$BUILD_DIR" --config "$CONFIG" --target "${PLUGIN_UC}_AU"
else
  yellow "▶ Skipping build (NO_BUILD=1)"
fi

# ── 2. Confirm install ──────────────────────────────────────────────────────
if [ ! -d "$AU_INSTALL_PATH" ]; then
  red "AU bundle not found at $AU_INSTALL_PATH"
  exit 1
fi
green "✓ Installed: $AU_INSTALL_PATH"

# ── 3. Quit GarageBand so it rescans AUs ────────────────────────────────────
if pgrep -x GarageBand >/dev/null; then
  bold "▶ Quitting GarageBand…"
  osascript -e 'tell application "GarageBand" to quit' 2>/dev/null || \
    killall GarageBand 2>/dev/null || true
  sleep 1
fi

# ── 4. Force AU registry rescan ─────────────────────────────────────────────
killall -9 AudioComponentRegistrar 2>/dev/null || true

# ── 5. Validate with auval ──────────────────────────────────────────────────
bold "▶ auval -v $AU_TYPE $AU_SUBTYPE $AU_MANUFACTURER …"
if auval -v "$AU_TYPE" "$AU_SUBTYPE" "$AU_MANUFACTURER"; then
  green "✓ auval PASSED — GarageBand will accept the plugin."
else
  red "✗ auval FAILED — GarageBand will refuse the plugin. See output above."
  exit 1
fi

# ── 6. Launch GarageBand ────────────────────────────────────────────────────
if [ "${LAUNCH_GB:-1}" = "1" ]; then
  if [ -d "/Applications/GarageBand.app" ]; then
    bold "▶ Launching GarageBand…"
    open -a GarageBand
    cat <<EOF

In GarageBand:
  1. New project → Empty Project → audio track.
  2. Smart Controls (B key) → Plug-ins area in the inspector.
  3. Audio Units → Sweet Papa Technologies → ${PLUGIN_UC}.

EOF
  else
    yellow "GarageBand not in /Applications — install it, then re-run."
  fi
fi

green "✓ Done."
