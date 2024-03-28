#pragma once
#include <optional>
#include "Scene.h"
#include "content-streamer.h"
#include "../../utils/utils.h"


struct SpotifyFileInfo {
    rgb_matrix::StreamIO *content_stream;
    tmillis_t wait_ms;
};

class SpotifyScene: public Scene {
private:
    void DisplaySpotifySong();
    std::optional<SpotifyFileInfo> curr_info;
    void get_info(RGBMatrix *matrix);
public:
    void tick(RGBMatrix *matrix) override;
};
