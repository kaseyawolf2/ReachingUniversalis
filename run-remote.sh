#!/usr/bin/env bash
# Pulls the latest ReachingUniversalis binary + world configs from the Hetzner
# build server and runs them locally. Extra args are forwarded to the binary
# (e.g. ./run-remote.sh --world worlds/medieval).
set -euo pipefail

SSH_KEY="${SSH_KEY:-$HOME/.ssh/Hetzner-AISandbox}"
REMOTE="${REMOTE:-kasey@178.104.121.55}"
REMOTE_DIR="${REMOTE_DIR:-/mnt/volume-Aisandbox/ReachingUniversalis}"
STAGE_DIR="${STAGE_DIR:-/tmp/ReachingUniversalis-run}"

echo "[run-remote] Staging into $STAGE_DIR"
mkdir -p "$STAGE_DIR"

echo "[run-remote] Pulling binary + worlds from $REMOTE ..."
ssh -i "$SSH_KEY" "$REMOTE" \
    "cd '$REMOTE_DIR' && tar czf - build/ReachingUniversalis worlds" \
    | tar xzf - -C "$STAGE_DIR"

cd "$STAGE_DIR"

echo "[run-remote] DISPLAY='${DISPLAY:-<unset>}' WAYLAND_DISPLAY='${WAYLAND_DISPLAY:-<unset>}'"
if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
    echo "[run-remote] WARNING: no DISPLAY or WAYLAND_DISPLAY set — Raylib will fail to open a window." >&2
    echo "[run-remote] On WSL2 with WSLg installed, DISPLAY is usually set automatically." >&2
fi

echo "[run-remote] Running ./build/ReachingUniversalis $*"
set +e
./build/ReachingUniversalis "$@"
rc=$?
set -e
echo "[run-remote] Exited with code $rc"
read -rp "[run-remote] Press Enter to close..." _
exit $rc
