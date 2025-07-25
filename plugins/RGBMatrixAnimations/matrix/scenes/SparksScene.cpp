#include "SparksScene.h"
#include "spdlog/spdlog.h"

using namespace Scenes;

SparksScene::SparksScene()
        : ParticleScene(), ax(0), ay(0) {
    // Sparks-specific defaults
    numParticles = MAKE_PROPERTY("numParticles", int, 40);
    shake = MAKE_PROPERTY("shake", int, 5);
    bounce = MAKE_PROPERTY("bounce", int, 250);
}

void SparksScene::initializeParticles() {
    RGB_color yellow = {255, 200, 120};
    int16_t maxVel = 10000;

    for (int i = 0; i < numParticles->get(); i++) {
        int16_t vx = renderer->get()->random_int16(-maxVel, maxVel + 1);
        int16_t vy = renderer->get()->random_int16(-maxVel, maxVel + 1);

        if (vx > 0) {
            vx += maxVel / 5;
        } else {
            vx -= maxVel / 5;
        }

        if (vy > 0) {
            vy += maxVel / 5;
        } else {
            vy -= maxVel / 5;
        }

        animation->get()->addParticle(yellow, vx, vy);
    }

    ax = 0;
    ay = -accel->get();
    animation->get()->setAcceleration(ax, ay);
}

void SparksScene::after_render_stop(RGBMatrixBase *matrix) {
    animation->get()->clearParticles();
    initializeParticles();
}

string SparksScene::get_name() const {
    return "sparks";
}

std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> SparksSceneWrapper::create() {
    return {new SparksScene(), [](Scenes::Scene *scene) {
        delete scene;
    }};
}