#!/usr/bin/env bash
# Build a ready-to-flash SD card image for the panel, without root.
#
#   packaging/pi-image/build_image.sh --state <dir> --deb <led-matrix-<version>_arm64.deb> --out <file.img>
#
# Layout of the image (see docs/PI_IMAGE.md):
#   p1  boot (FAT, from Raspberry Pi OS), read-only after setup
#   p2  system (ext4, ROOT_SIZE), read-only after setup (overlayroot)
#   p3  data (ext4, LABEL=LEDDATA, DATA_SIZE) mounted at /srv/led-matrix
#
# <dir> comes from export_state.sh (Wi-Fi, SSH keys, settings, app data). The
# first boot runs cloud-init, which creates the user, joins Wi-Fi and runs
# provision.sh from the data partition; that installs the package and reboots
# into the read-only system. The image contains secrets: keep it private and
# delete it after flashing.
#
# Needs: curl, xz, sfdisk, e2fsck, resize2fs, mkfs.ext4 (e2fsprogs >= 1.43),
# python3, and mtools (mcopy/mtype; set MTOOLS_DIR to an unpacked copy if it is
# not installed).
set -euo pipefail

BASE_URL=${BASE_URL:-https://downloads.raspberrypi.com/raspios_lite_arm64/images/raspios_lite_arm64-2026-10-06/2026-10-06-raspios-trixie-arm64-lite.img.xz}
CACHE=${CACHE:-$HOME/.cache/led-matrix-pi-image}
ROOT_SIZE_MIB=${ROOT_SIZE_MIB:-8192}
DATA_SIZE_MIB=${DATA_SIZE_MIB:-4096}
HOSTNAME_=${HOSTNAME_:-ledmat}
# Kernel options the panel needs: Wi-Fi region and a CPU core kept free for the
# matrix refresh thread. The serial console is removed (the halo uses the UART).
CMDLINE_EXTRA=${CMDLINE_EXTRA:-cfg80211.ieee80211_regdom=DE isolcpus=3}
REGDOM=${REGDOM:-DE}

here=$(cd "$(dirname "$0")" && pwd)
state= deb= out=
while (($#)); do
    case $1 in
    --state) state=$2; shift 2 ;;
    --deb) deb=$2; shift 2 ;;
    --out) out=$2; shift 2 ;;
    *) echo "unknown argument $1" >&2; exit 2 ;;
    esac
done
[[ -d $state && -f $deb && -n $out ]] || { sed -n '2,20p' "$0"; exit 2; }
for f in wifi.json authorized_keys debconf.txt; do
    [[ -s $state/$f ]] || { echo "missing $state/$f (run export_state.sh)" >&2; exit 1; }
done

if [[ -n ${MTOOLS_DIR:-} ]]; then export PATH=$MTOOLS_DIR/usr/bin:$PATH; fi
export MTOOLS_SKIP_CHECK=1
command -v mcopy >/dev/null || { echo "mtools not found (apt install mtools, or set MTOOLS_DIR)" >&2; exit 1; }

umask 077
mkdir -p "$CACHE"
# About 15 GB of scratch space; /tmp is often a small tmpfs.
work=$(mktemp -d "$CACHE/work.XXXXXX")
trap 'rm -rf "$work"' EXIT

# --- Base image -------------------------------------------------------------
mkdir -p "$CACHE"
xzfile=$CACHE/$(basename "$BASE_URL")
if [[ ! -f $xzfile ]]; then
    curl -sSfL -o "$xzfile.part" "$BASE_URL"
    curl -sSfL -o "$xzfile.sha256" "$BASE_URL.sha256"
    mv "$xzfile.part" "$xzfile"
fi
(cd "$CACHE" && sha256sum -c "$(basename "$xzfile").sha256")
echo "Unpacking $(basename "$xzfile")"
xz -dc "$xzfile" >"$work/base.img"

