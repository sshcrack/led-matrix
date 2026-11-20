#pragma once

#include <led-matrix.h>

#include <nlohmann/json.hpp>
#include <optional>
#include <string>

/// Panel hardware settings the web UI may change (#233). They end up as
/// `--led-*` flags in MATRIX_OPTS (/etc/default/led-matrix), which only root can
/// write: the daemon validates a request and queues it for
/// scripts/apply_hardware.sh (led-matrix-hardware.path), which validates it again,
/// rewrites MATRIX_OPTS, restarts the service and restores the old flags when the
/// daemon does not come back.
namespace PanelHardware {

struct Options {
    int rows = 64;
    int cols = 64;
    int chain = 1;
    int parallel = 1;
    int slowdown = 1;
    std::string gpio_mapping = "regular";
    std::string pixel_mapper;  // "" or e.g. "Rotate:270", "U-mapper;Rotate:90"
    int multiplexing = 0;
    int row_addr_type = 0;
    std::string panel_type;  // "", "FM6126A", "FM6127"
    int pwm_bits = 11;
    int pwm_lsb_nanoseconds = 130;
    int pwm_dither_bits = 0;
    std::string rgb_sequence = "RGB";

    bool operator==(const Options&) const = default;
};

/// Empty when valid, else what is wrong. Checks our whitelist (also enforced by
/// apply_hardware.sh) and the library's own Options::Validate().
std::string validate(const Options& options);

/// The `--led-*` flags for these options, in a fixed order.
std::string to_flags(const Options& options);

/// Strict: unknown keys and wrong types are errors (returned in `error`).
std::optional<Options> from_json(const nlohmann::json& j, std::string& error);
nlohmann::json to_json(const Options& options);

/// What the daemon was started with (set once at startup; emulator: defaults).
void set_running(const Options& options);
Options running();

/// Builds Options from the flags the library parsed.
Options from_library(const rgb_matrix::RGBMatrix::Options& options, const rgb_matrix::RuntimeOptions& runtime);

}  // namespace PanelHardware
