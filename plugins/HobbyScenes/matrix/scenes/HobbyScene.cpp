#include "HobbyScene.h"

using namespace Hobbies;

// ---------------------------------------------------------------- base
void HobbySceneBase::register_properties()
{
    speed_->label("Speed").description("Animation speed.").group("Animation").step(0.05);
    add_property(speed_);
}

void HobbySceneBase::initialize(int width, int height)
{
    Scene::initialize(width, height);
    set_target_fps(30);
    cv_.resize(width, height);
    t_ = 0.0f;
}

void HobbySceneBase::begin_frame(float& dt)
{
    dt = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.1f) * speed_->get();
    t_ += dt;
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
