#include "CoverOnlyScene.h"
#include "Magick++.h"
#include <spdlog/spdlog.h>
#include "../manager/shared_spotify.h"
#include "shared/utils/canvas_image.h"
#include "shared/utils/image_fetch.h"
#include "led-matrix.h"
#include <cmath>
#include <chrono>
#include <exception>
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
    if (curr_bpm && curr_bpm->second > 0) {
        float bpm = curr_bpm->second;
        float beat_interval_ms = 60000.0f / bpm;

        // Calculate phase within the beat (0.0 to 1.0)
        float phase = (float) elapsed / beat_interval_ms;
        target_beat_intensity = std::max(0.0f, 1.0f - phase * 2.0f);
    }

    // Smooth the current intensity towards the target
    current_beat_intensity = current_beat_intensity * 0.7f + target_beat_intensity * 0.3f;
}

bool CoverOnlyScene::perform_fade_transition(rgb_matrix::RGBMatrixBase *matrix) {
    if (!prev_frames.empty() && curr_info) {
        tmillis_t current_time = GetTimeInMillis();
        tmillis_t elapsed = current_time - fade_start_time;
        
        // Calculate fade progress (0.0 to 1.0)
        float fade_progress = std::min(1.0f, (float)elapsed / fade_duration->get());
        
        // Create a frame canvas for blending
        rgb_matrix::FrameCanvas* canvas = matrix->CreateFrameCanvas();
        
        // First display the previous frame
        auto& prev_img = prev_frames[0];
        Magick::Image blend(Magick::Geometry(matrix->width(), matrix->height()), Magick::Color("black"));
        
        // Get current frame from the new song
        uint32_t delay_us = 0;
        if (!curr_reader) {
            rgb_matrix::StreamReader temp(curr_info->get()->content_stream);
            curr_reader.emplace(temp);
        }
        
        // Get the first frame of the new cover
        if (!curr_reader->GetNext(offscreen_canvas, &delay_us)) {
            curr_reader->Rewind();
            if (!curr_reader->GetNext(offscreen_canvas, &delay_us)) {
                return false;
            }
        }
        
        // Create a temporary canvas to capture the current frame
        rgb_matrix::FrameCanvas* temp_canvas = matrix->CreateFrameCanvas();
        temp_canvas->CopyFrom(offscreen_canvas);
        
        // Draw the previous song frame with decreasing opacity
        Magick::Image prev_blend = prev_img;
        try {
            prev_blend.opacity((unsigned)(fade_progress * 65535)); // 0 = opaque, 65535 = transparent
        } catch (const std::exception &e) {
            trace("Failed to set opacity: {}", e.what());
        }
        
        StoreInCanvas(prev_blend, canvas);
        
        // Draw the new song frame with increasing opacity
        for (int y = 0; y < matrix->height(); y++) {
            for (int x = 0; x < matrix->width(); x++) {
                uint8_t r1, g1, b1;
                uint8_t r2, g2, b2;
                
                // Get color from previous frame
                canvas->GetPixel(x, y, &r1, &g1, &b1);
                
                // Get color from new frame
                temp_canvas->GetPixel(x, y, &r2, &g2, &b2);
                
                // Blend colors based on fade progress
                uint8_t r = r1 * (1.0f - fade_progress) + r2 * fade_progress;
                uint8_t g = g1 * (1.0f - fade_progress) + g2 * fade_progress;
                uint8_t b = b1 * (1.0f - fade_progress) + b2 * fade_progress;
                
                canvas->SetPixel(x, y, r, g, b);
            }
        }
        
        // Apply the progress indicator on top of the blended image
        auto progress_opt = curr_state->get_progress();
        if (progress_opt.has_value()) {
            float progress = progress_opt.value();
            int max_x = matrix->width();
            int max_y = matrix->height();
            
            // Draw progress indicators on the faded image (similar to DisplaySpotifySong)
            int perimeter = 2 * max_x + 2 * max_y - 4;
            int pixels_to_fill = static_cast<int>(perimeter * progress);
            int pixels_filled = 0;
            
            // Draw the progress indicators
            // Top edge (left to right)
            for (int x = 0; x < max_x && pixels_filled < pixels_to_fill; x++) {
                rgb_matrix::Color color = getProgressColor((float)pixels_filled / perimeter);
                canvas->SetPixel(x, 0, color.r, color.g, color.b);
                pixels_filled++;
            }
            
            // Right edge (top to bottom)
            for (int y = 1; y < max_y && pixels_filled < pixels_to_fill; y++) {
                rgb_matrix::Color color = getProgressColor((float)pixels_filled / perimeter);
                canvas->SetPixel(max_x - 1, y, color.r, color.g, color.b);
                pixels_filled++;
            }
            
            // Bottom edge (right to left)
            for (int x = max_x - 2; x >= 0 && pixels_filled < pixels_to_fill; x--) {
                rgb_matrix::Color color = getProgressColor((float)pixels_filled / perimeter);
                canvas->SetPixel(x, max_y - 1, color.r, color.g, color.b);
                pixels_filled++;
            }
            
            // Left edge (bottom to top)
            for (int y = max_y - 2; y >= 1 && pixels_filled < pixels_to_fill; y--) {
                rgb_matrix::Color color = getProgressColor((float)pixels_filled / perimeter);
                canvas->SetPixel(0, y, color.r, color.g, color.b);
                pixels_filled++;
            }
        }
        
        // Display the blended canvas
        canvas = matrix->SwapOnVSync(canvas);
        
        // If fade is complete, end the transition
        if (fade_progress >= 1.0f) {
            is_fading = false;
            prev_frames.clear();
            return true;
        }
        
        // Wait a bit to create a smooth transition
        SleepMillis(16); // ~60fps
        return true;
    }
    
    // If we don't have previous frames, end the transition
    is_fading = false;
    return false;
}