read -r label_id < <(sfdisk --json "$work/base.img" | python3 -I -c 'import json,sys; print(json.load(sys.stdin)["partitiontable"]["id"])')
read -r p1_start p1_size p2_start p2_size < <(sfdisk --json "$work/base.img" | python3 -I -c '
import json, sys
p = json.load(sys.stdin)["partitiontable"]["partitions"]
print(p[0]["start"], p[0]["size"], p[1]["start"], p[1]["size"])')

mib=2048 # sectors
root_sectors=$((ROOT_SIZE_MIB * mib))
data_sectors=$((DATA_SIZE_MIB * mib))
p3_start=$(((p2_start + root_sectors + 4 * mib - 1) / (4 * mib) * (4 * mib)))
total=$((p3_start + data_sectors))

# --- System partition: grow to ROOT_SIZE ------------------------------------
echo "Growing the system partition to ${ROOT_SIZE_MIB} MiB"
dd if="$work/base.img" of="$work/root.ext4" bs=512 skip="$p2_start" count="$p2_size" status=none
truncate -s $((root_sectors * 512)) "$work/root.ext4"
e2fsck -fy "$work/root.ext4" >/dev/null || [[ $? -le 1 ]]
resize2fs "$work/root.ext4" >/dev/null

# --- Data partition: provisioning files + exported state --------------------
echo "Building the data partition (${DATA_SIZE_MIB} MiB)"
stage=$work/data
mkdir -p "$stage/provision/state"
cp "$here/provision.sh" "$here/led-matrix-system-mode" "$stage/provision/"
chmod 755 "$stage/provision/provision.sh" "$stage/provision/led-matrix-system-mode"
cp "$deb" "$stage/provision/"
for f in led-matrix-data.tar.gz debconf.txt led-matrix.env; do
    [[ -f $state/$f ]] && cp "$state/$f" "$stage/provision/state/"
done
[[ -d $state/firewall ]] && cp -r "$state/firewall" "$stage/provision/state/"
chmod -R go-rwx "$stage/provision"
mkfs.ext4 -q -L LEDDATA -E root_owner=0:0 -d "$stage" "$work/data.ext4" "${DATA_SIZE_MIB}M"

# --- Boot partition: cloud-init and kernel options --------------------------
boot=$work/boot.vfat
dd if="$work/base.img" of="$boot" bs=512 skip="$p1_start" count="$p1_size" status=none

mtype -i "$boot" ::/cmdline.txt | tr -d '\r' | python3 -I -c '
import sys
drop = {"resize", "console=serial0,115200", "console=ttyAMA0,115200", "console=ttyS0,115200"}
words = [w for w in sys.stdin.read().split() if w not in drop]
words += sys.argv[1].split()
print(" ".join(words))' "$CMDLINE_EXTRA" >"$work/cmdline.txt"

mtype -i "$boot" ::/config.txt | tr -d '\r' | python3 -I -c '
import sys
lines = [l for l in sys.stdin.read().splitlines() if l.strip() not in ("dtparam=audio=on",)]
lines += ["", "[all]", "# led-matrix: onboard audio off (rpi-rgb-led-matrix needs the PWM), UART for the halo light.",
          "dtparam=audio=off", "enable_uart=1"]
print("\n".join(lines))' >"$work/config.txt"

python3 -I - "$state" "$HOSTNAME_" "$REGDOM" "$work" <<'EOF'
import json, os, pathlib, sys, time
state, hostname, regdom, work = map(pathlib.Path, sys.argv[1:5])
hostname, regdom = str(hostname), str(regdom)
wifi = json.loads((state / "wifi.json").read_text())
keys = [k for k in (state / "authorized_keys").read_text().splitlines() if k.strip() and not k.startswith("#")]
tz = (state / "timezone").read_text().strip() if (state / "timezone").exists() else "Europe/Berlin"

user_data = {
    "hostname": hostname,
    "manage_etc_hosts": True,
    "timezone": tz,
    "users": [{
        "name": "pi",
        "groups": ["adm", "dialout", "sudo", "video", "plugdev", "users", "input", "netdev", "gpio", "i2c", "spi"],
        "shell": "/bin/bash",
        "sudo": "ALL=(ALL) NOPASSWD:ALL",
        "lock_passwd": True,
        "ssh_authorized_keys": keys,
    }],
    "ssh_pwauth": False,
    "enable_ssh": True,
    "runcmd": [["sh", "-c",
                "mkdir -p /srv/led-matrix && mount LABEL=LEDDATA /srv/led-matrix && "
                "exec /srv/led-matrix/provision/provision.sh"]],
}
host_keys = state / "ssh_host_keys"
if host_keys.is_dir():
    ssh_keys = {}
    for kind in ("ed25519", "rsa", "ecdsa"):
        priv = host_keys / f"ssh_host_{kind}_key"
        if priv.exists():
            ssh_keys[f"{kind}_private"] = priv.read_text()
            ssh_keys[f"{kind}_public"] = (host_keys / f"ssh_host_{kind}_key.pub").read_text().strip()
    if ssh_keys:
        user_data["ssh_keys"] = ssh_keys
        user_data["ssh_deletekeys"] = True

network = {"network": {"version": 2, "wifis": {"wlan0": {
    "dhcp4": True, "optional": True, "regulatory-domain": regdom,
    "access-points": {wifi["ssid"]: {"password": wifi["psk"]}}}}}}

# JSON is valid YAML; cloud-init needs the #cloud-config header.
(work / "user-data").write_text("#cloud-config\n" + json.dumps(user_data, indent=2) + "\n")
(work / "network-config").write_text(json.dumps(network, indent=2) + "\n")
(work / "meta-data").write_text(f"dsmode: local\ninstance_id: led-matrix-{int(time.time())}\n")
EOF

for f in cmdline.txt config.txt user-data network-config meta-data; do
    mcopy -o -i "$boot" "$work/$f" "::/$f"
done

# --- Assemble ---------------------------------------------------------------
echo "Assembling $out"
rm -f "$out"
truncate -s $((total * 512)) "$out"
dd if="$work/base.img" of="$out" bs=512 count="$p1_start" conv=notrunc status=none
dd if="$boot" of="$out" bs=512 seek="$p1_start" conv=notrunc,sparse status=none
dd if="$work/root.ext4" of="$out" bs=4M seek=$((p2_start * 512)) oflag=seek_bytes conv=notrunc,sparse status=none
dd if="$work/data.ext4" of="$out" bs=4M seek=$((p3_start * 512)) oflag=seek_bytes conv=notrunc,sparse status=none
sfdisk -q --no-reread --no-tell-kernel "$out" <<EOF
label: dos
label-id: $label_id
unit: sectors

start=$p1_start, size=$p1_size, type=c
start=$p2_start, size=$root_sectors, type=83
start=$p3_start, size=$data_sectors, type=83
EOF
chmod 600 "$out"

echo
sfdisk -l "$out" | tail -4
echo
echo "Image ready: $out ($((total / mib)) MiB). It contains Wi-Fi and SSH secrets; delete it after flashing."
