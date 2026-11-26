#include "BonsaiTerrariumScene.h"

namespace Hobbies {

namespace {

// Layout in design space (128 × 128).
constexpr float ShelfTop = 104.0f, ShelfFront = 106.5f, ShelfBottom = 114.0f;
constexpr float BoxLeft = 24.0f, BoxRight = 104.0f, Eave = 48.0f, Apex = 28.0f, BoxMid = 64.0f;
constexpr float RailTop = 101.0f;
constexpr float PaneLeft = 25.2f, PaneRight = 102.8f, PaneTop = 49.2f, PaneBottom = 100.8f;
constexpr float GrowthPeriod = 6.0f * 3600.0f;  // one flush of new shoots, seconds
constexpr int FairyBulbs = 9;

float sq(float x)
{
    return x * x;
}

// Smooth value noise, 0..1.
float vnoise(float x, float y, uint32_t seed)
{
    const float fx = std::floor(x), fy = std::floor(y);
    const int ix = static_cast<int>(fx), iy = static_cast<int>(fy);
    const auto h = [&](int a, int b) {
        return hash01(static_cast<uint32_t>(a) * 73856093U ^ static_cast<uint32_t>(b) * 19349663U ^ seed * 83492791U);
    };
    float u = x - fx, v = y - fy;
    u = u * u * (3 - 2 * u);
    v = v * v * (3 - 2 * v);
    const float a = std::lerp(h(ix, iy), h(ix + 1, iy), u), b = std::lerp(h(ix, iy + 1), h(ix + 1, iy + 1), u);
    return std::lerp(a, b, v);
}

Rgb mul(Rgb a, Rgb b)
{
    return {a.r * b.r, a.g * b.g, a.b * b.b};
}
Rgb add(Rgb a, Rgb b)
{
    return {a.r + b.r, a.g + b.g, a.b + b.b};
}

// Top of the moss cushion.
float moss_top(float x)
{
    return 85.5f - 3.5f * std::exp(-sq((x - 60.0f) / 20.0f)) - 1.0f * std::exp(-sq((x - 90.0f) / 8.0f)) + 0.6f * std::sin(x * 0.37f) +
           0.4f * std::sin(x * 0.9f + 1.0f);
}

struct Stone {
    float x, y, rx, ry;
    Rgb color;
};
constexpr Stone Stones[] = {
    {84.0f, 84.6f, 6.0f, 3.4f, {0.56f, 0.55f, 0.52f}},
    {40.5f, 86.2f, 3.6f, 2.3f, {0.60f, 0.56f, 0.50f}},
    {93.5f, 86.0f, 2.3f, 1.5f, {0.50f, 0.50f, 0.50f}},
};

// Where something crawling on the moss rests (over the stones too).
float ground_top(float x)
{
    float top = moss_top(x) - 0.4f;
    for (const auto& s : Stones) {
        const float u = (x - s.x) / s.rx;
        if (std::abs(u) < 1.0f)
            top = std::min(top, s.y - s.ry * std::sqrt(1.0f - u * u));
    }
    return top;
}

struct Pad {
    float x, y, rx, ry;
};
constexpr Pad Pads[] = {
    {42.0f, 64.0f, 11.0f, 5.5f}, {82.0f, 56.5f, 10.0f, 5.0f}, {51.5f, 49.5f, 8.0f, 4.2f},
    {71.0f, 45.5f, 8.5f, 4.5f},  {61.0f, 39.5f, 9.0f, 5.0f},
};

// Point i of the fairy light string draped under the gable.
Point fairy_bulb(int i)
{
    const float u = static_cast<float>(i) / (FairyBulbs - 1);  // 0..1 across
    const float x = 30.0f + u * 68.0f;
    const float roof = Apex + (Eave - Apex) * std::abs(x - BoxMid) / (BoxMid - BoxLeft);
    // Sags between the hooks at the ends and the ridge.
    const float side = u < 0.5f ? u * 2.0f : (u - 0.5f) * 2.0f;
    return {x, roof + 3.2f + 2.2f * std::sin(side * kPi)};
}

}  // namespace

BonsaiTerrariumScene::BonsaiTerrariumScene()
{
    Environment::Adapt adapt;
    adapt.weather = false;  // indoors
    adapt.season = false;
    use_environment({Environment::TimeOfDay::Dusk, Environment::Weather::Clear, Environment::Season::Summer}, adapt);
}

float BonsaiTerrariumScene::growth() const
{
    return fract(growthOffset_ + t_ / GrowthPeriod);
}

void BonsaiTerrariumScene::initialize(int width, int height)
{
    HobbySceneBase::initialize(width, height);
    const int w = cv_.w, h = cv_.h;
    scale_ = std::min(w, h) / 128.0f;
    ox_ = (w - 128.0f * scale_) * 0.5f;
    oy_ = (h - 128.0f * scale_) * 0.5f;
    const size_t n = static_cast<size_t>(w * h);
    fog_.assign(n, 0.0f);
    fogTarget_.assign(n, 0.0f);
    glass_.assign(n, 0);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float X = (x + 0.5f - ox_) / scale_, Y = (y + 0.5f - oy_) / scale_;
            uint8_t g = 0;
            if (X > PaneLeft && X < PaneRight && Y > PaneTop && Y < PaneBottom)
                g = 1;
            else if (Y <= PaneTop && Y > Apex) {
                const float roof = Apex + (Eave - Apex) * std::abs(X - BoxMid) / (BoxMid - BoxLeft);
                if (Y > roof + 1.6f && Y < Eave - 1.0f)
                    g = 2;
            }
            glass_[static_cast<size_t>(y * w + x)] = g;
        }
    growthOffset_ = rnd();
    growthStep_ = -1;
    lightKey_.fill(-1000.0f);
    build_albedo();

