# Power-cut-safe panel card

The panel is meant to be unplugged like any appliance. A card built with
`packaging/pi-image/` keeps the system read-only, so a power cut can only
interrupt writes to a small data partition, and those writes are made durable
by the daemon (`DurableFile::replace`, config recovery in `MainConfig`). Plan
and progress: [#217](https://github.com/sshcrack/led-matrix/issues/217).

## Layout

| Partition | Size | Mounted at | Written |
|---|---|---|---|
| p1 boot (FAT) | 512 MiB | `/boot/firmware`, **read-only** | only in maintenance mode |
| p2 system (ext4) | 8 GiB | `/`, **read-only** (`overlayroot=tmpfs:recurse=0`; changes live in RAM and are gone after a reboot) | only in maintenance mode |
| p3 data (ext4, `LABEL=LEDDATA`) | 4 GiB | `/srv/led-matrix` (`nofail`, 10 s timeout) | when settings change |

The rest of the card stays unused. On the data partition:

| Path | Purpose |
|---|---|
| `/srv/led-matrix/state/` | bind-mounted at `/var/lib/led-matrix` (`config.json`, `config.json.last-good`, uploads, crash reports), owned by `pi` |
| `/srv/led-matrix/etc/led-matrix.env` | the service environment; `/etc/default/led-matrix` is a symlink to it (root, 0600) |
| `/srv/led-matrix/provision/` | the first-boot script, its log and the package it installed (root only) |

Other settings: the journal stays in RAM (32 MB), swap is zram only, the apt
timers and unattended upgrades are off, cloud-init is disabled after the first
boot. If the data partition is missing or broken the Pi still boots; the app
then starts with default settings that live in RAM until a recovery mode exists
([#226](https://github.com/sshcrack/led-matrix/issues/226)).

Self-updates are switched off on this card: `dpkg` would install into the RAM
overlay and the old version would come back at the next reboot. The updater
refuses with "The system is read-only" if asked anyway. Updates move onto the
data partition in [#225](https://github.com/sshcrack/led-matrix/issues/225); until
then, update in maintenance mode (below).

## Build and flash a card

Run on a Linux computer with an SD card reader. No root is needed to build.

```bash
# 1. Export settings, Wi-Fi, SSH keys and app data from the running Pi
#    (skip for a brand-new panel and write the files by hand, see export_state.sh).
packaging/pi-image/export_state.sh ledmat ~/.cache/led-matrix-pi-image/state

# 2. Build the image with a led-matrix-<version>_arm64.deb (release asset or CI artifact)
packaging/pi-image/build_image.sh --state ~/.cache/led-matrix-pi-image/state \
    --deb led-matrix-<version>_arm64.deb --out ~/.cache/led-matrix-pi-image/ledmat.img

# 3. Write it to the card (check the device name with lsblk first!)
sudo dd if=~/.cache/led-matrix-pi-image/ledmat.img of=/dev/mmcblk0 bs=4M conv=fsync status=progress

# 4. Delete the secrets
rm -rf ~/.cache/led-matrix-pi-image/state ~/.cache/led-matrix-pi-image/ledmat.img
```

The image uses Raspberry Pi OS Lite (64-bit, trixie); `build_image.sh` downloads
and checks it (`BASE_URL` to change). It needs `mtools`; without root, unpack it
with `apt-get download mtools && dpkg -x mtools_*.deb ~/.local/opt/mtools` and
pass `MTOOLS_DIR=~/.local/opt/mtools`. Use a high-endurance card (e.g. SanDisk
High Endurance, Samsung PRO Endurance): cheap cards are the usual cause of
corruption after power cuts.

First boot takes about 5–10 minutes: cloud-init creates the `pi` user, joins
Wi-Fi and runs `provision.sh`, which installs the package, moves the settings
in place, removes the secrets it brought along and reboots into the read-only
system. Its log is `/srv/led-matrix/provision/provision.log`.

## Maintenance (OS updates, debugging tools)

```bash
sudo led-matrix-system-mode status      # read-only now / next boot, data mount
sudo led-matrix-system-mode writable    # reboots with a normal writable system
sudo apt full-upgrade                    # or install a newer led-matrix .deb
sudo led-matrix-system-mode readonly    # reboots back into the read-only system
```

Do not use `raspi-config`'s overlay switch: it cannot remove the
`overlayroot=tmpfs:recurse=0` option this card uses.
