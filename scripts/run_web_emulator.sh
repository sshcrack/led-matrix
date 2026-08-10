#!/usr/bin/env bash
# Run the 128x128 emulator without a desktop window and expose its normal web
# UI (including the live matrix stream) to other devices on the local network.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$REPO_DIR/emulator_build}"
PORT="${PORT:-8080}"

cd "$REPO_DIR"

if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    cmake --preset emulator
fi
cmake --build "$BUILD_DIR" --target install

mapfile -t ADDRESSES < <(hostname -I 2>/dev/null | tr ' ' '\n' | grep -E '^[0-9]+\.' || true)
echo ""
echo "Web emulator is starting on port $PORT."
echo "Open one of these from your phone while it can reach this computer:"
echo "  http://localhost:$PORT/web/"
for address in "${ADDRESSES[@]}"; do
    echo "  http://$address:$PORT/web/"
done
echo ""
echo "Extra arguments are forwarded to led-matrix (for example: --scene audio_spectrum)."
echo "Press Ctrl+C to stop."
echo ""

export PORT
exec "$BUILD_DIR/install/bin/led-matrix" \
    --led-chain=2 \
    --led-parallel=2 \
    --led-rows=64 \
    --led-cols=64 \
    --led-emulator \
    --led-emulator-headless \
    "$@"
