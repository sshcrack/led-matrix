#include <algorithm>
#include <cmath>

#include "HipHopScenes.h"

namespace Scenes {
using namespace AudioKit;

namespace {
// Design space is 128x128; the wall ends where the pavement starts.
constexpr int WallTop = 9;
constexpr int Ground = 108;
constexpr float LampX = 64.0f, LampY = 14.0f;
constexpr float HoldSeconds = 8.0f;
const RGB BuffPaint{84, 58, 50};

float segment_distance(float px, float py, float ax, float ay, float bx, float by)
{
    const float vx = bx - ax, vy = by - ay;
    const float l2 = vx * vx + vy * vy;
    const float t = l2 > 0.0f ? std::clamp(((px - ax) * vx + (py - ay) * vy) / l2, 0.0f, 1.0f) : 0.0f;
    return len(px - ax - vx * t, py - ay - vy * t);
}

// Curated colourways: fill fade, outline, 3D block and the cloud behind it.
struct Colourway {
    RGB fillA, fillB, outline, shadow, cloud;
};
constexpr Colourway Colourways[] = {
    {{255, 60, 140}, {255, 170, 40}, {250, 248, 236}, {48, 14, 84}, {40, 170, 230}},
    {{90, 230, 90}, {245, 235, 60}, {28, 20, 44}, {200, 40, 150}, {120, 70, 210}},
    {{40, 200, 255}, {60, 80, 255}, {250, 250, 250}, {20, 24, 90}, {255, 130, 30}},
    {{255, 50, 50}, {240, 40, 190}, {255, 220, 50}, {24, 10, 18}, {30, 170, 160}},
    {{170, 80, 255}, {255, 110, 200}, {170, 255, 70}, {30, 16, 50}, {170, 175, 195}},
};
constexpr int ColourwayCount = static_cast<int>(sizeof(Colourways) / sizeof(Colourways[0]));
}  // namespace

void AudioGraffitiWallScene::register_properties()
{
    add_property(sensitivity_);
    add_property(paintSpeed_);
}

void AudioGraffitiWallScene::init_world()
{
    const int W = matrix_width, H = matrix_height;
    const float sx = W / 128.0f, sy = H / 128.0f;
    const size_t n = static_cast<size_t>(W * H);
    wall_.assign(n, {});
    light_.assign(n, 0.0f);
    texture_.assign(n, 1.0f);
    paint_.assign(n, {});
    alpha_.assign(n, 0.0f);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const size_t i = static_cast<size_t>(y * W + x);
            const float dx = (x + 0.5f) / sx - LampX, dy = (y + 0.5f) / sy - LampY;
            const float gy = (y + 0.5f) / sy;
            // Cone of the wall lamp: bright under the shade, fading down the wall.
            const float d = gy < Ground ? len(dx / 62.0f, dy / 92.0f) : len(dx / 54.0f, (gy - Ground) / 22.0f);
            light_[i] = std::exp(-d * d * 2.2f);
            if (gy < WallTop) {
                wall_[i] = lerp(RGB{6, 8, 22}, RGB{16, 14, 34}, gy / WallTop);
                continue;
            }
            if (gy < WallTop + 2) {  // concrete coping
                wall_[i] = RGB{70, 66, 64} * (0.4f + 0.7f * light_[i]);
                continue;
            }
            if (gy >= Ground) {
                const bool curb = gy < Ground + 2;
                const float grain = 0.85f + 0.3f * hash(static_cast<uint32_t>(x * 7919 + y * 104729));
                const RGB base = curb ? RGB{58, 56, 58} : RGB{30, 30, 34} * grain;
                wall_[i] = base * (0.35f + 0.9f * light_[i]);
                continue;
            }
            const int by = static_cast<int>(gy - WallTop - 2);
            const int row = by / 4;
            const int bxi = static_cast<int>(((x + 0.5f) / sx + (row % 2) * 4.0f) / 8.0f);
            const bool mortar = by % 4 == 3 || static_cast<int>((x + 0.5f) / sx + (row % 2) * 4.0f) % 8 == 7;
            const float tone = 0.75f + 0.35f * hash(static_cast<uint32_t>(row * 977 + bxi * 31));
            const RGB brick = mortar ? RGB{24, 22, 24} : RGB{70, 34, 26} * tone;
            wall_[i] = brick * (0.28f + 0.95f * light_[i]);
            // Paint follows the brick: mortar joints stay visible through it.
            texture_[i] = mortar ? 0.84f : 0.95f + 0.06f * tone;
        }
    drips_.clear();
    paletteIndex_ = static_cast<int>(rnd() * ColourwayCount) % ColourwayCount;
    new_piece();
    // Start on bare brick: the first piece goes straight onto the wall.
    phase_ = Phase::Paint;
    canX_ = static_cast<float>(right_);
    canY_ = (top_ + bottom_) * 0.5f;
}

