#!/bin/bash
# Runs in a separate systemd unit: restarting led-matrix must not kill its updater.
#
# Installs a led-matrix .deb as a new version below APP_DIR (see launcher.sh for
# the layout) instead of running dpkg: the package is unpacked into its own
# folder, made durable, and only then `current` is switched with one rename. The
# installed package (/usr/bin/led-matrix) stays as the factory copy. A power cut
# at any point leaves either the old or the new version running; a new version
# that does not come up healthy is switched back, here or by the launcher.
set -Eeuo pipefail

APP_DIR=/var/lib/led-matrix-app
DATA_DIR=/var/lib/led-matrix
LOG_FILE=/var/log/led-matrix-update.log
LAUNCHER=/usr/lib/led-matrix/launcher
SERVICE=led-matrix.service
HEALTH_URL="http://127.0.0.1:${LED_MATRIX_UPDATE_PORT:-8080}"
HEALTH_ATTEMPTS=30
HEALTH_INTERVAL=2
HEALTH_SUCCESSES=3
HEALTH_TIMEOUT=60
switched=false
committed=false
failure_message="Update failed"
previous_version=""

log() { printf '%s [UPDATE] %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*" | tee -a "$LOG_FILE" >&2 || true; }
fail() { failure_message="$*"; log "ERROR: $*"; return 1; }

package_matches() {
    [[ $(dpkg-deb -f "$1" Package) == led-matrix &&
       $(dpkg-deb -f "$1" Version) == "$2" &&
       $(dpkg-deb -f "$1" Architecture) == "$3" ]]
}

# The binary the service runs (as root, /proc/<pid>/exe is readable).
running_exe() { readlink "/proc/$1/exe" 2>/dev/null; }

# <version> [<folder the binary must run from>]: a rebuild of the same version
# that crash-loops is switched back by the launcher, and the old build then
# answers with the same version, so the version alone does not prove the switch.
health_check() {
    local expected_version=$1 expected_dir=${2:-} healthy=0 previous_pid="" pid response
    local deadline=$((SECONDS + HEALTH_TIMEOUT))
    for ((attempt=0; attempt<HEALTH_ATTEMPTS && SECONDS<deadline; ++attempt)); do
        pid=$(systemctl show "$SERVICE" --property=MainPID --value) || pid=""
        if [[ $pid =~ ^[1-9][0-9]*$ ]] && systemctl is-active --quiet "$SERVICE" &&
           { [[ -z $expected_dir ]] || [[ $(running_exe "$pid") == "$expected_dir"/* ]]; } &&
           response=$(api_get /api/update/status) &&
           jq -e --arg version "$expected_version" '.current_version == $version' <<< "$response" >/dev/null &&
           api_get /web/ >/dev/null; then
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

finish() {
    local result=$?
    trap - EXIT INT TERM
    set +e
    if ((result != 0)) && ! $committed; then
        if $switched; then
            log "Switching back to led-matrix $previous_version"
            systemctl stop "$SERVICE"
            "$LAUNCHER" rollback "$failure_message"
            # The launcher may have switched back already (crash loop); its flag
            # can be consumed by the restarted daemon, so record this failure too.
            [[ -f $DATA_DIR/.update_error ]] ||
                write_flag .update_error "$failure_message; rolled back to $previous_version"
            prune_versions # drops the failed version
            systemctl reset-failed "$SERVICE" 2>/dev/null
            if systemctl start "$SERVICE" && health_check "$previous_version"; then
                log "Rolled back to $previous_version"
            else
                write_flag .update_error "$failure_message; rolled back to $previous_version, but it did not pass the health checks"
            fi
        else
            # The old version may have exited for the update: bring it back.
            systemctl start "$SERVICE"
            write_flag .update_error "$failure_message"
        fi
        rm -f "$DATA_DIR/.update_success"
        log "Update not applied: $failure_message"
    fi
    rm -f "$APP_DIR/candidate.deb"
    rm -rf "$APP_DIR"/versions/.partial-*
    exit "$result"
}

# led-matrix-update.path hands over `--request <file>`: line 1 is the downloaded package, line 2 the
# API port for the health check. The daemon runs unprivileged (NoNewPrivileges, so no sudo); this unit
# is the only root step, hence the strict validation of what the request may point at.
resolve_request() {
    local request=$1 requested_deb="" requested_port="" real
    [[ -f $request && ! -L $request ]] || { log "Invalid update request: $request"; return 1; }
    { read -r requested_deb || true; read -r requested_port || true; } < "$request"
    rm -f "$request"
    if [[ $requested_port =~ ^[1-9][0-9]{0,4}$ ]] && ((requested_port <= 65535)); then
        HEALTH_URL="http://127.0.0.1:$requested_port"
    fi
    real=$(realpath -e -- "$requested_deb" 2>/dev/null) || { log "Update package not found: $requested_deb"; return 1; }
    [[ $real == "$DATA_DIR/update/"* && -f $real && ! -L $requested_deb ]] ||
        { log "Update package must be a regular file below $DATA_DIR/update/: $real"; return 1; }
    REQUESTED_DEB=$real
}

# Keep only what runs now and what it would fall back to.
prune_versions() {
    local current previous dir keep
    current=$(link_target current) previous=$(link_target previous)
    for dir in "$APP_DIR"/versions/*; do
        [[ -e $dir ]] || continue
        keep="versions/$(basename "$dir")"
        [[ $keep == "$current" ]] && continue
        [[ $keep == "$previous" ]] && is_complete "$current" && continue
        rm -rf "$dir"
    done
}

# Unpack the package as versions/<version>-<hash> (reused when it is already
# there, so a retry or redeploying the same build needs no space).
install_version() { # <deb> <version>
    local deb=$1 version=$2 hash name partial need avail keep current previous dir
    hash=$(sha256sum "$deb" | cut -c1-12)
    name="versions/$version-$hash"
    is_complete "$name" && { INSTALLED=$name; return 0; }

    prune_versions
    current=$(link_target current) previous=$(link_target previous)
    need=$(( $(dpkg-deb -f "$deb" Installed-Size) * 11 / 10 + 10240 )) # KiB
    avail=$(df -Pk "$APP_DIR" | awk 'NR == 2 { print $4 }')
    if ((avail < need)) && [[ -n $previous && $previous != "$current" ]] && is_complete "$current"; then
        rm -rf "${APP_DIR:?}/$previous"
        avail=$(df -Pk "$APP_DIR" | awk 'NR == 2 { print $4 }')
    fi
    ((avail >= need)) || fail "Not enough space for the update: $((need / 1024)) MiB needed, $((avail / 1024)) MiB free"

    partial=$(mktemp -d "$APP_DIR/versions/.partial-XXXXXX")
    dpkg-deb -x "$deb" "$partial" || fail "Could not unpack the update"
    [[ -x $partial/usr/bin/led-matrix ]] || fail "The update contains no usr/bin/led-matrix"
    printf '%s\n' "$version" >"$partial/.version"
    chmod 755 "$partial"
    sync -f "$partial"
    rm -rf "${APP_DIR:?}/$name"
    mv -T "$partial" "$APP_DIR/$name"
    sync "$APP_DIR/versions"
    INSTALLED=$name
}

main() {
    if [[ ${1:-} == --request ]]; then
        [[ $# == 2 ]] || { log "Usage: $0 --request <file>"; return 1; }
        resolve_request "$2" || return 1
        set -- "$REQUESTED_DEB"
    fi
    [[ $# == 1 ]] || { log "Usage: $0 <deb_file>"; return 1; }
    local original_deb=$1 deb_file candidate_version architecture current
    [[ -f $original_deb ]] || { log "DEB file not found: $original_deb"; return 1; }
    [[ -x $LAUNCHER ]] ||
        { log "This system has no $LAUNCHER yet: install this package once with dpkg"; return 1; }
    # The launcher holds the shared helpers (links, completeness, flags).
    APP_DIR=$APP_DIR DATA_DIR=$DATA_DIR source "$LAUNCHER"
    install -d -m 755 "$APP_DIR" "$APP_DIR/versions"
    exec 9>>"$APP_DIR/update.lock"
    if ! flock -n 9; then log "Another update is already running"; return 1; fi
    # Do not let a second invocation change the active transaction's flags/service.
    trap finish EXIT
    trap 'exit 1' INT TERM
    rm -f "$DATA_DIR/.update_success" "$DATA_DIR/.update_error"
    architecture=$(dpkg --print-architecture)
    [[ $architecture == arm64 ]] || fail "Updates require arm64"
    # On the read-only panel card APP_DIR is the data partition. If that is
    # missing, the folder is part of the RAM overlay and the update would vanish.
    [[ $(findmnt -n -o FSTYPE -T "$APP_DIR") != overlay ]] ||
        fail "The app folder $APP_DIR is not on persistent storage (is the data partition mounted?)"
    deb_file="$APP_DIR/candidate.deb"
    cp "$original_deb" "$deb_file"
    candidate_version=$(dpkg-deb -f "$deb_file" Version)
    current=$(link_target current)
    is_complete "$current" || current=""
    previous_version=$(version_of "$current")
    [[ $candidate_version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ &&
       $previous_version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || fail "Unsupported package version"
    package_matches "$deb_file" "$candidate_version" "$architecture" || fail "Wrong package name or architecture"
    [[ $(systemctl is-enabled "$SERVICE" 2>/dev/null) != masked* ]] || fail "Service is masked"

    log "Unpacking $candidate_version"
    install_version "$deb_file" "$candidate_version"
    rm -f "$deb_file"
    if [[ $INSTALLED == "$current" ]]; then
        log "This build of $candidate_version is already installed"
        systemctl start "$SERVICE"
        write_flag .update_success "$(date '+%Y-%m-%d %H:%M:%S')"
        committed=true
        return 0
    fi

    # Wait for the old process to flush its last configuration before snapshotting.
    systemctl stop "$SERVICE"
    log "Switching to $candidate_version, $previous_version stays as the fallback"
    if [[ -f $DATA_DIR/config.json ]]; then
        copy_file "$DATA_DIR/config.json" "$APP_DIR/trial-config.json"
    else
        rm -f "$APP_DIR/trial-config.json"
    fi
    write_file "$APP_DIR/trial" "$INSTALLED 0"
    if [[ -n $current ]]; then switch_link previous "$current"; else rm -f "$APP_DIR/previous"; fi
    switch_link current "$INSTALLED"
    switched=true
    systemctl reset-failed "$SERVICE" 2>/dev/null || true
    systemctl start "$SERVICE" || fail "Candidate service could not start"
    health_check "$candidate_version" "$APP_DIR/$INSTALLED" ||
        fail "Candidate did not pass service, binary, API version and web UI health checks"
    rm -f "$APP_DIR/trial" "$APP_DIR/trial-config.json"
    sync "$APP_DIR"
    prune_versions
    write_flag .update_success "$(date '+%Y-%m-%d %H:%M:%S')"
    committed=true
    log "Update to $candidate_version passed health checks"
    if [[ $original_deb == "$DATA_DIR/update/"* ]]; then
        rm -f "$original_deb"
        rmdir "$(dirname "$original_deb")" 2>/dev/null || true
    fi
}

# Sourcing provides a safe test seam: fixtures supply temporary paths and fake
# package/service commands, while exercising the real transaction and health loop.
if [[ ${BASH_SOURCE[0]} == "$0" ]]; then main "$@"; fi
