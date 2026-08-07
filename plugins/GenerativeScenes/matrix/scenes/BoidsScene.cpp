#include "BoidsScene.h"
#include <algorithm>
#include <cmath>
#include <shared/matrix/audio_state.h>

namespace {
constexpr float PI = 3.14159265358979323846f;

void hsv_to_rgb(float h, float s, float v, uint8_t &r, uint8_t &g, uint8_t &b) {
    h = std::fmod(h, 360.0f); if (h < 0) h += 360.0f;
    const float c = v * s;
    const float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    const float m = v - c;
    float rf=0,gf=0,bf=0;
    if (h < 60) { rf=c; gf=x; } else if (h < 120) { rf=x; gf=c; }
    else if (h < 180) { gf=c; bf=x; } else if (h < 240) { gf=x; bf=c; }
    else if (h < 300) { rf=x; bf=c; } else { rf=c; bf=x; }
    r = static_cast<uint8_t>((rf+m)*255); g = static_cast<uint8_t>((gf+m)*255); b = static_cast<uint8_t>((bf+m)*255);
}
}

using namespace GenerativeScenes;

void BoidsScene::register_properties() {
    count_->label("Boid count").description("Number of flock members. Higher values make the flock denser but cost more CPU.").group("Flock");
    speed_->label("Cruise speed").description("Base movement speed before audio modulation.").group("Flock").step(0.05);
    perception_->label("Perception radius").description("How far each boid can see nearby flock members.").group("Flock").unit("px").step(1.0);
    trail_fade_->label("Trail persistence").description("How long motion trails remain visible.").group("Appearance").step(0.02);
    rainbow_->label("Direction colors").description("Color boids by travel direction instead of using one fixed color.").group("Appearance");
    color_->label("Boid color").description("Fixed boid color when direction colors are disabled.").group("Appearance").visible_if("rainbow", false);
    audio_reactive_->label("Audio reactive").description("Let bass, beats, stereo balance and high frequencies steer the flock.").group("Audio");
    audio_strength_->label("Audio strength").description("Overall amount of music-driven motion.").group("Audio").visible_if("audio_reactive", true).step(0.05);

    add_property(count_); add_property(speed_); add_property(perception_);
    add_property(trail_fade_); add_property(audio_reactive_); add_property(audio_strength_); add_property(rainbow_); add_property(color_);
}

void BoidsScene::initialize(int width, int height) {
    Scene::initialize(width, height);
    set_target_fps(60);
    ensure_buffers();
    reset_boids();
    simulation_.reset();
}

void BoidsScene::ensure_buffers() {
    framebuffer_.assign(static_cast<size_t>(matrix_width * matrix_height * 3), 0);
}

void BoidsScene::reset_boids() {
    const int n = std::clamp(count_->get(), 8, 180);
    std::uniform_real_distribution<float> px(0.0f, static_cast<float>(std::max(1, matrix_width - 1)));
    std::uniform_real_distribution<float> py(0.0f, static_cast<float>(std::max(1, matrix_height - 1)));
    std::uniform_real_distribution<float> angle(0.0f, 2.0f * PI);
    boids_.clear(); boids_.reserve(n);
    for (int i=0;i<n;++i) { const float a=angle(rng_); boids_.push_back({px(rng_),py(rng_),std::cos(a),std::sin(a)}); }
}

