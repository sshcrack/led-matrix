#!/bin/bash

# LED Matrix Service Update Script
# This script handles the service update process independently from the main application

set -e

INSTALL_DIR="/opt/led-matrix"
SUCCESS_FLAG="${INSTALL_DIR}/.update_success"
ERROR_FLAG="${INSTALL_DIR}/.update_error"
CONFIG_BACKUP="/tmp/config.json.bak"

# Logging function
log() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') [UPDATE] $1" | tee -a /var/log/led-matrix-update.log
}

# Function to clean up temporary files
cleanup() {
    log "Cleaning up temporary files..."
    if [ -f "$CONFIG_BACKUP" ]; then
        rm -f "$CONFIG_BACKUP"
    fi
    if [ -f "$1" ]; then
        rm -f "$1"
    fi
}

# Error handler
error_exit() {
    log "ERROR: $1"
    echo "$1" > "$ERROR_FLAG"
    cleanup "$2"
    # Try to restart the service even after error
    log "Attempting to restart led-matrix service after error..."
    systemctl start led-matrix.service || log "Failed to restart service"
    exit 1
}

# Main update function
main() {
    local archive_file="$1"
    
    if [ $# -ne 1 ]; then
        error_exit "Usage: $0 <archive_file>" ""
    fi
    
    if [ ! -f "$archive_file" ]; then
        error_exit "Archive file not found: $archive_file" "$archive_file"
    fi
    
    log "Starting update process with archive: $archive_file"
    
    # Remove any existing flag files
    rm -f "$SUCCESS_FLAG" "$ERROR_FLAG"
    
    # Step 1: Stop the service
    log "Stopping led-matrix service..."
    if ! systemctl stop led-matrix.service; then
        log "Warning: Failed to stop led-matrix service, continuing..."
    fi
    
    # Step 2: Backup configuration if it exists
    if [ -f "${INSTALL_DIR}/config.json" ]; then
        log "Backing up configuration file..."
        if ! cp "${INSTALL_DIR}/config.json" "$CONFIG_BACKUP"; then
            error_exit "Failed to backup configuration file" "$archive_file"
        fi
    fi
    
    # Step 3: Extract the archive
    log "Extracting update archive..."
    if ! tar -xzf "$archive_file" -C "$INSTALL_DIR" --strip-components=1; then
        error_exit "Failed to extract update archive" "$archive_file"
    fi
    
    # Step 4: Restore configuration if it was backed up
    if [ -f "$CONFIG_BACKUP" ]; then
        log "Restoring configuration file..."
        if ! cp "$CONFIG_BACKUP" "${INSTALL_DIR}/config.json"; then
            log "Warning: Failed to restore configuration file"
        fi
    fi
    
    # Step 5: Start the service
    log "Starting led-matrix service..."
    if ! systemctl start led-matrix.service; then
        error_exit "Failed to start led-matrix service" "$archive_file"
    fi
    
    # Step 6: Create success flag
    log "Update completed successfully"
    echo "$(date '+%Y-%m-%d %H:%M:%S')" > "$SUCCESS_FLAG"
    
    # Step 7: Cleanup
    cleanup "$archive_file"
    
    log "Update process completed successfully"
}

# Run main function with all arguments
main "$@"