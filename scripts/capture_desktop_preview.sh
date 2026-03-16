#!/usr/bin/env bash
# capture_desktop_preview.sh
#
# Records animated GIF previews for scenes that require the desktop application
# (i.e. scenes where needs_desktop_app() == true, such as VideoScene,
# AudioSpectrumScene, ShadertoyScene).
#
# Prerequisites:
#   - A running matrix emulator:  ./main              (from the emulator build)
#   - A running desktop app connected to the emulator: ./led-matrix-desktop
#   - ffmpeg with x11grab support
#   - xdotool (used to locate the emulator window by title)
#
# Usage:
#   ./scripts/capture_desktop_preview.sh [OPTIONS]
#
# Options:
#   --api-url <url>      Matrix API base URL (default: http://localhost:8080)
#   --output <dir>       Output directory for GIF files (default: ./previews)
#   --scenes <s1,s2,...> Comma-separated list of scene names to capture.
#                        If omitted, all desktop-dependent scenes are captured.
#   --duration <secs>    Capture duration per scene in seconds (default: 6)
#   --fps <n>            Output GIF frame rate (default: 15)
#   --window-title <t>   Emulator window title to search for
#                        (default: "RGB Matrix Emulator")
#   --no-cleanup         Keep the temporary preset after capture
#   --help               Show this help
#
# Workflow:
#   1. Emulator preview target (auto scenes, no desktop needed):
#        cmake --build emulator_build --target generate_scene_previews_incremental
#   2. Start the emulator and desktop app, then run this script for desktop scenes:
#        ./scripts/capture_desktop_preview.sh --api-url http://localhost:8080
#   3. Deploy previews with the cross-compile build (build_upload.sh handles the sync).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# ---------------------------------------------------------------------------
# Defaults
# ---------------------------------------------------------------------------
API_URL="http://localhost:8080"
OUTPUT_DIR="$REPO_DIR/emulator_build/previews"
SCENES_OVERRIDE=""
DURATION=6
FPS=15
WINDOW_TITLE="RGB Matrix Emulator"
NO_CLEANUP=0
TEMP_PRESET_ID="__preview_capture_tmp__"

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        --api-url)       API_URL="$2";        shift 2 ;;
        --output)        OUTPUT_DIR="$2";     shift 2 ;;
        --scenes)        SCENES_OVERRIDE="$2"; shift 2 ;;
        --duration)      DURATION="$2";       shift 2 ;;
        --fps)           FPS="$2";            shift 2 ;;
        --window-title)  WINDOW_TITLE="$2";   shift 2 ;;
        --no-cleanup)    NO_CLEANUP=1;        shift ;;
        --help|-h)
            sed -n '/^# /,/^[^#]/p' "$0" | grep '^#' | sed 's/^# \?//'
            exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

# ---------------------------------------------------------------------------
# Dependency checks
# ---------------------------------------------------------------------------
for cmd in curl ffmpeg xdotool; do
    if ! command -v "$cmd" &>/dev/null; then
        echo "ERROR: '$cmd' not found. Please install it." >&2
        exit 1
    fi
done

mkdir -p "$OUTPUT_DIR"

# ---------------------------------------------------------------------------
# Helper: call the matrix REST API
# ---------------------------------------------------------------------------
api_get() {
    curl -sf "$API_URL$1"
}

api_post() {
    local path="$1"; shift
    curl -sf -X POST -H "Content-Type: application/json" -d "$1" "$API_URL$path"
}

api_delete() {
    curl -sf -X DELETE "$API_URL$1"
}

# ---------------------------------------------------------------------------
# Wait for the matrix API to be reachable
# ---------------------------------------------------------------------------
echo "Waiting for matrix API at $API_URL ..."
for i in $(seq 1 20); do
    if api_get "/list_scenes" &>/dev/null; then
        echo "API is up."
        break
    fi
    if [[ $i -eq 20 ]]; then
        echo "ERROR: Matrix API not reachable at $API_URL after 20 attempts." >&2
        exit 1
    fi
    sleep 1
done

# ---------------------------------------------------------------------------
# Get the list of desktop-dependent scenes from the API
# ---------------------------------------------------------------------------
SCENES_JSON=$(api_get "/list_scenes")

if [[ -n "$SCENES_OVERRIDE" ]]; then
    IFS=',' read -ra TARGET_SCENES <<< "$SCENES_OVERRIDE"
else
    mapfile -t TARGET_SCENES < <(
        echo "$SCENES_JSON" \
        | python3 -c "
import json, sys
scenes = json.load(sys.stdin)
for s in scenes:
    if s.get('needs_desktop', False):
        print(s['name'])
"
    )
fi

