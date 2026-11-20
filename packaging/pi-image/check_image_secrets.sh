#!/usr/bin/env bash
# Fails when a generic panel image carries anything machine- or person-specific:
# SSH host or user keys, private keys, password hashes, Wi-Fi credentials, a
# machine ID, a random seed, shell history, or filled-in led-matrix secrets.
#
#   sudo packaging/pi-image/check_image_secrets.sh <image.img | image.img.xz>
#
# Mounts the partitions read-only through a loop device (root needed).
set -euo pipefail

image=${1:?usage: check_image_secrets.sh <image.img|image.img.xz>}
[[ $(id -u) -eq 0 ]] || { echo "Run as root (loop mounts)." >&2; exit 1; }

work=$(mktemp -d)
loop=
cleanup() {
    set +e
    for m in "$work/data" "$work/boot" "$work/root"; do mountpoint -q "$m" && umount "$m"; done
    [[ -n $loop ]] && losetup -d "$loop"
    rm -rf "$work"
}
trap cleanup EXIT

raw=$image
if [[ $image == *.xz ]]; then
    raw=$work/image.img
    xz -dc "$image" >"$raw"
fi
loop=$(losetup -P -r --show -f "$raw")
udevadm settle 2>/dev/null || true
mkdir -p "$work/root" "$work/boot" "$work/data"
mount -o ro,noload "${loop}p2" "$work/root"
mount -o ro "${loop}p1" "$work/boot"
mount -o ro,noload "${loop}p3" "$work/data"

failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
root=$work/root

# SSH host keys are generated on the first boot.
for f in "$root"/etc/ssh/ssh_host_*; do [[ -e $f ]] && fail "SSH host key ${f#"$root"}"; done

# systemd creates the machine ID on the first boot.
if [[ -s $root/etc/machine-id ]] && ! grep -qx uninitialized "$root/etc/machine-id"; then
    fail "/etc/machine-id is set"
fi
if [[ -f $root/var/lib/dbus/machine-id && ! -L $root/var/lib/dbus/machine-id && -s $root/var/lib/dbus/machine-id ]]; then
    fail "/var/lib/dbus/machine-id is set"
fi
[[ -e $root/var/lib/systemd/random-seed ]] && fail "/var/lib/systemd/random-seed exists"

# No account may have a password or an empty (passwordless) entry.
while IFS=: read -r user hash _; do
    case $hash in
    '*' | '!'* ) ;;
    '') fail "account $user has an empty password" ;;
    *) fail "account $user has a password hash" ;;
    esac
done <"$root/etc/shadow"

# User keys and shell history.
while IFS= read -r -d '' f; do fail "${f#"$root"}"; done < <(
    find "$root/root" "$root/home" \( -name authorized_keys -o -name '.*_history' -o -name 'id_*' \) -print0 2>/dev/null)

# Any private key outside the package files (e.g. a generated snakeoil TLS key).
while IFS= read -r f; do fail "private key in ${f#"$work/"}"; done < <(
    grep -rlI -e '-----BEGIN [A-Z ]*PRIVATE KEY-----' "$root/etc" "$root/root" "$root/home" "$root/var/lib" "$work/data" 2>/dev/null || true)

# Wi-Fi credentials.
for f in "$root"/etc/NetworkManager/system-connections/*; do [[ -e $f ]] && fail "network connection ${f#"$root"}"; done
grep -rqs 'psk=' "$root/etc/wpa_supplicant" && fail "Wi-Fi key in /etc/wpa_supplicant"

# cloud-init seed on the boot partition: Imager writes these; the image's must be empty templates.
for f in user-data network-config; do
    [[ -f $work/boot/$f ]] || continue
    if grep -vE '^\s*(#|$)' "$work/boot/$f" | grep -qiE 'passw|psk|ssh_authorized_keys|ssh_keys|private'; then
        fail "credentials in boot/$f"
    fi
done
[[ -e $root/etc/cloud/cloud-init.disabled ]] && fail "cloud-init is disabled (Imager's settings would be ignored)"
[[ -d $root/var/lib/cloud/instances ]] && [[ -n $(ls -A "$root/var/lib/cloud/instances") ]] && fail "cloud-init instance data present"

# led-matrix secrets on the data partition.
env=$work/data/etc/led-matrix.env
if [[ -f $env ]]; then
    for key in SPOTIFY_CLIENT_SECRET SPOTIFY_CLIENT_ID LED_MATRIX_API_TOKEN; do
        if grep -qE "^(export )?$key=\"?[^\"[:space:]]+" "$env"; then fail "$key is set in led-matrix.env"; fi
    done
fi
[[ -e $work/data/state/config.json ]] && grep -qE '"(access_token|refresh_token)": *"[^"]+' "$work/data/state/config.json" &&
    fail "Spotify tokens in config.json"

if ((failures)); then
    echo "$failures problem(s) found in $image"
    exit 1
fi
echo "No secrets found in $image"
