#!/bin/bash
# Root side of the panel hardware settings (#233). The unprivileged daemon
# writes $DIR/request.json ({"options": {...}, "port": "8080"});
# led-matrix-hardware.path starts this script, which
#   1. validates the options again (same whitelist as src_matrix/hardware/panel_options.cpp),
#   2. rewrites MATRIX_OPTS in /etc/default/led-matrix: the managed --led-* flags
#      are replaced, every other flag (drop-priv, --led-no-hardware-pulse, ...) is kept,
#   3. updates the debconf answers, so `dpkg-reconfigure led-matrix` writes the same,
#   4. restarts the service and waits until it answers with the new options,
#   5. restores the previous file when it does not.
# The result is $DIR/state.json ({state, message, flags}), read by GET /hardware.
#
# Installed as /usr/lib/led-matrix/apply_hardware.sh.
set -uo pipefail

DIR=${DIR:-/var/lib/led-matrix/hardware}
ENV_FILE=${ENV_FILE:-/etc/default/led-matrix}
DAEMON_USER=${DAEMON_USER:-pi}
SERVICE=led-matrix.service
SYSTEMCTL=${SYSTEMCTL:-systemctl}
DEBCONF_SET=${DEBCONF_SET:-debconf-set-selections}
HEALTH_TIMEOUT=${HEALTH_TIMEOUT:-90}
HEALTH_INTERVAL=${HEALTH_INTERVAL:-2}
REQUEST=$DIR/request.json

log() { printf '[hardware] %s\n' "$*" >&2; }

# state.json in the daemon's directory: a new file renamed into place, so a link
# planted there is replaced, never followed.
report() { # <applied|failed> <message> [flags]
    local temporary
    temporary=$(mktemp "$DIR/.state.XXXXXX") || return 0
    jq -n --arg state "$1" --arg message "$2" --arg flags "${3:-}" --arg at "$(date -Is)" \
        '{state: $state, message: $message, flags: $flags, at: $at}' >"$temporary"
    chmod 644 "$temporary"
    chown "$DAEMON_USER:" "$temporary" 2>/dev/null || true
    mv -fT "$temporary" "$DIR/state.json"
    log "$1: $2"
}
fail() { report failed "$1"; exit 1; }

# --- Request ------------------------------------------------------------------
[[ -f $REQUEST && ! -L $REQUEST ]] || { log "no request"; exit 0; }
(($(stat -c %s "$REQUEST") <= 4096)) || { rm -f "$REQUEST"; fail "The request is too large"; }
request=$(jq -c . <"$REQUEST" 2>/dev/null) || request=
rm -f "$REQUEST"
[[ -n $request ]] && jq -e '.options | type == "object"' <<<"$request" >/dev/null || fail "The request is not valid JSON with options"

