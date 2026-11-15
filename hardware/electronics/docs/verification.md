# Verification report: power + halo board rev A

Date: 2026-11-15. Status: **prototype, unvalidated. No board has been built, no measurement
made, and no fabrication approval given.** Everything below was done on the design files.

## Done, with results

| Check | Tool | Result |
|---|---|---|
| ERC, all severities, no exclusions | `kicad-cli sch erc` via `tools/check_board.py` | **0 violations** |
| Independent netlist check: every pin-to-net connection in the exported netlist against `tools/circuit.py` | `tools/check_netlist.py` | **pass**: 191 components, 102 nets, 513 pins |
| DRC on the zone-filled board, all severities, no exclusions, with schematic parity | `kicad-cli pcb drc --severity-all --schematic-parity` | **0 violations, 0 unconnected, 0 parity issues** |
| Net classes and rules | `.kicad_pro`, `.kicad_dru` | 24 V clearance 0.3 mm; power taps ≥ 0.4 mm; edge 0.5 mm; autorouter kept 0.85 mm from edges. Smallest feature in use: 0.2 mm track, ~0.2 mm space (TI land-pattern gaps), 0.3 mm drill, 0.15 mm annular ring |
| Gerber sanity | KiCad skill `analyze_gerbers.py` | 0 findings |
| Schematic ↔ PCB cross-check | skill `cross_analysis.py` | 0 findings |
| Schematic analyzer | skill `analyze_schematic.py` | 78 findings, 2 warnings, both assessed below |
| PCB analyzer (`--full`) | skill `analyze_pcb.py` | 68 findings, 6 warnings, assessed below; routing complete |
| EMC pre-compliance heuristics (CISPR 32 class B) | skill `analyze_emc.py` | risk score 65.5/100; 6 errors and 16 warnings, assessed below |
| Threshold, sequencing and hot-plug simulation | ngspice 45.2, `tools/spice/run_spice.py` | 12 checks PASS, 2 informational; see `tools/spice/results.txt` |
| Power and thermal budget | `tools/power_budget.py` | brick 73 % loaded; worst-case TJ within limits, eFuse margin about 10 K ([power-and-thermal.md](power-and-thermal.md)) |
| HUB75 pin mapping | library source `lib/hardware-mapping.c` (fork v0.1.13, mapping `regular`) and the Electrodragon V2 vendor pages | chain 0/1 + A–E as documented; GPIO14 and GPIO26 free at `--led-parallel=2` ([wiring.md](wiring.md#hub75-and-the-electrodragon-v2)) |
| Mechanical envelope | `tools/gen_envelope.py` | JSON + STEP of outline, holes, part boxes and plug keepouts |

### SPICE results (behavioral models, not vendor models)

- **Thresholds** with 1 % resistors and datasheet min/max thresholds (`thresholds.cir`):
  - Panel eFuse UV rise 4.19–4.46 V and fall 3.81–4.07 V.
  - Panel OV trip 5.75–6.12 V, release 5.23–5.58 V. That is above the 5.12 V worst-case
    output, so a converter in tolerance never trips OV.
  - Halo OV 5.74–6.13 V.
  - AUX converter on at 11.9–16.8 V, off at 10.7–11.1 V.
- **Enable sequence** (`en_sequence.cir`):
  - With JP601 at 1-2, EN_PANEL stays at 0.02 V (below the 0.4 V wake threshold) until GPIO26
    rises. The panels enable 1.5 ms after GPIO26 goes high and disable 0.08 ms after it falls.
  - With JP601 at 2-3, the panel converters cross their EN threshold 0.4 ms before +5V_AUX
    reaches 4.75 V, with AUX above 4 V by then.
- **Hot-plug** into a running brick (`hotplug.cir`):
  - With the bulk capacitors, +24V peaks at 23.4 V (no overshoot). Without them it rings to
    32.9 V, which is why they are fitted.
  - Peak current is 83–103 A and I²t 0.21–0.41 A²s. The fuse rating for this pulse is open, see
    below.

### Analyzer findings and assessment

- **EMC GP-001** (6 errors, 6 warnings: "reference plane coverage" for VPANEL 1–4, VHALO,
  FORCE_ON, VPI and slow control nets): low concern.
  - The power nets are DC outputs downstream of the converters' output capacitors. Their
    current returns in the In1 solid GND plane, directly under F.Cu.
  - The coverage shortfall comes from the In2 split between the +5V_AUX regions and GND.
  - The control nets are static or change once at power-up.
- **EMC RP-001** (7 warnings: layer transitions without a stitching via within 1 mm):
  - EN_PANEL, HALO_EN(_F), PANEL_ON, UART_RX and UPDI are static or slow. That is acceptable.
  - HALO_DOUT is 800 kHz with ~10 ns edges and the 330 Ω series resistor. It has one
    transition, and about 145 stitching vias sit on a 4 mm grid.
  - HALO_DATA/HALO_DOUT run on B.Cu over the In2 +5V_AUX region. +5V_AUX is decoupled to GND,
    so it acts as an AC reference, but this is the one signal to probe for emissions.
- **EMC ES-001** ("D1 is 24 mm from J201"): false association. D1 protects the 24 V input at
  J1. J201 is a 5 V panel output whose ESD path is the eFuse plus internal wiring.
- **EMC GP-004** (F.Cu GND pour fill 59 %): expected. F.Cu carries the power zones. In1 is the
  solid GND reference.
- **EMC DC-003** (C1, the LM74700 charge-pump capacitor, 3.5 mm from a via): C1 sits between
  VCAP and the anode pin, not on a supply rail, and does no high-frequency decoupling.
- **PCB PM-002** (J1 0.6 mm, D1 0.85 mm, R505/R506 0.87 mm from the edge):
  - J1 is an edge connector by design.
  - The others are above the 0.5 mm DRC limit and there is no depaneling: single board, routed
    outline.
- **PCB TV-001** (Q1 drain tab without thermal vias): Q1 dissipates ≈ 0.17 W in a 2 oz +24V
  pour. No vias are needed.
- **PCB TE-001** (test points on 6 of 137 nets): deliberate. Test points cover every rail
  (+24V, GND, four panel outputs, VPI, +5V_AUX, VHALO). Logic is probed at component pads.
- **Schematic RS-001** (VIN_RAW has no power source): it is fed from connector J1, so this is
  expected.
- **Schematic DS-002** (no local datasheet directory): the datasheet URLs are in every part's
  `Datasheet` field. The PDFs used were read during design and are not committed.
- **Gerbers**: before the final run, small 0.35 mm drill marks were plotted onto the paste
  layers. The board's plot settings now disable drill marks, and the analyzer reports 0
  findings.

## Not verified / open (decide or check before ordering)

Part data that could not be checked against the manufacturer's datasheet (fetches were refused
or the datasheet was not found). Confirm before ordering:

- Littelfuse **SMBJ30A** (VBR, VC at IPP), **SMAJ6.0A**, Nexperia **PESD5V0S1BA**.
- Littelfuse **0453008.MR**: voltage rating ≥ 32 V DC, interrupting rating, cold resistance,
  and **melting I²t ≥ 2 A²s**. The hot-plug pulse is 0.4 A²s. If the I²t is too low, pick a
  slower NANO2 or require "DC plugged before mains".
- Panasonic **EEE-FK1H101P / EEE-FK1A221P**: case size and ESR (the hot-plug damping depends on
  the 100 µF ESR, simulated at 0.15–0.7 Ω).
- Molex **39-30-0040**: rating with two contacts paralleled, height.
- Murata MLCC part numbers in the BOM: confirm the 1210 10 µF / 50 V X7R parts' DC-bias
  derating. The simulation assumes 4.5 µF each at 24 V.
- **ATtiny1616-SNR** ordering code and package. The footprint is SOIC-20 wide (7.5 mm). Check
  that the code matches that package.

Other open items:

- **Waveshare panel**:
  - The power connector pin order is not published. Meter-check every harness.
  - HUB75E pin 8 = E is assumed, being the standard layout.
  - The panel's input capacitance, which sets eFuse inrush, is assumed ≤ 2000 µF.
  - Supply tolerance at 4.73 V under full load is not specified.
- **Project-local footprints** (TI RDF0022A B3QFN-22, TI RPW0010A VQFN-HR-10) were drawn from
  the datasheet land patterns. Print them 1:1 and compare with the datasheet drawings and the
  TI EVM layout before ordering.
- **Converter loop and transient response** were not simulated. Values follow the TPSM63610
  datasheet's application guidance (COUT 4 × 47 µF, CFF 22 pF, RBOOT 100 Ω), but the loop was
  not checked against it numerically. TI's WEBENCH or PSpice
  model was not run.
- **The KiCad skill's thermal and SPICE analyzers** (`analyze_thermal.py`,
  `simulate_subcircuits.py`) fail in this environment with
  `ModuleNotFoundError: datasheet_types`. They were replaced by `tools/power_budget.py` and the
  hand-written ngspice decks. The skill analyzers' results are therefore missing.