if [[ ${#TARGET_SCENES[@]} -eq 0 ]]; then
    echo "No desktop-dependent scenes found. Nothing to capture."
    exit 0
fi

echo "Will capture: ${TARGET_SCENES[*]}"

# ---------------------------------------------------------------------------
# Save the currently active preset so we can restore it afterwards
# ---------------------------------------------------------------------------
ORIGINAL_PRESET=$(api_get "/presets?include_active=true" 2>/dev/null | \
    python3 -c "import json,sys; d=json.load(sys.stdin); print(d.get('active',''))" 2>/dev/null || true)

cleanup_preset() {
    if [[ "$NO_CLEANUP" -eq 0 ]]; then
        api_delete "/preset?id=$TEMP_PRESET_ID" &>/dev/null || true
    fi
    # Restore original preset if we captured it
    if [[ -n "$ORIGINAL_PRESET" ]]; then
        api_get "/set_active?id=$(python3 -c "import urllib.parse; print(urllib.parse.quote('$ORIGINAL_PRESET'))")" &>/dev/null || true
    fi
}
trap cleanup_preset EXIT

# ---------------------------------------------------------------------------
# Find the emulator window
# ---------------------------------------------------------------------------
find_emulator_window() {
    xdotool search --name "$WINDOW_TITLE" 2>/dev/null | head -1
}

WINDOW_ID=$(find_emulator_window)
if [[ -z "$WINDOW_ID" ]]; then
    echo "ERROR: Could not find emulator window titled '$WINDOW_TITLE'." >&2
    echo "       Make sure the emulator is running with the default window title." >&2
    exit 1
fi
echo "Found emulator window: $WINDOW_ID"

# Get window geometry (x, y, width, height)
GEOM=$(xdotool getwindowgeometry "$WINDOW_ID" 2>/dev/null)
WIN_X=$(echo "$GEOM" | grep -oP 'Position: \K[0-9]+')
WIN_Y=$(echo "$GEOM" | grep -oP 'Position: [0-9]+,\K[0-9]+')
WIN_W=$(echo "$GEOM" | grep -oP 'Geometry: \K[0-9]+')
WIN_H=$(echo "$GEOM" | grep -oP 'Geometry: [0-9]+x\K[0-9]+')

echo "Emulator window geometry: ${WIN_W}x${WIN_H} at ${WIN_X},${WIN_Y}"

DISPLAY="${DISPLAY:-:0}"

# ---------------------------------------------------------------------------
# Capture loop
# ---------------------------------------------------------------------------
FRAME_DELAY=$(python3 -c "print(round(100/$FPS))")  # centiseconds for GIF delay

for SCENE_NAME in "${TARGET_SCENES[@]}"; do
    echo ""
    echo "=== Capturing: $SCENE_NAME ==="

    # Build a minimal preset JSON with just this scene
    PRESET_JSON=$(python3 -c "
import json
preset = {
    'scenes': [{'name': '$SCENE_NAME', 'properties': {}}],
    'transition_duration': 0,
    'transition_name': 'blend'
}
print(json.dumps(preset))
")

    # Create and activate the temporary preset
    if ! api_post "/preset?id=$TEMP_PRESET_ID" "$PRESET_JSON" &>/dev/null; then
        echo "WARNING: Could not create preset for '$SCENE_NAME'. Skipping." >&2
        continue
    fi
    if ! api_get "/set_active?id=$TEMP_PRESET_ID" &>/dev/null; then
        echo "WARNING: Could not activate preset for '$SCENE_NAME'. Skipping." >&2
        continue
    fi

    echo "Preset activated. Waiting 2 s for scene to load..."
    sleep 2

    # Record the emulator window
    RAW_VIDEO="/tmp/capture_${SCENE_NAME}.mp4"
    echo "Recording ${DURATION}s from emulator window..."
    ffmpeg -y -loglevel error \
        -f x11grab \
        -framerate "$FPS" \
        -video_size "${WIN_W}x${WIN_H}" \
        -i "${DISPLAY}+${WIN_X},${WIN_Y}" \
        -t "$DURATION" \
        "$RAW_VIDEO"

    # Convert to palette-optimised GIF
    GIF_PATH="$OUTPUT_DIR/${SCENE_NAME}.gif"
    echo "Converting to GIF: $GIF_PATH"

    PALETTE_FILE="/tmp/palette_${SCENE_NAME}.png"
    ffmpeg -y -loglevel error \
        -i "$RAW_VIDEO" \
        -vf "fps=$FPS,palettegen=stats_mode=full" \
        "$PALETTE_FILE"
    ffmpeg -y -loglevel error \
        -i "$RAW_VIDEO" \
        -i "$PALETTE_FILE" \
        -lavfi "fps=$FPS [x]; [x][1:v] paletteuse=dither=bayer" \
        "$GIF_PATH"

    rm -f "$RAW_VIDEO" "$PALETTE_FILE"
    echo "Saved: $GIF_PATH"
done

echo ""
echo "Done. Preview GIFs written to: $OUTPUT_DIR"
echo "Run 'cmake --build build --target install' (or build_upload.sh) to deploy."
