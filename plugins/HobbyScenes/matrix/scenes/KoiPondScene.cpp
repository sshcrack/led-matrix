#include "KoiPondScene.h"

namespace Hobbies {
namespace {

// Layout in a 128×128 design space, scaled to the canvas.
constexpr float CX = 62.0f, CY = 67.0f, Radius = 47.0f;
constexpr Point Lantern{111.0f, 112.0f};
constexpr Point Maple{0.0f, 2.0f};
constexpr float SpineGap = 1.45f;
constexpr std::array<float, 9> BodyRadius{1.45f, 1.95f, 2.2f, 2.15f, 1.9f, 1.5f, 1.1f, 0.75f, 0.5f};

Rgb koi_color(int pattern, uint32_t id, int segment)
{
    const Rgb white{0.95f, 0.93f, 0.88f}, red{0.92f, 0.22f, 0.10f}, black{0.10f, 0.10f, 0.12f};
    const float h = hash01(id * 977u + static_cast<uint32_t>(segment) * 131u);
    const bool patch = hash01(id * 31u + static_cast<uint32_t>(segment / 2) * 7u) > 0.45f;
    switch (pattern) {
        case 0:
            return patch && segment < 7 ? red : white;  // kohaku
        case 1:
            return mix({1.0f, 0.52f, 0.10f}, {1.0f, 0.75f, 0.35f}, segment == 0 ? 0.6f : 0.0f);  // orange ogon
        case 2:
            return {1.0f, 0.80f, 0.26f};  // yamabuki
        case 3:
            return patch ? (h > 0.5f ? red : white) : black;  // showa
        case 4:
            return h > 0.85f ? black : (patch && segment < 7 ? red : white);  // sanke
        default:
            return mix({0.62f, 0.42f, 0.24f}, {0.78f, 0.58f, 0.36f}, h * 0.5f);  // chagoi
    }
}

}  // namespace

KoiPondScene::KoiPondScene()
{
    Environment::Adapt adapt;
    // The pond is seen from straight above: no fog bank, no storm sky.
    adapt.weather_allowed = {Environment::Weather::Clear,   Environment::Weather::PartlyCloudy, Environment::Weather::Overcast,
                             Environment::Weather::Drizzle, Environment::Weather::Rain,         Environment::Weather::Snow};
    use_environment({Environment::TimeOfDay::Day, Environment::Weather::Clear, Environment::Season::Summer, 45.0f, 0.35f}, adapt);
}

float KoiPondScene::rim(float angle) const
{
    return Radius *
           (1.0f + 0.10f * std::sin(2.0f * angle + 0.7f) + 0.06f * std::sin(3.0f * angle + 2.1f) + 0.03f * std::sin(5.0f * angle + 0.4f));
}

float KoiPondScene::inside(float x, float y) const
{
    const float dx = x - CX, dy = y - CY;
    return rim(std::atan2(dy, dx)) - std::sqrt(dx * dx + dy * dy);
}

void KoiPondScene::initialize(int width, int height)
{
    HobbySceneBase::initialize(width, height);
    backdrop_.clear();
    ripples_.clear();
    floaters_.clear();
    floatClock_ = 0.0f;
    seeded_ = false;
    fly_ = {};
    fly_.wait = 4.0f;

    koi_.clear();
    for (int i = 0; i < 6; ++i) {
        Koi k{};
        const float a = kTau * i / 6.0f + rnd(), r = 8.0f + 22.0f * rnd();
        k.heading = rnd() * kTau;
        k.speed = 5.5f + 3.0f * rnd();
        k.turn = 0.0f;
        k.wander = rnd() * 100.0f;
        k.size = i == 0 ? 1.25f : 0.85f + 0.3f * rnd();
        k.pattern = i;
        const Point head{CX + std::cos(a) * r, CY + std::sin(a) * r};
        for (size_t s = 0; s < k.spine.size(); ++s)
            k.spine[s] = {head.x - std::cos(k.heading) * SpineGap * k.size * s, head.y - std::sin(k.heading) * SpineGap * k.size * s};
        koi_.push_back(k);
    }
    pads_.clear();
    const std::array<std::array<float, 3>, 7> padSpots{
        {{40, 50, 6.0f}, {48, 44, 4.5f}, {86, 82, 6.5f}, {93, 74, 4.6f}, {78, 90, 4.0f}, {36, 88, 5.2f}, {72, 34, 4.2f}}};
    for (size_t i = 0; i < padSpots.size(); ++i)
        pads_.push_back({padSpots[i][0], padSpots[i][1], padSpots[i][2], rnd() * kTau, (rnd() - 0.5f) * 0.04f, i == 0 || i == 2 || i == 5});
}

void KoiPondScene::build_backdrop()
{
    const auto& L = env();
    const int W = cv_.w, H = cv_.h;
    const float sx = W / 128.0f, sy = H / 128.0f, k = std::min(sx, sy);
    Canvas bg;
    bg.resize(W, H);
    depth_.assign(static_cast<size_t>(W * H), 0.0f);
    ice_.assign(static_cast<size_t>(W * H), 0.0f);
    canopy_.assign(static_cast<size_t>(W * H) * 4, 0.0f);

    // Lawn and moss around the pond, with a little texture.
    const Rgb grassA = meadow({0.22f, 0.40f, 0.16f}), grassB = meadow({0.30f, 0.47f, 0.20f}), moss = meadow({0.17f, 0.30f, 0.14f});
    const Rgb shallow = lit({0.22f, 0.36f, 0.30f}), deep = lit({0.03f, 0.10f, 0.14f});
    const Rgb skyTint = from_env(L.sky(0.35f));
    const float freeze = std::clamp((3.0f - L.temperature) / 7.0f, 0.0f, 1.0f) * std::max(L.winter, L.snow_cover);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const float dx = x + 0.5f, dy = y + 0.5f;
            const float in = inside(dx / sx, dy / sy);
            const size_t i = static_cast<size_t>(y * W + x);
            if (in > 0.0f) {
                const float d = std::clamp(in / 26.0f, 0.0f, 1.0f);
                depth_[i] = std::max(0.02f, d);
                Rgb c = mix(shallow, deep, std::sqrt(d));
                c = mix(c, skyTint, 0.16f + 0.10f * (1.0f - d));
                // Pebbles show through the shallow edge.
                if (d < 0.25f && hash01(static_cast<uint32_t>(i) * 2654435761u) > 0.8f)
                    c = mix(c, lit({0.45f, 0.42f, 0.36f}), 0.35f * (1.0f - d * 4.0f));
                bg.set(x, y, c);
                // Ice grows inward from the rim, thinner where the water is deep.
                const float iceWidth = 3.0f + 16.0f * freeze;
                if (freeze > 0.02f)
                    ice_[i] = std::clamp((iceWidth - in) / 3.0f, 0.0f, 1.0f) * std::min(1.0f, freeze * 2.0f);
            }
            else {
                const float n = hash01(static_cast<uint32_t>(x * 73 + y * 9973));
                Rgb c = mix(grassA, grassB, n);
                if (in > -9.0f)
                    c = mix(c, moss, 0.6f);
                bg.set(x, y, c);
            }
        }

