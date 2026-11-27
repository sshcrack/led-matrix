#pragma once

// Nixie-tube clock: the local time as glowing orange numerals in glass tubes
// on a walnut plinth (the look of the desktop shader music_tube_amp, drawn
// natively so it runs on the Pi).
//
// Cost: everything that does not move (room, plinth, glass, mesh, unlit
// cathodes) and every numeral's glow are baked once per layout. A frame only
// recomposes the rectangles of tubes whose digit or glow changed, then blits
// one cached RGB buffer. See NixieArt.h for the baked layers.

#include <cstdint>
#include <ctime>
#include <vector>

#include "NixieArt.h"
#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"

namespace AmbientScenes {

enum class HourFormat { H24, H12 };
enum class ColonStyle { BLINK, STEADY, OFF };
enum class DigitChange { FADE, SLOT, FLICKER, INSTANT };

class ClockScene : public Scenes::Scene {
    PropertyPointer<Plugins::EnumProperty<HourFormat>> hour_format_ = MAKE_ENUM_PROPERTY("hour_format", HourFormat, HourFormat::H24);
    PropertyPointer<bool> leading_zero_ = MAKE_PROPERTY("leading_zero", bool, true);
    // Same key and meaning as the old clock's show_seconds, so saved presets keep it.
    PropertyPointer<bool> show_seconds_ = MAKE_PROPERTY("show_seconds", bool, false);
    PropertyPointer<Plugins::EnumProperty<ColonStyle>> colon_ = MAKE_ENUM_PROPERTY("colon", ColonStyle, ColonStyle::BLINK);
    PropertyPointer<Plugins::EnumProperty<DigitChange>> digit_change_ = MAKE_ENUM_PROPERTY("digit_change", DigitChange, DigitChange::FADE);
    PropertyPointer<bool> cathode_cycle_ = MAKE_PROPERTY("cathode_cycle", bool, true);
    PropertyPointer<bool> music_glow_ = MAKE_PROPERTY("music_glow", bool, true);

    struct Tube {
        int target = -2;           // digit the time asks for (-1 blank, -2 not set yet)
        int from = -1;             // digit shown before the change
        double changed_at = -1e9;  // scene time of the last change
        // What was last composed, to skip unchanged tubes.
        int shown_a = -3, shown_b = -3;
        float shown_mix = -1.0f, shown_heat = -1.0f;
        float kick = 0.0f;  // music flare, decays
    };

    Nixie::Art art_;
    std::vector<Tube> tubes_;
    bool baked_seconds_ = false;
    int baked_w_ = 0, baked_h_ = 0;

    double scene_time_ = 0.0;
    double cycle_start_ = -1e9;    // cathode cycle: scene start and the top of each hour
    bool cycle_from_dark_ = true;  // the start-up cycle begins with dark tubes
    int last_cycle_hour_ = -1;
    std::int64_t preview_wall_ms_ = -1;

    std::vector<Nixie::TubeLook> looks_;
    float shown_colon_ = -1.0f;
    bool force_full_ = true;

    // Music glow
    float live_ = 0.0f, bass_ = 0.0f, music_poll_ = 0.0f;
    std::uint64_t beat_seen_ = 0;
    std::size_t beat_index_ = 0;

    void rebake();
    [[nodiscard]] std::int64_t wall_ms();
    [[nodiscard]] std::vector<int> wanted_digits(const std::tm& t) const;
    void update_music(float dt);
    // The numeral pair and mix tube i shows now.
    void tube_look(int i, const Tube& tube, int& a, int& b, float& mix) const;

public:
    ClockScene();
    ~ClockScene() override = default;

    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void initialize(int width, int height) override;
    [[nodiscard]] std::string get_name() const override { return "clock"; }
    [[nodiscard]] std::string get_category() const override { return "Ambient"; }
    void register_properties() override;
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
    [[nodiscard]] Scenes::SceneCapabilities get_capabilities() const override;
    [[nodiscard]] bool supports_virtual_time() const override { return true; }
    // 2: nixie rewrite, new property set.
    [[nodiscard]] int remote_contract_version() const override { return 2; }

    tmillis_t get_default_duration() override { return 20000; }
    int get_default_weight() override { return 3; }
};

class ClockSceneWrapper : public Plugins::SceneWrapper {
    std::unique_ptr<Scenes::Scene> create() override;
};

}  // namespace AmbientScenes
