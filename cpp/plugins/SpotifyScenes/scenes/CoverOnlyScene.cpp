#include "CoverOnlyScene.h"
#include "Magick++.h"
#include <spdlog/spdlog.h>
#include "shared/spotify/shared_spotify.h"
#include "shared/utils/canvas_image.h"
#include "shared/utils/image_fetch.h"
#include "led-matrix.h"

using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrix;
using namespace spdlog;
using namespace std;
using namespace Scenes;

bool CoverOnlyScene::DisplaySpotifySong(RGBMatrix *matrix) {
    if (!curr_reader) {
        rgb_matrix::StreamReader temp(curr_info->content_stream);
        curr_reader.emplace(temp);
    }

    const tmillis_t duration_ms = (curr_info->wait_ms);


    long progress_ms = curr_state->get_progress_ms();
    long duration = curr_state->get_track().get_duration();
    float flash_duration = 5000;

    int w = matrix->width() - 1;
    int h = matrix->height() - 1;

    const tmillis_t start_time = GetTimeInMillis();

    const tmillis_t end_time_ms = GetTimeInMillis() + duration_ms;
    if (GetTimeInMillis() >= end_time_ms) {
        trace("Reached end time, returning");
        return true;
    }

    uint32_t delay_us = 0;

    // If we can't get the picture to canvas
    if (!curr_reader->GetNext(offscreen_canvas, &delay_us)) {
        // Try to rewind
        curr_reader->Rewind();
        // And get again, if fails, return
        if (!curr_reader->GetNext(offscreen_canvas, &delay_us)) {
            trace("Returning, reader done");
            return true;
        }
    }


    const tmillis_t start_wait_ms = GetTimeInMillis();

    tmillis_t time_spent = GetTimeInMillis() - start_time;
    tmillis_t track_loc = progress_ms + time_spent;
    if (track_loc > duration) {
        trace("Track end reached, returning!");
        return true;
    }

    double brightness = abs(sin(M_PI * ((float) time_spent / flash_duration)));
    uint8_t p_brightness = brightness * 255;

    auto color = rgb_matrix::Color(0, p_brightness, 0);

    // Bottom / Top
    DrawLine(offscreen_canvas, 0, 0, w, 0, color);
    DrawLine(offscreen_canvas, 0, h, w, h, color);


    // Left / Right
    DrawLine(offscreen_canvas, 0, 0, 0, h, color);
    DrawLine(offscreen_canvas, w, 0, w, h, color);



    float progress = (float) track_loc / (float) duration;
    int line_width = matrix->width() * progress;


    DrawLine(offscreen_canvas, 0, 0, line_width, 0, rgb_matrix::Color(255, 255, 255));
    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas,
                                           curr_info->vsync_multiple);


    const tmillis_t time_already_spent = GetTimeInMillis() - start_wait_ms;
    if (time_already_spent > 100)
        return false;

    SleepMillis(100 - time_already_spent);
    return false;
}

bool CoverOnlyScene::tick(RGBMatrix *matrix) {
    if (!curr_info.has_value()) {
        auto temp = this->get_info(matrix);
        if (!temp) {
            error("Could not get spotify cover image: '{}'", temp.error());
            return true;
        }

        curr_info.emplace(temp.value());
    }

    return DisplaySpotifySong(matrix);
}

expected<SpotifyFileInfo, string> CoverOnlyScene::get_info(RGBMatrix *matrix) {
    auto temp = spotify->get_currently_playing();
    if (!temp.has_value()) {
        return unexpected("Nothing currently playing");
    }
    curr_state.emplace(temp.value());

    auto track = curr_state->get_track();
    auto temp2 = track.get_cover();
    if (!temp2.has_value()) {
        return unexpected("No track cover for track '" + track.get_id() + "'");
    }


    const auto& cover = temp2.value();
    string out_file = "/tmp/spotify_cover." + track.get_id() + ".jpg";

    if (!std::filesystem::exists(out_file)) {
        download_image(cover, out_file);
    }

    auto res = LoadImageAndScale(out_file, matrix->width(), matrix->height(), true, true, false);
    if (!res) {
        try_remove(out_file);

        return unexpected(res.error());
    }

    vector<Magick::Image> frames = std::move(res.value());
    SpotifyFileInfo file_info = SpotifyFileInfo();

    file_info.wait_ms = 15000;
    file_info.content_stream = new rgb_matrix::MemStreamIO();


    rgb_matrix::StreamWriter out(file_info.content_stream);
    for (const auto &img: frames) {
        StoreInStream(img, 100 * 1000, true, offscreen_canvas, &out);
    }

    return file_info;
}

int CoverOnlyScene::get_weight() const {
    if (spotify != nullptr && spotify->has_changed(false)) {
        return 100;
    }

    return Scene::get_weight();
}

string CoverOnlyScene::get_name() const {
    return "spotify";
}

Scenes::Scene *CoverOnlySceneWrapper::create_default() {
    return new CoverOnlyScene(Scene::create_default(3, 10 * 1000));
}

Scenes::Scene *CoverOnlySceneWrapper::from_json(const json &args) {
    return new CoverOnlyScene(args);
}
