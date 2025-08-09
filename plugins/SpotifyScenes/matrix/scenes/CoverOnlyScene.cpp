#include "CoverOnlyScene.h"
#include "Magick++.h"
#include "spdlog/spdlog.h"
#include "../manager/shared_spotify.h"
#include "shared/matrix/utils/canvas_image.h"
#include "shared/matrix/utils/image_fetch.h"
#include "led-matrix.h"
#include <cmath>
#include <chrono>
#include <exception>
#include <shared_mutex>
#include <vector>

#include "../manager/song_bpm_getter.h"

using namespace spdlog;
using namespace std;
using namespace Scenes;

// Helper function to create a color based on progress
rgb_matrix::Color getProgressColor(float progress) {
    // Create a gradient from blue to purple to red
    if (progress < 0.33f) {
        // Blue to purple
        uint8_t r = 64 + 128 * (progress / 0.33f);
        uint8_t g = 0;
        uint8_t b = 255;
        return {r, g, b};
    }
    if (progress < 0.66f) {
        // Purple to red
        float adjusted = (progress - 0.33f) / 0.33f;
        uint8_t r = 192 + 63 * adjusted;
        uint8_t g = 0;
        uint8_t b = 255 - 255 * adjusted;
        return {r, g, b};
    }
    // Red to yellow
    float adjusted = (progress - 0.66f) / 0.34f;
    uint8_t r = 255;
    uint8_t g = 0 + 255 * adjusted;
    uint8_t b = 0;
    return {r, g, b};
}

// Helper function to draw a glowing border
void drawGlowingBorder(rgb_matrix::FrameCanvas *canvas, int x, int y, int width, int height,
                       const rgb_matrix::Color &color, float intensity) {
    // Draw the main border
    for (int i = x; i < x + width; i++) {
        canvas->SetPixel(i, y, color.r, color.g, color.b);
        canvas->SetPixel(i, y + height - 1, color.r, color.g, color.b);
    }

    for (int i = y; i < y + height; i++) {
        canvas->SetPixel(x, i, color.r, color.g, color.b);
        canvas->SetPixel(x + width - 1, i, color.r, color.g, color.b);
    }

    // Draw a softer glow (if there's enough space)
    if (width > 4 && height > 4) {
        rgb_matrix::Color dimColor(
            color.r * intensity,
            color.g * intensity,
            color.b * intensity);

        for (int i = x + 1; i < x + width - 1; i++) {
            canvas->SetPixel(i, y + 1, dimColor.r, dimColor.g, dimColor.b);
            canvas->SetPixel(i, y + height - 2, dimColor.r, dimColor.g, dimColor.b);
        }

        for (int i = y + 1; i < y + height - 1; i++) {
            canvas->SetPixel(x + 1, i, dimColor.r, dimColor.g, dimColor.b);
            canvas->SetPixel(x + width - 2, i, dimColor.r, dimColor.g, dimColor.b);
        }
    }
}

void CoverOnlyScene::update_beat_simulation() {
    // Get the current time
    auto current_time = std::chrono::steady_clock::now();

    // Calculate elapsed time since last update
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_beat_time).count();

    // If we have a BPM value, use it to simulate beats
    if (curr_bpm > 0) {
        float beat_interval_ms = 60000.0f / curr_bpm;

        // Calculate phase within the beat (0.0 to 1.0)
        float phase = (float) elapsed / beat_interval_ms;
        target_beat_intensity = std::max(0.0f, 1.0f - phase * 2.0f);
    }

    // Smooth the current intensity towards the target
    current_beat_intensity = current_beat_intensity * 0.7f + target_beat_intensity * 0.3f;
}

