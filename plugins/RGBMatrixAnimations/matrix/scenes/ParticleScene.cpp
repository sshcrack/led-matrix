#include "ParticleScene.h"
#include "shared/matrix/interrupt.h"
#include "spdlog/spdlog.h"

using namespace Scenes;

ParticleScene::ParticleScene()
    : Scene(),
      prevTime(0),
      lastFpsLog(0),
      frameCount(0)
{
}

void ParticleScene::initialize(int width, int height)
{
    Scene::initialize(width, height);
    renderer.reset();
    animation.reset();
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
        auto local_animation = std::shared_ptr<GravityParticles>(
            new GravityParticles(local_renderer, shake->get(), bounce->get()),
            [](GravityParticles* a)
            {
                delete a;
            }
        );

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
        return true;
    }

    preRender(current_renderer, current_animation);
    current_animation->runCycle();

    uint8_t MAX_FPS = 1000 / delay_ms->get();
    uint32_t t;
    while ((t = micros() - prevTime) < (100000L / MAX_FPS))
    {
    }

    frameCount++;
    uint64_t now = micros();

    if (now - lastFpsLog >= 1000000)
    {
        spdlog::trace("FPS: {:.2f}", (float)frameCount * 1000000.0f / (now - lastFpsLog));
        frameCount = 0;
        lastFpsLog = now;
    }

    prevTime = now;
    return true;
}

uint64_t ParticleScene::micros()
{
    uint64_t us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::
        now().time_since_epoch()).count();
    return us;
}

void ParticleScene::register_properties()
{
    add_property(numParticles);
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
    prevTime = 0;
    lastFpsLog = 0;
    frameCount = 0;
}
