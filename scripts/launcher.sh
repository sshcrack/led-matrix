#!/bin/bash
# Starts the led-matrix version the panel should run, and owns the switch back
# after a bad update. Installed as /usr/lib/led-matrix/launcher; led-matrix.service
# runs `launcher run $MATRIX_OPTS` as root.
#
# Updates are unpacked below APP_DIR (on the panel card, the data partition):
#   versions/<version>-<hash>/   a complete install tree (usr/bin/led-matrix, ...)
#                                with .version; only complete trees are renamed here
#   current -> versions/...      what runs; switched with rename(2), so a power cut
#   previous -> versions/...     leaves either the old or the new link
#   trial                        "<version dir> <starts>" while a new version is on
#                                trial; root-owned like everything here
#   trial-config.json            config.json from before the switch, for rollback
#
# Order: current, then previous, then the factory install from the package
# (/usr/bin/led-matrix, never touched by updates).
#
#   launcher run [args...]        pick a version and exec it
#   launcher rollback <reason>    end a trial: switch back and record why
set -uo pipefail

APP_DIR=${APP_DIR:-/var/lib/led-matrix-app}
DATA_DIR=${DATA_DIR:-/var/lib/led-matrix}
FACTORY=${FACTORY:-/usr/bin/led-matrix}
# A new version gets this many service starts (crash loop, power cuts) to report
# healthy before the launcher switches back.
MAX_TRIAL_STARTS=${MAX_TRIAL_STARTS:-3}
CONFIRM_ATTEMPTS=${CONFIRM_ATTEMPTS:-60}
CONFIRM_INTERVAL=${CONFIRM_INTERVAL:-2}
CONFIRM_SUCCESSES=${CONFIRM_SUCCESSES:-3}
UPDATE_LOCK=${UPDATE_LOCK:-$APP_DIR/update.lock}
HEALTH_URL=${HEALTH_URL:-http://127.0.0.1:${PORT:-8080}}

say() { printf '[launcher] %s\n' "$*" >&2; }

# Durable small-file replace inside APP_DIR or DATA_DIR.
write_file() { # <path> <content> [owner]
    local temporary
    temporary=$(mktemp "$(dirname "$1")/.$(basename "$1").XXXXXX") || return 1
    printf '%s\n' "$2" >"$temporary" && sync "$temporary" || { rm -f "$temporary"; return 1; }
    [[ -z ${3:-} ]] || chown "$3" "$temporary" 2>/dev/null || true
    mv -fT "$temporary" "$1" && sync "$(dirname "$1")"
}

# Durable byte-exact copy, replacing <destination> atomically.
copy_file() { # <source> <destination> [owner]
    local temporary
    temporary=$(mktemp "$(dirname "$2")/.$(basename "$2").XXXXXX") || return 1
    cp -- "$1" "$temporary" && sync "$temporary" || { rm -f "$temporary"; return 1; }
    [[ -z ${3:-} ]] || chown "$3" "$temporary" 2>/dev/null || true
    mv -fT "$temporary" "$2" && sync "$(dirname "$2")"
}

# Point <link> at <target> (relative to APP_DIR) atomically.
switch_link() { # <link> <target>
    ln -sfn "$2" "$APP_DIR/.$1.new" && mv -fT "$APP_DIR/.$1.new" "$APP_DIR/$1" && sync "$APP_DIR"
}

link_target() { # <link> -> "versions/<dir>" or empty
    local target
    target=$(readlink "$APP_DIR/$1" 2>/dev/null) || return 0
    [[ $target =~ ^versions/[A-Za-z0-9._+-]+$ ]] && printf '%s\n' "$target"
}

is_complete() { # <versions/dir>
    [[ -n $1 && -f $APP_DIR/$1/.version && -x $APP_DIR/$1/usr/bin/led-matrix ]]
}

version_of() { # <versions/dir or empty for factory>
    if [[ -n $1 ]]; then head -n1 "$APP_DIR/$1/.version"; else dpkg-query -W -f '${Version}' led-matrix 2>/dev/null; fi
}

write_flag() { rm -f "$DATA_DIR/.update_success"; write_file "$DATA_DIR/$1" "$2" pi:pi; }

# The new version (current) failed: run previous again, or the factory install.
rollback() { # <reason>
    local reason=$1 previous back
    [[ -f $APP_DIR/trial ]] || { say "No version on trial; nothing to roll back"; return 0; }
    previous=$(link_target previous)
    if is_complete "$previous"; then
        switch_link current "$previous"
    else
        previous=""
        rm -f "$APP_DIR/current" && sync "$APP_DIR"
    fi
    back=$(version_of "$previous")
    if [[ -f $APP_DIR/trial-config.json ]]; then
        # The new version may have migrated config.json in a way the old one cannot read.
        copy_file "$APP_DIR/trial-config.json" "$DATA_DIR/config.json" pi:pi &&
            rm -f "$APP_DIR/trial-config.json"
    fi
    rm -f "$APP_DIR/trial" && sync "$APP_DIR"
    write_flag .update_error "$reason; rolled back to ${back:-the installed package}"
    say "$reason; rolled back to ${back:-the installed package}"
    ROLLED_BACK_TO=${back:-}
}

api_get() { # <path>; authenticates like the web UI when an API token is set
    if [[ -n ${LED_MATRIX_API_TOKEN:-} ]]; then
        curl --noproxy '*' -fsS --max-time 3 -H @<(printf 'X-Api-Token: %s\n' "$LED_MATRIX_API_TOKEN") "$HEALTH_URL$1"
    else
        curl --noproxy '*' -fsS --max-time 3 "$HEALTH_URL$1"
    fi
}

# Runs next to the service (same cgroup, so it dies with it): once the version
# on trial answers with its own version a few times in a row, it is kept. An
# update transaction in progress makes that decision itself.
confirm_trial() { # <versions/dir> <version>
    local healthy=0 response
    for ((attempt = 0; attempt < CONFIRM_ATTEMPTS; ++attempt)); do
        sleep "$CONFIRM_INTERVAL"
        if response=$(api_get /api/update/status) &&
            jq -e --arg version "$2" '.current_version == $version' <<<"$response" >/dev/null &&
            api_get /web/ >/dev/null; then
            healthy=$((healthy + 1))
            if ((healthy >= CONFIRM_SUCCESSES)); then
                exec 8>>"$UPDATE_LOCK"
                flock -n 8 || return 0
                local trial=""
                [[ -f $APP_DIR/trial ]] && read -r trial _ <"$APP_DIR/trial"
                if [[ $trial == "$1" ]]; then
                    rm -f "$APP_DIR/trial" "$APP_DIR/trial-config.json" && sync "$APP_DIR"
                    say "Version $2 is healthy; keeping it"
                fi
                return 0
            fi
        else
            healthy=0
        fi
    done
}

run() {
    local current trial="" starts=0
    current=$(link_target current)
    if [[ -f $APP_DIR/trial ]]; then
        read -r trial starts <"$APP_DIR/trial" || true
        [[ $starts =~ ^[0-9]+$ ]] || starts=0
        if [[ $trial != "$current" ]] || ! is_complete "$current"; then
            # Cut before the switch, or the version folder is gone: nothing on trial.
            rm -f "$APP_DIR/trial" "$APP_DIR/trial-config.json"
            trial=""
        elif ((starts >= MAX_TRIAL_STARTS)); then
            rollback "Version $(version_of "$current") did not start healthy $MAX_TRIAL_STARTS times"
            current=$(link_target current)
            trial=""
        else
            write_file "$APP_DIR/trial" "$trial $((starts + 1))" ||
                say "Could not count the start of the version on trial"
        fi
    fi

    local candidate exe=""
    for candidate in "$current" "$(link_target previous)"; do
        if is_complete "$candidate"; then exe=$APP_DIR/$candidate/usr/bin/led-matrix; break; fi
        [[ -z $candidate ]] || say "Skipping incomplete version $candidate"
    done
    if [[ -z $exe ]]; then
        exe=$FACTORY
        trial=""
    fi
    if [[ -n $trial && $exe == "$APP_DIR/$trial/"* ]]; then
        confirm_trial "$trial" "$(version_of "$trial")" </dev/null &
    fi
    say "Starting $exe"
    exec "$exe" "$@"
}

main() {
    case ${1:-} in
    run) shift; run "$@" ;;
    rollback) [[ $# == 2 ]] || { say "Usage: $0 rollback <reason>"; return 2; }; rollback "$2" ;;
    *) say "Usage: $0 run [args...] | rollback <reason>"; return 2 ;;
    esac
}

if [[ ${BASH_SOURCE[0]} == "$0" ]]; then main "$@"; fi
