#!/usr/bin/env bash
# Copy what a new panel card must keep from a running led-matrix Pi:
# app data, settings answers, Wi-Fi, SSH keys and the firewall.
#
#   packaging/pi-image/export_state.sh <ssh-host> <out-dir>
#
# <out-dir> holds secrets (Wi-Fi password, Spotify secret, API token, SSH host
# keys). It is created mode 0700; delete it once the card is flashed.
set -euo pipefail

host=${1:?usage: export_state.sh <ssh-host> <out-dir>}
out=${2:?usage: export_state.sh <ssh-host> <out-dir>}

umask 077
mkdir -p "$out"
chmod 700 "$out"

remote() { ssh -o BatchMode=yes "$host" "$@"; }

echo "Exporting state from $host into $out"

# App data: config.json, its last-good copy, uploads, images, crash reports.
# Caches are rebuilt and stay behind.
remote 'sudo -n tar -C /var/lib/led-matrix --exclude=./cache --exclude=./update -czf - .' >"$out/led-matrix-data.tar.gz"

# Settings answers (panel size, extra options, Spotify, API token, auto-update).
remote 'sudo -n debconf-get-selections' | awk '$1 == "led-matrix"' >"$out/debconf.txt"
# The generated environment file, kept for reference and as a fallback.
remote 'sudo -n cat /etc/default/led-matrix' >"$out/led-matrix.env"

# Wi-Fi of the first NetworkManager Wi-Fi connection, as JSON.
remote 'sudo -n sh -c "grep -l \"^type=wifi\" /etc/NetworkManager/system-connections/*.nmconnection | head -n1 | xargs cat"' |
    python3 -I -c '
import configparser, json, sys
c = configparser.ConfigParser(interpolation=None)
c.read_string(sys.stdin.read())
json.dump({"ssid": c["wifi"]["ssid"], "psk": c["wifi-security"]["psk"]}, sys.stdout)
' >"$out/wifi.json"

# SSH: who may log in, and the host keys so known_hosts stays valid.
remote 'cat ~/.ssh/authorized_keys' >"$out/authorized_keys"
mkdir -p "$out/ssh_host_keys"
for key in ed25519 rsa ecdsa; do
    if remote "sudo -n test -f /etc/ssh/ssh_host_${key}_key"; then
        remote "sudo -n cat /etc/ssh/ssh_host_${key}_key" >"$out/ssh_host_keys/ssh_host_${key}_key"
        remote "cat /etc/ssh/ssh_host_${key}_key.pub" >"$out/ssh_host_keys/ssh_host_${key}_key.pub"
    fi
done

# Host firewall (home_host_guard), if the Pi has one.
if remote 'test -f /etc/home-host-firewall.nft'; then
    mkdir -p "$out/firewall"
    remote 'cat /etc/home-host-firewall.nft' >"$out/firewall/home-host-firewall.nft"
    remote 'sudo -n cat /usr/local/sbin/home-host-firewall' >"$out/firewall/home-host-firewall"
    remote 'systemctl cat home-host-firewall.service | grep -v "^#"' >"$out/firewall/home-host-firewall.service"
fi

remote 'cat /etc/timezone 2>/dev/null || timedatectl show -p Timezone --value' >"$out/timezone"

echo "Done. Contents:"
ls -la "$out"
echo "This folder contains secrets. Delete it after flashing."
