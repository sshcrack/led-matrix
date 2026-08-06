#include "AudioReactiveScenes.h"
#include "shared/matrix/plugin_loader/loader.h"
#include "shared/matrix/utils/color.h"
#include <algorithm>
#include <cmath>

using namespace Scenes;

namespace {
float average_range(const std::vector<uint8_t>& data, size_t begin, size_t end) {
    if (data.empty() || begin >= data.size()) return 0.0f;
    end = std::min(end, data.size());
    float sum = 0.0f;
    for (size_t i = begin; i < end; ++i) sum += data[i] / 255.0f;
    return end > begin ? sum / static_cast<float>(end - begin) : 0.0f;
}

void set_additive(rgb_matrix::FrameCanvas* canvas, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || y < 0 || x >= canvas->width() || y >= canvas->height()) return;
    canvas->SetPixel(x, y, r, g, b);
}

void hsv(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b) {
    while (h < 0.0f) h += 360.0f;
    while (h >= 360.0f) h -= 360.0f;
    color::hsv_to_rgb(h, std::clamp(s, 0.0f, 1.0f), std::clamp(v, 0.0f, 1.0f), r, g, b);
}
}

std::unique_ptr<Scenes::Scene> AudioParticleFieldSceneWrapper::create() { return std::make_unique<AudioParticleFieldScene>(); }
std::unique_ptr<Scenes::Scene> AudioPulseTunnelSceneWrapper::create() { return std::make_unique<AudioPulseTunnelScene>(); }

AudioParticleFieldScene::AudioParticleFieldScene() { find_plugin(); }
void AudioParticleFieldScene::find_plugin() {
    for (auto& p : Plugins::PluginManager::instance()->get_plugins()) {
        if (auto* av = dynamic_cast<AudioVisualizer*>(p)) { plugin = av; break; }
    }
}
void AudioParticleFieldScene::register_properties() {
    add_property(sensitivity); add_property(particle_limit); add_property(trail_strength);
    add_property(gravity); add_property(rainbow); add_property(base_color); add_property(beat_bursts);
}

void AudioParticleFieldScene::spawn_particle(float bass, float mids, float treble, bool burst) {
    if (particles.size() >= static_cast<size_t>(particle_limit->get())) return;
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    const float angle = burst ? unit(rng) * 6.2831853f : (-2.72f + unit(rng) * 2.30f);
    const float power = burst ? 35.0f + bass * 90.0f : 14.0f + bass * 46.0f;
    Particle p;
    p.x = burst ? matrix_width * 0.5f : unit(rng) * matrix_width;
    p.y = burst ? matrix_height * 0.55f : matrix_height - 2.0f;
    p.vx = std::cos(angle) * power + (mids - 0.5f) * 15.0f;
    p.vy = std::sin(angle) * power - (burst ? 0.0f : 18.0f + bass * 35.0f);
    const float persistence = 0.45f + trail_strength->get() * 1.8f;
    p.max_life = p.life = persistence + unit(rng) * (0.55f + treble * 1.2f);
    p.hue = hue_time * 38.0f + unit(rng) * 80.0f + treble * 120.0f;
    p.size = 1.0f + (burst ? bass * 2.0f : treble);
    particles.push_back(p);
}

bool AudioParticleFieldScene::render(rgb_matrix::FrameCanvas* canvas) {
    if (!plugin) find_plugin();
    if (!plugin) return false;
    const auto ft = timer.tick();
    const float dt = std::clamp(static_cast<float>(ft.deltaFrame.count()), 0.0f, 0.05f);
    const auto audio = plugin->get_audio_data();
    if (audio.empty()) { canvas->Clear(); return false; }

    const size_t n = audio.size();
    const float gain = sensitivity->get();
    const float bass = std::clamp(average_range(audio, 0, std::max<size_t>(2, n / 8)) * gain, 0.0f, 1.0f);
    const float mids = std::clamp(average_range(audio, n / 8, std::max<size_t>(n / 8 + 1, n * 5 / 8)) * gain, 0.0f, 1.0f);
    const float treble = std::clamp(average_range(audio, n * 5 / 8, n) * gain, 0.0f, 1.0f);
    hue_time += dt;

    canvas->Fill(0, 0, 0);

    const uint64_t beat = plugin->get_beat_counter();
    if (beat_bursts->get() && beat != seen_beat_counter) {
        seen_beat_counter = beat;
        const int burst_count = 30 + static_cast<int>(bass * 110.0f);
        for (int i = 0; i < burst_count; ++i) spawn_particle(bass, mids, treble, true);
    }

    spawn_accumulator += dt * (10.0f + bass * 95.0f + treble * 45.0f);
    while (spawn_accumulator >= 1.0f) { spawn_particle(bass, mids, treble, false); spawn_accumulator -= 1.0f; }

    for (auto& p : particles) {
        p.life -= dt;
        p.vy += gravity->get() * dt;
        p.vx += std::sin(hue_time * 1.7f + p.y * 0.08f) * mids * 12.0f * dt;
        p.x += p.vx * dt; p.y += p.vy * dt;
        if (p.x < 0) p.x += matrix_width; if (p.x >= matrix_width) p.x -= matrix_width;

        const float life = std::clamp(p.life / p.max_life, 0.0f, 1.0f);
        uint8_t r, g, b;
        if (rainbow->get()) hsv(p.hue + hue_time * 20.0f, 0.75f, life, r, g, b);
        else { auto c = base_color->get(); r = c.r * life; g = c.g * life; b = c.b * life; }
        const int x = static_cast<int>(std::round(p.x));
        const int y = static_cast<int>(std::round(p.y));
        set_additive(canvas, x, y, r, g, b);
        if (p.size > 1.6f) { set_additive(canvas, x + 1, y, r / 2, g / 2, b / 2); set_additive(canvas, x, y + 1, r / 2, g / 2, b / 2); }
    }
    std::erase_if(particles, [&](const Particle& p) { return p.life <= 0.0f || p.y > matrix_height + 4.0f; });
    return true;
}

