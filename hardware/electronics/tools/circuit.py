"""Circuit definition of the power-distribution + halo-controller board.

Each sheet is a list of groups; each group is a list of parts.  A part maps
symbol pin numbers to net names.  ``None`` marks an intentionally unconnected
pin (a no-connect flag is drawn).  Net names listed in GLOBAL_NETS cross
sheets (global labels); every other name is local to its sheet (KiCad prefixes
it with the sheet path, so the four panel-channel instances stay separate).

Design values and their derivations are documented in
``hardware/electronics/docs/power-and-thermal.md``.
"""

from __future__ import annotations

from dataclasses import dataclass, field

GLOBAL_NETS = {"+24V", "+5V_AUX", "EN_PANEL", "GND"}

R0603 = "Resistor_SMD:R_0603_1608Metric"
C0603 = "Capacitor_SMD:C_0603_1608Metric"
C1210 = "Capacitor_SMD:C_1210_3225Metric"
LED0603 = "LED_SMD:LED_0603_1608Metric"

DS_TPSM = "https://www.ti.com/lit/ds/symlink/tpsm63610.pdf"
DS_EFUSE = "https://www.ti.com/lit/ds/symlink/tps25947.pdf"
DS_YAGEO = "https://www.yageo.com/upload/media/product/productsearch/datasheet/rchip/PYu-RC_Group_51_RoHS_L_12.pdf"
DS_MURATA = "https://www.murata.com/en-global/products/capacitor/ceramiccapacitor"


@dataclass
class Part:
    ref: str                  # e.g. "R3" or "R{n}03" in the repeated panel sheet
    lib: str                  # "Lib:Symbol"
    value: str
    footprint: str
    pins: dict
    mpn: str = ""
    mfr: str = ""
    datasheet: str = ""
    desc: str = ""
    extra: dict = field(default_factory=dict)
    in_bom: bool = True
    on_board: bool = True


def yageo_code(value: str) -> str:
    v = value.replace("Ω", "").replace("R", "")
    if v.endswith("k"):
        num = v[:-1]
        code = num.replace(".", "K") if "." in num else num + "K"
    else:
        num = v
        code = num.replace(".", "R") if "." in num else num + "R"
    return f"RC0603FR-07{code}L"


def R(ref, value, a, b, desc="", power=None):
    extra = {"Tolerance": "1%", "Rating": power or "0.1 W, 75 V"}
    return Part(ref, "Device:R", value, R0603, {"1": a, "2": b}, yageo_code(value), "Yageo",
                DS_YAGEO, desc or f"Resistor {value} 1% 0603", extra)


MLCC = {
    # value: (mpn, rating, footprint)
    "10uF/50V": ("GRM32ER71H106KA12L", "50 V X7R", C1210),
    "47uF/10V": ("GRM32ER71A476ME15L", "10 V X7R", C1210),
    "1uF/16V": ("GRM188R71C105KA12D", "16 V X7R", C0603),
    "100nF/50V": ("GRM188R71H104KA93D", "50 V X7R", C0603),
    "22pF/50V": ("GRM1885C1H220JA01D", "50 V C0G", C0603),
    "10nF/50V": ("GRM188R71H103KA01D", "50 V X7R", C0603),
    "4.7nF/50V": ("GRM188R71H472KA01D", "50 V X7R", C0603),
    "2.2nF/50V": ("GRM188R71H222KA01D", "50 V X7R", C0603),
}


def C(ref, value, a, b="GND", desc=""):
    mpn, rating, fp = MLCC[value]
    return Part(ref, "Device:C", value.split("/")[0], fp, {"1": a, "2": b}, mpn, "Murata",
                DS_MURATA, desc or f"MLCC {value}", {"Rating": rating})


