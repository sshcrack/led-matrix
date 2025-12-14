#!/bin/bash
set -e

# Configuration
PRESET="cross-compile"
BUILD_DIR="build"
DIST_DIR="dist"
PLUGIN_REPO="sshcrack/led-matrix-plugins"

# Check for github cli
if ! command -v gh &> /dev/null; then
    echo "Error: gh (GitHub CLI) is not installed."
    exit 1
fi

# Check for cmake
if ! command -v cmake &> /dev/null; then
    echo "Error: cmake is not installed."
    exit 1
fi

# Function to display usage
usage() {
    echo "Usage: $0 <version> [options]"
    echo ""
    echo "Examples:"
    echo "  $0 1.0.0                     # Release all plugins at version 1.0.0"
    echo "  $0 1.0.0 --plugin Tetris     # Release only Tetris plugin at version 1.0.0"
    echo ""
    echo "Options:"
    echo "  --plugin <name>  Release only the specified plugin"
    echo "  --no-build       Skip the build step (use existing build artifacts)"
    echo "  --draft          Create as a draft release"
    echo "  --prerelease     Create as a prerelease"
    echo "  --dry-run        Prepare assets but do not publish to GitHub"
    exit 1
}

if [ -z "$1" ]; then
    usage
fi

VERSION="$1"
shift
SKIP_BUILD=false
IS_DRAFT=false
IS_PRERELEASE=false
DRY_RUN=false
SINGLE_PLUGIN=""

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --plugin)
            SINGLE_PLUGIN="$2"
            shift
            ;;
        --no-build) SKIP_BUILD=true ;;
        --draft) IS_DRAFT=true ;;
        --prerelease) IS_PRERELEASE=true ;;
        --dry-run) DRY_RUN=true ;;
        *) echo "Unknown parameter: $1"; usage ;;
    esac
    shift
done

echo "=== Publishing Plugins v$VERSION ==="
if [ -n "$SINGLE_PLUGIN" ]; then
    echo "Mode: Single plugin ($SINGLE_PLUGIN)"
else
    echo "Mode: All plugins"
fi

if [ "$SKIP_BUILD" = false ]; then
    echo "--> Configuring with preset $PRESET..."
    cmake --preset "$PRESET"

    echo "--> Building with preset $PRESET..."
    cmake --build --preset "$PRESET"
fi

echo "--> Installing to temporary distribution directory..."
rm -rf "$DIST_DIR"
cmake --install "$BUILD_DIR" --prefix "$DIST_DIR"

if [ ! -d "$DIST_DIR/plugins" ]; then
    echo "Error: No plugins directory found in $DIST_DIR/plugins"
    exit 1
fi

# Create a directory for release assets
RELEASE_ASSETS_DIR="release_assets"
rm -rf "$RELEASE_ASSETS_DIR"
mkdir -p "$RELEASE_ASSETS_DIR"

# Store absolute path to release assets dir
RELEASE_ASSETS_ABS=$(realpath "$RELEASE_ASSETS_DIR")

echo "--> Packaging plugins..."
cd "$DIST_DIR/plugins"

PLUGINS_RELEASED=0

# Function to publish a single plugin
publish_plugin() {
    local PLUGIN_NAME="$1"
    local PLUGIN_DIR="$PLUGIN_NAME"
    
    if [ ! -d "$PLUGIN_DIR" ]; then
        echo "Error: Plugin directory not found: $PLUGIN_DIR"
        return 1
    fi
    
    local SO_NAME="lib${PLUGIN_NAME}.so"
    
    if [ ! -f "$PLUGIN_DIR/$SO_NAME" ]; then
        echo "Warning: $SO_NAME not found in $PLUGIN_DIR. Skipping."
        return 1
    fi
    
    # Create zip file: PluginName.zip
    local ZIP_NAME="${PLUGIN_NAME}.zip"
    local ZIP_PATH="$RELEASE_ASSETS_ABS/$ZIP_NAME"
    
    pushd "$PLUGIN_DIR" > /dev/null
    zip -r -q "$ZIP_PATH" .
    popd > /dev/null
    
    echo "  Packed $ZIP_NAME"
    
    # Tag format: PluginName-vX.Y.Z
    local TAG="${PLUGIN_NAME}-v${VERSION}"
    local TITLE="${PLUGIN_NAME} v${VERSION}"
    local NOTES="Release of ${PLUGIN_NAME} version ${VERSION}"
    
    local GH_FLAGS=""
    if [ "$IS_DRAFT" = true ]; then GH_FLAGS="$GH_FLAGS --draft"; fi
    if [ "$IS_PRERELEASE" = true ]; then GH_FLAGS="$GH_FLAGS --prerelease"; fi
    
    local CMD="gh release create \"$TAG\" \"$ZIP_PATH\" \
        --repo \"$PLUGIN_REPO\" \
        --title \"$TITLE\" \
        --notes \"$NOTES\" \
        $GH_FLAGS"
    
    if [ "$DRY_RUN" = true ]; then
        echo "[DRY RUN] Would execute:"
        echo "  Tag: $TAG"
        echo "  Asset: $ZIP_NAME"
        echo "  Command: $CMD"
    else
        echo "--> Creating release: $TAG"
        eval "$CMD"
        echo "  Released $PLUGIN_NAME v$VERSION"
    fi
    
    PLUGINS_RELEASED=$((PLUGINS_RELEASED+1))
    return 0
}

if [ -n "$SINGLE_PLUGIN" ]; then
    # Single plugin mode
    publish_plugin "$SINGLE_PLUGIN" || exit 1
else
    # All plugins mode
    for d in */ ; do
        if [ -d "$d" ]; then
            PLUGIN_NAME="${d%/}"
            echo "Processing $PLUGIN_NAME..."
            publish_plugin "$PLUGIN_NAME" || true
        fi
    done
fi

cd ../..

if [ "$PLUGINS_RELEASED" -eq 0 ]; then
    echo "Error: No plugins released."
    exit 1
fi

echo "=== Published $PLUGINS_RELEASED plugin(s) ==="
