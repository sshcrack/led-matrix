#!/bin/bash

# Configuration
SOURCE_DIR="$1"
DEST_DIR="$2"

# Check if directories are provided
if [ -z "$SOURCE_DIR" ] || [ -z "$DEST_DIR" ]; then
    echo "Usage: $0 <source_directory> <destination_directory>"
    exit 1
fi

# Check if fd is available
if ! command -v fd >/dev/null 2>&1; then
    echo "Error: fd is not installed. Install it with:"
    echo "  cargo install fd-find"
    echo "  # or"
    echo "  sudo apt install fd-find"
    exit 1
fi

# Convert to absolute paths
SOURCE_DIR=$(realpath "$SOURCE_DIR")
DEST_DIR=$(realpath "$DEST_DIR")

echo "Watching: $SOURCE_DIR"
echo "Syncing to: $DEST_DIR"
echo "Using fd for .gitignore support"
echo "Press Ctrl+C to stop..."

# Function to sync using fd for file discovery
sync_directories() {
    echo "Syncing changes..."
    cd "$SOURCE_DIR"
    
    # Create temporary file list using fd
    TEMP_FILE=$(mktemp)
    fd --type f --hidden . > "$TEMP_FILE"
    
    # Convert absolute paths to relative paths
    sed -i "s|^$SOURCE_DIR/||" "$TEMP_FILE"
    
    # Sync using the file list
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

# Watch for changes and sync
inotifywait -m -r -e modify,create,delete,move "$SOURCE_DIR" \
    --exclude '\.git' \
    --exclude '\.swp$' \
    --exclude '\.tmp$' \
    --exclude '~$' |
while read path action file; do
    # Check if the file should be ignored using fd
    full_path="$path$file"
    rel_path="${full_path#$SOURCE_DIR/}"
    
    cd "$SOURCE_DIR"
    if ! fd --type f --hidden . | grep -Fq "$full_path"; then
        echo "Ignoring $file (matches .gitignore)"
        continue
    fi
    
    echo "Detected $action on $file"
    sync_directories
done