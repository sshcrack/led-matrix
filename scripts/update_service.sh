#!/bin/bash
set -e

DATA_DIR="/var/lib/led-matrix"
SUCCESS_FLAG="${DATA_DIR}/.update_success"
ERROR_FLAG="${DATA_DIR}/.update_error"

log() { echo "$(date '+%Y-%m-%d %H:%M:%S') [UPDATE] $1" | tee -a /var/log/led-matrix-update.log; }

error_exit() {
    log "ERROR: $1"
    echo "$1" > "$ERROR_FLAG"
    systemctl start led-matrix.service || log "Failed to start service"
    exit 1
}

main() {
    local deb_file="$1"
    [ $# -ne 1 ] && error_exit "Usage: $0 <deb_file>" ""
    [ ! -f "$deb_file" ] && error_exit "DEB file not found: $deb_file" "$deb_file"

    log "Starting DEB update: $deb_file"
    rm -f "$SUCCESS_FLAG" "$ERROR_FLAG"

    # dpkg -i runs postinst which:
    # - preserves /etc/default/led-matrix (hardware config + Spotify env)
    # - preserves /var/lib/led-matrix/config.json (update settings + user data)
    # - reloads and restarts the service
    log "Installing package..."
    if ! DEBIAN_FRONTEND=noninteractive dpkg -i "$deb_file"; then
        error_exit "dpkg -i failed" "$deb_file"
    fi

    log "Update successful"
    echo "$(date '+%Y-%m-%d %H:%M:%S')" > "$SUCCESS_FLAG"
    chown pi:pi "$SUCCESS_FLAG" 2>/dev/null || true
    rm -f "$deb_file"
    log "Update process complete"
}

main "$@"
