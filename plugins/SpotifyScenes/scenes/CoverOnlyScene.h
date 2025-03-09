#pragma once

#include <optional>
#include <chrono>
#include <deque>
#include "Scene.h"
#include "shared/utils/utils.h"
#include "../manager/state.h"
#include "wrappers.h"
#include <nlohmann/json.hpp>
#include "shared/utils/PropertyMacros.h"

namespace Scenes {
    struct SpotifyFileInfo {
        rgb_matrix::StreamIO *content_stream;
        int vsync_multiple;

        SpotifyFileInfo() : content_stream(nullptr), vsync_multiple(1) {}

        ~SpotifyFileInfo() {
            delete content_stream;
        }
    };

    class CoverOnlyScene final : public Scene {
    private:
        PropertyPointer<float> border_intensity_prop = MAKE_PROPERTY_MINMAX("border_intensity", float, 0.6f, 0.0f, 1.0f);
        PropertyPointer<bool> wait_on_cover = MAKE_PROPERTY("wait_on_cover", bool, true);
        PropertyPointer<tmillis_t> zoom_wait = MAKE_PROPERTY("zoom_wait", tmillis_t, 40);
        PropertyPointer<tmillis_t> cover_wait = MAKE_PROPERTY("cover_wait", tmillis_t, 1000);
        PropertyPointer<int> new_song_weight = MAKE_PROPERTY("weight_if_new_song", int, 100);
        PropertyPointer<float> zoom_factor = MAKE_PROPERTY("zoom_factor", float, 10.0f);

        bool DisplaySpotifySong(rgb_matrix::RGBMatrixBase *matrix);

        std::optional<std::unique_ptr<SpotifyFileInfo, void (*)(SpotifyFileInfo *)>> curr_info;
        std::optional<SpotifyState> curr_state;
        std::optional<rgb_matrix::StreamReader> curr_reader;
        std::optional<std::pair<std::string, float>> curr_bpm;

        std::expected<void, std::string> refresh_info(rgb_matrix::RGBMatrixBase *matrix);
        
        // Beat detection simulation
        std::chrono::time_point<std::chrono::steady_clock> last_beat_time;
        std::deque<float> beat_intervals;
        float current_beat_intensity = 0.0f;
        float target_beat_intensity = 0.0f;
        
        // Simulates a beat based on song progress and tempo
        void update_beat_simulation();
        
        // Returns the current beat intensity (0.0 to 1.0)
        float get_beat_intensity() const { return current_beat_intensity; }

    public:
        CoverOnlyScene() : Scene() {
            last_beat_time = std::chrono::steady_clock::now();
        }
        
        ~CoverOnlyScene() override = default;
        bool render(RGBMatrixBase *matrix) override;

        [[nodiscard]] int get_weight() const override;

        [[nodiscard]] string get_name() const override;

        void register_properties() override;
    };

    class CoverOnlySceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