void AudioGraffitiWallScene::new_piece()
{
    const int W = matrix_width, H = matrix_height;
    const float sx = W / 128.0f, sy = H / 128.0f;
    paletteIndex_ = (paletteIndex_ + 1 + static_cast<int>(rnd() * (ColourwayCount - 1))) % ColourwayCount;
    const auto& cw = Colourways[paletteIndex_];
    colors_ = {cw.fillA, cw.fillB, cw.outline, cw.shadow, cw.cloud};

    // Abstract wildstyle: interlocking diagonal shards with bent ends along a
    // wavy line, an arrow off the end, bubbles and a swoosh. Nothing upright
    // and nothing joined like a letter, so it reads as a piece, never as text.
    capsules_.clear();
    // Alternate silhouettes: a dense run of shards, then a few big sweeping ones.
    bold_ = !bold_;
    const float mid = (bold_ ? 56.0f : 60.0f) + rnd() * 4.0f;
    const float wave = rnd() * Tau;
    float x = 18.0f + rnd() * 3.0f;
    float sign = rnd() < 0.5f ? 1.0f : -1.0f;
    float lastX = x, lastY = mid;
    int count = 0;
    while (x < 100.0f) {
        const float cy = mid + (bold_ ? 15.0f : 10.0f) * std::sin(x * (bold_ ? 0.045f : 0.06f) + wave) + (rnd() - 0.5f) * 8.0f;
        const float angle = sign * (0.55f + rnd() * 0.5f) + (count % 3 == 2 ? sign * 0.6f : 0.0f);
        const float length = (bold_ ? 32.0f : 24.0f) + rnd() * 10.0f;
        const float r = (bold_ ? 6.0f : 4.6f) + rnd() * 2.0f;
        const float dx = std::cos(angle) * length * 0.5f, dy = std::sin(angle) * length * 0.5f;
        capsules_.push_back({x - dx, cy - dy, x + dx, cy + dy, r});
        // Bent end: a short kick off one tip at a sharper angle.
        if (count % 2 == 0) {
            const float bend = angle - sign * (1.3f + rnd() * 0.6f);
            const float kl = 9.0f + rnd() * 5.0f;
            capsules_.push_back({x + dx, cy + dy, x + dx + std::cos(bend) * kl, cy + dy + std::sin(bend) * kl, r * 0.75f});
        }
        else {
            // A fat bubble where the shard crosses the line.
            capsules_.push_back({x - 1.0f, cy, x + 1.0f, cy, r + 2.5f + rnd() * 2.0f});
        }
        lastX = x + dx;
        lastY = cy + dy;
        sign = -sign;
        x += (bold_ ? 22.0f : 13.0f) + rnd() * 5.0f;
        ++count;
    }
    // Arrow off the last shard, up and right.
    const float ax = std::min(lastX, 104.0f), ay = std::min(lastY, mid);
    const float tx = ax + 13.0f, ty = ay - 13.0f;
    capsules_.push_back({ax, ay, tx, ty, 2.8f});
    capsules_.push_back({tx, ty, tx - 8.0f, ty + 0.5f, 2.4f});
    capsules_.push_back({tx, ty, tx - 0.5f, ty + 8.0f, 2.4f});
    // Bubbles and a swoosh under the piece.
    capsules_.push_back({14.0f, mid - 14.0f, 14.0f, mid - 14.0f, 3.0f});
    capsules_.push_back({tx + 1.0f, ty + 16.0f, tx + 1.0f, ty + 16.0f, 2.2f});
    capsules_.push_back({22.0f, mid + 22.0f, 74.0f + rnd() * 16.0f, mid + 18.0f, 2.2f});
    // Keep the whole piece inside the frame with a small margin.
    {
        float lo = 1e9f, hi = -1e9f;
        for (const auto& c : capsules_) {
            lo = std::min({lo, c.ax - c.r, c.bx - c.r});
            hi = std::max({hi, c.ax + c.r, c.bx + c.r});
        }
        const float fit = std::min(1.0f, 56.0f / std::max({1.0f, 64.0f - lo, hi - 64.0f}));
        for (auto& c : capsules_) {
            c.ax = 64.0f + (c.ax - 64.0f) * fit;
            c.bx = 64.0f + (c.bx - 64.0f) * fit;
        }
    }
    for (auto& c : capsules_) {
        c.ax *= sx;
        c.bx *= sx;
        c.ay *= sy;
        c.by *= sy;
        c.r *= std::min(sx, sy);
    }

    const size_t n = static_cast<size_t>(W * H);
    dist_.assign(n, 1e3f);
    shadow_.assign(n, 1e3f);
    cloud_.assign(n, 0.0f);
    const float off = 3.6f * std::min(sx, sy);
    const float wallTop = (WallTop + 3) * sy, wallBottom = (Ground - 1) * sy;
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
    for (const auto& c : capsules_) {
        minX = std::min({minX, c.ax - c.r, c.bx - c.r});
        maxX = std::max({maxX, c.ax + c.r, c.bx + c.r});
        minY = std::min({minY, c.ay - c.r, c.by - c.r});
        maxY = std::max({maxY, c.ay + c.r, c.by + c.r});
    }
    const float pad = 9.0f * std::min(sx, sy);
    top_ = std::clamp(static_cast<int>(minY - pad), static_cast<int>(wallTop), H);
    bottom_ = std::clamp(static_cast<int>(maxY + pad), top_, static_cast<int>(wallBottom));
    left_ = std::clamp(static_cast<int>(minX - pad), 0, W);
    right_ = std::clamp(static_cast<int>(maxX + pad), left_, W);
    for (int y = top_; y < bottom_; ++y)
        for (int x2 = left_; x2 < right_; ++x2) {
            const size_t i = static_cast<size_t>(y * W + x2);
            const float px = x2 + 0.5f, py = y + 0.5f;
            float d = 1e3f, ds = 1e3f;
            for (const auto& c : capsules_) {
                d = std::min(d, segment_distance(px, py, c.ax, c.ay, c.bx, c.by) - c.r);
                ds = std::min(ds, segment_distance(px - off, py - off, c.ax, c.ay, c.bx, c.by) - c.r);
            }
            dist_[i] = d;
            shadow_[i] = ds;
            // Bubbly lobes: wide in places, almost touching the piece in others.
            cloud_[i] = (5.5f + 4.0f * std::sin(px * 0.13f + 0.7f) * std::sin(py * 0.17f + 1.3f)) * std::min(sx, sy);
        }
    // Sparkles sit on the top edge of a few forms.
    stars_.clear();
    for (size_t k = 0; k < capsules_.size() && stars_.size() < 3; k += 3) {
        const auto& c = capsules_[k];
        stars_.push_back({std::min(c.ax, c.bx) + c.r * 0.2f, std::min(c.ay, c.by) - c.r * 0.6f});
    }
    build_strokes();
    stroke_ = 0;
    point_ = 0;
    sub_ = 0.0f;
    traveling_ = true;
    hold_ = 0.0f;
}

