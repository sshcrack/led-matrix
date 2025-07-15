#include "ColorPulseScene.h"
#include <cmath>

using namespace Scenes;

bool ColorPulseScene::render(RGBMatrixBase *matrix) {
    auto frameTime = frameTimer.tick();
    float t = frameTime.t * pulseSpeed->get();
    
    uint8_t r = 0, g = 0, b = 0;
    float pulse = (std::sin(t) + 1.0f) / 2.0f;
    
    switch(colorMode->get()) {
        case 0: // Red pulse
            r = static_cast<uint8_t>(pulse * 255);
            break;
        case 1: // Rainbow pulse
            r = static_cast<uint8_t>((std::sin(t) + 1.0f) * 127);
            g = static_cast<uint8_t>((std::sin(t + 2.094f) + 1.0f) * 127);
            b = static_cast<uint8_t>((std::sin(t + 4.189f) + 1.0f) * 127);
            break;
    }
    
    for(int y = 0; y < matrix->height(); y++) {
        for(int x = 0; x < matrix->width(); x++) {
            offscreen_canvas->SetPixel(x, y, r, g, b);
        }
    }
    
    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas);
    return true;
}

string ColorPulseScene::get_name() const {
    return "color_pulse";
}

void ColorPulseScene::register_properties() {
    add_property(pulseSpeed);
    add_property(colorMode);
}

tmillis_t ColorPulseScene::get_default_duration() {
    return 10000; // 10 seconds
}

int ColorPulseScene::get_default_weight() {
    return 10; // Default weight for this scene
}


std::unique_ptr<Scene, void (*)(Scene *)> ColorPulseSceneWrapper::create() {
    return {new ColorPulseScene(), [](Scene *scene) {
        delete scene;
    }};
}
