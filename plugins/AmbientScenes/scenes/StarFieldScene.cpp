#include "StarFieldScene.h"
#include <algorithm>
#include <cmath>
#include <shared/matrix/audio_state.h>
#include <shared/matrix/utils/color.h>

namespace AmbientScenes {
    namespace {
        // Longest frame step simulated at once; a stall or pause never jumps the animation.
        constexpr float MAX_FRAME_STEP_SECONDS = 0.10f;
        // Rate (1/s) at which smoothed audio features chase the live analysis.
        constexpr float AUDIO_SMOOTHING_RATE = 9.0f;
        // Beat and drop flashes fall by this much per second (1.0 = full flash).
        constexpr float BEAT_FLASH_DECAY_PER_SECOND = 3.5f;
        constexpr float DROP_FLASH_DECAY_PER_SECOND = 1.15f;
    }

    void StarFieldScene::Star::respawn(float max_depth, std::mt19937& rng) {
        std::uniform_real_distribution<float> coord(-1.0f, 1.0f);
        std::uniform_real_distribution<float> tint(0.0f, 1.0f);
        do { x = coord(rng); y = coord(rng); } while (x * x + y * y < 0.025f);
        z = max_depth;
        previous_z = z;
        hue = tint(rng);
    }

    void StarFieldScene::Star::update(float amount) {
        previous_z = z;
        z -= amount;
    }

    StarFieldScene::StarFieldScene() : Scene() {}

    void StarFieldScene::initialize(int width, int height) {
        Scene::initialize(width, height);
        stars.resize(std::max(1, num_stars->get()));
        const float depth = std::max(0.2f, max_depth->get());
        std::uniform_real_distribution<float> zdist(0.08f, depth);
        for (auto& star : stars) {
            star.respawn(depth, gen);
            star.z = zdist(gen);
            star.previous_z = std::min(depth, star.z + speed->get());
        }
        time = 0.0f;
        meteor_life_ = 0.0f;
        meteor_timer_ = 3.0f + 3.0f * std::uniform_real_distribution<float>(0.0f, 1.0f)(gen);
        buf_.resize(width, height);
        build_nebula();
    }

    namespace {
    constexpr int NEBULA_PAD = 12;

    float value_noise(float x, float y, uint32_t seed)
    {
        const int ix = static_cast<int>(std::floor(x)), iy = static_cast<int>(std::floor(y));
        const float fx = x - ix, fy = y - iy;
        const auto h = [&](int a, int b) {
            return AudioKit::hash(static_cast<uint32_t>(a) * 73856093u ^ static_cast<uint32_t>(b) * 19349663u ^ seed);
        };
        const float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
        const float top = h(ix, iy) + (h(ix + 1, iy) - h(ix, iy)) * sx;
        const float bottom = h(ix, iy + 1) + (h(ix + 1, iy + 1) - h(ix, iy + 1)) * sx;
        return top + (bottom - top) * sy;
    }

    float fbm(float x, float y, uint32_t seed)
    {
        float sum = 0, amp = 0.5f;
        for (int o = 0; o < 4; ++o) {
            sum += value_noise(x, y, seed + static_cast<uint32_t>(o) * 101u) * amp;
            x *= 2.03f;
            y *= 2.03f;
            amp *= 0.5f;
        }
        return sum / 0.9375f;
    }
    }  // namespace

    AudioKit::RGB StarFieldScene::star_color(float hue, float brightness) const
    {
        brightness = std::clamp(brightness, 0.0f, 1.0f);
        if (look_palette_->get().get() != Ambiance::Palette::ORIGINAL)
            brightness = 0.3f + 0.7f * brightness;  // dim looks keep their faint stars above the panel's lowest steps
        const auto palette = look_palette_->get().get();
        if (palette != Ambiance::Palette::ORIGINAL) {
            // Palette ends are dim, so stars are lifted above them to stay points of light.
            const auto tint = Ambiance::sample(palette, 0.55f + 0.45f * hue, 1.0f);
            return AudioKit::RGB{static_cast<float>(tint.r), static_cast<float>(tint.g), static_cast<float>(tint.b)} *
                   (1.6f * brightness * std::sqrt(look_brightness_->get()));
        }
        AudioKit::RGB c{255, 255, 255};
        if (colored_stars->get()) {
            // Star temperatures: mostly blue-white, some white, a few warm.
            static constexpr AudioKit::RGB blue{165, 195, 255}, white{238, 240, 255}, warm{255, 214, 160};
            c = hue < 0.55f ? AudioKit::lerp(blue, white, hue / 0.55f) : AudioKit::lerp(white, warm, (hue - 0.55f) / 0.45f);
            if (section_hue > 0.0f)
                c = AudioKit::lerp(c, AudioKit::hsv(std::fmod(0.55f + section_hue, 1.0f), 0.45f, 1.0f), 0.3f);
        }
        return c * (brightness * look_brightness_->get());
    }