    // A ring of rounded stones along the edge, snow on their tops.
    for (int s = 0; s < 46; ++s) {
        const float a = kTau * (s + 0.3f * hash01(s * 17u)) / 46.0f, r = rim(a) + 2.5f;
        const float x = CX + std::cos(a) * r, y = CY + std::sin(a) * r;
        const float size = 2.6f + 1.6f * hash01(s * 7u + 1u);
        const Rgb stone = lit(mix({0.42f, 0.41f, 0.40f}, {0.58f, 0.55f, 0.50f}, hash01(s * 3u + 2u)));
        bg.ellipse((x + 0.6f) * sx, (y + 0.8f) * sy, size * k, size * 0.8f * k, a, scale(stone, 0.4f), 0.6f);
        bg.ellipse(x * sx, y * sy, size * k, size * 0.8f * k, a, stone);
        bg.ellipse((x - 0.6f) * sx, (y - 0.6f) * sy, size * 0.55f * k, size * 0.4f * k, a,
                   terrain(mix(stone, {0.8f, 0.78f, 0.74f}, 0.3f), 0.0f, 1.0f), 0.8f);
    }

    // A stone lantern on the lawn (lit at night, see render()).
    {
        // Seen from above: a square pyramid roof with a knob, its shadow on the lawn.
        const Rgb light = terrain({0.62f, 0.60f, 0.56f}, 0.0f, 1.0f), dark = terrain({0.36f, 0.35f, 0.34f}, 0.0f, 0.6f);
        const float half = 6.5f;
        for (int y = static_cast<int>((Lantern.y - half) * sy); y <= static_cast<int>((Lantern.y + half + 3) * sy); ++y)
            for (int x = static_cast<int>((Lantern.x - half) * sx); x <= static_cast<int>((Lantern.x + half + 3) * sx); ++x) {
                const float dx = (x + 0.5f) / sx - Lantern.x, dy = (y + 0.5f) / sy - Lantern.y;
                if (std::max(std::fabs(dx - 2.5f), std::fabs(dy - 2.5f)) < half)
                    bg.blend(x, y, {0.03f, 0.05f, 0.03f}, 0.4f);  // shadow
            }
        for (int y = static_cast<int>((Lantern.y - half) * sy); y <= static_cast<int>((Lantern.y + half) * sy); ++y)
            for (int x = static_cast<int>((Lantern.x - half) * sx); x <= static_cast<int>((Lantern.x + half) * sx); ++x) {
                const float dx = (x + 0.5f) / sx - Lantern.x, dy = (y + 0.5f) / sy - Lantern.y;
                if (std::max(std::fabs(dx), std::fabs(dy)) >= half)
                    continue;
                // Four roof faces: lit from the upper left, darker to the lower right.
                const float face = std::fabs(dx) > std::fabs(dy) ? (dx < 0 ? 1.0f : 0.45f) : (dy < 0 ? 0.85f : 0.35f);
                const bool ridge = std::fabs(std::fabs(dx) - std::fabs(dy)) < 0.6f;
                bg.set(x, y, ridge ? dark : mix(dark, light, face));
            }
        bg.ellipse(Lantern.x * sx, Lantern.y * sy, 1.8f * k, 1.8f * k, 0.0f, light);
    }
    // Stepping stones across the lawn to the pond, and flowers in the grass
    // from spring to autumn.
    for (int i = 0; i < 4; ++i) {
        const float x = 8.0f + 9.0f * i, y = 122.0f - 5.0f * i;
        const Rgb flag = terrain(mix({0.50f, 0.48f, 0.44f}, {0.60f, 0.57f, 0.52f}, hash01(i * 5u)), 0.0f, 1.0f);
        bg.ellipse((x + 0.8f) * sx, (y + 0.9f) * sy, 3.6f * k, 2.6f * k, 0.3f * i, {0.03f, 0.05f, 0.03f}, 0.4f);
        bg.ellipse(x * sx, y * sy, 3.6f * k, 2.6f * k, 0.3f * i, flag);
    }
    const float blooms = std::clamp(L.spring + L.summer - L.snow_cover, 0.0f, 1.0f);
    if (blooms > 0.05f) {
        const std::array<Rgb, 4> petals{{{1.0f, 0.95f, 0.6f}, {0.95f, 0.45f, 0.65f}, {0.75f, 0.6f, 1.0f}, {1.0f, 1.0f, 0.95f}}};
        for (uint32_t i = 0; i < 110; ++i) {
            const float x = 128.0f * hash01(i * 7u + 3u), y = 128.0f * hash01(i * 7u + 4u);
            if (inside(x, y) > -6.0f || std::hypot(x - Lantern.x, y - Lantern.y) < 9.0f || x + y < 40.0f)
                continue;
            bg.blend(static_cast<int>(x * sx), static_cast<int>(y * sy), lit(petals[i % petals.size()]), 0.85f * blooms);
        }
    }

