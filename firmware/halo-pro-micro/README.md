# Halo firmware (Pro Micro)

Drives the rear WS2812B halo strip from colour frames the Raspberry Pi daemon sends over a
one-way UART. Wiring: `hardware/wiring/README.md` ("Halo"). Protocol and Pi setup:
`docs/HALO.md`.

- Board: Pro Micro, ATmega32U4, 5 V / 16 MHz (PlatformIO board `sparkfun_promicro16`).
- `Serial1` (RXI, pin 0) receives, pin 6 drives the strip through 330 Ω. TX is not connected.
- Library: Adafruit NeoPixel 1.12.3, platform `atmelavr` 5.0.0 (both pinned in `platformio.ini`).
- The frame parser is `shared/matrix/include/shared/matrix/halo/protocol.h`, the same header the
  daemon encodes with.

## Build and flash

```bash
pip install platformio        # once
cd firmware/halo-pro-micro
pio run                       # build (RAM ~1.6 kB of 2.5 kB, flash ~8.8 kB)
pio run -t upload             # flash over USB
```

**Unplug the RAW 5 V wire from the Pro Micro before connecting USB**, so the USB port and the
halo supply never feed each other. Reconnect RAW after unplugging USB. If the upload cannot find
the port, double-tap the reset pad (bootloader window is about 8 s) and run the upload again.

Build flags (in `platformio.ini`): `HALO_LED_PIN`, `HALO_DEFAULT_LEDS` (before the first frame),
`HALO_MAX_LEDS` (buffer size, 120), `HALO_MAX_MILLIAMPS` (hard cap, 2500),
`HALO_MA_PER_LED` (60), `HALO_FAILSAFE_MS` (3000). The LED count in use comes from each frame
(the daemon setting `led_count`), up to `HALO_MAX_LEDS`.

## Behaviour

- Dark until the first valid frame. Boot noise, partial frames and bad checksums are ignored.
- Colours and brightness move towards the latest frame at 50 Hz (about 160 ms time constant), so
  the Pi's ~15 frames/s look soft.
- Estimated current (60 mA per LED at full white) is capped at 2.5 A by scaling all channels, no
  matter what the Pi sends.
- No valid frame for 3 s (daemon stopped or crashed): fades to off.
- `show()` (interrupts off, ~1.2 ms for 40 LEDs) only runs when no frame bytes arrived in the last
  4 ms; the Pi sends at most every 66 ms and a frame takes about 3 ms on the wire.
