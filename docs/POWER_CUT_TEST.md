# Power-cut test (2026-11-20)

Evidence for #227 (part of #217): the read-only panel card (`docs/PI_IMAGE.md`)
comes back by itself after losing power at any moment, during playback, settings
saves, uploads, app updates and boot.

## Setup

- Panel `ledmat`: Raspberry Pi 3 Model B Rev 1.2, SD card `SD32G` (manfid 0x03,
  SanDisk, made 05/2022, 29 GiB), the live panel card.
- led-matrix 1.22.5 installed with `dpkg`, read-only system
  (`overlayroot=tmpfs:recurse=0`), data partition `/dev/mmcblk0p3` (ext4,
  `noatime`) with `/var/lib/led-matrix` and `/var/lib/led-matrix-app` bound into it.
- App updates alternated between 1.22.4 and 1.22.5 through the normal updater
  (`led-matrix-update.path`, `update_service.sh`).

## Method

`tests/powercut_soak.py ledmat results.jsonl <counts>`: each cycle starts an
activity, then reboots the Pi with `echo b > /proc/sysrq-trigger`. The kernel
resets right away, with no sync, no unmount and no service stop. Whatever was not
yet on the card is lost, exactly as in a power cut. After the reboot the script
waits for the API and records whether the panel came back in normal playback or
in recovery mode, whether the last acknowledged save survived and is complete,
the version links, failed units, ext4 messages for the data partition, and the
config recovery log lines.

**Simulated, not real power loss.** The SD card keeps power and the supply
voltage does not sag, so this does not test the card's own behaviour when power
fails mid-write (its flash translation layer). That is the hardware risk the
read-only system and a high-endurance card address. One real power pull on a
fresh image card (same date) also came back normally.

## Results

19 cycles, all ended in normal playback 32.7–38.4 s after the cut (time until the
API answered). None needed recovery mode, SSH or a reflash.

| State | Cuts | What was checked | Result |
|---|---|---|---|
| idle playback | 2 | comes back | ok |
| settings saves (`POST /brightness_schedule` loop, cut 1–10 s in) | 4 | value is the last acknowledged or last sent | ok, 4/4 |
| uploads (~240 KB shader loop) | 5 | file complete, last acknowledged or last sent version | ok, 5/5 |
| update, cut 2–75 s after queueing | 4 | still runs a complete version | ok: all four were cut before the switch (copy/unpack); the old version ran |
| update, cut 0–6 s after "Switching" | 2 | new version on trial is confirmed or rolled back | ok: both came back on the new version, trial confirmed, old version kept as `previous` |
| cut, then cut again 0–8 s after SSH answers | 2 | comes back | ok |

- No ext4 error or journal-recovery message for the data partition, no failed
  units, no `.corrupt-*` config files and no config recovery events in any cycle.
- An update cut during unpacking leaves a `versions/.partial-*` folder (about
  140 MB). The next update run removes it (`update_service.sh` exit handler);
  confirmed by an uncut update afterwards.

Earlier the same day, a deliberately corrupted data-partition superblock on the
same card started recovery mode, and its repair button restored everything
(`docs/RECOVERY.md`).
