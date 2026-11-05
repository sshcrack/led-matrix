#include "MusicVistaScenes.h"

namespace Scenes {
using namespace AudioKit;

void AudioBoomboxScene::register_properties()
{
    add_property(sensitivity_);
    add_property(pump_);
    add_property(mural_on_);
}

void AudioBoomboxScene::init_world()
{
    const int W = matrix_width, H = matrix_height;
    bricks_.assign(static_cast<size_t>(W * H), 0.0f);
    mural_.assign(static_cast<size_t>(W * H), 0.0f);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const int row = y / 5;
            const int bx = (x + (row % 2) * 5) / 10;
            const bool mortar = y % 5 == 0 || (x + (row % 2) * 5) % 10 == 0;
            bricks_[static_cast<size_t>(y * W + x)] = mortar ? -1.0f : 0.75f + 0.25f * hash(static_cast<uint32_t>(row * 977 + bx * 31));
        }
    // Abstract mural: a few overlapping soft-edged shapes (no lettering).
    // Stored as band index + 1 so each shape lights with its own band.
    struct Shape {
        float x, y, rx, ry, rot;
        int band;
    };
    const Shape shapes[] = {{0.22f, 0.30f, 0.16f, 0.10f, 0.4f, 1},  {0.48f, 0.22f, 0.12f, 0.12f, 0.0f, 3},
                            {0.74f, 0.33f, 0.18f, 0.08f, -0.5f, 5}, {0.36f, 0.45f, 0.10f, 0.06f, 0.9f, 7},
                            {0.62f, 0.48f, 0.09f, 0.09f, 0.0f, 9},  {0.88f, 0.18f, 0.07f, 0.11f, 0.3f, 11}};
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            for (const auto& s : shapes) {
                const float dx = (x + 0.5f) / W - s.x, dy = (y + 0.5f) / H - s.y;
                const float u = dx * std::cos(s.rot) + dy * std::sin(s.rot), v = -dx * std::sin(s.rot) + dy * std::cos(s.rot);
                const float d = (u * u) / (s.rx * s.rx) + (v * v) / (s.ry * s.ry);
                // Shape outline (spray-paint rim) gets a .5 marker.
                if (d < 1.0f)
                    mural_[static_cast<size_t>(y * W + x)] = static_cast<float>(s.band) + (d > 0.75f ? 0.5f : 0.0f) + 1.0f;
            }
    eq_.assign(10, 0.0f);
    rings_.clear();
    litter_.clear();
}

