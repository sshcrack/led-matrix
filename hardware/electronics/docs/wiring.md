# Wiring, connectors and pinouts

Pin 1 of each connector on the board is marked on the silkscreen (labels next to the pins).
Positions are in [mechanical-interface.md](mechanical-interface.md). Every harness is
internal to the enclosure except the brick's own DC cable.

## Overview

| Board connector | Family (board part / mating housing + crimp) | Pins | Goes to | Wire | Max current |
|---|---|---|---|---|---|
| J1 24V IN | Molex Mini-Fit Jr 2×2 R/A 39-30-0040 / 39-01-2040 + 39-00-0039 | 1, 2 = +24V; 3, 4 = GND | enclosure DIN jack (Kycon KPJX-PM-4S) | 4× AWG18, ≤ 0.5 m | 4.9 A worst case |
| J101, J201, J301, J401 PANEL | JST VH 4-pin side entry S4P-VH / VHR-4N + SVH-21T-P1.1 | 1, 2 = +5V; 3, 4 = GND | panel 1..4 power input | 4× AWG18, ≤ 0.5 m (AWG16 with SVH-41T-P1.1 where the crimp allows) | 5.0 A limit |
| J501 PI 5V | JST VH 2-pin S2P-VH / VHR-2N + SVH-21T-P1.1 | 1 = +5V, 2 = GND | Electrodragon V2 5 V input (XH2.54: XHP-2 + SXH-001T-P0.6) | 2× AWG22, ≤ 0.3 m | 3.0 A limit |
| J601 HALO | JST VH 3-pin S3P-VH / VHR-3N + SVH-21T-P1.1 | 1 = +5V, 2 = DIN, 3 = GND | halo strip input (+5V, DIN, GND) | 3× AWG20, ≤ 0.5 m | 2.6 A limit |
| J603 PI LOGIC | JST XH 3-pin side entry S3B-XH-A / XHP-3 + SXH-001T-P0.6 | 1 = GND, 2 = GPIO14 TXD, 3 = GPIO26 | Electrodragon V2 extra-pin headers (2.54 mm sockets) | 3× AWG26, ≤ 0.3 m | signal |
| J602 UPDI | 1×3 2.54 mm pin header (Würth 61300311121) | 1 = UPDI, 2 = +5V_AUX, 3 = GND | UPDI programmer, bench only | — | — |
| JP601 PANEL MODE | 1×3 2.54 mm header + shunt | 1-2 = Pi controls panels (GPIO26), 2-3 = panels always on | — | — | — |

Keying: Mini-Fit Jr, VH and XH housings are polarized and mate one way only. Mini-Fit Jr and VH
latch positively. XH holds by friction. The four panel harnesses are identical and
interchangeable: any panel connector can feed any panel. The Pi and halo harnesses use
different pin counts (2 and 3), so they cannot be swapped with a panel harness or with each
other.

## 24 V input

The GST160A24-R7B brick has a 4-pin power DIN plug: pin 1 = +V, 2 = −V, 3 = −V, 4 = +V. Its −V
is tied to the AC protective earth (Class I).

The enclosure jack is a Kycon KPJX-PM-4S, rated 7.5 A per pin at 48 V DC and fixed with M2.5
screws. Wire its pigtail like this:

| DIN jack pin | Mini-Fit Jr pin (J1) | Net |
|---|---|---|
| 1 | 1 | +24V |
| 4 | 2 | +24V |
| 2 | 3 | GND |
| 3 | 4 | GND |

- Both contacts of each pole are wired, so each AWG18 conductor carries about 2.5 A.
- Insulate the jack's solder cups with heatshrink. Tie the pigtail to an anchor in the enclosure
  within 50 mm of the jack, and again before J1.
