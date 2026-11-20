#!/bin/bash
# Root side of recovery mode (docs/RECOVERY.md). The unprivileged recovery
# daemon writes one action name to $RUN_DIR/request; led-matrix-recovery.path
# starts this script, which reports progress in $RUN_DIR/state.json.
#
#   repair   e2fsck the data partition and mount it again
#   reset    copy settings off the data partition, reformat it, put them back
#   retry    leave recovery mode: forget the crash count, restart the service
#
# Installed as /usr/lib/led-matrix/recovery_action.sh.
set -uo pipefail

RUN_DIR=${RUN_DIR:-/run/led-matrix-recovery}
WORK_DIR=${WORK_DIR:-/run/led-matrix-recovery-root}
COUNTER=${COUNTER:-/run/led-matrix/starts}
DATA_MOUNT=${DATA_MOUNT:-/srv/led-matrix}
DATA_DIR=${DATA_DIR:-/var/lib/led-matrix}
APP_DIR=${APP_DIR:-/var/lib/led-matrix-app}
FSTAB=${FSTAB:-/etc/fstab}
FALLBACK_ENV=${FALLBACK_ENV:-/etc/led-matrix/fallback.env}
DAEMON_USER=${DAEMON_USER:-pi}
SERVICE=led-matrix.service
# Uploads bigger than this stay behind on a reset (they are copied to RAM).
RESCUE_UPLOADS_MIB=${RESCUE_UPLOADS_MIB:-100}
LABEL=LEDDATA

log() { printf '[recovery] %s\n' "$*" >&2; }

# state.json in the daemon's directory: written via a new file and rename, so a
# link planted there is replaced, never followed.
report() { # <running|done|failed> <message> [progress]
    local temporary
    temporary=$(mktemp "$RUN_DIR/.state.XXXXXX") || return 0
    jq -n --arg action "$ACTION" --arg state "$1" --arg message "$2" --argjson progress "${3:--1}" \
        '{action: $action, state: $state, message: $message, progress: $progress}' >"$temporary"
    chmod 644 "$temporary"
    mv -fT "$temporary" "$RUN_DIR/state.json"
    log "$ACTION: $1: $2"
}
fail() { report failed "$1"; exit 1; }

is_panel_card() { grep -qE "^[[:space:]]*LABEL=$LABEL[[:space:]]" "$FSTAB"; }

system_device() { findmnt -n -o SOURCE /media/root-ro 2>/dev/null || findmnt -n -o SOURCE /; }

is_block_device() { [[ -b $1 ]]; }

# The data partition: by label, or (label gone with the filesystem) partition 3
# of the disk the system runs from. Never the system or boot partition.
data_device() {
    local device system disk
    system=$(system_device)
    device=$(blkid -L "$LABEL" 2>/dev/null) || device=""
    if [[ -z $device ]]; then
        disk=$(lsblk -no PKNAME "$system" 2>/dev/null | head -n1)
        [[ -n $disk ]] || return 1
        device=$(lsblk -nlpo NAME,PARTN "/dev/$disk" | awk '$2 == 3 { print $1 }')
    fi
    is_block_device "$device" && [[ $device != "$system" && $device != "$(findmnt -n -o SOURCE /boot/firmware 2>/dev/null)" ]] || return 1
    printf '%s\n' "$device"
}

unmount_data() {
    local mount
    for mount in "$APP_DIR" "$DATA_DIR" "$DATA_MOUNT"; do
        if mountpoint -q "$mount"; then
            umount "$mount" || fail "$mount is in use and cannot be unmounted"
        fi
    done
}

mount_data() {
    mount "$DATA_MOUNT" || return 1
    mkdir -p "$DATA_MOUNT/state" "$DATA_MOUNT/etc" "$DATA_MOUNT/app"
    chown root:root "$DATA_MOUNT/app" "$DATA_MOUNT/etc"
    chmod 755 "$DATA_MOUNT" "$DATA_MOUNT/app" "$DATA_MOUNT/etc"
    mount "$DATA_DIR" && mount "$APP_DIR"
}

data_writable() { local probe=$DATA_DIR/.recovery-probe; : >"$probe" 2>/dev/null && rm -f "$probe"; }

