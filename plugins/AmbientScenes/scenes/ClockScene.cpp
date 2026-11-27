#include "ClockScene.h"

#include <shared/matrix/audio_kit.h>
#include <shared/matrix/audio_state.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace AmbientScenes {

namespace {

std::tm local_time(std::time_t t)
{
    std::tm out{};
#ifdef _WIN32
    localtime_s(&out, &t);
#else
    localtime_r(&t, &out);
#endif
    return out;
}

float smooth(float a, float b, float x)
{
    return AudioKit::smoothstep(a, b, x);
}

constexpr float BaseHeat = 0.72f;
constexpr double FadeSeconds = 0.22;
constexpr double FlickerSeconds = 0.35;
constexpr double SlotStep = 0.045;
constexpr double CycleStep = 0.07;     // one cathode during the cycle
constexpr double CycleSeconds = 1.4;   // every cathode lit twice
constexpr double CycleStagger = 0.12;  // tubes start one after another
constexpr double CycleSettle = 0.2;

}  // namespace

ClockScene::ClockScene() : Scene()
{
    set_target_fps(30);
}

void ClockScene::initialize(int width, int height)
{
    Scene::initialize(width, height);
    rebake();
}

void ClockScene::rebake()
{
    baked_seconds_ = show_seconds_->get();
    baked_w_ = matrix_width;
    baked_h_ = matrix_height;
    art_.bake(matrix_width, matrix_height, baked_seconds_ ? 6 : 4);
    if (art_.last_bake_ms() > 0.0)
        spdlog::debug("clock: baked {}x{} with {} tubes in {:.1f} ms", matrix_width, matrix_height, art_.tubes(), art_.last_bake_ms());
    tubes_.assign(static_cast<std::size_t>(art_.tubes()), Tube{});
    shown_colon_ = -1.0f;
    force_full_ = true;
    cycle_from_dark_ = true;
    // A tube clock that is switched on warms up through all its cathodes.
    cycle_start_ = cathode_cycle_->get() ? scene_time_ : -1e9;
}

std::int64_t ClockScene::wall_ms()
{
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if (!frame_context().deterministic)
        return now;
    // Previews run faster than real time: start at the real time and follow
    // the preview timeline from there.
    if (preview_wall_ms_ < 0)
        preview_wall_ms_ = now;
    return preview_wall_ms_ + static_cast<std::int64_t>(frame_context().elapsed_seconds * 1000.0);
}

std::vector<int> ClockScene::wanted_digits(const std::tm& t) const
{
    int hour = t.tm_hour;
    if (hour_format_->get().get() == HourFormat::H12) {
        hour %= 12;
        if (hour == 0)
            hour = 12;
    }
    std::vector<int> d{hour / 10, hour % 10, t.tm_min / 10, t.tm_min % 10};
    if (art_.tubes() == 6) {
        d.push_back(t.tm_sec / 10);
        d.push_back(t.tm_sec % 10);
    }
    if (!leading_zero_->get() && d[0] == 0)
        d[0] = -1;  // unlit, like most nixie clocks
    return d;
}

void ClockScene::update_music(float dt)
{
    const float decay = std::exp(-dt * 5.0f);
    for (auto& t : tubes_) t.kick *= decay;

    bool playing = false;
    float bass = 0.0f;
    // While no music plays, look for it four times a second only.
    static constexpr float IdlePoll = 0.25f;
    music_poll_ -= dt;
    if (music_glow_->get() && (live_ > 0.0f || music_poll_ <= 0.0f)) {
        music_poll_ = IdlePoll;
        const auto audio = AudioState::snapshot();
        playing =
            audio.fresh() && !audio.event(AudioProtocol::Silent) && AudioKit::feature(audio, AudioProtocol::Feature::Loudness) > 0.02f;
        if (playing) {
            bass = AudioKit::feature(audio, AudioProtocol::Feature::Bass);
            if (AudioKit::consume(audio.beat_counter, beat_seen_) && !tubes_.empty())
                tubes_[static_cast<std::size_t>(beat_index_++ % tubes_.size())].kick = 1.0f;
        }
        else {
            beat_seen_ = audio.beat_counter;
        }
    }
    else if (!music_glow_->get()) {
        music_poll_ = 0.0f;
    }
    // ~1 s in, ~2 s out; bass follows quickly up, slowly down.
    live_ = std::clamp(live_ + (playing ? dt : -dt * 0.5f), 0.0f, 1.0f);
    const float rate = bass > bass_ ? 12.0f : 2.5f;
    bass_ += (bass - bass_) * (1.0f - std::exp(-dt * rate));
}

