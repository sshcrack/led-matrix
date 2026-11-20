# Rear halo light

Optional WS2812B strip behind the display. A Pro Micro (`firmware/halo-pro-micro/`) drives it;
the daemon talks to the Pro Micro over a one-way UART from the Pi (GPIO14 TX). Wiring and parts:
`hardware/wiring/README.md`. Requirements: issue #208.

## Settings

`config.json` → `halo`, `GET/POST /halo` (partial update, same cross-origin and token rules as
every route), and a card on the web UI Integrations page. Open **Set up halo strip** to enter the count
(1–120 with the stock Pro Micro), select LED 0 on the clickable perimeter, and set the
direction as seen from the front. The diagram spaces one dot per configured LED around
the frame. **Display sampling** adjusts the density and depth on the next sampled frame.

Opening setup starts alignment automatically. Red marks the strip's physical LED 0,
green shows the wiring direction, and blue shows where the current selection places the
top-left corner. Click a dot to move the blue light toward that physical corner. LED
count and direction also preview immediately. Save to keep the layout. The browser
renews the test while setup is open and stops it on close. If the browser disconnects,
the Pi restores normal output within 20 seconds.

| Key | Default | Meaning |
|---|---|---|
| `enabled` | `false` | Off sends a short burst of "off" frames, then nothing; the firmware is dark |
| `mode` | `match` | `fixed` (one colour) or `match` (follow the display's edge colours) |
| `brightness` | `50` | 0–100 %, independent of the matrix brightness |
| `color` | `#ffb070` | Colour of `fixed`, `#rrggbb` |
| `led_count` | `40` | Strip length, 1–200, sent in every frame (no reflash needed) |
| `device` | `/dev/serial0` | UART device, 115200 8N1. Only `/dev/serial<N>`, `/dev/ttyAMA<N>`, `/dev/ttyS<N>`, `/dev/ttyUSB<N>`, `/dev/ttyACM<N>` (and `/dev/pts/<N>` for tests) are accepted, and the opened file must be a tty: the daemon opens it as root before the privilege drop |
| `rotation`, `reverse` | `0`, `false` | Rotate the 8 edge zones / reverse their direction to fit where the strip starts and which way it runs. Zone 0 is the left half of the top edge, going clockwise seen from the front |
| `start_between_zones` | `false` | Place LED 0 halfway between the first two arranged edge colors. The daemon sends 16 colors in this case, including the original anchors |
| `first_led` | `-1` | Position of LED 0 among equally spaced slots, numbered clockwise from the top-left corner. `-1` uses the legacy zone alignment. Individual positions shift the Pi's 16 shipped color anchors without a protocol or firmware change |
| `sample_columns` | `8` | 1–16 samples per row (2–32 pixels per zone) |
| `sample_depth` | `6` | Inner row depth in display pixels (3–24). The outer row is 2 pixels in. Depths are clamped on small displays |

`GET /halo` returns `{settings, error, device_open, frames_sent}`. The daemon works without any
UART: on the emulator and dev machines it logs `no UART at …` once and keeps the settings.
`POST /halo/test` accepts `{"led_count": 1..120}` to run the temporary alignment pattern or
`{"led_count": 0}` to stop it. Pass a draft `first_led` (0 to count minus one) and
`reverse` (boolean) to include the moving blue corner reference. With just a count,
the original white and green test remains available. `GET /halo` includes `test_active`
and `test_first_led` (-1 when no position is under test). The test never writes the configuration.

## How `match` works (and what it costs)

The sender thread ticks at 15 Hz and, in `match` mode, sets an atomic "sample wanted" flag. The
render path's single output point, `FrameSink::present()`, calls `Halo::Service::offer()` right after
`LiveFrame::SnapshotStore::capture_if_requested()`, on the composed canvas that is about to be
swapped in (post-processing and transitions included). `offer()` is one relaxed atomic load while
the halo is off or between ticks. When a sample is due it reads 128 pixels (8 zones × 8 positions
× 2 depths along the edge, middle 75 % of each half edge) with `GetPixel` — at most 15 times a
second — and stores the 8 zone means. The sender smooths them (exponential, 0.35 per tick) and
sends. It never reads the whole frame and never touches `/live_frame`: that stays pull-only and
needs no browser. A held (unchanged) frame costs nothing, the last sample stays valid. While the
matrix is turned off the hardware loop submits black, so the halo fades out with it.

## Protocol

One-way, Pi → Pro Micro, 115200 8N1, ≤ 20 frames/s (the daemon sends ~15/s, also as heartbeat).

```
0xA5 0x5A  LEN  VERSION  BRIGHTNESS  LED_COUNT  ZONES  (R G B)×ZONES  CRC8
```

- `LEN` = 4 + 3·ZONES (7…52); `VERSION` = 1; `LED_COUNT` 1…200; `ZONES` 1…16 (the daemon sends 8 or 16).
- `CRC8` = CRC-8, polynomial 0x07, init 0, no reflection, over `LEN` and the payload
  (check value for `"123456789"` is `0xF4`).
- The ZONES colours are key colours spaced evenly around the LED loop; the firmware interpolates
  linearly between neighbours, the last blends into the first.
- Fixture used by the tests on both sides: brightness 128, 40 LEDs, zones red and `#00FF10` →
  `A5 5A 0A 01 80 28 02 FF 00 00 00 FF 10 DD`.
- The parser (`shared/matrix/include/shared/matrix/halo/protocol.h`) is the single definition the
  firmware and the daemon compile; it resynchronises inside failed frames, so garbage, partial
  frames and console boot messages on GPIO14 never light the strip.

## Pi setup

1. `/boot/firmware/config.txt` (older images `/boot/config.txt`): `enable_uart=1`.
2. Remove `console=serial0,115200` from `cmdline.txt` and stop the login getty on the port:
   `sudo systemctl disable --now serial-getty@ttyS0` (or raspi-config → Interfaces → Serial Port:
   login shell **No**, hardware **Yes**). Otherwise Linux boot messages and a login prompt are
   written to the Pro Micro. They are ignored by the checksum, but the port must be free.
3. Reboot. `/dev/serial0` exists. On a Pi 3B it is the mini UART (`ttyS0`) unless you add
   `dtoverlay=disable-bt` (then PL011); 115200 baud works on both once `enable_uart=1` pins the
   core clock.
4. Enable the halo in the web UI or `curl -X POST :8080/halo -d '{"enabled":true}'`.

**Privileges.** The service starts as root and the matrix library then drops to `pi:pi` with
`setresgid/setresuid` only (no `initgroups`, `rpi-rgb-led-matrix/lib/led-matrix.cc`
`drop_privs`). Supplementary groups survive that, so the unit sets `SupplementaryGroups=dialout`
and the daemon can open the UART later. It holds the UART only while the halo is on or an alignment
test runs: switched off, it sends the off frames and then closes the device. With the halo enabled
in `config.json` at start, `Halo::Service::preopen` opens it before the privilege drop as well.

**GPIO14 / GPIO15 check (library source).** With `--led-parallel=2` and the `regular` mapping
(`lib/hardware-mapping.c`) the library only claims bits of the pins it uses: GPIO14 is `p2_r1`,
used only for `--led-parallel=3`, so TX is untouched. GPIO15 is the `e` address line, which
`Framebuffer` adds to its row mask for 64-row panels (`double_rows > 16`); `GPIO::RequestOutputs`
switches exactly those pins to output with a per-pin function-select write after the kernel set
up the UART. That takes GPIO15 away from the UART's RX and does not touch GPIO14's TX function.

**An open UART is not free.** With GPIO15 no longer routed to RX, the mini UART's receiver sees a
constant break: one interrupt per character time, about 12,000 per second at 115200 baud. Clearing
`CREAD` or setting `IGNBRK` does not stop them, because the driver keeps receive interrupts on while
the port is open. Measured on ledmat (Pi 3B, 2026-11-20, `digitalrain`): with the port open, the
daemon used about 105 % CPU instead of 87 %, and about 12 % of frames were late instead of 0.14 %.
That's why the daemon closes the port while the halo is off.

**The receiver is switched off.** The halo only transmits, so at start (still root, before the
library drops privileges) the daemon clears the receiver-enable bit (bit 0 of `AUX_MU_CNTL_REG`,
found from `/sys/class/tty/ttyS0/iomem_base`) of the mini UART behind `halo.device`
(`src_matrix/halo/mini_uart.cpp`). The 8250 driver only rewrites that register in RS485 mode, so
the receiver stays off until reboot, across closes and reopens, and the transmitter keeps working.
The daemon leaves PL011 UARTs and kernel consoles alone. Measured on ledmat (2026-11-20,
`digitalrain`, refresh timing from `/diagnostics` → `refresh`, 60 s windows):

| Halo on | ttyS0 interrupts/s | Refresh rate idle | Median refresh | During a 1 GiB scp |
|---|---:|---:|---:|---:|
| Receiver on | 11,968 | 66.7 Hz | 14.99 ms | 57.6 Hz |
| Receiver off | 60 | 94.7 Hz | 10.57 ms | 83.7 Hz |
| Halo off (reference) | 0 | 94.8 Hz | 10.56 ms | 84.2 Hz |

With the receiver on, the panel was about 30 % dimmer while the halo was on (brightness follows
the refresh rate). The TX rate was unchanged (496 vs 480 bytes/s).

Other options checked: the `uart1` overlay cannot move RX off GPIO15 (`rxd1_pin=41` fails because
GPIO41 belongs to the firmware mailbox, and GPIO33 is the Bluetooth UART's RX), and
`smp_affinity` cannot keep the interrupt off the refresh core on a Pi 3 (see
[REFRESH_FLICKER.md](REFRESH_FLICKER.md)). The hardware fix would be to wire the halo to a UART
whose RX is not a panel line, e.g. a USB serial adapter (`/dev/ttyUSB0`/`ttyACM0`, the Pro Micro's
own USB port), but with the receiver off there is nothing left to fix.

## Not verified on hardware

Everything above the wire is unit/integration tested (`ctest -R halo`: protocol, parser
resync/fuzz, sampling, smoothing, settings, the real sender service writing into a pty). Not yet
run on the Pi or the Pro Micro: UART electrical levels through the BSS138 converter, the strip
output timing, current cap against a meter, the zone
rotation/direction for the real strip start.
