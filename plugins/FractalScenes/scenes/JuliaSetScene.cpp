#include "JuliaSetScene.h"
#include <shared/matrix/utils/color.h>
#include <cmath>

using namespace Scenes;

JuliaSetScene::JuliaSetScene() : Scene() {
}

void JuliaSetScene::initialize(int width, int height) {
    Scene::initialize(width, height);
}

bool JuliaSetScene::render(rgb_matrix::FrameCanvas *canvas) {
    // Calculate Julia set parameter from the canonical scene clock so previews,
    // local rendering and desktop offload stay visually in sync.
    if (animate_params->get()) {
        const float t = static_cast<float>(frame_context().elapsed_seconds) * move_speed->get();
        c = {-0.7f + 0.2f * std::sin(t * 0.3f), 0.27f + 0.1f * std::cos(t * 0.5f)};
    }
    
    int width = matrix_width;
    int height = matrix_height;
    
    // Clear the canvas
    canvas->Clear();
    
    float aspect_ratio = static_cast<float>(width) / height;
    
    // For each pixel in the matrix
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Map pixel coordinates to the complex plane
            float zoom_factor = zoom->get();
            
            // Scale coordinates to the complex plane with zoom centered at (0, 0)
            float real = (2.0f * x / width - 1.0f) * 1.5f * aspect_ratio / zoom_factor;
            float imag = (2.0f * y / height - 1.0f) * 1.5f / zoom_factor;
            
            // Create complex number z = real + imag*i
            std::complex<float> z(real, imag);
            
            // Julia set iteration
            int iteration = 0;
            int max_iter = max_iterations->get();
            while (std::abs(z) < 2.0f && iteration < max_iter) {
                z = z * z + c;
                iteration++;
            }
            
            // Smooth coloring
            float smoothed = iteration + 1 - std::log2(std::log2(std::abs(z)));
            smoothed = smoothed < 0 ? 0 : smoothed;
            
            // Calculate color based on iteration count
            if (iteration < max_iter) {
                uint8_t r, g, b;
                // Map iteration count to color
                float hue = std::fmod(smoothed * 0.01f + color_shift->get(), 1.0f) * 360.0f;
                color::hsv_to_rgb(hue, 0.9f, 1.0f, r, g, b);
                
                canvas->SetPixel(x, y, r, g, b);
            }
            // Pixels that exceed the max iterations remain black
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