    // Glass already fogged, some drops ready to run.
    drops_.clear();
    for (int i = 0; i < 35; ++i)
        drops_.push_back({PaneLeft + 1 + rnd() * (PaneRight - PaneLeft - 2), PaneTop + 1 + rnd() * (PaneBottom - PaneTop - 8),
                          0.3f + 0.75f * rnd() * rnd(), 0.0f, rnd() * kTau});
    motes_.clear();
    for (int i = 0; i < 10; ++i)
        motes_.push_back({rnd() * 128.0f, rnd() * 110.0f, (rnd() - 0.5f) * 0.8f, (rnd() - 0.5f) * 0.4f, rnd() * kTau});
    slime_.clear();
    snailActive_ = false;
    snailWait_ = 3.0f + 5.0f * rnd();
    fogPrimed_ = false;
}

// ---------------------------------------------------------------- static picture

void BonsaiTerrariumScene::build_albedo()
{
    const int w = cv_.w, h = cv_.h;
    const size_t n = static_cast<size_t>(w * h);
    const float g = 0.92f + 0.1f * growth();
    Canvas occ;
    occ.resize(w, h);
    const Rgb solid{1, 1, 1};

    // Wall, shelf and what shows through the glass.
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float X = (x + 0.5f - ox_) / scale_, Y = (y + 0.5f - oy_) / scale_;
            const float plaster = 0.9f + 0.1f * vnoise(X * 0.14f, Y * 0.14f, 3) + 0.04f * (hash01(static_cast<uint32_t>(y * w + x)) - 0.5f);
            Rgb c = scale({0.66f, 0.60f, 0.52f}, plaster);
            if (Y >= ShelfTop && Y < ShelfFront) {
                c = scale({0.64f, 0.47f, 0.31f}, 0.92f + 0.08f * vnoise(X * 0.3f, Y, 5));
            }
            else if (Y >= ShelfFront && Y < ShelfBottom) {
                const float grain =
                    0.86f + 0.1f * std::sin(X * 0.22f + 7.0f * vnoise(X * 0.04f, Y * 0.5f, 7)) + 0.05f * vnoise(X * 0.8f, Y * 0.8f, 8);
                c = scale({0.50f, 0.34f, 0.21f}, grain * (Y < ShelfFront + 1.0f ? 1.15f : 1.0f) * (Y > ShelfBottom - 1.0f ? 0.6f : 1.0f));
            }
            else if (Y >= ShelfBottom) {
                c = scale(c, 0.45f + 0.35f * std::clamp((Y - ShelfBottom) / 14.0f, 0.0f, 1.0f));  // under the shelf
            }
            const size_t i = static_cast<size_t>(y * w + x);
            if (glass_[i])
                c = mul(scale(c, 0.82f), {0.90f, 0.98f, 0.93f});  // wall seen through two panes
            cv_.set(x, y, c);
        }

    // The terrarium's back edges, dimmer, for depth.
    const Rgb backBrass{0.36f, 0.28f, 0.16f};
    cv_.capsule(sx(30), sy(53), px(0.5f), sx(30), sy(98), px(0.5f), backBrass);
    cv_.capsule(sx(98), sy(53), px(0.5f), sx(98), sy(98), px(0.5f), backBrass);
    cv_.capsule(sx(30), sy(53), px(0.45f), sx(BoxMid), sy(35), px(0.45f), backBrass);
    cv_.capsule(sx(BoxMid), sy(35), px(0.45f), sx(98), sy(53), px(0.45f), backBrass);

    // Pebble drainage layer, charcoal, soil and the moss cushion.
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float X = (x + 0.5f - ox_) / scale_, Y = (y + 0.5f - oy_) / scale_;
            if (X < PaneLeft || X > PaneRight || Y > RailTop + 0.5f)
                continue;
            const float top = moss_top(X) - 0.9f * vnoise(X * 1.3f, 0.5f, 11);
            if (Y < top - 0.5f)
                continue;
            const float charcoal = 92.6f + 0.5f * std::sin(X * 0.21f), soilTop = top + 3.2f + 0.8f * vnoise(X * 0.4f, 2.0f, 12);
            Rgb c;
            if (Y >= charcoal + 2.0f) {
                c = {0.14f, 0.12f, 0.10f};  // gaps between pebbles (pebbles drawn below)
            }
            else if (Y >= charcoal) {
                c = scale({0.10f, 0.10f, 0.11f}, 0.7f + 0.6f * hash01(static_cast<uint32_t>(x * 31 + y * 977)));
            }
            else if (Y >= soilTop) {
                const float speck = hash01(static_cast<uint32_t>(x * 131 + y * 7919));
                c = scale({0.24f, 0.16f, 0.10f}, 0.8f + 0.3f * vnoise(X * 0.6f, Y * 0.6f, 13) + (speck > 0.93f ? 0.5f : 0.0f));
            }
            else {
                // Moss: mottled greens, brighter on top, sunlit tips.
                const float depth = std::clamp((Y - top) / 3.5f, 0.0f, 1.0f);
                const float m = vnoise(X * 0.7f, Y * 0.7f, 14), tip = hash01(static_cast<uint32_t>(x * 613 + y * 389));
                c = mix({0.40f, 0.58f, 0.18f}, {0.13f, 0.30f, 0.09f}, depth * 0.9f + (1 - m) * 0.3f);
                if (tip > 0.88f && depth < 0.5f)
                    c = {0.48f, 0.62f, 0.20f};
            }
            cv_.set(x, y, c);
            occ.set(x, y, solid);
        }
    for (int i = 0; i < 46; ++i) {
        const float px0 = PaneLeft + 1 + hash01(i * 7 + 1) * (PaneRight - PaneLeft - 2), py0 = 95.4f + hash01(i * 7 + 2) * 5.5f;
        const float rx = 1.4f + 1.4f * hash01(i * 7 + 3), ry = 1.0f + 0.8f * hash01(i * 7 + 4);
        const float tone = hash01(i * 7 + 5);
        const Rgb c = mix({0.36f, 0.38f, 0.40f}, {0.52f, 0.47f, 0.40f}, tone);
        cv_.ellipse(sx(px0), sy(py0), px(rx), px(ry), 0.0f, c);
        cv_.ellipse(sx(px0 - rx * 0.2f), sy(py0 - ry * 0.3f), px(rx * 0.6f), px(ry * 0.5f), 0.0f, scale(c, 1.15f), 0.6f);
    }

    // Fern on the left.
    const Rgb fern{0.20f, 0.46f, 0.15f};
    for (int f = 0; f < 6; ++f) {
        const float a = -2.45f + f * 0.36f, len = 11.0f + 3.0f * hash01(f + 40);
        const Point base{35.0f, 85.5f};
        Point prev = base;
        for (int s = 1; s <= 8; ++s) {
            const float u = s / 8.0f;
            const Point p{base.x + std::cos(a) * len * u, base.y + std::sin(a) * len * u + 4.5f * u * u};
            cv_.capsule(sx(prev.x), sy(prev.y), px(0.35f), sx(p.x), sy(p.y), px(0.3f), scale(fern, 0.8f));
            occ.capsule(sx(prev.x), sy(prev.y), px(0.35f), sx(p.x), sy(p.y), px(0.3f), solid);
            // Leaflets either side, shorter toward the tip.
            const float dx = p.x - prev.x, dy = p.y - prev.y, l = std::max(0.01f, std::sqrt(dx * dx + dy * dy));
            const float nx = -dy / l, ny = dx / l, leaf = 2.2f * (1.0f - u * 0.7f);
            for (const float side : {-1.0f, 1.0f}) {
                const Rgb lc = side < 0 ? scale(fern, 1.15f) : fern;
                cv_.capsule(sx(p.x), sy(p.y), px(0.4f), sx(p.x + nx * leaf * side + dx * 0.3f), sy(p.y + ny * leaf * side + dy * 0.3f),
                            px(0.25f), lc);
                occ.capsule(sx(p.x), sy(p.y), px(0.4f), sx(p.x + nx * leaf * side), sy(p.y + ny * leaf * side), px(0.25f), solid);
            }
            prev = p;
        }
    }

    // Stones bedded in the moss.
    for (const auto& s : Stones) {
        cv_.ellipse(sx(s.x), sy(s.y), px(s.rx), px(s.ry), 0.0f, scale(s.color, 0.75f));
        occ.ellipse(sx(s.x), sy(s.y), px(s.rx), px(s.ry), 0.0f, solid);
        cv_.ellipse(sx(s.x - s.rx * 0.15f), sy(s.y - s.ry * 0.25f), px(s.rx * 0.78f), px(s.ry * 0.62f), 0.0f, s.color);
        cv_.ellipse(sx(s.x - s.rx * 0.35f), sy(s.y - s.ry * 0.5f), px(s.rx * 0.35f), px(s.ry * 0.25f), 0.0f, scale(s.color, 1.25f), 0.7f);
        // Moss creeping over the foot of the stone.
        for (float X = s.x - s.rx; X <= s.x + s.rx; X += 0.6f) {
            const float foot = s.y + s.ry * 0.55f - 0.8f * vnoise(X * 1.1f, s.y, 15);
            cv_.capsule(sx(X), sy(foot), px(0.45f), sx(X), sy(s.y + s.ry), px(0.45f), {0.20f, 0.40f, 0.12f});
        }
    }

    // Bonsai: an S-curved trunk with branches and cloud-pruned foliage pads.
    const Rgb bark{0.36f, 0.27f, 0.20f}, barkLight{0.66f, 0.50f, 0.34f};
    struct Node {
        float x, y, r;
    };
    const Node trunk[] = {{60.0f, 86.5f, 3.6f}, {57.5f, 79.0f, 3.0f}, {62.5f, 71.0f, 2.6f},
                          {59.5f, 63.0f, 2.1f}, {62.5f, 56.0f, 1.7f}, {61.0f, 49.0f, 1.3f}};
    cv_.ellipse(sx(60.0f), sy(86.0f), px(6.0f), px(2.0f), 0.0f, bark);
    occ.ellipse(sx(60.0f), sy(86.0f), px(6.0f), px(2.0f), 0.0f, solid);
    const auto limb = [&](Node a, Node b) {
        cv_.capsule(sx(a.x), sy(a.y), px(a.r), sx(b.x), sy(b.y), px(b.r), bark);
        occ.capsule(sx(a.x), sy(a.y), px(a.r), sx(b.x), sy(b.y), px(b.r), solid);
        cv_.capsule(sx(a.x - a.r * 0.45f), sy(a.y), px(a.r * 0.3f), sx(b.x - b.r * 0.45f), sy(b.y), px(b.r * 0.3f), barkLight, 0.8f);
    };
    for (size_t i = 0; i + 1 < std::size(trunk); ++i) limb(trunk[i], trunk[i + 1]);
    limb({58.0f, 74.0f, 1.4f}, {47.0f, 68.5f, 1.0f});
    limb({47.0f, 68.5f, 1.0f}, {38.0f, 66.0f, 0.6f});
    limb({62.0f, 67.0f, 1.3f}, {74.0f, 62.0f, 0.9f});
    limb({74.0f, 62.0f, 0.9f}, {84.0f, 59.5f, 0.6f});
    limb({60.5f, 58.5f, 0.9f}, {52.0f, 52.5f, 0.55f});
    limb({62.5f, 54.0f, 0.8f}, {70.0f, 48.5f, 0.5f});

    for (size_t p = 0; p < std::size(Pads); ++p) {
        const Pad& pad = Pads[p];
        const float rx = pad.rx * g, ry = pad.ry * (0.95f + 0.05f * g);
        const int x0 = std::max(0, static_cast<int>(sx(pad.x - rx - 2))), x1 = std::min(w - 1, static_cast<int>(sx(pad.x + rx + 2)));
        const int y0 = std::max(0, static_cast<int>(sy(pad.y - ry - 2))), y1 = std::min(h - 1, static_cast<int>(sy(pad.y + ry + 2)));
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                const float X = (x + 0.5f - ox_) / scale_, Y = (y + 0.5f - oy_) / scale_;
                const float dx = (X - pad.x) / rx, dy = (Y - pad.y) / ry;
                // Flatter underside, domed top, ragged leafy edge.
                const float shape = 1.0f - dx * dx - sq(dy > 0 ? dy * 1.25f : dy) + 0.45f * (vnoise(X * 0.55f, Y * 0.55f, 20 + p) - 0.5f);
                if (shape <= 0.0f)
                    continue;
                const float clump = vnoise(X * 0.9f, Y * 0.9f, 30 + p), top = -dy * 0.85f - dx * 0.3f;
                Rgb c = mix({0.05f, 0.17f, 0.06f}, {0.17f, 0.40f, 0.13f}, smoothstep(-0.7f, 0.5f, top + (clump - 0.5f) * 1.3f));
                if (top > 0.25f && clump > 0.62f)
                    c = mix(c, {0.40f, 0.60f, 0.22f}, 0.8f);
                const float a = std::clamp(shape * 6.0f, 0.0f, 1.0f);
                cv_.blend(x, y, c, a);
                occ.blend(x, y, solid, a);
            }
    }

    // Fairy light string (bulbs light up at night).
    for (int i = 0; i + 1 < FairyBulbs; ++i) {
        const Point a = fairy_bulb(i), b = fairy_bulb(i + 1);
        cv_.capsule(sx(a.x), sy(a.y), px(0.25f), sx(b.x), sy(b.y), px(0.25f), {0.12f, 0.16f, 0.10f}, 0.6f);
    }
    for (int i = 0; i < FairyBulbs; ++i) {
        const Point b = fairy_bulb(i);
        cv_.ellipse(sx(b.x), sy(b.y + 0.4f), px(0.6f), px(0.75f), 0.0f, {0.55f, 0.50f, 0.40f}, 0.8f);
    }

    // Glass sheen: two faint diagonal bands on the front pane.
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            if (!glass_[static_cast<size_t>(y * w + x)])
                continue;
            const float X = (x + 0.5f - ox_) / scale_, Y = (y + 0.5f - oy_) / scale_;
            const float d = X + Y * 0.55f;
            const float band = std::exp(-sq((d - 66.0f) / 3.0f)) * 0.7f + std::exp(-sq((d - 73.0f) / 1.2f)) * 0.5f +
                               std::exp(-sq((d - 126.0f) / 2.0f)) * 0.4f;
            float* q = cv_.at(x, y);
            for (int k = 0; k < 3; ++k) q[k] += (0.9f - q[k]) * band * 0.10f;
        }

    // Brass frame.
    const Rgb brass{0.62f, 0.46f, 0.22f}, brassLight{0.92f, 0.76f, 0.42f}, brassDark{0.30f, 0.21f, 0.10f};
    const auto bar = [&](float ax, float ay, float bx, float by, float r) {
        cv_.capsule(sx(ax), sy(ay), px(r), sx(bx), sy(by), px(r), brassDark);
        cv_.capsule(sx(ax), sy(ay), px(r * 0.75f), sx(bx), sy(by), px(r * 0.75f), brass);
        cv_.capsule(sx(ax - r * 0.3f), sy(ay - r * 0.3f), px(r * 0.3f), sx(bx - r * 0.3f), sy(by - r * 0.3f), px(r * 0.3f), brassLight,
                    0.8f);
        occ.capsule(sx(ax), sy(ay), px(r), sx(bx), sy(by), px(r), solid);
    };
    bar(BoxLeft, Eave, BoxLeft, ShelfTop - 0.5f, 1.1f);
    bar(BoxRight, Eave, BoxRight, ShelfTop - 0.5f, 1.1f);
    bar(BoxLeft, Eave, BoxRight, Eave, 1.0f);
    bar(BoxLeft, Eave, BoxMid, Apex, 1.0f);
    bar(BoxMid, Apex, BoxRight, Eave, 1.0f);
    for (int y = static_cast<int>(sy(RailTop)); y < static_cast<int>(sy(ShelfTop)); ++y)
        for (int x = static_cast<int>(sx(BoxLeft - 1.0f)); x <= static_cast<int>(sx(BoxRight + 1.0f)); ++x) {
            const float v = ((y + 0.5f - oy_) / scale_ - RailTop) / (ShelfTop - RailTop);
            cv_.set(x, y, v < 0.25f ? brassLight : (v > 0.8f ? brassDark : brass));
            occ.set(x, y, solid);
        }
    cv_.ellipse(sx(BoxMid), sy(Apex - 1.8f), px(1.5f), px(1.5f), 0.0f, brass);
    cv_.ellipse(sx(BoxMid - 0.5f), sy(Apex - 2.3f), px(0.6f), px(0.6f), 0.0f, brassLight);
    occ.ellipse(sx(BoxMid), sy(Apex - 1.8f), px(1.5f), px(1.5f), 0.0f, solid);

    albedo_ = cv_.px;
    opaque_.resize(n);
    for (size_t i = 0; i < n; ++i) opaque_[i] = occ.px[i * 3];
}

