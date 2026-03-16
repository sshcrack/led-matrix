#!/bin/bash
# record_scene_preview.sh
# Records preview GIFs for LED matrix scenes using the emulator and ffmpeg.
#
# Usage:
#   ./record_scene_preview.sh [OPTIONS]
#
# Options:
#   -a, --api-url URL       API base URL (default: http://localhost)
#   -s, --scene NAME        Record only this scene (default: record all scenes)
#   -d, --duration SECS     Recording duration in seconds (default: 5)
#   -o, --output-dir DIR    Output directory for GIFs (default: <api-exe-dir>/previews
#                           or ./previews if unreachable via SSH)
#   --fps FPS               GIF frame rate (default: 15)
#   --scale WxH             Scale of the output GIF (default: 128x128)
#   --display DISPLAY       X display to capture (default: :0)
#   --window-title TITLE    Window title to look for (default: "LED Matrix Emulator")
#   -h, --help              Show this help message
#
# Requirements:
#   - ffmpeg (for screen capture and GIF creation)
#   - xdotool (for finding the emulator window position)
#   - curl (for API calls)
#   - jq (for JSON parsing)
#
# Example - record all scenes from a running emulator:
#   ./record_scene_preview.sh --api-url http://localhost
#
# Example - record a single scene:
#   ./record_scene_preview.sh --api-url http://localhost --scene "WaveScene"
#
# Example - record with custom duration and output dir:
#   ./record_scene_preview.sh --api-url http://192.168.1.100 --duration 8 --output-dir ./previews

set -euo pipefail

# ── Defaults ──────────────────────────────────────────────────────────────────
API_URL="http://localhost"
SCENE_FILTER=""
DURATION=5
OUTPUT_DIR=""
FPS=15
SCALE="128x128"
DISPLAY_ENV="${DISPLAY:-:0}"
WINDOW_TITLE="LED Matrix Emulator"

# ── Parse arguments ───────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
  case "$1" in
    -a|--api-url)    API_URL="$2"; shift 2 ;;
    -s|--scene)      SCENE_FILTER="$2"; shift 2 ;;
    -d|--duration)   DURATION="$2"; shift 2 ;;
    -o|--output-dir) OUTPUT_DIR="$2"; shift 2 ;;
    --fps)           FPS="$2"; shift 2 ;;
    --scale)         SCALE="$2"; shift 2 ;;
    --display)       DISPLAY_ENV="$2"; shift 2 ;;
    --window-title)  WINDOW_TITLE="$2"; shift 2 ;;
    -h|--help)
      sed -n '2,/^set -/p' "$0" | grep '^#' | sed 's/^# \?//'
      exit 0
      ;;
    *) echo "Unknown option: $1" >&2; exit 1 ;;
  esac
done

