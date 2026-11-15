# Electronics: power distribution and halo controller

Status: **prototype design, unvalidated, not approved for fabrication.** Nothing here has been
built or measured. The outputs in `exports/` are marked PROTOTYPE / UNVALIDATED.
[docs/verification.md](docs/verification.md) lists what was checked and what is still open.

## Architecture

The display uses a 2×2 grid of Waveshare P3 64×64 HUB75E panels (1/32 scan, 5 V / 4 A, at most
20 W each). It is driven by a Raspberry Pi 3B through the existing software:
`--led-rows=64 --led-cols=64 --led-chain=2 --led-parallel=2`, the default (`regular`) GPIO
mapping, and address lines A–E.

The design uses **one custom board** plus off-the-shelf parts:

| Part | What it is | Custom? |
|---|---|---|
| 24 V supply | Mean Well GST160A24-R7B: external Class I brick, 160 W, 4-pin power DIN plug | no |
| Enclosure jack | Kycon KPJX-PM-4S panel-mount 4-pin power DIN jack, AWG18 pigtail to J1 | no |
| **Power + halo board** | `power-halo-board/`, 136 × 86 mm, 4 layers | **yes** |
| HUB75 interface | Electrodragon RGB matrix driver board for Raspberry Pi, V2 (the board the main README links) | no |
| Controller | Raspberry Pi 3B (Wi-Fi only) | no |
| Halo | 40 × WS2812B-class LEDs on one 5 V data line | no |

```
230 V ─ brick (outside the enclosure, 24 V SELV) ─ DIN jack ─ AWG18 ─ J1
  J1 ─ F1 8 A ─ LM74700 + PSMN5R2 ideal diode ─ SMBJ30A + bulk ─ +24V
  +24V ─ 4× TPSM63610 → 5.0 V ─ TPS259470A eFuse (5.0 A) ─ J101..J401 ─ panel 1..4 power
  +24V ─ TPSM63610 AUX → +5V_AUX ─┬─ TPS259470A (3.0 A) ─ J501 ─ Electrodragon 5 V in ─ Pi
                                  ├─ TPS259470A (2.6 A, MCU-enabled) ─ J601 ─ halo (+5V, DIN, GND)
                                  └─ ATtiny1616, AHCT buffer, LEDs
  Pi GPIO14 (TXD) ─ 1k ─ AHCT1G125 ─ ATtiny1616 UART RX       (halo commands)
  Pi GPIO26 ─ 1k ─ JP601 ─ BSS138 ×2 ─ EN of the four panel bucks (panel request)
```

Why one board: the brick keeps mains out of the enclosure. The Electrodragon V2 already
buffers HUB75 for two parallel chains with the E line, so a custom HUB75 board would add
nothing. What is missing is protected 5 V power for 85 W of panels plus the Pi and halo, and a
halo driver that uses no matrix pins. One board does both. The decisions behind this:

- **24 V distribution** instead of a 5 V brick. One 5 V rail would carry 18 A or more through
  connectors and wiring. At 24 V the input carries 4.9 A worst case. Each panel gets its own
  converter and its own limit.
- **Hardware current limit per output.** Each panel branch has an eFuse limited to 5.0 A ±10 %
  (the panel is rated 4 A). The Pi and halo branches are limited to 3.0 A and 2.6 A. Software
  brightness limits are not counted as protection anywhere.
- **No backfeed.** Every 5 V output eFuse blocks reverse current. The Pi logic lines only sink
  into the board through 1 kΩ resistors and high-impedance inputs. The board never drives a Pi
  pin.
- **Halo without matrix pins.** It uses GPIO14 (UART TXD) and GPIO26. With
  `--led-parallel=2` the library leaves chain-3 pins free (GPIO 2, 3, 14, 16, 21, 26). The
  ATtiny1616 generates the WS2812 timing, so the Pi only sends short UART commands. The halo
  must never be fed by continuously pushed `/live_frame` capture. The API preview stays
  pull-only.
