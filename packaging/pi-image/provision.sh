#!/bin/bash
# First-boot setup of a panel card built by build_image.sh. cloud-init runs it
# once as root (runcmd), after the network is up. It finishes with a reboot
# into the read-only system.
#
# Layout it sets up:
#   /srv/led-matrix            data partition (LABEL=LEDDATA), writable
#     state/                   bind-mounted at /var/lib/led-matrix (pi:pi)
#     app/                     bind-mounted at /var/lib/led-matrix-app (root):
#                              updated versions of the app (scripts/launcher.sh)
#     etc/led-matrix.env       /etc/default/led-matrix points here (root, 0600)
#     provision/               this script, the package and the exported state
#   /                          read-only (overlayroot, changes live in RAM)
#   /boot/firmware             read-only
set -euo pipefail

DATA=/srv/led-matrix
PROVISION=$DATA/provision
LOG=$PROVISION/provision.log

exec > >(tee -a "$LOG") 2>&1
echo "=== led-matrix provisioning $(date -Is)"

if [[ -e $PROVISION/done ]]; then
    echo "Already provisioned."
    exit 0
fi

# --- Data partition ---------------------------------------------------------
# nofail + a short device timeout: a missing or broken data partition must not
# stop the panel from booting.
mkdir -p "$DATA" /var/lib/led-matrix
if ! grep -q 'LABEL=LEDDATA' /etc/fstab; then
    cat >>/etc/fstab <<'EOF'
# led-matrix data partition: settings, uploads, crash reports (writable).
LABEL=LEDDATA  /srv/led-matrix      ext4  defaults,noatime,nofail,x-systemd.device-timeout=10s  0  2
/srv/led-matrix/state  /var/lib/led-matrix  none  bind,nofail,x-systemd.requires-mounts-for=/srv/led-matrix  0  0
EOF
fi
if ! grep -q '/var/lib/led-matrix-app' /etc/fstab; then
    cat >>/etc/fstab <<'EOF'
/srv/led-matrix/app    /var/lib/led-matrix-app  none  bind,nofail,x-systemd.requires-mounts-for=/srv/led-matrix  0  0
EOF
fi
systemctl daemon-reload
mountpoint -q "$DATA" || mount "$DATA"
mkdir -p "$DATA/state" "$DATA/etc" "$DATA/app" /var/lib/led-matrix-app
chmod 755 "$DATA" "$DATA/etc" "$DATA/app"
chown root:root "$DATA/app"
chown -R root:root "$PROVISION"
chmod 700 "$PROVISION"
mountpoint -q /var/lib/led-matrix || mount /var/lib/led-matrix
mountpoint -q /var/lib/led-matrix-app || mount /var/lib/led-matrix-app

# Migrated app data (exported from the previous card).
if [[ -f $PROVISION/state/led-matrix-data.tar.gz && ! -e $DATA/state/config.json ]]; then
    tar -C /var/lib/led-matrix -xzf "$PROVISION/state/led-matrix-data.tar.gz"
fi
chown -R pi:pi /var/lib/led-matrix

# The environment file lives on the data partition; /etc is read-only later.
# The package's postinst writes it through this symlink.
if [[ -f $PROVISION/state/led-matrix.env && ! -e $DATA/etc/led-matrix.env ]]; then
    install -m 600 -o root -g root "$PROVISION/state/led-matrix.env" "$DATA/etc/led-matrix.env"
fi
ln -sfn "$DATA/etc/led-matrix.env" /etc/default/led-matrix

# --- Package ----------------------------------------------------------------
if [[ -f $PROVISION/state/debconf.txt ]]; then
    debconf-set-selections <"$PROVISION/state/debconf.txt"
fi

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y overlayroot nftables systemd-coredump
shopt -s nullglob
debs=("$PROVISION"/led-matrix*.deb)
[[ ${#debs[@]} -eq 1 ]] || { echo "Expected exactly one led-matrix*.deb in $PROVISION"; exit 1; }
apt-get install -y "${debs[0]}"

# --- Host firewall (copied from the previous card) --------------------------
if [[ -d $PROVISION/state/firewall ]]; then
    install -m 644 "$PROVISION/state/firewall/home-host-firewall.nft" /etc/home-host-firewall.nft
    install -m 700 "$PROVISION/state/firewall/home-host-firewall" /usr/local/sbin/home-host-firewall
    install -m 644 "$PROVISION/state/firewall/home-host-firewall.service" /etc/systemd/system/home-host-firewall.service
    systemctl daemon-reload
    systemctl enable home-host-firewall.service
fi

# Onboard sound shares hardware with the LED driver (README, Quick start step 1).
echo "blacklist snd_bcm2835" >/etc/modprobe.d/blacklist-rgb-matrix.conf

# --- Keep writes off the system ----------------------------------------------
mkdir -p /etc/systemd/journald.conf.d
cat >/etc/systemd/journald.conf.d/led-matrix.conf <<'EOF'
# The system is read-only: keep the journal in RAM and small.
[Journal]
Storage=volatile
RuntimeMaxUse=32M
EOF

# Swap only to compressed RAM, never to a file on the card.
if [[ -d /etc/rpi ]]; then
    mkdir -p /etc/rpi/swap.conf.d
    cat >/etc/rpi/swap.conf.d/led-matrix.conf <<'EOF'
[Main]
Mechanism=zram
EOF
fi

# Package updates of the OS happen in maintenance mode, not in the background.
systemctl disable --now apt-daily.timer apt-daily-upgrade.timer 2>/dev/null || true
systemctl disable --now unattended-upgrades.service 2>/dev/null || true

install -m 755 "$PROVISION/led-matrix-system-mode" /usr/local/sbin/led-matrix-system-mode
# On the read-only system it would remount the overlay root, which overlayfs
# refuses ("No changes allowed in reconfigure"). Masking it would also keep / read-only
# in maintenance mode, so skip it only when booting with overlayroot.
mkdir -p /etc/systemd/system/systemd-remount-fs.service.d
cat >/etc/systemd/system/systemd-remount-fs.service.d/led-matrix.conf <<'EOF'
[Unit]
ConditionKernelCommandLine=!overlayroot
EOF

# cloud-init has done its job; never run it again on this card.
touch /etc/cloud/cloud-init.disabled

# Remove the secrets this setup brought along: the exported state (Wi-Fi
# password, Spotify secret, API token) and the cloud-init files on the
# world-readable boot partition (Wi-Fi password, SSH host keys).
rm -rf "$PROVISION/state"
for f in user-data network-config; do
    printf '# Consumed by led-matrix provisioning on %s; cloud-init is disabled.\n' "$(date -I)" >"/boot/firmware/$f"
done

# --- Read-only system --------------------------------------------------------
touch "$PROVISION/done"
sync
/usr/local/sbin/led-matrix-system-mode readonly --no-reboot
echo "=== provisioning finished $(date -Is); rebooting into the read-only system"
sync
systemctl reboot
