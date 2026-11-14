#include "RenderRuntime.h"

#include <shared/common/crash_reporter.h>
#include <shared/common/remote_render_protocol.h>
#include <shared/matrix/audio_state.h>
#include <shared/matrix/media_artwork_state.h>
#include <shared/matrix/runtime_inputs.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace SceneWorker {

std::vector<std::uint8_t> capture(rgb_matrix::FrameCanvas* canvas, int width, int height)
{
    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(width) * height * 3U);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            std::uint8_t r = 0, g = 0, b = 0;
            canvas->GetPixel(x, y, &r, &g, &b);
            const auto offset = static_cast<std::size_t>((y * width + x) * 3);
            rgb[offset] = r;
            rgb[offset + 1] = g;
            rgb[offset + 2] = b;
        }
    }
    return rgb;
}

/// Last relayed audio frame applied, so repeated relays of the same frame do
/// not renew its freshness. Only the worker's command loop touches it.
namespace {
std::optional<std::uint32_t> applied_audio_sequence;
}

void apply_audio(const nlohmann::json& item)
{
    if (!item.is_object() || !item.value("available", false)) {
        AudioState::clear();
        applied_audio_sequence.reset();
        return;
    }
    // The Pi keeps relaying its last audio frame after the desktop stream
    // stops. Applying it again would make stale features look fresh; leave the
    // local frame to age instead.
    if (!item.value("fresh", true))
        return;
    const auto sequence = item.value("sequence", 0U);
    if (applied_audio_sequence == sequence)
        return;
    applied_audio_sequence = sequence;
    AudioProtocol::Frame frame;
    frame.sequence = sequence;
    frame.timestamp_ms = item.value("timestamp_ms", 0U);
    frame.flags = item.value("flags", static_cast<std::uint16_t>(0));
    frame.beat_counter = item.value("beat_counter", std::uint64_t{0});
    frame.onset_counter = item.value("onset_counter", std::uint64_t{0});
    frame.drop_counter = item.value("drop_counter", std::uint64_t{0});
    frame.section_counter = item.value("section_counter", std::uint64_t{0});
    if (const auto it = item.find("features"); it != item.end() && it->is_array()) {
        std::size_t i = 0;
        for (const auto& feature : *it) {
            if (i >= frame.features.size())
                break;
            if (feature.is_number())
                frame.features[i] = feature.get<float>();
            ++i;
        }
    }
    if (const auto it = item.find("spectrum"); it != item.end() && it->is_array())
        frame.spectrum = it->get<std::vector<float>>();
    if (const auto it = item.find("waveform"); it != item.end() && it->is_array())
        frame.waveform = it->get<std::vector<float>>();
    // Older matrices send no age; then the relay time is the best estimate.
    const double age_seconds = std::clamp(item.value("age_seconds", 0.0), 0.0, 60.0);
    AudioState::update(frame, RenderRuntime::Clock::now() -
                                  std::chrono::duration_cast<RenderRuntime::Clock::duration>(std::chrono::duration<double>(age_seconds)));
}

RenderRuntime::RenderRuntime(TransitionManager& transition_manager, TimeSource* time_source)
    : transitions(transition_manager),
      clock(time_source ? time_source : &wall_clock),
      composer(&transition_manager, TransitionComposer::OutgoingPolicy::KeepMoving)
{
}

void RenderRuntime::remember_sent_frame(const std::vector<std::uint8_t>& rgb)
{
    SentFrame frame;
    if (sent_frames.size() >= SentFrameHistory) {
        frame = std::move(sent_frames.front());  // reuse the oldest buffer
        sent_frames.pop_front();
    }
    frame.session = session;
    frame.sequence = sequence;
    frame.rgb.assign(rgb.begin(), rgb.end());
    sent_frames.push_back(std::move(frame));
}

const std::vector<std::uint8_t>* RenderRuntime::find_sent_frame(const nlohmann::json& reference) const
{
    const auto wanted_session = reference.value("session", 0U);
    const auto wanted_sequence = reference.value("sequence", 0U);
    const SentFrame* best = nullptr;
    for (const auto& frame : sent_frames) {
        if (frame.session != wanted_session)
            continue;
        if (!best || frame.sequence <= wanted_sequence)
            best = &frame;
        if (frame.sequence >= wanted_sequence)
            break;
    }
    return best ? &best->rgb : nullptr;
}

void RenderRuntime::drop_outgoing()
{
    if (outgoing_scene) {
        try {
            outgoing_scene->after_render_stop();
        }
        catch (...) {
        }
    }
    outgoing_scene.reset();
    outgoing_canvas = nullptr;
    delete outgoing_matrix;
    outgoing_matrix = nullptr;
}

void RenderRuntime::render_outgoing()
{
    if (!outgoing_scene)
        return;
    if (!composer.blending() || !transition_from) {
        drop_outgoing();
        return;
    }
    bool keep = false;
    try {
        keep = outgoing_scene->render_frame(outgoing_canvas, std::nullopt, true);
        if (keep && outgoing_scene->frame_was_updated())
            composer.refresh_outgoing(*outgoing_canvas);
    }
    catch (const std::exception& e) {
        spdlog::warn("Outgoing scene '{}' failed during the transition: {}", outgoing_scene->get_name(), e.what());
        keep = false;
    }
    if (!keep)
        drop_outgoing();
}

rgb_matrix::FrameCanvas* RenderRuntime::output_frame()
{
    if (!composer.active())
        return canvas;
    const auto frame = composer.frame(transition_output, canvas, scene->frame_was_updated(), clock->now_ms());
    return frame.done ? canvas : transition_output;
}

