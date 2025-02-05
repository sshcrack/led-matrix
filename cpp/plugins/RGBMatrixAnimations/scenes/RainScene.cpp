#include "RainScene.h"
#include <spdlog/spdlog.h>
#include <led-matrix.h>

using namespace Scenes;

RainScene::RainScene(const nlohmann::json &config)
    : ParticleScene(config),
      cols(nullptr),
      vels(nullptr),
      lengths(nullptr),
      counter(0),
      currentColorId(1),
      totalColors(0)
{
    // Rain-specific defaults
    numParticles = config.value("numParticles", 4000);
    velocity = config.value("velocity", 6000);
    shake = config.value("shake", 0);
    bounce = config.value("bounce", 0);
}

RainScene::~RainScene() {
    delete[] cols;
    delete[] vels;
    delete[] lengths;
}

void RainScene::initialize(RGBMatrix *p_matrix) {
    matrix = p_matrix;
    totalCols = p_matrix->width() / 1.4;
    
    // Use ParticleMatrixRenderer directly instead of RainMatrixRenderer
    renderer = new ParticleMatrixRenderer(p_matrix->width(), p_matrix->height(), p_matrix);
    animation = new GravityParticles(*renderer, shake, bounce);
    animation->setAcceleration(0, -accel);

    initializeParticles();
}

void RainScene::initializeParticles() {
    initializeColumns();
    createColorPalette();
}

void RainScene::initializeColumns() {
    cols = new uint16_t[totalCols];
    vels = new uint16_t[totalCols];
    lengths = new uint8_t[totalCols];

    for (uint16_t x = 0; x < totalCols; ++x) {
        cols[x] = matrix->width();
        vels[x] = random_int16(velocity/4, velocity);
        lengths[x] = 0;
    }
}

void RainScene::createColorPalette() {
    uint16_t brightness = 255;
    uint8_t red = 0, green = 255, blue = 0;
    uint8_t shadeSize = 8;
    uint16_t colID = 0;

    // Create color gradient: green -> yellow -> red -> magenta -> blue -> cyan -> green
    for (uint16_t i = 0; i <= 255; i++){
        //Green to yellow
        for (uint8_t j = 0; j < shadeSize; j++){
            brightness = random_int16(50,255);
            red = uint16_t(brightness * i / 255);
            green = brightness;
            colID = renderer->getColourId(RGB_color(red,green,blue));
        }
    }
    for (uint16_t i = 0; i <= 255; i++){
        //Yellow to red
        for (uint8_t j = 0; j < shadeSize; j++){
            brightness = random_int16(50,255);
            red = brightness;
            green = uint16_t(brightness * (255-i) / 255);
            colID = renderer->getColourId(RGB_color(red,green,blue));
        }
    }
    for (uint16_t i = 0; i <= 255; i++){
        //Red to magenta
        for (uint8_t j = 0; j < shadeSize; j++){
            brightness = random_int16(50,255);
            red = brightness;
            blue = uint16_t(brightness * i / 255);
            colID = renderer->getColourId(RGB_color(red,green,blue));
        }
    }
    for (uint16_t i = 0; i <= 255; i++){
        //Magenta to blue
        for (uint8_t j = 0; j < shadeSize; j++){
            brightness = random_int16(50,255);
            red = uint16_t(brightness * (255-i) / 255);
            blue = brightness;
            colID = renderer->getColourId(RGB_color(red,green,blue));
        }
    }
    for (uint16_t i = 0; i <= 255; i++){
        //Blue to cyan
        for (uint8_t j = 0; j < shadeSize; j++){
            brightness = random_int16(50,255);
            green = uint16_t(brightness * i / 255);
            blue = brightness;
            colID = renderer->getColourId(RGB_color(red,green,blue));
        }
    }
    for (uint16_t i = 0; i <= 255; i++){
        //Cyan to green
        for (uint8_t j = 0; j < shadeSize; j++){
            brightness = random_int16(50,255);
            green = brightness;
            blue = uint16_t(brightness * (255-i) / 255);
            colID = renderer->getColourId(RGB_color(red,green,blue));
        }
    }

    // ...additional color transitions...
    totalColors = renderer->getColourId(RGB_color(0, 255, 0)) - 1;
}

bool RainScene::render(RGBMatrix *matrix) {
    addNewParticles();
    removeOldParticles();
    
    // Call parent class render which handles animation and FPS
    return ParticleScene::render(matrix);
}

void RainScene::addNewParticles() {
    const uint16_t stepSize = 1;
    if (animation->getParticleCount() >= numParticles) return;

    counter++;
    if (counter >= stepSize) counter = 0;

    for (int i = 0; i < totalCols; i++) {
        if (lengths[i] < 1) {
            bool colClear = false;
            uint16_t newPos;
            while (!colClear) {
                colClear = true;
                newPos = random_int16(0, matrix->width());
                for (uint16_t x = 0; x < totalCols; ++x) {
                    if (newPos == cols[x]) colClear = false;
                }
            }
            cols[i] = newPos;
            lengths[i] = random_int16(8, 24);
            vels[i] = random_int16(velocity/4, velocity);
        }

        if (renderer->getPixelValue((renderer->getGridHeight()-1) * renderer->getGridWidth() + cols[i]) == false) {
            if (counter == 0) {
                currentColorId++;
                if (currentColorId >= totalColors) currentColorId = 1;
            }
            RGB_color color = renderer->getColor(currentColorId);
            animation->addParticle(cols[i], renderer->getGridHeight()-1, color, 0, -vels[i]);
            lengths[i]--;
        }
    }
}

void RainScene::removeOldParticles() {
    uint16_t removeNum = std::min((uint16_t) (numParticles - 1), (uint16_t)matrix->width());
    if (animation->getParticleCount() > removeNum) {
        for (uint16_t i = 0; i < removeNum; i++) {
            auto particle = animation->getParticle(removeNum-1-i);
            if (particle.y == 0) {
                animation->deleteParticle(removeNum-1-i);
            }
        }
    }
}

string RainScene::get_name() const {
    return "rain";
}



Scene *RainSceneWrapper::create_default() {
    const nlohmann::json json = {
        {"weight", 1},
        {"duration", 15000},
        {"numParticles", 4000},
        {"velocity", 6000},
        {"acceleration", 1},
        {"shake", 0},
        {"bounce", 0},
        {"delay_ms", 10}
    };
    return new RainScene(json);
}

Scene *RainSceneWrapper::from_json(const nlohmann::json &args) {
    return new RainScene(args);
}
