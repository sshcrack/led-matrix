#include "GenreVistaScenes.h"

namespace Scenes {
using namespace AudioKit;

void AudioDrumKitScene::register_properties()
{
    add_property(sensitivity_);
    add_property(glowAmount_);
    add_property(sticks_);
}

bool AudioDrumKitScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    spectrum_.update(audio, dt, 0.04f, 0.2f);

    const float kickF = feature(audio, AudioProtocol::Feature::Kick);
    const float snareF = feature(audio, AudioProtocol::Feature::Snare);
    const float hatF = feature(audio, AudioProtocol::Feature::Hihat);
    if (events_.onsetHit) {
        if (kickF > 0.3f && kickF >= snareF)
            kick_.hit(0.6f + 0.4f * kickF);
        else if (snareF > 0.3f)
            snare_.hit(0.6f + 0.4f * snareF);
        else if (hatF > 0.25f) {
            hat_.hit(0.5f + 0.5f * hatF);
            hatOpen_ = hatF > 0.6f ? 1.0f : 0.0f;
        } else {
            // Fills walk over the toms.
            (++hits_ % 2 ? tom1_ : tom2_).hit(0.8f);
        }
    }
    if (events_.beatHit) {
        beat_.hit();
        hat_.hit(0.4f);
    }
    if (events_.dropHit || events_.sectionHit) {
        crash_.hit();
        palette_.next();
        if (events_.dropHit)
            drop_.hit();
    }
    kick_.decay(dt, 0.13f);
    snare_.decay(dt, 0.12f);
    hat_.decay(dt, 0.07f);
    tom1_.decay(dt, 0.16f);
    tom2_.decay(dt, 0.16f);
    crash_.decay(dt, 0.9f);
    beat_.decay(dt, 0.2f);
    drop_.decay(dt, 1.8f);
    palette_.update(dt, 1.2f);
    hatOpen_ *= std::exp(-dt / 0.3f);

    const RGB accent = hsv(palette_.value, 0.85f, 1.0f);
    const float bl = glowAmount_->get();
    // --- stage: dark with three backlight cones ------------------------------------------------------------------
    for (int y = 0; y < H; ++y) {
        const float t = static_cast<float>(y) / H;
        for (int x = 0; x < W; ++x) buf_.at(x, y) = lerp(RGB{8, 7, 14}, RGB{22, 17, 24}, t);
    }
    for (int l = 0; l < 3; ++l) {
        const float lx = W * (0.2f + 0.3f * l);
        const float lvl = spectrum_.band(0.15f * l, 0.15f * l + 0.3f);
        const RGB c = hsv(palette_.value + 55.0f * l, 0.8f, 1.0f) * ((0.07f + 0.22f * lvl + 0.1f * beat_.v) * bl);
        for (int y = 0; y < static_cast<int>(H * 0.8f); ++y) {
            const float w = 3.0f + y * 0.35f;
            for (int x = static_cast<int>(lx - w); x <= static_cast<int>(lx + w); ++x) {
                const float a = 1.0f - std::fabs(x + 0.5f - lx) / w;
                buf_.add(x, y, c * (a * a * (1.0f - 0.5f * y / H)));
            }
        }
    }
    // Riser floor.
    for (int y = static_cast<int>(H * 0.84f); y < H; ++y)
        for (int x = 0; x < W; ++x) buf_.at(x, y) = RGB{26, 20, 22} * (0.6f + 0.4f * ((x / 10 + y / 4) % 2));