void BonsaiTerrariumScene::build_light()
{
    const auto& L = env();
    const int w = cv_.w, h = cv_.h;
    const size_t n = static_cast<size_t>(w * h);
    light_.resize(n * 3);
    backdrop_.resize(n * 3);
    beamAir_.resize(n);

    Rgb ambient = mix(add({0.08f, 0.09f, 0.14f}, scale({0.06f, 0.07f, 0.12f}, L.moon_light)), {0.64f, 0.62f, 0.60f}, L.daylight);
    ambient = add(mul(ambient, mix({1, 1, 1}, {1.08f, 0.86f, 0.78f}, L.twilight)), scale({0.13f, 0.09f, 0.08f}, L.twilight));
    const float sunUp = smoothstep(-4.0f, 3.0f, L.sun_elevation);
    Rgb sun = from_env(L.sun_color);
    if (L.morning) {
        // Dawn: paler rose-gold light and a cooler room than the amber evening.
        sun = mix(sun, {1.0f, 0.80f, 0.68f}, 0.6f * L.twilight);
        ambient = mul(ambient, mix({1, 1, 1}, {0.86f, 0.92f, 1.22f}, L.twilight));
    }
    beam_ = add(scale(sun, (0.95f - 0.2f * L.daylight) * sunUp),
                scale({0.20f, 0.25f, 0.38f}, 0.6f * L.moon_light * (1.0f - smoothstep(-6.0f, 0.0f, L.sun_elevation))));
    // Window patch: long, low and slanted when the sun is low, shorter and
    // steeper by day; it slides across the wall over the day.
    const float e = std::lerp(0.5f, std::clamp(L.sun_elevation / 45.0f, 0.0f, 1.0f), smoothstep(-3.0f, 3.0f, L.sun_elevation));
    const float top = std::lerp(26.0f, 18.0f, e), height = std::lerp(56.0f, 86.0f, e), width = std::lerp(70.0f, 46.0f, e);
    const float skew = std::lerp(1.1f, 0.4f, e), left = std::lerp(-40.0f, 0.0f, e) + (L.sun_x - 0.5f) * 24.0f;
    const float shx = 3.0f + 10.0f * skew, shy = 6.0f;
    const float lamp = L.lamp();
    const Rgb warm{1.0f, 0.62f, 0.28f};
    Point bulbs[FairyBulbs];
    for (int i = 0; i < FairyBulbs; ++i) bulbs[i] = fairy_bulb(i);

    const auto in_box = [&](float X, float Y) {
        if (X < BoxLeft - 1 || X > BoxRight + 1 || Y > ShelfTop || Y < Apex - 3)
            return false;
        return Y >= Eave || Y >= Apex + (Eave - Apex) * std::abs(X - BoxMid) / (BoxMid - BoxLeft) - 1.0f;
    };
    const auto opaque_at = [&](float X, float Y) {
        const int x = static_cast<int>(sx(X)), y = static_cast<int>(sy(Y));
        if (x < 0 || y < 0 || x >= w || y >= h)
            return 0.0f;
        return opaque_[static_cast<size_t>(y * w + x)];
    };

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const size_t i = static_cast<size_t>(y * w + x);
            const float X = (x + 0.5f - ox_) / scale_, Y = (y + 0.5f - oy_) / scale_;
            const float v = (Y - top) / height, u = (X - left - (Y - top) * skew) / width;
            float b = smoothstep(0.0f, 0.12f, u) * smoothstep(1.0f, 0.88f, u) * smoothstep(0.0f, 0.08f, v) * smoothstep(1.0f, 0.9f, v);
            // Window bars.
            b *= 1.0f - 0.5f * (1.0f - smoothstep(0.01f, 0.04f, std::abs(u - 0.5f)));
            b *= 1.0f - 0.5f * (1.0f - smoothstep(0.01f, 0.035f, std::abs(v - 0.46f)));
            beamAir_[i] = b;
            const bool part = in_box(X, Y) && (opaque_[i] > 0.5f || glass_[i]);
            if (Y >= ShelfBottom)
                b = 0.0f;  // the shelf blocks it
            else if (Y >= ShelfFront)
                b *= 0.75f;  // the shelf's front face
            else if (glass_[i] && opaque_[i] < 0.5f)
                b *= 0.7f;  // back wall seen through the terrarium
            else if (part)
                b *= 0.72f;  // the terrarium and its plants
            else if (in_box(X - shx, Y - shy))
                b *= 0.74f - 0.64f * opaque_at(X - shx, Y - shy);  // its shadow on the wall
            Rgb l = add(ambient, scale(beam_, b));
            if (lamp > 0.01f) {
                float glow = 0.0f;
                for (const Point& p : bulbs) glow += 1.0f / (1.0f + (sq(X - p.x) + sq(Y - p.y)) * 0.035f);
                if (in_box(X, Y))
                    glow += 0.5f;
                l = add(l, scale(warm, lamp * 0.3f * glow));
            }
            light_[i * 3] = l.r;
            light_[i * 3 + 1] = l.g;
            light_[i * 3 + 2] = l.b;
            for (int k = 0; k < 3; ++k) backdrop_[i * 3 + k] = albedo_[i * 3 + k] * light_[i * 3 + k];
        }

    // How much the glass fogs: most in the cool hours, least by day.
    condensation_ = std::clamp(0.45f + 0.4f * (1.0f - L.daylight) + 0.2f * (L.morning ? L.twilight : 0.0f), 0.0f, 1.0f);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const size_t i = static_cast<size_t>(y * w + x);
            if (!glass_[i]) {
                fogTarget_[i] = 0.0f;
                continue;
            }
            const float X = (x + 0.5f - ox_) / scale_, Y = (y + 0.5f - oy_) / scale_;
            const float v = std::clamp((Y - PaneTop) / (PaneBottom - PaneTop), 0.0f, 1.0f);
            const float edge = 1.0f - std::clamp(std::min(X - PaneLeft, PaneRight - X) / 10.0f, 0.0f, 1.0f);
            const float patch = smoothstep(0.3f, 0.75f, vnoise(X * 0.09f, Y * 0.09f, 50));
            const float corner = smoothstep(0.6f, 1.0f, v) * edge;
            float f = condensation_ * (0.08f + 0.6f * smoothstep(0.55f, 0.0f, v) * (0.5f + 0.5f * patch) + 0.6f * corner + 0.35f * patch);
            if (glass_[i] == 2)
                f *= 0.85f;
            fogTarget_[i] = std::clamp(f, 0.0f, 1.0f);
        }
}

