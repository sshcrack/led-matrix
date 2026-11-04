#include "AmbientMusicScenes.h"

namespace Scenes {
using namespace AudioKit;

namespace {
constexpr float Focal = 62.0f;      // projection focal length (pixels)
constexpr float CamHeight = 1.4f;   // eye height above the road
constexpr float RoadHalf = 2.4f;    // half road width
constexpr float LampX = 3.6f;       // lateral lamp position
constexpr float LampHeight = 5.2f;
constexpr float LampSpacing = 7.0f; // world units between consecutive lamps (alternating sides)
constexpr float FarZ = 95.0f;
}  // namespace

void AudioNightDriveScene::register_properties()
{
    add_property(sensitivity_);
    add_property(baseSpeed_);
    add_property(bounceAmount_);
}

void AudioNightDriveScene::init_city()
{
    towers_.clear();
    int x = -4;
    uint32_t seed = 11;
    while (x < matrix_width + 4) {
        const int w = 5 + static_cast<int>(hash(seed) * 9);
        const int h = 6 + static_cast<int>(hash(seed * 3 + 1) * 20 * (1.0f - std::fabs(x - matrix_width * 0.5f) / matrix_width));
        towers_.push_back({x, w, h, seed, static_cast<int>(hash(seed * 5) * 8)});
        x += w - 1;
        seed += 7;
    }
    lamps_.clear();
    for (float z = 2.0f; z < FarZ; z += LampSpacing) lamps_.push_back({z, static_cast<int>(lamps_.size() % 2) * 2 - 1});
}

bool AudioNightDriveScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    if (towers_.empty())
        init_city();
    spectrum_.update(audio, dt, 0.05f, 0.30f);

    const float kickF = feature(audio, AudioProtocol::Feature::Kick);
    const float bass = spectrum_.band(0.0f, 0.22f);
    const float energy = spectrum_.level;
    if (events_.onsetHit && kickF > 0.3f) {
        kick_.hit(0.5f + 0.5f * kickF);
        bounceV_ -= (10.0f + 22.0f * kickF) * bounceAmount_->get();  // hydraulics kick the car up
    }
    if (events_.beatHit)
        beat_.hit();
    if (events_.dropHit) {
        drop_.hit();
        palette_.next();
    }
    if (events_.sectionHit)
        palette_.next();
    kick_.decay(dt, 0.15f);
    beat_.decay(dt, 0.25f);
    drop_.decay(dt, 2.0f);
    palette_.update(dt, 2.0f);
    // Damped spring for the bass bounce.
    bounceV_ += (-bounce_ * 160.0f - bounceV_ * 13.0f) * dt;
    bounce_ += bounceV_ * dt;

    // One lamp passes per beat: speed = spacing * beats per second.
    float bpm = audio.feature(AudioProtocol::Feature::Bpm);
    if (bpm < 60.0f || bpm > 200.0f)
        bpm = 95.0f;
    const float targetSpeed = LampSpacing * bpm / 60.0f * baseSpeed_->get() * (0.55f + 0.25f * energy + 0.6f * drop_.v);
    speed_ += (targetSpeed - speed_) * (1.0f - std::exp(-dt / 0.8f));
    travel_ += speed_ * dt;
    for (auto& l : lamps_) {
        l.z -= speed_ * dt;
        if (l.z < 0.6f)
            l.z += static_cast<float>(lamps_.size()) * LampSpacing;
    }

    const float hy = H * 0.44f + bounce_;
    const float cx = W * 0.5f + std::sin(travel_ * 0.013f) * 6.0f;  // gentle road curve
    const RGB skyTop = hsv(palette_.value, 0.75f, 0.10f), skyLow = hsv(palette_.value + 25.0f, 0.65f, 0.42f);
    const RGB lampCol{255, 160, 60};