bool AudioGraffitiWallScene::clip(size_t i, Layer layer) const
{
    const float d = dist_[i];
    switch (layer) {
        case Layer::Cloud:
            return d >= 1.9f && shadow_[i] >= 1.0f && d < cloud_[i] + 1.0f;  // a brick gap around the piece
        case Layer::Fill:
            return d < 0.0f;
        case Layer::Shadow:
            return d >= 0.6f && shadow_[i] < 0.0f;
        case Layer::Outline:
            return d > -1.3f && d < 0.9f;
        case Layer::Highlight:
            return d < -1.8f;
    }
    return false;
}

void AudioGraffitiWallScene::build_strokes()
{
    const int W = matrix_width;
    strokes_.clear();
    const float k = std::min(matrix_width, matrix_height) / 128.0f;
    // Splits a dense path into runs that actually cover pixels of the layer:
    // the writer lifts the can over parts that are already done.
    auto add_path = [&](const std::vector<Point>& path, Layer layer, float radius) {
        Stroke run{{}, layer, radius};
        const int reach = static_cast<int>(std::ceil(radius));
        for (const auto& p : path) {
            bool covers = false;
            for (int dy = -reach; dy <= reach && !covers; ++dy)
                for (int dx = -reach; dx <= reach && !covers; ++dx) {
                    const int x = static_cast<int>(p.x) + dx, y = static_cast<int>(p.y) + dy;
                    if (x < left_ || x >= right_ || y < top_ || y >= bottom_)
                        continue;
                    covers = clip(static_cast<size_t>(y * W + x), layer);
                }
            if (covers)
                run.pts.push_back(p);
            else if (!run.pts.empty()) {
                if (run.pts.size() > 2)
                    strokes_.push_back(run);
                run.pts.clear();
            }
        }
        if (run.pts.size() > 2)
            strokes_.push_back(run);
    };
    auto perimeter = [&](const Capsule& c, float grow, float shift) {
        std::vector<Point> path;
        const float vx = c.bx - c.ax, vy = c.by - c.ay;
        const float l = std::max(0.01f, len(vx, vy));
        const float ux = vx / l, uy = vy / l, nx = -uy, ny = ux;
        const float r = c.r + grow;
        const float base = std::atan2(ny, nx);
        // Side a->b, cap around b, side b->a, cap around a.
        for (float t = 0; t <= l; t += 0.7f) path.push_back({c.ax + ux * t + nx * r + shift, c.ay + uy * t + ny * r + shift});
        for (float a = 0; a <= Pi; a += 0.7f / r)
            path.push_back({c.bx + std::cos(base - a) * r + shift, c.by + std::sin(base - a) * r + shift});
        for (float t = l; t >= 0; t -= 0.7f) path.push_back({c.ax + ux * t - nx * r + shift, c.ay + uy * t - ny * r + shift});
        for (float a = 0; a <= Pi; a += 0.7f / r)
            path.push_back({c.ax + std::cos(base + Pi - a) * r + shift, c.ay + std::sin(base + Pi - a) * r + shift});
        return path;
    };

    // 1. Fill first: big patches of colour show up early.
    for (const auto& c : capsules_) {
        std::vector<Point> path;
        const float vx = c.bx - c.ax, vy = c.by - c.ay;
        const float l = std::max(0.01f, len(vx, vy));
        const float ux = vx / l, uy = vy / l, nx = -uy, ny = ux;
        const float amp = std::max(0.5f, c.r - 1.6f * k);
        for (float t = -c.r; t <= l + c.r; t += 0.35f) {
            const float side = std::sin(t / (1.5f * k) * Pi);  // one sweep across per 1.5 px along the form
            path.push_back({c.ax + ux * t + nx * amp * side, c.ay + uy * t + ny * amp * side});
        }
        add_path(path, Layer::Fill, 2.4f * k);
    }
    // 2. 3D block, 3. outline (4. cloud follows), 5. highlights.
    for (const auto& c : capsules_) add_path(perimeter(c, 1.4f * k, 1.6f * k), Layer::Shadow, 2.1f * k);
    for (const auto& c : capsules_) add_path(perimeter(c, 0.0f, 0.0f), Layer::Outline, 1.5f * k);
    // 4. Cloud: broad horizontal passes behind the finished outline.
    {
        bool flip = false;
        for (float y = top_ + 2.0f; y < bottom_; y += 4.5f * k) {
            std::vector<Point> path;
            for (float x = left_; x < right_; x += 0.8f) path.push_back({flip ? right_ - (x - left_) : x, y});
            flip = !flip;
            add_path(path, Layer::Cloud, 4.2f * k);
        }
    }
    for (const auto& c : capsules_) {
        const float vx = c.bx - c.ax, vy = c.by - c.ay;
        const float l = len(vx, vy);
        if (l < 6.0f * k)
            continue;
        const float ux = vx / l, uy = vy / l;
        // Offset towards the upper left side of the form.
        float nx = -uy, ny = ux;
        if (nx + ny > 0) {
            nx = -nx;
            ny = -ny;
        }
        const float o = c.r * 0.45f;
        std::vector<Point> path;
        const bool fromTop = c.ay < c.by;
        for (float t = l * 0.12f; t <= l * 0.42f; t += 0.6f) {
            const float tt = fromTop ? t : l - t;
            path.push_back({c.ax + ux * tt + nx * o, c.ay + uy * tt + ny * o});
        }
        add_path(path, Layer::Highlight, 0.8f * k);
    }
}

