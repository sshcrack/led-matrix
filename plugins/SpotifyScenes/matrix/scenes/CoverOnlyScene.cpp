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
rgb_matrix::Color getProgressColor(float progress)
{
    // Create a gradient from blue to purple to red
    if (progress < 0.33f)
    {
        // Blue to purple
        uint8_t r = 64 + 128 * (progress / 0.33f);
        uint8_t g = 0;
        uint8_t b = 255;
        return {r, g, b};
    }
    if (progress < 0.66f)
    {
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
                       const rgb_matrix::Color &color, float intensity)
{
    // Draw the main border
    for (int i = x; i < x + width; i++)
    {
        canvas->SetPixel(i, y, color.r, color.g, color.b);
        canvas->SetPixel(i, y + height - 1, color.r, color.g, color.b);
    }

    for (int i = y; i < y + height; i++)
    {
        canvas->SetPixel(x, i, color.r, color.g, color.b);
        canvas->SetPixel(x + width - 1, i, color.r, color.g, color.b);
    }

    // Draw a softer glow (if there's enough space)
    if (width > 4 && height > 4)
    {
        rgb_matrix::Color dimColor(
            color.r * intensity,
            color.g * intensity,
            color.b * intensity);

        for (int i = x + 1; i < x + width - 1; i++)
        {
            canvas->SetPixel(i, y + 1, dimColor.r, dimColor.g, dimColor.b);
            canvas->SetPixel(i, y + height - 2, dimColor.r, dimColor.g, dimColor.b);
        }

        for (int i = y + 1; i < y + height - 1; i++)
        {
            canvas->SetPixel(x + 1, i, dimColor.r, dimColor.g, dimColor.b);
            canvas->SetPixel(x + width - 2, i, dimColor.r, dimColor.g, dimColor.b);
        }
    }
}

void CoverOnlyScene::update_beat_simulation()
{
    const auto now = std::chrono::steady_clock::now();
    const float elapsed_ms = std::chrono::duration<float, std::milli>(now - last_beat_time).count();
    const float interval_ms = 60000.0f / std::max(curr_bpm, 30.0f);
    const float phase = std::fmod(elapsed_ms, interval_ms) / interval_ms;
    const float attack = std::max(0.0f, 1.0f - phase * 5.5f);
    target_beat_intensity = attack * attack;
    current_beat_intensity += (target_beat_intensity - current_beat_intensity) * 0.28f;
}

bool CoverOnlyScene::DisplaySpotifySong(rgb_matrix::FrameCanvas *canvas)
{
    bool has_anim = false;
    {
        std::shared_lock anim_lock(animation_mtx);
        if (curr_animation.has_value() && !disable_cover_animation->get())
        {
            has_anim = true;
            uint32_t delay_us = 0;

            // Peek at the current frame to render it without consuming
            if (!curr_animation->PeekNext(canvas, &delay_us))
            {
                curr_animation->Rewind();
                if (!curr_animation->PeekNext(canvas, &delay_us))
                {
                    trace("Returning, reader done");
                    return false;
                }
                anim_frame_start_ms = 0;
            }

            // Record start time and delay on first peek after an advance (or at start)
            if (anim_frame_start_ms == 0)
            {
                anim_frame_start_ms = GetTimeInMillis();
                anim_frame_delay_ms = delay_us / 1000;
            }

            // Consume the frame and advance once its hold time has elapsed
            if (GetTimeInMillis() >= anim_frame_start_ms + anim_frame_delay_ms)
            {
                curr_animation->GetNext(canvas, &delay_us);
                anim_frame_start_ms = 0;
            }
        }
    }

    if (!has_anim)
    {
        std::shared_lock cover_lock(quick_cover_mtx);

        const int x_offset = (canvas->width() - quick_cover->columns()) / 2;
        const int y_offset = (canvas->height() - quick_cover->rows()) / 2;

        // Get direct access to pixel data
        const Magick::PixelPacket *pixels = quick_cover->getConstPixels(0, 0, quick_cover->columns(),
                                                                        quick_cover->rows());

        for (size_t y = 0; y < quick_cover->rows(); ++y)
        {
            const Magick::PixelPacket *row = pixels + (y * quick_cover->columns());
            for (size_t x = 0; x < quick_cover->columns(); ++x)
            {
                const auto &q = row[x];
                if (q.opacity != MaxRGB)
                {
                    // Check for non-transparent pixels
                    canvas->SetPixel(x + x_offset, y + y_offset,
                                               ScaleQuantumToChar(q.red),
                                               ScaleQuantumToChar(q.green),
                                               ScaleQuantumToChar(q.blue));
                }
            }
        }
    }

    auto progress_opt = curr_state->get_progress();
    if (!progress_opt.has_value()) {
        error("Could not get progress");
        return false;
    }

    update_beat_simulation();
    const float beat = get_beat_intensity();
    const float progress = std::clamp(progress_opt.value(), 0.0f, 1.0f);
    const int width = canvas->width();
    const int height = canvas->height();

    // A restrained beat-reactive halo keeps the artwork alive without obscuring it.
    const float halo = cover_border_glow_intensity->get() + beat * beat_pulse_strength->get();
    const auto accent = getProgressColor(progress);
    if (halo > 0.01f) {
        const uint8_t r = static_cast<uint8_t>(accent.r * std::min(halo, 1.0f));
        const uint8_t g = static_cast<uint8_t>(accent.g * std::min(halo, 1.0f));
        const uint8_t b = static_cast<uint8_t>(accent.b * std::min(halo, 1.0f));
        for (int x = 1; x < width - 1; ++x) {
            canvas->SetPixel(x, 1, r, g, b);
            canvas->SetPixel(x, height - 2, r, g, b);
        }
        for (int y = 2; y < height - 2; ++y) {
            canvas->SetPixel(1, y, r, g, b);
            canvas->SetPixel(width - 2, y, r, g, b);
        }
    }

    // A conventional bottom progress bar reads much better than a racing perimeter.
    if (show_progress->get()) {
        const int bar_height = std::clamp(progress_bar_height->get(), 1, std::max(1, height / 8));
        const int y0 = height - bar_height;
        const int filled = static_cast<int>(std::round(progress * width));
        for (int y = y0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (x < filled) {
                    const auto color = getProgressColor(width > 1 ? static_cast<float>(x) / (width - 1) : 0.0f);
                    const float highlight = y == y0 ? 1.0f : 0.72f;
                    canvas->SetPixel(x, y,
                        static_cast<uint8_t>(color.r * highlight),
                        static_cast<uint8_t>(color.g * highlight),
                        static_cast<uint8_t>(color.b * highlight));
                } else {
                    canvas->SetPixel(x, y, 3, 5, 9);
                }
            }
        }
    }

    wait_until_next_frame();

    return true;
}