void ClockScene::tube_look(int i, const Tube& tube, int& a, int& b, float& mix) const
{
    a = tube.from;
    b = tube.target;
    mix = 1.0f;

    // Cathode cycle: every numeral lights in turn (keeps unused cathodes
    // from poisoning on a real tube; here it is the warm-up and the hourly
    // flourish).
    const double tc = scene_time_ - cycle_start_ - CycleStagger * i;
    if (tc < CycleSeconds + CycleSettle && scene_time_ - cycle_start_ >= 0.0) {
        const int seed = 3 * i + 1;
        if (tc < 0.0) {
            // Not started yet: the warm-up starts dark, the hourly cycle from the time.
            a = b = cycle_from_dark_ ? -1 : tube.target;
            return;
        }
        if (tc < CycleSeconds) {
            const int k = static_cast<int>(tc / CycleStep);
            a = k == 0 ? (cycle_from_dark_ ? -1 : tube.target) : (seed + k - 1) % 10;
            b = (seed + k) % 10;
            mix = smooth(0.0f, 0.6f, static_cast<float>(tc / CycleStep - k));
            return;
        }
        a = (seed + static_cast<int>(CycleSeconds / CycleStep) - 1) % 10;
        b = tube.target;
        mix = smooth(0.0f, static_cast<float>(CycleSettle), static_cast<float>(tc - CycleSeconds));
        return;
    }

    const double t = scene_time_ - tube.changed_at;
    switch (digit_change_->get().get()) {
        case DigitChange::INSTANT:
            break;
        case DigitChange::FADE:
            mix = smooth(0.0f, static_cast<float>(FadeSeconds), static_cast<float>(t));
            break;
        case DigitChange::FLICKER:
            if (t < FlickerSeconds) {
                // The new cathode catches unevenly, more often as it warms.
                const auto step = static_cast<std::uint32_t>(t * 45.0);
                mix = AudioKit::hash(step * 7919U + static_cast<std::uint32_t>(i) * 104729U) < t / FlickerSeconds ? 1.0f : 0.0f;
            }
            break;
        case DigitChange::SLOT: {
            if (tube.from < 0 || tube.target < 0) {
                mix = smooth(0.0f, static_cast<float>(FadeSeconds), static_cast<float>(t));
                break;
            }
            // Roll forward through one full turn of cathodes onto the new numeral.
            const int steps = (tube.target - tube.from + 10) % 10 + 10;
            if (t >= 0.0 && t < steps * SlotStep) {
                const int k = static_cast<int>(t / SlotStep);
                a = (tube.from + k) % 10;
                b = (tube.from + k + 1) % 10;
                mix = smooth(0.0f, 0.5f, static_cast<float>(t / SlotStep - k));
            }
            break;
        }
    }
}