    // The maple crown over the top-left corner: clustered blobs of leaves in
    // the season's colour, bare branches with snow in winter.
    {
        const float bare = std::clamp(L.winter * 1.2f, 0.0f, 1.0f);
        const Rgb leafA = plant({0.24f, 0.46f, 0.18f}), leafB = plant({0.36f, 0.58f, 0.22f}), bark = lit({0.30f, 0.24f, 0.20f});
        const Rgb snow = lit({0.92f, 0.94f, 0.98f});
        auto put = [&](int x, int y, Rgb c, float a) {
            if (x < 0 || y < 0 || x >= W || y >= H || a <= 0.0f)
                return;
            float* p = &canopy_[static_cast<size_t>(y * W + x) * 4];
            const float na = p[3] + a * (1.0f - p[3]);
            for (int ch = 0; ch < 3; ++ch) p[ch] = (p[ch] * p[3] * (1.0f - a) + (&c.r)[ch] * a) / std::max(na, 1e-4f);
            p[3] = na;
        };
        // Branches radiate from the trunk off the corner.
        for (int b = 0; b < 9; ++b) {
            const float a = 0.02f + 1.52f * b / 8.0f, len = 30.0f + 12.0f * hash01(b * 11u);
            for (float t = 0; t < len; t += 0.5f) {
                const float x = Maple.x + std::cos(a) * t + 2.0f * std::sin(t * 0.3f + b), y = Maple.y + std::sin(a) * t;
                const float w = 1.3f * (1.0f - t / len) + 0.25f;
                for (float o = -w; o <= w; o += 0.5f)
                    put(static_cast<int>((x - std::sin(a) * o) * sx), static_cast<int>((y + std::cos(a) * o) * sy), bark, 0.9f);
                if (L.snow_cover > 0.05f && hash01(static_cast<uint32_t>(t * 4) + b * 101u) > 0.5f)
                    put(static_cast<int>(x * sx), static_cast<int>(y * sy), snow, 0.8f * L.snow_cover);
            }
        }
        const float leafy = 1.0f - bare;
        if (leafy > 0.02f)
            for (int i = 0; i < 120; ++i) {
                const float a = 1.57f * hash01(i * 13u + 5u), r = 40.0f * std::sqrt(hash01(i * 29u + 3u));
                const float bx = Maple.x + std::cos(a) * r, by = Maple.y + std::sin(a) * r, br = 3.0f + 3.5f * hash01(i * 41u);
                const Rgb c = mix(leafA, leafB, hash01(i * 53u));
                for (int y = static_cast<int>((by - br) * sy); y <= static_cast<int>((by + br) * sy); ++y)
                    for (int x = static_cast<int>((bx - br) * sx); x <= static_cast<int>((bx + br) * sx); ++x) {
                        const float dx = (x + 0.5f) / sx - bx, dy = (y + 0.5f) / sy - by, d = std::sqrt(dx * dx + dy * dy) / br;
                        if (d >= 1.0f)
                            continue;
                        // Sunlit from the upper left, darker underneath; leaves thin out at the edge.
                        const float shade = 0.75f + 0.35f * std::clamp(-(dx + dy) / (2.0f * br), -0.5f, 0.6f);
                        const float speck = hash01(static_cast<uint32_t>(x * 7 + y * 1031));
                        if (d > 0.7f && speck > 0.6f)
                            continue;
                        put(x, y, scale(c, shade * (0.85f + 0.3f * speck)), leafy * std::clamp((1.0f - d) * 3.0f, 0.0f, 1.0f));
                    }
            }
    }
    backdrop_ = std::move(bg.px);
}

