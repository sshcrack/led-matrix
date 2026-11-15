// Rear halo light: WS2812B strip on pin 6, colour frames from the Pi on
// Serial1 (RXI, 115200 8N1). See README.md and the protocol header.
//
// - Only complete frames with a valid CRC-8 change anything; boot noise and
//   half frames are dropped by the parser.
// - show() disables interrupts (~30 us per LED, ~1.2 ms for 40) and UART bytes
//   arriving meanwhile are lost. So the strip is updated right after a valid
//   frame, in the quiet gap before the next one (the Pi sends one frame every
//   ~66 ms, a frame takes ~3 ms). Free-running ticks only happen once the line
//   has been silent for kSilentMs (the failsafe fade).
// - The strip object is sized to the LED count the Pi sends, so show() never
//   clocks out unused pixels.
// - The total current estimate is capped regardless of what the Pi sends.
// - No valid frame for HALO_FAILSAFE_MS: fade to off.
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

#include "shared/matrix/halo/protocol.h"

#ifndef HALO_LED_PIN
#define HALO_LED_PIN 6
#endif
#ifndef HALO_DEFAULT_LEDS
#define HALO_DEFAULT_LEDS 40
#endif
#ifndef HALO_MAX_LEDS
#define HALO_MAX_LEDS 120
#endif
#ifndef HALO_MAX_MILLIAMPS
#define HALO_MAX_MILLIAMPS 2500UL
#endif
#ifndef HALO_MA_PER_LED
#define HALO_MA_PER_LED 60UL  // one WS2812B at full white
#endif
#ifndef HALO_FAILSAFE_MS
#define HALO_FAILSAFE_MS 3000UL
#endif

static const unsigned long kSilentTickMs = 20;   // free-running ticks while no frames arrive
static const unsigned long kSilentMs = 150;      // line silent this long: frames have stopped
static const unsigned long kRefreshMs = 500;     // re-send an unchanged picture now and then
static const uint8_t kSmoothShift = 2;           // 1/4 of the distance per step (~0.25 s at 15 frames/s)

static Adafruit_NeoPixel strip(HALO_DEFAULT_LEDS, HALO_LED_PIN, NEO_GRB + NEO_KHZ800);
static Halo::Parser parser;

// Key colours and brightness, in 8.8 fixed point: current values chase targets.
static int32_t cur_rgb[3 * Halo::kMaxZones];
static int32_t tgt_rgb[3 * Halo::kMaxZones];
static int32_t cur_bright = 0, tgt_bright = 0;
static uint8_t zones = 1;
static uint8_t led_count = HALO_DEFAULT_LEDS;

static unsigned long last_valid_ms = 0, last_byte_ms = 0, last_tick_ms = 0, last_show_ms = 0;
static bool have_frame = false;

static int32_t chase(int32_t cur, int32_t target)
{
    const int32_t diff = target - cur;
    int32_t step = diff / (1 << kSmoothShift);
    if (step == 0 && diff != 0)
        step = diff > 0 ? 1 : -1;
    return cur + step;
}

// Stay below the supply budget whatever the frame says: scale everything down
// when the estimated current (60 mA per LED at full white) would exceed it.
static void apply_current_cap(uint8_t *rgb, uint16_t n)
{
    uint32_t sum = 0;
    for (uint16_t i = 0; i < 3u * n; ++i)
        sum += rgb[i];
    // mA = sum / (3 * 255) * 60
    const uint32_t ma = sum * HALO_MA_PER_LED / (3UL * 255UL);
    if (ma <= HALO_MAX_MILLIAMPS)
        return;
    for (uint16_t i = 0; i < 3u * n; ++i)
        rgb[i] = (uint8_t)(((uint32_t)rgb[i] * HALO_MAX_MILLIAMPS) / ma);
}

static void render(bool force)
{
    static uint8_t out[3 * HALO_MAX_LEDS];
    static uint8_t prev[3 * HALO_MAX_LEDS];
    const uint8_t b = (uint8_t)(cur_bright >> 8);
    const uint8_t n = led_count;

    for (uint8_t i = 0; i < n; ++i) {
        // position around the loop in zone units, 8.8 fixed point; the last
        // zone blends back into the first
        const uint32_t pos = ((uint32_t)i * zones * 256UL) / n;
        const uint8_t z0 = (uint8_t)(pos >> 8) % zones;
        const uint8_t z1 = (uint8_t)(z0 + 1) % zones;
        const uint16_t f = pos & 0xFF;
        for (uint8_t c = 0; c < 3; ++c) {
            const int32_t a = cur_rgb[3 * z0 + c], d = cur_rgb[3 * z1 + c];
            const int32_t v = (a * (256 - f) + d * f) >> 16;  // 8.8 * 8.8 -> 8 bits
            const uint32_t scaled = ((uint32_t)(v < 0 ? 0 : v > 255 ? 255 : v) * b) / 255UL;
            out[3 * i + c] = (uint8_t)scaled;
        }
    }
    apply_current_cap(out, n);

    if (strip.numPixels() != n) {
        // Shorter strip: blank the LEDs that drop out once, then resize.
        strip.clear();
        strip.show();
        strip.updateLength(n);
        force = true;
    }
    const unsigned long now = millis();
    if (!force && memcmp(out, prev, 3u * n) == 0 && now - last_show_ms < kRefreshMs)
        return;
    memcpy(prev, out, 3u * n);
    for (uint8_t i = 0; i < n; ++i)
        strip.setPixelColor(i, out[3 * i], out[3 * i + 1], out[3 * i + 2]);
    strip.show();
    last_show_ms = millis();
}

// One smoothing step toward the targets, then output.
static void step()
{
    for (uint8_t i = 0; i < 3 * zones; ++i)
        cur_rgb[i] = chase(cur_rgb[i], tgt_rgb[i]);
    cur_bright = chase(cur_bright, tgt_bright);
    render(false);
}

void setup()
{
    strip.begin();
    strip.clear();
    strip.show();
    Serial1.begin(Halo::kBaud);
    last_valid_ms = millis();
}

void loop()
{
    bool got_frame = false;
    while (Serial1.available() > 0) {
        last_byte_ms = millis();
        if (parser.feed((uint8_t)Serial1.read())) {
            const Halo::Frame &f = parser.frame;
            const uint8_t n = f.led_count > HALO_MAX_LEDS ? HALO_MAX_LEDS : f.led_count;
            if (!have_frame) {
                // first frame after power-up: start from black, not mid-colour
                for (uint8_t i = 0; i < 3 * f.zones; ++i)
                    cur_rgb[i] = (int32_t)f.rgb[i] << 8;
            }
            zones = f.zones;
            led_count = n;
            for (uint8_t i = 0; i < 3 * f.zones; ++i)
                tgt_rgb[i] = (int32_t)f.rgb[i] << 8;
            tgt_bright = (int32_t)f.brightness << 8;
            have_frame = true;
            last_valid_ms = millis();
            got_frame = true;
        }
    }
    if (!have_frame)
        return;  // stay dark until the first valid frame

    const unsigned long now = millis();
    if (got_frame && parser.pending() == 0) {
        // Just after a complete frame: the line is quiet for the next ~60 ms.
        last_tick_ms = now;
        step();
        return;
    }
    // Frames have stopped (daemon off, crashed or unplugged): tick on our own
    // so the failsafe can fade the strip out.
    if (now - last_byte_ms < kSilentMs || now - last_tick_ms < kSilentTickMs)
        return;
    last_tick_ms = now;
    if (now - last_valid_ms > HALO_FAILSAFE_MS)
        tgt_bright = 0;
    step();
}
