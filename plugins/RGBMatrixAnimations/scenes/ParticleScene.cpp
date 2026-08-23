#include "ParticleScene.h"
#include <algorithm>
#include "shared/matrix/interrupt.h"
#include "spdlog/spdlog.h"

using namespace Scenes;

ParticleScene::ParticleScene()
    : Scene()
{
    set_target_fps(60);
    num_particles->legacy_name("numParticles");
}

void ParticleScene::initialize(int width, int height)
{
    Scene::initialize(width, height);
    renderer.reset();
    animation.reset();
    simulation_.reset();
}

bool ParticleScene::render(rgb_matrix::FrameCanvas* canvas)
{
    if (renderer.has_value())
    {
        renderer.value()->setCanvas(canvas);
    }

    if (!renderer.has_value() || !animation.has_value())
    {
        spdlog::trace("Init particle scenes");
        auto local_renderer = std::make_shared<ParticleMatrixRenderer>(matrix_width, matrix_height, canvas);
        auto local_animation = std::make_shared<GravityParticles>(local_renderer, shake->get(), bounce->get());

        renderer = local_renderer;
        animation = local_animation;

        initializeParticles(local_renderer, local_animation);
    }

    auto current_renderer = renderer.value();
    auto current_animation = animation.value();

    if (!current_renderer || !current_animation)
    {
        spdlog::warn("Particle scene renderer or animation was unexpectedly null, reinitializing on next frame.");
        renderer.reset();
        animation.reset();
        hold_current_frame();
        return true;
    }

    const double physics_hz = 1000.0 / static_cast<double>(std::clamp(delay_ms->get(), 4, 50));
    simulation_.configure(physics_hz, 8);
    simulation_.advance(frame_context().delta_seconds, [&](double) {
        preRender(current_renderer, current_animation);
        current_animation->runCycle();
    });

    wait_until_next_frame();
    return true;
}

void ParticleScene::register_properties()
{
    num_particles->label("Particle limit").description("Maximum number of particles maintained by the scene.").group("Particles");
    velocity->label("Initial velocity").description("Base launch or fall velocity used when creating particles.").group("Physics");
    accel->label("Acceleration").description("Acceleration applied on each physics step.").group("Physics");
    shake->label("Random motion").description("Adds small random acceleration to keep particles moving naturally.").group("Physics");
    bounce->label("Bounce").description("Collision rebound amount; zero disables edge bounce.").group("Physics");
    delay_ms->label("Physics step").description("Simulation step duration. Lower values update physics more frequently without increasing render FPS.").group("Physics").unit("ms");

    add_property(num_particles);
    add_property(velocity);
    add_property(accel);
    add_property(shake);
    add_property(bounce);
    add_property(delay_ms);
}

void ParticleScene::after_render_stop()
{
    if (animation.has_value() && renderer.value())
    {
        this->particle_on_render_stop(renderer.value(), animation.value());
    }

    if (animation.has_value())
    {
        animation.value()->clearParticles();
    }

    if (renderer.has_value())
    {
        renderer.value()->clearImage();
    }

    animation.reset();
    renderer.reset();
    simulation_.reset();
}