void KoiPondScene::update_koi(float dt)
{
    const auto& L = env();
    const float chill = 1.0f - 0.55f * std::clamp((6.0f - L.temperature) / 10.0f, 0.0f, 1.0f);
    for (size_t n = 0; n < koi_.size(); ++n) {
        auto& k = koi_[n];
        k.wander += dt * 0.35f;
        // Gentle wandering, plus a firm turn back towards open water near the edge.
        float steer = 0.6f * std::sin(k.wander * 1.7f + n) + 0.4f * std::sin(k.wander * 0.63f + n * 2.3f);
        const Point ahead{k.spine[0].x + std::cos(k.heading) * 9.0f, k.spine[0].y + std::sin(k.heading) * 9.0f};
        if (inside(ahead.x, ahead.y) < 7.0f) {
            const float toCenter = std::atan2(CY - k.spine[0].y, CX - k.spine[0].x);
            const float diff = std::remainder(toCenter - k.heading, kTau);
            steer = diff > 0 ? 2.2f : -2.2f;
        }
        // Keep a little distance from the other fish.
        for (size_t m = 0; m < koi_.size(); ++m) {
            if (m == n)
                continue;
            const float dx = koi_[m].spine[2].x - ahead.x, dy = koi_[m].spine[2].y - ahead.y;
            if (dx * dx + dy * dy < 30.0f)
                steer += std::remainder(std::atan2(dy, dx) - k.heading, kTau) > 0 ? -0.9f : 0.9f;
        }
        k.turn += (steer - k.turn) * std::min(1.0f, dt * 1.2f);
        const float speed = k.speed * chill * (0.75f + 0.25f * std::sin(k.wander * 2.1f + n));
        k.heading += k.turn * 0.55f * dt;
        // A swimming fish swings its head a little from side to side.
        const float swing = 0.32f * std::sin(t_ * (2.2f + 0.3f * n) + n);
        k.spine[0].x += std::cos(k.heading + swing) * speed * dt;
        k.spine[0].y += std::sin(k.heading + swing) * speed * dt;
        const float gap = SpineGap * k.size;
        for (size_t s = 1; s < k.spine.size(); ++s) {
            const float dx = k.spine[s - 1].x - k.spine[s].x, dy = k.spine[s - 1].y - k.spine[s].y;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len > gap) {
                k.spine[s].x = k.spine[s - 1].x - dx / len * gap;
                k.spine[s].y = k.spine[s - 1].y - dy / len * gap;
            }
        }
        // Now and then a koi noses the surface.
        if (rnd() < dt * 0.025f && ripples_.size() < 40)
            ripples_.push_back({k.spine[0].x, k.spine[0].y, 0.5f, 1.0f, 0.8f});
    }
}

