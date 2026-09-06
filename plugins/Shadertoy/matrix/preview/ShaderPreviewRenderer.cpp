#include "ShaderPreviewRenderer.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ShadertoyPreview {
namespace {

struct Config {
    int width = 128;
    int height = 128;
    int fps = 15;
    int total_frames = 90;
    float audio_bpm = 120.0f;
    std::string audio_profile = "balanced";
};

struct Cache {
    std::filesystem::path shader;
    Config config;
    std::vector<std::uint8_t> rgb_frames;
};

std::mutex state_mutex;
std::optional<Config> active_config;
std::optional<Cache> cache;
std::atomic<std::uint64_t> temp_counter{0};

std::string shell_quote(const std::string& value)
{
#ifdef _WIN32
    std::string quoted = "\"";
    for (const char c : value) {
        if (c == '"')
            quoted += "\\\"";
        else
            quoted += c;
    }
    quoted += '"';
    return quoted;
#else
    std::string quoted = "'";
    for (const char c : value) {
        if (c == '\'')
            quoted += "'\\''";
        else
            quoted += c;
    }
    quoted += '\'';
    return quoted;
#endif
}

std::filesystem::path executable_directory()
{
#if defined(__linux__)
    std::error_code ec;
    const auto executable = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec && executable.has_parent_path())
        return executable.parent_path();
#endif
    return std::filesystem::current_path();
}

std::filesystem::path worker_path()
{
    if (const char* override_path = std::getenv("SHADERTOY_PREVIEW_WORKER"); override_path != nullptr && *override_path != '\0')
        return override_path;

#ifdef _WIN32
    constexpr std::string_view worker_name = "shadertoy_scene_preview_worker.exe";
#else
    constexpr std::string_view worker_name = "shadertoy_scene_preview_worker";
#endif
    const auto next_to_generator = executable_directory() / worker_name;
    if (std::filesystem::exists(next_to_generator))
        return next_to_generator;

    const auto cwd_candidate = std::filesystem::current_path() / worker_name;
    if (std::filesystem::exists(cwd_candidate))
        return cwd_candidate;

    throw std::runtime_error("Shadertoy preview worker was not found next to preview_gen; rebuild the emulator target");
}

std::filesystem::path make_temp_output()
{
    const auto token = temp_counter.fetch_add(1, std::memory_order_relaxed);
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           ("led-matrix-shadertoy-preview-" + std::to_string(stamp) + "-" + std::to_string(token) + ".rgb");
}

bool config_matches(const Config& a, const Config& b)
{
    return a.width == b.width && a.height == b.height && a.fps == b.fps && a.total_frames == b.total_frames && a.audio_bpm == b.audio_bpm &&
           a.audio_profile == b.audio_profile;
}

std::vector<std::uint8_t> render_all_frames(const std::filesystem::path& shader, const Config& config)
{
    const auto worker = worker_path();
    const auto output = make_temp_output();
    struct TempCleanup {
        std::filesystem::path path;
        ~TempCleanup()
        {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    } cleanup{output};

    std::ostringstream command;
#ifndef _WIN32
    // preview_gen itself is headless. Give only the isolated GL worker a small
    // virtual X server when the caller has no display of its own.
    if (std::getenv("DISPLAY") == nullptr) {
        if (!std::filesystem::exists("/usr/bin/xvfb-run"))
            throw std::runtime_error("Shadertoy preview rendering needs a display or /usr/bin/xvfb-run on headless Linux");
        command << "/usr/bin/xvfb-run -a ";
    }
#endif
    command << shell_quote(worker.string()) << ' ' << shell_quote(shader.string()) << ' ' << shell_quote(output.string()) << ' '
            << config.width << ' ' << config.height << ' ' << config.total_frames << ' ' << config.fps << ' ' << config.audio_bpm << ' '
            << shell_quote(config.audio_profile);

    const int status = std::system(command.str().c_str());
    if (status != 0)
        throw std::runtime_error("Shadertoy preview worker failed for '" + shader.string() + "' with status " + std::to_string(status));

    std::ifstream input(output, std::ios::binary);
    if (!input)
        throw std::runtime_error("Shadertoy preview worker did not produce its RGB output");
    std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    const auto frame_bytes = static_cast<std::size_t>(config.width) * static_cast<std::size_t>(config.height) * 3U;
    const auto expected = frame_bytes * static_cast<std::size_t>(config.total_frames);
    if (bytes.size() != expected)
        throw std::runtime_error("Shadertoy preview worker returned " + std::to_string(bytes.size()) + " RGB bytes; expected " +
                                 std::to_string(expected));
    return bytes;
}

}  // namespace

void begin(const Previews::RunContext& context)
{
    std::lock_guard lock(state_mutex);
    Config next;
    next.width = std::max(1, context.width);
    next.height = std::max(1, context.height);
    next.fps = std::max(1, context.fps);
    next.total_frames = std::max(1, context.total_frames);
    const auto audio_options = context.options_for(Previews::Inputs::Audio);
    next.audio_bpm = std::clamp(audio_options.value("bpm", 120.0f), 40.0f, 240.0f);
    next.audio_profile = audio_options.value("profile", std::string("balanced"));
    if (next.audio_profile != "balanced" && next.audio_profile != "bass" && next.audio_profile != "percussion" &&
        next.audio_profile != "ambient")
        next.audio_profile = "balanced";
    active_config = std::move(next);
}

void end() noexcept
{
    std::lock_guard lock(state_mutex);
    active_config.reset();
    cache.reset();
}

bool render_shader(const std::filesystem::path& shader_path, rgb_matrix::FrameCanvas* canvas, const Scenes::SceneFrameContext& frame)
{
    if (canvas == nullptr)
        return false;

    std::lock_guard lock(state_mutex);
    if (!active_config)
        throw std::runtime_error("Shadertoy preview provider was not initialized");
    const auto& config = *active_config;
    if (canvas->width() != config.width || canvas->height() != config.height)
        throw std::runtime_error("Shadertoy preview canvas does not match preview-generator dimensions");

    std::error_code ec;
    auto normalized_shader = std::filesystem::absolute(shader_path, ec);
    if (ec)
        normalized_shader = shader_path;
    normalized_shader = normalized_shader.lexically_normal();

    if (!cache || cache->shader != normalized_shader || !config_matches(cache->config, config)) {
        cache = Cache{
            .shader = normalized_shader,
            .config = config,
            .rgb_frames = render_all_frames(normalized_shader, config),
        };
    }

    const auto frame_bytes = static_cast<std::size_t>(config.width) * static_cast<std::size_t>(config.height) * 3U;
    const auto requested = frame.frame_index > 0 ? frame.frame_index - 1 : 0;
    const auto frame_index = std::min<std::uint64_t>(requested, static_cast<std::uint64_t>(config.total_frames - 1));
    const auto* rgb = cache->rgb_frames.data() + static_cast<std::size_t>(frame_index) * frame_bytes;

    // renderToBuffer is OpenGL-bottom-up, matching the desktop plugin's UDP
    // payload. Mirror the production matrix path by flipping rows here.
    for (int source_y = 0; source_y < config.height; ++source_y) {
        const int target_y = config.height - 1 - source_y;
        for (int x = 0; x < config.width; ++x) {
            const auto index =
                (static_cast<std::size_t>(source_y) * static_cast<std::size_t>(config.width) + static_cast<std::size_t>(x)) * 3U;
            canvas->SetPixel(x, target_y, rgb[index], rgb[index + 1], rgb[index + 2]);
        }
    }
    return true;
}

}  // namespace ShadertoyPreview
