#include "WatermelonPlasmaScene.h"

#include <algorithm>
#include <cmath>

using AudioKit::RGB;

void Scenes::WatermelonPlasmaScene::register_properties()
{
    speed_->label("Speed").description("How fast the plasma flows.").group("Motion").step(0.05);
    look_palette_->label("Look palette").description("Original keeps the pink and green watermelon colours.").group("Appearance");
    look_brightness_->label("Look brightness").group("Appearance").step(0.05);
    add_property(speed_);
    add_property(look_palette_);
    add_property(look_brightness_);
}

void Scenes::WatermelonPlasmaScene::initialize(int width, int height)
{
    Scene::initialize(width, height);
    buf_.resize(width, height);
    time_ = 0.0f;
}

bool Scenes::WatermelonPlasmaScene::render(rgb_matrix::FrameCanvas *canvas) {
    // Slow enough for the larger forms to linger on an ambient panel.
    time_ += std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.1f) * speed_->get() * 0.22f;
    const float t = time_;
    const float w = static_cast<float>(matrix_width), h = static_cast<float>(matrix_height);

    // Per-frame terms of the field, hoisted out of the per-pixel function.
    const float zoom = 6.5f + 0.6f * std::sin(t * 0.18f);
    const float pan_x = std::sin(t * 0.25f) * 3.0f, pan_y = std::cos(t * 0.25f) * 3.0f;
    const float dir_x = std::sin(0.25f * t), dir_y = std::cos(0.29f * t);
    const float c1x = std::sin(t * 0.25f) * 4.0f, c1y = std::cos(t * 0.43f) * 4.0f;
    const float c2x = std::cos(t * 0.36f) * 6.0f, c2y = std::sin(t * 0.39f) * 5.3f;
    const auto palette = look_palette_->get().get();
    const float brightness = look_brightness_->get();

    buf_.fill({});
    buf_.add_field(0, matrix_height, AudioKit::field_step(render_quality_scale()), [&](float px, float py) -> RGB {
        const float xp = (px / w - 0.5f) * zoom + pan_x;
        const float yp = (py / h - 0.5f) * zoom + pan_y;
        const float v = std::sin(std::sin(dir_x * xp + dir_y * yp + t) + std::sin(AudioKit::len(xp + c1x, yp + c1y) + t) -
                                 std::cos(AudioKit::len(xp + c2x, yp + c2y) + t));
        const float su = std::cos(4.5f * v + 0.5f * xp + t) * 0.5f + 0.5f;
        const float sv = std::sin(4.5f * v + 0.5f * yp + t) * 0.5f + 0.5f;
        const float u = su * su, g = sv * sv;
        if (palette == Ambiance::Palette::ORIGINAL) {
            // One flowing field splits the panel into pink flesh and green rind of about equal share.
            // Each area is lightest along the seam and settles into a deep tone (plum, bottle green)
            // inside; the seam itself is a thin dark line, so the two hues never smear together.
            static constexpr RGB pink{1.0f, 0.30f, 0.45f}, plum{0.62f, 0.04f, 0.26f};
            static constexpr RGB green{0.40f, 0.95f, 0.32f}, bottle{0.04f, 0.42f, 0.18f};
            // A finer, weaker layer keeps several areas of each colour on the panel.
            const float side = std::sin(3.2f * v + 0.35f * (xp - yp) + t) + 0.35f * std::sin(1.7f * xp - 1.3f * yp + 0.7f * t) + 0.12f;
            const float a = std::abs(side);
            // The seam thickens and thins along its length instead of being an even outline.
            const float seam = AudioKit::smoothstep(0.02f, 0.10f + 0.10f * (0.5f + 0.5f * std::sin(2.0f * v + 0.6f * xp)), a);
            const float nearness = std::exp(-a * 2.0f);
            // A faint second ripple shows the flow inside each area.
            const float ripple = 0.85f + 0.15f * std::cos(5.0f * v - 0.4f * yp + 1.3f * t);
            const RGB hue = side < 0 ? AudioKit::lerp(plum, pink, nearness) : AudioKit::lerp(bottle, green, nearness);
            return hue * ((0.12f + 0.88f * seam) * ripple * 175.0f * brightness);
        }
        // Palette looks: the two bands become the palette's dark and light ends.
        // Dark gaps between the bands keep these looks restful.
        const auto c = Ambiance::sample(palette, 0.5f + 0.5f * (g - u), AudioKit::smoothstep(0.45f, 1.1f, (u + g) * 0.8f) * brightness);
        // Glowing cores where the bands overlap, in the palette's brightest colour.
        const auto hot = Ambiance::sample(palette, 1.0f, brightness);
        const float core = (u * g) * (u * g) * 0.9f;
        return RGB{static_cast<float>(c.r), static_cast<float>(c.g), static_cast<float>(c.b)} +
               RGB{static_cast<float>(hot.r), static_cast<float>(hot.g), static_cast<float>(hot.b)} * core;
    });
    buf_.blit(canvas);

    wait_until_next_frame();
    return true;
}

string Scenes::WatermelonPlasmaScene::get_name() const {
    return "watermelon_plasma";
}

Scenes::SceneDescriptor Scenes::WatermelonPlasmaScene::get_descriptor() const {
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "plasma";
    d.tags = {"ambient", "generative", "plasma", "flow", "vivid"};
    d.intensity = 0.58f;
    d.motion = 0.64f;
    d.music_affinity = 0.28f;
    d.performance_cost = 0.40f;
    d.variants = {
        {"slow", "Slow melon", "The watermelon plasma at half speed", {{"speed", 0.5f}}, {"calm", "plasma"}, 0.42f, 0.38f, 0.18f, 0.40f},
        {"ember",
         "Ember plasma",
         "Dim amber bands flowing slowly",
         {{"look_palette", "EMBER"}, {"look_brightness", 0.5f}, {"speed", 0.6f}},
         {"calm", "warm", "amber"},
         0.32f,
         0.36f,
         0.12f,
         0.40f},
        {"deep_sea",
         "Deep sea plasma",
         "Cool blue bands flowing slowly",
         {{"look_palette", "RAIN"}, {"look_brightness", 0.55f}, {"speed", 0.6f}},
         {"calm", "blue"},
         0.32f,
         0.36f,
         0.12f,
         0.40f},
        {"deep_space",
         "Indigo plasma",
         "Slow violet and indigo bands",
         {{"look_palette", "SPACE"}, {"look_brightness", 0.55f}, {"speed", 0.45f}},
         {"calm", "space", "indigo"},
         0.26f,
         0.30f,
         0.10f,
         0.40f},
    };
    Scenes::set_off_by_default(d, {"ember", "deep_sea", "deep_space"});
    return d;
}


std::unique_ptr<Scenes::Scene> Scenes::WatermelonPlasmaSceneWrapper::create() {
    return std::make_unique<WatermelonPlasmaScene>();
}