bool CoverOnlyScene::DisplaySpotifySong(rgb_matrix::RGBMatrixBase *matrix) {
    uint32_t delay_us = 0;

    bool has_anim = false; {
        std::shared_lock anim_lock(animation_mtx);
        if (curr_animation.has_value()) {
            has_anim = true;
            if (!curr_animation->GetNext(offscreen_canvas, &delay_us)) {
                // Try to rewind
                curr_animation->Rewind();
                // And get again, if fails, return
                if (!curr_animation->GetNext(offscreen_canvas, &delay_us)) {
                    trace("Returning, reader done");
                    return false;
                }
            }
        }
    }

    if (!has_anim) {
        std::shared_lock cover_lock(quick_cover_mtx);

        const int x_offset = (offscreen_canvas->width() - quick_cover->columns()) / 2;
        const int y_offset = (offscreen_canvas->height() - quick_cover->rows()) / 2;

        // Get direct access to pixel data
        const Magick::PixelPacket *pixels = quick_cover->getConstPixels(0, 0, quick_cover->columns(),
                                                                        quick_cover->rows());

        for (size_t y = 0; y < quick_cover->rows(); ++y) {
            const Magick::PixelPacket *row = pixels + (y * quick_cover->columns());
            for (size_t x = 0; x < quick_cover->columns(); ++x) {
                const auto &q = row[x];
                if (q.opacity != MaxRGB) {
                    // Check for non-transparent pixels
                    offscreen_canvas->SetPixel(x + x_offset, y + y_offset,
                                               ScaleQuantumToChar(q.red),
                                               ScaleQuantumToChar(q.green),
                                               ScaleQuantumToChar(q.blue));
                }
            }
        }
    }

    const tmillis_t start_wait_ms = GetTimeInMillis();

    auto progress_opt = curr_state->get_progress();
    if (!progress_opt.has_value()) {
        error("Could not get progress");
        return false;
    }

    // Update beat simulation
    update_beat_simulation();
    float beat_intensity = get_beat_intensity();

    auto progress = progress_opt.value();
    int max_x = matrix->width();
    int max_y = matrix->height();

    // Calculate pulsing effect based on progress and beat
    float pulse_intensity = 0.3f + 0.7f * beat_intensity;

    // Draw a glowing border around the entire image with beat-reactive intensity
    int border_margin = 1;
    rgb_matrix::Color border_color = getProgressColor(progress);

    // Enhance border color based on beat intensity
    border_color.r = std::min(255, (int) (border_color.r * (1.0f + beat_intensity * 0.5f)));
    border_color.g = std::min(255, (int) (border_color.g * (1.0f + beat_intensity * 0.5f)));
    border_color.b = std::min(255, (int) (border_color.b * (1.0f + beat_intensity * 0.5f)));

    /* I don't like the glowing border, disabled for now
        drawGlowingBorder(offscreen_canvas, border_margin, border_margin,
                          max_x - 2 * border_margin, max_y - 2 * border_margin,
                          border_color, pulse_intensity * border_intensity_prop->get());
    */

    // Draw progress indicators with a continuous gradient around the entire perimeter
    // Calculate total perimeter length
    int perimeter = 2 * max_x + 2 * max_y - 4; // -4 to account for corners

    // Calculate how many pixels to fill based on progress
    int pixels_to_fill = static_cast<int>(perimeter * progress);
    int pixels_filled = 0;


    // Draw a black border first
    for (int x = 0; x < max_x; x++) { offscreen_canvas->SetPixel(x, 0, 0, 0, 0); }
    for (int y = 1; y < max_y; y++) { offscreen_canvas->SetPixel(max_x - 1, y, 0, 0, 0); }
    for (int x = max_x - 2; x >= 0; x--) { offscreen_canvas->SetPixel(x, max_y - 1, 0, 0, 0); }
    for (int y = max_y - 2; y >= 1; y--) { offscreen_canvas->SetPixel(0, y, 0, 0, 0); }

    // Top edge (left to right)
    for (int x = 0; x < max_x && pixels_filled < pixels_to_fill; x++) {
        rgb_matrix::Color color = getProgressColor((float) pixels_filled / perimeter);
        offscreen_canvas->SetPixel(x, 0, color.r, color.g, color.b);
        pixels_filled++;
    }

    // Right edge (top to bottom)
    for (int y = 1; y < max_y && pixels_filled < pixels_to_fill; y++) {
        rgb_matrix::Color color = getProgressColor((float) pixels_filled / perimeter);
        offscreen_canvas->SetPixel(max_x - 1, y, color.r, color.g, color.b);
        pixels_filled++;
    }

    // Bottom edge (right to left)
    for (int x = max_x - 2; x >= 0 && pixels_filled < pixels_to_fill; x--) {
        rgb_matrix::Color color = getProgressColor((float) pixels_filled / perimeter);
        offscreen_canvas->SetPixel(x, max_y - 1, color.r, color.g, color.b);
        pixels_filled++;
    }

    // Left edge (bottom to top)
    for (int y = max_y - 2; y >= 1 && pixels_filled < pixels_to_fill; y--) {
        rgb_matrix::Color color = getProgressColor((float) pixels_filled / perimeter);
        offscreen_canvas->SetPixel(0, y, color.r, color.g, color.b);
        pixels_filled++;
    }


    wait_until_next_frame();

    return true;
}

