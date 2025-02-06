#include "SparksScene.h"
#include <spdlog/spdlog.h>

using namespace Scenes;

SparksScene::SparksScene(const nlohmann::json &config)
        : ParticleScene(config), ax(0), ay(0) {
    // Sparks-specific defaults
    numParticles = config.value("numParticles", 40);
    shake = config.value("shake", 5);
    bounce = config.value("bounce", 250);
}

SparksScene::~SparksScene() {
    delete animation;
    delete renderer;
}

void SparksScene::initialize(RGBMatrix *p_matrix) {
    initialized = true;
    matrix = p_matrix;

    // Use ParticleMatrixRenderer directly instead of SparksMatrixRenderer
    renderer = new ParticleMatrixRenderer(p_matrix->width(), p_matrix->height(), p_matrix);
    animation = new GravityParticles(*renderer, shake, bounce);

    initializeParticles();
}

void SparksScene::initializeParticles() {
    RGB_color yellow = {255, 200, 120};
    int16_t maxVel = 10000;

    for (int i = 0; i < numParticles; i++) {
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
    ay = -accel;
    animation->setAcceleration(ax, ay);
}

void SparksScene::after_render_stop(rgb_matrix::RGBMatrix *matrix) {
    animation->clearParticles();
    initializeParticles();
}

string SparksScene::get_name() const {
    return "sparks";
}

Scene *SparksSceneWrapper::create_default() {
    const nlohmann::json json = {
            {"weight",       1},
            {"duration",     15000},
            {"numParticles", 40},
            {"acceleration", 1},
            {"shake",        5},
            {"bounce",       250},
            {"delay_ms",     10}
    };
    return new SparksScene(json);
}

Scene *SparksSceneWrapper::from_json(const nlohmann::json &args) {
    return new SparksScene(args);
}
