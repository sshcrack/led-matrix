#!/bin/bash
# First boot of the generic panel image (led-matrix-firstboot.service). By now
# cloud-init has applied Raspberry Pi Imager's customisation (user, Wi-Fi, SSH
# keys, hostname, locale). This finishes the card and reboots into the
# read-only system:
#   - grow the data partition to the end of the card
#   - SSH host keys, data ownership
#   - remove Imager's settings from the world-readable boot partition (Wi-Fi
#     password); NetworkManager keeps its own root-only copy
#   - cloud-init off, read-only on, reboot
set -euo pipefail

DATA=/srv/led-matrix
BOOT=${BOOT:-/boot/firmware}
LOG=$DATA/firstboot.log

if mountpoint -q "$DATA"; then
    exec > >(tee -a "$LOG") 2>&1
fi
echo "=== led-matrix first boot $(date -Is)"

if ! mountpoint -q "$DATA"; then
    # Without the data partition the card cannot work; leave the system writable
    # so it can be inspected, and let recovery mode show it on the panel.
    echo "The data partition is not mounted; not finishing the setup."
    exit 1
fi

# Imager's settings must be applied before they are removed below.
if command -v cloud-init >/dev/null; then
    cloud-init status --wait >/dev/null || true
fi

# --- Data partition: grow to the end of the card ----------------------------------
device=$(findmnt -n -o SOURCE "$DATA")
part=$(basename "$device")
if [[ -r /sys/class/block/$part/partition ]]; then
    disk=/dev/$(lsblk -n -o PKNAME "$device")
    number=$(cat "/sys/class/block/$part/partition")
    if growpart "$disk" "$number"; then
        resize2fs "$device"
    else
        echo "Data partition already fills the card (or cannot grow)."
    fi
fi

# --- Keys and ownership ----------------------------------------------------------------
ssh-keygen -A
/usr/local/lib/led-matrix-image/ensure_daemon_user.sh
chmod 600 "$DATA/etc/led-matrix.env" 2>/dev/null || true

# --- Secrets off the boot partition ------------------------------------------------------
for f in user-data network-config; do
    if [[ -f $BOOT/$f ]]; then
        printf '# Applied by the led-matrix first boot on %s; cloud-init is disabled.\n' "$(date -I)" >"$BOOT/$f"
    fi
done
rm -rf /var/lib/cloud/instances /var/lib/cloud/instance /var/lib/cloud/data
touch /etc/cloud/cloud-init.disabled

# --- Read-only system --------------------------------------------------------------------
systemctl disable led-matrix-firstboot.service
sync
/usr/local/sbin/led-matrix-system-mode readonly --no-reboot
echo "=== first boot finished $(date -Is); rebooting into the read-only system"
sync
systemctl reboot