def CP(ref, value, a, desc, mpn, size):
    fp = {"8x10.2": "Capacitor_SMD:CP_Elec_8x10", "8x6.2": "Capacitor_SMD:CP_Elec_8x6.2"}[size]
    return Part(ref, "Device:C_Polarized", value, fp, {"1": a, "2": "GND"}, mpn, "Panasonic",
                "https://industrial.panasonic.com/ww/products/capacitors/polymer-capacitors-aluminum-electrolytic-capacitors",
                desc, {"Rating": value.split("/")[-1] if "/" in value else "", "Case": f"{size} mm",
                       "Verify": "case size and ripple rating against the current Panasonic FK data sheet"})


def LED(ref, colour, anode, cathode, desc):
    mpn = {"red": "APT1608EC", "green": "APT1608SGC"}[colour]
    return Part(ref, "Device:LED", colour, LED0603, {"1": cathode, "2": anode}, mpn, "Kingbright",
                "https://www.kingbrightusa.com/images/catalog/SPEC/APT1608EC.pdf" if colour == "red"
                else "https://www.kingbrightusa.com/images/catalog/SPEC/APT1608SGC.pdf",
                desc, {"Rating": "20 mA max, run at ~2 mA"})


def TP(ref, net, desc):
    return Part(ref, "Connector:TestPoint", "TP", "TestPoint:TestPoint_Pad_D1.5mm", {"1": net},
                desc=desc, in_bom=False)


# --------------------------------------------------------------------------
# Building blocks
# --------------------------------------------------------------------------

def converter(p, vout, en, desc):
    """TPSM63610 24 V -> 5.0 V stage.  ``p`` formats a reference suffix."""
    u = Part(p("U", 1), "ledpanel:TPSM63610", "TPSM63610RDFR",
             "ledpanel:TI_RDF0022A_B3QFN-22_6.5x7.5mm",
             {"1": "+24V", "18": "+24V", "17": en, "15": "GND", "14": "SPSP", "12": "RT",
              "13": None, "16": None, "9": vout, "10": vout, "5": vout, "8": "FB", "6": None,
              "2": "RBOOT", "3": "CBOOT", "4": None,
              "19": "GND", "20": "GND", "7": "GND", "11": "GND", "21": "GND", "22": "GND"},
             "TPSM63610RDFR", "Texas Instruments", DS_TPSM, desc,
             {"Rating": "VIN 3-36 V (42 V transient), 8 A; used at <=4.5 A",
              "Config": "1 MHz (RT 15.8k), auto PFM/PWM (MODE=GND), spread spectrum + tone correction (RSPSP 29.4k); VCC pin internal 1uF, no connection"})
    return [
        u,
        C(p("C", 1), "10uF/50V", "+24V", desc="CIN at VIN1 (TI: 2x10uF X7R 50V 1210 min)"),
        C(p("C", 2), "10uF/50V", "+24V", desc="CIN at VIN2"),
        C(p("C", 3), "47uF/10V", vout, desc="COUT at VOUT1"),
        C(p("C", 4), "47uF/10V", vout, desc="COUT at VOUT1"),
        C(p("C", 5), "47uF/10V", vout, desc="COUT at VOUT2"),
        C(p("C", 6), "47uF/10V", vout, desc="COUT at VOUT2"),
        C(p("C", 7), "1uF/16V", vout, desc="VLDOIN bypass"),
        R(p("R", 1), "100k", vout, "FB", desc="RFBT, VOUT = 1.0 V x (1 + 100k/24.9k) = 5.02 V"),
        R(p("R", 2), "24.9k", "FB", "GND", desc="RFBB"),
        C(p("C", 8), "22pF/50V", vout, "FB", desc="CFF feed-forward"),
        R(p("R", 3), "15.8k", "RT", "GND", desc="RRT, FSW = 1 MHz"),
        R(p("R", 4), "100", "RBOOT", "CBOOT", desc="RBOOT 100 ohm: SW slew-rate / EMI balance (TI)"),
        R(p("R", 7), "29.4k", "SPSP", "GND",
          desc="RSPSP: spread spectrum on with ripple tone correction (TI eq. 7: 29.5k at 24->5 V, 1 MHz)"),
    ]


