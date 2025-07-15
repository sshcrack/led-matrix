#!/bin/bash

# Configuration
SOURCE_DIR="$1"
DEST_DIR="$2"

# Check if directories are provided
if [ -z "$SOURCE_DIR" ] || [ -z "$DEST_DIR" ]; then
    echo "Usage: $0 <source_directory> <destination_directory>"
    exit 1
fi

# Check if ripgrep is available
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
echo "Using ripgrep for .gitignore support"
echo "Press Ctrl+C to stop..."

# Function to check if file should be ignored using ripgrep
is_ignored_rg() {
    local file_path="$1"
    cd "$SOURCE_DIR"
    
    # Use ripgrep to check if file would be found (not ignored)
    rg --files | grep -Fqx "${file_path#$SOURCE_DIR/}"
    # Return opposite (0 if ignored, 1 if not ignored)
    return $((!$?))
}

# Function to sync using ripgrep for file discovery
sync_directories() {
    echo "Syncing changes..."
    cd "$SOURCE_DIR"
    
    # Create temporary file list using ripgrep (excludes .git automatically)
    TEMP_FILE=$(mktemp)
    rg --files --hidden > "$TEMP_FILE"
    
    # Sync using the file list
    rsync -av \
        --delete \
        --delete-excluded \
        --files-from="$TEMP_FILE" \
        "$SOURCE_DIR/" "$DEST_DIR/"
    
    rm "$TEMP_FILE"
    echo "Sync completed at $(date)"
}

# Function to sync directories and handle deletions properly
sync_with_delete() {
    echo "Performing full sync with deletions..."
    cd "$SOURCE_DIR"
    
    # Get all files that should be synced (excludes .git automatically)
    TEMP_INCLUDE=$(mktemp)
    rg --files --hidden > "$TEMP_INCLUDE"
    
    # First sync: copy/update files
    rsync -av \
        --files-from="$TEMP_INCLUDE" \
        "$SOURCE_DIR/" "$DEST_DIR/"
    
    # Second pass: remove files in destination that shouldn't be there
    if [ -d "$DEST_DIR" ]; then
        find "$DEST_DIR" -type f | while read -r dest_file; do
            rel_path="${dest_file#$DEST_DIR/}"
            if ! grep -Fqx "$rel_path" "$TEMP_INCLUDE"; then
                echo "Removing deleted/ignored file: $rel_path"
                rm -f "$dest_file"
            fi
        done
        
        # Remove empty directories
        find "$DEST_DIR" -type d -empty -delete 2>/dev/null || true
    fi
    
    rm "$TEMP_INCLUDE"
    echo "Full sync completed at $(date)"
}

# Initial sync
sync_with_delete

# Watch for changes and sync
inotifywait -m -r -e modify,create,delete,move "$SOURCE_DIR" \
    --exclude '\.git' \
    --exclude '\.swp$' \
    --exclude '\.tmp$' \
    --exclude '~$' |
while read path action file; do
    # Check if the file should be ignored
    full_path="$path$file"
    rel_path="${full_path#$SOURCE_DIR/}"
    
    cd "$SOURCE_DIR"
    if ! rg --files --hidden | grep -Fqx "$rel_path" 2>/dev/null; then
        echo "Ignoring $file (matches .gitignore or is not a regular file)"
        continue
    fi
    
    echo "Detected $action on $file"
    sync_directories
done