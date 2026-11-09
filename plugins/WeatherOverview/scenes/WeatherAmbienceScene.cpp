#include "WeatherAmbienceScene.h"

#include <algorithm>
#include <cmath>

namespace Scenes {
namespace {
using Environment::Rgb;

float hill(float x, float seed)
{
    return 0.5f + 0.3f * std::sin(x * 0.041f + seed) + 0.15f * std::sin(x * 0.13f + seed * 2.1f);
}
} // namespace

WeatherAmbienceScene::WeatherAmbienceScene()
{
    set_target_fps(30);
    // Saved presets used "mode" (AUTO, CLEAR, ... STORM) for the same choice.
    environment_.weather_property().legacy_name("mode");
}

std::unique_ptr<Scenes::Scene> WeatherAmbienceSceneWrapper::create() { return std::make_unique<WeatherAmbienceScene>(); }

void WeatherAmbienceScene::register_properties()
{
    intensity_->label("Intensity").description("Density of rain and snow.").group("Atmosphere");
    animation_speed_->label("Animation speed").group("Motion").step(0.05);
    particle_limit_->label("Particle limit").description("Upper CPU budget for rain/snow particles. Adaptive quality may lower this automatically.").group("Performance");
    lightning_enabled_->label("Lightning").group("Atmosphere");
    for (const auto &p : environment_.properties()) add_property(p);
    add_property(intensity_);
    add_property(animation_speed_);
    add_property(particle_limit_);
    add_property(lightning_enabled_);
}

void WeatherAmbienceScene::set_random_seed(uint32_t seed)
{
    precip_.set_seed(seed);
    lightning_.set_seed(seed + 1);
    environment_.set_random_seed(seed + 2);
}

void WeatherAmbienceScene::initialize(int width, int height)
{
    Scene::initialize(width, height);
    px_.assign(static_cast<size_t>(width * height * 3), 0.0f);
    precip_.reset();
    environment_.snap();
    time_ = 0.0f;
}

bool WeatherAmbienceScene::render(rgb_matrix::FrameCanvas *canvas)
{
    const float dt = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.1f);
    time_ += dt * animation_speed_->get();
    const Environment::Look *look = &environment_.update(dt);
    // Lightning off means no flashes either, not only no bolts.
    Environment::Look calm;
    if (!lightning_enabled_->get() && (look->flash > 0.0f || look->bolt)) {
        calm = *look;
        calm.flash = 0.0f;
        calm.bolt = false;
        look = &calm;
    }
    const auto &L = *look;
    const Environment::Surface s{px_.data(), matrix_width, matrix_height, 1.0f};
    const float W = static_cast<float>(matrix_width), H = static_cast<float>(matrix_height), k = W / 128.0f;

    // Mostly sky, with a low line of hills to stand on.
    const float horizon = H * 0.84f;
    Environment::paint_sky(s, L, horizon);
    Environment::paint_stars(s, L, horizon * 0.9f, time_, 3, 120);
    Environment::paint_sun(s, L, Environment::sun_point(L, W, horizon), 9.0f * k);
    Environment::paint_moon(s, L, Environment::moon_point(L, W, horizon), 6.0f * k);
    Environment::paint_clouds(s, L, time_, H * 0.06f, H * 0.55f, 0.0f, 17);

    auto ground = [&](float x) { return H * 0.86f - hill(x, 1.3f) * 8.0f * k; };
    auto farHill = [&](float x) { return H * 0.80f - hill(x + 300.0f, 4.2f) * 9.0f * k; };
    const Rgb farCol = L.lit(L.ground(L.foliage({0.30f, 0.40f, 0.42f}, 0.5f), 0.8f), 0.7f);
    const Rgb nearCol = L.lit(L.ground(L.grass({0.20f, 0.32f, 0.18f}), 1.0f), 0.1f);
    for (int x = 0; x < matrix_width; ++x) {
        const float f = farHill(x + 0.5f), g = ground(x + 0.5f);
        // The top pixel of each ridge is blended by coverage: smooth hill lines.
        // Down to and including the near hill's blended top pixel, so no sky
        // shows through between the two.
        for (int y = std::max(0, static_cast<int>(f)); y <= std::min(matrix_height - 1, static_cast<int>(g)); ++y)
            s.blend(x, y, farCol, y == static_cast<int>(f) ? 1.0f - (f - std::floor(f)) : 1.0f);
        for (int y = std::max(0, static_cast<int>(g)); y < matrix_height; ++y)
            s.blend(x, y, Environment::mix(nearCol, nearCol * 0.6f, (y - g) / std::max(1.0f, H - g)),
                    y == static_cast<int>(g) ? 1.0f - (g - std::floor(g)) : 1.0f);
    }
    // A small tree on the far hills, then the fog, then a lone tree on the
    // near hill that shows the season and leans in the wind.
    Environment::paint_tree(s, L, W * 0.20f, farHill(W * 0.20f) + 1.0f, 15.0f * k, 3, time_, 0.6f);
    Environment::paint_fog(s, L, time_, H * 0.40f, H);
    Environment::paint_tree(s, L, W * 0.76f, ground(W * 0.76f) + 1.5f * k, 44.0f * k, 7, time_, 0.05f);

    const float quality = render_quality_scale();
    precip_.set_max(static_cast<int>(particle_limit_->get() * (0.40f + 0.60f * quality) * (0.4f + 0.1f * intensity_->get())));
    precip_.update(dt * animation_speed_->get(), L, matrix_width, matrix_height, ground);
    precip_.draw(s, L);
    if (lightning_enabled_->get())
        lightning_.update_and_draw(s, L, dt, horizon);

    for (int y = 0; y < matrix_height; ++y)
        for (int x = 0; x < matrix_width; ++x) {
            const float *p = s.at(x, y);
            canvas->SetPixel(x, y, static_cast<uint8_t>(std::clamp(p[0], 0.0f, 1.0f) * 255.0f),
                             static_cast<uint8_t>(std::clamp(p[1], 0.0f, 1.0f) * 255.0f),
                             static_cast<uint8_t>(std::clamp(p[2], 0.0f, 1.0f) * 255.0f));
        }
    wait_until_next_frame();
    return true;
}

SceneDescriptor WeatherAmbienceScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.family = "weather";
    d.tags = {"weather", "sky", "calm", "scenery"};
    d.intensity = 0.2f;
    d.motion = 0.3f;
    d.performance_cost = 0.25f;
    environment_.add_variants(d);
    return d;
}

} // namespace Scenes
