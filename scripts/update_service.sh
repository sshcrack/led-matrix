#!/bin/bash
# Runs in a separate systemd unit: restarting led-matrix must not kill its updater.
set -Eeuo pipefail

DATA_DIR=/var/lib/led-matrix
CACHE_DIR=/var/cache/led-matrix
BACKUP_DIR=/var/backups/led-matrix
ENV_FILE=/etc/default/led-matrix
LOG_FILE=/var/log/led-matrix-update.log
SERVICE=led-matrix.service
HEALTH_URL="http://127.0.0.1:${LED_MATRIX_UPDATE_PORT:-8080}"
HEALTH_ATTEMPTS=30
HEALTH_INTERVAL=2
HEALTH_SUCCESSES=3
HEALTH_TIMEOUT=60
transaction_started=false
committed=false
masked=false
failure_message="Update failed"
previous_version=""
previous_deb=""
transaction=""

log() { printf '%s [UPDATE] %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*" | tee -a "$LOG_FILE"; }
fail() { failure_message="$*"; log "ERROR: $*"; return 1; }

package_matches() {
    [[ $(dpkg-deb -f "$1" Package) == led-matrix &&
       $(dpkg-deb -f "$1" Version) == "$2" &&
       $(dpkg-deb -f "$1" Architecture) == "$3" ]]
}

health_check() {
    local expected_version=$1 healthy=0 previous_pid="" pid response
    local deadline=$((SECONDS + HEALTH_TIMEOUT))
    for ((attempt=0; attempt<HEALTH_ATTEMPTS && SECONDS<deadline; ++attempt)); do
        pid=$(systemctl show "$SERVICE" --property=MainPID --value) || pid=""
        if [[ $pid =~ ^[1-9][0-9]*$ ]] && systemctl is-active --quiet "$SERVICE" &&
           response=$(curl --noproxy "*" -fsS --max-time 3 "$HEALTH_URL/api/update/status") &&
           jq -e --arg version "$expected_version" '.current_version == $version' <<< "$response" >/dev/null &&
           curl --noproxy "*" -fsS --max-time 3 -o /dev/null "$HEALTH_URL/web/"; then
            if [[ $pid == "$previous_pid" ]]; then healthy=$((healthy+1)); else healthy=1; fi
            previous_pid=$pid
            if ((healthy >= HEALTH_SUCCESSES)); then return 0; fi
        else
            healthy=0
            previous_pid=""
        fi
        sleep "$HEALTH_INTERVAL"
    done
    return 1
}

snapshot_file() {
    if [[ -e $1 || -L $1 ]]; then cp -a "$1" "$transaction/$2"; fi
}

restore_file() {
    if [[ -e $transaction/$2 || -L $transaction/$2 ]]; then
        cp -a "$transaction/$2" "$1"
    else
        rm -f "$1"
    fi
}

restore_configuration() {
    restore_file "$ENV_FILE" environment &&
        restore_file "$DATA_DIR/config.json" config.json
}

write_flag() {
    local temporary
    temporary=$(mktemp "$DATA_DIR/.update-flag.XXXXXX")
    printf '%s\n' "$2" > "$temporary"
    chown pi:pi "$temporary" 2>/dev/null || true
    mv -fT "$temporary" "$DATA_DIR/$1"
}

finish() {
    local result=$?
    trap - EXIT INT TERM
    set +e
    if ((result != 0)) && ! $committed; then
        if $transaction_started; then
            log "Restoring led-matrix $previous_version and its configuration"
            systemctl stop "$SERVICE"
            # A failed candidate start may have already unmasked the service.
            if ! $masked; then systemctl mask --runtime "$SERVICE"; masked=true; fi
            if DEBIAN_FRONTEND=noninteractive dpkg --force-confdef --force-confold -i "$previous_deb"; then
                local restored=true
                restore_configuration || restored=false
                systemctl unmask --runtime "$SERVICE"
                masked=false
                systemctl daemon-reload
                if $restored && systemctl start "$SERVICE" && health_check "$previous_version"; then
                    failure_message="$failure_message; rolled back to $previous_version"
                else
                    failure_message="$failure_message; rollback installed but health checks failed"
                fi
            else
                restore_configuration
                failure_message="$failure_message; rollback package installation failed"
            fi
        fi
        if $masked; then systemctl unmask --runtime "$SERVICE"; masked=false; fi
        # Also recover the old service when preflight failed after the daemon exited.
        if ! $transaction_started; then systemctl start "$SERVICE"; fi
        rm -f "$DATA_DIR/.update_success"
        write_flag .update_error "$failure_message"
        log "ERROR: $failure_message"
    fi
    rm -f "$CACHE_DIR/candidate.deb" "$CACHE_DIR/previous.deb.partial"
    exit "$result"
}

main() {
    [[ $# == 1 ]] || { log "Usage: $0 <deb_file>"; return 1; }
    local original_deb=$1 deb_file candidate_version architecture
    [[ -f $original_deb ]] || { log "DEB file not found: $original_deb"; return 1; }
    install -d -m 700 "$CACHE_DIR" "$BACKUP_DIR"
    exec 9>"$CACHE_DIR/update.lock"
    if ! flock -n 9; then log "Another update is already running"; return 1; fi
    # Do not let a second invocation change the active transaction's flags/service.
    trap finish EXIT
    trap 'exit 1' INT TERM
    rm -f "$DATA_DIR/.update_success" "$DATA_DIR/.update_error"
    deb_file="$CACHE_DIR/candidate.deb"
    cp "$original_deb" "$deb_file"
    architecture=$(dpkg --print-architecture)
    [[ $architecture == arm64 ]] || fail "Updates require arm64"
    candidate_version=$(dpkg-deb -f "$deb_file" Version)
    previous_version=$(dpkg-query -W -f '${Version}' led-matrix)
    [[ $candidate_version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ &&
       $previous_version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || fail "Unsupported package version"
    package_matches "$deb_file" "$candidate_version" "$architecture" || fail "Wrong package name or architecture"
    [[ $(systemctl is-enabled "$SERVICE" 2>/dev/null) != masked* ]] || fail "Service is masked"
    previous_deb="$CACHE_DIR/previous.deb"
    if [[ -f $CACHE_DIR/current.deb ]] && package_matches "$CACHE_DIR/current.deb" "$previous_version" "$architecture"; then
        cp -a "$CACHE_DIR/current.deb" "$previous_deb"
    else
        log "Caching rollback package $previous_version before installation"
        curl -fL --connect-timeout 10 --max-time 180 --retry 2 \
            "https://github.com/sshcrack/led-matrix/releases/download/v$previous_version/led-matrix-${previous_version}_arm64.deb" \
            -o "$previous_deb.partial" || fail "Could not obtain rollback package; keeping current installation"
        package_matches "$previous_deb.partial" "$previous_version" "$architecture" || fail "Invalid rollback package"
        mv "$previous_deb.partial" "$previous_deb"
    fi
    # Wait for the old process to flush its last configuration before snapshotting.
    systemctl stop "$SERVICE"
    transaction="$BACKUP_DIR/current"
    rm -rf "$transaction"
    install -d -m 700 "$transaction"
    snapshot_file "$ENV_FILE" environment
    snapshot_file "$DATA_DIR/config.json" config.json
    log "Installing $candidate_version with rollback to $previous_version available"
    systemctl mask --runtime "$SERVICE"
    masked=true
    transaction_started=true
    DEBIAN_FRONTEND=noninteractive dpkg --force-confdef --force-confold -i "$deb_file" || fail "Candidate package installation failed"
    restore_configuration
    systemctl unmask --runtime "$SERVICE"
    masked=false
    systemctl daemon-reload
    systemctl start "$SERVICE" || fail "Candidate service could not start"
    health_check "$candidate_version" || fail "Candidate did not pass service, API version and web UI health checks"
    cp "$deb_file" "$CACHE_DIR/current.deb.partial"
    mv "$CACHE_DIR/current.deb.partial" "$CACHE_DIR/current.deb"
    write_flag .update_success "$(date '+%Y-%m-%d %H:%M:%S')"
    committed=true
    log "Update to $candidate_version passed health checks"
    rm -f "$deb_file"
    if [[ $original_deb != "$CACHE_DIR/"* && $original_deb != "$BACKUP_DIR/"* ]]; then rm -f "$original_deb"; fi
}

# Sourcing provides a safe test seam: fixtures supply temporary paths and fake
# package/service commands, while exercising the real transaction and health loop.
if [[ ${BASH_SOURCE[0]} == "$0" ]]; then main "$@"; fi