RGB AudioGraffitiWallScene::layer_color(int, int y, Layer layer) const
{
    switch (layer) {
        case Layer::Cloud:
            return colors_.cloud;
        case Layer::Fill: {
            const float t = static_cast<float>(y - top_) / std::max(1, bottom_ - top_);
            return lerp(colors_.fillA, colors_.fillB, smoothstep(0.25f, 0.75f, t));
        }
        case Layer::Shadow:
            return lerp(colors_.shadow, colors_.fillB, 0.3f);  // a lit side face, not just a dark gap
        case Layer::Outline:
            return colors_.outline;
        case Layer::Highlight:
            return RGB{255, 255, 255};
    }
    return {};
}

void AudioGraffitiWallScene::spray(float x, float y, float radius, Layer layer, float strength)
{
    const int W = matrix_width, H = matrix_height;
    const int r = static_cast<int>(std::ceil(radius)) + 1;
    for (int py = static_cast<int>(y) - r; py <= static_cast<int>(y) + r; ++py)
        for (int px = static_cast<int>(x) - r; px <= static_cast<int>(x) + r; ++px) {
            if (px < 0 || py < 0 || px >= W || py >= H)
                continue;
            const size_t i = static_cast<size_t>(py * W + px);
            const float d = len(px + 0.5f - x, py + 0.5f - y) / radius;
            if (d >= 1.0f)
                continue;
            if (!clip(i, layer)) {
                // Overspray: a faint haze just past the clipped edge.
                if (layer != Layer::Highlight && d < 0.8f && dist_[i] < 1e2f) {
                    const float a = 0.025f * strength;
                    const float na = alpha_[i] + a * (1.0f - alpha_[i]);
                    paint_[i] = (paint_[i] * (alpha_[i] * (1.0f - a)) + layer_color(px, py, layer) * a) * (1.0f / na);
                    alpha_[i] = na;
                }
                continue;
            }
            const float a = std::clamp((1.0f - d * d) * strength, 0.0f, 1.0f);
            const float na = alpha_[i] + a * (1.0f - alpha_[i]);
            paint_[i] = (paint_[i] * (alpha_[i] * (1.0f - a)) + layer_color(px, py, layer) * a) * (1.0f / std::max(na, 1e-3f));
            alpha_[i] = na;
        }
}