Rgb BonsaiTerrariumScene::lit_at(float x, float y, Rgb albedo) const
{
    const int ix = std::clamp(static_cast<int>(sx(x)), 0, cv_.w - 1), iy = std::clamp(static_cast<int>(sy(y)), 0, cv_.h - 1);
    const float* l = &light_[static_cast<size_t>((iy * cv_.w + ix) * 3)];
    return {albedo.r * l[0], albedo.g * l[1], albedo.b * l[2]};
}

// ---------------------------------------------------------------- moving parts

void BonsaiTerrariumScene::update_fog(float dt)
{
    if (!fogPrimed_) {
        fog_ = fogTarget_;
        fogPrimed_ = true;
        return;
    }
    const float rate = std::min(1.0f, dt / 25.0f);
    for (size_t i = 0; i < fog_.size(); ++i) fog_[i] += (fogTarget_[i] - fog_[i]) * rate;
}

void BonsaiTerrariumScene::update_drops(float dt)
{
    const int w = cv_.w, h = cv_.h;
    const auto fog_at = [&](float X, float Y) -> float& {
        const int x = std::clamp(static_cast<int>(sx(X)), 0, w - 1), y = std::clamp(static_cast<int>(sy(Y)), 0, h - 1);
        return fog_[static_cast<size_t>(y * w + x)];
    };
    const size_t cap = static_cast<size_t>(90.0f * render_quality_scale());

    // New droplets bead where the glass is fogged.
    spawnDebt_ += dt * 0.9f * condensation_;
    while (spawnDebt_ >= 1.0f) {
        spawnDebt_ -= 1.0f;
        const float X = PaneLeft + 1 + rnd() * (PaneRight - PaneLeft - 2), Y = PaneTop + 1 + rnd() * (PaneBottom - PaneTop - 6);
        if (drops_.size() < cap && rnd() < fog_at(X, Y) * 1.4f)
            drops_.push_back({X, Y, 0.3f, 0.0f, rnd() * kTau});
    }

    std::vector<Drop> residue;
    for (auto& d : drops_) {
        if (d.speed <= 0.0f) {
            d.r += dt * (0.004f + 0.022f * fog_at(d.x, d.y));
            if (d.r > 1.05f && rnd() < dt * 0.6f)
                d.speed = 2.0f;
            continue;
        }
        // Running: accelerate with size, wander a little, collect what it meets.
        d.speed = std::min(d.speed + dt * 12.0f, 3.0f + 9.0f * (d.r - 0.5f));
        const float step = d.speed * dt;
        d.y += step;
        d.x += std::sin(d.wobble + d.y * 0.7f) * 0.18f * step;
        d.r -= step * 0.010f;
        for (float o = -d.r; o <= d.r; o += 0.5f) fog_at(d.x + o, d.y) *= 0.4f;
        if (rnd() < step * 0.06f)
            residue.push_back({d.x, d.y - 0.6f, 0.25f + 0.15f * rnd(), 0.0f, 0.0f});
        for (auto& o : drops_)
            if (&o != &d && o.speed <= 0.0f && o.r > 0.0f && std::abs(o.x - d.x) < d.r + o.r && std::abs(o.y - d.y) < d.r + o.r) {
                d.r = std::sqrt(d.r * d.r + o.r * o.r);
                o.r = 0.0f;
            }
        if (d.r < 0.55f)
            d.speed = 0.0f;  // rests again
    }
    drops_.erase(std::remove_if(drops_.begin(), drops_.end(), [](const Drop& d) { return d.r <= 0.0f || d.y > PaneBottom - 0.5f; }),
                 drops_.end());
    for (const auto& r : residue)
        if (drops_.size() < cap + 20)
            drops_.push_back(r);
}