- **Electrodragon V2 is fine with conditions** (see [docs/wiring.md](docs/wiring.md#hub75-and-the-electrodragon-v2)):
  - Set the E-line selector so pin 8 carries E and pin 4 is GND.
  - Leave its third HUB75 output (P2) unconnected.
  - Power it only from J501.

## Files

| Path | Content |
|---|---|
| `power-halo-board/` | KiCad 9 project: hierarchical schematic (root + Input, Panel ×4, Aux, Halo), routed PCB, project-local symbol and footprint library `lib/`, net classes and rules (`.kicad_pro`, `.kicad_dru`) |
| `tools/circuit.py` | the circuit as data: every part with value, MPN, manufacturer, datasheet, rating and pin-to-net map |
| `tools/gen_lib.py`, `gen_sch.py`, `gen_project.py` | write the library, schematic and project files from `circuit.py` |
| `tools/gen_pcb.py` | placement, power zones, Freerouting autorouting, ground pours, stitching, silkscreen |
| `tools/check_board.py` | ERC, independent netlist check, DRC with schematic parity, prototype exports |
| `tools/check_netlist.py` | checks every pin-to-net connection of the exported netlist against `circuit.py` |
| `tools/spice/` | ngspice decks (thresholds, enable sequence, hot-plug), runner and `results.txt` |
| `tools/power_budget.py` | worst-case power and thermal budget |
| `tools/gen_envelope.py` | height envelope and plug keepouts as JSON and STEP for the enclosure |
| `docs/wiring.md` | harnesses, pinouts, keying, wire gauge, strain relief, HUB75 and the Electrodragon V2 |
| `docs/power-and-thermal.md` | supply choice, rails and loads, protection, thermal budget, ventilation |
| `docs/mechanical-interface.md` | outline, holes, connector positions and directions, heights, keepouts |
| `docs/verification.md` | what was verified, with results, and the open gaps |
| `docs/bom.csv` | BOM (copied from the export) |

## Board

- 136 × 86 mm, 1.6 mm FR4, 4 layers, ENIG.
- Copper is 2 oz on the outer layers and 1 oz on the inner layers.
- Layers:
  - F.Cu: components, +24 V and 5 V power zones.
  - In1: solid GND.
  - In2: +5V_AUX plane regions, GND elsewhere.
  - B.Cu: signals and a GND pour.
- Net classes:
  - `Power24`: +24V and VIN_*, 0.5 mm, 0.3 mm clearance.
  - `Power5`: panel VOUT/VPANEL, VPI and VHALO, 0.4 mm.
  - `GND`, `Power5AUX`, `Default`: 0.2 mm.
- Custom rules: 24 V clearance 0.3 mm, minimum 0.4 mm power taps, 0.5 mm copper-to-edge.
  Autorouted tracks and vias are additionally kept 0.85 mm from the edge by rule areas.
- The committed `.kicad_pcb` has its **zones unfilled**: filled it is about 1.9 MB, against the
  repo's 2 MiB per-file guard. Press `B` in KiCad, or use `exports/power-halo-board-filled.kicad_pcb`.

## Regenerating

Requires KiCad 9 (`kicad-cli` and the `pcbnew` Python module), ngspice, Java 21+, xvfb-run and
Freerouting 1.9.0. The jar is not committed: set `FREEROUTING_JAR`, or place it at
`tools/freerouting-1.9.0.jar`. Freerouting runs headless with analytics off, but checks for new
versions online at startup. `LEDPANEL_SCRATCH` sets its work directory (default
`.route/`, git-ignored).

```bash
cd hardware/electronics
python3 tools/gen_lib.py && python3 tools/gen_sch.py && python3 tools/gen_project.py
mkdir -p .route && kicad-cli sch export netlist -o .route/net.net power-halo-board/power-halo-board.kicad_sch
python3 tools/gen_pcb.py place .route/net.net
python3 tools/gen_pcb.py route     # Freerouting; retried until all nets are connected
python3 tools/gen_pcb.py finish    # pours, stitching, silkscreen, title block, stackup
python3 tools/check_board.py       # ERC + netlist + DRC/parity, then exports/ (exit 1 on any failure)
python3 tools/spice/run_spice.py
python3 tools/power_budget.py
python3 tools/gen_envelope.py && <python with cadquery> tools/gen_envelope.py --step
```

Freerouting is not deterministic. A new `route` run gives different tracks, so the finished
board is committed. `finish` reuses the last route in the scratch directory
(`routed.kicad_pcb`). Re-run `place` and `route` only after placement or netlist changes.
Hand edits in KiCad are fine: `check_board.py` verifies whatever board is committed.

## Outputs (git-ignored `exports/`)

`check_board.py` writes:

- The schematic PDF and BOM CSV.
- Board PDFs: top, bottom, in1, in2, assembly.
- `fab-PROTOTYPE-UNVALIDATED/`: Gerbers, Excellon drill files with maps, the front position
  file and a README, plus a zip of the same.
- Board STEP. It is board-only when KiCad's 3D model package is not installed. Use the envelope
  STEP for enclosure work.
- Outline SVG and DXF, the filled board, and the reports under `reports/`.

`gen_envelope.py` writes `mechanical/`. None of these are a fabrication release.
