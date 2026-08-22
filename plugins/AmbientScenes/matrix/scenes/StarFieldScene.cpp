#include "StarFieldScene.h"
#include <algorithm>
#include <cmath>
#include <shared/matrix/audio_state.h>

namespace AmbientScenes {
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
    }

    void StarFieldScene::hsv_to_rgb(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b) {
        h = h - std::floor(h);
        const float c = v * s;
        const float x = c * (1.0f - std::fabs(std::fmod(h * 6.0f, 2.0f) - 1.0f));
        const float m = v - c;
        float rr = 0, gg = 0, bb = 0;
        const int sector = static_cast<int>(h * 6.0f) % 6;
        if (sector == 0) { rr = c; gg = x; }
        else if (sector == 1) { rr = x; gg = c; }
        else if (sector == 2) { gg = c; bb = x; }
        else if (sector == 3) { gg = x; bb = c; }
        else if (sector == 4) { rr = x; bb = c; }
        else { rr = c; bb = x; }
        r = static_cast<uint8_t>(std::clamp((rr + m) * 255.0f, 0.0f, 255.0f));
        g = static_cast<uint8_t>(std::clamp((gg + m) * 255.0f, 0.0f, 255.0f));
        b = static_cast<uint8_t>(std::clamp((bb + m) * 255.0f, 0.0f, 255.0f));
    }

    void StarFieldScene::draw_line(rgb_matrix::FrameCanvas* canvas, int x0, int y0, int x1, int y1,
                                   uint8_t r, uint8_t g, uint8_t b, int width, int height) {
        int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        for (;;) {
            if (x0 >= 0 && x0 < width && y0 >= 0 && y0 < height) canvas->SetPixel(x0, y0, r, g, b);
            if (x0 == x1 && y0 == y1) break;
            const int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    bool StarFieldScene::render(rgb_matrix::FrameCanvas *canvas) {
        canvas->Clear();
        const float dt = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.10f);
        time += dt;
        if (audio_reactive->get()) {
            const auto audio = AudioState::snapshot();
            const bool has_audio = audio.fresh();
            const float response = 1.0f - std::exp(-dt * 9.0f);
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
        beat_flash = std::max(0.0f, beat_flash - dt * 3.5f);
        drop_flash = std::max(0.0f, drop_flash - dt * 1.15f);
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

        for (auto& star : stars) {
            const float audio_warp = audio_reactive->get() ? 1.0f + audio_bass * audio_strength->get() * 1.8f + beat_flash * 0.62f + drop_flash * 1.25f : 1.0f;
            const float movement = std::max(0.001f, speed->get()) * dt * 60.0f * audio_warp;
            star.update(movement);
            if (star.z <= 0.025f) star.respawn(depth, gen);

            const float inv_z = 1.0f / star.z;
            const float inv_prev = 1.0f / std::max(star.z + std::max(0.001f, speed->get()) * streak_length->get() * 6.0f, 0.025f);
            const int x = static_cast<int>(cx + star.x * scale * inv_z);
            const int y = static_cast<int>(cy + star.y * scale * inv_z);
            const int px = static_cast<int>(cx + star.x * scale * inv_prev);
            const int py = static_cast<int>(cy + star.y * scale * inv_prev);

            if (x < -8 || x >= matrix_width + 8 || y < -8 || y >= matrix_height + 8) {
                star.respawn(depth, gen);
                continue;
            }

            float brightness = std::clamp((depth - star.z) / depth, 0.05f, 1.0f);
            brightness = std::sqrt(brightness);
            if (audio_reactive->get()) brightness *= 1.0f + audio_treble * audio_strength->get() * 0.48f +
                beat_flash * 0.22f + hihat_detail * audio_strength->get() * 0.20f + drop_flash * 0.20f;
            if (enable_twinkle->get()) brightness *= 0.82f + (0.18f + hihat_detail * 0.12f) *
                std::sin(time * (7.0f + hihat_detail * 9.0f) + star.hue * 31.0f);

            uint8_t r, g, b;
            if (colored_stars->get()) {
                const float hue = 0.55f + section_hue + (star.hue - 0.5f) * 0.18f + hihat_detail * 0.025f;
                hsv_to_rgb(hue, 0.28f, std::clamp(brightness, 0.0f, 1.0f), r, g, b);
            } else {
                r = g = b = static_cast<uint8_t>(255.0f * std::clamp(brightness, 0.0f, 1.0f));
            }

            if (streak_length->get() > 0.01f)
                draw_line(canvas, px, py, x, y, r / 2, g / 2, b / 2, matrix_width, matrix_height);
            if (x >= 0 && x < matrix_width && y >= 0 && y < matrix_height) {
                canvas->SetPixel(x, y, r, g, b);
                if (star.z < depth * 0.22f) {
                    const uint8_t gr = r / 4, gg = g / 4, gb = b / 4;
                    if (x > 0) canvas->SetPixel(x - 1, y, gr, gg, gb);
                    if (x + 1 < matrix_width) canvas->SetPixel(x + 1, y, gr, gg, gb);
                    if (y > 0) canvas->SetPixel(x, y - 1, gr, gg, gb);
                    if (y + 1 < matrix_height) canvas->SetPixel(x, y + 1, gr, gg, gb);
                }
            }
        }

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
        {"calm", "Calm drift", "Sparse, slow stars for a quiet background",
         {{"num_stars", 68}, {"speed", 0.012f}, {"streak_length", 0.30f}, {"drifting_center", true}, {"audio_reactive", false}},
         {"calm", "minimal"}, 0.22f, 0.35f, 0.20f, 0.24f},
        {"warp", "Warp drive", "Dense fast streaks with a strong sense of depth",
         {{"num_stars", 155}, {"speed", 0.072f}, {"streak_length", 1.35f}, {"enable_twinkle", false}},
         {"energetic", "depth"}, 0.88f, 0.96f, 0.55f, 0.42f},
        {"music", "Music flight", "Balanced starfield with beat-driven acceleration",
         {{"num_stars", 112}, {"speed", 0.030f}, {"streak_length", 0.82f}, {"audio_reactive", true}, {"audio_strength", 1.15f}},
         {"music", "beat-driven"}, 0.72f, 0.86f, 1.0f, 0.36f},
    };
    return d;
}