void BonsaiTerrariumScene::draw_fog()
{
    const size_t n = fog_.size();
    for (size_t i = 0; i < n; ++i) {
        const float f = fog_[i];
        if (f <= 0.01f)
            continue;
        float* p = &cv_.px[i * 3];
        const float* l = &light_[i * 3];
        const float a = f * 0.55f;
        for (int k = 0; k < 3; ++k) p[k] += (l[k] * 0.72f + 0.015f - p[k]) * a;
    }
}

void BonsaiTerrariumScene::draw_drops()
{
    for (const auto& d : drops_) {
        const float X = sx(d.x), Y = sy(d.y);
        const int ix = static_cast<int>(std::floor(X - 0.5f)), iy = static_cast<int>(std::floor(Y - 0.5f));
        const float fx = X - 0.5f - ix, fy = Y - 0.5f - iy;
        const Rgb shine = lit_at(d.x, d.y, {0.95f, 0.97f, 1.0f});
        const float a = std::clamp((d.r - 0.3f) * 1.1f, 0.0f, 0.95f);
        // Bilinear splat of the bright face, a darker refracted edge below big drops.
        cv_.blend(ix, iy, shine, a * (1 - fx) * (1 - fy));
        cv_.blend(ix + 1, iy, shine, a * fx * (1 - fy));
        cv_.blend(ix, iy + 1, shine, a * (1 - fx) * fy);
        cv_.blend(ix + 1, iy + 1, shine, a * fx * fy);
        if (d.r > 0.7f) {
            // Big drops: a darker refracted belly under the bright cap.
            const int bx = static_cast<int>(X), by = static_cast<int>(Y) + 1;
            cv_.blend(bx, by, scale(shine, 0.35f), 0.5f);
            cv_.blend(bx, by + 1, {0, 0, 0}, 0.2f * std::min(1.0f, d.r - 0.7f));
        }
    }
}