bool CoverOnlyScene::render(RGBMatrixBase *matrix) {
    auto temp = spotify->get_currently_playing();
    if (!temp.has_value()) {
        spdlog::debug("Tried to render CoverOnlyScene, but no current track");
        return false;
    }

    auto track = std::move(temp.value());
    const auto track_id = track.get_track().get_id();
    if (!track_id.has_value()) {
        spdlog::debug("No track id, exiting");
        return false;
    }

    if (!curr_state.has_value() || curr_state->get_track().get_id().value() != track_id) {
        {
            std::unique_lock lock(state_mtx);
            curr_state.emplace(track);
        }

        refresh_future = std::async(launch::async,
                                    [this, matrix
                                    ]() -> std::expected<std::vector<std::pair<int64_t, Magick::Image> >, std::string> {
                                        return this->refresh_info(matrix->width(), matrix->height());
                                    });
    } else {
        std::unique_lock lock(state_mtx);
        curr_state.emplace(track);
    }

    if (refresh_future.valid() && refresh_future.wait_for(std::chrono::seconds(0)) == future_status::ready) {
        spdlog::trace("Future is ready");
        const auto res = refresh_future.get();
        if (!res.has_value()) {
            spdlog::error("Failed to refresh info: {}", res.error());
            return false;
        }

        auto images = std::move(res.value());
        if (images.empty()) {
            spdlog::debug("Exited refresh thread, waiting for new future");
            return true;
        }

        auto content_stream = new rgb_matrix::MemStreamIO();
        rgb_matrix::StreamWriter out(content_stream);

        for (auto pair: images) {
            StoreInStream(pair.second, pair.first, true, offscreen_canvas, &out);
        }

        spdlog::trace("Deleting curr content stream");
        if (curr_content_stream.has_value())
            delete curr_content_stream.value();

        std::unique_lock lock(animation_mtx);

        spdlog::trace("Constructing reader");
        curr_animation = rgb_matrix::StreamReader(content_stream);
        spdlog::trace("Setting stream");
        curr_content_stream = content_stream;
    }

    if (!quick_cover.has_value() && !curr_animation.has_value()) {
        SleepMillis(10);
        return true;
    }

    if(!curr_state->is_playing()) {
        SleepMillis(500);
        return true;
    }

    return DisplaySpotifySong(matrix);
}

