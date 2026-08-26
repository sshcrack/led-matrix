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

Scenes::SceneDescriptor SparksScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "sparks";
    d.tags = {"ambient", "particles", "physics", "vivid", "energetic"};
    d.intensity = 0.70f; d.motion = 0.78f; d.music_affinity = 0.30f; d.performance_cost = 0.42f;
    d.variants = {
        {"embers", "Floating embers", "A restrained low-density spark field",
         {{"num_particles", 24}, {"shake", 2}, {"bounce", 210}, {"delay_ms", 14}},
         {"calm", "particles"}, 0.36f, 0.52f, 0.18f, 0.32f},
        {"burst", "Spark burst", "Brighter energetic bouncing particles",
         {{"num_particles", 64}, {"shake", 7}, {"bounce", 250}, {"delay_ms", 8}},
         {"vivid", "energetic", "particles"}, 0.76f, 0.82f, 0.34f, 0.48f},
    };
    return d;
}

std::unique_ptr<Scenes::Scene> SparksSceneWrapper::create()
{
    return std::make_unique<SparksScene>();
}