bool ClockScene::render(rgb_matrix::FrameCanvas* canvas)
{
    const float dt = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.25f);
    scene_time_ += dt;

    if (!art_.ready() || baked_w_ != matrix_width || baked_h_ != matrix_height || baked_seconds_ != show_seconds_->get())
        rebake();

    const std::int64_t ms = wall_ms();
    const std::tm now = local_time(static_cast<std::time_t>(ms / 1000));
    const float second_phase = static_cast<float>(ms % 1000) / 1000.0f;

    if (now.tm_min == 0 && now.tm_hour != last_cycle_hour_ && cathode_cycle_->get() && scene_time_ > CycleSeconds * 2) {
        cycle_start_ = scene_time_;
        cycle_from_dark_ = false;
    }
    if (now.tm_min == 0)
        last_cycle_hour_ = now.tm_hour;

    const auto digits = wanted_digits(now);
    for (std::size_t i = 0; i < tubes_.size() && i < digits.size(); ++i) {
        Tube& t = tubes_[i];
        if (t.target == -2) {
            t.target = t.from = digits[i];
        }
        else if (digits[i] != t.target) {
            t.from = t.target;
            t.target = digits[i];
            t.changed_at = scene_time_;
        }
    }

    update_music(dt);

    // Lower Pi quality: coarser glow steps, so music redraws fewer frames.
    const float quality = render_quality_scale();
    const float heat_step = quality >= 0.9f ? 0.004f : (quality >= 0.7f ? 0.010f : 0.020f);

    Nixie::Rect dirty;
    if (force_full_) {
        dirty = art_.full();
        force_full_ = false;
    }
    looks_.resize(tubes_.size());
    float heat_sum = 0.0f;
    for (std::size_t i = 0; i < tubes_.size(); ++i) {
        Tube& t = tubes_[i];
        Nixie::TubeLook& look = looks_[i];
        tube_look(static_cast<int>(i), t, look.a, look.b, look.mix);
        // Real tubes never glow quite alike.
        const float own = 1.0f + 0.05f * (AudioKit::hash(static_cast<std::uint32_t>(i) * 2654435761U) - 0.5f);
        const float heat = (BaseHeat + live_ * (0.10f * bass_ + 0.12f * t.kick)) * own;
        // Hold the composed heat until it moves a visible step.
        look.heat = std::fabs(heat - t.shown_heat) >= heat_step ? heat : t.shown_heat;
        heat_sum += look.heat;

        if (look.heat != t.shown_heat) {
            dirty.unite(art_.tube_rect(static_cast<int>(i)));
        }
        else if (look.a != t.shown_a || look.b != t.shown_b || std::fabs(look.mix - t.shown_mix) > 0.002f) {
            dirty.unite(art_.box_rect(static_cast<int>(i)));
        }
        t.shown_a = look.a;
        t.shown_b = look.b;
        t.shown_mix = look.mix;
        t.shown_heat = look.heat;
    }

    float colon = 0.0f;
    switch (colon_->get().get()) {
        case ColonStyle::STEADY:
            colon = 1.0f;
            break;
        case ColonStyle::BLINK:
            colon = smooth(0.0f, 0.06f, second_phase) * (1.0f - smooth(0.48f, 0.56f, second_phase));
            break;
        case ColonStyle::OFF:
            break;
    }
    if (!tubes_.empty())
        colon *= heat_sum / (static_cast<float>(tubes_.size()) * BaseHeat);
    colon = std::round(colon * 64.0f) / 64.0f;
    if (colon != shown_colon_) {
        dirty.unite(art_.colon_rect());
        shown_colon_ = colon;
    }

    if (!dirty.empty())
        art_.compose(dirty, looks_, shown_colon_, 0.94f);
    canvas->SetRGB(0, 0, matrix_width, matrix_height, art_.rgb());
    return true;
}

void ClockScene::register_properties()
{
    hour_format_->label("Hour format").description("24-hour (H24) or 12-hour (H12) time.").group("Clock");
    leading_zero_->label("Leading zero").description("Light a leading 0 in the hour tube; off leaves that tube dark.").group("Clock");
    show_seconds_->label("Seconds tubes").description("Six tubes with seconds instead of four.").group("Clock");
    colon_->label("Colon dots").description("Neon dots between the tube pairs: blinking each second, steady or off.").group("Clock");
    digit_change_->label("Digit change")
        .description("How a tube changes numeral: soft fade, a roll through all cathodes, a short flicker or instant.")
        .group("Tubes");
    cathode_cycle_->label("Cathode cycle")
        .description("Run every numeral through each tube when the scene starts and on the hour.")
        .group("Tubes");
    music_glow_->label("Music glow").description("The neon swells gently with bass and beats while music plays.").group("Tubes");
    add_property(hour_format_);
    add_property(leading_zero_);
    add_property(show_seconds_);
    add_property(colon_);
    add_property(digit_change_);
    add_property(cathode_cycle_);
    add_property(music_glow_);
}

Scenes::SceneDescriptor ClockScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    // Shows readable time: stays a manual scene.
    d.automatic_eligible = false;
    d.family = "clock";
    d.tags = {"clock", "nixie", "retro", "warm", "calm"};
    d.intensity = 0.30f;
    d.motion = 0.10f;
    d.music_affinity = 0.20f;
    d.performance_cost = 0.06f;
    // Seconds stay a property (off by default); the curated look keeps four tubes.
    d.variants = {
        {"rolling",
         "Rolling digits",
         "Every change rolls through all cathodes",
         {{"digit_change", "SLOT"}},
         {"clock", "nixie"},
         {},
         0.20f,
         {},
         {},
         false},
    };
    return d;
}

Scenes::SceneCapabilities ClockScene::get_capabilities() const
{
    auto caps = Scene::get_capabilities();
    caps.supports_audio = true;
    return caps;
}

std::unique_ptr<Scenes::Scene> ClockSceneWrapper::create()
{
    return std::make_unique<ClockScene>();
}

}  // namespace AmbientScenes