    void StarFieldScene::build_nebula()
    {
        nebula_w_ = matrix_width + 2 * NEBULA_PAD;
        nebula_h_ = matrix_height + 2 * NEBULA_PAD;
        nebula_.assign(static_cast<std::size_t>(nebula_w_ * nebula_h_), {});
        const uint32_t seed = static_cast<uint32_t>(gen());
        std::uniform_real_distribution<float> unit(0.0f, 1.0f);
        fixed_.resize(static_cast<std::size_t>(std::max(4, matrix_width * matrix_height / 450)));
        for (auto& f : fixed_)
            f = {unit(gen) * matrix_width, unit(gen) * matrix_height, 0.45f + 0.55f * unit(gen) * unit(gen), unit(gen), unit(gen) * 6.28f};
        const auto palette = look_palette_->get().get();
        const float scale = 1.0f / 34.0f;
        for (int y = 0; y < nebula_h_; ++y)
            for (int x = 0; x < nebula_w_; ++x) {
                // Patchy clouds: only the denser parts of the noise glow, leaving dark sky between.
                // One off-centre region is denser and brighter, a second point of interest.
                const float fx = (x - nebula_w_ * 0.30f) / (nebula_w_ * 0.28f), fy = (y - nebula_h_ * 0.34f) / (nebula_h_ * 0.24f);
                const float core = std::exp(-(fx * fx + fy * fy));
                const float density =
                    AudioKit::smoothstep(0.42f - 0.12f * core, 0.92f, fbm(x * scale, y * scale, seed)) * (1.0f + 1.2f * core);
                if (density <= 0.0f)
                    continue;
                const float mix = fbm(x * scale * 0.7f + 17.0f, y * scale * 0.7f, seed ^ 0x9E37u);
                AudioKit::RGB c;
                if (palette != Ambiance::Palette::ORIGINAL) {
                    // Palette colours are darker than the original violet and teal; lift them to match.
                    const auto tint = Ambiance::sample(palette, 0.2f + 0.6f * mix, 1.0f);
                    c = AudioKit::RGB{static_cast<float>(tint.r), static_cast<float>(tint.g), static_cast<float>(tint.b)} * 1.6f;
                }
                else {
                    c = AudioKit::lerp(AudioKit::RGB{95, 40, 150}, AudioKit::RGB{25, 95, 140}, mix);
                }
                nebula_[static_cast<std::size_t>(y * nebula_w_ + x)] =
                    c * (0.22f * density * std::sqrt(look_brightness_->get()) * std::max(0.3f, 1.0f - speed->get() * 12.0f));
            }
    }

    bool StarFieldScene::render(rgb_matrix::FrameCanvas* canvas)
    {
        const float dt = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, MAX_FRAME_STEP_SECONDS);
        time += dt;
        if (audio_reactive->get()) {
            const auto audio = AudioState::snapshot();
            const bool has_audio = audio.fresh();
            const float response = 1.0f - std::exp(-dt * AUDIO_SMOOTHING_RATE);
            audio_bass += ((has_audio ? 0.5f * (audio.feature(AudioProtocol::Feature::SubBass) + audio.feature(AudioProtocol::Feature::Bass)) : 0.0f) - audio_bass) * response;
            audio_mids += ((has_audio ? (audio.feature(AudioProtocol::Feature::LowMid) + audio.feature(AudioProtocol::Feature::Mid) + audio.feature(AudioProtocol::Feature::HighMid)) / 3.0f : 0.0f) - audio_mids) * response;
            audio_treble += ((has_audio ? 0.5f * (audio.feature(AudioProtocol::Feature::Treble) + audio.feature(AudioProtocol::Feature::Air)) : 0.0f) - audio_treble) * response;
            audio_balance += ((has_audio ? audio.feature(AudioProtocol::Feature::StereoBalance) : 0.0f) - audio_balance) * response;
            audio_width += ((has_audio ? audio.feature(AudioProtocol::Feature::StereoWidth) : 0.0f) - audio_width) * response;
            hihat_detail += ((has_audio ? audio.feature(AudioProtocol::Feature::Hihat) : 0.0f) - hihat_detail) * response;
            if (has_audio && audio.beat_counter != last_beat_counter) {
                last_beat_counter = audio.beat_counter;
                beat_flash = std::max(beat_flash, 0.65f + audio.feature(AudioProtocol::Feature::Kick) * 0.35f);
            }
            if (has_audio && audio.drop_counter != last_drop_counter) { last_drop_counter = audio.drop_counter; drop_flash = 1.0f; }
            if (has_audio && audio.section_counter != last_section_counter) { last_section_counter = audio.section_counter; section_hue += 0.13f; }
        } else { audio_bass = audio_mids = audio_treble = audio_balance = audio_width = hihat_detail = 0.0f; }
        beat_flash = std::max(0.0f, beat_flash - dt * BEAT_FLASH_DECAY_PER_SECOND);
        drop_flash = std::max(0.0f, drop_flash - dt * DROP_FLASH_DECAY_PER_SECOND);
        const float depth = std::max(0.2f, max_depth->get());