void KoiPondScene::draw_water(float dt)
{
    const auto& L = env();
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f;
    const auto wet = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < cv_.w && y < cv_.h && depth_[static_cast<size_t>(y * cv_.w + x)] > 0.0f;
    };
    const Rgb sheen = mix(from_env(L.sky(0.2f)), {0.85f, 0.92f, 1.0f}, 0.4f);
    const float calm = 1.0f - 0.5f * L.rain;
    // Slow light ripples drifting across the surface.
    for (uint32_t i = 0; i < 46; ++i) {
        const float x = CX - 44 + 88 * hash01(i * 5u) + 3.0f * std::sin(t_ * 0.3f + i), y = CY - 40 + 80 * hash01(i * 5u + 1u);
        const float pulse = 0.5f + 0.5f * std::sin(t_ * (0.7f + 0.4f * hash01(i * 5u + 2u)) + i * 2.1f);
        const int row = static_cast<int>(y * sy), len = static_cast<int>((2 + 5 * hash01(i * 5u + 3u)) * sx);
        for (int c = 0; c < len; ++c)
            if (wet(static_cast<int>(x * sx) + c, row))
                cv_.add(static_cast<int>(x * sx) + c, row, sheen, (0.04f + 0.05f * L.daylight) * pulse * calm);
    }
    // Sun glitter by day, a soft moon reflection at night.
    const float sun = std::clamp(L.sun_elevation / 20.0f, 0.0f, 1.0f) * (1.0f - 0.85f * L.overcast);
    if (sun > 0.02f) {
        // A few glints where the sun catches the ripples: each one twinkles
        // slowly on and off at its own spot instead of random sparkle.
        const Point glint{CX + 18.0f - 30.0f * L.sun_x, CY - 12.0f};
        for (uint32_t i = 0; i < 9; ++i) {
            const float gx = glint.x + 20.0f * (hash01(i * 3u) - 0.5f) + 1.5f * std::sin(t_ * 0.4f + i);
            const float gy = glint.y + 12.0f * (hash01(i * 3u + 1u) - 0.5f);
            const float on = std::max(0.0f, std::sin(t_ * (0.9f + 0.6f * hash01(i * 3u + 2u)) + i * 2.4f));
            const int x = static_cast<int>(gx * sx), y = static_cast<int>(gy * sy);
            if (!wet(x, y) || on < 0.05f)
                continue;
            const Rgb c = mix(from_env(L.sun_color), {1, 1, 1}, 0.5f);
            cv_.add(x, y, c, 0.6f * sun * on);
            for (const auto& [ox, oy] : {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}})
                if (wet(x + ox, y + oy))
                    cv_.add(x + ox, y + oy, c, 0.2f * sun * on * on);
        }
    }
    if (L.night > 0.3f && L.moon_light > 0.05f)
        for (int ring = 0; ring < 3; ++ring) {
            const float r = 2.5f + ring * 1.6f + 0.4f * std::sin(t_ * 0.8f + ring);
            cv_.ellipse((CX + 14) * sx, (CY - 16) * sy, r * sx, r * sy, 0.0f, {0.8f, 0.86f, 1.0f}, 0.10f * L.moon_light * (3 - ring));
        }

    // Rain rings the surface; koi make the odd ring of their own.
    if (rnd() < dt * 28.0f * L.rain && ripples_.size() < 60) {
        const float a = rnd() * kTau, r = Radius * std::sqrt(rnd());
        ripples_.push_back({CX + std::cos(a) * r, CY + std::sin(a) * r, 0.3f, 1.0f, 1.0f});
    }
    for (auto& r : ripples_) {
        r.r += dt * 4.5f;
        r.life -= dt * 0.7f;
    }
    std::erase_if(ripples_, [](const Ripple& r) { return r.life <= 0.0f; });
    const Rgb ring = mix(sheen, {1, 1, 1}, 0.3f);
    for (const auto& r : ripples_) {
        const int steps = std::max(8, static_cast<int>(r.r * 4.0f));
        for (int i = 0; i < steps; ++i) {
            const float a = kTau * i / steps;
            const int x = static_cast<int>((r.x + std::cos(a) * r.r) * sx), y = static_cast<int>((r.y + std::sin(a) * r.r) * sy);
            if (wet(x, y))
                cv_.add(x, y, ring, 0.22f * r.life * r.strength);
        }
    }
}

void KoiPondScene::draw_koi()
{
    const auto& L = env();
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f, k = std::min(sx, sy);
    const Rgb water = lit({0.05f, 0.14f, 0.17f});
    const float chill = std::clamp((6.0f - L.temperature) / 10.0f, 0.0f, 1.0f);  // koi sink deeper in the cold
    const auto depthAt = [&](Point p) {
        const int x = std::clamp(static_cast<int>(p.x * sx), 0, cv_.w - 1), y = std::clamp(static_cast<int>(p.y * sy), 0, cv_.h - 1);
        return depth_[static_cast<size_t>(y * cv_.w + x)];
    };
    // Shadows on the pond floor first, offset away from the light.
    const Point off{1.6f + 1.2f * L.sun_x, 2.2f};
    for (const auto& f : koi_)
        for (size_t s = 1; s < f.spine.size(); ++s)
            cv_.capsule((f.spine[s - 1].x + off.x) * sx, (f.spine[s - 1].y + off.y) * sy, BodyRadius[s - 1] * f.size * k,
                        (f.spine[s].x + off.x) * sx, (f.spine[s].y + off.y) * sy, BodyRadius[s] * f.size * k, {0.0f, 0.03f, 0.04f},
                        0.22f * (0.4f + 0.6f * L.daylight));

    for (size_t n = 0; n < koi_.size(); ++n) {
        const auto& f = koi_[n];
        const float under = std::clamp(0.20f + 0.25f * depthAt(f.spine[2]) + 0.3f * chill, 0.0f, 0.75f);
        const auto tone = [&](Rgb c) { return mix(subject(c), water, under); };
        const auto& sp = f.spine;
        // Tail fins fan out behind the last spine point and flick as it swims.
        const float back = std::atan2(sp[8].y - sp[7].y, sp[8].x - sp[7].x), flick = 0.25f * std::sin(t_ * 4.0f + n);
        const Rgb fin = tone(mix(koi_color(f.pattern, static_cast<uint32_t>(n), 8), {1, 1, 1}, 0.2f));
        for (float side : {-0.55f, 0.55f}) {
            const float a = back + side + flick;
            cv_.capsule(sp[8].x * sx, sp[8].y * sy, 0.5f * k, (sp[8].x + std::cos(a) * 3.2f * f.size) * sx,
                        (sp[8].y + std::sin(a) * 3.2f * f.size) * sy, 0.9f * f.size * k, fin, 0.75f);
        }
        // Pectoral fins.
        const float body = std::atan2(sp[1].y - sp[3].y, sp[1].x - sp[3].x);
        for (float side : {-1.0f, 1.0f}) {
            const float a = body + side * (1.9f + 0.25f * std::sin(t_ * 3.0f + n));
            cv_.ellipse((sp[2].x + std::cos(a) * 2.4f * f.size) * sx, (sp[2].y + std::sin(a) * 2.4f * f.size) * sy, 1.4f * f.size * k,
                        0.7f * f.size * k, a, fin, 0.6f);
        }
        for (size_t s = sp.size() - 1; s > 0; --s)
            cv_.capsule(sp[s].x * sx, sp[s].y * sy, BodyRadius[s] * f.size * k, sp[s - 1].x * sx, sp[s - 1].y * sy,
                        BodyRadius[s - 1] * f.size * k, tone(koi_color(f.pattern, static_cast<uint32_t>(n), static_cast<int>(s - 1))));
        // A soft highlight along the back.
        cv_.capsule(sp[1].x * sx, sp[1].y * sy, 0.5f * k, sp[4].x * sx, sp[4].y * sy, 0.4f * k, {1, 1, 1}, 0.18f * (1.0f - under));
    }
}

