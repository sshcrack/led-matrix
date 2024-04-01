#pragma once

#include <optional>
#include "Scene.h"
#include "shared/utils/utils.h"
#include "spotify/state.h"
#include "wrappers.h"

namespace Scenes {
    struct SpotifyFileInfo {
        rgb_matrix::StreamIO *content_stream;
        tmillis_t wait_ms;
        int vsync_multiple;

        SpotifyFileInfo() : content_stream(nullptr), wait_ms(1500), vsync_multiple(1) {}
    };

    class CoverOnlyScene : public Scene {
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

    class CoverOnlySceneWrapper : public Plugins::SceneWrapper {
        string get_name() override;
        Scenes::Scene * create_default() override;
        Scenes::Scene * from_json(const nlohmann::json &args) override;
    };
}