port=$(jq -r '.port // "8080" | tostring' <<<"$request")
[[ $port =~ ^[1-9][0-9]{0,4}$ ]] && ((port <= 65535)) || port=8080
HEALTH_URL=${HEALTH_URL:-http://127.0.0.1:$port}

# --- Validation (keep in sync with panel_options.cpp) -------------------------------
declare -A defaults=([rows]=64 [cols]=64 [chain]=1 [parallel]=1 [slowdown]=1 [gpio_mapping]=regular [pixel_mapper]=""
    [multiplexing]=0 [row_addr_type]=0 [panel_type]="" [pwm_bits]=11 [pwm_lsb_nanoseconds]=130 [pwm_dither_bits]=0 [rgb_sequence]=RGB)
declare -A low=([rows]=8 [cols]=16 [chain]=1 [parallel]=1 [slowdown]=0 [multiplexing]=0 [row_addr_type]=0 [pwm_bits]=1
    [pwm_lsb_nanoseconds]=50 [pwm_dither_bits]=0)
declare -A high=([rows]=64 [cols]=256 [chain]=16 [parallel]=3 [slowdown]=5 [multiplexing]=32 [row_addr_type]=5 [pwm_bits]=11
    [pwm_lsb_nanoseconds]=3000 [pwm_dither_bits]=2)
mapper_part='(Rotate:(0|90|180|270)|Mirror:[HV]|U-mapper|V-mapper(:Z)?)'

while IFS= read -r key; do
    [[ -v defaults[$key] ]] || fail "Unknown setting $key"
done < <(jq -r '.options | keys[]' <<<"$request")

declare -A value
for key in "${!defaults[@]}"; do
    if [[ -v low[$key] ]]; then
        v=$(jq -r --arg k "$key" '.options[$k] | if . == null then "" elif type == "number" and . == floor then tostring else "!" end' <<<"$request")
        [[ $v == "" ]] && v=${defaults[$key]}
        [[ $v =~ ^-?[0-9]+$ ]] || fail "$key must be a whole number"
        ((v >= low[$key] && v <= high[$key])) || fail "$key must be ${low[$key]}..${high[$key]}"
    else
        # Every string is checked against a fixed list or pattern below.
        case $(jq -r --arg k "$key" '.options[$k] | type' <<<"$request") in
        null) v=${defaults[$key]} ;;
        string) v=$(jq -r --arg k "$key" '.options[$k]' <<<"$request") ;;
        *) fail "$key must be a string" ;;
        esac
    fi
    value[$key]=$v
done
case ${value[gpio_mapping]} in regular | adafruit-hat | adafruit-hat-pwm | regular-pi1 | classic | classic-pi1 | compute-module) ;;
*) fail "unknown gpio_mapping" ;; esac
case ${value[panel_type]} in "" | FM6126A | FM6127) ;; *) fail "panel_type must be empty, FM6126A or FM6127" ;; esac
case ${value[rgb_sequence]} in RGB | RBG | GRB | GBR | BRG | BGR) ;; *) fail "rgb_sequence must be an order of R, G and B" ;; esac
[[ -z ${value[pixel_mapper]} || ${value[pixel_mapper]} =~ ^$mapper_part(\;$mapper_part){0,3}$ ]] || fail "invalid pixel_mapper"

# Same order as PanelHardware::to_flags(), so the health check can compare them.
flags="--led-rows=${value[rows]} --led-cols=${value[cols]} --led-chain=${value[chain]} --led-parallel=${value[parallel]}"
flags+=" --led-slowdown-gpio=${value[slowdown]} --led-gpio-mapping=${value[gpio_mapping]}"
[[ -n ${value[pixel_mapper]} ]] && flags+=" --led-pixel-mapper=${value[pixel_mapper]}"
flags+=" --led-multiplexing=${value[multiplexing]} --led-row-addr-type=${value[row_addr_type]}"
[[ -n ${value[panel_type]} ]] && flags+=" --led-panel-type=${value[panel_type]}"
flags+=" --led-pwm-bits=${value[pwm_bits]} --led-pwm-lsb-nanoseconds=${value[pwm_lsb_nanoseconds]}"
flags+=" --led-pwm-dither-bits=${value[pwm_dither_bits]} --led-rgb-sequence=${value[rgb_sequence]}"

# --- Environment file --------------------------------------------------------
real=$(readlink -f -- "$ENV_FILE") || fail "Cannot resolve $ENV_FILE"
current=""
if [[ -f $real ]]; then
    current=$(sed -n 's/^MATRIX_OPTS="\(.*\)"[[:space:]]*$/\1/p' "$real" | head -n1)
fi