bool CoverOnlyScene::render(rgb_matrix::FrameCanvas *canvas)
{
    auto temp = spotify->get_currently_playing();
    if (!temp.has_value())
    {
        spdlog::debug("Tried to render CoverOnlyScene, but no current track");
        return false;
    }

    auto track = std::move(temp.value());
    const auto track_id = track.get_track().get_id();
    if (!track_id.has_value())
    {
        spdlog::debug("No track id, exiting");
        return false;
    }

    if (!curr_state.has_value() || curr_state->get_track().get_id().value() != track_id)
    {
        if (refresh_future.valid() && refresh_future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            return true; // Previous fetch still in progress
        }

        {
            std::unique_lock lock(state_mtx);
            curr_state.emplace(track);
        }

        refresh_future = std::async(launch::async,
                                    [this]() -> std::expected<std::vector<std::pair<int64_t, Magick::Image>>, std::string>
                                    {
                                        return this->refresh_info(matrix_width, matrix_height);
                                    });
    }
    else
    {
        std::unique_lock lock(state_mtx);
        curr_state.emplace(track);
    }

    if (refresh_future.valid() && refresh_future.wait_for(std::chrono::seconds(0)) == future_status::ready)
    {
        spdlog::trace("Future is ready");
        const auto res = refresh_future.get();
        if (!res.has_value())
        {
            spdlog::error("Failed to refresh info: {}", res.error());
            return false;
        }

        auto images = std::move(res.value());
        if (images.empty())
        {
            spdlog::debug("Exited refresh thread, waiting for new future");
            return true;
        }

        auto content_stream = std::make_unique<rgb_matrix::MemStreamIO>();
        rgb_matrix::StreamWriter out(content_stream.get());

        for (auto pair : images)
        {
            StoreInStream(pair.second, pair.first, true, canvas, &out);
        }

        spdlog::trace("Deleting curr content stream");
        if (curr_content_stream.has_value())
            delete curr_content_stream.value();

        std::unique_lock lock(animation_mtx);

        spdlog::trace("Constructing reader");
        curr_animation = rgb_matrix::StreamReader(content_stream.get());
        anim_frame_start_ms = 0;
        spdlog::trace("Setting stream");
        curr_content_stream = content_stream.release();
    }

    if (!quick_cover.has_value() && !curr_animation.has_value())
    {
        return true;
    }

    if (!curr_state->is_playing())
    {
        return true;
    }

    wait_until_next_frame();
    return DisplaySpotifySong(canvas);
}

