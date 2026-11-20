# LED refresh flicker on the Pi

The library's refresh thread (SCHED_FIFO 99, CPU 3, `isolcpus=3`) clocks every row and bit plane
out of the GPIO pins, and hardware timers set how long each bit plane stays lit. So the lit time
per refresh is fixed, and **the brightness follows the refresh rate**. A refresh that takes
longer makes the panel dimmer for that moment. A single long refresh shows as a flicker, and a
sustained slowdown (an upload) shows as dimming.

## Measuring it

`/diagnostics` → `refresh` (`src_matrix/matrix_control/refresh_monitor.*`, from the library's
`RGBMatrix::GetRefreshTiming()`, sshcrack/rpi-rgb-led-matrix#21) shows windows over the last 10 s
and 60 s:
- `refresh_hz`
- min/median/p99/max refresh time
- `late`: refreshes over 1.5 × median
- per-CPU interrupt rates from `/proc/interrupts`, IPIs kept separate
- the top interrupts on the refresh CPU

The web UI shows it on the Diagnostics page ("Panel refresh"). Late refreshes catch single
glitches. Dimming shows only in `refresh_hz` and the median.

## What causes it (ledmat, Pi 3B, 2026-11-20)

Hardware: 128×128 (64×64, chain 2, parallel 2), `--led-slowdown-gpio=3`, scene `digitalrain`. All
windows are 60 s unless noted.

| Condition | Refresh rate | Median | p99 | Max | Late / 1000 |
|---|---:|---:|---:|---:|---:|
| Idle | 94.8 Hz | 10.56 ms | 11.03 ms | 12.34 ms | 0 |
| 1 GiB scp → `/dev/null` | 84.2 Hz | 11.91 ms | 13.26 ms | 13.88 ms | 0 |
| 1 GiB scp → SD card | 82.8 Hz | 12.02 ms | 14.06 ms | 16.00 ms | 0 |
| Interactive SSH session (`top`) | 94.4 Hz | 10.58 ms | 11.77 ms | 13.16 ms | 0 |
| A new SSH login every ~3 s | 91.6 Hz | 10.59 ms | 16.42 ms | 19.61 ms | 16.4 |
| Halo on, mini UART receiver on | 66.7 Hz | 14.99 ms | 15.51 ms | 17.94 ms | — |

- **Uploads dim the panel by ~12 %** through memory and bus contention, not interrupts. In 10 s
  windows, one `dd if=/dev/zero of=/dev/null bs=8M` pinned to CPU 0 dropped the refresh to 18 Hz.
  Three of them, on CPUs 0–2, dropped it to 9.6 Hz. Interrupts on CPU 3 *fell* meanwhile
  (8,460 → 2,500/s). The same `dd` with cache-resident 4 KiB blocks cost 7 % (87.9 Hz).
- **SSH logins cause single long refreshes** (up to ~20–24 ms), i.e. visible flicker.
- **The halo's mini UART receiver** sees a break on every character (its RX pin, GPIO15, is the
  panel's E line): ~12,000 interrupts/s, 30 % dimmer. Fixed: the daemon switches that receiver
  off (see [HALO.md](HALO.md)).

**Interrupts cannot be moved off CPU 3 on a Pi 3.** The BCM2836 interrupt controller driver
(`irq-bcm2836.c`, `bcm2836_arm_irqchip_spin_gpu_irq()`, RPi kernel) routes the GPU interrupts
(USB, SD/Wi-Fi `mmc1`, UARTs, DMA) to the next active core after *every* interrupt. Writes to
`/proc/irq/*/smp_affinity` fail with EIO, `irqaffinity=` has no effect, and `isolcpus` does not
change the active cores. The USB FIQ (`dwc_otg_sim-fiq`, ~2,000/s) and the local timer also run on
every core. The daemon logs this at start, and `/diagnostics` → `hardware.irq_affinity_supported`
records it. On interrupt controllers that support affinity (GIC, Pi 4/5) the daemon warns and
suggests `irqaffinity=0-2`.

## Steady brightness (opt-in)

Settings: `config.json` → `steady_brightness`, `GET/POST /steady_brightness`
`{"enabled": bool}`, the Diagnostics page switch. Off by default.

How it works:
1. When it is switched on, the refresh monitor removes any refresh limit for 3 s.
2. It measures the fastest refresh.
3. It sets the library's refresh limit (`RGBMatrix::SetRefreshLimit()`) to 1 / (1.2 × that
   time). The limit is ≥ 50 Hz, otherwise pacing is skipped.

Every refresh is then padded with dark time to the same period. The brightness stays constant
as long as a stretched refresh still fits, and the whole panel is about 15–17 % dimmer. Turning
it off restores the `--led-limit-refresh` value from the command line.

Measured on ledmat with the same build and the halo on (receiver off). Steady brightness
calibrated to 81 Hz from a fastest refresh of 10.27 ms. Brightness change is the refresh rate relative to idle in
the same mode:

| Condition | Steady off: rate | median / p99 / max | Brightness | Steady on: rate | median / p99 / max | Brightness |
|---|---:|---|---:|---:|---|---:|
| Idle | 94.7 Hz | 10.57 / 11.04 / 12.28 ms | — | 81.0 Hz | 12.35 / 12.35 / 12.38 ms | — |
| 1 GiB scp → `/dev/null` | 84.2 Hz¹ | 11.91 / 13.26 / 13.88 ms | −11 % | 80.2 Hz | 12.35 / 13.41 / 14.00 ms | −1.0 % |
| 1 GiB scp → SD card | 82.0 Hz | 12.16 / 14.21 / 15.31 ms | −13 % | 79.5 Hz | 12.35 / 14.12 / 17.14 ms | −1.9 % |
| Interactive SSH (`top`) | 94.4 Hz¹ | 10.58 / 11.77 / 13.16 ms | −0.3 % | 81.0 Hz | 12.35 / 12.35 / 12.62 ms | 0 % |
| SSH login every ~3 s | 91.2 Hz | 10.61 / 16.83 / 19.28 ms | −3.7 % | 79.5 Hz | 12.35 / 16.91 / 19.88 ms | −1.9 % |

¹ From the baseline run (halo off), same conditions otherwise.

Steady brightness removes the dimming during uploads. It does not remove the single long
refreshes during SSH logins (max ~20 ms either way). Those exceed any useful period, so they need
their cause found on the login path, not pacing.
