#!/bin/bash
#
# generate_scene_previews.sh
#
# Generates animated GIF previews for LED matrix scenes.
# Previews are saved to the scene_previews/ directory in the repository root.
# These previews are then committed to git and deployed with the application.
#
# Usage:
#   ./generate_scene_previews.sh [OPTIONS]
#
# Options:
#   --all                    Generate all scenes (default behavior)
#   --scenes <list>          Comma-separated scene names (e.g., "WaveScene,ColorPulseScene")
#   --list <file>            File containing scene names (one per line, # for comments)
#   --output <dir>           Output directory (default: scene_previews/)
#   --fps <n>                Frames per second (default: 15)
#   --frames <n>             Total frames per GIF (default: 90 = 6s @ 15fps)
#   --width <n>              Matrix width in pixels (default: 128)
#   --height <n>             Matrix height in pixels (default: 128)
#   --build-dir <dir>        Build directory (default: emulator_build)
#   --skip-validation        Skip checking if emulator binary exists
#   --dry-run                Show what would be done without executing
#   --help                   Show this help message
#
# Examples:
#   # Generate all scenes to scene_previews/
#   ./generate_scene_previews.sh --all
#
#   # Generate specific scenes
#   ./generate_scene_previews.sh --scenes "WaveScene,ColorPulseScene,FractalScene"
#
#   # Generate from a list file
#   ./generate_scene_previews.sh --list my_scenes.txt
#
#   # Custom settings
#   ./generate_scene_previews.sh --all --fps 20 --frames 120 --output ./custom_previews
#

set -euo pipefail

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# ---------------------------------------------------------------------------
# Script configuration
# ---------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ENV_FILE="$REPO_DIR/.env"

if [[ -f "$ENV_FILE" ]]; then
    # Export .env variables so child processes (preview_gen) can read them.
    set -a
    # shellcheck disable=SC1090
    source "$ENV_FILE"
    set +a
fi

MODE="all"  # all, scenes, or list
SCENE_LIST=""
OUTPUT_DIR="${REPO_DIR}/scene_previews"
BUILD_DIR="${REPO_DIR}/emulator_build"
FPS=15
FRAMES=90
WIDTH=128
HEIGHT=128
SKIP_VALIDATION=0
DRY_RUN=0

# ---------------------------------------------------------------------------
# Helper functions
# ---------------------------------------------------------------------------

print_usage() {
    grep '^#' "$0" | tail -n +2 | sed 's/^# *//'
}

print_error() {
    echo -e "${RED}ERROR: $*${NC}" >&2
}

print_success() {
    echo -e "${GREEN}✓ $*${NC}"
}

print_info() {
    echo -e "${YELLOW}ℹ $*${NC}"
}

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------

while [[ $# -gt 0 ]]; do
    case "$1" in
    --all)
        MODE="all"
        shift
        ;;
    --scenes)
        MODE="scenes"
        SCENE_LIST="$2"
        shift 2
        ;;
    --list)
        MODE="list"
        SCENE_LIST="$2"
        shift 2
        ;;
    --output)
        OUTPUT_DIR="$2"
        shift 2
        ;;
    --fps)
        FPS="$2"
        shift 2
        ;;
    --frames)
        FRAMES="$2"
        shift 2
        ;;
    --width)
        WIDTH="$2"
        shift 2
        ;;
    --height)
        HEIGHT="$2"
        shift 2
        ;;
    --build-dir)
        BUILD_DIR="$2"
        shift 2
        ;;
    --skip-validation)
        SKIP_VALIDATION=1
        shift
        ;;
    --dry-run)
        DRY_RUN=1
        shift
        ;;
    --help)
        print_usage
        exit 0
        ;;
    *)
        print_error "Unknown option: $1"
        print_usage
        exit 1
        ;;
    esac
done

# ---------------------------------------------------------------------------
# Validation
# ---------------------------------------------------------------------------