        const std::size_t wanted_stars = static_cast<std::size_t>(std::max(1, num_stars->get()));
        if (stars.size() != wanted_stars) {
            const std::size_t old_size = stars.size();
            stars.resize(wanted_stars);
            for (std::size_t i = old_size; i < stars.size(); ++i) stars[i].respawn(depth, gen);
        }
        const float spatialStrength = audio_reactive->get() ? audio_strength->get() : 0.0f;
        const float cx = matrix_width * (0.5f + audio_balance * spatialStrength * 0.10f) +
            (drifting_center->get() ? std::sin(time * 0.63f) * matrix_width * 0.075f : 0.0f);
        const float cy = matrix_height * 0.5f + (drifting_center->get() ? std::cos(time * 0.47f) * matrix_height * 0.06f : 0.0f);
        const float scale = static_cast<float>(std::min(matrix_width, matrix_height)) *
            (0.48f + audio_width * spatialStrength * 0.055f);

        // Nebula backdrop drifts a little with the vanishing point, far behind the stars.
        {
            const float ox = NEBULA_PAD + (cx - matrix_width * 0.5f) * 0.25f;
            const float oy = NEBULA_PAD + (cy - matrix_height * 0.5f) * 0.25f;
            const int ix = std::clamp(static_cast<int>(std::floor(ox)), 0, 2 * NEBULA_PAD - 1);
            const int iy = std::clamp(static_cast<int>(std::floor(oy)), 0, 2 * NEBULA_PAD - 1);
            const float fx = std::clamp(ox - ix, 0.0f, 1.0f), fy = std::clamp(oy - iy, 0.0f, 1.0f);
            for (int y = 0; y < matrix_height; ++y) {
                const AudioKit::RGB* r0 = &nebula_[static_cast<std::size_t>((y + iy) * nebula_w_ + ix)];
                const AudioKit::RGB* r1 = r0 + nebula_w_;
                for (int x = 0; x < matrix_width; ++x) {
                    const auto top = r0[x] * (1 - fx) + r0[x + 1] * fx;
                    const auto bottom = r1[x] * (1 - fx) + r1[x + 1] * fx;
                    buf_.at(x, y) = top * (1 - fy) + bottom * fy;
                }
            }
        }

        for (const auto& f : fixed_) {
            const float b = f.brightness * (0.85f + 0.15f * std::sin(time * 0.9f + f.phase));
            const AudioKit::RGB c = star_color(f.hue, b);
            buf_.disc(f.x, f.y, 0.6f, c);
            buf_.disc(f.x, f.y, 2.2f, c * 0.07f);
        }