void BonsaiTerrariumScene::update_snail(float dt)
{
    for (auto& s : slime_) s.age += dt;
    slime_.erase(std::remove_if(slime_.begin(), slime_.end(), [](const Trail& s) { return s.age > 90.0f; }), slime_.end());
    if (!snailActive_) {
        snailWait_ -= dt;
        if (snailWait_ <= 0.0f) {
            snailActive_ = true;
            snailDir_ = rnd() < 0.5f ? 1.0f : -1.0f;
            snailX_ = snailDir_ > 0 ? 30.0f : 98.0f;
            snailPause_ = 0.0f;
            snailEyes_ = 0.0f;
        }
        return;
    }
    if (snailPause_ > 0.0f) {
        snailPause_ -= dt;
        snailEyes_ = std::max(0.25f, snailEyes_ - dt * 1.5f);
        return;
    }
    snailEyes_ = std::min(1.0f, snailEyes_ + dt * 0.8f);
    snailCrawl_ += dt;
    const float pace = 0.7f + 0.5f * std::sin(snailCrawl_ * 2.2f);  // muscular waves
    const float before = snailX_;
    snailX_ += snailDir_ * 1.1f * pace * dt;
    if (std::floor(before) != std::floor(snailX_))
        slime_.push_back({snailX_ - snailDir_ * 3.0f, ground_top(snailX_ - snailDir_ * 3.0f), 0.0f});
    if (rnd() < dt * 0.04f)
        snailPause_ = 3.0f + 4.0f * rnd();
    if (snailX_ < 28.0f || snailX_ > 100.0f) {
        snailActive_ = false;
        snailWait_ = 35.0f + 50.0f * rnd();
    }
}