    auto drum = [&](float cx, float cy, float rx, float ry, float hit, const RGB& shell, const RGB& head) {
        const float pop = 1.0f + 0.08f * hit;
        ellipse(buf_, cx, cy + ry * 0.35f, rx * pop, ry * pop, shell * 0.55f);  // shell body
        ellipse(buf_, cx, cy, rx * pop, ry * 0.55f * pop, head * (0.6f + 0.9f * hit) + RGB{25, 25, 30});
        // Rim light.
        for (int a = 0; a < 48; ++a) {
            const float ang = a * Tau / 48.0f;
            buf_.add(static_cast<int>(cx + std::cos(ang) * rx * pop), static_cast<int>(cy + std::sin(ang) * ry * 0.55f * pop),
                     RGB{190, 190, 200} * (0.5f + 0.5f * hit));
        }
        if (hit > 0.05f)
            glow(buf_, cx, cy, rx * 1.5f, accent * (0.5f * hit));
    };
    auto cymbal = [&](float cx, float cy, float r, float hit, float tilt) {
        const float swing = std::sin(time_ * 28.0f) * hit * 1.2f;
        for (int k = -static_cast<int>(r); k <= static_cast<int>(r); ++k) {
            const float t = k / r;
            const float y = cy + t * tilt + swing * t;
            const float a = 0.55f + 0.5f * hit + 0.15f * std::sin(k * 1.7f);
            buf_.splat(cx + k - 0.5f, y - 0.5f, RGB{235, 190, 70} * a);
            buf_.splat(cx + k - 0.5f, y + 0.5f, RGB{140, 105, 35} * a);
        }
        glow(buf_, cx, cy, r * 1.2f, RGB{255, 210, 100} * (0.5f * hit));
        buf_.add(static_cast<int>(cx), static_cast<int>(cy) + 4, RGB{90, 90, 96});  // stand hint
        for (int y = static_cast<int>(cy) + 2; y < static_cast<int>(H * 0.78f); ++y) buf_.set(static_cast<int>(cx), y, RGB{60, 60, 66});
    };

    const RGB white{210, 210, 220}, black{22, 22, 26};
    const float kcx = W * 0.5f, kcy = H * 0.66f;
    // Cymbals and hi-hat behind the drums.
    cymbal(W * 0.17f, H * 0.28f, W * 0.13f, crash_.v, -3.0f);
    cymbal(W * 0.84f, H * 0.34f, W * 0.15f, 0.6f * crash_.v + 0.4f * beat_.v * 0.5f, 3.0f);
    cymbal(W * 0.10f, H * 0.52f, W * 0.09f, hat_.v + hatOpen_ * 0.5f, 1.0f);
    drum(W * 0.34f, H * 0.52f, W * 0.10f, H * 0.12f, tom1_.v, RGB{150, 30, 30}, white);
    drum(W * 0.66f, H * 0.52f, W * 0.10f, H * 0.12f, tom2_.v, RGB{150, 30, 30}, white);
    drum(W * 0.88f, H * 0.70f, W * 0.13f, H * 0.14f, tom2_.v * 0.6f, RGB{150, 30, 30}, white);   // floor tom
    drum(W * 0.14f, H * 0.72f, W * 0.11f, H * 0.07f, snare_.v, RGB{150, 150, 160}, white);        // snare
    // Bass drum: a big front head with a glowing ring that follows the kick.
    drum(kcx, kcy, W * 0.20f, H * 0.28f, kick_.v, RGB{150, 30, 30}, black);
    for (int a = 0; a < 90; ++a) {
        const float ang = a * Tau / 90.0f;
        const float r = (0.55f + 0.1f * std::sin(a * 0.7f + time_)) * W * 0.20f;
        buf_.add(static_cast<int>(kcx + std::cos(ang) * r), static_cast<int>(kcy + std::sin(ang) * r * 0.9f * 1.0f),
                 accent * (0.25f + 0.9f * kick_.v + 0.4f * spectrum_.band(0.0f, 0.15f)));
    }
    // Sticks flick towards whichever drum was just hit.
    if (sticks_->get()) {
        const float sx[2] = {W * 0.30f, W * 0.62f};
        const float hitL = std::max(snare_.v, tom1_.v), hitR = std::max(tom2_.v, hat_.v);
        const float hit[2] = {hitL, hitR};
        for (int s = 0; s < 2; ++s) {
            const float lift = 8.0f * (1.0f - hit[s]);
            line(buf_, sx[s], H * 0.42f - lift, sx[s] + (s ? 9.0f : -9.0f), H * 0.52f - lift * 0.4f, RGB{210, 180, 120} * 0.9f);
        }
    }
    if (drop_.v > 0.02f)
        for (auto& p : buf_.px) p = p + accent * (0.20f * drop_.v);
    if (kick_.v > 0.02f)
        for (auto& p : buf_.px) p = p * (1.0f + kick_.v * 0.10f);
    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioDrumKitScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "drum-kit";
    d.tags = {"music", "rock", "drums", "stage", "instrument", "audio-reactive", "beat-driven", "drop-reactive",
              "music-showcase", "style:rock", "style:hiphop"};
    d.intensity = 0.72f;
    d.motion = 0.65f;
    d.music_affinity = 1.0f;
    d.performance_cost = 0.45f;
    return d;
}
}  // namespace Scenes
