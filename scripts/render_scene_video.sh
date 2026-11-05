#!/usr/bin/env bash
#
# render_scene_video.sh: render matrix scenes to MP4 (for PRs, reviews, demos).
#
# Uses preview_gen (emulator preset) to render frames deterministically in
# virtual time, then ffmpeg to encode an upscaled H.264 MP4 with an LED-panel
# look. Audio-reactive scenes can be driven by a real recording (.audr from
# audio_analysis_dump) instead of the synthetic fixture.
#
# Usage:
#   scripts/render_scene_video.sh --scenes audio_disco,audio_stage [options]
#
# Options:
#   --scenes <a,b,..>    Scene names (required). One MP4 per scene.
#   --output <dir>       Output directory (default: ./scene_videos)
#   --seconds <n>        Length in seconds (default: 12)
#   --fps <n>            Frame rate (default: 30)
#   --scale <n>          Upscale factor (default: 6 -> 768x768 for 128x128)
#   --style <led|flat>   led = round dots with dark gaps (default), flat = nearest
#   --replay <file.audr> Drive the "audio" preview provider with a recording
#   --start <s>          Skip this many seconds of the replay (default: 0)
#   --audio <file.wav>   Mux this audio track (aligned with --start). Keep such
#                        videos local when the audio is copyrighted.
#   --preview-option p:k=v  Passed through to preview_gen (repeatable)
#   --build-dir <dir>    Build dir with install/bin/preview_gen (default: emulator_build)
#
# Example (real song analysis, silent video for a PR):
#   scripts/render_scene_video.sh --scenes audio_volcano --replay numb.audr --start 8 --seconds 15
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SCENES="" OUTPUT="$PWD/scene_videos" SECONDS_LEN=12 FPS=30 SCALE=6 STYLE=led REPLAY="" START=0 AUDIO=""
BUILD_DIR="$REPO_DIR/emulator_build"
EXTRA=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --scenes) SCENES="$2"; shift 2 ;;
        --output) OUTPUT="$2"; shift 2 ;;
        --seconds) SECONDS_LEN="$2"; shift 2 ;;
        --fps) FPS="$2"; shift 2 ;;
        --scale) SCALE="$2"; shift 2 ;;
        --style) STYLE="$2"; shift 2 ;;
        --replay) REPLAY="$(realpath "$2")"; shift 2 ;;
        --start) START="$2"; shift 2 ;;
        --audio) AUDIO="$(realpath "$2")"; shift 2 ;;
        --preview-option) EXTRA+=(--preview-option "$2"); shift 2 ;;
        --build-dir) BUILD_DIR="$2"; shift 2 ;;
        -h|--help) sed -n '2,30p' "$0"; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done

[[ -n "$SCENES" ]] || { echo "--scenes is required" >&2; exit 2; }
command -v ffmpeg >/dev/null || { echo "ffmpeg not found" >&2; exit 1; }
PREVIEW_GEN="$BUILD_DIR/install/bin/preview_gen"
[[ -x "$PREVIEW_GEN" ]] || { echo "$PREVIEW_GEN missing; build the emulator preset first" >&2; exit 1; }

WIDTH=128 HEIGHT=128
mkdir -p "$OUTPUT"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

START_FRAMES=$(python3 -c "print(int(round($START*$FPS)))")
TOTAL=$(python3 -c "print(int(round(($START+$SECONDS_LEN)*$FPS)))")
if [[ -n "$REPLAY" ]]; then
    EXTRA+=(--preview-option "audio:replay=\"$REPLAY\"")
fi

# LED look: each matrix pixel becomes a round-ish dot with a dark gap.
if [[ "$STYLE" == "led" && "$SCALE" -ge 4 ]]; then
    VF="scale=iw*${SCALE}:ih*${SCALE}:flags=neighbor,geq=r='r(X,Y)*LED(X,Y)':g='g(X,Y)*LED(X,Y)':b='b(X,Y)*LED(X,Y)'"
    # geq has no user functions; expand the dot mask inline.
    MASK="lte(hypot(mod(X,${SCALE})-(${SCALE}-1)/2,mod(Y,${SCALE})-(${SCALE}-1)/2),${SCALE}*0.52)"
    VF="${VF//LED(X,Y)/$MASK}"
else
    VF="scale=iw*${SCALE}:ih*${SCALE}:flags=neighbor"
fi

IFS=',' read -ra LIST <<< "$SCENES"
for scene in "${LIST[@]}"; do
    echo "==> rendering $scene"
    (cd "$WORK" && "$PREVIEW_GEN" --scenes "$scene" --frames "$TOTAL" --fps "$FPS" \
        --width "$WIDTH" --height "$HEIGHT" --raw-out "$WORK/raw" "${EXTRA[@]}" >"$WORK/$scene.log" 2>&1) || {
        tail -20 "$WORK/$scene.log" >&2; exit 1; }
    RAW="$WORK/raw/$scene.rgb"
    [[ -s "$RAW" ]] || { echo "no frames for $scene (scene name typo, or it rendered nothing?)" >&2; tail -5 "$WORK/$scene.log" >&2; exit 1; }
    AUDIO_ARGS=()
    if [[ -n "$AUDIO" ]]; then
        AUDIO_ARGS=(-ss "$START" -i "$AUDIO" -map 0:v -map 1:a -c:a aac -b:a 160k -shortest)
    fi
    ffmpeg -loglevel error -y -f rawvideo -pix_fmt rgb24 -s "${WIDTH}x${HEIGHT}" -r "$FPS" \
        -ss "$(python3 -c "print($START_FRAMES/$FPS)")" -i "$RAW" "${AUDIO_ARGS[@]}" \
        -vf "$VF" -c:v libx264 -preset slow -crf 20 -pix_fmt yuv420p -movflags +faststart \
        "$OUTPUT/$scene.mp4"
    echo "    $OUTPUT/$scene.mp4"
done