void BoidsScene::simulate_step() {
    const float perception = std::clamp(perception_->get(), 4.0f, 48.0f);
    const float p2 = perception * perception;
    const float audio_motion = audio_reactive_->get()
        ? std::clamp(1.0f + audio_bass_ * audio_strength_->get() * 1.15f
                     + beat_pulse_ * audio_strength_->get() * 0.42f
                     + drop_pulse_ * audio_strength_->get() * 0.95f,
                     1.0f, 2.75f)
        : 1.0f;
    const float max_speed = std::clamp(speed_->get(), 0.12f, 2.0f) * 0.55f * audio_motion;
    std::vector<Boid> next = boids_;

    for (size_t i=0;i<boids_.size();++i) {
        float ax=0, ay=0, cx=0, cy=0, sx=0, sy=0; int neighbours=0;
        for (size_t j=0;j<boids_.size();++j) {
            if (i==j) continue;
            float dx=boids_[j].x-boids_[i].x, dy=boids_[j].y-boids_[i].y;
            if (dx > matrix_width/2.0f) dx -= matrix_width; if (dx < -matrix_width/2.0f) dx += matrix_width;
            if (dy > matrix_height/2.0f) dy -= matrix_height; if (dy < -matrix_height/2.0f) dy += matrix_height;
            const float d2=dx*dx+dy*dy;
            if (d2>0.01f && d2<p2) {
                ax += boids_[j].vx; ay += boids_[j].vy;
                cx += dx; cy += dy;
                if (d2 < 36.0f) { sx -= dx / d2; sy -= dy / d2; }
                ++neighbours;
            }
        }
        float vx=boids_[i].vx + audio_balance_ * 0.012f, vy=boids_[i].vy;
        if (neighbours>0) {
            ax/=neighbours; ay/=neighbours; cx/=neighbours; cy/=neighbours;
            const float cohesion = 0.0025f * (1.0f + audio_mids_ * audio_strength_->get());
            const float separation = 0.22f * (1.0f + audio_treble_ * audio_strength_->get() * 1.6f);
            vx += ax*0.035f + cx*cohesion + sx*separation;
            vy += ay*0.035f + cy*cohesion + sy*separation;
        }
        const float len=std::sqrt(vx*vx+vy*vy);
        if (len>0.001f) { vx=vx/len*max_speed; vy=vy/len*max_speed; }
        next[i].vx=vx; next[i].vy=vy; next[i].x=boids_[i].x+vx; next[i].y=boids_[i].y+vy;
        if (next[i].x<0) next[i].x+=matrix_width; if (next[i].x>=matrix_width) next[i].x-=matrix_width;
        if (next[i].y<0) next[i].y+=matrix_height; if (next[i].y>=matrix_height) next[i].y-=matrix_height;
    }
    boids_.swap(next);

}

