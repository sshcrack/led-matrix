#include "JuliaSetScene.h"

#include <shared/matrix/utils/color.h>

#include <algorithm>
#include <cmath>

using namespace Scenes;

JuliaSetScene::JuliaSetScene() : Scene() {
}

void JuliaSetScene::initialize(int width, int height) {
    Scene::initialize(width, height);
}

bool JuliaSetScene::render(rgb_matrix::FrameCanvas *canvas) {
    // The parameter c walks along the edge of the Mandelbrot cardioid and
    // dips slightly inside and outside of it. Inside, the Julia set is one
    // connected shape; outside, it breaks into glittering dust. Sitting on
    // that edge keeps fine structure on screen at every moment, where the old
    // small orbit around (-0.7, 0.27) settled into a solid blob.
    if (animate_params->get()) {
        const float t = static_cast<float>(frame_context().elapsed_seconds) * move_speed->get();
        const float theta = 0.9f + t * 0.55f;
        const float edge = 1.0f + 0.03f * std::sin(t * 0.8f + 1.3f);
        const std::complex<float> boundary = {0.5f * std::cos(theta) - 0.25f * std::cos(2.0f * theta),
                                              0.5f * std::sin(theta) - 0.25f * std::sin(2.0f * theta)};
        c = boundary * edge;
    }

    const int width = matrix_width;
    const int height = matrix_height;
    const float zoom_factor = zoom->get();
    const int max_iter = max_iterations->get();
    const float shift = color_shift->get();
    const float hue_drift = static_cast<float>(frame_context().elapsed_seconds) * 0.004f;
    const float cr = c.real();
    const float ci = c.imag();
    const float aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
    // Escape radius 16 (squared 256) gives a smooth iteration count.
    constexpr float bailout = 256.0f;
    const float glow_span = std::max(8.0f, static_cast<float>(max_iter) * 0.3f);

    canvas->Clear();

    for (int y = 0; y < height; ++y) {
        const float imag = (2.0f * static_cast<float>(y) / static_cast<float>(height) - 1.0f) * 1.5f / zoom_factor;
        for (int x = 0; x < width; ++x) {
            const float real = (2.0f * static_cast<float>(x) / static_cast<float>(width) - 1.0f) * 1.5f * aspect_ratio / zoom_factor;
            float zr = real;
            float zi = imag;
            float trap = 1e9f;
            int iteration = 0;
            float norm = zr * zr + zi * zi;
            while (norm < bailout && iteration < max_iter) {
                const float next_r = zr * zr - zi * zi + cr;
                zi = 2.0f * zr * zi + ci;
                zr = next_r;
                norm = zr * zr + zi * zi;
                trap = std::min(trap, std::fabs(norm - 0.55f));
                ++iteration;
            }

            float hue;
            float sat;
            float val;
            if (iteration < max_iter) {
                // Smooth escape time: slow escapers sit on the set and glow;
                // quick escapers far away fade into the dark background.
                const float smoothed = static_cast<float>(iteration) + 1.0f - std::log2(0.5f * std::log2(std::max(norm, 1.0001f)));
                const float glow = std::clamp((smoothed - 3.0f) / glow_span, 0.0f, 1.0f);
                const float eased = glow * glow * (3.0f - 2.0f * glow);
                hue = 0.74f - 0.62f * std::sqrt(glow) + smoothed * 0.004f;
                sat = 0.95f - 0.25f * eased;
                val = 0.02f + 0.98f * eased;
            }
            else {
                // Inside the set: a dim circle-trap pattern rather than black.
                const float line = std::exp(-trap * 6.0f);
                hue = 0.80f + 0.1f * line;
                sat = 0.85f;
                val = 0.12f + 0.38f * line;
            }
            hue = std::fmod(hue + shift + hue_drift, 1.0f);
            if (hue < 0.0f)
                hue += 1.0f;
            uint8_t r, g, b;
            color::hsv_to_rgb(hue * 360.0f, sat, val, r, g, b);
            canvas->SetPixel(x, y, r, g, b);
        }
    }

    wait_until_next_frame();
    return true;
}

string JuliaSetScene::get_name() const {
    return "julia_set";
}

Scenes::SceneDescriptor JuliaSetScene::get_descriptor() const {
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "fractal";
    d.tags = {"ambient", "fractal", "organic", "texture", "evolving"};
    d.intensity = 0.46f; d.motion = 0.30f; d.music_affinity = 0.16f; d.performance_cost = 0.78f;
    d.variants = {
        {"drift", "Slow fractal drift", "Lower-cost slowly evolving Julia geometry",
         {{"zoom", 0.9f}, {"move_speed", 0.07f}, {"max_iterations", 56}, {"animate_params", true}},
         {"calm", "texture", "organic"}, 0.30f, 0.22f, 0.08f, 0.58f},
        {"vivid", "Vivid fractal", "Richer animated Julia detail for higher-headroom moments",
         {{"zoom", 1.05f}, {"move_speed", 0.16f}, {"max_iterations", 92}, {"animate_params", true}, {"color_shift", 0.16f}},
         {"vivid", "texture", "evolving"}, 0.58f, 0.40f, 0.18f, 0.76f},
    };
    Scenes::set_off_by_default(d, {"vivid"});
    return d;
}

void JuliaSetScene::register_properties() {
    add_property(zoom);
    add_property(move_speed);
    add_property(max_iterations);
    add_property(animate_params);
    add_property(color_shift);
}

std::unique_ptr<Scene> JuliaSetSceneWrapper::create() {
    return std::make_unique<JuliaSetScene>();
}
