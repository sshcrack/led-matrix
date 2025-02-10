#include "ParticleScene.h"
#include "shared/interrupt.h"
#include <spdlog/spdlog.h>

using namespace Scenes;

ParticleScene::ParticleScene()
        : Scene(),
          prevTime(0),
          lastFpsLog(0),
          frameCount(0) {}

ParticleScene::~ParticleScene() {
    delete animation;
    delete renderer;
}

void ParticleScene::initialize(rgb_matrix::RGBMatrix *p_matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) {
    Scene::initialize(p_matrix, l_offscreen_canvas);

    matrix = p_matrix;
    renderer = new ParticleMatrixRenderer(p_matrix->width(), p_matrix->height(), p_matrix);
    animation = new GravityParticles(*renderer, shake.get(), bounce.get());
    initializeParticles();
}

bool ParticleScene::render(RGBMatrix *rgbMatrix) {
    animation->runCycle();

    uint8_t MAX_FPS = 1000 / delay_ms.get();
    uint32_t t;
    while ((t = micros() - prevTime) < (100000L / MAX_FPS));

    frameCount++;
    uint64_t now = micros();

    if (now - lastFpsLog >= 1000000) {
        spdlog::debug("FPS: {:.2f}", (float) frameCount * 1000000.0f / (now - lastFpsLog));
        frameCount = 0;
        lastFpsLog = now;
    }

    prevTime = now;
    return true;
}

uint64_t ParticleScene::micros() {
    uint64_t us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::
                                                                        now().time_since_epoch()).count();
    return us;
}

void ParticleScene::register_properties() {
    add_property(&numParticles);
    add_property(&velocity);
    add_property(&accel);
    add_property(&shake);
    add_property(&bounce);
    add_property(&delay_ms);
}

void ParticleScene::after_render_stop(rgb_matrix::RGBMatrix *m) {
    this->animation->clearParticles();
}
