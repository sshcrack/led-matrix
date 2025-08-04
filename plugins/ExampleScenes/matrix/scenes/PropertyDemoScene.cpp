#include "PropertyDemoScene.h"
#include <cmath>

using namespace Scenes;

bool PropertyDemoScene::render(rgb_matrix::RGBMatrixBase *matrix) {
    auto frame = frameTimer.tick();
    animation_time += frame.dt;
    
    // Clear canvas
    offscreen_canvas->Clear();
    
    int width = matrix->width();
    int height = matrix->height();
    
    // Demonstrate property usage
    if (boolean_demo->get()) {
        // Primary color animation when feature is enabled
        auto primary = color_demo->get();
        auto secondary = secondary_color->get();
        
        // Create animated pattern based on properties
        float pulse = (sin(animation_time * float_demo->get()) + 1.0f) / 2.0f;
        
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                // Use number_demo to create pattern density
                bool draw_pixel = ((x + y) % number_demo->get()) == 0;
                
                if (draw_pixel) {
                    // Interpolate between primary and secondary colors
                    uint8_t r = static_cast<uint8_t>(primary.r() * pulse + secondary.r() * (1.0f - pulse));
                    uint8_t g = static_cast<uint8_t>(primary.g() * pulse + secondary.g() * (1.0f - pulse));
                    uint8_t b = static_cast<uint8_t>(primary.b() * pulse + secondary.b() * (1.0f - pulse));
                    
                    offscreen_canvas->SetPixel(x, y, r, g, b);
                }
            }
        }
        
        // Draw a border using the string demo length
        int border_size = std::min(static_cast<int>(string_demo->get().length()) / 4, 3);
        auto border_color = secondary_color->get();
        
        for (int i = 0; i < border_size; i++) {
            // Top and bottom borders
            for (int x = 0; x < width; x++) {
                offscreen_canvas->SetPixel(x, i, border_color.r(), border_color.g(), border_color.b());
                offscreen_canvas->SetPixel(x, height - 1 - i, border_color.r(), border_color.g(), border_color.b());
            }
            
            // Left and right borders
            for (int y = 0; y < height; y++) {
                offscreen_canvas->SetPixel(i, y, border_color.r(), border_color.g(), border_color.b());
                offscreen_canvas->SetPixel(width - 1 - i, y, border_color.r(), border_color.g(), border_color.b());
            }
        }
    } else {
        // Show a simple gradient when feature is disabled
        auto base_color = color_demo->get();
        
        for (int y = 0; y < height; y++) {
            float intensity = static_cast<float>(y) / height;
            
            for (int x = 0; x < width; x++) {
                uint8_t r = static_cast<uint8_t>(base_color.r() * intensity);
                uint8_t g = static_cast<uint8_t>(base_color.g() * intensity);
                uint8_t b = static_cast<uint8_t>(base_color.b() * intensity);
                
                offscreen_canvas->SetPixel(x, y, r, g, b);
            }
        }
    }
    
    return true;
}

void PropertyDemoScene::register_properties() {
    add_property(number_demo);
    add_property(float_demo);
    add_property(boolean_demo);
    add_property(string_demo);
    add_property(color_demo);
    add_property(secondary_color);
    add_property(required_demo);
}

std::unique_ptr<Scene, void (*)(Scene *)> PropertyDemoSceneWrapper::create() {
    return {new PropertyDemoScene(), [](Scene *scene) {
        delete scene;
    }};
}