#!/bin/bash

# Configuration
SOURCE_DIR="$1"
DEST_DIR="$2"

# Check if directories are provided
if [ -z "$SOURCE_DIR" ] || [ -z "$DEST_DIR" ]; then
    echo "Usage: $0 <source_directory> <destination_directory>"
    exit 1
fi

# Check if both entr and ripgrep are available
if ! command -v entr >/dev/null 2>&1; then
    echo "Error: entr is not installed. Install it with:"
    echo "  sudo apt install entr"
    echo "  # or"
    echo "  brew install entr"
    exit 1
fi

if ! command -v rg >/dev/null 2>&1; then
    echo "Error: ripgrep (rg) is not installed. Install it with:"
    echo "  cargo install ripgrep"
    echo "  # or"
    echo "  sudo apt install ripgrep"
    exit 1
fi

# Convert to absolute paths
SOURCE_DIR=$(realpath "$SOURCE_DIR")
DEST_DIR=$(realpath "$DEST_DIR")

echo "Watching: $SOURCE_DIR"
echo "Syncing to: $DEST_DIR"
echo "Using entr for fast file watching"
echo "Press Ctrl+C to stop..."

# Function to sync using ripgrep
sync_directories() {
    echo "Syncing changes..."
    cd "$SOURCE_DIR"
    
    # Use ripgrep for file discovery
    TEMP_FILE=$(mktemp)
    rg --files --hidden > "$TEMP_FILE"
    
    rsync -av \
        --delete \
        --delete-excluded \
        --files-from="$TEMP_FILE" \
        "$SOURCE_DIR/" "$DEST_DIR/"
    
    rm "$TEMP_FILE"
    echo "Sync completed at $(date)"
}

# Initial sync
sync_directories

# Use entr to watch for changes with ripgrep
echo "Using ripgrep + entr for optimal performance..."
rg --files --hidden "$SOURCE_DIR" | entr -r bash -c "
    echo 'Change detected, syncing...'
    cd '$SOURCE_DIR'
    TEMP_FILE=\$(mktemp)
    rg --files --hidden > \"\$TEMP_FILE\"
    rsync -av --delete --delete-excluded --files-from=\"\$TEMP_FILE\" '$SOURCE_DIR/' '$DEST_DIR/'
    rm \"\$TEMP_FILE\"
    echo 'Sync completed at \$(date)'
"