#include "panel_options.h"

#include <algorithm>
#include <array>
#include <mutex>
#include <regex>

namespace PanelHardware {

namespace {
// Keep in sync with scripts/apply_hardware.sh.
constexpr std::array kGpioMappings = {"regular", "adafruit-hat", "adafruit-hat-pwm", "regular-pi1",
                                      "classic", "classic-pi1",  "compute-module"};
constexpr std::array kPanelTypes = {"", "FM6126A", "FM6127"};
constexpr std::array kRgbSequences = {"RGB", "RBG", "GRB", "GBR", "BRG", "BGR"};
const std::regex kPixelMapper(
    R"(^((Rotate:(0|90|180|270))|(Mirror:[HV])|U-mapper|(V-mapper(:Z)?))(;((Rotate:(0|90|180|270))|(Mirror:[HV])|U-mapper|(V-mapper(:Z)?))){0,3}$)");

std::mutex running_mutex;
Options running_options;

template <std::size_t N>
bool one_of(const std::string& value, const std::array<const char*, N>& allowed)
{
    return std::any_of(allowed.begin(), allowed.end(), [&](const char* a) { return value == a; });
}

std::string range(const char* name, int value, int low, int high)
{
    if (value < low || value > high)
        return std::string(name) + " must be " + std::to_string(low) + ".." + std::to_string(high);
    return {};
}
}  // namespace

std::string validate(const Options& o)
{
    for (const auto& error :
         {range("rows", o.rows, 8, 64), range("cols", o.cols, 16, 256), range("chain", o.chain, 1, 16), range("parallel", o.parallel, 1, 3),
          range("slowdown", o.slowdown, 0, 5), range("multiplexing", o.multiplexing, 0, 32), range("row_addr_type", o.row_addr_type, 0, 5),
          range("pwm_bits", o.pwm_bits, 1, 11), range("pwm_lsb_nanoseconds", o.pwm_lsb_nanoseconds, 50, 3000),
          range("pwm_dither_bits", o.pwm_dither_bits, 0, 2)})
        if (!error.empty())
            return error;
    if (!one_of(o.gpio_mapping, kGpioMappings))
        return "unknown gpio_mapping";
    if (!one_of(o.panel_type, kPanelTypes))
        return "panel_type must be empty, FM6126A or FM6127";
    if (!one_of(o.rgb_sequence, kRgbSequences))
        return "rgb_sequence must be an order of R, G and B";
    if (!o.pixel_mapper.empty() && !std::regex_match(o.pixel_mapper, kPixelMapper))
        return "pixel_mapper must be up to four of Rotate:<0|90|180|270>, Mirror:<H|V>, U-mapper, V-mapper[:Z], separated by ';'";

    rgb_matrix::RGBMatrix::Options library;
    library.hardware_mapping = o.gpio_mapping.c_str();
    library.rows = o.rows;
    library.cols = o.cols;
    library.chain_length = o.chain;
    library.parallel = o.parallel;
    library.pwm_bits = o.pwm_bits;
    library.pwm_lsb_nanoseconds = o.pwm_lsb_nanoseconds;
    library.pwm_dither_bits = o.pwm_dither_bits;
    library.multiplexing = o.multiplexing;
    library.row_address_type = o.row_addr_type;
    library.led_rgb_sequence = o.rgb_sequence.c_str();
    library.pixel_mapper_config = o.pixel_mapper.empty() ? nullptr : o.pixel_mapper.c_str();
    library.panel_type = o.panel_type.empty() ? nullptr : o.panel_type.c_str();
    std::string error;
    if (!library.Validate(&error)) {
        while (!error.empty() && (error.back() == '\n' || error.back() == ' ')) error.pop_back();
        return error.empty() ? "the LED library rejects these settings" : error;
    }
    return {};
}

std::string to_flags(const Options& o)
{
    std::string flags = "--led-rows=" + std::to_string(o.rows) + " --led-cols=" + std::to_string(o.cols) +
                        " --led-chain=" + std::to_string(o.chain) + " --led-parallel=" + std::to_string(o.parallel) +
                        " --led-slowdown-gpio=" + std::to_string(o.slowdown) + " --led-gpio-mapping=" + o.gpio_mapping;
    if (!o.pixel_mapper.empty())
        flags += " --led-pixel-mapper=" + o.pixel_mapper;
    flags += " --led-multiplexing=" + std::to_string(o.multiplexing) + " --led-row-addr-type=" + std::to_string(o.row_addr_type);
    if (!o.panel_type.empty())
        flags += " --led-panel-type=" + o.panel_type;
    flags += " --led-pwm-bits=" + std::to_string(o.pwm_bits) + " --led-pwm-lsb-nanoseconds=" + std::to_string(o.pwm_lsb_nanoseconds) +
             " --led-pwm-dither-bits=" + std::to_string(o.pwm_dither_bits) + " --led-rgb-sequence=" + o.rgb_sequence;
    return flags;
}

nlohmann::json to_json(const Options& o)
{
    return {{"rows", o.rows},
            {"cols", o.cols},
            {"chain", o.chain},
            {"parallel", o.parallel},
            {"slowdown", o.slowdown},
            {"gpio_mapping", o.gpio_mapping},
            {"pixel_mapper", o.pixel_mapper},
            {"multiplexing", o.multiplexing},
            {"row_addr_type", o.row_addr_type},
            {"panel_type", o.panel_type},
            {"pwm_bits", o.pwm_bits},
            {"pwm_lsb_nanoseconds", o.pwm_lsb_nanoseconds},
            {"pwm_dither_bits", o.pwm_dither_bits},
            {"rgb_sequence", o.rgb_sequence}};
}

std::optional<Options> from_json(const nlohmann::json& j, std::string& error)
{
    if (!j.is_object()) {
        error = "expected an object";
        return std::nullopt;
    }
    Options o;
    const auto known = to_json(o);
    for (const auto& [key, value] : j.items()) {
        if (!known.contains(key)) {
            error = "unknown setting " + key;
            return std::nullopt;
        }
        if (known[key].is_number_integer() ? !value.is_number_integer() : !value.is_string()) {
            error = key + (known[key].is_string() ? " must be a string" : " must be a whole number");
            return std::nullopt;
        }
    }
    auto merged = known;
    merged.update(j);
    o.rows = merged["rows"];
    o.cols = merged["cols"];
    o.chain = merged["chain"];
    o.parallel = merged["parallel"];
    o.slowdown = merged["slowdown"];
    o.gpio_mapping = merged["gpio_mapping"];
    o.pixel_mapper = merged["pixel_mapper"];
    o.multiplexing = merged["multiplexing"];
    o.row_addr_type = merged["row_addr_type"];
    o.panel_type = merged["panel_type"];
    o.pwm_bits = merged["pwm_bits"];
    o.pwm_lsb_nanoseconds = merged["pwm_lsb_nanoseconds"];
    o.pwm_dither_bits = merged["pwm_dither_bits"];
    o.rgb_sequence = merged["rgb_sequence"];
    return o;
}

void set_running(const Options& options)
{
    std::lock_guard lock(running_mutex);
    running_options = options;
}

Options running()
{
    std::lock_guard lock(running_mutex);
    return running_options;
}

Options from_library(const rgb_matrix::RGBMatrix::Options& l, const rgb_matrix::RuntimeOptions& runtime)
{
    Options o;
    o.rows = l.rows;
    o.cols = l.cols;
    o.chain = l.chain_length;
    o.parallel = l.parallel;
    o.slowdown = runtime.gpio_slowdown;
    o.gpio_mapping = l.hardware_mapping ? l.hardware_mapping : "regular";
    o.pixel_mapper = l.pixel_mapper_config ? l.pixel_mapper_config : "";
    o.multiplexing = l.multiplexing;
    o.row_addr_type = l.row_address_type;
    o.panel_type = l.panel_type ? l.panel_type : "";
    o.pwm_bits = l.pwm_bits;
    o.pwm_lsb_nanoseconds = l.pwm_lsb_nanoseconds;
    o.pwm_dither_bits = l.pwm_dither_bits;
    o.rgb_sequence = l.led_rgb_sequence ? l.led_rgb_sequence : "RGB";
    // The library accepts "Rotate:270" and the flag "--led-pixel-mapper Rotate:270"
    // alike; normalise spaces the shell may have left.
    o.pixel_mapper.erase(std::remove(o.pixel_mapper.begin(), o.pixel_mapper.end(), ' '), o.pixel_mapper.end());
    return o;
}

}  // namespace PanelHardware