AudioPulseTunnelScene::AudioPulseTunnelScene() { find_plugin(); }
void AudioPulseTunnelScene::find_plugin() {
    for (auto& p : Plugins::PluginManager::instance()->get_plugins()) {
        if (auto* av = dynamic_cast<AudioVisualizer*>(p)) { plugin = av; break; }
    }
}
void AudioPulseTunnelScene::register_properties() {
    add_property(sensitivity); add_property(speed); add_property(ring_count); add_property(twist);
    add_property(rainbow); add_property(base_color); add_property(show_spectrum_ribs);
}

bool AudioPulseTunnelScene::render(rgb_matrix::FrameCanvas* canvas) {
    if (!plugin) find_plugin();
    if (!plugin) return false;
    const auto ft = timer.tick();
    const float dt = std::clamp(static_cast<float>(ft.deltaFrame.count()), 0.0f, 0.05f);
    const auto audio = plugin->get_audio_data();
    if (audio.empty()) { canvas->Clear(); return false; }
    const size_t n = audio.size();
    const float gain = sensitivity->get();
    const float bass = std::clamp(average_range(audio, 0, std::max<size_t>(2, n / 8)) * gain, 0.0f, 1.0f);
    const float mids = std::clamp(average_range(audio, n / 8, std::max<size_t>(n / 8 + 1, n * 5 / 8)) * gain, 0.0f, 1.0f);
    const float highs = std::clamp(average_range(audio, n * 5 / 8, n) * gain, 0.0f, 1.0f);

    const uint64_t beat = plugin->get_beat_counter();
    if (beat != seen_beat_counter) { seen_beat_counter = beat; beat_pulse = 1.0f; }
    beat_pulse = std::max(0.0f, beat_pulse - dt * 3.2f);
    travel = std::fmod(travel + dt * speed->get() * (0.65f + bass * 1.5f), 1.0f);
    rotation += dt * (0.15f + mids * 0.8f) * twist->get();

    canvas->Clear();
    const float cx = matrix_width * 0.5f + std::sin(ft.t * 0.37f) * matrix_width * 0.055f * mids;
    const float cy = matrix_height * 0.5f + std::cos(ft.t * 0.29f) * matrix_height * 0.045f * mids;
    const float max_r = std::hypot(matrix_width, matrix_height) * 0.72f;
    const int rings = ring_count->get();

    for (int y = 0; y < matrix_height; ++y) for (int x = 0; x < matrix_width; ++x) {
        const float dx = x - cx, dy = y - cy;
        const float radius = std::sqrt(dx * dx + dy * dy);
        const float angle = std::atan2(dy, dx) + rotation;
        const float normalized = radius / max_r;
        const float warped = normalized + std::sin(angle * 5.0f + ft.t * 1.2f) * mids * 0.018f;
        const float phase = std::fmod(warped * rings - travel * rings + 100.0f, 1.0f);
        const float ring_line = std::exp(-std::pow((phase - 0.5f) / (0.045f + beat_pulse * 0.018f), 2.0f));
        float rib = 0.0f;
        if (show_spectrum_ribs->get()) {
            const float rib_phase = std::abs(std::sin(angle * (6.0f + highs * 10.0f)));
            rib = std::pow(std::max(0.0f, 1.0f - rib_phase), 12.0f) * (0.12f + highs * 0.7f);
        }
        float value = std::clamp((ring_line * (0.42f + bass * 0.75f + beat_pulse * 0.6f) + rib) * (0.35f + normalized), 0.0f, 1.0f);
        if (value < 0.025f) continue;
        uint8_t r, g, b;
        if (rainbow->get()) hsv(angle * 57.2958f + normalized * 210.0f + ft.t * 24.0f, 0.82f, value, r, g, b);
        else { auto c = base_color->get(); r = c.r * value; g = c.g * value; b = c.b * value; }
        canvas->SetPixel(x, y, r, g, b);
    }
    return true;
}
