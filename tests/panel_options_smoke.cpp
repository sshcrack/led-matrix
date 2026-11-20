// Panel hardware options (#233): validation, flags, JSON, and that the flags
// the daemon writes parse back to the same options in the LED library.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "hardware/panel_options.h"

using namespace PanelHardware;

static int failures = 0;
#define CHECK(condition)                                                                \
    do {                                                                                \
        if (!(condition)) {                                                             \
            std::fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #condition); \
            ++failures;                                                                 \
        }                                                                               \
    } while (0)

static Options parse_flags(const std::string& flags)
{
    std::vector<std::string> words = {"led-matrix"};
    for (std::size_t start = 0; start < flags.size();) {
        const auto end = flags.find(' ', start);
        words.push_back(flags.substr(start, end == std::string::npos ? std::string::npos : end - start));
        start = end == std::string::npos ? flags.size() : end + 1;
    }
    std::vector<char*> argv;
    for (auto& w : words) argv.push_back(w.data());
    int argc = static_cast<int>(argv.size());
    char** args = argv.data();
    rgb_matrix::RGBMatrix::Options library;
    rgb_matrix::RuntimeOptions runtime;
    if (!rgb_matrix::ParseOptionsFromFlags(&argc, &args, &library, &runtime))
        ++failures;
    return from_library(library, runtime);
}

int main()
{
    Options ledmat;
    ledmat.rows = 32;
    ledmat.cols = 64;
    ledmat.chain = 2;
    ledmat.parallel = 2;
    ledmat.slowdown = 2;
    ledmat.pixel_mapper = "Rotate:90";
    CHECK(validate(ledmat).empty());
    // Same text as tests/apply_hardware_test.py expects from the root helper.
    CHECK(to_flags(ledmat) ==
          "--led-rows=32 --led-cols=64 --led-chain=2 --led-parallel=2 --led-slowdown-gpio=2 --led-gpio-mapping=regular "
          "--led-pixel-mapper=Rotate:90 --led-multiplexing=0 --led-row-addr-type=0 --led-pwm-bits=11 "
          "--led-pwm-lsb-nanoseconds=130 --led-pwm-dither-bits=0 --led-rgb-sequence=RGB");

    // The written flags mean the same options to the library.
    CHECK(parse_flags(to_flags(ledmat)) == ledmat);
    Options special = ledmat;
    special.gpio_mapping = "adafruit-hat-pwm";
    special.panel_type = "FM6126A";
    special.multiplexing = 3;
    special.row_addr_type = 1;
    special.pwm_bits = 9;
    special.pwm_lsb_nanoseconds = 200;
    special.pwm_dither_bits = 1;
    special.rgb_sequence = "GBR";
    special.pixel_mapper = "U-mapper;Rotate:180";
    CHECK(validate(special).empty());
    CHECK(parse_flags(to_flags(special)) == special);

    // Whitelist and the library's own checks.
    auto bad = ledmat;
    bad.rows = 7;
    CHECK(!validate(bad).empty());
    bad = ledmat;
    bad.pixel_mapper = "Rotate:45";
    CHECK(!validate(bad).empty());
    bad = ledmat;
    bad.pixel_mapper = "Rotate:90; rm -rf /";
    CHECK(!validate(bad).empty());
    bad = ledmat;
    bad.gpio_mapping = "regular --led-daemon=1";
    CHECK(!validate(bad).empty());
    bad = ledmat;
    bad.multiplexing = 31;  // within our range, but no such multiplexer
    CHECK(!validate(bad).empty());

    // JSON: strict types and keys, missing keys take defaults.
    std::string error;
    auto parsed = from_json(to_json(special), error);
    CHECK(parsed && *parsed == special);
    parsed = from_json({{"rows", 32}}, error);
    CHECK(parsed && parsed->rows == 32 && parsed->cols == 64);
    CHECK(!from_json({{"rows", "32"}}, error));
    CHECK(!from_json({{"rows", 32.5}}, error));
    CHECK(!from_json({{"brightness", 50}}, error));
    CHECK(!from_json(nlohmann::json::array(), error));

    if (failures)
        std::fprintf(stderr, "%d check(s) failed\n", failures);
    else
        std::puts("panel_options_smoke: OK");
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