bool CoverOnlyScene::DisplaySpotifySong(rgb_matrix::RGBMatrixBase *matrix) {
    // If we're in a fade transition, handle it
    if (is_fading) {
        return perform_fade_transition(matrix);
    }

    if (!curr_reader) {
        rgb_matrix::StreamReader temp(curr_info->get()->content_stream);
        curr_reader.emplace(temp);
    }

    uint32_t delay_us = 0;

    // If we can't get the picture to canvas
    if (!curr_reader->GetNext(offscreen_canvas, &delay_us)) {
        // Try to rewind
        curr_reader->Rewind();
        // And get again, if fails, return
        if (!curr_reader->GetNext(offscreen_canvas, &delay_us)) {
            trace("Returning, reader done");
            return false;
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

    drawGlowingBorder(offscreen_canvas, border_margin, border_margin,
                      max_x - 2 * border_margin, max_y - 2 * border_margin,
                      border_color, pulse_intensity * border_intensity_prop->get());

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

    // Display the canvas
    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas);

    // Wait for the remaining time
    const tmillis_t delay_ms = delay_us / 1000;
    const tmillis_t elapsed_ms = GetTimeInMillis() - start_wait_ms;
    const tmillis_t wait_ms = delay_ms - std::min(elapsed_ms, delay_ms);

    SleepMillis(wait_ms);

    return true;
}

bool CoverOnlyScene::render(RGBMatrixBase *matrix) {
    auto temp = this->refresh_info(matrix);
    if (!temp) {
        error("Could not get spotify cover image: '{}'", temp.error());
        return false;
    }

    return DisplaySpotifySong(matrix);
}

expected<void, string> CoverOnlyScene::refresh_info(rgb_matrix::RGBMatrixBase *matrix) {
    auto temp = spotify->get_currently_playing();
    if (!temp.has_value()) {
        return unexpected("Nothing currently playing");
    }

    if (curr_state.has_value() && curr_state->get_track().get_id() == temp.value().get_track().get_id()) {
        curr_state.emplace(temp.value());
        return {};
    }

    // Store the previous track id and prepare for transition
    if (curr_state.has_value()) {
        auto prev_track_id_opt = curr_state->get_track().get_id();
        if (prev_track_id_opt.has_value()) {
            // Save the current cover for fading
            prev_track_id = prev_track_id_opt;
            
            // Store the current frame for fading
            if (!curr_info) {
                // If we don't have a current frame, we can't fade
                prev_frames.clear();
            } else {
                // Capture the current image for cross-fade
                prev_frames.clear();
                
                // Create a snapshot of the current display
                Magick::Image snapshot(Magick::Geometry(matrix->width(), matrix->height()), Magick::Color("black"));
                
                // If we have a valid reader and frame, use it
                if (curr_reader) {
                    uint32_t delay_us;
                    rgb_matrix::FrameCanvas* temp_canvas = matrix->CreateFrameCanvas();
                    if (curr_reader->GetNext(temp_canvas, &delay_us)) {
                        // Convert the canvas to a Magick image
                        for (int y = 0; y < matrix->height(); y++) {
                            for (int x = 0; x < matrix->width(); x++) {
                                uint8_t r, g, b;
                                temp_canvas->GetPixel(x, y, &r, &g, &b);
                                snapshot.pixelColor(x, y, Magick::ColorRGB(r/255.0, g/255.0, b/255.0));
                            }
                        }
                        prev_frames.push_back(snapshot);
                    }
                }
            }
            
            // Set fade start time to now
            fade_start_time = GetTimeInMillis();
            is_fading = true;
        }
    }

    curr_state.emplace(temp.value());
    auto track_id_opt = temp.value().get_track().get_id();
    if (!track_id_opt.has_value())
        return unexpected("No track id");

    string track_id = track_id_opt.value();
    trace("New track, refreshing state: {}", track_id);

    auto track = curr_state->get_track();
    auto temp2 = track.get_cover();
    if (!temp2.has_value()) {
        return unexpected("No track cover for track '" + track_id + "'");
    }

    const auto &cover = temp2.value();
    string out_file = "/tmp/spotify_cover." + track_id + ".jpg";

    if (!std::filesystem::exists(out_file)) {
        const auto res = utils::download_image(cover, out_file);
        if (!res.has_value())
            return unexpected(res.error());
    }

    // Load the image with enhanced quality settings
    auto res = LoadImageAndScale(out_file, matrix->width(), matrix->height(), true, true,
                                 true);
    try_remove(out_file);
    if (!res) {
        return unexpected(res.error());
    }

    vector<Magick::Image> frames = std::move(res.value());
    auto file_info = new SpotifyFileInfo();

    // Increase display time for better user experience
    file_info->content_stream = new rgb_matrix::MemStreamIO();

    rgb_matrix::StreamWriter out(file_info->content_stream);

    // Create a more interesting transition effect
    const int transition_steps = 50;


    // Then, create a zoom-in effect
    for (int i = 0; i < transition_steps; i++) {
        float zoom = (float) i / (float) (transition_steps);

        Magick::Image img(Magick::Geometry(matrix->width(), matrix->height()), Magick::Color("black"));

        // Create a copy of the cover
        Magick::Image cover_copy = frames[0];

        // Calculate margins for the zoom effect
        int zoom_margin = (int) (matrix->width() * zoom * zoom_factor->get());

        // Composite the cover onto the black background
        img.draw(Magick::DrawableCompositeImage(-zoom_margin, -zoom_margin,
                                                (matrix->width() + zoom_margin * 2),
                                                (matrix->height() + zoom_margin * 2),
                                                cover_copy));

        // Can't be zero, so it will be at least 1 to not disappear
        int size = std::max(matrix->width() * zoom, 1.0f);
        int x = matrix->width() / 2 - size / 2;
        int y = matrix->height() / 2 - size / 2;


        img.draw(Magick::DrawableCompositeImage(x, y, size, size, cover_copy));

        // Store the frame with a short delay
        StoreInStream(img, zoom_wait->get() * 1000, true, offscreen_canvas, &out);
    }

    // Then add the main cover frames
    for (const auto &cover: frames) {
        if (!wait_on_cover->get())
            break;

        Magick::Image img(Magick::Geometry(matrix->width(), matrix->height()), Magick::Color("black"));

        // Apply a subtle enhancement to the cover
        Magick::Image enhanced_cover = cover;

        try {
            // Try to enhance the image with all three required parameters
            // modulate(brightness, saturation, hue)
            enhanced_cover.modulate(105.0, 110.0, 100.0);
            // Slightly increase brightness and saturation, keep hue unchanged
        } catch (const std::exception &e) {
            trace("Failed to modulate image: {}", e.what());
            // Just use the original cover if modulate fails
            enhanced_cover = cover;
        }

        img.draw(Magick::DrawableCompositeImage(0, 0,
                                                matrix->width(),
                                                matrix->height(),
                                                enhanced_cover));

        StoreInStream(img, cover_wait->get() * 1000, true, offscreen_canvas, &out);
    }

    curr_info.emplace(std::move(
        std::unique_ptr<SpotifyFileInfo, void (*)(SpotifyFileInfo *)>(file_info, [](SpotifyFileInfo *info) {
            delete info;
        })));

    curr_reader = std::nullopt;
    return {};
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
    add_property(fade_duration);
}