    // --- sky --------------------------------------------------------------
    for (int y = 0; y < H; ++y) {
        const float t = std::clamp((y - 0.0f) / std::max(1.0f, hy), 0.0f, 1.0f);
        const RGB c = lerp(skyTop, skyLow, t * t);
        for (int x = 0; x < W; ++x) buf_.at(x, y) = c;
    }
    for (int i = 0; i < 50; ++i) {
        const int sx = static_cast<int>(hash(i * 5 + 3) * W), sy = static_cast<int>(hash(i * 11 + 7) * hy * 0.6f);
        buf_.add(sx, sy, RGB{140, 140, 170} * (0.3f + 0.4f * std::fabs(std::sin(time_ * 0.8f + i))));
    }
    buf_.disc(W * 0.80f, hy * 0.30f, 5.0f, RGB{235, 225, 205}, false);
    buf_.disc(W * 0.80f + 2.0f, hy * 0.30f - 1.5f, 4.2f, skyTop, false);  // crescent

    // --- skyline on the horizon; lit windows follow the spectrum ------------
    for (const auto& t : towers_) {
        const float level = spectrum_.value[static_cast<size_t>(std::clamp(t.band * 4, 0, 31))];
        const int top = static_cast<int>(hy) - t.h;
        for (int y = top; y < static_cast<int>(hy) + 1; ++y)
            for (int x = t.x; x < t.x + t.w; ++x) {
                RGB c{6, 5, 12};
                if ((x - t.x) % 2 == 1 && (y - top) % 3 == 1) {
                    const float h = hash(t.seed + static_cast<uint32_t>(x * 31 + y * 7));
                    if (h < 0.25f + level * 0.6f)
                        c = lerp(RGB{255, 190, 110}, hsv(palette_.value + 180.0f, 0.5f, 1.0f), h) * (0.45f + 0.5f * level);
                }
                buf_.set(x, y, c);
            }
    }

    // --- road ----------------------------------------------------------------
    for (int y = static_cast<int>(std::ceil(hy)); y < H; ++y) {
        const float dy = y + 0.5f - hy;
        if (dy <= 0.2f)
            continue;
        const float z = CamHeight * Focal / dy;
        const float fog = std::exp(-z / 60.0f);
        for (int x = 0; x < W; ++x) {
            const float X = (x + 0.5f - cx) * z / Focal;
            RGB c = lerp(skyLow * 0.5f, RGB{26, 24, 32}, fog);
            if (std::fabs(X) < RoadHalf) {
                const float dash = std::fmod(z + travel_, 4.0f);
                if (std::fabs(X) < 0.07f + 0.012f * z && dash < 1.8f)
                    c = lerp(c, RGB{200, 190, 150}, fog);
                if (std::fabs(std::fabs(X) - RoadHalf + 0.15f) < 0.06f + 0.012f * z)
                    c = lerp(c, RGB{150, 150, 160}, fog);
            } else {
                c = lerp(skyLow * 0.4f, RGB{4, 9, 6}, fog);  // verge
            }
            buf_.at(x, y) = c;
        }
    }
    // Lamp light pools on the road, then the lamps themselves.
    for (const auto& l : lamps_) {
        if (l.z > FarZ || l.z < 0.8f)
            continue;
        const float s = Focal / l.z;
        const float gx = cx + l.side * (LampX - 0.9f) * s, gy = hy + CamHeight * s;
        const float fade = std::exp(-l.z / 55.0f);
        const float r = std::min(18.0f, 1.8f * s);
        for (int y = static_cast<int>(gy - r * 0.35f); y <= static_cast<int>(gy + r * 0.35f); ++y)
            for (int x = static_cast<int>(gx - r); x <= static_cast<int>(gx + r); ++x) {
                const float d = std::hypot((x - gx) / r, (y - gy) / (r * 0.35f));
                if (d < 1.0f)
                    buf_.add(x, y, lampCol * ((1.0f - d) * (1.0f - d) * 0.22f * fade));
            }
    }
    std::vector<const Lamp*> order;
    for (const auto& l : lamps_) order.push_back(&l);
    std::sort(order.begin(), order.end(), [](const Lamp* a, const Lamp* b) { return a->z > b->z; });
    for (const auto* l : order) {
        if (l->z > 60.0f || l->z < 0.8f)  // far poles would converge into an X on the horizon
            continue;
        const float s = Focal / l->z;
        const float px = cx + l->side * LampX * s;
        const float baseY = hy + CamHeight * s, topY = hy + (CamHeight - LampHeight) * s;
        const float fade = std::exp(-l->z / 55.0f);
        const RGB pole = lerp(skyLow, RGB{20, 20, 26}, std::min(1.0f, fade * 1.6f));
        for (int y = static_cast<int>(topY); y <= static_cast<int>(baseY); ++y)
            for (int w = 0; w < std::max(1, static_cast<int>(0.15f * s)); ++w) buf_.set(static_cast<int>(px) + w, y, pole);
        // Arm reaching over the road and the glowing lamp head.
        const float headX = px - l->side * 1.0f * s;
        for (float x = std::min(px, headX); x <= std::max(px, headX); x += 0.5f) buf_.set(static_cast<int>(x), static_cast<int>(topY), pole);
        const float glow = 0.55f + 0.45f * beat_.v * (l->z < 25.0f ? 1.0f : 0.4f);
        if (topY > -4.0f) {
            buf_.disc(headX, topY + 0.6f, std::clamp(0.25f * s, 0.6f, 2.2f), lampCol * (fade * glow), true);
            buf_.disc(headX, topY + 0.6f, std::clamp(0.7f * s, 1.5f, 5.0f), lampCol * (0.10f * fade * glow), true);
        }
    }