def efuse(p, vin, vout, ilm, ilm_desc, itimer, uv="string", en_net=None, flt_net="FLT_N", flt_pullup=False):
    """TPS259470A branch protection.  uv='string' uses a 3-resistor UV/OV string."""
    parts = []
    pins = {"5": vin, "6": vout, "8": "GND", "9": "ILM", "10": "ITIMER", "7": "DVDT",
            "3": None, "4": flt_net}
    if uv == "string":
        pins.update({"1": "EN_UV", "2": "OVLO"})
        parts += [R(p("R", 11), "143k", vin, "EN_UV", desc="UV/OV string top: UV rise 4.31 V"),
                  R(p("R", 12), "15.0k", "EN_UV", "OVLO", desc="UV/OV string middle"),
                  R(p("R", 13), "40.2k", "OVLO", "GND", desc="UV/OV string bottom: OV rise 5.92 V")]
    else:
        pins.update({"1": en_net, "2": "OVLO"})
        parts += [R(p("R", 11), "147k", vin, "OVLO", desc="OVLO divider top: OV rise 5.92 V"),
                  R(p("R", 13), "37.4k", "OVLO", "GND", desc="OVLO divider bottom"),
                  R(p("R", 12), "100k", en_net, "GND", desc="EN pull-down: branch off unless the MCU enables it")]
    u = Part(p("U", 2), "ledpanel:TPS259470A", "TPS259470ARPWR",
             "ledpanel:TI_RPW0010A_VQFN-HR-10_2x2mm", pins, "TPS259470ARPWR", "Texas Instruments",
             DS_EFUSE, "eFuse: current limit, OVLO, UVLO, reverse blocking, auto-retry",
             {"Rating": "2.7-23 V (28 V abs), 5.5 A, 28 mOhm", "ILIM": ilm_desc})
    parts.insert(0, u)
    parts += [
        R(p("R", 14), ilm, "ILM", "GND", desc=f"RILM: ILIM = 3334/RILM = {ilm_desc}"),
        C(p("C", 11), itimer, "ITIMER", desc="ITIMER: overcurrent blanking"),
        C(p("C", 12), "2.2nF/50V", "DVDT", desc="dVdt: ~0.9 V/ms output ramp"),
        LED(p("D", 1), "red", "LEDA", flt_net, "Branch fault (FLT low)"),
        R(p("R", 15), "1.5k", "+5V_AUX", "LEDA", desc="Fault LED current ~2 mA"),
    ]
    if flt_pullup:
        parts.append(R(p("R", 16), "10k", flt_net, "+5V_AUX", desc="FLT pull-up for MCU input"))
    return parts


# --------------------------------------------------------------------------
# Sheets
# --------------------------------------------------------------------------

