#pragma once

#include <optional>
#include "Scene.h"
#include "../../utils/utils.h"
#include "../../spotify/state.h"

namespace Scenes {
    struct SpotifyFileInfo {
        rgb_matrix::StreamIO *content_stream;
        tmillis_t wait_ms;
        int vsync_multiple;

        SpotifyFileInfo() : content_stream(nullptr), wait_ms(1500), vsync_multiple(1) {}
    };

    class SpotifyScene: public Scene {
    private:
        bool DisplaySpotifySong(RGBMatrix *matrix);
        std::optional<SpotifyFileInfo> curr_info;
        std::optional<SpotifyState> curr_state;
        std::optional<rgb_matrix::StreamReader> curr_reader;
        std::optional<SpotifyFileInfo> get_info(RGBMatrix *matrix);

    public:
        bool tick(RGBMatrix *matrix) override;
        using Scene::Scene;
    };

}
