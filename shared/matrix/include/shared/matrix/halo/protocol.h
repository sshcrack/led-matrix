#pragma once
// Halo UART protocol: the one definition shared by the daemon (encoder) and
// the Pro Micro firmware (parser). Plain C++ with no STL and no exceptions so
// it also compiles for the ATmega32U4; tests/halo_smoke.cpp runs this very
// parser against fixtures and garbage.
//
// Frame (one-way, Pi -> Pro Micro, 115200 8N1):
//
//   0xA5 0x5A  LEN  PAYLOAD[LEN]  CRC8
//
//   PAYLOAD = VERSION(1) BRIGHTNESS(0..255) LED_COUNT(1..200) ZONES(1..16)
//             ZONES x (R G B)
//   LEN     = 4 + 3*ZONES      (7..52)
//   CRC8    = CRC-8 (poly 0x07, init 0x00, no reflection) over LEN and PAYLOAD
//
// The ZONES colours are key colours spaced evenly around the LED loop; the
// firmware interpolates between neighbours (the last one blends into the
// first).
#include <stddef.h>
#include <stdint.h>

namespace Halo {

constexpr uint8_t kSync0 = 0xA5;
constexpr uint8_t kSync1 = 0x5A;
constexpr uint8_t kVersion = 1;
constexpr uint8_t kMaxZones = 16;
constexpr uint8_t kMaxLeds = 200;
constexpr uint8_t kHeaderBytes = 4;  // version, brightness, led count, zones
constexpr uint8_t kMaxLen = kHeaderBytes + 3 * kMaxZones;      // 52
constexpr uint8_t kMaxFrameBytes = 2 + 1 + kMaxLen + 1;        // 56
constexpr uint32_t kBaud = 115200;

inline uint8_t crc8(const uint8_t *data, size_t n, uint8_t crc = 0)
{
    for (size_t i = 0; i < n; ++i) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; ++b)
            crc = (crc & 0x80) ? uint8_t((crc << 1) ^ 0x07) : uint8_t(crc << 1);
    }
    return crc;
}

struct Frame {
    uint8_t brightness = 0;
    uint8_t led_count = 0;
    uint8_t zones = 0;
    uint8_t rgb[3 * kMaxZones] = {};
};

/// Writes one frame to `out` (at least kMaxFrameBytes). Returns its size, or 0
/// when the arguments are out of range.
inline size_t encode(uint8_t *out, uint8_t brightness, uint8_t led_count,
                     uint8_t zones, const uint8_t *rgb)
{
    if (zones < 1 || zones > kMaxZones || led_count < 1 || led_count > kMaxLeds)
        return 0;
    const uint8_t len = uint8_t(kHeaderBytes + 3 * zones);
    out[0] = kSync0;
    out[1] = kSync1;
    out[2] = len;
    out[3] = kVersion;
    out[4] = brightness;
    out[5] = led_count;
    out[6] = zones;
    for (size_t i = 0; i < size_t(3) * zones; ++i)
        out[7 + i] = rgb[i];
    out[3 + len] = crc8(out + 2, size_t(len) + 1);
    return size_t(4) + len;
}

/// Byte-at-a-time parser. Any garbage, truncated frame or bad checksum is
/// dropped and the parser re-synchronises on the next sync pair, even when it
/// sits inside the bytes of a failed frame.
class Parser {
public:
    /// Feed one received byte. True when `frame` now holds a new, valid frame.
    bool feed(uint8_t byte)
    {
        if (n_ == sizeof(buf_))
            drop(1);  // cannot happen after a rescan; defensive
        buf_[n_++] = byte;
        return scan();
    }

    void reset() { n_ = 0; }
    /// Bytes buffered for a frame that is not complete yet.
    size_t pending() const { return n_; }

    Frame frame;

private:
    void drop(size_t k)
    {
        if (k >= n_) {
            n_ = 0;
            return;
        }
        for (size_t i = 0; i + k < n_; ++i)
            buf_[i] = buf_[i + k];
        n_ -= k;
    }

    bool scan()
    {
        for (;;) {
            // Align on the sync pair.
            while (n_ > 0 && buf_[0] != kSync0)
                drop(1);
            if (n_ >= 2 && buf_[1] != kSync1) {
                drop(1);
                continue;
            }
            if (n_ < 3)
                return false;
            const uint8_t len = buf_[2];
            const bool len_ok = len >= kHeaderBytes + 3 && len <= kMaxLen && (len - kHeaderBytes) % 3 == 0;
            if (!len_ok) {
                drop(1);
                continue;
            }
            const size_t total = size_t(4) + len;
            if (n_ < total)
                return false;
            if (crc8(buf_ + 2, size_t(len) + 1) != buf_[total - 1]
                || buf_[3] != kVersion
                || buf_[5] < 1 || buf_[5] > kMaxLeds
                || buf_[6] != (len - kHeaderBytes) / 3) {
                drop(1);
                continue;
            }
            frame.brightness = buf_[4];
            frame.led_count = buf_[5];
            frame.zones = buf_[6];
            for (size_t i = 0; i < size_t(3) * frame.zones; ++i)
                frame.rgb[i] = buf_[7 + i];
            drop(total);
            return true;
        }
    }

    uint8_t buf_[kMaxFrameBytes] = {};
    size_t n_ = 0;
};

}  // namespace Halo