def sheet_input():
    def p(k, i):
        return f"{k}{i}" if i is not None else k
    g_in = [
        Part("J1", "Connector_Generic:Conn_02x02_Odd_Even", "24V IN",
             "Connector_Molex:Molex_Mini-Fit_Jr_5569-04A2_2x02_P4.20mm_Horizontal",
             {"1": "VIN_RAW", "2": "VIN_RAW", "3": "GND", "4": "GND"}, "39-30-0040", "Molex",
             "https://www.molex.com/en-us/products/part-detail/39300040",
             "24 V input, Mini-Fit Jr 2x2 right-angle header; pins 1+2 = +24V, 3+4 = 0V",
             {"Rating": "Mini-Fit Jr: up to 9 A/circuit with 16-18 AWG (verify for 4-circuit, 2 contacts paralleled)",
              "Mate": "39-01-2040 receptacle + 39-00-0039 crimp, AWG18"}),
        Part("F1", "Device:Fuse", "8A", "Fuse:Fuse_Littelfuse-NANO2-451_453",
             {"1": "VIN_RAW", "2": "VIN_F"}, "0453008.MR", "Littelfuse",
             "https://www.littelfuse.com/products/fuses/surface-mount-fuses/nano2-fuses/453",
             "Input fuse, 8 A very fast-acting, NANO2 2410 (wiring-fault backup; PSU limits normal faults)",
             {"Rating": "8 A, very fast; confirm voltage rating >= 32 V on current data sheet"}),
        Part("U1", "Power_Management:LM74700", "LM74700QDBVRQ1", "Package_TO_SOT_SMD:SOT-23-6",
             {"6": "VIN_F", "4": "+24V", "5": "GATE", "1": "VCAP", "3": "VIN_F", "2": "GND"},
             "LM74700QDBVRQ1", "Texas Instruments", "https://www.ti.com/lit/ds/symlink/lm74700-q1.pdf",
             "Ideal-diode controller: reverse-polarity / reverse-current protection",
             {"Rating": "3.2-65 V, -65 V reverse"}),
        Part("Q1", "Transistor_FET:PSMN5R2-60YL", "PSMN5R2-60YL", "Package_TO_SOT_SMD:LFPAK56",
             {"4": "GATE", "1": "VIN_F", "2": "VIN_F", "3": "VIN_F", "5": "+24V"},
             "PSMN5R2-60YL", "Nexperia", "https://assets.nexperia.com/documents/data-sheet/PSMN5R2-60YL.pdf",
             "Ideal-diode N-MOSFET (source = input, drain = +24V)", {"Rating": "60 V, 5.2 mOhm"}),
        C("C1", "100nF/50V", "VCAP", "VIN_F", desc="LM74700 charge-pump capacitor VCAP-ANODE"),
        Part("D1", "ledpanel:SMBJ30A", "SMBJ30A", "Diode_SMD:D_SMB", {"1": "+24V", "2": "GND"},
             "SMBJ30A", "Littelfuse", "https://www.littelfuse.com/products/tvs-diodes/surface-mount/smbj",
             "Input TVS after the ideal diode (protects 42 V module transient rating)",
             {"Rating": "VRWM 30 V, VBR 33.3 V min, 600 W 10/1000 us"}),
        CP("C2", "100uF/50V", "+24V", "Input bulk / hot-plug damping", "EEE-FK1H101P", "8x10.2"),
        CP("C3", "100uF/50V", "+24V", "Input bulk / hot-plug damping", "EEE-FK1H101P", "8x10.2"),
        R("R1", "22k", "+24V", "LED24", desc="24 V present LED, ~1 mA"),
        LED("D2", "green", "LED24", "GND", "+24V present"),
        TP("TP1", "+24V", "Test point +24V"),
        TP("TP2", "GND", "Test point GND"),
    ]
    return [("24 V input: connector, fuse, ideal-diode reverse protection, TVS, bulk", g_in)]


def sheet_panel():
    """Repeated sheet: one per panel.  Refs use {n} = panel number."""
    def p(k, i):
        return f"{k}{{n}}{i:02d}" if i is not None else k
    conv = converter(p, "VOUT", "EN_PANEL", "Panel buck 24 V -> 5.0 V, 4 A load (50% of rating)")
    fuse = efuse(p, "VOUT", "VPANEL", "665", "5.0 A (+-10%)", "10nF/50V")
    out = [
        CP(p("C", 21), "220uF/10V", "VPANEL", "Panel branch output buffer", "EEE-FK1A221P", "8x6.2"),
        Part(p("J", 1), "Connector_Generic:Conn_01x04", "PANEL",
             "Connector_JST:JST_VH_S4P-VH_1x04_P3.96mm_Horizontal",
             {"1": "VPANEL", "2": "VPANEL", "3": "GND", "4": "GND"}, "S4P-VH", "JST",
             "https://www.jst-mfg.com/product/pdf/eng/eVH.pdf",
             "Panel power out: 1+2 = +5V, 3+4 = GND, to Waveshare VH4 input",
             {"Rating": "10 A/contact (AWG16), 7 A (AWG18); 2 contacts per pole", "Wire": "AWG18, <= 0.5 m"}),
        TP(p("TP", 1), "VPANEL", "Test point panel 5 V"),
    ]
    return [("Panel buck converter (TPSM63610): 24 V -> 5.0 V", conv),
            ("Panel branch eFuse (TPS259470A): 5.0 A limit, UV 4.31 V, OV 5.92 V, reverse blocking", fuse),
            ("Panel power connector", out)]


