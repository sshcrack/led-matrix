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
| `/srv/led-matrix/app/` | bind-mounted at `/var/lib/led-matrix-app`: updated versions of the app, root only |
| `/srv/led-matrix/etc/led-matrix.env` | the service environment; `/etc/default/led-matrix` is a symlink to it (root, 0600) |
| `/srv/led-matrix/provision/` | the first-boot script, its log and the package it installed (root only) |

Other settings: the journal stays in RAM (32 MB), swap is zram only, the apt
timers and unattended upgrades are off, cloud-init is disabled after the first
boot. If the data partition is missing or broken the Pi still boots and the panel
starts in [recovery mode](RECOVERY.md): it shows `DATA` and the address of a
page that repairs or resets the partition.

App updates (automatic, the Updates page, `build_upload.sh`) work as on any
install: the updater unpacks each version into `/srv/led-matrix/app`
(bind-mounted at `/var/lib/led-matrix-app`) and switches to it in one rename;
the package installed at first boot stays on the read-only system as the factory
copy, used when the data partition has no working version
([details](transactional-updates.md)). If the data partition is missing, the
updater refuses instead of installing into RAM.

## Generic image (Raspberry Pi Imager)

`build_generic_image.sh` builds the public image. It contains no user, Wi-Fi or keys,
and the package is already installed, so the first boot needs no internet:

```bash
sudo packaging/pi-image/build_generic_image.sh --deb led-matrix-<version>_arm64.deb \
    --out ~/.cache/led-matrix-pi-image/out/led-matrix-<version>-arm64.img.xz
sudo packaging/pi-image/check_image_secrets.sh ~/.cache/led-matrix-pi-image/out/led-matrix-<version>-arm64.img.xz
```

**Build requirements.** The build runs as root: it uses loop mounts and an arm64 chroot,
via qemu-user with a binfmt `F` handler (`qemu-user-static`) on x86. It needs network
access for apt.

**What `customize_root.sh` sets up in the chroot.** Everything `provision.sh` does that
does not depend on the person:
- packages (`overlayroot`, `nftables`, `systemd-coredump`, `avahi-daemon`)
- the data partition binds
- journald in RAM, zram swap, no apt timers
- the hostname `led-matrix`, so `led-matrix.local` resolves through mDNS

It empties the machine ID and removes the SSH host keys and logs.

**Size.** The system partition is 5 GiB and the data partition 256 MiB, so the image
fits an 8 GB card.

**First boot.** Flash the image with Raspberry Pi Imager and fill in its customisation:
user, Wi-Fi, SSH key, hostname. cloud-init applies these on the first boot. Then
`led-matrix-firstboot.service` (`firstboot.sh`) does the rest:
1. grows the data partition to the end of the card
2. generates SSH host keys
3. replaces Imager's `user-data`/`network-config` on the boot partition, which hold the
   Wi-Fi password
4. disables cloud-init and switches to the read-only system
5. reboots

Its log is `/srv/led-matrix/firstboot.log`.

**Panel layout.** The image starts with the package defaults: four 64×64 panels in a 2×2
grid (`--led-chain=2 --led-parallel=2`) and GPIO slowdown 3. Until someone has checked it,
the web UI (`http://led-matrix.local:8080`) shows "Set up your panel"; the **Panel
hardware** card on the Integrations page shows a test pattern and changes the layout,
wiring and advanced `--led-*` options (see [Panel hardware](PANEL_HARDWARE.md)).

**The `pi` account.** The daemon drops its privileges to `pi`. The image keeps Raspberry
Pi OS's locked `pi` account (uid 1000): Raspberry Pi OS's cloud-init sets up Imager's user
by renaming that account, so without it no login user and no SSH key would be created.
When Imager's user has another name, `led-matrix-user.service` creates `pi` again as a
locked account before the panel service starts. It then hands the panel data to `pi`.

**Releases.** `create-release.yml` creates the release as a draft, builds the image from its
`.deb` (through `pi-image.yml`), uploads `led-matrix-<version>-arm64.img.xz`, its `.sha256`
and `led-matrix-imager.json`, and only then publishes the release (`publish-release`; it
also publishes when the image failed and then fails itself). That last one is a Raspberry Pi Imager repository file written by
`imager_repo.py`. Its fixed name makes
`https://github.com/sshcrack/led-matrix/releases/latest/download/led-matrix-imager.json`
always list the newest image. It sets `init_format: cloudinit-rpi`, so Imager writes
its customisation as cloud-init files.

**Secrets check.** `check_image_secrets.sh` fails when the image contains any of:
- SSH host or user keys, or other private keys
- password hashes
- Wi-Fi credentials
- a machine ID or random seed
- shell history
- led-matrix secrets

## Build and flash a card from an existing panel

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
Power-cut test results (simulated cuts during playback, saves, uploads, updates
and boot): `docs/POWER_CUT_TEST.md`.

First boot takes about 5–10 minutes: cloud-init creates the `pi` user, joins
Wi-Fi and runs `provision.sh`, which installs the package, moves the settings
in place, removes the secrets it brought along and reboots into the read-only
system. Its log is `/srv/led-matrix/provision/provision.log`.

## Maintenance (OS updates, debugging tools)

```bash
sudo led-matrix-system-mode status      # read-only now / next boot, data mount
sudo led-matrix-system-mode writable    # reboots with a normal writable system
sudo apt full-upgrade                    # OS updates (app updates need no maintenance mode)
sudo led-matrix-system-mode readonly    # reboots back into the read-only system
```

Do not use `raspi-config`'s overlay switch: it cannot remove the
`overlayroot=tmpfs:recurse=0` option this card uses.
