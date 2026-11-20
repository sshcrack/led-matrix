#!/bin/bash
# Build-time setup of the generic panel image (build_image.sh --generic). Runs
# as root inside the image's system partition (chroot, arm64 through qemu-user)
# with the data partition mounted at /srv/led-matrix and its binds in place.
#
#   customize_root.sh <led-matrix.deb>
#
# Does everything provision.sh does that does not depend on the user: packages,
# data partition layout, read-only preparations. What depends on the user (Wi-Fi,
# SSH keys, hostname) comes from Raspberry Pi Imager through cloud-init on the
# first boot; led-matrix-firstboot.service then finishes (firstboot.sh).
set -euo pipefail

deb=$1
DATA=/srv/led-matrix
here=$(cd "$(dirname "$0")" && pwd)
export DEBIAN_FRONTEND=noninteractive LC_ALL=C

# Services must not start inside the chroot.
printf '#!/bin/sh\nexit 101\n' >/usr/sbin/policy-rc.d
chmod 755 /usr/sbin/policy-rc.d
trap 'rm -f /usr/sbin/policy-rc.d' EXIT

# --- Daemon user (build time only) ---------------------------------------------
# The package's postinst hands the data to pi. Raspberry Pi OS ships pi (uid 1000,
# locked) as a placeholder: its cloud-init renames that account to Imager's user
# and sets the password and keys, so it must stay. If Imager's user has another
# name, led-matrix-user.service creates pi again on the first boot.
if ! id pi >/dev/null 2>&1; then
    useradd --uid 1000 --user-group --no-create-home --home-dir /var/lib/led-matrix --shell /usr/sbin/nologin pi
fi

# --- Data partition ------------------------------------------------------------
cat >>/etc/fstab <<'EOF'
# led-matrix data partition: settings, uploads, crash reports (writable).
LABEL=LEDDATA  /srv/led-matrix      ext4  defaults,noatime,nofail,x-systemd.device-timeout=10s  0  2
/srv/led-matrix/state  /var/lib/led-matrix  none  bind,nofail,x-systemd.requires-mounts-for=/srv/led-matrix  0  0
/srv/led-matrix/app    /var/lib/led-matrix-app  none  bind,nofail,x-systemd.requires-mounts-for=/srv/led-matrix  0  0
EOF
chmod 755 "$DATA" "$DATA/etc" "$DATA/app"
# The environment file lives on the data partition; postinst writes it through this link.
ln -sfn "$DATA/etc/led-matrix.env" /etc/default/led-matrix

# --- Packages --------------------------------------------------------------------
apt-get update
apt-get install -y --no-install-recommends overlayroot nftables systemd-coredump avahi-daemon jq cloud-guest-utils
apt-get install -y "$deb"
chown -R pi:pi /var/lib/led-matrix
chmod 600 "$DATA/etc/led-matrix.env"

# --- Same as provision.sh ----------------------------------------------------------
# Onboard sound shares hardware with the LED driver.
echo "blacklist snd_bcm2835" >/etc/modprobe.d/blacklist-rgb-matrix.conf

mkdir -p /etc/systemd/journald.conf.d
cat >/etc/systemd/journald.conf.d/led-matrix.conf <<'EOF'
# The system is read-only: keep the journal in RAM and small.
[Journal]
Storage=volatile
RuntimeMaxUse=32M
EOF

if [[ -d /etc/rpi ]]; then
    mkdir -p /etc/rpi/swap.conf.d
    cat >/etc/rpi/swap.conf.d/led-matrix.conf <<'EOF'
[Main]
Mechanism=zram
EOF
fi

systemctl disable apt-daily.timer apt-daily-upgrade.timer 2>/dev/null || true
systemctl disable unattended-upgrades.service 2>/dev/null || true
# The system partition has a fixed size (the first boot grows the data partition).
# Raspberry Pi OS's rpi-resize pulls in systemd-growfs-root, which fails on every
# boot once / is the read-only overlay.
systemctl disable rpi-resize.service 2>/dev/null || true
systemctl mask systemd-growfs-root.service

install -m 755 "$here/led-matrix-system-mode" /usr/local/sbin/led-matrix-system-mode
mkdir -p /etc/systemd/system/systemd-remount-fs.service.d
cat >/etc/systemd/system/systemd-remount-fs.service.d/led-matrix.conf <<'EOF'
[Unit]
ConditionKernelCommandLine=!overlayroot
EOF

# --- Name and first boot -------------------------------------------------------------
# Imager's customisation can change it; avahi announces <hostname>.local.
echo led-matrix >/etc/hostname
sed -i -E 's/^127\.0\.1\.1.*/127.0.1.1\tled-matrix/' /etc/hosts
grep -q '^127\.0\.1\.1' /etc/hosts || printf '127.0.1.1\tled-matrix\n' >>/etc/hosts

install -d -m 755 /usr/local/lib/led-matrix-image
install -m 755 "$here/firstboot.sh" /usr/local/lib/led-matrix-image/firstboot.sh
install -m 755 "$here/ensure_daemon_user.sh" /usr/local/lib/led-matrix-image/ensure_daemon_user.sh
install -m 644 "$here/led-matrix-firstboot.service" /etc/systemd/system/led-matrix-firstboot.service
install -m 644 "$here/led-matrix-user.service" /etc/systemd/system/led-matrix-user.service
systemctl enable led-matrix-firstboot.service led-matrix-user.service

# --- Nothing machine- or build-specific on the image -------------------------------------
apt-get clean
rm -rf /var/lib/apt/lists/* /var/cache/debconf/*-old /var/lib/dpkg/*-old
rm -f /etc/ssh/ssh_host_* /var/lib/systemd/random-seed /root/.bash_history
# Empty machine-id: systemd generates one on the first boot (a missing file would
# be created on the read-only system later).
: >/etc/machine-id
if [[ -d /var/lib/dbus ]]; then ln -sfn /etc/machine-id /var/lib/dbus/machine-id; fi
find /var/log -type f -delete
find /tmp /var/tmp -mindepth 1 -delete
