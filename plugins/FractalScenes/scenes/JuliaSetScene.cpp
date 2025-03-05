#include "JuliaSetScene.h"
#include <cmath>

using namespace Scenes;

JuliaSetScene::JuliaSetScene() : Scene() {
}

void JuliaSetScene::initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) {
    Scene::initialize(matrix, l_offscreen_canvas);
    last_update = std::chrono::steady_clock::now();
    total_time = 0.0f;
}

bool JuliaSetScene::render(RGBMatrixBase *matrix) {
    auto current_time = std::chrono::steady_clock::now();
    float delta_time = std::chrono::duration<float>(current_time - last_update).count();
    last_update = current_time;
    total_time += delta_time;
    
    // Calculate Julia set parameter based on time if animation is enabled
    if (animate_params->get()) {
        float t = total_time * move_speed->get();
        c = {-0.7f + 0.2f * std::sin(t * 0.3f), 0.27f + 0.1f * std::cos(t * 0.5f)};
    }
    
    int width = matrix->width();
    int height = matrix->height();
    
    // Clear the canvas
    offscreen_canvas->Clear();
    
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
                hsv_to_rgb(hue, 0.9f, 1.0f, r, g, b);
                
                offscreen_canvas->SetPixel(x, y, r, g, b);
            }
            // Pixels that exceed the max iterations remain black
        }
    }
    
    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas);
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

// Convert HSV to RGB color
void JuliaSetScene::hsv_to_rgb(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b) const {
    float c = v * s;
    float x = c * (1 - std::abs(std::fmod(h * 6, 2) - 1));
    float m = v - c;
    float r1, g1, b1;
    
    if (h < 1.0f/6.0f) { r1 = c; g1 = x; b1 = 0; }
    else if (h < 2.0f/6.0f) { r1 = x; g1 = c; b1 = 0; }
    else if (h < 3.0f/6.0f) { r1 = 0; g1 = c; b1 = x; }
    else if (h < 4.0f/6.0f) { r1 = 0; g1 = x; b1 = c; }
    else if (h < 5.0f/6.0f) { r1 = x; g1 = 0; b1 = c; }
    else { r1 = c; g1 = 0; b1 = x; }
    
    r = static_cast<uint8_t>((r1 + m) * 255);
    g = static_cast<uint8_t>((g1 + m) * 255);
    b = static_cast<uint8_t>((b1 + m) * 255);
}

std::unique_ptr<Scene, void (*)(Scene *)> JuliaSetSceneWrapper::create() {
    return {
        new JuliaSetScene(), [](Scene *scene) {
            delete dynamic_cast<JuliaSetScene*>(scene);
        }
    };
}
