/**
 * preview_gen: Generates animated GIF previews for all registered matrix scenes.
 *
 * Usage:
 *   preview_gen [--output <dir>] [--scene <name>] [--scenes <n1,n2,...>]
 *               [--frames <n>] [--fps <n>] [--width <n>] [--height <n>]
 *               [--dump-manifest] [--manifest-out <file>]
 *               [--metrics-out <file>] [--audio-bpm <n>]
 *               [--audio-profile <balanced|bass|percussion|ambient>]
 *               [--preview-option <provider>:<key>=<json>]
 *               [--virtual-time-only] [--strict]
 *
 * Defaults:
 *   --output    ./previews
 *   --frames    90   (6 seconds at 15 fps)
 *   --fps       15
 *   --width     128
 *   --height    128
 *
 * Manifest mode (--dump-manifest):
 *   Writes a JSON array of {name, plugin_name, plugin_path} objects and exits
 *   without rendering any GIFs.  Use --manifest-out to specify the output file
 *   (defaults to stdout).
 */

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <thread>
#include <chrono>
#include <cmath>

#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>
#include <Magick++.h>
#include <nlohmann/json.hpp>

#ifdef ENABLE_EMULATOR
#include "emulator.h"
#include "matrix-factory.h"
#endif

#include "led-matrix.h"
#include "shared/matrix/preview.h"
#include "shared/matrix/plugin_loader/loader.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/canvas_consts.h"
#include "shared/matrix/utils/consts.h"

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Argument parsing helpers
// ---------------------------------------------------------------------------
static bool parse_int(const char* str, int& out)
{
    try
    {
        out = std::stoi(str);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

static bool parse_float(const char* str, float& out)
{
    try
    {
        out = std::stof(str);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

struct Args
{
    std::string output_dir = "./previews";
    std::string filter_scene; // legacy --scene (single scene)
    std::vector<std::string> filter_scenes; // --scenes (comma-separated list)
    int fps = 15;
    int total_frames = 90; // 6 seconds @ 15 fps
    int matrix_width = 128;
    int matrix_height = 128;
    bool dump_manifest = false;
    bool strict = false;
    bool virtual_time_only = false;
    std::string manifest_out; // path for --manifest-out; empty = stdout
    std::string metrics_out;  // optional perceptual metrics JSON for regression tests
    float audio_bpm = 120.0f;
    std::string audio_profile = "balanced";
    nlohmann::json preview_options = nlohmann::json::object();
};

static Args parse_args(int argc, char* argv[])
{
    Args a;
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--output" && i + 1 < argc)
            a.output_dir = argv[++i];
        else if (std::string(argv[i]) == "--scene" && i + 1 < argc)
            a.filter_scene = argv[++i];
        else if (std::string(argv[i]) == "--scenes" && i + 1 < argc)
        {
            // comma-separated list of scene names
            std::string csv = argv[++i];
            std::stringstream ss(csv);
            std::string token;
            while (std::getline(ss, token, ','))
            {
                if (!token.empty())
                    a.filter_scenes.push_back(token);
            }
        }
        else if (std::string(argv[i]) == "--fps" && i + 1 < argc)
            parse_int(argv[++i], a.fps);
        else if (std::string(argv[i]) == "--frames" && i + 1 < argc)
            parse_int(argv[++i], a.total_frames);
        else if (std::string(argv[i]) == "--width" && i + 1 < argc)
            parse_int(argv[++i], a.matrix_width);
        else if (std::string(argv[i]) == "--height" && i + 1 < argc)
            parse_int(argv[++i], a.matrix_height);
        else if (std::string(argv[i]) == "--dump-manifest")
            a.dump_manifest = true;
        else if (std::string(argv[i]) == "--strict")
            a.strict = true;
        else if (std::string(argv[i]) == "--virtual-time-only")
            a.virtual_time_only = true;
        else if (std::string(argv[i]) == "--manifest-out" && i + 1 < argc)
            a.manifest_out = argv[++i];
        else if (std::string(argv[i]) == "--metrics-out" && i + 1 < argc)
            a.metrics_out = argv[++i];
        else if (std::string(argv[i]) == "--audio-bpm" && i + 1 < argc)
            parse_float(argv[++i], a.audio_bpm);
        else if (std::string(argv[i]) == "--audio-profile" && i + 1 < argc)
            a.audio_profile = argv[++i];
        else if (std::string(argv[i]) == "--preview-option" && i + 1 < argc)
        {
            const std::string raw = argv[++i];
            const auto colon = raw.find(':');
            const auto equals = raw.find('=', colon == std::string::npos ? 0 : colon + 1);
            if (colon != std::string::npos && equals != std::string::npos && colon > 0 && equals > colon + 1)
            {
                const auto provider = raw.substr(0, colon);
                const auto key = raw.substr(colon + 1, equals - colon - 1);
                const auto value = raw.substr(equals + 1);
                try {
                    a.preview_options[provider][key] = nlohmann::json::parse(value);
                } catch (const nlohmann::json::parse_error &) {
                    a.preview_options[provider][key] = value;
                }
            }
        }
    }
    // Normalise: merge --scene into filter_scenes
    if (!a.filter_scene.empty())
        a.filter_scenes.push_back(a.filter_scene);
    // Clamp fps to a sane range
    if (a.fps < 1)
        a.fps = 1;
    if (a.fps > 60)
        a.fps = 60;
    if (a.total_frames < 1)
        a.total_frames = 1;
    a.audio_bpm = std::clamp(a.audio_bpm, 40.0f, 240.0f);
    if (a.audio_profile != "balanced" && a.audio_profile != "bass" &&
        a.audio_profile != "percussion" && a.audio_profile != "ambient")
        a.audio_profile = "balanced";
    // Legacy convenience flags map onto the generic provider option bag.
    // Explicit --preview-option values win.
    auto &audio_options = a.preview_options[std::string(Previews::Inputs::Audio)];
    if (!audio_options.contains("bpm")) audio_options["bpm"] = a.audio_bpm;
    if (!audio_options.contains("profile")) audio_options["profile"] = a.audio_profile;
    return a;
}

// ---------------------------------------------------------------------------
// Read all pixels from a FrameCanvas into a flat RGB byte vector
// ---------------------------------------------------------------------------
struct VisualMetricAccumulator
{
    int frames = 0;
    int temporal_samples = 0;
    double lit_fraction_sum = 0.0;
    double lit_fraction_min = 1.0;
    double lit_fraction_max = 0.0;
    double mean_luma_sum = 0.0;
    double temporal_change_sum = 0.0;

    void add(const std::vector<uint8_t> &rgb, const std::vector<uint8_t> &previous)
    {
        const size_t pixels = rgb.size() / 3;
        if (pixels == 0) return;
        size_t lit = 0;
        size_t changed = 0;
        double luma = 0.0;
        for (size_t i = 0; i < pixels; ++i) {
            const int r = rgb[i * 3];
            const int g = rgb[i * 3 + 1];
            const int b = rgb[i * 3 + 2];
            if (std::max({r, g, b}) > 5) ++lit;
            luma += (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255.0;
            if (previous.size() == rgb.size()) {
                const int delta = std::abs(r - static_cast<int>(previous[i * 3])) +
                                  std::abs(g - static_cast<int>(previous[i * 3 + 1])) +
                                  std::abs(b - static_cast<int>(previous[i * 3 + 2]));
                if (delta >= 24) ++changed;
            }
        }
        const double lit_fraction = static_cast<double>(lit) / static_cast<double>(pixels);
        lit_fraction_sum += lit_fraction;
        lit_fraction_min = std::min(lit_fraction_min, lit_fraction);
        lit_fraction_max = std::max(lit_fraction_max, lit_fraction);
        mean_luma_sum += luma / static_cast<double>(pixels);
        if (previous.size() == rgb.size()) {
            temporal_change_sum += static_cast<double>(changed) / static_cast<double>(pixels);
            ++temporal_samples;
        }
        ++frames;
    }

    [[nodiscard]] nlohmann::json json() const
    {
        const double frame_count = static_cast<double>(std::max(1, frames));
        return {
            {"frames", frames},
            {"lit_fraction_average", lit_fraction_sum / frame_count},
            {"lit_fraction_min", frames > 0 ? lit_fraction_min : 0.0},
            {"lit_fraction_max", lit_fraction_max},
            {"mean_luma_average", mean_luma_sum / frame_count},
            {"temporal_change_average", temporal_samples > 0
                ? temporal_change_sum / static_cast<double>(temporal_samples) : 0.0}
        };
    }
};

static std::vector<uint8_t> capture_canvas(rgb_matrix::FrameCanvas* canvas,
                                           int w, int h)
{
    std::vector<uint8_t> buf(static_cast<size_t>(w * h * 3));
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            uint8_t r = 0, g = 0, b = 0;
            canvas->GetPixel(x, y, &r, &g, &b);
            const size_t idx = static_cast<size_t>((y * w + x) * 3);
            buf[idx] = r;
            buf[idx + 1] = g;
            buf[idx + 2] = b;
        }
    }
    return buf;
}

// ---------------------------------------------------------------------------
// Convert a flat RGB buffer to a GraphicsMagick Image with a GIF delay
// ---------------------------------------------------------------------------
static Magick::Image make_frame(const std::vector<uint8_t>& rgb,
                                int w, int h,
                                size_t delay_centiseconds)
{
    Magick::Image img(Magick::Geometry(static_cast<size_t>(w),
                                       static_cast<size_t>(h)),
                      Magick::Color(0, 0, 0));
    img.modifyImage();

    Magick::PixelPacket* pixels =
        img.getPixels(0, 0, static_cast<size_t>(w), static_cast<size_t>(h));

    // Scale each 8-bit channel to the full Quantum range [0, MaxRGB].
    // MaxRGB is a GraphicsMagick compile-time constant (65535 for 16-bit depth).
    const size_t total = static_cast<size_t>(w * h);
    for (size_t i = 0; i < total; ++i)
    {
        using MagickLib::Quantum;
        pixels[i].red = static_cast<Quantum>(
            static_cast<unsigned long>(rgb[i * 3]) * MaxRGB / 255UL);
        pixels[i].green = static_cast<Quantum>(
            static_cast<unsigned long>(rgb[i * 3 + 1]) * MaxRGB / 255UL);
        pixels[i].blue = static_cast<Quantum>(
            static_cast<unsigned long>(rgb[i * 3 + 2]) * MaxRGB / 255UL);
        pixels[i].opacity = 0; // fully opaque
    }
    img.syncPixels();

    img.animationDelay(delay_centiseconds);
    img.animationIterations(0); // loop forever
    return img;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    spdlog::cfg::load_env_levels();

    const Args args = parse_args(argc, argv);
    Previews::RuntimeScope preview_runtime;

    // ---- initialise GraphicsMagick ----------------------------------------
    Magick::InitializeMagick(*argv);

    // ---- create output directory ------------------------------------------
    std::error_code ec;
    fs::create_directories(args.output_dir, ec);
    if (ec)
    {
        spdlog::error("Cannot create output directory '{}': {}",
                      args.output_dir, ec.message());
        return 1;
    }

    // ---- create headless emulator matrix ----------------------------------
#ifndef ENABLE_EMULATOR
    spdlog::error("preview_gen requires ENABLE_EMULATOR to be set at compile time.");
    return 1;
#else
    rgb_matrix::RGBMatrix::Options led_opts;
    led_opts.rows = args.matrix_height;
    led_opts.cols = args.matrix_width;
    led_opts.chain_length = 1;
    led_opts.parallel = 1;

    rgb_matrix::EmulatorOptions emu_opts;
    emu_opts.headless = true;
    emu_opts.refresh_rate_hz = args.fps;

    rgb_matrix::EmulatorMatrix* matrix =
        rgb_matrix::EmulatorMatrix::Create(led_opts, emu_opts);
    if (!matrix)
    {
        spdlog::error("Failed to create headless emulator matrix.");
        return 1;
    }


    if (!filesystem::exists(Constants::root_dir))
    {
        filesystem::create_directory(Constants::root_dir);
    }

    // ---- initialise shared globals expected by SharedToolsMatrix ----------
    Constants::width = args.matrix_width;
    Constants::height = args.matrix_height;
    Constants::global_post_processor = nullptr;
    Constants::global_transition_manager = nullptr;
    Constants::global_update_manager = nullptr;

    // provide a minimal config so nothing derefs a null pointer
    const fs::path cfg_path = fs::temp_directory_path() / "preview_gen_config.json";
    config = new Config::MainConfig(cfg_path.string());

    // ---- load plugins ------------------------------------------------------
    spdlog::trace("Loading plugins…");
    const auto pl = Plugins::PluginManager::instance();
    pl->initialize();

    for (auto plugin : pl->get_plugins())
    {
        plugin->before_server_init();
        plugin->after_server_init();
    }

    std::unordered_map<std::string, std::shared_ptr<Previews::DataProvider>> preview_providers;
    for (auto *plugin : pl->get_plugins())
    {
        for (auto &provider : plugin->get_preview_data_providers())
        {
            if (!provider) continue;
            const std::string provider_id(provider->id());
            if (provider_id.empty()) continue;
            if (!preview_providers.emplace(provider_id, provider).second)
                spdlog::warn("Duplicate preview data provider '{}'; keeping the first one.", provider_id);
        }
    }

    const Previews::RunContext preview_run_context{
        .width = args.matrix_width,
        .height = args.matrix_height,
        .fps = args.fps,
        .total_frames = args.total_frames,
        .options = args.preview_options,
    };

    auto wrappers = pl->get_scenes();
    if (wrappers.empty())
    {
        spdlog::warn("No scenes found. Make sure PLUGIN_DIR points to the "
            "built plugins directory.");
    }

    // ---- dump-manifest mode: output scene→plugin mapping then exit --------
    if (args.dump_manifest)
    {
        nlohmann::json manifest = nlohmann::json::array();

        for (const auto& wrapper : wrappers)
        {
            const std::string scene_name = wrapper->get_name();
            const auto default_scene = wrapper->get_default();
            const auto capabilities = default_scene->get_capabilities();
            const auto preview_spec = default_scene->get_preview_spec();
            const auto runtime_spec = default_scene->get_effective_runtime_inputs();
            const bool needs_desktop = capabilities.requires_desktop;
            std::string plugin_name;
            std::string plugin_path;

            // Find which loaded plugin owns this scene wrapper
            for (const auto& pi : pl->get_plugins())
            {
                for (const auto& sw : pi->get_scenes())
                {
                    if (sw->get_name() == scene_name)
                    {
                        plugin_name = pi->get_plugin_name();
                        plugin_path = pi->get_plugin_location();
                        break;
                    }
                }
                if (!plugin_name.empty())
                    break;
            }

            manifest.push_back({
                {"name", scene_name},
                {"plugin_name", plugin_name},
                {"plugin_path", plugin_path},
                {"needs_desktop", needs_desktop},
                {"runtime_inputs", {
                    {"required", runtime_spec.required},
                    {"optional", runtime_spec.optional}
                }},
                {"capabilities", {
                    {"requires_desktop", capabilities.requires_desktop},
                    {"requires_audio", capabilities.requires_audio},
                    {"requires_network", capabilities.requires_network},
                    {"interactive", capabilities.interactive},
                    {"can_generate_preview", preview_spec.enabled},
                    {"supports_audio", capabilities.supports_audio},
                    {"music_director_eligible", capabilities.music_director_eligible}
                }},
                {"preview", {
                    {"enabled", preview_spec.enabled},
                    {"inputs", preview_spec.inputs},
                    {"property_overrides", preview_spec.property_overrides}
                }},
            });
        }

        const std::string manifest_str = manifest.dump(2);

        if (args.manifest_out.empty())
        {
            std::cout << manifest_str << "\n";
        }
        else
        {
            std::ofstream out(args.manifest_out);
            if (!out)
            {
                spdlog::error("Cannot write manifest to '{}'", args.manifest_out);
                return 1;
            }
            out << manifest_str << "\n";
            spdlog::info("Scene manifest written to {}", args.manifest_out);
        }

        // Cleanup and exit without rendering. Release all wrapper references
        // before plugin DSOs are unloaded.
        for (auto *plugin : pl->get_plugins()) plugin->pre_exit();
        wrappers.clear();
        preview_providers.clear();
        config->release_scene_references();
        pl->delete_references();
        pl->destroy_plugins();
        delete config;
        config = nullptr;
        delete matrix;
        return 0;
    }

    // ---- allocate a single render canvas ----------------------------------
    rgb_matrix::FrameCanvas* canvas = matrix->CreateFrameCanvas();
    canvas->Clear();

    // Legacy scenes may still read wall clock time. Scenes that explicitly
    // support virtual time can be generated quickly; legacy ones retain
    // real-time preview pacing.
    const int frame_delay_ms = 1000 / std::max(1, args.fps);
    const size_t frame_delay_cs =
        static_cast<size_t>(std::max(1, 100 / args.fps)); // centiseconds

    int generated = 0;
    int skipped = 0;
    int attempted = 0;
    nlohmann::json visual_metrics = nlohmann::json::object();

    // ---- iterate scenes ---------------------------------------------------
    for (const auto& wrapper : wrappers)
    {
        const std::string scene_name = wrapper->get_name();

        // SceneCapabilities answers only whether preview_gen can generate the
        // scene. Timing strategy remains an internal Scene implementation detail.
        const auto default_scene = wrapper->get_default();
        const auto preview_spec = default_scene->get_preview_spec();
        if (!preview_spec.enabled)
        {
            spdlog::info("Skipping '{}': scene did not opt into generated previews.", scene_name);
            continue;
        }

        std::vector<std::shared_ptr<Previews::DataProvider>> scene_preview_providers;
        bool missing_provider = false;
        for (const auto &input : preview_spec.inputs)
        {
            const auto it = preview_providers.find(input);
            if (it == preview_providers.end())
            {
                spdlog::warn("Skipping '{}': preview input '{}' has no registered provider.",
                             scene_name, input);
                missing_provider = true;
                break;
            }
            scene_preview_providers.push_back(it->second);
        }
        if (missing_provider)
        {
            if (args.strict) ++skipped;
            continue;
        }
        if (args.virtual_time_only && !default_scene->supports_virtual_time())
            continue;

        // Apply scene filter (--scene or --scenes)
        if (!args.filter_scenes.empty())
        {
            bool found = false;
            for (const auto& f : args.filter_scenes)
                if (f == scene_name)
                {
                    found = true;
                    break;
                }
            if (!found)
                continue;
        }

        ++attempted;
        spdlog::info("Rendering preview for '{}' ({} frames @ {} fps)…",
                     scene_name, args.total_frames, args.fps);

        // Per-scene crash isolation: wrap the entire render in try/catch so a
        // single broken scene does not abort the rest of the batch.
        try
        {
            // Create a fresh instance so each scene starts from t=0.
            // After register_properties(), dump default values back to a JSON object
            // so that load_properties() can set registered=true even for required
            // properties that have no user-supplied value.
            auto scene = wrapper->create();
            scene->update_default_properties();
            scene->register_properties();

            nlohmann::json default_props = nlohmann::json::object();
            for (const auto& prop : scene->get_properties())
                prop->dump_to_json(default_props);
            for (const auto &[name, value] : preview_spec.property_overrides.items())
                default_props[name] = value;

            scene->load_properties(default_props);

            struct ProviderSession {
                std::vector<std::shared_ptr<Previews::DataProvider>> started;
                ~ProviderSession() {
                    for (auto it = started.rbegin(); it != started.rend(); ++it)
                        (*it)->end();
                }
            } provider_session;
            for (const auto &provider : scene_preview_providers)
            {
                provider_session.started.push_back(provider);
                provider->begin(preview_run_context);
            }

            scene->initialize(args.matrix_width, args.matrix_height);

            std::vector<Magick::Image> frames;
            frames.reserve(static_cast<size_t>(args.total_frames));
            std::vector<uint8_t> previous_rgb;
            VisualMetricAccumulator metric_accumulator;
            int identical_tail_frames = 0;

            const double preview_dt = 1.0 / static_cast<double>(std::max(1, args.fps));
            for (int f = 0; f < args.total_frames; ++f)
            {
                const Scenes::SceneFrameContext preview_frame{
                    .delta_seconds = preview_dt,
                    .elapsed_seconds = static_cast<double>(f) * preview_dt,
                    .frame_index = static_cast<std::uint64_t>(f),
                    .now_ms = static_cast<std::uint64_t>(static_cast<double>(f) * preview_dt * 1000.0),
                    .deterministic = true,
                };
                for (const auto &provider : scene_preview_providers)
                    provider->update(preview_frame);
                canvas->Clear();
                bool keep_going = true;
                if (scene->supports_virtual_time()) {
                    // Migrated scenes can use fast virtual frame time.
                    keep_going = scene->render_frame(canvas, preview_dt, true);
                } else {
                    // Preserve correct animation for legacy wall-clock scenes.
                    std::this_thread::sleep_for(std::chrono::milliseconds(frame_delay_ms));
                    keep_going = scene->render_frame(canvas);
                }

                const auto rgb = capture_canvas(canvas, args.matrix_width,
                                                args.matrix_height);
                if (!previous_rgb.empty() && rgb == previous_rgb)
                    ++identical_tail_frames;
                else
                    identical_tail_frames = 0;
                metric_accumulator.add(rgb, previous_rgb);
                previous_rgb = rgb;
                frames.push_back(make_frame(rgb, args.matrix_width,
                                            args.matrix_height, frame_delay_cs));

                if (!keep_going)
                {
                    spdlog::debug("Scene '{}' stopped at frame {}/{}", scene_name,
                                  f + 1, args.total_frames);
                    if (args.strict && f + 1 < args.total_frames)
                        ++skipped;
                    break;
                }
            }

            if (frames.empty())
            {
                spdlog::warn("No frames captured for '{}', skipping.", scene_name);
                ++skipped;
                continue;
            }
            if (args.strict && frames.size() >= 6 && identical_tail_frames >= 5)
            {
                spdlog::warn("Scene '{}' stopped changing during its final {} frames.",
                             scene_name, identical_tail_frames + 1);
                ++skipped;
            }

            visual_metrics[scene_name] = metric_accumulator.json();

            // Quantise colours (required for GIF palette, 256 colours max)
            Magick::quantizeImages(frames.begin(), frames.end());

            const fs::path gif_path =
                fs::path(args.output_dir) / (scene_name + ".gif");

            Magick::writeImages(frames.begin(), frames.end(), gif_path.string());
            spdlog::info("Saved preview → {}", gif_path.string());
            ++generated;
        }
        catch (const std::exception& e)
        {
            spdlog::warn("Scene '{}' failed ({}); skipping — existing preview (if any) preserved.",
                         scene_name, e.what());
            ++skipped;
        }
        catch (...)
        {
            spdlog::warn("Scene '{}' threw an unknown exception; skipping.", scene_name);
            ++skipped;
        }
    }

    if (!args.metrics_out.empty()) {
        std::ofstream metrics_file(args.metrics_out);
        if (!metrics_file) {
            spdlog::error("Cannot write visual metrics to '{}'", args.metrics_out);
            ++skipped;
        } else {
            metrics_file << visual_metrics.dump(2) << "\n";
        }
    }

    spdlog::info("Done. Generated: {}  Skipped: {}", generated, skipped);

    // ---- cleanup ----------------------------------------------------------
    for (auto *plugin : pl->get_plugins()) plugin->pre_exit();
    wrappers.clear();
    preview_providers.clear();
    config->release_scene_references();
    pl->delete_references();
    pl->destroy_plugins();

    delete config;
    config = nullptr;

    delete matrix;

    if (args.strict && (skipped > 0 || attempted == 0))
        return 1;
    return (skipped > 0 && generated == 0) ? 1 : 0;
#endif
}