std::expected<std::vector<std::pair<int64_t, Magick::Image>>, std::string> CoverOnlyScene::refresh_info(
    int width, int height)
{
    // Verified previously that this must have a value

    std::shared_lock state_lock(state_mtx);
    auto track = curr_state->get_track();

    state_lock.unlock();

    auto opt_track = track.get_id();
    if (!opt_track.has_value())
    {
        trace("No track id, exiting future");
        return {};
    }

    string track_id = opt_track.value();
    trace("New track, refreshing state: {}", track_id);

    auto cover_opt = track.get_cover();
    if (!cover_opt.has_value())
    {
        return unexpected("No track cover for track '" + track_id + "'");
    }

    const auto &cover = cover_opt.value();
    string out_file = "/tmp/spotify_cover." + track_id + ".jpg";

    if (!std::filesystem::exists(out_file))
    {
        const auto res = utils::download_image(cover, out_file);
        if (!res.has_value())
            return unexpected(res.error());
    }

    // Load the image with enhanced quality settings
    auto res = LoadImageAndScale(out_file, width, height, true, true,
                                 true);
    try_remove(out_file);
    if (!res)
    {
        return unexpected(res.error());
    }

    vector<Magick::Image> frames = std::move(res.value());
    Magick::Image source = frames[0];
    Magick::Image cover_img(Magick::Geometry(width, height), Magick::Color("black"));

    try {
        // Fill the complete matrix with a dark, blurred version of the artwork.
        Magick::Image background = source;
        background.resize(Magick::Geometry(width, height, 0, 0));
        background.crop(Magick::Geometry(width, height,
            std::max(0L, static_cast<long>(background.columns() - width) / 2),
            std::max(0L, static_cast<long>(background.rows() - height) / 2)));
        if (background_blur->get() > 0)
            background.blur(0.0, static_cast<double>(background_blur->get()));
        background.modulate(static_cast<double>(background_brightness->get()), 112.0, 100.0);
        cover_img.composite(background, 0, 0, Magick::OverCompositeOp);

        // Put a crisp, slightly inset square cover above the atmospheric background.
        const int cover_size = std::clamp(
            std::min(width, height) * cover_size_percent->get() / 100,
            8, std::min(width, height));
        Magick::Image foreground = source;
        foreground.resize(Magick::Geometry(cover_size, cover_size));
        foreground.modulate(104.0, 112.0, 100.0);
        const int x = (width - cover_size) / 2;
        const int y = (height - cover_size) / 2 - std::max(0, progress_bar_height->get() / 2);

        // Two dark pixels of separation preserve the cover edge on bright artwork.
        Magick::Image shadow(Magick::Geometry(cover_size + 4, cover_size + 4), Magick::Color("#05070a"));
        cover_img.composite(shadow, x - 2, y - 2, Magick::OverCompositeOp);
        cover_img.composite(foreground, x, y, Magick::OverCompositeOp);
    } catch (const std::exception &e) {
        trace("Failed to compose Spotify artwork: {}", e.what());
        cover_img.draw(Magick::DrawableCompositeImage(0, 0, width, height, source));
    }

    state_lock.lock();
    if (curr_state.has_value() && curr_state->get_track().get_id().value_or("") != track_id)
    {
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
    auto slowed_down = curr_bpm > bpm_slowdown_threshold->get()
                           ? curr_bpm / bpm_slowdown_factor->get()
                           : curr_bpm;

    // Fix: Calculate beat duration correctly (milliseconds per beat)
    float beat_duration_ms = 60000.0f / slowed_down;

    // Create a more interesting transition effect
    const int transition_steps = this->cover_transition_steps->get();

    float single_img_duration_ms = beat_duration_ms / transition_steps;

    std::vector<std::pair<int64_t, Magick::Image>> track_images;
    // Then, create a zoom-in effect
    for (int i = 0; i < transition_steps; i++)
    {
        state_lock.lock();
        if (curr_state.has_value() && curr_state->get_track().get_id().value_or("") != track_id)
        {
            state_lock.unlock();
            spdlog::debug("New track detected, exiting");
            return {};
        }

        state_lock.unlock();
        float zoom = (float)i / (float)(transition_steps);

        Magick::Image img(Magick::Geometry(width, height), Magick::Color("black"));

        // Create a copy of the cover
        Magick::Image cover_copy = frames[0];

        // Calculate margins for the zoom effect
        int zoom_margin = (int)(width * zoom * cover_zoom_factor->get());

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
        int64_t delay = sync_transitions_with_beat->get() ? single_img_duration_ms * 1000 : zoom_transition_frame_wait->get() * 1000;

        track_images.push_back(std::make_pair(delay, std::move(img)));
    }

    if (wait_on_final_cover->get() && !sync_transitions_with_beat->get())
    {
        track_images.push_back(std::make_pair(final_cover_wait->get() * 1000, std::move(cover_img)));
    }

    return track_images;
}

int CoverOnlyScene::get_weight() const
{
    if (spotify != nullptr)
    {
        if (spotify->has_changed(false))
            return scene_weight_if_new_song->get();

        if (spotify->get_currently_playing().has_value())
            return Scene::get_weight();
    }

    // Don't display this scene if no song is playing
    return 0;
}

string CoverOnlyScene::get_name() const
{
    return "spotify";
}

std::unique_ptr<Scene> CoverOnlySceneWrapper::create()
{
    return std::make_unique<CoverOnlyScene>();
}

void CoverOnlyScene::register_properties()
{
    add_property(cover_border_glow_intensity);
    add_property(cover_size_percent);
    add_property(background_blur);
    add_property(background_brightness);
    add_property(progress_bar_height);
    add_property(beat_pulse_strength);
    add_property(show_progress);
    add_property(wait_on_final_cover);
    add_property(zoom_transition_frame_wait);
    add_property(final_cover_wait);
    add_property(scene_weight_if_new_song);
    add_property(cover_zoom_factor);
    add_property(sync_transitions_with_beat);
    add_property(bpm_slowdown_factor);
    add_property(bpm_slowdown_threshold);
    add_property(cover_transition_steps);
    add_property(disable_cover_animation);
}

CoverOnlyScene::~CoverOnlyScene()
{
    spdlog::info("Waiting for CoverOnlyScene to finish...");
    if (refresh_future.valid())
    {
        refresh_future.wait();
    }

    curr_animation.reset(); // Ensure animation is cleaned up
    if (curr_content_stream.has_value())
        delete curr_content_stream.value();
}
