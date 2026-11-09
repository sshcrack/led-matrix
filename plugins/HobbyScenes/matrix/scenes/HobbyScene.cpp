#include "HobbyScene.h"

#include "FigureDraw.h"

using namespace Hobbies;

// ---------------------------------------------------------------- base
void HobbySceneBase::register_properties()
{
    speed_->label("Speed").description("Animation speed.").group("Animation").step(0.05);
    add_property(speed_);
    if (environment_)
        for (const auto& p : environment_->properties()) add_property(p);
}

void HobbySceneBase::initialize(int width, int height)
{
    Scene::initialize(width, height);
    set_target_fps(30);
    cv_.resize(width, height);
    t_ = 0.0f;
    precip_.reset();
    if (environment_)
        environment_->snap();
}

void HobbySceneBase::begin_frame(float& dt)
{
    const float raw = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.1f);
    dt = raw * speed_->get();
    t_ += dt;
    if (environment_) {
        environment_->update(raw);
        // Fewer drops when the Pi is short on time.
        precip_.set_max(static_cast<int>(260 * render_quality_scale()));
    }
}

void HobbySceneBase::paint_sky_layers(float horizon, float scroll, float sun_radius, float cloud_top, float cloud_bottom)
{
    const auto& l = env();
    const auto s = surface();
    const float W = static_cast<float>(cv_.w), k = W / 128.0f;
    Environment::paint_sky(s, l, horizon);
    Environment::paint_stars(s, l, horizon * 0.9f, t_);
    Environment::paint_sun(s, l, Environment::sun_point(l, W, horizon), sun_radius * k);
    Environment::paint_moon(s, l, Environment::moon_point(l, W, horizon), 5.0f * k);
    Environment::paint_clouds(s, l, t_, horizon * cloud_top, horizon * cloud_bottom, scroll);
}

void HobbySceneBase::paint_weather(float raw_dt, float horizon, const Environment::Precipitation::GroundFn& ground)
{
    const auto s = surface();
    precip_.update(raw_dt, env(), cv_.w, cv_.h, ground);
    precip_.draw(s, env());
    lightning_.update_and_draw(s, env(), raw_dt, horizon);
}

bool HobbySceneBase::environment_changed(std::array<float, 12>& key) const
{
    const auto& L = env();
    const std::array<float, 12> now{L.sun_elevation * 0.5f, L.sun_x * 40.0f, L.overcast * 20.0f, L.fog * 20.0f,
                                    L.snow_cover * 20.0f, L.winter * 20.0f, L.autumn * 20.0f, L.spring * 20.0f,
                                    L.clouds * 10.0f, L.storm * 20.0f, L.rain * 10.0f, L.moon_phase * 30.0f};
    bool changed = false;
    for (size_t i = 0; i < now.size(); ++i)
        if (std::fabs(now[i] - key[i]) > 0.5f)
            changed = true;
    if (changed)
        key = now;
    return changed;
}

void HobbySceneBase::light_look(Hobbies::Look& look) const
{
    for (Rgb* c : {&look.skin, &look.hair, &look.shirt, &look.pants, &look.shoes, &look.pack, &look.capColor}) *c = subject(*c);
}

void HobbySceneBase::present(rgb_matrix::FrameCanvas* canvas)
{
    for (int y = 0; y < cv_.h; ++y)
        for (int x = 0; x < cv_.w; ++x) {
            const float* p = cv_.at(x, y);
            canvas->SetPixel(x, y, static_cast<uint8_t>(std::clamp(p[0], 0.0f, 1.0f) * 255.0f),
                             static_cast<uint8_t>(std::clamp(p[1], 0.0f, 1.0f) * 255.0f),
                             static_cast<uint8_t>(std::clamp(p[2], 0.0f, 1.0f) * 255.0f));
        }
}

// ---------------------------------------------------------------- audio
namespace {
float feature(const AudioState::Snapshot& a, AudioProtocol::Feature f) { return std::clamp(a.feature(f), 0.0f, 1.0f); }

bool consume(uint64_t current, uint64_t& seen)
{
    const bool advanced = current > seen;
    seen = current;
    return advanced;
}
}  // namespace

void AudioPulse::update(bool enabled, float strength, float dt)
{
    beatHit = onsetHit = dropHit = false;
    // Strength 0 switches audio off completely, discrete events included.
    enabled = enabled && strength > 0.01f;
    const auto a = enabled ? AudioState::snapshot() : AudioState::Snapshot{};
    const bool fresh = enabled && a.fresh() && !(a.flags & AudioProtocol::Silent);
    // Fade in over ~1 s, out over ~2 s, so music starting/stopping eases in.
    const float target = fresh ? std::clamp(strength, 0.0f, 2.0f) : 0.0f;
    amount += (target - amount) * (1.0f - std::exp(-dt / (fresh ? 1.0f : 2.0f)));

    const float k = 1.0f - std::exp(-dt * 10.0f);
    const float b = fresh ? 0.5f * (feature(a, AudioProtocol::Feature::SubBass) + feature(a, AudioProtocol::Feature::Bass)) : 0.0f;
    const float m = fresh ? (feature(a, AudioProtocol::Feature::LowMid) + feature(a, AudioProtocol::Feature::Mid) +
                             feature(a, AudioProtocol::Feature::HighMid)) / 3.0f
                          : 0.0f;
    const float t = fresh ? 0.5f * (feature(a, AudioProtocol::Feature::Treble) + feature(a, AudioProtocol::Feature::Air)) : 0.0f;
    const float l = fresh ? feature(a, AudioProtocol::Feature::LoudnessFast) : 0.0f;
    bass += (b - bass) * k;
    mid += (m - mid) * k;
    treble += (t - treble) * k;
    level += (l - level) * k;

    if (fresh) {
        if (!primed_) {
            beatSeen_ = a.beat_counter;
            onsetSeen_ = a.onset_counter;
            dropSeen_ = a.drop_counter;
            primed_ = true;
        }
        beatHit = consume(a.beat_counter, beatSeen_);
        onsetHit = consume(a.onset_counter, onsetSeen_);
        dropHit = consume(a.drop_counter, dropSeen_);
        const bool confident = feature(a, AudioProtocol::Feature::BeatConfidence) >= 0.35f;
        const float rawBpm = a.feature(AudioProtocol::Feature::Bpm);
        bpm = confident && rawBpm >= 50.0f && rawBpm <= 220.0f ? rawBpm : 0.0f;
        beatPhase = feature(a, AudioProtocol::Feature::BeatPhase);
    }
    else {
        primed_ = false;
        bpm = 0.0f;
    }
    beat *= std::exp(-dt / 0.18f);
    onset *= std::exp(-dt / 0.12f);
    drop *= std::exp(-dt / 1.2f);
    if (beatHit)
        beat = 1.0f;
    if (onsetHit)
        onset = 1.0f;
    if (dropHit)
        drop = 1.0f;
}

void AudioHobbyScene::register_properties()
{
    HobbySceneBase::register_properties();
    audioReactive_->label("Audio reactive").description("Follow the music when the desktop app streams audio.").group("Audio");
    audioStrength_->label("Audio strength").group("Audio").step(0.05).visible_if("audio_reactive", true);
    add_property(audioReactive_);
    add_property(audioStrength_);
}

void AudioHobbyScene::begin_audio_frame(float& dt, float& raw_dt)
{
    raw_dt = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.1f);
    begin_frame(dt);
    audio_.update(audioReactive_->get(), audioStrength_->get(), raw_dt);
}
