#!/usr/bin/env bash
# Build the public panel image: Raspberry Pi OS Lite with led-matrix installed,
# no user, no Wi-Fi, no keys. Raspberry Pi Imager's customisation supplies those
# on the first boot (cloud-init); led-matrix-firstboot.service then grows the data
# partition and switches to the read-only system (see docs/PI_IMAGE.md).
#
#   sudo packaging/pi-image/build_generic_image.sh --deb <led-matrix-<version>_arm64.deb> --out <file.img.xz>
#
# Layout:
#   p1  boot (FAT, from Raspberry Pi OS)
#   p2  system (ext4, ROOT_SIZE_MIB), read-only after the first boot (overlayroot)
#   p3  data (ext4, LABEL=LEDDATA, DATA_SIZE_MIB), grown to the end of the card
#       on the first boot, mounted at /srv/led-matrix
#
# Runs as root: loop mounts and an arm64 chroot, so non-arm64 hosts need
# qemu-user with a binfmt handler that has the F flag (Debian/Ubuntu:
# qemu-user-static or qemu-user-binfmt). Needs network access for apt.
# Output: <file.img.xz> (or a raw image when --out does not end in .xz) and
# <file>.sha256.
set -euo pipefail

BASE_URL=${BASE_URL:-https://downloads.raspberrypi.com/raspios_lite_arm64/images/raspios_lite_arm64-2026-10-06/2026-10-06-raspios-trixie-arm64-lite.img.xz}
CACHE=${CACHE:-${SUDO_USER:+$(getent passwd "$SUDO_USER" | cut -d: -f6)/.cache/led-matrix-pi-image}}
CACHE=${CACHE:-$HOME/.cache/led-matrix-pi-image}
ROOT_SIZE_MIB=${ROOT_SIZE_MIB:-5120}
DATA_SIZE_MIB=${DATA_SIZE_MIB:-256}
# A CPU core kept free for the matrix refresh thread. No Wi-Fi region: Imager
# sets it with the Wi-Fi settings.
CMDLINE_EXTRA=${CMDLINE_EXTRA:-isolcpus=3}

here=$(cd "$(dirname "$0")" && pwd)
deb= out=
while (($#)); do
    case $1 in
    --deb) deb=$(realpath "$2"); shift 2 ;;
    --out) out=$(realpath -m "$2"); shift 2 ;;
    *) echo "unknown argument $1" >&2; exit 2 ;;
    esac
done
[[ -f $deb && -n $out ]] || { sed -n '2,20p' "$0"; exit 2; }
[[ $(id -u) -eq 0 ]] || { echo "Run as root (loop mounts and chroot)." >&2; exit 1; }
if [[ $(uname -m) != aarch64 && ! -e /proc/sys/fs/binfmt_misc/qemu-aarch64 ]]; then
    echo "No binfmt handler for arm64; install qemu-user-static (or qemu-user-binfmt)." >&2
    exit 1
fi