repair() {
    is_panel_card || fail "Repair needs the panel card's data partition"
    local device rc
    report running "Looking for the data partition" 5
    device=$(data_device) || fail "No data partition found"
    unmount_data
    report running "Checking and repairing $device" 20
    e2fsck -fy "$device" >&2
    rc=$?
    # 0: clean, 1: errors fixed, 2: fixed, reboot advised, 3: 1+2; 4 and up: not fixed.
    ((rc < 4)) || fail "The check could not repair $device (e2fsck exit code $rc); try Reset storage"
    report running "Mounting the data partition" 80
    mount_data || fail "$device was repaired but cannot be mounted; try Reset storage"
    data_writable || fail "$device was repaired but stays read-only; try Reset storage"
    report done "Storage repaired; starting the panel" 100
    retry
}

reset() {
    is_panel_card || fail "Reset needs the panel card's data partition"
    local device old rescue kept=0 file uploads_mib
    report running "Looking for the data partition" 5
    device=$(data_device) || fail "No data partition found"
    unmount_data
    rm -rf "$WORK_DIR"
    install -d -m 700 "$WORK_DIR" "$WORK_DIR/old" "$WORK_DIR/rescue"
    old=$WORK_DIR/old rescue=$WORK_DIR/rescue
    report running "Saving settings from $device" 15
    if mount -o ro "$device" "$old" 2>/dev/null; then
        for file in state/config.json state/config.json.last-good etc/led-matrix.env; do
            if [[ -f $old/$file && ! -L $old/$file ]]; then
                install -D -m 600 "$old/$file" "$rescue/$file" && kept=$((kept + 1))
            fi
        done
        if [[ -d $old/state/data ]]; then
            uploads_mib=$(du -sm "$old/state/data" 2>/dev/null | cut -f1)
            if [[ -n $uploads_mib ]] && ((uploads_mib <= RESCUE_UPLOADS_MIB)); then
                cp -a "$old/state/data" "$rescue/state/" 2>/dev/null && kept=$((kept + 1))
            else
                log "Uploads (${uploads_mib:-?} MiB) are too big to keep"
            fi
        fi
        umount "$old"
    else
        log "$device cannot be mounted; nothing to save"
    fi
    report running "Formatting $device" 40
    mkfs.ext4 -F -q -L "$LABEL" "$device" >&2 || fail "Formatting $device failed; the card may be worn out"
    report running "Restoring settings" 75
    mount_data || fail "$device was formatted but cannot be mounted"
    if [[ -d $rescue/state ]]; then cp -a "$rescue/state/." "$DATA_DIR/"; fi
    if [[ -f $rescue/etc/led-matrix.env ]]; then
        install -m 600 "$rescue/etc/led-matrix.env" "$DATA_MOUNT/etc/led-matrix.env"
    elif [[ -f $FALLBACK_ENV ]]; then
        # Panel layout only; Spotify and the API token have to be entered again.
        install -m 600 "$FALLBACK_ENV" "$DATA_MOUNT/etc/led-matrix.env"
    fi
    chown -R "$DAEMON_USER:$DAEMON_USER" "$DATA_DIR" 2>/dev/null || true
    rm -rf "$WORK_DIR"
    data_writable || fail "$device was formatted but stays read-only; the card may be worn out"
    report done "Storage reset ($kept settings items kept); starting the panel" 100
    retry
}

retry() {
    rm -f "$COUNTER"
    systemctl reset-failed "$SERVICE" 2>/dev/null || true
    systemctl restart --no-block "$SERVICE"
}

main() {
    local request=$RUN_DIR/request
    ACTION=unknown
    [[ -f $request && ! -L $request ]] || { log "No request"; return 1; }
    read -r ACTION <"$request" || true
    rm -f "$request"
    case $ACTION in
    repair) repair ;;
    reset) reset ;;
    retry) report done "Starting the panel"; retry ;;
    *) log "Unknown action '$ACTION'"; return 1 ;;
    esac
}

if [[ ${BASH_SOURCE[0]} == "$0" ]]; then main "$@"; fi