# ── Helpers ───────────────────────────────────────────────────────────────────
check_deps() {
  local missing=()
  for dep in ffmpeg xdotool curl jq; do
    command -v "$dep" &>/dev/null || missing+=("$dep")
  done
  if [[ ${#missing[@]} -gt 0 ]]; then
    echo "ERROR: Missing required tools: ${missing[*]}" >&2
    echo "Install them with: sudo apt-get install ${missing[*]}" >&2
    exit 1
  fi
}

api() {
  curl -sf "${API_URL}${1}" || { echo "ERROR: API call failed: ${API_URL}${1}" >&2; exit 1; }
}

api_post() {
  curl -sf -X POST -H "Content-Type: application/json" -d "$3" "${API_URL}${1}" || {
    echo "ERROR: POST to ${API_URL}${1} failed" >&2; exit 1;
  }
}

find_window_geometry() {
  # Returns "WxH+X+Y" for ffmpeg's grab_x/grab_y/video_size
  local wid
  wid=$(xdotool search --name "$WINDOW_TITLE" 2>/dev/null | head -1) || true
  if [[ -z "$wid" ]]; then
    echo "WARNING: Could not find window titled '$WINDOW_TITLE'. Falling back to full screen." >&2
    echo ""
    return
  fi
  local geom
  geom=$(xdotool getwindowgeometry --shell "$wid" 2>/dev/null) || true
  local W H X Y
  W=$(echo "$geom" | grep '^WIDTH=' | cut -d= -f2)
  H=$(echo "$geom" | grep '^HEIGHT=' | cut -d= -f2)
  X=$(echo "$geom" | grep '^X=' | cut -d= -f2)
  Y=$(echo "$geom" | grep '^Y=' | cut -d= -f2)
  echo "${W}x${H}+${X}+${Y}"
}

record_scene_gif() {
  local scene_name="$1"
  local out_path="$2"

  echo "→ Recording scene: $scene_name"

  # Activate the scene by creating a temporary preset and setting it active
  local tmp_preset_id="__preview_${scene_name}__"
  local scene_json
  scene_json=$(jq -cn --arg type "$scene_name" '
    [{"uuid": "00000000-0000-0000-0000-000000000001", "type": $type, "arguments": {}}]
  ')

  api_post "/preset?id=$(python3 -c "import urllib.parse,sys; print(urllib.parse.quote(sys.argv[1]))" "$tmp_preset_id")" \
    "application/json" "$scene_json" >/dev/null 2>&1 || true

  # Activate the preset
  curl -sf "${API_URL}/set_active?id=$(python3 -c "import urllib.parse,sys; print(urllib.parse.quote(sys.argv[1]))" "$tmp_preset_id")" >/dev/null 2>&1 || true

  # Give the scene a moment to start
  sleep 1

  # Find window geometry
  local geom
  geom=$(find_window_geometry)

  local tmp_video
  tmp_video=$(mktemp /tmp/scene_preview_XXXXXX.mp4)

  if [[ -n "$geom" ]]; then
    # Capture the specific window
    local vid_size="${geom%%+*}"
    local offset="${geom#*+}"
    local grab_x="${offset%+*}"
    local grab_y="${offset#*+}"
    echo "  Capturing window: size=${vid_size} at ${grab_x},${grab_y}"
    DISPLAY="$DISPLAY_ENV" ffmpeg -y -loglevel error \
      -f x11grab \
      -video_size "$vid_size" \
      -grab_x "$grab_x" \
      -grab_y "$grab_y" \
      -i "$DISPLAY_ENV" \
      -t "$DURATION" \
      -c:v libx264 -preset ultrafast \
      "$tmp_video"
  else
    # Fall back to full screen capture
    echo "  Capturing full screen"
    DISPLAY="$DISPLAY_ENV" ffmpeg -y -loglevel error \
      -f x11grab \
      -i "$DISPLAY_ENV" \
      -t "$DURATION" \
      -c:v libx264 -preset ultrafast \
      "$tmp_video"
  fi

  # Convert video to GIF using a high-quality palette
  local tmp_palette
  tmp_palette=$(mktemp /tmp/palette_XXXXXX.png)

  ffmpeg -y -loglevel error \
    -i "$tmp_video" \
    -vf "fps=${FPS},scale=${SCALE}:flags=lanczos,palettegen=max_colors=128" \
    "$tmp_palette"

  ffmpeg -y -loglevel error \
    -i "$tmp_video" \
    -i "$tmp_palette" \
    -lavfi "fps=${FPS},scale=${SCALE}:flags=lanczos [x]; [x][1:v] paletteuse=dither=bayer:bayer_scale=5" \
    "$out_path"

  rm -f "$tmp_video" "$tmp_palette"

  # Clean up temp preset
  curl -sf -X DELETE "${API_URL}/preset?id=$(python3 -c "import urllib.parse,sys; print(urllib.parse.quote(sys.argv[1]))" "$tmp_preset_id")" >/dev/null 2>&1 || true

  echo "  ✓ Saved: $out_path"
}

# ── Main ──────────────────────────────────────────────────────────────────────
check_deps

# Determine output directory
if [[ -z "$OUTPUT_DIR" ]]; then
  OUTPUT_DIR="./previews"
  echo "No output directory specified, using: $OUTPUT_DIR"
fi

mkdir -p "$OUTPUT_DIR"

# Get scene list
echo "Fetching scene list from $API_URL ..."
SCENES_JSON=$(api "/list_scenes")
SCENE_NAMES=$(echo "$SCENES_JSON" | jq -r '.[].name')

if [[ -z "$SCENE_NAMES" ]]; then
  echo "ERROR: No scenes found." >&2
  exit 1
fi

echo "Available scenes:"
echo "$SCENE_NAMES" | sed 's/^/  - /'
echo ""

# Filter scenes if requested
if [[ -n "$SCENE_FILTER" ]]; then
  if ! echo "$SCENE_NAMES" | grep -qxF "$SCENE_FILTER"; then
    echo "ERROR: Scene '$SCENE_FILTER' not found." >&2
    echo "Available: $(echo "$SCENE_NAMES" | tr '\n' ', ' | sed 's/,$//')" >&2
    exit 1
  fi
  SCENE_NAMES="$SCENE_FILTER"
fi

TOTAL=$(echo "$SCENE_NAMES" | wc -l)
CURRENT=0

while IFS= read -r scene; do
  CURRENT=$((CURRENT + 1))
  echo "[$CURRENT/$TOTAL] Processing: $scene"
  out_file="${OUTPUT_DIR}/${scene}.gif"
  record_scene_gif "$scene" "$out_file"
done <<< "$SCENE_NAMES"

echo ""
echo "Done! Recorded $CURRENT preview(s) to: $OUTPUT_DIR"
echo ""
echo "To deploy, copy the GIFs to the 'previews/' directory next to the matrix executable:"
echo "  scp ${OUTPUT_DIR}/*.gif pi@<matrix-host>:/opt/led-matrix/previews/"