// Moves the can `distance` design pixels along the work: travel between
// strokes is quick and paints nothing; along a stroke it sprays.
void AudioGraffitiWallScene::advance(float distance)
{
    sub_ += distance;
    while (sub_ > 0.0f && stroke_ < strokes_.size()) {
        const auto& s = strokes_[stroke_];
        if (traveling_) {
            const Point& to = s.pts.front();
            const float d = len(to.x - canX_, to.y - canY_);
            const float budget = sub_ * 3.0f;
            if (d <= budget) {
                canX_ = to.x;
                canY_ = to.y;
                sub_ -= d / 3.0f;
                traveling_ = false;
                point_ = 0;
            }
            else {
                canX_ += (to.x - canX_) * budget / d;
                canY_ += (to.y - canY_) * budget / d;
                sub_ = 0.0f;
            }
            continue;
        }
        const Point& p = s.pts[point_];
        spray(p.x, p.y, s.radius, s.layer, s.layer == Layer::Cloud ? 0.8f : 0.92f);
        canX_ = p.x;
        canY_ = p.y;
        sub_ -= s.layer == Layer::Highlight ? 1.2f : 0.6f;
        if (++point_ >= s.pts.size()) {
            ++stroke_;
            traveling_ = true;
            point_ = 0;
        }
    }
    if (stroke_ >= strokes_.size())
        sub_ = 0.0f;
}

