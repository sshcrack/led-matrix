# Power and thermal design

All numbers are design calculations or behavioral simulations. None are measurements. The
budget is reproduced by `python3 tools/power_budget.py` and the simulations by
`python3 tools/spice/run_spice.py` (results in [`tools/spice/results.txt`](../tools/spice/results.txt)).
Datasheet values quoted here were read from the manufacturers' PDFs: TI TPSM63610, TPS25947,
LM74700-Q1, SN74AHCT1G125; Mean Well GST160A; onsemi BSS138; JST VH and XH; Kycon KPJX;
Microchip ATtiny1614/16/17. Values not checked against a datasheet are marked *unverified*.

## Loads and supply choice

| Load | Budget | Basis |
|---|---|---|
| 4 × Waveshare P3 64×64 | 4 × 4.0 A at 5 V = 80 W | panel rating 5 V / 4 A, ≤ 20 W (full white, full brightness) |
| Raspberry Pi 3B + Electrodragon V2 | 2.5 A at 5 V = 12.5 W | Pi 3B recommended supply; typical draw 0.5–1 A |
| Halo, 40 WS2812B | 2.4 A at 5 V = 12 W | ~60 mA per LED at full white |
| Board logic | 0.06 A | MCU, buffer, LEDs |
| Board losses | 12.6 W | budget below |
| **Brick output, worst case** | **118 W at 24 V, 4.9 A** | 73 % of 160 W |

**Supply: Mean Well GST160A24-R7B**:

- 24 V, 6.67 A, 160 W external desktop brick. Class I: −V is tied to protective earth.
- 4-pin power DIN output (R7B).
- OVP at 105–135 % (≤ 32.4 V, hiccup). Overload hiccups at 105–150 %.
- Output ripple ≤ 150 mVp-p. Rise time ≤ 50 ms.
- Conducted and radiated emissions EN55032 Class B.
- Operating range −30…+70 °C. Full load is derated above about 40–50 °C, depending on input
  voltage. At 73 % load it has margin to about 50 °C ambient.
- Keep it outside the enclosure. The enclosure then holds only 24 V SELV, with no mains wiring.

Why 24 V: the worst case is 4.9 A through one Mini-Fit Jr connector and AWG18, compared with
more than 22 A for a 5 V brick. Generic barrel jacks are not used anywhere. The 4-pin DIN jack
is rated 7.5 A per pin and two pins carry each pole.

## Rails

| Rail | Source | Setting | Protection | Feeds |
|---|---|---|---|---|
| +24V | J1 → F1 → ideal diode Q1/U1 | — | 8 A very-fast fuse, reverse polarity to −60 V, SMBJ30A TVS, 2 × 100 µF bulk + 10 × 10 µF X7R | five bucks |
| Panel n VOUT (n = 1..4) | TPSM63610 U{n}01 | 5.02 V (FB 100k/24.9k), 1 MHz (RT 15.8k), spread spectrum + tone correction (RSPSP 29.4k) | module OCP and thermal shutdown | eFuse U{n}02 |
| Panel n VPANEL | TPS259470A U{n}02 | ILIM 5.0 A ±10 % (RILM 665 Ω), UV 4.31 V, OV 5.92 V, dVdt 2.2 nF, ITIMER 10 nF | current limit, UV/OV lockout, reverse blocking, thermal shutdown with 110 ms auto-retry | J{n}01 → panel n |
| +5V_AUX | TPSM63610 U501, EN from +24V divider (on 15.3 V, off 11.1 V) | 5.02 V, 1 MHz | as above | Pi and halo eFuses, MCU, buffer, LEDs, panel EN pull-up |
| VPI | TPS259470A U502 | ILIM 3.0 A (RILM 1.10k), UV 4.31 V, OV 5.92 V | as above | J501 → Electrodragon → Pi |
| VHALO | TPS259470A U602, EN from the MCU | ILIM 2.6 A (RILM 1.30k), OV 5.92 V | as above; branch off until enabled | J601 → halo |

Design notes:

- **Converter loading.** Each TPSM63610 is rated 8 A. The panel converters run at 4 A (50 %) and
  AUX at 5 A (62 %). TI's efficiency table gives 93 % at 24 V → 5 V, 4 A, 1 MHz, the operating
  point the datasheet characterizes. 1 MHz sits inside the AM broadcast band. Spread spectrum
  smears it, but it is not eliminated: conducted and radiated emissions are unmeasured.