    // --- car ahead: tail lights breathe with the bass ---------------------------
    {
        const float z = 9.5f + std::sin(time_ * 0.21f) * 2.0f;
        const float s = Focal / z;
        const float carX = cx + (1.1f + std::sin(time_ * 0.13f) * 0.2f) * s;
        const float carY = hy + CamHeight * s;
        const float bw = 1.7f * s, bh = 1.1f * s;
        for (int y = static_cast<int>(carY - bh); y < static_cast<int>(carY); ++y)
            for (int x = static_cast<int>(carX - bw / 2); x <= static_cast<int>(carX + bw / 2); ++x) buf_.set(x, y, RGB{8, 8, 12});
        const float tl = 0.35f + 0.65f * bass + 0.3f * drop_.v;
        for (int side = -1; side <= 1; side += 2) {
            const float lx = carX + side * bw * 0.38f, ly = carY - bh * 0.55f;
            buf_.disc(lx, ly, std::max(0.8f, 0.18f * s), RGB{255, 20, 25} * tl, true);
            buf_.disc(lx, ly, std::max(2.0f, 0.6f * s), RGB{255, 10, 20} * (0.12f * tl), true);
        }
    }

    // --- own hood: dark curve with passing lamp reflections ------------------------
    const float hoodTop = H - 16.0f - bounce_ * 0.35f;
    float reflect = 0.0f;
    for (const auto& l : lamps_)
        if (l.z < 9.0f)
            reflect = std::max(reflect, 1.0f - l.z / 9.0f);
    for (int x = 0; x < W; ++x) {
        const float u = (x + 0.5f) / W * 2.0f - 1.0f;
        const int top = static_cast<int>(hoodTop + u * u * 9.0f);
        const float su = (u + 0.15f) * 3.0f;
        const float colSheen = std::exp(-su * su);
        float rowFall = 1.0f;
        const float fallStep = std::exp(-1.0f / 3.0f);
        for (int y = std::max(0, top); y < H; ++y, rowFall *= fallStep) {
            const float sheen = colSheen * rowFall;
            const RGB edge = y - top < 1 ? skyLow * 0.6f : RGB{};
            buf_.set(x, y, RGB{12, 11, 18} + edge + lampCol * (sheen * reflect * 0.6f) + skyLow * (sheen * 0.35f));
        }
    }

    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioNightDriveScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "night-drive";
    d.tags = {"music", "city", "night", "road", "audio-reactive", "beat-driven", "music-showcase", "style:hiphop", "style:party",
              "style:edm", "drop-reactive"};
    d.intensity = 0.55f;
    d.motion = 0.62f;
    d.music_affinity = 1.0f;
    d.performance_cost = 0.35f;
    return d;
}
}  // namespace Scenes