mkdir -p "$CACHE"
work=$(mktemp -d "$CACHE/generic.XXXXXX")
mnt=$work/mnt
loop=
mounts=()
cleanup() {
    set +e
    for ((i = ${#mounts[@]} - 1; i >= 0; i--)); do umount -l "${mounts[i]}" 2>/dev/null; done
    [[ -n $loop ]] && losetup -d "$loop"
    rm -rf "$work"
}
trap cleanup EXIT
mount_() { mount "$@"; mounts+=("${*: -1}"); }

# --- Base image ---------------------------------------------------------------------
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
img=$work/image.img

echo "Assembling the partitions (system ${ROOT_SIZE_MIB} MiB, data ${DATA_SIZE_MIB} MiB)"
truncate -s $((total * 512)) "$img"
dd if="$work/base.img" of="$img" bs=512 count=$((p1_start + p1_size)) conv=notrunc status=none
dd if="$work/base.img" of="$img" bs=4M skip=$((p2_start * 512)) seek=$((p2_start * 512)) count=$((p2_size * 512)) \
    iflag=skip_bytes,count_bytes oflag=seek_bytes conv=notrunc,sparse status=none
rm "$work/base.img"
sfdisk -q --no-reread --no-tell-kernel "$img" <<EOF
label: dos
label-id: $label_id
unit: sectors

start=$p1_start, size=$p1_size, type=c
start=$p2_start, size=$root_sectors, type=83
start=$p3_start, size=$data_sectors, type=83
EOF

loop=$(losetup -P --show -f "$img")
udevadm settle 2>/dev/null || true
e2fsck -fy "${loop}p2" >/dev/null || [[ $? -le 1 ]]
resize2fs "${loop}p2" >/dev/null
mkfs.ext4 -q -L LEDDATA "${loop}p3"

# --- Chroot ---------------------------------------------------------------------------
mkdir -p "$mnt"
mount_ "${loop}p2" "$mnt"
mount_ "${loop}p1" "$mnt/boot/firmware"
mkdir -p "$mnt/srv/led-matrix"
mount_ "${loop}p3" "$mnt/srv/led-matrix"
mkdir -p "$mnt/srv/led-matrix/state" "$mnt/srv/led-matrix/etc" "$mnt/srv/led-matrix/app" \
    "$mnt/var/lib/led-matrix" "$mnt/var/lib/led-matrix-app"
mount_ --bind "$mnt/srv/led-matrix/state" "$mnt/var/lib/led-matrix"
mount_ --bind "$mnt/srv/led-matrix/app" "$mnt/var/lib/led-matrix-app"
mount_ -t proc proc "$mnt/proc"
mount_ -t sysfs sys "$mnt/sys"
mount_ --bind /dev "$mnt/dev"
mount_ --bind /dev/pts "$mnt/dev/pts"
mount_ -t tmpfs tmpfs "$mnt/run"
mount_ -t tmpfs tmpfs "$mnt/tmp"

# Name resolution for apt; the image's own resolv.conf comes back afterwards.
resolv=$mnt/etc/resolv.conf
saved_resolv=$work/resolv.conf.image
cp -a "$resolv" "$saved_resolv" 2>/dev/null || saved_resolv=
rm -f "$resolv"
cp -L /etc/resolv.conf "$resolv"

stage=$mnt/tmp/led-matrix-image
mkdir -p "$stage"
cp "$here/customize_root.sh" "$here/firstboot.sh" "$here/ensure_daemon_user.sh" "$here/led-matrix-firstboot.service" \
    "$here/led-matrix-user.service" "$here/led-matrix-system-mode" "$stage/"
cp "$deb" "$stage/led-matrix.deb"
chmod 755 "$stage"/*.sh "$stage/led-matrix-system-mode"
echo "Installing led-matrix into the system partition (arm64 chroot)"
chroot "$mnt" /tmp/led-matrix-image/customize_root.sh /tmp/led-matrix-image/led-matrix.deb

rm -f "$resolv"
if [[ -n $saved_resolv ]]; then cp -a "$saved_resolv" "$resolv"; fi

# --- Boot partition ------------------------------------------------------------------
# No "resize": the first boot grows the data partition, not the system. No serial
# console: the halo light uses the UART.
python3 -I - "$mnt/boot/firmware/cmdline.txt" "$CMDLINE_EXTRA" <<'EOF'
import sys
path, extra = sys.argv[1], sys.argv[2]
drop = {"resize", "console=serial0,115200", "console=ttyAMA0,115200", "console=ttyS0,115200"}
words = [w for w in open(path).read().split() if w not in drop]
words += [w for w in extra.split() if w not in words]
open(path, "w").write(" ".join(words) + "\n")
EOF
python3 -I - "$mnt/boot/firmware/config.txt" <<'EOF'
import sys
path = sys.argv[1]
lines = [l for l in open(path).read().splitlines() if l.strip() != "dtparam=audio=on"]
lines += ["", "[all]", "# led-matrix: onboard audio off (rpi-rgb-led-matrix needs the PWM), UART for the halo light.",
          "dtparam=audio=off", "enable_uart=1"]
open(path, "w").write("\n".join(lines) + "\n")
EOF

# --- Finish ---------------------------------------------------------------------------
# Free blocks to zeros (loop discard punches holes) so the image compresses well.
fstrim "$mnt/srv/led-matrix" || true
fstrim "$mnt" || true
for ((i = ${#mounts[@]} - 1; i >= 0; i--)); do umount "${mounts[i]}"; done
mounts=()
e2fsck -fy "${loop}p2" >/dev/null || [[ $? -le 1 ]]
e2fsck -fy "${loop}p3" >/dev/null || [[ $? -le 1 ]]
losetup -d "$loop"
loop=

mkdir -p "$(dirname "$out")"
if [[ $out == *.xz ]]; then
    echo "Compressing $out"
    xz -T0 -6 -c "$img" >"$out.part"
    mv "$out.part" "$out"
else
    mv "$img" "$out"
fi
(cd "$(dirname "$out")" && sha256sum "$(basename "$out")" >"$(basename "$out").sha256")
if [[ -n ${SUDO_UID:-} ]]; then chown "$SUDO_UID:$SUDO_GID" "$out" "$out.sha256"; fi
echo "Image ready: $out ($(du -h "$out" | cut -f1); $((total / mib)) MiB when flashed)"
