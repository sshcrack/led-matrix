#pragma once

#include "Scene.h"
#include "plugin/main.h"
#include <vector>
#include <ctime>

namespace AmbientScenes {
class ClockScene : public Scenes::Scene {
private:
    PropertyPointer<bool> show_digital = MAKE_PROPERTY("show_digital", bool, true);
    PropertyPointer<bool> show_analog = MAKE_PROPERTY("show_analog", bool, true);
    PropertyPointer<bool> show_seconds = MAKE_PROPERTY("show_seconds", bool, true);
    PropertyPointer<bool> show_date = MAKE_PROPERTY("show_date", bool, false);
    PropertyPointer<uint8_t> hour_color_r = MAKE_PROPERTY("hour_color_r", uint8_t, 255);
    PropertyPointer<uint8_t> hour_color_g = MAKE_PROPERTY("hour_color_g", uint8_t, 0);
    PropertyPointer<uint8_t> hour_color_b = MAKE_PROPERTY("hour_color_b", uint8_t, 0);
    PropertyPointer<uint8_t> minute_color_r = MAKE_PROPERTY("minute_color_r", uint8_t, 0);
    PropertyPointer<uint8_t> minute_color_g = MAKE_PROPERTY("minute_color_g", uint8_t, 255);
    PropertyPointer<uint8_t> minute_color_b = MAKE_PROPERTY("minute_color_b", uint8_t, 0);
    PropertyPointer<uint8_t> second_color_r = MAKE_PROPERTY("second_color_r", uint8_t, 0);
    PropertyPointer<uint8_t> second_color_g = MAKE_PROPERTY("second_color_g", uint8_t, 0);
    PropertyPointer<uint8_t> second_color_b = MAKE_PROPERTY("second_color_b", uint8_t, 255);
    PropertyPointer<uint8_t> bg_color_r = MAKE_PROPERTY("bg_color_r", uint8_t, 0);
    PropertyPointer<uint8_t> bg_color_g = MAKE_PROPERTY("bg_color_g", uint8_t, 0);
    PropertyPointer<uint8_t> bg_color_b = MAKE_PROPERTY("bg_color_b", uint8_t, 10);
    PropertyPointer<bool> smooth_motion = MAKE_PROPERTY("smooth_motion", bool, true);
    PropertyPointer<uint8_t> clock_style = MAKE_PROPERTY("clock_style", uint8_t, 0); // 0=classic, 1=minimal, 2=elegant
    PropertyPointer<bool> use_glow_effect = MAKE_PROPERTY("use_glow_effect", bool, true);
    PropertyPointer<bool> use_antialiasing = MAKE_PROPERTY("use_antialiasing", bool, true);

    // For smooth transitions
    float current_hour;
    float current_minute;
    float current_second;
    int last_digit_update;

    // Clock face helpers
    void draw_analog_clock(rgb_matrix::RGBMatrixBase *matrix, int center_x, int center_y, int radius);
    void draw_digital_clock(rgb_matrix::RGBMatrixBase *matrix, int y_position);
    void draw_date(rgb_matrix::RGBMatrixBase *matrix, int y_position);
    void draw_clock_hand(rgb_matrix::RGBMatrixBase *matrix, int center_x, int center_y, 
                        float angle, int length, uint8_t r, uint8_t g, uint8_t b, int thickness);
    void draw_antialiased_line(rgb_matrix::RGBMatrixBase *matrix, int x0, int y0, int x1, int y1, 
                        uint8_t r, uint8_t g, uint8_t b, bool apply_glow = false);
    void draw_small_digit(rgb_matrix::RGBMatrixBase *matrix, int digit, int x, int y, 
                        uint8_t r, uint8_t g, uint8_t b);
    void set_pixel_with_brightness(int x, int y, uint8_t r, uint8_t g, uint8_t b, float brightness);
    void apply_glow(int x, int y, uint8_t r, uint8_t g, uint8_t b, float intensity);
    void draw_clock_face(rgb_matrix::RGBMatrixBase *matrix, int center_x, int center_y, int radius);

public:
    explicit ClockScene();
    ~ClockScene() override = default;

    bool render(rgb_matrix::RGBMatrixBase *matrix) override;
    void initialize(rgb_matrix::RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) override;
    [[nodiscard]] std::string get_name() const override;
    void register_properties() override;

    using Scene::Scene;
};

class ClockSceneWrapper : public Plugins::SceneWrapper {
    std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create();
};
}