        // Flight speed swells and eases off slowly, so fast flights get quieter intervals.
        const float swell = 0.5f + 0.5f * std::sin(time * 0.26f);
        const float cruise = 0.45f + 0.65f * swell * swell;
        for (auto& star : stars) {
            const float audio_warp = audio_reactive->get() ? 1.0f + audio_bass * audio_strength->get() * 1.8f + beat_flash * 0.62f + drop_flash * 1.25f : 1.0f;
            const float movement = std::max(0.001f, speed->get()) * dt * 60.0f * audio_warp * cruise;
            star.update(movement);
            if (star.z <= 0.025f) star.respawn(depth, gen);

            const float inv_z = 1.0f / star.z;
            const float inv_prev = 1.0f / std::max(star.z + std::max(0.001f, speed->get()) * streak_length->get() * 6.0f, 0.025f);
            const float x = cx + star.x * scale * inv_z;
            const float y = cy + star.y * scale * inv_z;
            const float px = cx + star.x * scale * inv_prev;
            const float py = cy + star.y * scale * inv_prev;

            if (x < -8 || x >= matrix_width + 8 || y < -8 || y >= matrix_height + 8) {
                star.respawn(depth, gen);
                continue;
            }

            // Stars fade in from the far end of the volume instead of popping up.
            const float closeness = std::clamp((depth - star.z) / depth, 0.0f, 1.0f);
            float brightness = std::sqrt(closeness) * std::min(1.0f, closeness * 6.0f);
            if (audio_reactive->get()) brightness *= 1.0f + audio_treble * audio_strength->get() * 0.48f +
                beat_flash * 0.22f + hihat_detail * audio_strength->get() * 0.20f + drop_flash * 0.20f;
            if (enable_twinkle->get()) brightness *= 0.82f + (0.18f + hihat_detail * 0.12f) *
                std::sin(time * (7.0f + hihat_detail * 9.0f) + star.hue * 31.0f);

            const AudioKit::RGB c = star_color(star.hue, brightness);
            // Streak: anti-aliased, fading towards the tail.
            // Only near stars draw strong trails; distant ones leave faint short marks.
            const float trail = (0.08f + 0.5f * closeness * closeness * closeness) * (0.6f + 0.6f * streak_length->get());
            float sx = x - px, sy = y - py;
            float length = AudioKit::len(sx, sy);
            // Cap trails so stars leaving the edge do not become long bright dashes.
            const float max_length = 3.0f + 12.0f * streak_length->get() * closeness;
            if (length > max_length) {
                sx *= max_length / length;
                sy *= max_length / length;
                length = max_length;
            }
            if (streak_length->get() > 0.01f && length > 0.75f) {
                const int samples = std::min(64, static_cast<int>(length * 2.0f) + 1);
                for (int i = 1; i <= samples; ++i) {
                    const float t = static_cast<float>(i) / static_cast<float>(samples);
                    buf_.splat(x - sx * t - 0.5f, y - sy * t - 0.5f, c * (trail * (1.0f - t) * (1.0f - t) / std::max(1.0f, length * 0.3f)));
                }
            }
            // Near stars grow into soft discs with a faint halo; far ones are sub-pixel points.
            const float size = std::clamp((0.30f - star.z / depth) * 4.5f, 0.0f, 1.0f);
            if (size > 0.0f) {
                buf_.disc(x, y, 0.45f + 0.75f * size, c * 0.85f);
                buf_.disc(x, y, 1.6f + 1.4f * size, c * (0.10f * size));
            }
            else {
                buf_.splat(x - 0.5f, y - 0.5f, c * (0.7f / (1.0f + 0.6f * streak_length->get())));  // distant moving stars stay quiet
            }
        }

        // Now and then a faint meteor crosses the sky.
        meteor_timer_ -= dt;
        if (meteor_timer_ <= 0.0f && meteor_life_ <= 0.0f) {
            std::uniform_real_distribution<float> unit(0.0f, 1.0f);
            meteor_x_ = matrix_width * (0.15f + 0.7f * unit(gen));
            meteor_y_ = matrix_height * 0.25f * unit(gen);
            meteor_dx_ = (unit(gen) < 0.5f ? -1.0f : 1.0f) * (45.0f + 25.0f * unit(gen));
            meteor_dy_ = 30.0f + 20.0f * unit(gen);
            meteor_life_ = 0.9f;
            meteor_timer_ = 10.0f + 10.0f * unit(gen);
        }
        if (meteor_life_ > 0.0f) {
            meteor_life_ -= dt;
            meteor_x_ += meteor_dx_ * dt;
            meteor_y_ += meteor_dy_ * dt;
            const float fade = std::sin(std::clamp(meteor_life_ / 0.9f, 0.0f, 1.0f) * AudioKit::Pi);
            const AudioKit::RGB c = star_color(0.4f, fade) * 1.8f;
            const float len = AudioKit::len(meteor_dx_, meteor_dy_);
            for (int i = 0; i < 24; ++i) {
                const float t = i / 24.0f;
                buf_.splat(meteor_x_ - meteor_dx_ / len * t * 18.0f - 0.5f, meteor_y_ - meteor_dy_ / len * t * 18.0f - 0.5f,
                           c * (0.5f * (1.0f - t) * (1.0f - t)));
            }
        }