void KoiPondScene::draw_pads()
{
    const auto& L = env();
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f, k = std::min(sx, sy);
    const float present = std::clamp(1.0f - L.winter * 1.3f, 0.0f, 1.0f);
    if (present <= 0.02f)
        return;
    const Rgb padA = plant({0.20f, 0.44f, 0.16f}, 0.0f, 0.0f, 0.6f), padB = plant({0.30f, 0.54f, 0.20f}, 0.0f, 0.0f, 0.6f);
    const float bloom = std::clamp((L.summer - 0.3f) * 2.0f, 0.0f, 1.0f);
    for (auto& p : pads_) {
        const float x0 = (p.x - p.r - 1) * sx, x1 = (p.x + p.r + 1) * sx, y0 = (p.y - p.r - 1) * sy, y1 = (p.y + p.r + 1) * sy;
        for (int y = std::max(0, static_cast<int>(y0)); y <= std::min(cv_.h - 1, static_cast<int>(y1)); ++y)
            for (int x = std::max(0, static_cast<int>(x0)); x <= std::min(cv_.w - 1, static_cast<int>(x1)); ++x) {
                const float dx = (x + 0.5f) / sx - p.x, dy = (y + 0.5f) / sy - p.y, d = std::sqrt(dx * dx + dy * dy);
                if (d > p.r + 0.5f)
                    continue;
                const float a = std::remainder(std::atan2(dy, dx) - p.angle, kTau);
                if (std::fabs(a) < 0.32f && d > 0.12f * p.r)
                    continue;  // the notch
                const float vein = std::fabs(std::remainder(a * 7.0f / kTau * kTau, kTau / 7.0f)) < 0.12f ? 0.85f : 1.0f;
                const Rgb c = scale(mix(padA, padB, 0.5f + 0.5f * std::sin(a * 2.0f) * (d / p.r)), vein * (d > p.r - 0.8f ? 1.15f : 1.0f));
                cv_.blend(x, y, c, present * std::clamp(p.r + 0.5f - d, 0.0f, 1.0f));
            }
        if (p.flower && bloom > 0.02f) {
            const float fx = p.x + 0.45f * p.r * std::cos(p.angle + kPi), fy = p.y + 0.45f * p.r * std::sin(p.angle + kPi);
            for (int i = 0; i < 6; ++i) {
                const float a = kTau * i / 6.0f + p.angle;
                cv_.ellipse((fx + std::cos(a) * 1.6f) * sx, (fy + std::sin(a) * 1.6f) * sy, 1.6f * k, 0.9f * k, a,
                            subject({1.0f, 0.62f, 0.78f}), bloom);
            }
            cv_.ellipse(fx * sx, fy * sy, 1.0f * k, 1.0f * k, 0.0f, subject({1.0f, 0.88f, 0.35f}), bloom);
        }
    }
}