bool AudioGraffitiWallScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    const float k = std::min(W, H) / 128.0f;
    if (static_cast<int>(wall_.size()) != W * H)
        init_world();
    spectrum_.update(audio, dt, 0.04f, 0.25f);

    const float snare = feature(audio, AudioProtocol::Feature::Snare);
    const float hihat = feature(audio, AudioProtocol::Feature::Hihat);
    const float kick = feature(audio, AudioProtocol::Feature::Kick);
    const float energy = spectrum_.level;
    if (events_.beatHit) {
        beat_.hit(0.6f + 0.4f * kick);
        shake_ = 1.0f;
    }
    if (events_.onsetHit && hihat > 0.4f)
        hat_.hit(hihat);
    if (events_.dropHit)
        drop_.hit();
    beat_.decay(dt, 0.22f);
    hat_.decay(dt, 0.12f);
    drop_.decay(dt, 1.5f);
    shake_ *= std::exp(-dt / 0.18f);

    // --- the writer at work ---------------------------------------------------------------------
    bool spraying = false;
    Layer layer = Layer::Fill;
    switch (phase_) {
        case Phase::Buff: {
            roller_ += dt * 34.0f * k * (0.6f + 0.6f * energy + 0.8f * beat_.v);
            const int to = std::min(buffRight_, buffLeft_ + static_cast<int>(roller_));
            for (int y = buffTop_; y < buffBottom_; ++y)
                for (int x = buffLeft_; x < to; ++x) {
                    // Rough, feathered edges so the patch sits in the wall, not on it.
                    const size_t i = static_cast<size_t>(y * W + x);
                    const float edge = static_cast<float>(std::min({x - buffLeft_, buffRight_ - 1 - x, y - buffTop_, buffBottom_ - 1 - y}));
                    const float rough = hash(static_cast<uint32_t>(x * 7349 + y * 1931));
                    const float a = std::clamp(0.25f + edge / (4.0f * k) + 0.3f * rough, 0.0f, 0.82f);  // brick still shows through
                    paint_[i] = lerp(paint_[i], BuffPaint, a);
                    alpha_[i] = std::max(alpha_[i], a);
                }
            if (to >= buffRight_) {
                phase_ = Phase::Paint;
                canX_ = static_cast<float>(buffRight_);
                canY_ = (top_ + bottom_) * 0.5f;
            }
            break;
        }
        case Phase::Paint: {
            // Spray bursts land on the beat; the mix keeps the can moving between them.
            const float speed = 150.0f * k * paintSpeed_->get() *
                                (0.15f + 0.5f * energy + 3.0f * beat_.v + 0.8f * drop_.v);  // most paint lands in beat bursts
            const size_t before = stroke_;
            const bool wasTraveling = traveling_;
            advance(speed * dt);
            spraying = !traveling_ || (!wasTraveling && stroke_ != before);
            if (stroke_ < strokes_.size())
                layer = strokes_[stroke_].layer;
            // Drips run from fresh paint on snares.
            if (spraying && events_.onsetHit && snare > 0.45f && (layer == Layer::Fill || layer == Layer::Cloud) && drips_.size() < 24)
                drips_.push_back({canX_, canY_ + 1.5f * k, 0.0f, (3.0f + rnd() * 7.0f) * k, (6.0f + rnd() * 6.0f) * k,
                                  layer_color(static_cast<int>(canX_), static_cast<int>(canY_), layer)});
            if (stroke_ >= strokes_.size()) {
                phase_ = Phase::Hold;
                hold_ = 0.0f;
            }
            break;
        }
        case Phase::Hold:
            hold_ += dt;
            // A shine sweeps across the finished piece on every beat.
            if (events_.beatHit) {
                shine_ = left_ - 10.0f * k;
                shineEnv_.hit(0.6f + 0.4f * beat_.v);
            }
            // Fresh paint keeps running on snares while the piece rests.
            if (events_.onsetHit && snare > 0.4f && drips_.size() < 24 && !capsules_.empty()) {
                const auto& cap = capsules_[static_cast<size_t>(rnd() * capsules_.size()) % capsules_.size()];
                const float bx = cap.ay > cap.by ? cap.ax : cap.bx, by = std::max(cap.ay, cap.by) + cap.r - 0.5f;
                drips_.push_back({bx, by, 0.0f, (8.0f + rnd() * 8.0f) * k, (6.0f + rnd() * 4.0f) * k, colors_.outline});
            }
            if (hold_ > HoldSeconds && (events_.sectionHit || events_.beatHit || hold_ > HoldSeconds * 1.5f)) {
                // Plan the next piece, then roll a patch over the old one and its space.
                const int oldTop = top_, oldBottom = bottom_, oldLeft = left_, oldRight = right_;
                new_piece();
                buffTop_ = std::min(oldTop, top_);
                buffBottom_ = std::max(oldBottom, bottom_);
                buffLeft_ = std::min(oldLeft, left_);
                buffRight_ = std::max(oldRight, right_);
                phase_ = Phase::Buff;
                roller_ = 0.0f;
                drips_.clear();
            }
            break;
    }
    spray_.decay(dt, 0.1f);
    if (spraying)
        spray_.hit(0.5f + 0.5f * beat_.v);

    for (auto& d : drips_) {
        if (d.length >= d.max)
            continue;
        const float step = d.speed * dt * (1.0f - d.length / d.max * 0.7f);
        d.length = std::min(d.max, d.length + step);
        const int x = static_cast<int>(d.x);
        const int y = static_cast<int>(d.y + d.length);
        if (x >= 0 && x < W && y >= 0 && y < static_cast<int>(Ground * H / 128.0f)) {
            const size_t i = static_cast<size_t>(y * W + x);
            paint_[i] = d.color;
            alpha_[i] = 1.0f;
        }
    }

    // --- compose: wall, paint lit by the lamp ---------------------------------------------------
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const size_t i = static_cast<size_t>(y * W + x);
            RGB c = wall_[i];
            if (alpha_[i] > 0.0f)
                c = lerp(c, paint_[i] * ((0.32f + 0.9f * light_[i]) * texture_[i]), alpha_[i]);
            buf_.at(x, y) = c;
        }

    // Shine sweep and bright heads on running drips.
    shine_ += dt * 90.0f * k;
    shineEnv_.decay(dt, 0.9f);
    if (phase_ == Phase::Hold && shineEnv_.v > 0.05f)
        for (int y = top_; y < bottom_; ++y)
            for (int x = std::max(left_, static_cast<int>(shine_ - 40.0f * k)); x < std::min(right_, static_cast<int>(shine_ + 4.0f * k));
                 ++x) {
                const size_t i = static_cast<size_t>(y * W + x);
                const float band = std::fabs(x - shine_ + (y - top_) * 0.6f);
                if (dist_[i] < 0.0f && band < 3.5f * k)
                    buf_.add(x, y, RGB{255, 255, 255} * (0.6f * shineEnv_.v * (1.0f - band / (3.5f * k))));
            }
    for (const auto& d : drips_)
        if (d.length < d.max)
            buf_.add(static_cast<int>(d.x), static_cast<int>(d.y + d.length), lerp(d.color, RGB{255, 255, 255}, 0.5f) * 0.8f);

    // Sparkles once the piece is done, twinkling with the hats.
    if (phase_ == Phase::Hold)
        for (size_t s = 0; s < stars_.size(); ++s) {
            const float tw = std::clamp(0.35f + 0.65f * hat_.v + 0.5f * drop_.v + 0.25f * std::sin(time_ * 3.0f + s * 2.1f), 0.0f, 1.2f);
            const float sx = stars_[s].x, sy = stars_[s].y;
            const RGB c = RGB{255, 255, 240} * tw;
            const int arm = static_cast<int>(1 + 2 * tw * k);
            for (int a = -arm; a <= arm; ++a) {
                const float f = 1.0f - std::fabs(static_cast<float>(a)) / (arm + 1);
                buf_.add(static_cast<int>(sx) + a, static_cast<int>(sy), c * f);
                buf_.add(static_cast<int>(sx), static_cast<int>(sy) + a, c * f);
            }
        }

    // --- lamp ------------------------------------------------------------------------------------
    {
        const float lx = LampX * W / 128.0f, ly = LampY * H / 128.0f;
        rect(buf_, static_cast<int>(lx - 4 * k), static_cast<int>(ly - 3 * k), static_cast<int>(lx + 4 * k), static_cast<int>(ly - 1 * k),
             RGB{30, 32, 36});
        rect(buf_, static_cast<int>(lx - 3 * k), static_cast<int>(ly - 1 * k), static_cast<int>(lx + 3 * k), static_cast<int>(ly),
             RGB{255, 214, 150});
        glow(buf_, lx, ly + 1.0f, 7.0f * k, RGB{120, 90, 50});
    }

    // --- spray cans on the pavement, the can in the hand, the roller -----------------------------
    {
        const Colourway& cw = Colourways[paletteIndex_];
        const RGB caps[] = {cw.fillA, cw.outline, cw.cloud, cw.fillB};
        const float gy = Ground * H / 128.0f;
        for (int c = 0; c < 4; ++c) {
            const float cx = (98.0f + c * 6.0f + (c == 3 ? 4.0f : 0.0f)) * k;
            const float lit = 0.35f + 0.8f * light_[static_cast<size_t>(std::min(H - 1, static_cast<int>(gy + 6 * k)) * W +
                                                                        std::clamp(static_cast<int>(cx), 0, W - 1))];
            if (c == 3) {  // one lies on its side
                rect(buf_, static_cast<int>(cx - 3 * k), static_cast<int>(gy + 9 * k), static_cast<int>(cx + 4 * k),
                     static_cast<int>(gy + 12 * k), RGB{120, 120, 130} * lit);
                rect(buf_, static_cast<int>(cx + 4 * k), static_cast<int>(gy + 9 * k), static_cast<int>(cx + 5 * k),
                     static_cast<int>(gy + 12 * k), caps[c] * lit);
                continue;
            }
            rect(buf_, static_cast<int>(cx - 1.5f * k), static_cast<int>(gy + 4 * k), static_cast<int>(cx + 1.5f * k),
                 static_cast<int>(gy + 12 * k), RGB{120, 120, 130} * lit);
            rect(buf_, static_cast<int>(cx - 1.5f * k), static_cast<int>(gy + 2 * k), static_cast<int>(cx + 1.5f * k),
                 static_cast<int>(gy + 4 * k), caps[c] * lit);
        }
    }
    if (phase_ == Phase::Paint) {
        const RGB paintC = layer_color(static_cast<int>(canX_), static_cast<int>(canY_), layer);
        // Mist where the paint lands.
        if (spray_.v > 0.05f) {
            glow(buf_, canX_, canY_, 4.0f * k, paintC * (0.8f * spray_.v));
            // Bright spray head right where the paint lands.
            buf_.disc(canX_, canY_, 1.3f * k, lerp(paintC, RGB{255, 255, 255}, 0.5f) * std::min(1.0f, 0.4f + spray_.v), true);
            // A fan of bright overspray around the tip, densest on the beat.
            const int specks = static_cast<int>(6 + 12 * spray_.v);
            for (int s = 0; s < specks; ++s) {
                const float a = rnd() * Tau, r = (1.5f + rnd() * 4.5f) * k;
                buf_.splat(canX_ + std::cos(a) * r, canY_ + std::sin(a) * r, paintC * (0.9f * spray_.v));
            }
        }
        // The can, held just right of and below the spray point; it shakes on the beat between strokes.
        const float jig = traveling_ ? std::sin(time_ * 40.0f) * shake_ * 1.2f * k : 0.0f;
        const float cx = canX_ + 2.5f * k, cy = canY_ + 1.0f * k + jig;
        const float lit =
            0.5f +
            0.7f * light_[static_cast<size_t>(std::clamp(static_cast<int>(cy), 0, H - 1) * W + std::clamp(static_cast<int>(cx), 0, W - 1))];
        rect(buf_, static_cast<int>(cx), static_cast<int>(cy + 1 * k), static_cast<int>(cx + 3 * k), static_cast<int>(cy + 8 * k),
             RGB{150, 150, 160} * lit);
        rect(buf_, static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(cx + 3 * k), static_cast<int>(cy + 1 * k), paintC * lit);
        // Gloved hand round the can.
        rect(buf_, static_cast<int>(cx - 1 * k), static_cast<int>(cy + 3 * k), static_cast<int>(cx + 4 * k), static_cast<int>(cy + 6 * k),
             RGB{24, 22, 26});
    }
    if (phase_ == Phase::Buff) {
        const float rx = buffLeft_ + roller_;
        // Wet edge just behind the roller, a wide roller head and its pole.
        for (int y = buffTop_; y < buffBottom_; ++y)
            for (int w = 1; w <= static_cast<int>(7 * k); ++w)
                buf_.add(static_cast<int>(rx) - w, y, RGB{90, 84, 76} * (1.0f - w / (8.0f * k)));
        const float mid = (buffTop_ + buffBottom_) * 0.5f, half = (buffBottom_ - buffTop_) * 0.5f;
        // Flecks thrown off the wet roller.
        for (int f = 0; f < 3; ++f)
            buf_.set(static_cast<int>(rx + (2.0f + rnd() * 8.0f) * k), static_cast<int>(buffTop_ + rnd() * (buffBottom_ - buffTop_)),
                     BuffPaint * 1.3f);
        rect(buf_, static_cast<int>(rx), static_cast<int>(mid - half), static_cast<int>(rx + 7 * k), static_cast<int>(mid + half),
             RGB{150, 140, 128});
        rect(buf_, static_cast<int>(rx + 7 * k), static_cast<int>(mid - half), static_cast<int>(rx + 8 * k), static_cast<int>(mid + half),
             RGB{60, 56, 52});
        const float gy = Ground * H / 128.0f;
        for (int t = 0; t < 2; ++t) line(buf_, rx + 8.0f * k + t, mid, rx + 22.0f * k + t, gy + 10.0f * k, RGB{140, 100, 60});
    }

    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioGraffitiWallScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "graffiti";
    d.tags = {"music", "street", "night", "urban", "art", "audio-reactive", "beat-driven", "music-showcase", "style:hiphop"};
    d.intensity = 0.35f;
    d.motion = 0.3f;
    d.music_affinity = 1.0f;
    d.performance_cost = 0.3f;
    return d;
}
}  // namespace Scenes