        buf_.blit(canvas);
        wait_until_next_frame();
        return true;
    }

    std::string StarFieldScene::get_name() const { return "starfield"; }

    void StarFieldScene::register_properties() {
        num_stars->label("Star count").description("Number of stars flying through the field.").group("Field");
        speed->label("Flight speed").description("Base forward travel speed.").group("Motion").step(0.005);
        max_depth->label("Depth").description("Depth of the star volume before stars reach the viewer.").group("Field").step(0.1);
        streak_length->label("Streak length").description("Length of motion trails behind fast stars.").group("Appearance").step(0.05);
        enable_twinkle->label("Twinkle").description("Add subtle independent brightness variation.").group("Appearance");
        colored_stars->label("Colored stars").description("Use cool blue/cyan tint variation instead of pure white.").group("Appearance");
        drifting_center->label("Drifting vanishing point").description("Slowly move the flight center for a less mechanical camera path.").group("Motion");
        audio_reactive->label("Audio reactive").description("Let bass accelerate the flight, beats punch forward and treble brighten stars.").group("Audio");
        audio_strength->label("Audio strength").description("Overall amount of music-driven modulation.").group("Audio").visible_if("audio_reactive", true).step(0.05);

        look_palette_->label("Look palette").group("Appearance");
        look_brightness_->label("Look brightness").group("Appearance").step(0.05);
        add_property(look_palette_);
        add_property(look_brightness_);
        add_property(num_stars);
        add_property(speed);
        add_property(enable_twinkle);
        add_property(max_depth);
        add_property(colored_stars);
        add_property(streak_length);
        add_property(audio_reactive);
        add_property(audio_strength);
        add_property(drifting_center);
    }

    std::unique_ptr<Scenes::Scene> StarFieldSceneWrapper::create() {
        return std::make_unique<StarFieldScene>();
    }
}

Scenes::SceneDescriptor AmbientScenes::StarFieldScene::get_descriptor() const {
    auto d = Scene::get_descriptor(); d.automatic_eligible = true;
    d.family = "space";
    d.tags = {"ambient", "space", "depth", "particles", "audio-reactive"};
    d.intensity = 0.45f; d.motion = 0.72f; d.music_affinity = 0.68f; d.performance_cost = 0.30f;
    d.variants = {
        {"calm",
         "Calm drift",
         "Sparse, slow stars for a quiet background",
         {{"num_stars", 68}, {"speed", 0.012f}, {"streak_length", 0.30f}, {"drifting_center", true}, {"audio_reactive", false}},
         {"calm", "minimal"},
         0.22f,
         0.35f,
         0.20f,
         0.24f},
        {"warp",
         "Warp drive",
         "Dense fast streaks with a strong sense of depth",
         {{"num_stars", 80}, {"speed", 0.06f}, {"streak_length", 1.0f}, {"enable_twinkle", false}},
         {"energetic", "depth"},
         0.88f,
         0.96f,
         0.55f,
         0.42f},
        {"music",
         "Music flight",
         "Balanced starfield with beat-driven acceleration",
         {{"num_stars", 112}, {"speed", 0.030f}, {"streak_length", 0.82f}, {"audio_reactive", true}, {"audio_strength", 1.15f}},
         {"music", "beat-driven"},
         0.72f,
         0.86f,
         1.0f,
         0.36f},
    };
    d.variants.push_back({"evening", "Evening starlight", "Sparse amber stars with almost no flight trails",
        {{"look_palette", "EMBER"}, {"look_brightness", 0.42f}, {"num_stars", 42}, {"speed", 0.006f}, {"streak_length", 0.05f}, {"audio_reactive", false}},
        {"calm", "warm", "amber"}, 0.12f, 0.14f, 0.0f, 0.18f});
    d.variants.push_back({"deep_space", "Deep space drift", "Cool distant stars and restrained violet trails",
        {{"look_palette", "SPACE"}, {"look_brightness", 0.55f}, {"num_stars", 72}, {"speed", 0.009f}, {"streak_length", 0.18f}, {"audio_reactive", false}},
        {"calm", "space", "indigo"}, 0.18f, 0.22f, 0.0f, 0.22f});
    Scenes::set_off_by_default(d, {"warp", "evening", "deep_space"});
    return d;
}