void BonsaiTerrariumScene::draw_snail()
{
    for (const auto& s : slime_) {
        const float a = 0.14f * (1.0f - s.age / 90.0f);
        const int x = static_cast<int>(sx(s.x)), y = static_cast<int>(sy(s.y));
        const float* l = &light_[static_cast<size_t>((std::clamp(y, 0, cv_.h - 1) * cv_.w + std::clamp(x, 0, cv_.w - 1)) * 3)];
        cv_.add(x, y, {l[0], l[1], l[2]}, a);
    }
    if (!snailActive_)
        return;
    // Fades in and out at the ends of its path (behind the fern, by the frame).
    const float alpha = smoothstep(28.0f, 33.0f, snailX_) * smoothstep(100.0f, 95.0f, snailX_);
    const float dir = snailDir_, stretch = 0.3f * std::sin(snailCrawl_ * 2.2f);
    const float tailX = snailX_ - dir * (3.4f + stretch), headX = snailX_ + dir * (3.0f + stretch);
    const float tailY = ground_top(tailX) - 0.6f, headY = ground_top(headX) - 1.0f;
    // The snail stays readable in the shade: a floor under the light it gets.
    const auto shade = [&](float x, float y, Rgb albedo) {
        const Rgb l = lit_at(x, y, {1, 1, 1});
        const float m = std::max({l.r, l.g, l.b}), lift = m < 0.35f ? 0.35f / std::max(m, 0.05f) : 1.0f;
        return scale(mul(albedo, l), std::min(lift, 2.2f));
    };
    const Rgb body = shade(snailX_, headY, {0.66f, 0.60f, 0.50f});
    cv_.capsule(sx(tailX), sy(tailY), px(0.6f), sx(headX), sy(headY), px(1.1f), body, alpha);
    const float e = snailEyes_;
    for (const float k : {0.0f, 1.0f}) {
        const float ex = headX + dir * (0.9f + 0.7f * k) * e, ey = headY - (0.8f + 2.4f * e) + k * 0.4f;
        cv_.capsule(sx(headX + dir * 0.2f * k), sy(headY - 0.5f), px(0.32f), sx(ex), sy(ey), px(0.3f), body, alpha * (k > 0 ? 1.0f : 0.7f));
        cv_.blend(static_cast<int>(sx(ex)), static_cast<int>(sy(ey)), scale(body, 0.5f), alpha * 0.8f);
    }
    const float cx = snailX_ - dir * 0.7f, cy = ground_top(snailX_) - 3.1f;
    cv_.ellipse(sx(cx), sy(cy), px(2.9f), px(2.6f), 0.0f, shade(cx, cy, {0.78f, 0.55f, 0.24f}), alpha);  // ochre rim
    cv_.ellipse(sx(cx + dir * 0.2f), sy(cy + 0.1f), px(2.2f), px(1.9f), 0.0f, shade(cx, cy, {0.50f, 0.31f, 0.14f}), alpha);
    cv_.ellipse(sx(cx + dir * 0.4f), sy(cy + 0.2f), px(1.2f), px(1.1f), 0.0f, shade(cx, cy, {0.28f, 0.16f, 0.08f}), alpha * 0.85f);
    cv_.ellipse(sx(cx + dir * 0.5f), sy(cy + 0.3f), px(0.5f), px(0.5f), 0.0f, shade(cx, cy, {0.74f, 0.54f, 0.28f}), alpha);
    cv_.ellipse(sx(cx - dir * 0.9f), sy(cy - 1.4f), px(0.7f), px(0.45f), 0.0f, shade(cx, cy, {0.95f, 0.82f, 0.55f}), alpha * 0.7f);
}

