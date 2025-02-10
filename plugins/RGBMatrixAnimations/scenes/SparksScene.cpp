#include "SparksScene.h"
#include <spdlog/spdlog.h>

using namespace Scenes;

SparksScene::SparksScene()
        : ParticleScene(), ax(0), ay(0) {
    // Sparks-specific defaults
    numParticles = Property("numParticles", 40);
    shake = Property("shake", 5);
    bounce = Property("bounce", 250);
}

SparksScene::~SparksScene() {
    delete animation;
    delete renderer;
}

void SparksScene::initialize(rgb_matrix::RGBMatrix *p_matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) {
    initialized = true;
    matrix = p_matrix;

    // Use ParticleMatrixRenderer directly instead of SparksMatrixRenderer
    renderer = new ParticleMatrixRenderer(p_matrix->width(), p_matrix->height(), p_matrix);
    animation = new GravityParticles(*renderer, shake.get(), bounce.get());

    initializeParticles();
}

void SparksScene::initializeParticles() {
    RGB_color yellow = {255, 200, 120};
    int16_t maxVel = 10000;

    for (int i = 0; i < numParticles.get(); i++) {
        int16_t vx = renderer->random_int16(-maxVel, maxVel + 1);
        int16_t vy = renderer->random_int16(-maxVel, maxVel + 1);

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

        animation->addParticle(yellow, vx, vy);
    }

    ax = 0;
    ay = -accel.get();
    animation->setAcceleration(ax, ay);
}

void SparksScene::after_render_stop(rgb_matrix::RGBMatrix *matrix) {
    animation->clearParticles();
    initializeParticles();
}

string SparksScene::get_name() const {
    return "sparks";
}

Scene *SparksSceneWrapper::create() {
    return new SparksScene();
}