# Check if emulator build exists
BUILD_INSTALL_DIR="$BUILD_DIR/install"
PREVIEW_GEN="$BUILD_INSTALL_DIR/preview_gen"
if [[ ! -f "$PREVIEW_GEN" ]]; then
    if [[ $SKIP_VALIDATION -eq 0 ]]; then
        print_error "preview_gen binary not found at: $PREVIEW_GEN"
        print_info "Please build the emulator first:"
        print_info "  cmake --preset emulator"
        print_info "  cmake --build emulator_build"
        exit 1
    fi
fi

# Check if output directory path is valid
if [[ ! -d "$(dirname "$OUTPUT_DIR")" ]]; then
    print_error "Parent directory of output path does not exist: $(dirname "$OUTPUT_DIR")"
    exit 1
fi

# ---------------------------------------------------------------------------
# Build scene list
# ---------------------------------------------------------------------------

SCENES_CSV=""

case "$MODE" in
all)
    # Empty SCENES_CSV means render all scenes
    SCENES_CSV=""
    print_info "Mode: Render ALL scenes"
    ;;
scenes)
    # Use provided comma-separated list
    SCENES_CSV="$SCENE_LIST"
    SCENE_COUNT=$(echo "$SCENES_CSV" | tr ',' '\n' | wc -l)
    print_info "Mode: Render $SCENE_COUNT specific scenes"
    ;;
list)
    # Read from file, skip comments and empty lines
    if [[ ! -f "$SCENE_LIST" ]]; then
        print_error "Scene list file not found: $SCENE_LIST"
        exit 1
    fi
    SCENES_CSV=$(grep -v '^#' "$SCENE_LIST" | grep -v '^\s*$' | tr '\n' ',' | sed 's/,$//')
    SCENE_COUNT=$(echo "$SCENES_CSV" | tr ',' '\n' | grep -v '^$' | wc -l)
    print_info "Mode: Render $SCENE_COUNT scenes from $SCENE_LIST"
    ;;
esac

# ---------------------------------------------------------------------------
# Prepare output directory
# ---------------------------------------------------------------------------

if [[ $DRY_RUN -eq 0 ]]; then
    mkdir -p "$OUTPUT_DIR"
    print_success "Output directory: $OUTPUT_DIR"
fi

# ---------------------------------------------------------------------------
# Build command
# ---------------------------------------------------------------------------

PREVIEW_GEN="$BUILD_INSTALL_DIR/preview_gen"
PLUGIN_DIR="$BUILD_INSTALL_DIR/plugins"

CMD="$PREVIEW_GEN"
CMD="$CMD --output '$OUTPUT_DIR'"
CMD="$CMD --fps $FPS"
CMD="$CMD --frames $FRAMES"
CMD="$CMD --width $WIDTH"
CMD="$CMD --height $HEIGHT"

if [[ -n "$SCENES_CSV" ]]; then
    CMD="$CMD --scenes '$SCENES_CSV'"
fi

# ---------------------------------------------------------------------------
# Execute
# ---------------------------------------------------------------------------

print_info "Configuration:"
echo "  FPS:           $FPS"
echo "  Frames:        $FRAMES"
echo "  Resolution:    ${WIDTH}x${HEIGHT}"
echo "  Output:        $OUTPUT_DIR"
echo "  Runtime dir:   $BUILD_INSTALL_DIR"
echo ""

if [[ $DRY_RUN -eq 1 ]]; then
    print_info "DRY RUN - Command that would execute:"
    echo ""
    echo "(cd '$BUILD_INSTALL_DIR' && $CMD)"
    echo ""
    exit 0
fi

print_info "Starting preview generation..."
echo ""

# Unsetting Spotify secret because it needs manual rendering
unset SPOTIFY_CLIENT_SECRET
if eval "$CMD"; then
    echo ""
    print_success "Preview generation completed successfully!"
    print_info "Previews saved to: $OUTPUT_DIR"
    print_info "Next steps:"
    print_info "  1. Review the generated GIFs"
    print_info "  2. Commit them to git: git add scene_previews/"
    print_info "  3. Deploy with: cmake --build emulator_build --target install"
else
    EXIT_CODE=$?
    print_error "Preview generation failed with exit code $EXIT_CODE"
    exit $EXIT_CODE
fi