- **Spread spectrum** (SPSP pin with RSPSP 29.4 kΩ to GND) is on. The datasheet range is
  10–30 kΩ. TI's equation 7 gives 29.5 kΩ for 24 V → 5 V, 1 MHz, 8 A, 2.2 µH. Ripple tone
  correction is therefore on as well. VCC is unloaded (internal 1 µF, no external connection,
  as the datasheet requires). MODE/SYNC = GND selects auto PFM/PWM.
- **Panel ILIM of 665 Ω** gives 5.0 A typical, with a minimum above the panel's 4 A rating.
  TI's UL 2367 recognition covers RILM ≥ 750 Ω only. 750 Ω would give 4.45 A typical but
  3.96 A minimum, below the rated panel current. This board is not submitted for UL 2367, so
  665 Ω is used. If a certified build is needed, use 750 Ω and confirm the panels' real peak
  current first.
- **EN_PANEL** is pulled up from +5V_AUX (47 kΩ, 100 nF). Panel converters cannot start before
  AUX exists. A BSS138 (Q602) holds them off unless PANEL_ON is high, through JP601 from Pi
  GPIO26 or the always-on shunt.

## Protection and how each requirement is covered

| Hazard | Hardware protection | Evidence |
|---|---|---|
| Reverse polarity at J1 | LM74700-Q1 + PSMN5R2-60YL in the + path: blocks to −60 V. The TVS sits after it, so it is never forward-biased by a reversed input | LM74700 datasheet; schematic |
| Input over-voltage / transients | SMBJ30A (VRWM 30 V, VBR ≥ 33.3 V *unverified*) after the ideal diode. Brick OVP tops out at 32.4 V, below TVS breakdown. TPSM63610 VIN max 36 V, 42 V transient | hot-plug sim: peak 23.4 V with bulk caps, 32.9 V without |
| Wiring short upstream of the eFuses | F1 Littelfuse 0453008.MR 8 A very fast, at 61 % load; the brick's hiccup OCP | budget |
| Panel/halo/Pi overload or short | TPS259470A current limit (5.0 / 2.6 / 3.0 A); fast trip at 2 × ILIM; FLT and LED after ITIMER (5–15 ms panel); a hard short heats the eFuse into thermal shutdown with 110 ms auto-retry | TPS25947 datasheet table 7-3 |
| Over-voltage on a 5 V branch (failed converter) | eFuse OVLO, trips at 5.75–6.12 V over tolerance corners | `thresholds.cir` |
| Brownout | AUX converter off below 10.7–11.1 V input. Panel and Pi eFuses shut off below 3.8–4.1 V and restart above 4.2–4.5 V. Panel EN follows +5V_AUX | `thresholds.cir`, `en_sequence.cir` |
| Inrush at power-on | brick soft start; dVdt 2.2 nF on each eFuse gives 0.9 V/ms; AUX enable at 15 V | below |
| USB/GPIO backfeed | Every 5 V output eFuse blocks reverse current (FLT LED lights). Pi logic lines only see 1 kΩ + high-Z inputs; the board never drives a Pi pin. Pi micro-USB must stay unplugged while J501 powers the Pi | wiring.md |
| ESD at harnesses | SMAJ6.0A + PESD5V0S1BA at J601 (the halo strip is the longest, most handled cable); 1 kΩ series resistors on Pi lines; SMBJ30A + bulk on 24 V. Panel and Pi harnesses are internal | layout |
| Software brightness limit missing or buggy | none needed: each panel branch is limited in hardware to 5.0 A ±10 % | — |
| Pi crash with panels lit (frozen scan, one row at full duty) | optional: JP601 1-2 turns panels off within 0.1 ms when GPIO26 drops. Needs the daemon change listed in verification.md | `en_sequence.cir` |

