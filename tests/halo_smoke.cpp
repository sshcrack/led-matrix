// Rear halo light: UART protocol (the parser the Pro Micro firmware runs),
// edge sampling and smoothing, settings parsing, and the real sender service
// writing into a pseudo-terminal.
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "halo/halo_service.h"
#include "shared/matrix/config/data.h"
#include "shared/matrix/halo/protocol.h"
#include "shared/matrix/halo/sampler.h"
#include "shared/matrix/halo/settings.h"

using namespace Halo;

namespace {
int failures = 0;
void check(bool ok, const std::string& what)
{
    if (!ok) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

bool feed_all(Parser& p, const std::vector<std::uint8_t>& bytes, int* valid = nullptr)
{
    bool any = false;
    for (auto b : bytes)
        if (p.feed(b)) {
            any = true;
            if (valid)
                ++*valid;
        }
    return any;
}

void protocol()
{
    const std::uint8_t check_str[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    check(crc8(check_str, 9) == 0xF4, "CRC-8 check value");

    // Fixture computed independently (python) for the firmware/daemon contract.
    const std::vector<std::uint8_t> fixture = {0xA5, 0x5A, 0x0A, 0x01, 0x80, 0x28, 0x02, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x10, 0xDD};
    const std::uint8_t rgb[] = {255, 0, 0, 0, 255, 16};
    std::uint8_t buf[kMaxFrameBytes];
    const size_t n = encode(buf, 128, 40, 2, rgb);
    check(n == fixture.size() && std::memcmp(buf, fixture.data(), n) == 0, "encoder matches fixture");

    Parser p;
    int valid = 0;
    check(feed_all(p, fixture, &valid) && valid == 1, "fixture parses");
    check(p.frame.brightness == 128 && p.frame.led_count == 40 && p.frame.zones == 2 && p.frame.rgb[0] == 255 && p.frame.rgb[4] == 255 &&
              p.frame.rgb[5] == 16,
          "fixture content");

    check(encode(buf, 1, 0, 2, rgb) == 0 && encode(buf, 1, 40, 0, rgb) == 0 && encode(buf, 1, 40, 17, rgb) == 0 &&
              encode(buf, 1, 201, 2, rgb) == 0,
          "encoder rejects out-of-range arguments");

    // Every single-bit corruption of every byte is rejected (CRC + framing).
    int accepted = 0;
    for (size_t i = 0; i < fixture.size(); ++i)
        for (int bit = 0; bit < 8; ++bit) {
            auto bad = fixture;
            bad[i] ^= std::uint8_t(1 << bit);
            Parser q;
            feed_all(q, bad, &accepted);
        }
    check(accepted == 0, "corrupted frames are dropped (accepted " + std::to_string(accepted) + ")");

    // Truncated frame, then a good one: the good one still parses.
    Parser t;
    valid = 0;
    std::vector<std::uint8_t> stream(fixture.begin(), fixture.begin() + 9);
    stream.insert(stream.end(), fixture.begin(), fixture.end());
    feed_all(t, stream, &valid);
    check(valid == 1, "truncated frame followed by a good frame resyncs");

    // Boot noise (console text) around frames never lights anything by itself.
    Parser noise;
    valid = 0;
    std::vector<std::uint8_t> boot;
    for (const char* c = "[    0.000000] Booting Linux on physical CPU 0x0\r\nraspberrypi login: "; *c; ++c)
        boot.push_back(std::uint8_t(*c));
    feed_all(noise, boot, &valid);
    check(valid == 0, "console noise yields no frame");
    boot.insert(boot.end(), fixture.begin(), fixture.end());
    boot.insert(boot.end(), {0xA5, 0xA5, 0x5A, 0x00, 0xFF});
    boot.insert(boot.end(), fixture.begin(), fixture.end());
    feed_all(noise, boot, &valid);
    check(valid == 2, "frames are found between noise and false sync bytes");

    // Pseudo-random garbage with the sync pair sprinkled in never validates.
    unsigned seed = 12345;
    Parser fuzz;
    valid = 0;
    for (int i = 0; i < 200000; ++i) {
        seed = seed * 1103515245u + 12345u;
        std::uint8_t b = std::uint8_t(seed >> 16);
        if ((seed >> 8) % 11 == 0)
            b = kSync0;
        if ((seed >> 8) % 13 == 0)
            b = kSync1;
        if (fuzz.feed(b))
            ++valid;
    }
    // A random 8-bit CRC passes 1/256 of the time, but only for well-formed
    // headers; a handful of accidental frames in 200k bytes is expected, the
    // parser must simply never crash or loop. The checksum is the defence.
    check(valid < 400, "fuzz stays sane (" + std::to_string(valid) + " accidental frames)");

    // A maximum-size frame round-trips.
    std::uint8_t big[3 * kMaxZones];
    for (size_t i = 0; i < sizeof(big); ++i) big[i] = std::uint8_t(i * 5);
    const size_t m = encode(buf, 255, 200, kMaxZones, big);
    check(m == kMaxFrameBytes, "max frame size");
    Parser bp;
    std::vector<std::uint8_t> v(buf, buf + m);
    check(feed_all(bp, v) && bp.frame.zones == kMaxZones && std::memcmp(bp.frame.rgb, big, sizeof(big)) == 0, "max frame round trip");
}

void sampling()
{
    // Paint a 128x128 display: top edge red, right green, bottom blue, left white, interior grey.
    const int w = 128, h = 128;
    auto pixel = [&](int x, int y) -> Rgb {
        const int dt = y, dr = w - 1 - x, db = h - 1 - y, dl = x;
        const int near = std::min({dt, dr, db, dl});
        if (near >= 8)
            return {90, 90, 90};
        if (near == dt)
            return {255, 0, 0};
        if (near == dr)
            return {0, 255, 0};
        if (near == db)
            return {0, 0, 255};
        return {255, 255, 255};
    };
    int reads = 0;
    const auto zones = sample_zones(w, h, [&](int x, int y) {
        ++reads;
        check(x >= 0 && x < w && y >= 0 && y < h, "sample inside the display");
        return pixel(x, y);
    });
    check(reads == kEdgeZones * 16, "sparse: 16 reads per zone (" + std::to_string(reads) + ")");
    int custom_reads = 0;
    const auto deeper = sample_zones(
        w, h,
        [&](int, int y) -> Rgb {
            ++custom_reads;
            return y == 14 ? Rgb{200, 0, 0} : Rgb{};
        },
        12, 14);
    check(custom_reads == kEdgeZones * 24 && deeper[0] == Rgb{100, 0, 0}, "custom density and depth change the actual samples");
    check(zones[0] == Rgb{255, 0, 0} && zones[1] == Rgb{255, 0, 0}, "top zones red");
    check(zones[2] == Rgb{0, 255, 0} && zones[3] == Rgb{0, 255, 0}, "right zones green");
    check(zones[4] == Rgb{0, 0, 255} && zones[5] == Rgb{0, 0, 255}, "bottom zones blue");
    check(zones[6] == Rgb{255, 255, 255} && zones[7] == Rgb{255, 255, 255}, "left zones white");

    // Zone order is clockwise: a bright patch at the top-right only lights zone 1.
    const auto patch = sample_zones(w, h, [&](int x, int y) -> Rgb { return (y < 8 && x >= w / 2) ? Rgb{200, 100, 50} : Rgb{}; });
    check(patch[1] == Rgb{200, 100, 50} && patch[0] == Rgb{} && patch[2] == Rgb{}, "clockwise zone order");
    // Bottom-left patch is zone 5 (bottom, clockwise = right to left, second half).
    const auto bl = sample_zones(w, h, [&](int x, int y) -> Rgb { return (y >= h - 8 && x < w / 2) ? Rgb{9, 8, 7} : Rgb{}; });
    check(bl[5] == Rgb{9, 8, 7} && bl[4] == Rgb{}, "bottom-left is zone 5");

    // Odd sizes and tiny panels do not read outside.
    for (auto [sw, sh] : {std::pair{64, 32}, {5, 5}, {200, 7}}) {
        sample_zones(sw, sh, [&](int x, int y) {
            check(x >= 0 && x < sw && y >= 0 && y < sh, "in-bounds sample on odd size");
            return Rgb{};
        });
    }

    Zones id{};
    for (int i = 0; i < kEdgeZones; ++i) id[i] = {std::uint8_t(i), 0, 0};
    check(arrange_zones(id, 0, false)[3].r == 3, "identity arrangement");
    check(arrange_zones(id, 2, false)[0].r == 2 && arrange_zones(id, 2, false)[6].r == 0, "rotation");
    check(arrange_zones(id, 0, true)[0].r == 7 && arrange_zones(id, 0, true)[7].r == 0, "reverse");

    ZoneSmoother sm(0.5f);
    Zones white{};
    white.fill({200, 100, 0});
    check(sm.step(white)[0] == Rgb{200, 100, 0}, "first step snaps to the target");
    Zones black{};
    const auto s1 = sm.step(black)[0];
    check(s1.r == 100 && s1.g == 50, "step moves half way");
    Rgb last = s1;
    for (int i = 0; i < 20; ++i) last = sm.step(black)[0];
    check(last == Rgb{}, "converges to the target");
}

void settings()
{
    Settings d;
    check(!d.enabled && d.mode == "match" && d.led_count == 40 && d.device == "/dev/serial0", "defaults");
    check(d.validate().empty(), "defaults validate");
    check(d.sample_columns == 8 && d.sample_depth == 6, "old-config sampling defaults");
    Settings custom_sampling = nlohmann::json{{"sample_columns", 16}, {"sample_depth", 24}};
    const auto sampling_json = nlohmann::json(custom_sampling);
    check(sampling_json["sample_columns"] == 16 && sampling_json["sample_depth"] == 24, "sampling settings round trip");
    custom_sampling.sample_columns = 17;
    check(!custom_sampling.validate().empty(), "sampling density is bounded");

    Settings s = nlohmann::json{{"enabled", true}, {"mode", "fixed"},          {"brightness", 30}, {"color", "#00FF7f"},
                                {"led_count", 33}, {"device", "/dev/ttyAMA0"}, {"rotation", 3},    {"reverse", true}};
    check(s.enabled && s.mode == "fixed" && s.brightness == 30 && s.led_count == 33 && s.device == "/dev/ttyAMA0" && s.rotation == 3 &&
              s.reverse,
          "parses all keys");
    unsigned char r, g, b;
    check(parse_color(s.color, r, g, b) && r == 0 && g == 255 && b == 127, "colour parse");
    check(!parse_color("00ff00", r, g, b) && !parse_color("#00ff0", r, g, b) && !parse_color("#00ff0g", r, g, b), "bad colours");

    Settings bad = nlohmann::json{{"mode", "disco"}, {"brightness", 500}, {"led_count", 0}, {"color", "red"},
                                  {"rotation", 99},  {"enabled", "yes"},  {"device", ""}};
    check(bad.mode == "match" && bad.brightness == 100 && bad.led_count == 1 && bad.color == d.color && bad.rotation == 7 && !bad.enabled &&
              bad.device == "/dev/serial0",
          "lenient parse clamps and defaults");
    check(Settings(nlohmann::json::array()).mode == "match", "non-object json keeps defaults");

    Settings v;
    v.brightness = 101;
    check(!v.validate().empty(), "strict validate rejects");

    // The device is opened as root before the privilege drop: only serial port names pass.
    for (const char* ok : {"/dev/serial0", "/dev/ttyAMA0", "/dev/ttyS0", "/dev/ttyUSB12", "/dev/ttyACM0", "/dev/pts/3"})
        check(is_allowed_device(ok), std::string("allowed device ") + ok);
    for (const char* no : {"", "/etc/shadow", "/dev/sda", "/dev/serial", "/dev/ttyS0/../../etc/passwd", "/dev/ttyUSB0x", "/dev/pts/",
                           "/dev/ttyS12345", "/var/lib/led-matrix/config.json", "dev/serial0"})
        check(!is_allowed_device(no), std::string("rejected device ") + no);
    Settings dev;
    dev.device = "/etc/shadow";
    check(!dev.validate().empty(), "validate rejects a non-serial device");
    check(Settings(nlohmann::json{{"device", "/etc/shadow"}}).device == "/dev/serial0", "lenient parse drops a non-serial device");
    nlohmann::json j = s;
    check(Settings(j).color == s.color && Settings(j).led_count == 33, "json round trip");

    // Persisted in the config root next to environment.
    ConfigData::Root root;
    root.halo = s;
    nlohmann::json rj = root;
    check(rj.contains("halo") && rj["halo"]["led_count"] == 33, "halo is written to the config");
    ConfigData::Root back = rj.get<ConfigData::Root>();
    check(back.halo.enabled && back.halo.mode == "fixed", "halo is read from the config");
    nlohmann::json old = rj;
    old.erase("halo");
    check(!old.get<ConfigData::Root>().halo.enabled, "old config without halo keeps it off");
}

std::vector<std::uint8_t> read_available(int fd, int wait_ms)
{
    std::vector<std::uint8_t> out;
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(wait_ms);
    while (std::chrono::steady_clock::now() < end) {
        pollfd pfd{fd, POLLIN, 0};
        if (poll(&pfd, 1, 20) > 0) {
            std::uint8_t b[256];
            const ssize_t n = ::read(fd, b, sizeof(b));
            if (n > 0)
                out.insert(out.end(), b, b + n);
        }
    }
    return out;
}

void service_over_pty()
{
    const int master = posix_openpt(O_RDWR | O_NOCTTY);
    if (master < 0 || grantpt(master) != 0 || unlockpt(master) != 0) {
        std::cerr << "SKIP: no pty available\n";
        return;
    }
    const std::string slave = ptsname(master);
    // Raw master so we read exactly what was written.
    // (the service sets raw mode on the slave; the master side is passthrough)

    auto& svc = Service::instance();
    Settings s;
    s.enabled = true;
    s.mode = "fixed";
    s.color = "#102030";
    s.brightness = 40;
    s.led_count = 36;
    s.device = slave;
    svc.start(s);

    Parser parser;
    int frames = 0;
    Frame last;
    auto collect = [&](int ms) {
        for (auto b : read_available(master, ms))
            if (parser.feed(b)) {
                ++frames;
                last = parser.frame;
            }
    };
    collect(1000);
    check(frames >= 8 && frames <= 20, "about 15 frames/s in fixed mode (got " + std::to_string(frames) + " in 1 s)");
    check(last.zones == kEdgeZones && last.led_count == 36 && last.brightness == 102, "frame header from settings");
    check(last.rgb[0] == 0x10 && last.rgb[1] == 0x20 && last.rgb[2] == 0x30 && last.rgb[23] == 0x30, "fixed colour in every zone");

    // Match mode: the service asks for a sample and follows the display.
    s.mode = "match";
    s.brightness = 100;
    svc.update_settings(s);
    collect(200);
    Zones red{};
    red.fill({200, 0, 0});
    for (int i = 0; i < 30; ++i) {
        svc.submit_zones(red);
        collect(70);
    }
    check(last.brightness == 255 && last.rgb[0] > 190 && last.rgb[1] == 0, "match mode follows submitted colours");
    // Start at the left edge midpoint, between the lower and upper left zones.
    s.rotation = 6;
    s.start_between_zones = true;
    s.led_count = 63;
    svc.update_settings(s);
    red[6] = {64, 0, 0};
    red[7] = {192, 0, 0};
    svc.submit_zones(red);
    collect(1600);
    check(last.led_count == 63 && last.zones == kMaxZones && last.rgb[0] == 128 && last.rgb[3] == 192,
          "midpoint start preserves the original next anchor and configurable LED count");
    svc.submit_black();
    collect(1500);
    check(last.rgb[0] < 5, "match mode follows the display going dark");

    // Disabling sends off frames.
    s.enabled = false;
    svc.update_settings(s);
    collect(300);
    check(last.brightness == 0, "disabling sends brightness 0");
    const int before = frames;
    collect(2500);
    check(frames - before <= 20, "no frames are sent after the off burst");

    // An alignment test can run while the halo is disabled, using the draft count.
    svc.test_alignment(45);
    collect(1200);
    check(svc.status()["test_active"] == true && last.led_count == 45 && last.brightness == 40 && last.zones == kMaxZones &&
              last.rgb[0] == 255 && last.rgb[1] == 255 && last.rgb[2] == 255 && last.rgb[3] == 0 && last.rgb[4] == 255 && last.rgb[5] == 0,
          "alignment test marks LED zero and direction");
    svc.test_alignment(0);
    collect(250);
    check(svc.status()["test_active"] == false && last.led_count == 63 && last.brightness == 0,
          "stopping the test restores the disabled halo and saved LED count");

    check(svc.status()["frames_sent"].get<int>() > 0 && svc.status()["device_open"] == true, "status");
    svc.stop();
    close(master);

    // A missing device never throws or blocks.
    Settings missing;
    missing.enabled = true;
    missing.device = "/dev/ttyUSB97";
    svc.start(missing);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    const auto st = svc.status();
    check(st["device_open"] == false && st["error"].is_string(), "missing device is reported, not fatal");
    svc.stop();

    // A disallowed name or a regular file is never opened, even if it is in the settings.
    char path[] = "/tmp/halo-not-a-tty-XXXXXX";
    const int file = mkstemp(path);
    check(file >= 0, "temp file");
    for (const std::string& device : {std::string("/etc/shadow"), std::string(path)}) {
        Settings bad_dev;
        bad_dev.enabled = true;
        bad_dev.device = device;
        svc.start(bad_dev);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        check(svc.status()["device_open"] == false, "not opened: " + device);
        svc.stop();
    }
    struct stat st_file{};
    check(file >= 0 && fstat(file, &st_file) == 0 && st_file.st_size == 0, "nothing was written to the regular file");
    if (file >= 0) {
        close(file);
        unlink(path);
    }
}
}  // namespace

int main()
{
    protocol();
    sampling();
    settings();
    service_over_pty();
    if (failures) {
        std::cerr << failures << " halo check(s) failed\n";
        return 1;
    }
    std::cout << "halo smoke passed\n";
    return 0;
}