def sheet_aux():
    def p(k, i):
        return f"{k}5{i:02d}" if i is not None else k
    conv = converter(p, "+5V_AUX", "EN_AUX", "AUX buck 24 V -> 5.0 V for Pi, halo and logic")
    conv += [R(p("R", 5), "200k", "+24V", "EN_AUX", desc="AUX EN/UVLO top: on at ~15.3 V, off ~11 V"),
             R(p("R", 6), "18k", "EN_AUX", "GND", desc="AUX EN/UVLO bottom")]
    pi = efuse(p, "+5V_AUX", "VPI", "1.10k", "3.03 A (+-10%)", "4.7nF/50V")
    pi += [
        CP(p("C", 21), "220uF/10V", "VPI", "Pi branch output buffer", "EEE-FK1A221P", "8x6.2"),
        Part(p("J", 1), "Connector_Generic:Conn_01x02", "PI 5V",
             "Connector_JST:JST_VH_S2P-VH_1x02_P3.96mm_Horizontal",
             {"1": "VPI", "2": "GND"}, "S2P-VH", "JST", "https://www.jst-mfg.com/product/pdf/eng/eVH.pdf",
             "Pi power out: 1 = +5V, 2 = GND, to Electrodragon V2 5 V input (XH2.54)",
             {"Rating": "10 A/contact", "Wire": "AWG20-22 pair, <= 0.3 m"}),
        TP(p("TP", 1), "VPI", "Test point Pi 5 V"),
        TP(p("TP", 2), "+5V_AUX", "Test point +5V_AUX"),
        R(p("R", 31), "1.5k", "+5V_AUX", "LEDAUX", desc="AUX present LED ~2 mA"),
        LED(p("D", 31), "green", "LEDAUX", "GND", "+5V_AUX present"),
    ]
    return [("AUX buck converter (TPSM63610): 24 V -> 5.0 V, enables at ~15 V input", conv),
            ("Raspberry Pi branch eFuse (TPS259470A): 3.0 A limit, reverse blocking (no USB/GPIO backfeed)", pi)]