bool AudioBoomboxScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    if (static_cast<int>(bricks_.size()) != W * H)
        init_world();
    spectrum_.update(audio, dt, 0.04f, 0.25f);

    const float kickF = feature(audio, AudioProtocol::Feature::Kick);
    const float snareF = feature(audio, AudioProtocol::Feature::Snare);
    const float bass = spectrum_.band(0.0f, 0.2f);
    const float energy = spectrum_.level;
    if (events_.beatHit)
        beat_.hit();
    if (events_.onsetHit && kickF > 0.3f) {
        kick_.hit(0.5f + 0.5f * kickF);
        rings_.push_back({4.0f, 0.5f + 0.5f * kickF, 0});
        rings_.push_back({4.0f, 0.5f + 0.5f * kickF, 1});
    }
    if (events_.onsetHit && snareF > 0.4f) {
        snare_.hit(snareF);
        netSwing_ += 0.6f * snareF;
    }
    if (events_.dropHit) {
        drop_.hit();
        palette_.next();
        for (int i = 0; i < 14; ++i)
            litter_.push_back({rnd() * W * 0.4f, H - 4.0f - rnd() * 6.0f, 20.0f + rnd() * 40.0f, -(5.0f + rnd() * 20.0f), rnd() * Tau,
                               20.0f + rnd() * 30.0f});
    }
    if (events_.sectionHit)
        palette_.next();
    beat_.decay(dt, 0.2f);
    kick_.decay(dt, 0.14f);
    snare_.decay(dt, 0.15f);
    drop_.decay(dt, 2.0f);
    palette_.update(dt, 1.0f);

    // Speaker cone: a damped spring kicked by the bass.
    const float pump = pump_->get();
    coneV_ += (kick_.v * 1.2f + bass * 0.6f) * pump * 40.0f * dt;
    coneV_ += (-cone_ * 260.0f - coneV_ * 18.0f) * dt;
    cone_ += coneV_ * dt;
    netSwing_ *= std::exp(-dt * 1.5f);

    const int ground = H - 22;
    const RGB tint = hsv(palette_.value, 0.75f, 1.0f);
    const float lampX = W * 0.80f, lampY = 10.0f;

    // --- brick wall lit by the street lamp, mural on top --------------------------------------
    for (int y = 0; y < ground; ++y)
        for (int x = 0; x < W; ++x) {
            const size_t i = static_cast<size_t>(y * W + x);
            const float d = std::hypot((x - lampX) / W, (y - lampY) / H * 0.8f);
            const float lamp = 0.30f + 0.85f * std::exp(-d * d * 6.0f);
            const float brick = bricks_[i];
            RGB c = brick < 0 ? RGB{22, 20, 22} : RGB{64, 30, 24} * brick;
            c = c * lamp;
            if (mural_on_->get() && mural_[i] > 0.0f) {
                const float m = mural_[i] - 1.0f;
                const int band = static_cast<int>(m);
                const bool rim = (m - band) > 0.25f;
                const float lv = spectrum_.band(band / 12.0f, (band + 1) / 12.0f);
                const RGB paint = hsv(palette_.value + band * 32.0f, 0.75f, 1.0f);
                // Paint is visible under the lamp; the band makes it glow like UV paint.
                RGB p = paint * (0.16f * lamp + 0.40f * lv + 0.08f * beat_.v);
                if (rim)
                    p = RGB{230, 230, 230} * (0.12f * lamp + 0.18f * lv);
                c = brick < 0 ? p * 0.7f : p;
            }
            buf_.at(x, y) = c;
        }

    // --- basketball hoop on the wall; the net swings on snares -------------------------------------
    {
        const float hx = W * 0.22f, hy = H * 0.20f;
        for (int x = static_cast<int>(hx - 9); x <= static_cast<int>(hx + 9); ++x)
            for (int y = static_cast<int>(hy - 10); y <= static_cast<int>(hy + 2); ++y) {
                const bool edge = x == static_cast<int>(hx - 9) || x == static_cast<int>(hx + 9) || y == static_cast<int>(hy - 10) ||
                                  y == static_cast<int>(hy + 2);
                if (edge)
                    buf_.set(x, y, RGB{140, 140, 140} * 0.45f);
            }
        for (int x = static_cast<int>(hx - 5); x <= static_cast<int>(hx + 5); ++x) buf_.set(x, static_cast<int>(hy + 4), RGB{170, 60, 20});
        const float swing = std::sin(time_ * 9.0f) * netSwing_;
        for (int k = 0; k < 6; ++k) {
            const float t = k / 5.0f;
            const float lx = hx - 5 + t * 10;
            for (int s = 0; s < 8; ++s) {
                const float fy = hy + 5 + s;
                const float fx = lx + (hx - lx) * (s / 8.0f) * 0.5f + swing * s * 0.4f;
                buf_.splat(fx, fy, RGB{90, 90, 95} * 0.6f);
            }
        }
    }

    // --- street lamp -------------------------------------------------------------------------------
    for (int y = static_cast<int>(lampY); y < ground; ++y) buf_.set(static_cast<int>(lampX + 8), y, RGB{18, 18, 22});
    for (int x = static_cast<int>(lampX - 1); x <= static_cast<int>(lampX + 8); ++x) buf_.set(x, static_cast<int>(lampY - 1), RGB{18, 18, 22});
    buf_.disc(lampX, lampY + 1.0f, 2.2f, RGB{255, 210, 140});

    // --- sidewalk ---------------------------------------------------------------------------------
    for (int y = ground; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const float d = std::hypot((x - lampX) / W, (y - ground) / H * 2.0f);
            const bool seam = x % 24 == 0 || y == ground;
            const RGB base = (seam ? RGB{18, 18, 20} : RGB{34, 32, 36}) * (0.35f + 0.8f * std::exp(-d * d * 6.0f));
            buf_.at(x, y) = base;
        }

    // --- the boombox ------------------------------------------------------------------------------
    const float bx = W * 0.50f, by = ground - 8.0f;  // centre of the box
    const float bw = W * 0.42f, bh = H * 0.21f;
    const int x0 = static_cast<int>(bx - bw / 2), x1 = static_cast<int>(bx + bw / 2);
    const int y0 = static_cast<int>(by - bh / 2), y1 = static_cast<int>(by + bh / 2);
    // Soft shadow on the pavement.
    for (int x = x0 - 2; x <= x1 + 2; ++x) buf_.mix(x, y1 + 1, RGB{0, 0, 0}, 0.6f);
    // Handle.
    for (int x = x0 + 8; x <= x1 - 8; ++x) buf_.set(x, y0 - 6, RGB{60, 60, 66});
    for (int y = y0 - 6; y < y0; ++y) {
        buf_.set(x0 + 8, y, RGB{60, 60, 66});
        buf_.set(x1 - 8, y, RGB{60, 60, 66});
    }
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            const bool edge = x == x0 || x == x1 || y == y0 || y == y1;
            buf_.at(x, y) = edge ? RGB{90, 90, 100} : RGB{38, 38, 44};
        }
    // EQ display in the middle: the spectrum in ten little LED columns.
    const int eqX0 = static_cast<int>(bx - 9), eqY1 = static_cast<int>(by + bh * 0.20f), eqH = static_cast<int>(bh * 0.45f);
    for (int b = 0; b < 10; ++b) {
        auto& lv = eq_[static_cast<size_t>(b)];
        lv += (spectrum_.band(b / 10.0f, (b + 1) / 10.0f) - lv) * (1.0f - std::exp(-dt / 0.05f));
        const int h = static_cast<int>(lv * eqH + 0.5f);
        for (int k = 0; k < eqH; ++k) {
            const RGB c = k < h ? hsv(palette_.value + 120.0f * k / eqH, 0.8f, 1.0f) * 0.9f : RGB{10, 12, 14};
            buf_.set(eqX0 + b * 2, eqY1 - k, c);
        }
    }
    // Tape deck window above the EQ.
    for (int x = static_cast<int>(bx - 9); x <= static_cast<int>(bx + 9); ++x) buf_.set(x, y0 + 3, RGB{20, 20, 24});
    for (int r = 0; r < 2; ++r) {
        const float rx = bx - 5 + r * 10, ry = y0 + 5.5f;
        const float a = time_ * 2.5f + r;
        buf_.disc(rx, ry, 2.0f, RGB{22, 22, 26}, false);
        buf_.set(static_cast<int>(rx + std::cos(a) * 1.5f), static_cast<int>(ry + std::sin(a) * 1.5f), RGB{120, 120, 130});
    }
    // Speakers: cones bulge outward on bass hits.
    const float speakerR = bh * 0.38f;
    for (int s = 0; s < 2; ++s) {
        const float sx = s == 0 ? x0 + bw * 0.20f : x1 - bw * 0.20f;
        const float push = std::clamp(cone_ * 6.0f, -0.6f, 1.6f);
        const float r = speakerR + push;
        for (int y = static_cast<int>(by - r - 1); y <= static_cast<int>(by + r + 1); ++y)
            for (int x = static_cast<int>(sx - r - 1); x <= static_cast<int>(sx + r + 1); ++x) {
                const float d = std::hypot(x + 0.5f - sx, y + 0.5f - by) / r;
                if (d > 1.0f)
                    continue;
                // Concentric grille rings, lit from the lamp at the top right.
                const float ring = 0.5f + 0.5f * std::cos(d * 18.0f - push * 3.0f);
                const float light = 0.5f + 0.5f * ((x - sx) - (y - by)) / (r * 1.5f);
                RGB c = RGB{20, 20, 24} + RGB{60, 60, 68} * (ring * 0.5f * light);
                if (d < 0.28f)  // dust cap
                    c = RGB{70, 70, 78} * (0.6f + 0.6f * light + 0.4f * std::max(0.0f, push));
                if (d > 0.9f)
                    c = RGB{90, 90, 100};
                buf_.at(x, y) = c;
            }
        // Small accent LED next to each speaker.
        buf_.disc(sx, y0 + 2.5f, 0.9f, tint * (0.4f + 0.6f * beat_.v));
    }

    // --- sound rings pushed out of the speakers ----------------------------------------------------
    for (auto& r : rings_) {
        r.r += dt * (45.0f + energy * 30.0f);
        r.strength *= std::exp(-dt / 0.55f);
    }
    std::erase_if(rings_, [](const Ring& r) { return r.strength < 0.04f || r.r > 90.0f; });
    for (const auto& ring : rings_) {
        const float sx = ring.side == 0 ? x0 + bw * 0.20f : x1 - bw * 0.20f;
        const int steps = static_cast<int>(ring.r * 4.0f);
        // Only the outward-facing half (away from the box) is drawn.
        for (int k = 0; k <= steps; ++k) {
            const float a = (ring.side == 0 ? Pi * 0.5f : -Pi * 0.5f) + Pi * k / steps;
            const float px = sx + std::cos(a) * ring.r, py = by + std::sin(a) * ring.r * 0.8f;
            if (py > ground)
                continue;
            const bool overBox = px > x0 - 1 && px < x1 + 1 && py > y0 - 7 && py < y1 + 1;
            if (!overBox)
                buf_.splat(px, py, tint * (ring.strength * 0.55f));
        }
    }

    // --- litter / leaves blown across on drops -------------------------------------------------------
    for (auto& l : litter_) {
        l.vy += 25.0f * dt;
        l.x += l.vx * dt;
        l.y += l.vy * dt;
        if (l.y > H - 3) {
            l.y = H - 3;
            l.vy = -l.vy * 0.3f;
            l.vx *= 0.8f;
        }
        l.spin += dt * 6.0f;
    }
    std::erase_if(litter_, [&](const Leaf& l) { return l.x > W + 3 || std::fabs(l.vx) < 1.0f; });
    for (const auto& l : litter_) {
        buf_.set(static_cast<int>(l.x), static_cast<int>(l.y), hsv(l.hue, 0.7f, 0.5f));
        buf_.set(static_cast<int>(l.x + std::cos(l.spin)), static_cast<int>(l.y + std::sin(l.spin) * 0.6f), hsv(l.hue, 0.7f, 0.35f));
    }

    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioBoomboxScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "boombox";
    d.tags = {"music", "street", "night", "urban", "spectrum", "audio-reactive", "beat-driven", "bass-driven", "music-showcase",
              "style:hiphop", "style:party"};
    d.intensity = 0.55f;
    d.motion = 0.45f;
    d.music_affinity = 1.0f;
    d.performance_cost = 0.38f;
    return d;
}
}  // namespace Scenes