void BonsaiTerrariumScene::draw_shoots()
{
    // Fresh tips push out of the pads over the hours, then get pinched back.
    const float g = growth();
    for (size_t p = 0; p < std::size(Pads); ++p)
        for (int k = 0; k < 3; ++k) {
            const uint32_t id = static_cast<uint32_t>(p * 3 + k);
            const float ph = fract(g * 3.0f + hash01(id + 90));
            const float len = 2.6f * smoothstep(0.0f, 0.8f, ph) * (1.0f - smoothstep(0.93f, 1.0f, ph));
            if (len < 0.3f)
                continue;
            const Pad& pad = Pads[p];
            const float u = (hash01(id + 120) - 0.5f) * 1.4f;
            const float bx = pad.x + u * pad.rx, by = pad.y - pad.ry * std::sqrt(std::max(0.05f, 1.0f - u * u)) * 0.85f;
            const float lean = u * 0.6f;
            const Rgb c = lit_at(bx, by, {0.50f, 0.74f, 0.26f});
            cv_.capsule(sx(bx), sy(by), px(0.5f), sx(bx + lean * len), sy(by - len), px(0.35f), c);
        }
}

void BonsaiTerrariumScene::draw_fairy_lights()
{
    const float lamp = env().lamp();
    if (lamp <= 0.02f)
        return;
    const Rgb warm{1.0f, 0.70f, 0.34f};
    for (int i = 0; i < FairyBulbs; ++i) {
        const Point b = fairy_bulb(i);
        const float twinkle = 0.75f + 0.25f * std::sin(t_ * (0.5f + 0.13f * i) + i * 1.9f);
        const float k = lamp * twinkle * (0.35f + 0.65f * hash01(static_cast<uint32_t>(i) * 17U + 5U));
        cv_.glow(sx(b.x), sy(b.y + 0.4f), px(3.0f), warm, 0.28f * k);
        cv_.blend(static_cast<int>(sx(b.x)), static_cast<int>(sy(b.y + 0.4f)), {1.0f, 0.88f, 0.62f}, k);
    }
}

void BonsaiTerrariumScene::draw_motes(float dt)
{
    const float strength = std::max(beam_.r, std::max(beam_.g, beam_.b));
    for (auto& m : motes_) {
        m.vx += (rnd() - 0.5f) * dt * 0.3f;
        m.vy += (rnd() - 0.5f) * dt * 0.2f - m.vy * dt * 0.1f;
        m.vx = std::clamp(m.vx, -0.6f, 0.6f);
        m.vy = std::clamp(m.vy, -0.3f, 0.35f);
        m.x += m.vx * dt;
        m.y += m.vy * dt;
        if (m.x < -2)
            m.x += 130;
        if (m.x > 128)
            m.x -= 130;
        if (m.y < 0 || m.y > ShelfTop)
            m.y = 4.0f + rnd() * 90.0f;
        m.phase += dt * 0.9f;
        const int x = static_cast<int>(sx(m.x)), y = static_cast<int>(sy(m.y));
        if (x < 0 || y < 0 || x >= cv_.w || y >= cv_.h)
            continue;
        const float a = beamAir_[static_cast<size_t>(y * cv_.w + x)] * strength * (0.5f + 0.5f * std::sin(m.phase)) * 0.22f;
        if (a > 0.01f)
            cv_.add(x, y, beam_, a);
    }
}

bool BonsaiTerrariumScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt = 0.0f;
    begin_frame(dt);
    const int step = static_cast<int>(growth() * 40.0f);
    if (step != growthStep_) {
        growthStep_ = step;
        build_albedo();
        lightKey_.fill(-1000.0f);
    }
    if (environment_changed(lightKey_))
        build_light();

    update_fog(dt);
    update_drops(dt);
    update_snail(dt);

    cv_.px = backdrop_;
    draw_shoots();
    draw_snail();
    draw_fairy_lights();
    draw_fog();
    draw_drops();
    draw_motes(dt);
    present(canvas);
    wait_until_next_frame();
    return true;
}

Scenes::SceneDescriptor BonsaiTerrariumScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "scenery";
    d.tags = {"scenery", "calm", "indoor", "plants", "fits:dawn", "fits:day", "fits:dusk", "fits:night"};
    d.intensity = 0.05f;
    d.motion = 0.05f;
    d.music_affinity = 0.0f;
    d.performance_cost = 0.12f;
    environment_->add_variants(d);
    return d;
}

}  // namespace Hobbies
