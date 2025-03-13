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
        PropertyPointer<bool> sync_with_beat = MAKE_PROPERTY("sync_with_beat", bool, true);
        PropertyPointer<float> beat_sync_slowdown_factor = MAKE_PROPERTY("beat_sync_slowdown_factor", float, 1.0f);
        PropertyPointer<float> beat_sync_slowdown_threshold = MAKE_PROPERTY("beat_sync_slowdown_threshold", float, 110.0f);

        bool DisplaySpotifySong(rgb_matrix::RGBMatrixBase *matrix);

        std::optional<std::unique_ptr<SpotifyFileInfo, void (*)(SpotifyFileInfo *)>> curr_info;
        std::optional<SpotifyState> curr_state;
        std::optional<rgb_matrix::StreamReader> curr_reader;
        float curr_bpm = 120.0f;

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


        tmillis_t get_default_duration() override {
            return 25000;
        }

        int get_default_weight() override {
            return 3;
        }
    };

    class CoverOnlySceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
