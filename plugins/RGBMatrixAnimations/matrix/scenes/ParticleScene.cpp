#include "ParticleScene.h"
#include "shared/matrix/interrupt.h"
#include "spdlog/spdlog.h"

using namespace Scenes;

ParticleScene::ParticleScene()
    : Scene(),
      prevTime(0),
      lastFpsLog(0),
      frameCount(0),
      matrix(nullptr) {
}

void ParticleScene::initialize(int width, int height) {
    Scene::initialize(width, height);
    matrix = nullptr;
    renderer.reset();
    animation.reset();
}

bool ParticleScene::render(rgb_matrix::FrameCanvas *canvas) {
    if (matrix != canvas || !renderer.has_value() || !animation.has_value()) {
        matrix = canvas;
        renderer = std::make_shared<ParticleMatrixRenderer>(matrix_width, matrix_height, matrix);
        animation = std::unique_ptr<GravityParticles, void(*)(GravityParticles *)>(
            new GravityParticles(renderer.value(), shake->get(), bounce->get()),
            [](GravityParticles *a) {
                delete a;
            }
        );
        initializeParticles();
    }

    animation->get()->runCycle();

    uint8_t MAX_FPS = 1000 / delay_ms->get();
    uint32_t t;
    while ((t = micros() - prevTime) < (100000L / MAX_FPS)) {
    }

    frameCount++;
    uint64_t now = micros();

    if (now - lastFpsLog >= 1000000) {
        spdlog::trace("FPS: {:.2f}", (float) frameCount * 1000000.0f / (now - lastFpsLog));
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
    add_property(numParticles);
    add_property(velocity);
    add_property(accel);
    add_property(shake);
    add_property(bounce);
    add_property(delay_ms);
}

void ParticleScene::after_render_stop() {
    if (this->animation.has_value()) {
        this->animation->get()->clearParticles();
    }

    animation.reset();
    renderer.reset();
    matrix = nullptr;
}