Hardware protection and software hardening are kept separate. Every row above works with no
software. Software items (GPIO26 control, halo firmware and driver) are listed under
[not implemented](verification.md#not-implemented-software). None of them is relied on for
safety.

## Inrush and hot-plug

- **Power-on with the brick switched on afterwards** (normal):
  - The brick ramps 24 V in ≤ 50 ms. 345 µF of input capacitance draws ≈ 0.2 A. AUX starts at
    15.3 V. Panel converters start after AUX (2.2 ms soft start each).
  - The panel eFuses then ramp VPANEL at 2000 / 2200 pF = 0.9 V/ms (TPS25947 eq. 4). That is
    5.5 ms to 5 V.
  - Inrush into C{n}21 220 µF plus the panel's own input capacitance (*unverified*, assumed
    ≤ 2000 µF) is ≤ 2 A, below ILIM.
- **Hot-plug into a running brick** (`hotplug.cir`, cable 2 µH / 83 mΩ, brick output cap
  1000 µF *assumed*):
  - Peak current is 83–103 A for about 50 µs, depending on bulk-cap ESR. I²t is 0.21–0.41 A²s.
    Overshoot is absent (peak 23.4 V) because the electrolytics damp the ceramics.
  - Without them the input would ring to 32.9 V.
  - The fuse's melting I²t has not been verified. Its datasheet was not reachable. Check that
    0453008's nominal melting I²t is ≥ 2 A²s (roughly 5× the pulse, for pulse derating), or
    tell users to plug the DC cable before switching mains on.
  - The DIN jack carries the arc, not J1.

## Thermal budget

Worst case: everything at full load at once, sustained.

| Item | Value |
|---|---|
| Panel buck loss, each | 1.51 W (93 %) |
| AUX buck loss | 2.02 W (92.5 %) |
| Panel eFuse loss, each | 0.64 W (4 A², 40 mΩ hot) |
| Pi / halo eFuse | 0.25 / 0.23 W |
| Q1 + fuse + J1 | 0.71 W at 4.9 A |
| 4 panel connectors | 0.77 W |
| **Board total** | **12.6 W** |

| Temperature (enclosure air 45 °C, still air) | Estimate | Limit |
|---|---|---|
| Board mean rise over air (12 W/m²K per face, 2 × 117 cm²) | +45 K | — |
| TPSM63610 panel module TJ | ≈ 100 °C | 125 °C operating, 168 °C shutdown |
| TPSM63610 AUX module TJ | ≈ 111 °C | 125 °C |
| TPS259470A panel eFuse TJ | ≈ 115 °C | 125 °C, internal TSD |

How the estimates were made:

- Each junction estimate adds half the board rise and P × RθJA:
  - TPSM63610 RθJA is 21.6 °C/W on TI's EVM.
  - TPS25947 RθJA is 74.5 °C/W on JEDEC 2s2p without thermal vias. This board has no vias
    under the eFuses: the 2 × 2 mm HotRod package has no thermal pad and sheds heat through
    its IN/OUT pins into the 2 oz pours. TI's figure with eight vias is 41.7 °C/W.
  - The eFuse has the least margin, about 10 K at the absolute worst case. Measure it at
    bring-up. A rev B improvement is B.Cu VOUT/VPANEL islands under each eFuse with vias.
- These estimates are deliberately pessimistic. Real content averages well under full white,
  and at 50 % panel load the board loss is about 6 W.
- The worst case still stays inside the operating limits. If it is exceeded, for example
  because the enclosure air goes above 45 °C, the parts protect themselves (converter and eFuse
  thermal shutdown with auto-retry). That shows up as panel blackouts, not damage.

## Enclosure (ventilated PETG)

Heat inside the enclosure:

- Board: 12.6 W.
- Pi: about 4 W.
- Up to about half of the panels' 80 W, depending on how much leaves through the front face.
- Total: 17–57 W.

PETG softens near its glass transition (about 80 °C). Keep the enclosure air at or below 45 °C
in a 30 °C room. That requires:

- Free vent area of ≥ 100 cm² at the bottom and ≥ 100 cm² at the top of the back. Natural draft
  over 0.30 m at a 15 K rise moves about 3.1 L/s.
- Board mounted vertically, or with the converter side facing the airflow. Keep ≥ 10 mm of air
  above the components and ≥ 6 mm below the board (THT leads).
- Converter modules and C2/C3 not touching PETG. Keep ≥ 5 mm.
- Pi and board not stacked on each other.
- The brick outside the enclosure.

A fan is not required for the realistic content mix. If one is added later, it needs a 5 V
source with its own protection: the AUX budget has about 3 A of headroom against the module
rating, but the eFuse branches do not.
