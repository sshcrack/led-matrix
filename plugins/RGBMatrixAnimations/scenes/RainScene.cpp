#include "RainScene.h"
#include "spdlog/spdlog.h"
#include "led-matrix.h"

using namespace Scenes;

RainScene::RainScene()
        : ParticleScene(),
          cols(nullptr),
          vels(nullptr),
          lengths(nullptr),
          counter(0),
          currentColorId(1),
          totalColors(0) {

    num_particles = MAKE_PROPERTY_MINMAX("num_particles", int, 4000, 1, 12000);
    num_particles->legacy_name("numParticles");
    velocity = MAKE_PROPERTY_MINMAX("velocity", int16_t, 6000, 100, 16000);
    shake = MAKE_PROPERTY_MINMAX("shake", int, 0, 0, 100);
    bounce = MAKE_PROPERTY_MINMAX("bounce", int, 0, 0, 255);
}

RainScene::~RainScene() {
    delete[] cols;
    delete[] vels;
    delete[] lengths;
}

void RainScene::initialize(int width, int height) {
    ParticleScene::initialize(width, height);
    totalCols = matrix_width / 1.4;
}

void RainScene::initializeParticles(std::shared_ptr<ParticleMatrixRenderer> renderer, std::shared_ptr<GravityParticles> animation) {
    animation->setAcceleration(0, -accel->get());
    initializeColumns();
    createColorPalette(renderer);
}

void RainScene::initializeColumns() {
    delete[] cols;
    delete[] vels;
    delete[] lengths;

    cols = new uint16_t[totalCols];
    vels = new uint16_t[totalCols];
    lengths = new uint8_t[totalCols];

    float v = velocity->get();
    for (uint16_t x = 0; x < totalCols; ++x) {
        cols[x] = matrix_width;
        vels[x] = random_int16(v / 4, v);
        lengths[x] = 0;
    }
}

void RainScene::createColorPalette(std::shared_ptr<ParticleMatrixRenderer> renderer) {
    uint16_t brightness = 255;
    uint8_t red = 0, green = 255, blue = 0;
    uint8_t shadeSize = 8;
    uint16_t colID = 0;

    // Create color gradient: green -> yellow -> red -> magenta -> blue -> cyan -> green
    for (uint16_t i = 0; i <= 255; i++) {
        //Green to yellow
        for (uint8_t j = 0; j < shadeSize; j++) {
            brightness = random_int16(50, 255);
            red = uint16_t(brightness * i / 255);
            green = brightness;
            colID = renderer->getColourId(RGB_color(red, green, blue));
        }
    }
    for (uint16_t i = 0; i <= 255; i++) {
        //Yellow to red
        for (uint8_t j = 0; j < shadeSize; j++) {
            brightness = random_int16(50, 255);
            red = brightness;
            green = uint16_t(brightness * (255 - i) / 255);
            colID = renderer->getColourId(RGB_color(red, green, blue));
        }
    }
    for (uint16_t i = 0; i <= 255; i++) {
        //Red to magenta
        for (uint8_t j = 0; j < shadeSize; j++) {
            brightness = random_int16(50, 255);
            red = brightness;
            blue = uint16_t(brightness * i / 255);
            colID = renderer->getColourId(RGB_color(red, green, blue));
        }
    }
    for (uint16_t i = 0; i <= 255; i++) {
        //Magenta to blue
        for (uint8_t j = 0; j < shadeSize; j++) {
            brightness = random_int16(50, 255);
            red = uint16_t(brightness * (255 - i) / 255);
            blue = brightness;
            colID = renderer->getColourId(RGB_color(red, green, blue));
        }
    }
    for (uint16_t i = 0; i <= 255; i++) {
        //Blue to cyan
        for (uint8_t j = 0; j < shadeSize; j++) {
            brightness = random_int16(50, 255);
            green = uint16_t(brightness * i / 255);
            blue = brightness;
            colID = renderer->getColourId(RGB_color(red, green, blue));
        }
    }
    for (uint16_t i = 0; i <= 255; i++) {
        //Cyan to green
        for (uint8_t j = 0; j < shadeSize; j++) {
            brightness = random_int16(50, 255);
            green = brightness;
            blue = uint16_t(brightness * (255 - i) / 255);
            colID = renderer->getColourId(RGB_color(red, green, blue));
        }
    }

    // ...additional color transitions...
    totalColors = renderer->getColourId(RGB_color(0, 255, 0)) - 1;
}

bool RainScene::render(rgb_matrix::FrameCanvas *canvas) {
    return ParticleScene::render(canvas);
}

void RainScene::preRender(std::shared_ptr<ParticleMatrixRenderer> renderer, std::shared_ptr<GravityParticles> animation)
{
    if (!renderer || !animation || !cols || !vels || !lengths) {
        return;
    }

    addNewParticles(renderer, animation);
    removeOldParticles(animation);
}

void RainScene::addNewParticles(std::shared_ptr<ParticleMatrixRenderer> ren, std::shared_ptr<GravityParticles> animation) {
    const uint16_t stepSize = 1;
    if (animation->getParticleCount() >= num_particles->get()) return;
    float v = velocity->get();

    counter++;
    if (counter >= stepSize) counter = 0;

    for (int i = 0; i < totalCols; i++) {
        if (lengths[i] < 1) {
            bool colClear = false;
            uint16_t newPos;
            while (!colClear) {
                colClear = true;
                newPos = random_int16(0, matrix_width);
                for (uint16_t x = 0; x < totalCols; ++x) {
                    if (newPos == cols[x]) colClear = false;
                }
            }
            cols[i] = newPos;
            lengths[i] = random_int16(8, 24);
            vels[i] = random_int16(v / 4, v);
        }

        if (ren->getPixelValue((ren->getGridHeight() - 1) * ren->getGridWidth() + cols[i]) == false) {
            if (counter == 0) {
                currentColorId++;
                if (currentColorId >= totalColors) currentColorId = 1;
            }
            RGB_color color = ren->getColor(currentColorId);
            animation->addParticle(cols[i], ren->getGridHeight() - 1, color, 0, -vels[i]);
            lengths[i]--;
        }
    }
}

void RainScene::removeOldParticles(std::shared_ptr<GravityParticles> anim) {
    // Walk backwards so deleting an entry cannot invalidate later indices.
    for (uint16_t i = anim->getParticleCount(); i > 0; --i) {
        const uint16_t index = static_cast<uint16_t>(i - 1);
        if (anim->getParticle(index).y == 0)
            anim->deleteParticle(index);
    }
}

string RainScene::get_name() const {
    return "rain";
}

Scenes::SceneDescriptor RainScene::get_descriptor() const {
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "particle-rain";
    d.tags = {"ambient", "particles", "rain", "flow"};
    d.intensity = 0.54f; d.motion = 0.70f; d.music_affinity = 0.22f; d.performance_cost = 0.68f;
    d.variants = {
        {"mist", "Light particle rain", "Sparse relaxed rain with a lower simulation budget",
         {{"num_particles", 1800}, {"velocity", 3500}, {"delay_ms", 14}},
         {"calm", "particles", "flow"}, 0.30f, 0.48f, 0.12f, 0.46f},
        {"downpour", "Color downpour", "Dense fast particle rain for lively ambient passages",
         {{"num_particles", 4800}, {"velocity", 7600}, {"delay_ms", 8}},
         {"vivid", "dense", "particles"}, 0.68f, 0.82f, 0.28f, 0.74f},
    };
    return d;
}


std::unique_ptr<Scene> RainSceneWrapper::create() {
    return std::make_unique<RainScene>();
}