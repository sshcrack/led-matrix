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
    
    // Get enum values for demonstration
    AnimationMode anim_mode = animation_mode->get().get();
    DisplayPattern pattern = display_pattern->get().get();
    
    // Demonstrate property usage
    if (boolean_demo->get()) {
        // Primary color animation when feature is enabled
        auto primary = color_demo->get();
        auto secondary = secondary_color->get();
        
        // Calculate animation factor based on animation mode
        float anim_factor = 1.0f;
        switch (anim_mode) {
            case AnimationMode::STATIC:
                anim_factor = 1.0f;
                break;
            case AnimationMode::FADE:
                anim_factor = (sin(animation_time * 2.0f) + 1.0f) / 2.0f;
                break;
            case AnimationMode::PULSE:
                anim_factor = (sin(animation_time * 4.0f * float_demo->get()) + 1.0f) / 2.0f;
                break;
            case AnimationMode::RAINBOW:
                anim_factor = fmod(animation_time * 0.5f, 1.0f);
                break;
            case AnimationMode::BOUNCE:
                anim_factor = abs(sin(animation_time * 3.0f));
                break;
        }
        
        // Render based on display pattern
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                bool draw_pixel = false;
                float color_intensity = 1.0f;
                
                switch (pattern) {
                    case DisplayPattern::SOLID:
                        draw_pixel = true;
                        break;
                    case DisplayPattern::STRIPES:
                        draw_pixel = (y % number_demo->get()) == 0;
                        break;
                    case DisplayPattern::CHECKERBOARD:
                        draw_pixel = ((x + y) % (number_demo->get() * 2)) < number_demo->get();
                        break;
                    case DisplayPattern::GRADIENT:
                        draw_pixel = true;
                        color_intensity = static_cast<float>(y) / height;
                        break;
                    case DisplayPattern::SPIRAL:
                        {
                            float cx = width / 2.0f;
                            float cy = height / 2.0f;
                            float dx = x - cx;
                            float dy = y - cy;
                            float angle = atan2(dy, dx) + animation_time;
                            float dist = sqrt(dx*dx + dy*dy);
                            draw_pixel = fmod(angle + dist * 0.2f, 3.14159f * 2.0f / number_demo->get()) < 1.0f;
                        }
                        break;
                }
                
                if (draw_pixel) {
                    // Apply animation and color interpolation
                    uint8_t r, g, b;
                    
                    if (anim_mode == AnimationMode::RAINBOW) {
                        // Rainbow mode: cycle through hues
                        float hue = fmod(anim_factor + (float)(x + y) / (width + height), 1.0f) * 6.0f;
                        int h_i = (int)hue;
                        float h_f = hue - h_i;
                        float q = 1.0f - h_f;
                        
                        switch (h_i) {
                            case 0: r = 255; g = (uint8_t)(255 * h_f); b = 0; break;
                            case 1: r = (uint8_t)(255 * q); g = 255; b = 0; break;
                            case 2: r = 0; g = 255; b = (uint8_t)(255 * h_f); break;
                            case 3: r = 0; g = (uint8_t)(255 * q); b = 255; break;
                            case 4: r = (uint8_t)(255 * h_f); g = 0; b = 255; break;
                            default: r = 255; g = 0; b = (uint8_t)(255 * q); break;
                        }
                    } else {
                        // Interpolate between primary and secondary colors
                        r = static_cast<uint8_t>((primary.r * anim_factor + secondary.r * (1.0f - anim_factor)) * color_intensity);
                        g = static_cast<uint8_t>((primary.g * anim_factor + secondary.g * (1.0f - anim_factor)) * color_intensity);
                        b = static_cast<uint8_t>((primary.b * anim_factor + secondary.b * (1.0f - anim_factor)) * color_intensity);
                    }
                    
                    offscreen_canvas->SetPixel(x, y, r, g, b);
                }
            }
        }
    } else {
        // Show a simple gradient when feature is disabled
        auto base_color = color_demo->get();
        
        for (int y = 0; y < height; y++) {
            float intensity = static_cast<float>(y) / height;
            
            for (int x = 0; x < width; x++) {
                uint8_t r = static_cast<uint8_t>(base_color.r * intensity);
                uint8_t g = static_cast<uint8_t>(base_color.g * intensity);
                uint8_t b = static_cast<uint8_t>(base_color.b * intensity);
                
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
    // Register new enum properties
    add_property(animation_mode);
    add_property(display_pattern);
}

std::unique_ptr<Scene, void (*)(Scene *)> PropertyDemoSceneWrapper::create() {
    return {new PropertyDemoScene(), [](Scene *scene) {
        delete scene;
    }};
}