void RenderRuntime::reset()
{
    drop_outgoing();
    if (scene) {
        try {
            scene->after_render_stop();
        }
        catch (...) {
        }
    }
    scene.reset();
    composer.end();
    transition_from = transition_output = nullptr;
    canvas = nullptr;
    delete matrix;
    matrix = nullptr;
    session = sequence = 0;
    width = height = 0;
    CrashReporter::set_activity("scene worker idle");
}

void RenderRuntime::apply_state(const nlohmann::json& command)
{
    if (const auto it = command.find("target_fps"); it != command.end()) {
        const int fps = std::clamp(it->get<int>(), 1, 60);
        if (fps != target_fps) {
            target_fps = fps;
            if (scene)
                scene->set_runtime_target_fps(fps);
            // Discard the old rate's deadline; preserve animation state.
            next_frame = Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / fps));
        }
    }
    if (const auto it = command.find("inputs"); it != command.end())
        RuntimeInputs::replace_from_json(*it);
    if (const auto it = command.find("audio"); it != command.end())
        apply_audio(*it);
    if (const auto it = command.find("artwork"); it != command.end())
        MediaArtworkState::replace_from_json(*it);
}

void RenderRuntime::start(const nlohmann::json& command)
{
    const auto started_at = Clock::now();
    // Resolve a referenced outgoing frame before reset() ends its session.
    std::vector<std::uint8_t> referenced;
    bool has_reference = false;
    // When the reference is this worker's running scene, keep that scene
    // alive (same size) so it keeps moving during the blend.
    std::unique_ptr<Scenes::Scene> live_scene;
    rgb_matrix::EmulatorMatrix* live_matrix = nullptr;
    rgb_matrix::FrameCanvas* live_canvas = nullptr;
    if (const auto it = command.find("transition"); it != command.end() && it->is_object())
        if (const auto ref = it->find("from_frame"); ref != it->end() && ref->is_object()) {
            has_reference = true;
            if (const auto* frame = find_sent_frame(*ref))
                referenced = *frame;
            if (!referenced.empty() && scene && matrix && session != 0 && ref->value("session", 0U) == session &&
                command.value("width", 128) == width && command.value("height", 128) == height) {
                live_scene = std::move(scene);
                live_matrix = std::exchange(matrix, nullptr);
                live_canvas = std::exchange(canvas, nullptr);
            }
        }
    sent_frames.clear();
    reset();
    outgoing_scene = std::move(live_scene);
    outgoing_matrix = live_matrix;
    outgoing_canvas = live_canvas;
    session = command.value("session", 0U);
    width = std::clamp(command.value("width", 128), 1, 1024);
    height = std::clamp(command.value("height", 128), 1, 1024);
    target_fps = std::clamp(command.value("target_fps", 60), 1, 60);
    if (session == 0)
        throw std::runtime_error("remote session id is zero");

    scene = Scenes::Scene::from_remote_command(command);

    rgb_matrix::RGBMatrix::Options led_options;
    led_options.rows = height;
    led_options.cols = width;
    led_options.chain_length = 1;
    led_options.parallel = 1;
    rgb_matrix::EmulatorOptions emulator_options;
    emulator_options.headless = true;
    emulator_options.refresh_rate_hz = target_fps;
    matrix = rgb_matrix::EmulatorMatrix::Create(led_options, emulator_options);
    if (!matrix)
        throw std::runtime_error("failed to create headless scene-worker canvas");
    canvas = matrix->CreateFrameCanvas();
    canvas->Clear();

    if (const auto it = command.find("inputs"); it != command.end())
        RuntimeInputs::replace_from_json(*it);
    if (const auto it = command.find("audio"); it != command.end())
        apply_audio(*it);
    if (const auto it = command.find("artwork"); it != command.end())
        MediaArtworkState::replace_from_json(*it);

    scene->initialize(width, height);
    scene->set_runtime_target_fps(target_fps);
    scene->restore_frame_timeline(command.value("elapsed_seconds", 0.0));
    if (const auto state = command.find("runtime_state"); state != command.end())
        scene->restore_runtime_state(*state);
    if (const auto it = command.find("transition"); it != command.end()) {
        std::vector<std::uint8_t> rgb;
        if (has_reference) {
            if (referenced.empty())
                throw std::runtime_error("referenced transition frame is no longer available");
            rgb = std::move(referenced);
        }
        else if (const auto encoded = it->find("from_base64"); encoded != it->end()) {
            auto decoded = RemoteRenderProtocol::decode_base64(encoded->get_ref<const std::string&>());
            if (!decoded)
                throw std::runtime_error("invalid base64 transition snapshot");
            rgb = std::move(*decoded);
        }
        else {
            rgb = it->at("from").get<std::vector<std::uint8_t>>();
        }
        if (rgb.size() != static_cast<std::size_t>(width * height * 3))
            throw std::runtime_error("invalid transition snapshot size");
        const int duration_ms = std::clamp(it->at("duration_ms").get<int>(), 1, 10000);
        transition_from = matrix->CreateFrameCanvas();
        transition_output = matrix->CreateFrameCanvas();
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                const auto offset = static_cast<std::size_t>((y * width + x) * 3);
                transition_from->SetPixel(x, y, rgb[offset], rgb[offset + 1], rgb[offset + 2]);
            }
        if (!composer.begin(transition_from, it->at("name").get<std::string>(), duration_ms, width, height, clock->now_ms()))
            throw std::runtime_error("worker has no transition effect");
    }
    sequence = 0;
    next_frame = Clock::now();
    CrashReporter::set_activity(std::string("offloading scene '") + scene->get_name() + "' session " + std::to_string(session));
    spdlog::info("Remote scene '{}' started (session {}, {}x{} @ {} FPS) in {:.1f} ms", scene->get_name(), session, width, height,
                 target_fps, std::chrono::duration<double, std::milli>(Clock::now() - started_at).count());
}

}  // namespace SceneWorker