- **EMC**: no measurement. 1 MHz switching is inside the AM band (spread spectrum reduces the
  peaks). The 24 V brick cable is the longest conductor. Pre-scan with a near-field probe at
  bring-up, and add a clip-on ferrite on the DC cable if needed. The brick itself is EN55032
  class B.
- **Thermal**: estimates only. Measure the TPSM63610 and TPS259470A case temperatures at full
  white with the enclosure closed.
- **Height envelopes** of the Mini-Fit Jr, VH and XH connectors are conservative assumptions,
  not drawing values. KiCad's 3D model package is not installed here, so the plain STEP from
  `check_board.py` contains the board only.
- **Fabrication**: no fab DRC or DFM check (2 oz outer copper with 0.2 mm spacing is at the
  edge of standard 2 oz capabilities), no impedance requirement, no panelization.
- **Freerouting** contacts its update server at startup (prints "New version available").
  Analytics are disabled. No design data is sent, but the network access exists.
- **The Electrodragon V2** was assessed from vendor pages, not from a board in hand. Check the
  revision (avoid MPC1119), the E-line selector, and that its 5 V input feeds the Pi's 5 V pins.

## Not implemented (software)

These are features, not protection. The board is safe without them: every protection above is
hardware. Default assembly is JP601 at 2-3 with the halo off.

1. **Halo firmware** (ATtiny1616), flashed through J602 UPDI:
   - A UART receiver with framing and checksum, so boot noise on GPIO14 is ignored.
   - The WS2812 driver on HALO_DATA.
   - HALO_EN control and HALO_FLT_N monitoring.
   - A status LED pattern.
2. **Daemon halo driver**:
   - Derives halo colours from scene and director state at a low rate and writes them to the
     UART on GPIO14.
   - It must not use continuously pushed `/live_frame` capture. The API preview stays pull-only.
3. **Pi configuration for the UART**:
   - Enable the UART TX on GPIO14 (`enable_uart=1`, or the PL011 via `dtoverlay=disable-bt`) and
     remove the serial console from the kernel command line.
   - The library re-claims GPIO15 (E) as an output at matrix init. Confirm that it does after
     the UART is enabled.
4. **GPIO26 panel request** (only for JP601 1-2):
   - The daemon drives GPIO26 high after matrix init and low on shutdown.
   - A systemd stop hook or the crash reporter drives it low when the daemon dies.
5. No fault telemetry reaches the Pi. The eFuse fault lines go to LEDs, plus the halo FLT to the
   MCU.