bool BoidsScene::render(rgb_matrix::FrameCanvas *canvas) {
    const int wanted = std::clamp(count_->get(), 8, 180);
    if (static_cast<int>(boids_.size()) != wanted) reset_boids();

    const float elapsed = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.20f);

    if (audio_reactive_->get()) {
        const auto audio = AudioState::snapshot();
        const bool has_audio = audio.fresh();
        const float response = 1.0f - std::exp(-elapsed * 9.0f);
        audio_bass_ += ((has_audio ? 0.5f * (audio.feature(AudioProtocol::Feature::SubBass) + audio.feature(AudioProtocol::Feature::Bass)) : 0.0f) - audio_bass_) * response;
        audio_mids_ += ((has_audio ? (audio.feature(AudioProtocol::Feature::LowMid) + audio.feature(AudioProtocol::Feature::Mid) + audio.feature(AudioProtocol::Feature::HighMid)) / 3.0f : 0.0f) - audio_mids_) * response;
        audio_treble_ += ((has_audio ? 0.5f * (audio.feature(AudioProtocol::Feature::Treble) + audio.feature(AudioProtocol::Feature::Air)) : 0.0f) - audio_treble_) * response;
        audio_balance_ += ((has_audio ? audio.feature(AudioProtocol::Feature::StereoBalance) : 0.0f) - audio_balance_) * response;
        if (has_audio && audio.beat_counter != last_beat_counter_) {
            last_beat_counter_ = audio.beat_counter;
            beat_pulse_ = std::max(beat_pulse_, 0.65f + audio.feature(AudioProtocol::Feature::Kick) * 0.35f);
            const float impulse = 0.08f + audio.feature(AudioProtocol::Feature::Kick) * 0.28f;
            const float cx = matrix_width * 0.5f, cy = matrix_height * 0.5f;
            for (auto &boid : boids_) {
                float dx = boid.x - cx, dy = boid.y - cy;
                const float length = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
                boid.vx += dx / length * impulse; boid.vy += dy / length * impulse;
            }
        }
        if (has_audio && audio.drop_counter != last_drop_counter_) {
            last_drop_counter_ = audio.drop_counter;
            drop_pulse_ = 1.0f;
            const float cx = matrix_width * 0.5f, cy = matrix_height * 0.5f;
            for (auto &boid : boids_) {
                const float dx = boid.x - cx, dy = boid.y - cy;
                const float tangent_x = -dy;
                const float tangent_y = dx;
                const float length = std::max(1.0f, std::sqrt(tangent_x * tangent_x + tangent_y * tangent_y));
                boid.vx += tangent_x / length * 0.55f;
                boid.vy += tangent_y / length * 0.55f;
            }
        }
    } else {
        audio_bass_ = audio_mids_ = audio_treble_ = audio_balance_ = 0.0f;
        beat_pulse_ = drop_pulse_ = 0.0f;
    }

    beat_pulse_ = std::max(0.0f, beat_pulse_ - elapsed * 2.8f);
    drop_pulse_ = std::max(0.0f, drop_pulse_ - elapsed * 1.35f);

    simulation_.advance(elapsed, [&](double) { simulate_step(); });

    // Trail decay is based on elapsed real time, not the number of display frames.
    const float configured_fade = std::clamp(trail_fade_->get(), 0.0f, 0.96f);
    const float fade = std::pow(configured_fade, elapsed * 30.0f);
    for (auto &v : framebuffer_) v = static_cast<uint8_t>(v * fade);

    for (const auto &b: boids_) {
        uint8_t r,g,bl;
        const float audio_glow = audio_reactive_->get()
            ? std::clamp(0.82f + audio_treble_ * audio_strength_->get() * 0.32f + beat_pulse_ * 0.22f, 0.65f, 1.0f)
            : 1.0f;
        if (rainbow_->get()) hsv_to_rgb(std::atan2(b.vy,b.vx)*180.0f/PI+180.0f,0.8f,audio_glow,r,g,bl);
        else {
            const auto c=color_->get();
            r=static_cast<uint8_t>(c.r * audio_glow);
            g=static_cast<uint8_t>(c.g * audio_glow);
            bl=static_cast<uint8_t>(c.b * audio_glow);
        }
        const int x=static_cast<int>(std::round(b.x)), y=static_cast<int>(std::round(b.y));
        for (int oy=-1;oy<=1;++oy) for (int ox=-1;ox<=1;++ox) {
            const int px=(x+ox+matrix_width)%matrix_width, py=(y+oy+matrix_height)%matrix_height;
            const float gain=(ox==0&&oy==0)?1.0f:0.35f; const size_t idx=static_cast<size_t>((py*matrix_width+px)*3);
            framebuffer_[idx]=std::max(framebuffer_[idx],static_cast<uint8_t>(r*gain));
            framebuffer_[idx+1]=std::max(framebuffer_[idx+1],static_cast<uint8_t>(g*gain));
            framebuffer_[idx+2]=std::max(framebuffer_[idx+2],static_cast<uint8_t>(bl*gain));
        }
    }
    for (int y=0;y<matrix_height;++y) for (int x=0;x<matrix_width;++x) {
        const size_t idx=static_cast<size_t>((y*matrix_width+x)*3);
        canvas->SetPixel(x,y,framebuffer_[idx],framebuffer_[idx+1],framebuffer_[idx+2]);
    }
    wait_until_next_frame(); return true;
}

std::unique_ptr<Scenes::Scene> BoidsSceneWrapper::create() { return std::make_unique<BoidsScene>(); }
