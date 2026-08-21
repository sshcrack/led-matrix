#include "SparksScene.h"
#include "spdlog/spdlog.h"

using namespace Scenes;

SparksScene::SparksScene()
    : ParticleScene(), ax(0), ay(0)
{
    // Sparks-specific defaults
    num_particles = MAKE_PROPERTY_MINMAX("num_particles", int, 40, 1, 12000);
    num_particles->legacy_name("numParticles");
    shake = MAKE_PROPERTY_MINMAX("shake", int, 5, 0, 100);
    bounce = MAKE_PROPERTY_MINMAX("bounce", int, 250, 0, 255);
}

void SparksScene::initializeParticles(std::shared_ptr<ParticleMatrixRenderer> renderer,
                                      std::shared_ptr<GravityParticles> animation)
{
    RGB_color yellow = {255, 200, 120};
    int16_t maxVel = 10000;

    for (int i = 0; i < num_particles->get(); i++)
    {
        int16_t vx = renderer->random_int16(-maxVel, maxVel + 1);
        int16_t vy = renderer->random_int16(-maxVel, maxVel + 1);

        if (vx > 0)
        {
            vx += maxVel / 5;
        }
        else
        {
            vx -= maxVel / 5;
        }

        if (vy > 0)
        {
            vy += maxVel / 5;
        }
        else
        {
            vy -= maxVel / 5;
        }

        animation->addParticle(yellow, vx, vy);
    }

    ax = 0;
    ay = -accel->get();
    animation->setAcceleration(ax, ay);
}


void SparksScene::particle_on_render_stop(std::shared_ptr<ParticleMatrixRenderer> renderer,
                                          std::shared_ptr<GravityParticles> animation)
{
    animation->clearParticles();
    initializeParticles(renderer, animation);
}

string SparksScene::get_name() const
{
    return "sparks";
}

std::unique_ptr<Scenes::Scene> SparksSceneWrapper::create()
{
    return std::make_unique<SparksScene>();
}
