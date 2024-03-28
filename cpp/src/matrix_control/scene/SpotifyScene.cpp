#include "SpotifyScene.h"
#include "Magick++.h"
#include <spdlog/spdlog.h>
#include "../../spotify/shared_spotify.h"
#include "../image.h"
#include "../pixel_art.h"
#include "content-streamer.h"
#include "led-matrix.h"

using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrix;
using namespace spdlog;
using namespace std;


void SpotifyScene::DisplaySpotifySong() {

}

void SpotifyScene::tick(RGBMatrix *matrix) {

}

void SpotifyScene::get_info(RGBMatrix *matrix) {
    info("Showing spotify song change");
    auto temp = spotify->get_currently_playing();
    if (!temp.has_value()) {
        return;
    }

    auto state = temp.value();
    auto temp2 = state.get_track().get_cover();
    if (!temp2.has_value()) {
        return;
    }


    auto cover = temp2.value();
    string out_file = "/tmp/spotify_cover." + state.get_track().get_id() + ".jpg";

    if (!std::filesystem::exists(out_file)) {
        debug("Downloading");
        download_image(cover, out_file);
    }

    vector<Magick::Image> frames;
    string err_msg;

    LoadImageAndScale(out_file, matrix->width(), matrix->height(), true, true, false, &frames, &err_msg);
    if (!err_msg.empty()) {
        error("Error loading image: {}", err_msg);
        return;
    }

    SpotifyFileInfo file_info = SpotifyFileInfo();
    file_info.wait_ms = 15000;
    file_info.content_stream = new rgb_matrix::MemStreamIO();


    debug("Showing and storing");
    rgb_matrix::StreamWriter out(file_info.content_stream);
    for (const auto &img: frames) {
        StoreInStream(img, 100 * 1000, true, offscreen_canvas, &out);
    }

    curr_info.emplace(file_info);
}