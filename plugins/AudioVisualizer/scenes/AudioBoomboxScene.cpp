#include <array>

#include "MusicVistaScenes.h"
#include "kit/GenreVistaKit.h"

namespace Scenes {
using namespace AudioKit;

namespace {
// Layout in a 128 x 128 design space; scaled to the panel in init_world()/render().
constexpr float BodyX0 = 12, BodyX1 = 116, BodyY0 = 42, BodyY1 = 88;
constexpr float SpeakerY = 67, SpeakerL = 35, SpeakerR = 93, SpeakerOuter = 19, ConeR = 14;
constexpr float DeckX0 = 56, DeckX1 = 72;
constexpr float StepTop = 88, TreadH = 3, RiserH = 11;

RGB chrome(float t)  // t: 0 = top edge .. 1 = bottom edge of the body
{
    if (t < 0.5f)
        return lerp(RGB{204, 206, 214}, RGB{150, 152, 164}, t * 2.0f);
    return lerp(RGB{150, 152, 164}, RGB{106, 108, 120}, (t - 0.5f) * 2.0f);
}
}  // namespace

void AudioBoomboxScene::register_properties()
{
    add_property(sensitivity_);
    add_property(pump_);
}

void AudioBoomboxScene::init_world()
{
    const int W = matrix_width, H = matrix_height;
    const float sx = W / 128.0f, sy = H / 128.0f;
    backdrop_.assign(static_cast<size_t>(W * H), RGB{});
    sun_.assign(static_cast<size_t>(W * H), 0.0f);
    auto at = [&](int x, int y) -> RGB& { return backdrop_[static_cast<size_t>(y * W + x)]; };
    auto inside = [&](int x, int y) { return x >= 0 && y >= 0 && x < W && y < H; };

    // --- brownstone facade in late-afternoon sun; a building to the left shades its lower left --------------------------
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const float dx = x / sx, dy = y / sy;
            const float lit = smoothstep(-2.0f, 2.0f, dx - (6.0f + 0.42f * dy));
            sun_[static_cast<size_t>(y * W + x)] = lit;
            const float n = hash(static_cast<uint32_t>(x * 613 + y * 1291));
            const bool course = static_cast<int>(dy) % 12 == 11;
            RGB stone = RGB{150, 86, 62} * (0.90f + 0.10f * n);
            if (course)
                stone = stone * 0.78f;
            at(x, y) = stone * (0.48f + 0.62f * lit);
        }
    // Tall window with a cream frame; the glass reflects the sky.
    {
        const int x0 = static_cast<int>(66 * sx), x1 = static_cast<int>(110 * sx), y0 = static_cast<int>(2 * sy),
                  y1 = static_cast<int>(36 * sy);
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x) {
                const bool frame = x < x0 + 2 || x >= x1 - 2 || y < y0 + 2 || y >= y1 - 2 || std::abs(x - (x0 + x1) / 2) < 1 ||
                                   std::abs(y - (y0 + y1) / 2) < 1;
                const float lit = sun_[static_cast<size_t>(y * W + x)];
                if (frame)
                    at(x, y) = RGB{222, 210, 186} * (0.55f + 0.45f * lit);
                else {
                    const float streak = fract((x - y * 0.8f) / (18.0f * sx));
                    const float t = static_cast<float>(y - y0) / (y1 - y0);
                    RGB glass = lerp(RGB{90, 130, 170}, RGB{40, 58, 80}, t);
                    if (streak < 0.18f)
                        glass = glass + RGB{34, 40, 46};
                    at(x, y) = glass * (0.7f + 0.3f * lit);
                }
            }
        // Sill.
        for (int x = x0 - 2; x < x1 + 2; ++x)
            for (int y = y1; y < y1 + 2; ++y)
                if (inside(x, y))
                    at(x, y) = RGB{200, 190, 170} * (0.55f + 0.45f * sun_[static_cast<size_t>(y * W + x)]);
    }

    // --- stone stoop: three steps -------------------------------------------------------------------------------------
    for (int k = 0; k < 4; ++k) {
        const float top = StepTop + k * (TreadH + RiserH);
        for (int y = static_cast<int>(top * sy); y < std::min(H, static_cast<int>((top + TreadH + RiserH) * sy)); ++y)
            for (int x = 0; x < W; ++x) {
                const bool tread = y < static_cast<int>((top + TreadH) * sy);
                const float n = hash(static_cast<uint32_t>(x * 37 + y * 7919));
                RGB c = tread ? RGB{184, 170, 150}
                              : RGB{150, 136, 120} * (0.85f + 0.15f * static_cast<float>(y - (top + TreadH) * sy) / (RiserH * sy));
                c = c * (0.92f + 0.08f * n);
                const float lit = smoothstep(-2.0f, 2.0f, x / sx - (6.0f + 0.42f * (y / sy)));
                sun_[static_cast<size_t>(y * W + x)] = lit;
                at(x, y) = c * (0.55f + 0.50f * lit);
            }
    }

    // --- the boombox body (moving parts are drawn in render) --------------------------------------------------------
    const int bx0 = static_cast<int>(BodyX0 * sx), bx1 = static_cast<int>(BodyX1 * sx);
    const int by0 = static_cast<int>(BodyY0 * sy), by1 = static_cast<int>(BodyY1 * sy);
    // Cast shadow on the wall (sun from the upper left) and on the step.
    for (int y = by0 + 3; y < by1 + 1; ++y)
        for (int x = bx0 + 5; x < bx1 + 6; ++x)
            if (inside(x, y))
                at(x, y) = at(x, y) * 0.62f;
    for (int y = by1; y < by1 + static_cast<int>(3 * sy); ++y)
        for (int x = bx0 + 2; x < bx1 + 9; ++x)
            if (inside(x, y))
                at(x, y) = at(x, y) * 0.55f;
    // Antenna and handle.
    {
        const float ax0 = 106 * sx, ay0 = BodyY0 * sy, ax1 = 124 * sx, ay1 = 3 * sy;
        const int n = static_cast<int>(std::max(std::fabs(ax1 - ax0), std::fabs(ay1 - ay0)));
        for (int i = 0; i <= n; ++i) {
            const int x = static_cast<int>(ax0 + (ax1 - ax0) * i / n), y = static_cast<int>(ay0 + (ay1 - ay0) * i / n);
            if (inside(x, y))
                at(x, y) = RGB{210, 212, 220};
            if (inside(x + 1, y))
                at(x + 1, y) = RGB{110, 112, 120};
        }
        if (inside(static_cast<int>(ax1), static_cast<int>(ay1)))
            at(static_cast<int>(ax1), static_cast<int>(ay1)) = RGB{240, 240, 246};
    }
    for (int y = static_cast<int>(32 * sy); y < by0; ++y)
        for (int x = static_cast<int>(30 * sx); x < static_cast<int>(98 * sx); ++x) {
            const bool post = x < static_cast<int>(34 * sx) || x >= static_cast<int>(94 * sx);
            const bool bar = y < static_cast<int>(36 * sy);
            if (post || bar)
                at(x, y) = bar ? lerp(RGB{70, 70, 78}, RGB{28, 28, 32}, static_cast<float>(y - 32 * sy) / (4 * sy)) : RGB{40, 40, 46};
        }
    for (int y = by0; y < by1; ++y)
        for (int x = bx0; x < bx1; ++x) {
            const float t = static_cast<float>(y - by0) / (by1 - by0);
            const bool corner = (x == bx0 || x == bx1 - 1) && (y == by0 || y == by1 - 1);
            if (corner)
                continue;
            RGB c = chrome(t);
            if (x == bx0 || x == bx1 - 1 || y == by1 - 1)
                c = RGB{46, 46, 52};
            else if (y == by0)
                c = RGB{250, 250, 255};
            else if (x == bx0 + 1)
                c = c * 1.12f;
            at(x, y) = c;
        }
    // Piano keys on the top edge.
    for (int k = 0; k < 6; ++k) {
        const int kx = static_cast<int>((42 + k * 7.5f) * sx);
        for (int y = static_cast<int>((BodyY0 - 3) * sy); y < by0; ++y)
            for (int x = kx; x < kx + static_cast<int>(6 * sx); ++x) at(x, y) = k == 2 ? RGB{60, 60, 66} : RGB{196, 198, 206};
    }
    // Tuning strip across the top: dark glass with tick marks.
    for (int y = static_cast<int>(45 * sy); y < static_cast<int>(49 * sy); ++y)
        for (int x = static_cast<int>(24 * sx); x < static_cast<int>(104 * sx); ++x) {
            const bool tick = (x % 4 == 0) && y >= static_cast<int>(47 * sy);
            at(x, y) = tick ? RGB{120, 150, 150} : RGB{22, 30, 34};
        }
    // Speaker housings: perforated grille ring around the cone, chrome bezel.
    for (const float scx : {SpeakerL, SpeakerR}) {
        const float cx = scx * sx, cy = SpeakerY * sy, R = SpeakerOuter * sx;
        for (int y = static_cast<int>(cy - R - 2); y <= static_cast<int>(cy + R + 2); ++y)
            for (int x = static_cast<int>(cx - R - 2); x <= static_cast<int>(cx + R + 2); ++x) {
                if (!inside(x, y))
                    continue;
                const float d = len(x + 0.5f - cx, y + 0.5f - cy);
                if (d > R + 1.0f)
                    continue;
                if (d > R - 1.5f) {
                    // Bezel: bright on the upper left, darker lower right.
                    const float l = 0.5f - 0.5f * ((x + 0.5f - cx) + (y + 0.5f - cy)) / (d * 1.41f + 0.01f);
                    at(x, y) = lerp(RGB{90, 92, 100}, RGB{245, 246, 252}, l);
                }
                else
                    at(x, y) = ((x + y) % 2 == 0) ? RGB{44, 44, 50} : RGB{20, 20, 24};
            }
    }
    // Tape deck window between the speakers, EQ window below it.
    for (int y = static_cast<int>(52 * sy); y < static_cast<int>(84 * sy); ++y)
        for (int x = static_cast<int>(DeckX0 * sx); x < static_cast<int>(DeckX1 * sx); ++x) {
            const bool rim = x == static_cast<int>(DeckX0 * sx) || x == static_cast<int>(DeckX1 * sx) - 1 ||
                             y == static_cast<int>(52 * sy) || y == static_cast<int>(84 * sy) - 1 || y == static_cast<int>(70 * sy);
            at(x, y) = rim ? RGB{70, 72, 80} : RGB{16, 18, 22};
        }
    // The cassette behind the deck window.
    for (int y = static_cast<int>(55 * sy); y < static_cast<int>(68 * sy); ++y)
        for (int x = static_cast<int>(58 * sx); x < static_cast<int>(70 * sx); ++x) {
            const bool label = y < static_cast<int>(60 * sy);
            at(x, y) = label ? RGB{224, 196, 120} : RGB{58, 52, 48};
        }
    for (int x = static_cast<int>(58 * sx); x < static_cast<int>(70 * sx); ++x) at(x, static_cast<int>(57 * sy)) = RGB{220, 70, 60};
    // Small sliders on the right of the deck.
    for (int k = 0; k < 2; ++k) {
        const int x = static_cast<int>((BodyX1 - 5 - k * 3) * sx);
        for (int y = static_cast<int>(54 * sy); y < static_cast<int>(80 * sy); ++y) at(x, y) = RGB{70, 72, 80};
    }

    motes_.clear();
    for (int i = 0; i < 26; ++i) motes_.push_back({rnd() * W, rnd() * H * 0.85f, rnd() * Tau, 0.5f + rnd()});
    eq_.assign(5, 0.0f);
    eqPeak_.assign(5, 0.3f);
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
    const float sx = W / 128.0f, sy = H / 128.0f;
    if (static_cast<int>(backdrop_.size()) != W * H)
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
    }
    if (events_.onsetHit && kickF > 0.55f) {  // rings only on strong hits
        rings_.push_back({SpeakerOuter * sx, 0.5f + 0.5f * kickF, 0});
        rings_.push_back({SpeakerOuter * sx, 0.5f + 0.5f * kickF, 1});
    }
    if (events_.onsetHit && snareF > 0.4f)
        snare_.hit(snareF);
    if (events_.dropHit) {
        drop_.hit();
        palette_.next();
        for (int i = 0; i < 14; ++i)
            litter_.push_back({-rnd() * 10.0f, H * 0.70f + rnd() * H * 0.25f, 25.0f + rnd() * 40.0f, -(5.0f + rnd() * 20.0f), rnd() * Tau,
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
    coneV_ += (kick_.v * 1.2f + bass * 0.6f) * pump * 60.0f * dt;
    coneV_ += (-cone_ * 260.0f - coneV_ * 18.0f) * dt;
    cone_ += coneV_ * dt;
    reel_ += dt * (2.0f + 2.0f * energy);
    const float push = std::clamp(cone_ * 6.0f, -0.6f, 1.6f);

    std::copy(backdrop_.begin(), backdrop_.end(), buf_.px.begin());

    // --- dust motes drifting in the sunlight; they jump a little on beats ------------------------------------------------
    for (auto& m : motes_) {
        m.phase += dt * m.speed;
        m.x += dt * (1.5f + 2.0f * std::sin(m.phase)) * m.speed;
        m.y += dt * (std::cos(m.phase * 0.7f) * 1.5f - beat_.v * 6.0f);
        if (m.x > W)
            m.x -= W;
        if (m.y < 0)
            m.y += H * 0.85f;
        if (m.y > H * 0.85f)
            m.y -= H * 0.85f;
        const int ix = std::clamp(static_cast<int>(m.x), 0, W - 1), iy = std::clamp(static_cast<int>(m.y), 0, H - 1);
        const float lit = sun_[static_cast<size_t>(iy * W + ix)];
        if (lit > 0.1f)
            buf_.splat(m.x, m.y, RGB{255, 230, 180} * (lit * (0.22f + 0.25f * beat_.v)));
    }

    // --- speaker cones ---------------------------------------------------------------------------------------------------
    for (const float scx : {SpeakerL, SpeakerR}) {
        const float cx = scx * sx, cy = SpeakerY * sy;
        const float rc = ConeR * sx * (1.0f + 0.08f * push);
        const float cap = (4.5f + 1.4f * push) * sx;
        for (int y = static_cast<int>(cy - rc - 2); y <= static_cast<int>(cy + rc + 2); ++y)
            for (int x = static_cast<int>(cx - rc - 2); x <= static_cast<int>(cx + rc + 2); ++x) {
                if (!buf_.inside(x, y))
                    continue;
                const float ux = x + 0.5f - cx, uy = y + 0.5f - cy;
                const float d = len(ux, uy);
                if (d > rc + 1.5f)
                    continue;
                // Light from the upper left: facing = how much the surface tilts towards it.
                const float facing = d > 0.01f ? -(ux + uy) / (d * 1.41f) : 0.0f;
                RGB c;
                if (d > rc) {  // rubber surround, bulges and catches light as the cone pushes out
                    c = RGB{56, 56, 62} * (0.8f + 0.5f * facing * (0.5f + 0.5f * std::max(0.0f, push))) +
                        RGB{150, 90, 40} * (std::max(0.0f, push) * 0.45f * std::max(0.0f, facing)) + RGB{30, 30, 30} * std::max(0.0f, push);
                }
                else {
                    // Paper cone: concentric ridges, shaded by the light direction (a dish, so inverse); it lightens
                    // as it pushes out so the pump reads across the whole cone.
                    const float t = std::max(0.0f, (d - cap) / (rc - cap));
                    const float ridge = 0.85f + 0.15f * std::cos(t * 18.0f - push * 2.0f);
                    c = RGB{38, 38, 44} * (ridge * (1.0f - 0.35f * facing)) + RGB{36, 36, 40} * (0.4f * t) +
                        RGB{70, 70, 78} * (std::max(0.0f, push) * 0.45f * (1.0f - t) * (0.6f + 0.4f * std::max(0.0f, -facing)));
                    // Chrome dust cap with an anti-aliased rim so it stays round at every size.
                    const float capA = std::clamp(cap - d + 0.5f, 0.0f, 1.0f);
                    if (capA > 0.0f) {
                        const float l = 0.5f + 0.5f * (-(ux + uy) / (cap * 1.41f));
                        const RGB chromeCap =
                            lerp(RGB{110, 112, 122}, RGB{248, 248, 255}, std::clamp(l, 0.0f, 1.0f)) * (0.9f + 0.15f * std::max(0.0f, push));
                        c = lerp(c, chromeCap, capA);
                    }
                }
                buf_.at(x, y) = c;
            }
    }

    // --- tuning needle, cassette reels, EQ, power LED -------------------------------------------------------------------
    {
        const float nx = (26.0f + 76.0f * (0.5f + 0.45f * std::sin(time_ * 0.11f))) * sx;
        for (int y = static_cast<int>(45 * sy); y < static_cast<int>(49 * sy); ++y) buf_.set(static_cast<int>(nx), y, RGB{255, 90, 40});
        for (int r = 0; r < 2; ++r) {
            const float rx = (61.0f + r * 6.0f) * sx, ry = 64.0f * sy;
            buf_.disc(rx, ry, 2.2f * sx, RGB{230, 230, 235}, false);
            buf_.set(static_cast<int>(rx), static_cast<int>(ry), RGB{30, 30, 34});
            for (int k = 0; k < 3; ++k) {
                const float a = reel_ + k * Tau / 3.0f;
                buf_.set(static_cast<int>(rx + std::cos(a) * 1.6f * sx), static_cast<int>(ry + std::sin(a) * 1.6f * sx), RGB{60, 60, 66});
            }
        }
        const int eqY1 = static_cast<int>(82 * sy), eqH = static_cast<int>(10 * sy);
        for (int b = 0; b < 5; ++b) {
            auto& lv = eq_[static_cast<size_t>(b)];
            auto& peak = eqPeak_[static_cast<size_t>(b)];
            const float raw = spectrum_.band(b / 5.0f, (b + 1) / 5.0f);
            peak = std::max(raw, peak * std::exp(-dt / 6.0f));  // each bar gets its own gain
            lv += (std::min(1.0f, raw / std::max(0.15f, peak) * 0.95f + 0.08f * beat_.v) - lv) * (1.0f - std::exp(-dt / 0.05f));
            const int h = static_cast<int>(lv * eqH + 0.5f);
            for (int k = 0; k < eqH; ++k) {
                const float f = static_cast<float>(k) / eqH;
                const RGB on = f < 0.55f ? RGB{70, 255, 110} : f < 0.8f ? RGB{255, 220, 60} : RGB{255, 70, 50};
                const RGB c = k < h ? on : on * 0.08f;
                for (int dx = 0; dx < std::max(1, static_cast<int>(2 * sx)); ++dx)
                    buf_.set(static_cast<int>((57.5f + b * 3.0f) * sx) + dx, eqY1 - k, c);
            }
        }
        const RGB led = RGB{255, 50, 40} * (0.45f + 0.55f * beat_.v);
        buf_.disc(20 * sx, 84 * sy, 1.0f * sx, led, false);
        glow(buf_, 20 * sx, 84 * sy, 3.5f * sx, led * 0.25f);
    }

    // --- air pushed out of the speakers: faint rings outside the box -------------------------------------------------------
    for (auto& r : rings_) {
        r.r += dt * (40.0f + energy * 30.0f) * sx;
        r.strength *= std::exp(-dt / 0.45f);
    }
    std::erase_if(rings_, [&](const Ring& r) { return r.strength < 0.04f || r.r > 70.0f * sx; });
    const float bx0 = BodyX0 * sx, bx1 = BodyX1 * sx, by0 = (BodyY0 - 11) * sy, by1 = BodyY1 * sy;
    for (const auto& ring : rings_) {
        const float cx = (ring.side == 0 ? SpeakerL : SpeakerR) * sx, cy = SpeakerY * sy;
        const int steps = static_cast<int>(ring.r * 5.0f);
        for (int k = 0; k <= steps; ++k) {
            const float a = Tau * k / steps;
            const float px = cx + std::cos(a) * ring.r, py = cy + std::sin(a) * ring.r * 0.9f;
            if (px > bx0 - 1 && px < bx1 + 1 && py > by0 && py < by1 + 1)
                continue;
            if (py > StepTop * sy)
                continue;
            buf_.splat(px, py, RGB{255, 240, 210} * (ring.strength * 0.45f));
        }
    }

    // --- leaves blown across the steps on drops ----------------------------------------------------------------------------
    for (auto& l : litter_) {
        l.vy += 25.0f * dt;
        l.x += l.vx * dt;
        l.y += l.vy * dt;
        if (l.y > H - 2) {
            l.y = H - 2;
            l.vy = -l.vy * 0.3f;
            l.vx *= 0.8f;
        }
        l.spin += dt * 6.0f;
    }
    std::erase_if(litter_, [&](const Leaf& l) { return l.x > W + 3 || std::fabs(l.vx) < 1.0f; });
    for (const auto& l : litter_) {
        buf_.set(static_cast<int>(l.x), static_cast<int>(l.y), hsv(l.hue, 0.8f, 0.85f));
        buf_.set(static_cast<int>(l.x + std::cos(l.spin)), static_cast<int>(l.y + std::sin(l.spin) * 0.6f), hsv(l.hue, 0.8f, 0.6f));
    }

    // Warm sun flare on drops, a small lift on kicks.
    if (drop_.v > 0.02f)
        glow(buf_, 0.0f, 0.0f, 70.0f * sx, RGB{255, 200, 120} * (0.45f * drop_.v));
    if (kick_.v > 0.02f)
        for (auto& p : buf_.px) p = p * (1.0f + kick_.v * 0.06f);
    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioBoomboxScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "boombox";
    d.tags = {"music",          "street",      "day",         "urban",          "stoop",        "spectrum",
              "audio-reactive", "beat-driven", "bass-driven", "music-showcase", "style:hiphop", "style:party"};
    d.intensity = 0.55f;
    d.motion = 0.45f;
    d.music_affinity = 1.0f;
    d.performance_cost = 0.30f;
    return d;
}
}  // namespace Scenes