- Plug the DC cable in before switching the brick on. Its soft start (rise time ≤ 50 ms) then
  charges the board's input capacitance gently. Hot-plugging into a running brick is allowed,
  but it causes an 80–100 A, ~50 µs current spike: see
  [power-and-thermal.md](power-and-thermal.md#inrush-and-hot-plug).

## Panels

The Waveshare P3 64×64 panel takes 5 V on its own power connector. Waveshare's wiki gives the
rating, 5 V / 4 A ≤ 20 W, but not the connector pin order. **Before the first connection, check
each panel harness with a meter against the +5V/GND marking on the panel.** The board protects
against overload and backfeed. It cannot protect a panel from a harness crimped with reversed
polarity.

- Run each harness on its own route, with a service loop and a tie point within 50 mm of the
  board.
- Drop at 4 A: eFuse, contacts and a 0.5 m AWG18 pair together lose about 0.29 V, so the panel
  sees about 4.73 V at full white. Shorter or AWG16 harnesses improve this.

## Raspberry Pi power

J501 feeds the Electrodragon V2's 5 V input, an XH2.54 2-pin header. That input sits before the
Electrodragon's own protection, which then powers the Pi through the GPIO 5 V pins. The
TPS259470A on J501 limits the branch to 3.0 A ±10 % and blocks reverse current. Rules:

- **Do not plug a USB supply into the Pi's micro-USB while the board powers the Pi.** The Pi 3B
  has no ideal diode between its 5 V pins and micro-USB, so the board's 5 V would push into the
  USB supply. The eFuse protects the board side only.
- Use AWG22: the XH crimp takes AWG22–28. Keep the run ≤ 0.3 m. At 2.5 A the drop is about
  0.1 V.

## Pi logic (halo commands and panel request)

The library leaves chain-3 GPIOs free at `--led-parallel=2`. The Electrodragon V2 brings some of
them out on its "extra pins" headers: top IO2, IO12 (a silkscreen typo, it is **IO14**) and IO3;
bottom IO21, IO26 and IO16. Use the 5V/3V3/GND header for ground.

| J603 pin | Signal | Electrodragon V2 pin | Pi |
|---|---|---|---|
| 1 | GND | GND on the 5V/3V3/GND header | GND |
| 2 | PI_TXD | "IO12" (= IO14) | GPIO14 / UART TXD, header pin 8 |
| 3 | PI_PANEL_REQ | IO26 | GPIO26, header pin 37 |

On the board side:

- **GPIO14** passes through 1 kΩ to a SN74AHCT1G125. Its TTL input switches at V_IH 2.0 V, and
  the buffer lifts the signal to 5 V for the ATtiny1616 RX. A 100 kΩ pull-down holds a defined
  level when the Pi is absent.
- **GPIO26** passes through 1 kΩ into JP601, then a 100 kΩ pull-down and the BSS138 inverter
  pair that drives `EN_PANEL`.
- Neither line sources current into the Pi.

## JP601 panel mode

- **2-3** (fit at assembly, bring-up default): panels start about 1 ms after +5V_AUX comes up,
  whatever the Pi does.
- **1-2**: panels run only while the Pi drives GPIO26 high. They switch off within 0.1 ms of
  GPIO26 going low, or of the Pi stopping. This keeps a crashed or halted Pi from leaving
  panels lit with a frozen row pattern. It needs daemon support that does not exist yet (see
  [verification.md](verification.md#not-implemented-software)).

## Halo

J601 carries +5V, the WS2812 data line (DIN, 5 V level) and GND. The data leaves the MCU
through a 330 Ω series resistor. The halo branch has an SMAJ6.0A TVS on +5V and a PESD5V0S1BA
on DIN, both placed next to J601 (the TVS cathode is about 6 mm from J601 pin 1).

- The halo eFuse is off until the ATtiny1616 enables it, with a 100 kΩ pull-down on its enable.
  Its limit is 2.6 A ±10 %, enough for 40 LEDs at full white (about 2.4 A).
- Feed power at the strip's input end. For runs over about 1 m or more than 60 LEDs, also inject
  power at the far end from the same J601 +5V/GND.

## UPDI programming (J602)

Pin 1 is UPDI, pin 2 is +5V_AUX and pin 3 is GND. Program with a serialUPDI adapter while the
board runs from 24 V, and **leave the adapter's 5 V pin unconnected**: pin 2 is the board's own
rail and is there for sensing only.

## Status LEDs

| LED | Colour | On means |
|---|---|---|
| D2 | green | +24V present (after the reverse-polarity stage) |
| D531 | green | +5V_AUX present |
| D101, D201, D301, D401 | red | that panel's eFuse reports a fault: current limit lasting longer than ITIMER, overtemperature, or reverse current. Under/overvoltage shuts the branch off without lighting it |
| D501 | red | Pi branch eFuse fault; also lights on reverse current, e.g. a USB supply plugged into the Pi |
| D601 | red | halo branch eFuse fault (same conditions as the panel branches) |
| D602 | green | halo controller status (firmware-defined) |

## HUB75 and the Electrodragon V2

The HUB75 data path does not change and does not touch this board.

The pin mapping was audited against the library source the build uses, `lib/hardware-mapping.c`
of the `rpi-rgb-led-matrix` fork, v0.1.13 as built by vcpkg, mapping `regular`:

| Signal | GPIO |
|---|---|
| OE | 18 |
| CLK | 17 |
| LAT (strobe) | 4 |
| A, B, C, D | 22, 23, 24, 25 |
| E | 15 |
| Chain 0 R1 G1 B1 R2 G2 B2 | 11, 27, 7, 8, 9, 10 |
| Chain 1 R1 G1 B1 R2 G2 B2 | 12, 5, 6, 19, 13, 20 |
| Chain 2 (unused at `--led-parallel=2`) | 14, 2, 3, 26, 16, 21 |

- 64 rows at 1/32 scan need address lines A–E, so E (GPIO15, the UART RXD pin) is in use.
  The halo link is therefore transmit-only, on GPIO14.
- The Electrodragon V2 is derived from hzeller's active-3 adapter. Its pages state the same
  chain 0/1/2 pin assignment and "ROW_E = GPIO 15".
- The E-line selector on the Electrodragon V2 is the 2×4 "E_BUFF / P4 / P8 / GND" holes. For a
  HUB75E panel, connect E_BUFF to pin 8 and pin 4 to GND. A user report on the vendor page
  confirms this for 64×64 panels. Check that the Waveshare panel's input connector shows E on
  pin 8. This is the standard HUB75E layout, but Waveshare's wiki does not list pin numbers.
- Leave the third HUB75 output (P2) unconnected. Its buffer still sees GPIO14 and GPIO26 as
  inputs, which is harmless, but its output would carry the UART and panel-request signals.
- Wiring:
  - Chain 0: Electrodragon P0 → panel A in → panel A out → panel B in.
  - Chain 1: P1 → panel C → panel D.
  - With `--led-parallel=2`, chain 0 is the upper 64 rows and chain 1 the lower.
  - Confirm the panel order with a test pattern. If two panels show swapped halves, swap the
    ribbons, not the software.
- Avoid revision MPC1119 of the Electrodragon V2: the vendor reports a board fault in units
  from February to April 2025, fixed with a wire, while MPC1120 is fully fixed. Check the
  revision on arrival.