managed=" rows cols chain parallel slowdown-gpio gpio-mapping hardware-mapping pixel-mapper multiplexing row-addr-type panel-type pwm-bits pwm-lsb-nanoseconds pwm-dither-bits rgb-sequence "
read -ra tokens <<<"$current"
kept=()
for ((i = 0; i < ${#tokens[@]}; i++)); do
    token=${tokens[i]}
    if [[ $token =~ ^--led-([a-z0-9-]+)(=.*)?$ && $managed == *" ${BASH_REMATCH[1]} "* ]]; then
        # "--led-pixel-mapper Rotate:270": the value is the next word.
        if [[ -z ${BASH_REMATCH[2]} && $((i + 1)) -lt ${#tokens[@]} && ${tokens[i + 1]} != -* ]]; then i=$((i + 1)); fi
        continue
    fi
    kept+=("$token")
done
[[ " ${kept[*]} " == *" --led-drop-priv-user="* ]] || kept+=("--led-drop-priv-user=$DAEMON_USER")
[[ " ${kept[*]} " == *" --led-drop-priv-group="* ]] || kept+=("--led-drop-priv-group=$DAEMON_USER")
new_opts="$flags ${kept[*]}"

write_env() { # <file content source: "new" or a backup path>
    local temporary
    temporary=$(mktemp "$(dirname "$real")/.led-matrix.env.XXXXXX") || return 1
    chmod 600 "$temporary"
    if [[ $1 == new ]]; then
        if [[ -f $real ]] && grep -q '^MATRIX_OPTS=' "$real"; then
            awk -v line="MATRIX_OPTS=\"$new_opts\"" '/^MATRIX_OPTS=/ && !done { print line; done = 1; next } { print }' "$real" >"$temporary"
        else
            { [[ -f $real ]] && cat "$real"; printf 'MATRIX_OPTS="%s"\n' "$new_opts"; } >"$temporary"
        fi
    else
        cat "$1" >"$temporary"
    fi
    sync "$temporary" && mv -f "$temporary" "$real" && sync "$(dirname "$real")"
}

backup=$(mktemp) || fail "No temporary file"
trap 'rm -f "$backup"' EXIT
had_file=false
if [[ -f $real ]]; then cp "$real" "$backup"; had_file=true; fi
write_env new || fail "Could not write $real"
log "MATRIX_OPTS=\"$new_opts\""

# --- debconf: dpkg-reconfigure / package upgrades write the same --------------
if command -v "$DEBCONF_SET" >/dev/null 2>&1; then
    extra=()
    read -ra words <<<"$new_opts"
    for word in "${words[@]}"; do
        case $word in --led-rows=* | --led-cols=* | --led-chain=* | --led-parallel=* | --led-slowdown-gpio=* | --led-drop-priv-*) ;; *) extra+=("$word") ;; esac
    done
    "$DEBCONF_SET" <<EOF || log "debconf answers not updated"
led-matrix led-matrix/led-rows string ${value[rows]}
led-matrix led-matrix/led-cols string ${value[cols]}
led-matrix led-matrix/led-chain string ${value[chain]}
led-matrix led-matrix/led-parallel string ${value[parallel]}
led-matrix led-matrix/led-slowdown-gpio string ${value[slowdown]}
led-matrix led-matrix/extra-opts string ${extra[*]}
EOF
fi

# --- Restart and check ---------------------------------------------------------
api_get() {
    if [[ -n ${LED_MATRIX_API_TOKEN:-} ]]; then
        curl --noproxy '*' -fsS --max-time 3 -H @<(printf 'X-Api-Token: %s\n' "$LED_MATRIX_API_TOKEN") "$HEALTH_URL$1"
    else
        curl --noproxy '*' -fsS --max-time 3 "$HEALTH_URL$1"
    fi
}
answers_with() { # <flags>
    local deadline=$((SECONDS + HEALTH_TIMEOUT)) response
    while ((SECONDS < deadline)); do
        if response=$(api_get /hardware 2>/dev/null) && jq -e --arg flags "$1" '.flags == $flags' <<<"$response" >/dev/null; then
            return 0
        fi
        sleep "$HEALTH_INTERVAL"
    done
    return 1
}

"$SYSTEMCTL" restart "$SERVICE" || log "restart failed"
if answers_with "$flags"; then
    report applied "The panel restarted with the new settings" "$flags"
    exit 0
fi

log "The panel did not come back with the new settings; restoring the previous ones"
if $had_file; then write_env "$backup"; else rm -f "$real"; fi
"$SYSTEMCTL" restart "$SERVICE" || true
fail "The panel did not start with these settings, so the previous ones were restored"