def sheet_halo():
    def p(k, i):
        return f"{k}6{i:02d}" if i is not None else k
    mcu_pins = {"1": "+5V_AUX", "20": "GND", "16": "UPDI", "2": "HALO_DATA", "3": "HALO_EN",
                "4": "HALO_FLT_N", "5": "LED_STAT", "8": "UART_RX"}
    for n in ["17", "18", "19", "11", "10", "9", "7", "6", "12", "13", "14", "15"]:
        mcu_pins[n] = None
    mcu = [
        Part(p("U", 1), "MCU_Microchip_ATtiny:ATtiny1616-S", "ATtiny1616-SNR",
             "Package_SO:SOIC-20W_7.5x12.8mm_P1.27mm", mcu_pins, "ATtiny1616-SNR", "Microchip",
             "https://ww1.microchip.com/downloads/en/DeviceDoc/ATtiny1614-16-17-DataSheet-DS40002204A.pdf",
             "Halo controller: WS2812 timing, halo enable/fault, UART command input from the Pi",
             {"Rating": "1.8-5.5 V, 20 MHz at 4.5-5.5 V"}),
        C(p("C", 1), "100nF/50V", "+5V_AUX", desc="MCU decoupling"),
        C(p("C", 2), "1uF/16V", "+5V_AUX", desc="MCU bulk decoupling"),
        Part(p("J", 2), "Connector_Generic:Conn_01x03", "UPDI", "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical",
             {"1": "UPDI", "2": "+5V_AUX", "3": "GND"}, "61300311121", "Wurth Elektronik",
             "https://www.we-online.com/components/products/datasheet/61300311121.pdf",
             "UPDI programming header: 1 UPDI, 2 +5V, 3 GND"),
        R(p("R", 2), "1k", "LED_STAT", "LEDS", desc="Status LED ~2.8 mA"),
        LED(p("D", 2), "green", "LEDS", "GND", "Halo controller status"),
    ]
    halo = efuse(p, "+5V_AUX", "VHALO", "1.30k", "2.56 A (+-10%)", "4.7nF/50V", uv="en",
                 en_net="HALO_EN_F", flt_net="HALO_FLT_N", flt_pullup=True)
    halo += [
        R(p("R", 21), "1k", "HALO_EN", "HALO_EN_F", desc="MCU -> eFuse EN"),
        R(p("R", 22), "330", "HALO_DATA", "HALO_DOUT", desc="WS2812 data series resistor"),
        Part(p("D", 21), "ledpanel:SMAJ6.0A", "SMAJ6.0A", "Diode_SMD:D_SMA", {"1": "VHALO", "2": "GND"},
             "SMAJ6.0A", "Littelfuse", "https://www.littelfuse.com/products/tvs-diodes/surface-mount/smaj",
             "Halo 5 V output TVS (cable ESD / hot-plug)", {"Rating": "VRWM 6.0 V, 400 W"}),
        Part(p("D", 22), "Device:D_TVS", "PESD5V0S1BA", "Diode_SMD:D_SOD-323", {"1": "HALO_DOUT", "2": "GND"},
             "PESD5V0S1BA", "Nexperia", "https://assets.nexperia.com/documents/data-sheet/PESD5V0S1BA.pdf",
             "Halo data line ESD (bidirectional, 5 V)", {"Rating": "VRWM 5 V"}),
        CP(p("C", 21), "220uF/10V", "VHALO", "Halo branch output buffer", "EEE-FK1A221P", "8x6.2"),
        Part(p("J", 1), "Connector_Generic:Conn_01x03", "HALO",
             "Connector_JST:JST_VH_S3P-VH_1x03_P3.96mm_Horizontal",
             {"1": "VHALO", "2": "HALO_DOUT", "3": "GND"}, "S3P-VH", "JST",
             "https://www.jst-mfg.com/product/pdf/eng/eVH.pdf",
             "Halo out: 1 = +5V (<= 2.4 A), 2 = WS2812 DIN, 3 = GND",
             {"Rating": "10 A/contact", "Wire": "AWG20 x3"}),
        TP(p("TP", 1), "VHALO", "Test point halo 5 V"),
    ]
    logic = [
        Part(p("J", 3), "Connector_Generic:Conn_01x03", "PI LOGIC",
             "Connector_JST:JST_XH_S3B-XH-A_1x03_P2.50mm_Horizontal",
             {"1": "GND", "2": "PI_TXD", "3": "PI_PANEL_REQ"}, "S3B-XH-A", "JST",
             "https://www.jst-mfg.com/product/pdf/eng/eXH.pdf",
             "Pi logic: 1 GND, 2 GPIO14 TXD, 3 GPIO26 panel request (Electrodragon extra-pins header)",
             {"Rating": "3 A/contact", "Wire": "AWG26-28, <= 0.3 m"}),
        R(p("R", 31), "1k", "PI_TXD", "TXD_R", desc="Series limit into buffer (protects Pi pin)"),
        R(p("R", 32), "100k", "TXD_R", "GND", desc="Defined level with Pi absent"),
        Part(p("U", 3), "74xGxx:74AHCT1G125", "SN74AHCT1G125DBVR", "Package_TO_SOT_SMD:SOT-23-5",
             {"1": "GND", "2": "TXD_R", "3": "GND", "4": "UART_RX", "5": "+5V_AUX"},
             "SN74AHCT1G125DBVR", "Texas Instruments", "https://www.ti.com/lit/ds/symlink/sn74ahct1g125.pdf",
             "3.3 V -> 5 V UART level shift (VIH 2.0 V TTL input)", {"Rating": "4.5-5.5 V"}),
        C(p("C", 31), "100nF/50V", "+5V_AUX", desc="Buffer decoupling"),
        R(p("R", 33), "1k", "PI_PANEL_REQ", "PANEL_REQ_R", desc="Series limit from GPIO26"),
        Part(p("JP", 1), "Jumper:Jumper_3_Open", "PANEL MODE",
             "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical",
             {"1": "PANEL_REQ_R", "2": "PANEL_ON", "3": "FORCE_ON"}, "61300311121", "Wurth Elektronik",
             "https://www.we-online.com/components/products/datasheet/61300311121.pdf",
             "Panel enable source: shunt 1-2 = Pi GPIO26 controls panels, 2-3 = panels always on (bring-up default)",
             {"Shunt": "2.54 mm jumper shunt, fitted 2-3 at assembly"}),
        R(p("R", 34), "100k", "PANEL_ON", "GND", desc="Panels off when the Pi does not drive GPIO26"),
        R(p("R", 35), "10k", "FORCE_ON", "+5V_AUX", desc="Always-on source for JP1 2-3"),
        Part(p("Q", 1), "Transistor_FET:BSS138", "BSS138", "Package_TO_SOT_SMD:SOT-23",
             {"1": "PANEL_ON", "2": "GND", "3": "PANEL_KILL"}, "BSS138", "onsemi",
             "https://www.onsemi.com/pdf/datasheet/bss138-d.pdf", "Inverter: PANEL_ON high -> PANEL_KILL low",
             {"Rating": "50 V, 0.22 A, Vgs(th) <= 1.5 V"}),
        R(p("R", 36), "100k", "+5V_AUX", "PANEL_KILL", desc="PANEL_KILL pull-up"),
        Part(p("Q", 2), "Transistor_FET:BSS138", "BSS138", "Package_TO_SOT_SMD:SOT-23",
             {"1": "PANEL_KILL", "2": "GND", "3": "EN_PANEL"}, "BSS138", "onsemi",
             "https://www.onsemi.com/pdf/datasheet/bss138-d.pdf", "Pulls all panel converter EN pins low",
             {"Rating": "50 V, 0.22 A"}),
        R(p("R", 37), "47k", "+5V_AUX", "EN_PANEL", desc="EN_PANEL pull-up: panels need +5V_AUX first"),
        C(p("C", 32), "100nF/50V", "EN_PANEL", desc="EN_PANEL ~5 ms soft delay"),
    ]
    return [("Halo controller (ATtiny1616)", mcu),
            ("Halo branch eFuse (TPS259470A): 2.6 A limit, MCU-enabled, OV 5.92 V", halo),
            ("Raspberry Pi logic interface: UART RX (GPIO14) and panel enable (GPIO26)", logic)]


def sheet_root_parts():
    parts = []
    for i in range(1, 5):
        parts.append(Part(f"H{i}", "Mechanical:MountingHole", "M3", "MountingHole:MountingHole_3.2mm_M3", {},
                          desc="M3 mounting hole, isolated (no copper)", in_bom=False))
    for i in range(1, 4):
        parts.append(Part(f"FID{i}", "Mechanical:Fiducial", "Fiducial", "Fiducial:Fiducial_1mm_Mask2mm", {},
                          desc="Assembly fiducial", in_bom=False))
    return [("Mechanical", parts)]


SHEETS = [
    # (sheet name, file, builder, instances[list of {n}], page)
    ("Input", "input.kicad_sch", sheet_input, [None]),
    ("Panel", "panel_channel.kicad_sch", sheet_panel, [1, 2, 3, 4]),
    ("Aux", "aux_pi.kicad_sch", sheet_aux, [None]),
    ("Halo", "halo_control.kicad_sch", sheet_halo, [None]),
]