std::expected<std::vector<std::pair<int64_t, Magick::Image> >, std::string> CoverOnlyScene::refresh_info(
    int width, int height) {
    // Verified previously that this must have a value

    std::shared_lock state_lock(state_mtx);
    auto track = curr_state->get_track();

    state_lock.unlock();

    auto opt_track = track.get_id();
    if (!opt_track.has_value()) {
        trace("No track id, exiting future");
        return {};
    }

    string track_id = opt_track.value();
    trace("New track, refreshing state: {}", track_id);

    auto cover_opt = track.get_cover();
    if (!cover_opt.has_value()) {
        return unexpected("No track cover for track '" + track_id + "'");
    }

    const auto &cover = cover_opt.value();
    string out_file = "/tmp/spotify_cover." + track_id + ".jpg";

    if (!std::filesystem::exists(out_file)) {
        const auto res = utils::download_image(cover, out_file);
        if (!res.has_value())
            return unexpected(res.error());
    }

    // Load the image with enhanced quality settings
    auto res = LoadImageAndScale(out_file, width, height, true, true,
                                 true);
    try_remove(out_file);
    if (!res) {
        return unexpected(res.error());
    }

    vector<Magick::Image> frames = std::move(res.value());
    Magick::Image cover_img(Magick::Geometry(width, height), Magick::Color("black"));

    // Apply a subtle enhancement to the cover
    Magick::Image enhanced_cover = frames[0];

    try {
        // Try to enhance the image with all three required parameters
        // modulate(brightness, saturation, hue)
        enhanced_cover.modulate(105.0, 110.0, 100.0);
        // Slightly increase brightness and saturation, keep hue unchanged
    } catch (const std::exception &e) {
        trace("Failed to modulate image: {}", e.what());
        // Just use the original cover if modulate fails
        enhanced_cover = frames[0];
    }

    cover_img.draw(Magick::DrawableCompositeImage(0, 0,
                                                  width,
                                                  height,
                                                  enhanced_cover));

    state_lock.lock();
    if (curr_state.has_value() && curr_state->get_track().get_id().value_or("") != track_id) {
        state_lock.unlock();
        spdlog::debug("New track detected, exiting");
        return {};
    }
    state_lock.unlock();

    //
    {
        std::unique_lock lock(animation_mtx);
        std::unique_lock lock2(quick_cover_mtx);

        this->quick_cover = std::move(cover_img);
        curr_animation = nullopt;
    }

    auto bpm_res = SongBpmApi::get_bpm(track.get_song_name().value_or(""), track.get_artist_name().value_or(""));
    if (!bpm_res.has_value())
        spdlog::error("Couldn't get bpm {}", bpm_res.error());


    curr_bpm = bpm_res.value_or(120);
    auto slowed_down = curr_bpm > beat_sync_slowdown_threshold->get()
                           ? curr_bpm / beat_sync_slowdown_factor->get()
                           : curr_bpm;

    // Fix: Calculate beat duration correctly (milliseconds per beat)
    float beat_duration_ms = 60000.0f / slowed_down;

    // Create a more interesting transition effect
    const int transition_steps = this->transition_steps->get();

    float single_img_duration_ms = beat_duration_ms / transition_steps;

    std::vector<std::pair<int64_t, Magick::Image> > track_images;
    // Then, create a zoom-in effect
    for (int i = 0; i < transition_steps; i++) {
        state_lock.lock();
        if (curr_state.has_value() && curr_state->get_track().get_id().value_or("") != track_id) {
            state_lock.unlock();
            spdlog::debug("New track detected, exiting");
            return {};
        }

        state_lock.unlock();
        float zoom = (float) i / (float) (transition_steps);

        Magick::Image img(Magick::Geometry(width, height), Magick::Color("black"));

        // Create a copy of the cover
        Magick::Image cover_copy = frames[0];

        // Calculate margins for the zoom effect
        int zoom_margin = (int) (width * zoom * zoom_factor->get());

        // Composite the cover onto the black background
        img.draw(Magick::DrawableCompositeImage(-zoom_margin, -zoom_margin,
                                                (width + zoom_margin * 2),
                                                (height + zoom_margin * 2),
                                                cover_copy));

        // Can't be zero, so it will be at least 1 to not disappear
        int size = std::max(width * zoom, 1.0f);
        int x = width / 2 - size / 2;
        int y = height / 2 - size / 2;


        img.draw(Magick::DrawableCompositeImage(x, y, size, size, cover_copy));

        // Store the frame with a short delay
        int64_t delay = sync_with_beat->get() ? single_img_duration_ms * 1000 : zoom_wait->get() * 1000;

        track_images.push_back(std::make_pair(delay, std::move(img)));
        // Simulate calculation
        SleepMillis(100);
    }

    if (wait_on_cover->get() && !sync_with_beat->get()) {
        track_images.push_back(std::make_pair(cover_wait->get() * 1000, std::move(cover_img)));
    }

    return track_images;
}

int CoverOnlyScene::get_weight() const {
    if (spotify != nullptr) {
        if (spotify->has_changed(false))
            return new_song_weight->get();

        if (spotify->get_currently_playing().has_value())
            return Scene::get_weight();
    }

    // Don't display this scene if no song is playing
    return 0;
}

string CoverOnlyScene::get_name() const {
    return "spotify";
}

std::unique_ptr<Scene, void (*)(Scene *)> CoverOnlySceneWrapper::create() {
    return {
        new CoverOnlyScene(), [](Scene *scene) {
            delete scene;
        }
    };
}

void CoverOnlyScene::register_properties() {
    add_property(border_intensity_prop);
    add_property(wait_on_cover);
    add_property(zoom_wait);
    add_property(cover_wait);
    add_property(new_song_weight);
    add_property(zoom_factor);
    add_property(sync_with_beat);
    add_property(beat_sync_slowdown_factor);
    add_property(beat_sync_slowdown_threshold);
    add_property(transition_steps);
}

CoverOnlyScene::~CoverOnlyScene() {
    spdlog::debug("Waiting for CoverOnlyScene to finish...");
    if (refresh_future.valid()) {
        refresh_future.wait();
    }

    curr_animation.reset(); // Ensure animation is cleaned up
    if (curr_content_stream.has_value())
        delete curr_content_stream.value();
}
