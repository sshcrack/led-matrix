#include "CoverOnlyScene.h"
#include "Magick++.h"
#include <spdlog/spdlog.h>
#include "../manager/shared_spotify.h"
#include "shared/utils/canvas_image.h"
#include "shared/utils/image_fetch.h"
#include "led-matrix.h"

using namespace spdlog;
using namespace std;
using namespace Scenes;

bool CoverOnlyScene::DisplaySpotifySong(rgb_matrix::RGBMatrix *matrix) {
    if (!curr_reader) {
        rgb_matrix::StreamReader temp(curr_info->content_stream);
        curr_reader.emplace(temp);
    }

    const tmillis_t duration_ms = (curr_info->wait_ms);
    const tmillis_t start_time = GetTimeInMillis();
    const tmillis_t end_time_ms = start_time + duration_ms;

    if (GetTimeInMillis() >= end_time_ms) {
        trace("Reached end time, returning");
        return false;
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

    auto progress = progress_opt.value() / 0.25f;
    auto top_prog = std::clamp(progress, 0.0f, 1.0f);
    auto right_prog = std::clamp(progress - 1.0f, 0.0f, 1.0f);
    auto bottom_prog = std::clamp(progress - 2.0f, 0.0f, 1.0f);
    auto left_prog = std::clamp(progress - 3.0f, 0.0f, 1.0f);

    int max_x = matrix->width();
    int max_y = matrix->height();
/*
    if (GetTimeInMillis() % 10 == 0) {
        trace("Progress: {} {} {} {} duration {} and prog {} ", top_prog, right_prog, bottom_prog, left_prog,
              curr_state->get_track().get_duration(), curr_state->get_progress_ms());
    }
*/
    if (top_prog > 0.0f)
        DrawLine(offscreen_canvas,
                 0, 0,
                 max_x * top_prog, 0,
                 rgb_matrix::Color(255, 255, 255)
        );

    if (right_prog > 0.0f)
        DrawLine(offscreen_canvas,
                 max_x - 1, 0,
                 max_x - 1, max_y * right_prog,
                 rgb_matrix::Color(255, 255, 255)
        );

    if (bottom_prog > 0.0f)
        DrawLine(offscreen_canvas,
                 max_x * (1.0f - bottom_prog), max_y - 1,
                 max_x, max_y - 1,
                 rgb_matrix::Color(255, 255, 255)
        );

    if (left_prog > 0.0f)
        DrawLine(offscreen_canvas,
                 0, max_y * (1.0f - left_prog),
                 0, max_y,
                 rgb_matrix::Color(255, 255, 255)
        );


    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas,
                                           curr_info->vsync_multiple);


    const tmillis_t time_already_spent = GetTimeInMillis() - start_wait_ms;
    if (time_already_spent > 100)
        return true;

    SleepMillis(100 - time_already_spent);
    return true;
}

bool CoverOnlyScene::render(rgb_matrix::RGBMatrix *matrix) {
    auto temp = this->refresh_info(matrix);
    if (!temp) {
        error("Could not get spotify cover image: '{}'", temp.error());
        return false;
    }

    return DisplaySpotifySong(matrix);
}

expected<void, string> CoverOnlyScene::refresh_info(rgb_matrix::RGBMatrix *matrix) {
    auto temp = spotify->get_currently_playing();
    if (!temp.has_value()) {
        return unexpected("Nothing currently playing");
    }

    if (!temp.value().is_playing() && false)
        return unexpected("Media is paused, skipping");

    if (curr_state.has_value() && curr_state->get_track().get_id() == temp.value().get_track().get_id()) {
        curr_state.emplace(temp.value());
        return {};
    }

    curr_state.emplace(temp.value());
    auto track_id_opt = temp.value().get_track().get_id();
    if(!track_id_opt.has_value())
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
        download_image(cover, out_file);
    }

    int margin = 2;
    auto res = LoadImageAndScale(out_file, matrix->width() - margin * 2, matrix->height() - margin * 2, true, true,
                                 false);
    if (!res) {
        try_remove(out_file);

        return unexpected(res.error());
    }

    vector<Magick::Image> frames = std::move(res.value());
    SpotifyFileInfo file_info = SpotifyFileInfo();

    file_info.wait_ms = 15000;
    file_info.content_stream = new rgb_matrix::MemStreamIO();


    rgb_matrix::StreamWriter out(file_info.content_stream);
    for (const auto &cover: frames) {
        Magick::Image img(Magick::Geometry(matrix->width(), matrix->height()), Magick::Color("black"));
        img.draw(Magick::DrawableCompositeImage(margin, margin, matrix->width() - margin, matrix->height() - margin,
                                                cover));

        StoreInStream(img, 100 * 1000, true, offscreen_canvas, &out);
    }

    curr_info.emplace(file_info);
    curr_reader = std::nullopt;
    return {};
}

int CoverOnlyScene::get_weight() const {
    if (spotify != nullptr) {
        if (spotify->has_changed(false))
            return 100;

        if (spotify->get_currently_playing().has_value())
            return Scene::get_weight();
    }

    // Don't display this scene if no song is playing
    return 0;
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
