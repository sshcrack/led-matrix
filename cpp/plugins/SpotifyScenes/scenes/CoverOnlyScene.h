#pragma once

#include <optional>
#include "Scene.h"
#include "shared/utils/utils.h"
#include "../manager/state.h"
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
        bool DisplaySpotifySong(rgb_matrix::RGBMatrix *matrix);

        std::optional<SpotifyFileInfo> curr_info;
        std::optional<SpotifyState> curr_state;
        std::optional<rgb_matrix::StreamReader> curr_reader;

        std::expected<void, std::string> refresh_info(rgb_matrix::RGBMatrix *matrix);

    public:
        bool render(rgb_matrix::RGBMatrix *matrix) override;

        [[nodiscard]] int get_weight() const override;

        [[nodiscard]] string get_name() const override;

        void register_properties() override {};

        using Scene::Scene;
    };

    class CoverOnlySceneWrapper : public Plugins::SceneWrapper {
        Scenes::Scene *create() override;
    };
}
