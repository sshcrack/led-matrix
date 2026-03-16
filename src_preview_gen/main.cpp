/**
 * preview_gen: Generates animated GIF previews for all registered matrix scenes.
 *
 * Usage:
 *   preview_gen [--output <dir>] [--scene <name>] [--scenes <n1,n2,...>]
 *               [--frames <n>] [--fps <n>] [--width <n>] [--height <n>]
 *               [--dump-manifest] [--manifest-out <file>]
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

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>
#include <Magick++.h>
#include <nlohmann/json.hpp>

#ifdef ENABLE_EMULATOR
#include "emulator.h"
#include "matrix-factory.h"
#endif

#include "led-matrix.h"
#include "shared/matrix/plugin_loader/loader.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/canvas_consts.h"
#include "shared/matrix/utils/consts.h"

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Argument parsing helpers
// ---------------------------------------------------------------------------
static bool parse_int(const char *str, int &out)
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

struct Args
{
    std::string output_dir = "./previews";
    std::string filter_scene;            // legacy --scene (single scene)
    std::vector<std::string> filter_scenes; // --scenes (comma-separated list)
    int fps = 15;
    int total_frames = 90; // 6 seconds @ 15 fps
    int matrix_width = 128;
    int matrix_height = 128;
    bool dump_manifest = false;
    std::string manifest_out; // path for --manifest-out; empty = stdout
};

static Args parse_args(int argc, char *argv[])
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
        else if (std::string(argv[i]) == "--manifest-out" && i + 1 < argc)
            a.manifest_out = argv[++i];
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
    return a;
}

// ---------------------------------------------------------------------------
// Read all pixels from a FrameCanvas into a flat RGB byte vector
// ---------------------------------------------------------------------------
static std::vector<uint8_t> capture_canvas(rgb_matrix::FrameCanvas *canvas,
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
static Magick::Image make_frame(const std::vector<uint8_t> &rgb,
                                int w, int h,
                                size_t delay_centiseconds)
{
    Magick::Image img(Magick::Geometry(static_cast<size_t>(w),
                                       static_cast<size_t>(h)),
                      Magick::Color(0, 0, 0));
    img.modifyImage();

    Magick::PixelPacket *pixels =
        img.getPixels(0, 0, static_cast<size_t>(w), static_cast<size_t>(h));

    // Scale each 8-bit channel to the full Quantum range [0, MaxRGB].
    // MaxRGB is a GraphicsMagick compile-time constant (65535 for 16-bit depth).
    const size_t total = static_cast<size_t>(w * h);
    for (size_t i = 0; i < total; ++i)
    {
        using MagickLib::Quantum;
        pixels[i].red   = static_cast<Quantum>(
            static_cast<unsigned long>(rgb[i * 3])     * MaxRGB / 255UL);
        pixels[i].green = static_cast<Quantum>(
            static_cast<unsigned long>(rgb[i * 3 + 1]) * MaxRGB / 255UL);
        pixels[i].blue  = static_cast<Quantum>(
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
int main(int argc, char *argv[])
{
    spdlog::cfg::load_env_levels();

    const Args args = parse_args(argc, argv);

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

    rgb_matrix::EmulatorMatrix *matrix =
        rgb_matrix::EmulatorMatrix::Create(led_opts, emu_opts);
    if (!matrix)
    {
        spdlog::error("Failed to create headless emulator matrix.");
        return 1;
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
    spdlog::info("Loading plugins…");
    const auto pl = Plugins::PluginManager::instance();
    pl->initialize();

    const auto &wrappers = pl->get_scenes();
    if (wrappers.empty())
    {
        spdlog::warn("No scenes found. Make sure PLUGIN_DIR points to the "
                     "built plugins directory.");
    }

    // ---- dump-manifest mode: output scene→plugin mapping then exit --------
    if (args.dump_manifest)
    {
        nlohmann::json manifest = nlohmann::json::array();

        for (const auto &wrapper : wrappers)
        {
            const std::string scene_name = wrapper->get_name();
            const bool needs_desktop = wrapper->get_default()->needs_desktop_app();
            std::string plugin_name;
            std::string plugin_path;

            // Find which loaded plugin owns this scene wrapper
            for (const auto &pi : pl->get_plugins())
            {
                for (const auto &sw : pi->get_scenes())
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
                {"name",          scene_name},
                {"plugin_name",   plugin_name},
                {"plugin_path",   plugin_path},
                {"needs_desktop", needs_desktop},
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

        // Cleanup and exit without rendering
        pl->delete_references();
        pl->destroy_plugins();
        delete config;
        config = nullptr;
        delete matrix;
        return 0;
    }

    // ---- allocate a single render canvas ----------------------------------
    rgb_matrix::FrameCanvas *canvas = matrix->CreateFrameCanvas();
    canvas->Clear();

    // Timing constants
    const int frame_delay_ms = 1000 / args.fps;
    const size_t frame_delay_cs =
        static_cast<size_t>(std::max(1, 100 / args.fps)); // centiseconds

    int generated = 0;
    int skipped = 0;

    // ---- iterate scenes ---------------------------------------------------
    for (const auto &wrapper : wrappers)
    {
        const std::string scene_name = wrapper->get_name();

        // Skip scenes that require the desktop app - they cannot be rendered
        // headlessly and need a running desktop connection.  Use
        // scripts/capture_desktop_preview.sh to capture them manually.
        if (wrapper->get_default()->needs_desktop_app())
        {
            spdlog::info("Skipping '{}': requires desktop app (use capture_desktop_preview.sh).",
                         scene_name);
            continue;
        }

        // Apply scene filter (--scene or --scenes)
        if (!args.filter_scenes.empty())
        {
            bool found = false;
            for (const auto &f : args.filter_scenes)
                if (f == scene_name) { found = true; break; }
            if (!found)
                continue;
        }

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
            for (const auto &prop : scene->get_properties())
                prop->dump_to_json(default_props);

            scene->load_properties(default_props);
            scene->initialize(args.matrix_width, args.matrix_height);

            std::vector<Magick::Image> frames;
            frames.reserve(static_cast<size_t>(args.total_frames));

            for (int f = 0; f < args.total_frames; ++f)
            {
                // Sleep so that time-based animations (FrameTimer) advance at the
                // intended rate.  Most scenes use real wall-clock time, so without
                // this sleep the entire animation would appear as a single instant.
                std::this_thread::sleep_for(std::chrono::milliseconds(frame_delay_ms));

                canvas->Clear();
                const bool keep_going = scene->render(canvas);

                const auto rgb = capture_canvas(canvas, args.matrix_width,
                                                args.matrix_height);
                frames.push_back(make_frame(rgb, args.matrix_width,
                                            args.matrix_height, frame_delay_cs));

                if (!keep_going)
                {
                    spdlog::debug("Scene '{}' stopped at frame {}/{}", scene_name,
                                  f + 1, args.total_frames);
                    break;
                }
            }

            if (frames.empty())
            {
                spdlog::warn("No frames captured for '{}', skipping.", scene_name);
                ++skipped;
                continue;
            }

            // Quantise colours (required for GIF palette, 256 colours max)
            Magick::quantizeImages(frames.begin(), frames.end());

            const fs::path gif_path =
                fs::path(args.output_dir) / (scene_name + ".gif");

            Magick::writeImages(frames.begin(), frames.end(), gif_path.string());
            spdlog::info("Saved preview → {}", gif_path.string());
            ++generated;
        }
        catch (const std::exception &e)
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

    spdlog::info("Done. Generated: {}  Skipped: {}", generated, skipped);

    // ---- cleanup ----------------------------------------------------------
    pl->delete_references();
    pl->destroy_plugins();

    delete config;
    config = nullptr;

    delete matrix;

    return (skipped > 0 && generated == 0) ? 1 : 0;
#endif
}