void KoiPondScene::draw_floaters(float dt)
{
    const auto& L = env();
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f, k = std::min(sx, sy);
    // Blossom in spring and leaves in autumn land on the water and drift.
    // The first frame starts with some already floating.
    const float rate = 0.7f * L.spring + 0.8f * L.autumn;
    if (!seeded_) {
        seeded_ = true;
        floatClock_ = 18.0f * (L.spring + L.autumn);
    }
    while (floatClock_ >= 1.0f && floaters_.size() < 40) {
        floatClock_ -= 1.0f;
        const bool leaf = rnd() * (L.spring + L.autumn) < L.autumn;
        // Most fall from the maple, the rest blow in from anywhere.
        const float a = rnd() * kTau, r = Radius * 0.9f * std::sqrt(rnd());
        Point p = rnd() < 0.6f ? Point{24.0f + 20.0f * rnd(), 22.0f + 20.0f * rnd()} : Point{CX + std::cos(a) * r, CY + std::sin(a) * r};
        if (inside(p.x, p.y) > 2.0f) {
            const std::array<Rgb, 4> leaves{{{0.62f, 0.36f, 0.14f}, {0.78f, 0.62f, 0.22f}, {0.55f, 0.22f, 0.12f}, {0.50f, 0.40f, 0.18f}}};
            const Rgb petal = rnd() < 0.5f ? Rgb{1.0f, 0.80f, 0.86f} : Rgb{1.0f, 0.93f, 0.95f};
            floaters_.push_back({p.x, p.y, 0.0f, 0.0f, rnd() * kTau, 60.0f + 40.0f * rnd(),
                                 leaf ? leaves[static_cast<size_t>(rnd() * 3.99f)] : petal, leaf});
            if (seeded_ && floatClock_ < 2.0f)
                ripples_.push_back({p.x, p.y, 0.3f, 0.6f, 0.6f});
        }
    }
    floatClock_ += dt * rate;
    for (auto& f : floaters_) {
        // A slow swirl around the pond plus the wind.
        const float dx = f.x - CX, dy = f.y - CY;
        f.vx += (-dy * 0.02f + L.wind * 0.8f - f.vx) * dt * 0.5f;
        f.vy += (dx * 0.02f - f.vy) * dt * 0.5f;
        f.x += f.vx * dt;
        f.y += f.vy * dt;
        f.angle += dt * 0.1f;
        f.life -= dt;
        if (inside(f.x, f.y) < 1.5f) {
            f.vx -= dx * 0.02f;
            f.vy -= dy * 0.02f;
        }
    }
    std::erase_if(floaters_, [](const Floater& f) { return f.life <= 0.0f; });
    for (const auto& f : floaters_) {
        const float a = std::min(1.0f, f.life / 4.0f);
        if (f.leaf) {
            cv_.ellipse(f.x * sx, f.y * sy, 2.0f * k, 1.2f * k, f.angle, subject(f.color), a);
            cv_.capsule(f.x * sx, f.y * sy, 0.2f * k, (f.x + std::cos(f.angle) * 2.4f) * sx, (f.y + std::sin(f.angle) * 2.4f) * sy,
                        0.2f * k, subject(scale(f.color, 0.6f)), a);
        }
        else {
            cv_.ellipse(f.x * sx, f.y * sy, 1.1f * k, 0.75f * k, f.angle, subject(f.color), a);
        }
    }
}

void KoiPondScene::draw_dragonfly(float dt)
{
    const auto& L = env();
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f, k = std::min(sx, sy);
    const bool season = L.summer > 0.5f && L.daylight > 0.5f && L.rain < 0.1f;
    if (fly_.life <= 0.0f) {
        fly_.wait -= dt;
        if (fly_.wait <= 0.0f && season) {
            fly_ = {rnd() < 0.5f ? -4.0f : 132.0f, 20.0f + 80.0f * rnd(), CX, CY, 0.0f, 18.0f + 10.0f * rnd()};
            fly_.tx = CX + (rnd() - 0.5f) * 60.0f;
            fly_.ty = CY + (rnd() - 0.5f) * 60.0f;
        }
        return;
    }
    fly_.life -= dt;
    // Darts to a spot, hovers, darts on; leaves the frame at the end.
    const float dx = fly_.tx - fly_.x, dy = fly_.ty - fly_.y, d = std::sqrt(dx * dx + dy * dy);
    if (d > 0.5f) {
        const float step = std::min(d, 55.0f * dt);
        fly_.x += dx / d * step;
        fly_.y += dy / d * step;
    }
    else if ((fly_.wait -= dt) <= 0.0f) {
        fly_.wait = 1.0f + 2.5f * rnd();
        if (fly_.life < 2.0f) {
            fly_.tx = fly_.x < CX ? -10.0f : 138.0f;
        }
        else {
            fly_.tx = std::clamp(fly_.x + (rnd() - 0.5f) * 50.0f, 15.0f, 113.0f);
            fly_.ty = std::clamp(fly_.y + (rnd() - 0.5f) * 50.0f, 15.0f, 113.0f);
        }
    }
    if (fly_.life <= 0.0f)
        fly_.wait = 20.0f + 30.0f * rnd();
    const float a = d > 0.5f ? std::atan2(dy, dx) : 0.3f * std::sin(t_);
    const float ca = std::cos(a), sa = std::sin(a);
    const float flutter = 0.4f + 0.6f * std::fabs(std::sin(t_ * 40.0f));
    for (float side : {-1.0f, 1.0f})
        for (float along : {0.6f, -0.4f}) {
            const float wx = fly_.x + ca * along - sa * side * 2.0f, wy = fly_.y + sa * along + ca * side * 2.0f;
            cv_.ellipse(wx * sx, wy * sy, 1.9f * k, 0.6f * k, a + side * 1.45f, {0.85f, 0.92f, 1.0f}, 0.35f * flutter);
        }
    cv_.capsule((fly_.x + ca * 1.5f) * sx, (fly_.y + sa * 1.5f) * sy, 0.7f * k, (fly_.x - ca * 3.5f) * sx, (fly_.y - sa * 3.5f) * sy,
                0.35f * k, subject({0.15f, 0.55f, 0.65f}));
}

