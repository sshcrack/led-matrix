#include "JuliaSetScene.h"
#include <shared/matrix/utils/color.h>
#include <cmath>

using namespace Scenes;

JuliaSetScene::JuliaSetScene() : Scene() {
}

void JuliaSetScene::initialize(int width, int height) {
    Scene::initialize(width, height);
    last_update = std::chrono::steady_clock::now();
    total_time = 0.0f;
}

bool JuliaSetScene::render(rgb_matrix::FrameCanvas *canvas) {
    auto current_time = std::chrono::steady_clock::now();
    float delta_time = std::chrono::duration<float>(current_time - last_update).count();
    last_update = current_time;
    total_time += delta_time;
    
    // Calculate Julia set parameter based on time if animation is enabled
    if (animate_params->get()) {
        float t = total_time * move_speed->get();
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
                float hue = std::fmod(smoothed * 0.01f + color_shift->get(), 1.0f);
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