bool KoiPondScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt = 0.0f;
    begin_frame(dt);
    const float raw = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.1f);
    const auto& L = env();
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f, k = std::min(sx, sy);
    if (environment_changed(backdropKey_) || backdrop_.empty())
        build_backdrop();
    cv_.px = backdrop_;

    update_koi(dt);
    draw_water(dt);
    draw_koi();
    draw_pads();
    draw_floaters(dt);

    // Ice over the rim covers the fish and pads beneath it.
    const Rgb ice = lit({0.80f, 0.88f, 0.95f}), iceDark = lit({0.55f, 0.66f, 0.74f});
    for (size_t i = 0; i < ice_.size(); ++i)
        if (ice_[i] > 0.0f) {
            const int x = static_cast<int>(i % static_cast<size_t>(cv_.w)), y = static_cast<int>(i / static_cast<size_t>(cv_.w));
            cv_.blend(x, y, hash01(static_cast<uint32_t>(i) * 747796405u) > 0.93f ? iceDark : ice, 0.85f * ice_[i]);
        }

    // The maple crown hangs over everything, swaying a touch in the wind.
    const int shift = static_cast<int>(std::lround((0.6f + std::fabs(L.wind)) * std::sin(t_ * 0.5f) * sx));
    for (int y = 0; y < cv_.h; ++y)
        for (int x = 0; x < cv_.w; ++x) {
            const int src = x - shift;
            if (src < 0 || src >= cv_.w)
                continue;
            const float* c = &canopy_[static_cast<size_t>(y * cv_.w + src) * 4];
            if (c[3] > 0.0f)
                cv_.blend(x, y, {c[0], c[1], c[2]}, c[3]);
        }

    // The lantern glows after dusk and its light falls on the water.
    if (L.lamp() > 0.02f) {
        const float flicker = 0.9f + 0.1f * std::sin(t_ * 7.0f) * std::sin(t_ * 3.1f);
        cv_.glow(Lantern.x * sx, Lantern.y * sy, 16.0f * k, {1.0f, 0.62f, 0.28f}, 0.32f * L.lamp() * flicker);
        cv_.ellipse(Lantern.x * sx, Lantern.y * sy, 1.5f * k, 1.5f * k, 0.0f, {1.0f, 0.82f, 0.5f}, L.lamp());
    }
    // Fireflies over the water on summer nights.
    if (L.summer > 0.4f && L.night > 0.3f)
        for (uint32_t i = 0; i < 9; ++i) {
            const float x = CX + 50.0f * std::sin(t_ * 0.07f * (1 + hash01(i)) + i * 2.0f),
                        y = CY + 46.0f * std::sin(t_ * 0.05f * (1 + hash01(i + 9u)) + i * 3.7f);
            const float pulse = std::max(0.0f, std::sin(t_ * (0.8f + 0.5f * hash01(i + 3u)) + i * 1.3f));
            cv_.glow(x * sx, y * sy, 3.0f * k, {0.85f, 1.0f, 0.4f}, 0.5f * pulse * L.summer * L.night);
        }
    draw_dragonfly(dt);

    if (L.snow > 0.02f)
        paint_weather(raw, 0.0f);
    if (L.flash > 0.01f)
        for (int y = 0; y < cv_.h; ++y)
            for (int x = 0; x < cv_.w; ++x) cv_.add(x, y, {0.8f, 0.85f, 1.0f}, 0.35f * L.flash);
    present(canvas);
    wait_until_next_frame();
    return true;
}

Scenes::SceneDescriptor KoiPondScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "scenery";
    d.tags = {"scenery",  "garden",    "water",       "animals",     "calm",        "nature",     "seasons",
              "fits:day", "fits:dawn", "fits:spring", "fits:summer", "fits:autumn", "fits:clear", "fits:rain"};
    d.intensity = 0.12f;
    d.motion = 0.2f;
    d.music_affinity = 0.05f;
    d.performance_cost = 0.3f;
    environment_->add_variants(d);
    return d;
}

}  // namespace Hobbies
