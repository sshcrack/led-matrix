#include "HarbourScene.h"

namespace Hobbies {
namespace {

// Layout in a 128×128 design space, scaled to the canvas.
constexpr float Horizon = 72.0f;
constexpr float QuayTop = 86.0f, QuayWater = 95.0f, QuayEnd = 58.0f;
constexpr float PierTop = 83.0f, PierWater = 88.0f, PierStart = 80.0f;
constexpr float LighthouseX = 108.0f, LampY = 43.0f;
constexpr float Cycle = 120.0f;  // arrive, moor, depart, away

float water_line(float x)
{
    if (x < QuayEnd)
        return QuayWater;
    if (x >= PierStart)
        return PierWater;
    return Horizon;
}

float ease(float u)
{
    u = std::clamp(u, 0.0f, 1.0f);
    return u * u * (3.0f - 2.0f * u);
}

}  // namespace

HarbourScene::HarbourScene()
{
    use_environment({Environment::TimeOfDay::Dawn, Environment::Weather::Clear, Environment::Season::Summer, 4.0f, 0.22f});
}

void HarbourScene::initialize(int width, int height)
{
    HobbySceneBase::initialize(width, height);
    backdrop_.clear();
    ripples_.clear();
    gulls_.clear();
    for (int i = 0; i < 3; ++i)
        gulls_.push_back(
            {20.0f + 40.0f * i, 20.0f + 14.0f * i + 8.0f * rnd(), (i % 2 ? -1.0f : 1.0f) * (6.0f + 4.0f * rnd()), rnd() * kTau, 0.0f});
    // Start shortly before a departure so the first minute shows the boat leave.
    voyage_ = 62.0f;
    beam_ = 0.0f;
}

void HarbourScene::build_backdrop()
{
    const auto& L = env();
    Canvas bg;
    bg.resize(cv_.w, cv_.h);
    const float sx = bg.w / 128.0f, sy = bg.h / 128.0f, k = std::min(sx, sy), W = static_cast<float>(bg.w);
    const Environment::Surface s{bg.px.data(), bg.w, bg.h, 1.0f};
    const auto rect = [&](float x, float y, float w, float h, Rgb c) {
        const int x0 = std::max(0, static_cast<int>(std::lround(x * sx))), x1 = std::min(bg.w, static_cast<int>(std::lround((x + w) * sx)));
        const int y0 = std::max(0, static_cast<int>(std::lround(y * sy))), y1 = std::min(bg.h, static_cast<int>(std::lround((y + h) * sy)));
        for (int row = y0; row < y1; ++row)
            for (int col = x0; col < x1; ++col) bg.set(col, row, c);
    };

    // Sky, stars, sun and moon over the open sea.
    Environment::paint_sky(s, L, Horizon * sy);
    Environment::paint_stars(s, L, 60 * sy, 2.1f, 17, 70);
    Environment::paint_sun(s, L, Environment::sun_point(L, W, Horizon * sy), 6.0f * k);
    Environment::paint_moon(s, L, Environment::moon_point(L, W, Horizon * sy), 4.5f * k);
    const std::vector<float> skyStage = bg.px;

    // A low headland across the bay, pale in the haze.
    bg.fill_below(
        [&](float x) { return sy * (Horizon - 1.0f - 6.0f * std::max(0.0f, hill(x / sx, 0.8f) - 0.25f) * smoothstep(60, 100, x / sx)); },
        plant({0.30f, 0.38f, 0.34f}, 0.9f, 0.6f, 0.3f));

    // The sea: the sky's colour, darker towards the viewer.
    const Rgb seaFar = mix(lit({0.20f, 0.32f, 0.40f}, 0.6f), from_env(L.sky(0.9f)), 0.55f);
    const Rgb seaNear = lit({0.06f, 0.14f, 0.20f}, 0.1f);
    for (int y = static_cast<int>(Horizon); y < 128; ++y)
        rect(0, static_cast<float>(y), 128, 1, mix(seaFar, seaNear, std::pow((y - Horizon) / (128 - Horizon), 0.7f)));
    const std::vector<float> seaStage = bg.px;

    // Quay houses: tall, narrow gabled fronts in harbour colours.
    struct House {
        float x, w, top;
        Rgb wall;
    };
    const std::array<House, 6> houses{{{0, 9, 60, {0.72f, 0.30f, 0.24f}},
                                       {9, 8, 64, {0.88f, 0.80f, 0.62f}},
                                       {17, 9, 58, {0.36f, 0.50f, 0.62f}},
                                       {26, 8, 63, {0.92f, 0.90f, 0.86f}},
                                       {34, 9, 61, {0.44f, 0.58f, 0.42f}},
                                       {43, 8, 65, {0.86f, 0.62f, 0.30f}}}};
    for (size_t i = 0; i < houses.size(); ++i) {
        const auto& h = houses[i];
        rect(h.x, h.top, h.w, QuayTop - h.top, lit(h.wall, 0.1f));
        rect(h.x + h.w - 1, h.top, 1, QuayTop - h.top, lit(scale(h.wall, 0.75f), 0.1f));  // shaded side
        const Rgb roof = terrain(i % 2 ? Rgb{0.30f, 0.26f, 0.26f} : Rgb{0.52f, 0.22f, 0.16f}, 0.1f, 1.0f);
        // Snow settles on the roof faces; the eave and gable edges stay dark so
        // the gable keeps its shape against a pale sky.
        const Rgb edge = lit(scale(i % 2 ? Rgb{0.30f, 0.26f, 0.26f} : Rgb{0.52f, 0.22f, 0.16f}, 0.6f), 0.1f);
        for (int r = 0; r < static_cast<int>(h.w / 2) + 1; ++r) {
            const float w = h.w - 2 * r;
            rect(h.x + r, h.top - 1 - r, w, 1, r == 0 ? edge : roof);
            rect(h.x + r, h.top - 1 - r, 1, 1, edge);
            rect(h.x + r + w - 1, h.top - 1 - r, 1, 1, edge);
        }
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 2; ++col) {
                const float wx = h.x + 1.5f + col * (h.w - 4.0f), wy = h.top + 3.0f + row * 6.0f;
                if (wy + 3 < QuayTop - 4)
                    Environment::paint_window(s, L, static_cast<int>(wx * sx), static_cast<int>(wy * sy),
                                              std::max(1, static_cast<int>(2 * sx)), std::max(1, static_cast<int>(3 * sy)),
                                              static_cast<uint32_t>(i * 7 + row * 2 + col));
            }
        // Window boxes: tulips in spring, red geraniums in summer.
        const float flowers = std::clamp(L.spring + L.summer - L.snow_cover, 0.0f, 1.0f);
        if (flowers > 0.05f)
            for (int col = 0; col < 2; ++col) {
                int row = 2;
                while (row > 0 && h.top + 3.0f + row * 6.0f + 3 >= QuayTop - 4) --row;
                const float wx = h.x + 1.5f + col * (h.w - 4.0f), wy = h.top + 3.0f + row * 6.0f + 3.0f;
                const Rgb bloom = mix(i % 2 ? Rgb{1.0f, 0.85f, 0.25f} : Rgb{1.0f, 0.45f, 0.65f}, Rgb{0.90f, 0.12f, 0.10f},
                                      L.summer / std::max(0.01f, L.spring + L.summer));
                rect(wx - 0.5f, wy, 3, 1, mix(lit(h.wall, 0.1f), lit(bloom, 0.1f), flowers));
            }
        rect(h.x + h.w / 2 - 1, QuayTop - 4, 2, 4, lit({0.16f, 0.12f, 0.10f}, 0.1f));  // door
    }
    // The quay: cobbles on top, a stone wall down to the water, lamps and bollards.
    rect(51, 76, 7, 10, lit({0.40f, 0.38f, 0.36f}, 0.1f));  // harbour office
    rect(51, 75, 7, 1, terrain({0.30f, 0.30f, 0.32f}, 0.1f, 1.0f));
    Environment::paint_window(s, L, static_cast<int>(53 * sx), static_cast<int>(78 * sy), std::max(1, static_cast<int>(3 * sx)),
                              std::max(1, static_cast<int>(2 * sy)), 41);
    rect(0, QuayTop, QuayEnd, 2, terrain({0.50f, 0.47f, 0.43f}, 0.05f, 1.0f));
    for (int y = static_cast<int>(QuayTop) + 2; y < static_cast<int>(QuayWater); ++y)
        for (int x = 0; x < static_cast<int>(QuayEnd); ++x) {
            const int course = (y - static_cast<int>(QuayTop)) / 2;
            const bool joint = (y - static_cast<int>(QuayTop)) % 2 == 1 || (x + course * 3) % 6 == 0;
            rect(static_cast<float>(x), static_cast<float>(y), 1, 1,
                 lit(joint ? Rgb{0.24f, 0.23f, 0.22f} : Rgb{0.36f, 0.34f, 0.32f}, 0.05f));
        }
    rect(0, QuayWater - 2, QuayEnd, 2, lit({0.16f, 0.20f, 0.16f}, 0.05f));  // weed at the waterline
    for (float x : {14.0f, 32.0f}) rect(x, 78, 1, 8, lit({0.14f, 0.14f, 0.16f}, 0.05f));
    for (float x : {22.0f, 40.0f, 56.0f}) rect(x, QuayTop - 1, 2, 1, lit({0.12f, 0.12f, 0.13f}, 0.05f));

    // The pier with its lighthouse, red and white bands under a lantern room.
    rect(PierStart, PierTop, 128 - PierStart, 2, terrain({0.56f, 0.53f, 0.48f}, 0.4f, 1.0f));
    rect(PierStart, PierTop + 2, 128 - PierStart, PierWater - PierTop - 2, lit({0.34f, 0.33f, 0.31f}, 0.4f));
    for (float x = PierStart + 2; x < 128; x += 6) rect(x, PierTop + 2, 1, PierWater - PierTop - 2, lit({0.24f, 0.23f, 0.22f}, 0.4f));
    for (int y = static_cast<int>(LampY) + 5; y < static_cast<int>(PierTop); ++y) {
        const float half = 2.0f + 1.6f * (y - LampY - 5) / (PierTop - LampY - 5);
        const bool red = ((y - static_cast<int>(LampY)) / 6) % 2 == 0;
        const Rgb band = lit(red ? Rgb{0.78f, 0.16f, 0.14f} : Rgb{0.92f, 0.90f, 0.86f}, 0.15f);
        rect(LighthouseX - half, static_cast<float>(y), half, 1, band);
        rect(LighthouseX, static_cast<float>(y), half, 1, scale(band, 0.8f));
    }
    rect(LighthouseX - 4, LampY + 4, 8, 1, lit({0.18f, 0.18f, 0.20f}, 0.15f));  // gallery
    rect(LighthouseX - 2, LampY - 1, 4, 5, lit({0.30f, 0.34f, 0.36f}, 0.15f));  // lantern room
    rect(LighthouseX - 2.5f, LampY - 3, 5, 2, lit({0.20f, 0.22f, 0.24f}, 0.15f));
    rect(LighthouseX - 0.5f, LampY - 5, 1, 2, lit({0.20f, 0.22f, 0.24f}, 0.15f));

    // Surfaces carry the weather: wet stone shines with the sky in rain,
    // frost rims the quay and pier edges in winter, and petals or leaves lie
    // under the tree on the pier.
    {
        const float wetness = std::clamp(L.rain, 0.0f, 1.0f), frost = std::clamp(L.winter - 0.5f * L.snow_cover, 0.0f, 1.0f);
        const Rgb sheen = from_env(L.sky(0.4f)), rime = lit({0.86f, 0.90f, 0.98f}, 0.1f);
        const auto tint = [&](float x0, float x1, float y0, float y1, Rgb c, float a) {
            for (int y = static_cast<int>(y0 * sy); y < static_cast<int>(y1 * sy); ++y)
                for (int x = static_cast<int>(x0 * sx); x < static_cast<int>(x1 * sx); ++x) bg.blend(x, y, c, a);
        };
        if (wetness > 0.02f) {
            tint(0, QuayEnd, QuayTop, QuayWater, {0.0f, 0.0f, 0.0f}, 0.18f * wetness);  // darker wet stone
            tint(PierStart, 128, PierTop, PierWater, {0.0f, 0.0f, 0.0f}, 0.18f * wetness);
            for (float x = 0; x < QuayEnd; x += 1.0f)
                tint(x, x + 1, QuayTop, QuayTop + 2, sheen, (0.40f + 0.3f * hash01(static_cast<uint32_t>(x) * 13u)) * wetness);
            for (float x = PierStart; x < 128; x += 1.0f)
                tint(x, x + 1, PierTop, PierTop + 2, sheen, (0.40f + 0.3f * hash01(static_cast<uint32_t>(x) * 17u)) * wetness);
        }
        if (frost > 0.02f) {
            tint(0, QuayEnd, QuayTop, QuayTop + 1, rime, 0.65f * frost);
            tint(PierStart, 128, PierTop, PierTop + 1, rime, 0.65f * frost);
            tint(0, QuayEnd, QuayWater - 1, QuayWater, rime, 0.35f * frost);
        }
        // Climbing ivy on the white house: green through summer, red in autumn,
        // bare stems in winter.
        {
            const Rgb ivy = mix(plant({0.22f, 0.42f, 0.16f}), lit({0.72f, 0.16f, 0.10f}, 0.1f), std::clamp(L.autumn * 1.3f, 0.0f, 1.0f));
            const float leaves = 1.0f - std::clamp(L.winter * 1.3f, 0.0f, 1.0f);
            for (int y = 70; y < static_cast<int>(QuayTop); ++y)
                for (int x = 26; x < 34; ++x) {
                    const float reach = 1.0f - (y - 70) / 16.0f;  // thins out upwards
                    const float h = hash01(static_cast<uint32_t>(x * 53 + y * 977));
                    if (h < 0.15f + 0.55f * reach + 0.3f * std::sin(x * 1.3f))
                        tint(static_cast<float>(x), x + 1.0f, static_cast<float>(y), y + 1.0f,
                             leaves > 0.1f ? ivy : lit({0.30f, 0.24f, 0.20f}, 0.1f), leaves > 0.1f ? 0.95f * leaves : 0.5f);
                }
        }
        const float litter = std::clamp(std::max(L.spring, L.autumn) - L.snow_cover, 0.0f, 1.0f);
        // Pots of blossom (spring) or drifts of leaves (autumn) along the quay.
        if (litter > 0.05f)
            for (float cx : {6.0f, 21.0f, 37.0f, 49.0f})
                for (uint32_t i = 0; i < 7; ++i) {
                    const float x = cx + 4.0f * (hash01(static_cast<uint32_t>(cx) * 7u + i) - 0.5f),
                                y = QuayTop - (i % 3 == 0 ? 1.0f : 0.0f);
                    const Rgb c = L.autumn > L.spring ? (i % 2 ? Rgb{0.88f, 0.45f, 0.12f} : Rgb{0.72f, 0.24f, 0.10f})
                                                      : (i % 2 ? Rgb{1.0f, 0.62f, 0.78f} : Rgb{1.0f, 0.86f, 0.90f});
                    tint(x, x + 1, y, y + 1, lit(c, 0.1f), 0.95f * litter);
                }
        if (litter > 0.05f)
            for (uint32_t i = 0; i < 14; ++i) {
                const float x = 80.0f + 15.0f * hash01(i * 3u + 1u), y = PierTop + (hash01(i * 3u + 2u) > 0.5f ? 1.0f : 0.0f);
                const Rgb c = L.autumn > L.spring ? (i % 3 ? Rgb{0.85f, 0.42f, 0.12f} : Rgb{0.70f, 0.22f, 0.10f})
                                                  : (i % 3 ? Rgb{1.0f, 0.68f, 0.80f} : Rgb{1.0f, 0.86f, 0.90f});
                tint(x, x + 1, y, y + 1, lit(c, 0.15f), 0.9f * litter);
            }
    }

    // Masks: open sky, open water and the pier.
    const auto same = [&](const std::vector<float>& stage, size_t i) {
        return bg.px[i * 3] == stage[i * 3] && bg.px[i * 3 + 1] == stage[i * 3 + 1] && bg.px[i * 3 + 2] == stage[i * 3 + 2];
    };
    const size_t n = static_cast<size_t>(bg.w * bg.h);
    sky_.assign(n, 0);
    water_.assign(n, 0);
    pier_.assign(n, 0);
    for (int y = 0; y < bg.h; ++y)
        for (int x = 0; x < bg.w; ++x) {
            const size_t i = static_cast<size_t>(y * bg.w + x);
            const float dy = (y + 0.5f) / sy;  // design units
            if (dy < Horizon)
                sky_[i] = same(skyStage, i);
            else
                water_[i] = same(seaStage, i);
        }
    // Out in the bay, a boat passes behind everything that is not open sky or
    // water: the pier, the lighthouse and the headland.
    for (size_t i = 0; i < n; ++i) pier_[i] = !sky_[i] && !water_[i];

    // Still reflections: the houses and lighthouse mirrored in the water,
    // dimmed and broken into rows by small ripples.
    const std::vector<float> before = bg.px;
    for (int y = 0; y < bg.h; ++y)
        for (int x = 0; x < bg.w; ++x) {
            const size_t i = static_cast<size_t>(y * bg.w + x);
            if (!water_[i])
                continue;
            const float line = water_line((x + 0.5f) / sx) * sy;
            const int shift = static_cast<int>(std::lround((hash01(static_cast<uint32_t>(y) * 131u) - 0.5f) * 2.0f * sx));
            const int srcY = static_cast<int>(2.0f * line - y), srcX = std::clamp(x + shift, 0, bg.w - 1);
            if (srcY < 0 || srcY >= bg.h || line <= Horizon * sy + 0.5f)
                continue;
            const size_t j = static_cast<size_t>(srcY * bg.w + srcX);
            if (water_[j] || sky_[j])
                continue;
            const float fade = 0.55f * (1.0f - std::clamp((y - line) / (40.0f * sy), 0.0f, 1.0f)) * (1.0f - 0.7f * L.fog);
            for (int c = 0; c < 3; ++c) bg.px[i * 3 + c] = before[i * 3 + c] + (before[j * 3 + c] * 0.6f - before[i * 3 + c]) * fade;
        }
    backdrop_ = std::move(bg.px);
}

void HarbourScene::draw_water(float raw_dt)
{
    const auto& L = env();
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f, W = static_cast<float>(cv_.w);
    const auto wet = [&](int x, int y) { return x >= 0 && y >= 0 && x < cv_.w && y < cv_.h && water_[static_cast<size_t>(y * cv_.w + x)]; };
    const auto streak = [&](float x, float y, float length, Rgb color, float alpha) {
        const int row = static_cast<int>(y * sy);
        for (int col = static_cast<int>(x * sx); col < static_cast<int>((x + length) * sx); ++col)
            if (wet(col, row))
                cv_.add(col, row, color, alpha);
    };
    // A point spread over the two pixels it falls between, for sub-pixel motion.
    const auto soft = [&](float x, float y, Rgb color, float alpha) {
        const float fx = x * sx - 0.5f;
        const int col = static_cast<int>(std::floor(fx)), row = static_cast<int>(y * sy);
        const float u = fx - static_cast<float>(col);
        if (wet(col, row))
            cv_.add(col, row, color, alpha * (1.0f - u));
        if (wet(col + 1, row))
            cv_.add(col + 1, row, color, alpha * u);
    };
    const float rough = 1.0f + 1.5f * L.storm + 0.6f * std::fabs(L.wind);
    // Swell lines catch the sky, longer and slower far out.
    const Rgb sheen = mix(from_env(L.sky(0.6f)), {0.85f, 0.9f, 1.0f}, 0.25f);
    for (uint32_t i = 0; i < 60; ++i) {
        const float d = hash01(i * 4u);  // 0 far .. 1 near
        const float y = Horizon + 1 + (128 - Horizon - 1) * d * d;
        const float x = 128 * hash01(i * 4u + 1u) + (2.0f + 3.0f * d) * std::sin(t_ * (0.3f + 0.3f * d) * rough + i * 1.7f);
        const float pulse = 0.5f + 0.5f * std::sin(t_ * (0.6f + 0.5f * hash01(i * 4u + 2u)) * rough + i * 2.3f);
        streak(x, y, 2 + 8 * d, sheen, (0.05f + 0.05f * L.night) * pulse * rough * (1.0f - 0.5f * L.fog));
    }
    // The sun or moon draws a glittering path across the bay.
    const bool moon = L.night > 0.5f;
    const float lightX = (moon ? Environment::moon_point(L, W, Horizon * sy).x : Environment::sun_point(L, W, Horizon * sy).x) / sx;
    const float pathVis = (moon ? 0.7f * L.moon_light + 0.2f * L.night : std::clamp((L.sun_elevation + 3.0f) / 5.0f, 0.0f, 1.0f)) *
                          (1.0f - 0.9f * L.overcast) * (1.0f - 0.8f * L.fog);
    if (pathVis > 0.02f) {
        const Rgb pathColor = moon ? Rgb{0.78f, 0.85f, 1.0f} : mix(from_env(L.sun_color), {1.0f, 0.9f, 0.7f}, 0.4f);
        // Each glint swells and fades on its own and slides sub-pixel, so the
        // path shimmers without stepping.
        for (int y = static_cast<int>(Horizon) + 1; y < 128; y += 2) {
            const float d = (y - Horizon) / (128 - Horizon);
            const float half = 2 + 10 * d;
            const float wobble = 1.5f * std::sin(y * 0.7f + t_ * 1.3f);
            for (int j = 0; j < static_cast<int>(2 * half); ++j) {
                const float x = lightX - half + j + 0.5f;
                const float fall = 1 - std::fabs(x - lightX) / half;
                const uint32_t id = static_cast<uint32_t>(y * 977 + j * 131);
                const float tw = 0.5f + 0.5f * std::sin(t_ * (1.0f + 1.6f * hash01(id)) + kTau * hash01(id + 7u));
                soft(x + wobble, static_cast<float>(y), pathColor, pathVis * fall * (0.06f + 0.30f * tw * tw * tw));
            }
        }
    }
    // Lit windows and quay lamps leave wavering streaks below the quay.
    const float lamp = L.lamp();
    if (lamp > 0.03f)
        for (float x : {14.5f, 32.5f, 4.0f, 21.0f, 38.0f, 47.0f, 54.5f}) {
            const uint32_t id = static_cast<uint32_t>(x * 10);
            for (int y = static_cast<int>(QuayWater) + 1; y < 124; y += 2) {
                const uint32_t h = id * 131u + static_cast<uint32_t>(y) * 17u;
                const float tw = 0.5f + 0.5f * std::sin(t_ * (1.5f + 2.0f * hash01(h)) + kTau * hash01(h + 3u));
                const float fade = 1 - (y - QuayWater) / 30.0f;
                const float cx = x + 1.5f * std::sin(y * 0.8f + t_ * 1.6f + x);
                for (float o : {0.0f, 1.0f})
                    soft(cx + o, static_cast<float>(y), {1.0f, 0.70f, 0.36f},
                         0.16f * lamp * std::max(0.0f, fade) * tw * (o > 0.0f ? tw : 1.0f));
            }
        }
    // Rain rings the water.
    for (auto& r : ripples_) r.life -= raw_dt * 1.2f;
    std::erase_if(ripples_, [](const Ripple& r) { return r.life <= 0.0f; });
    if (rnd() < raw_dt * 30.0f * L.rain && ripples_.size() < 40)
        ripples_.push_back({128.0f * rnd(), QuayWater + 2 + 30 * rnd(), 1.0f});
    for (const auto& r : ripples_) {
        const float rr = 0.5f + (1.0f - r.life) * 3.0f;
        streak(r.x - rr - 1, r.y, 1.5f, sheen, 0.3f * r.life);
        streak(r.x + rr, r.y, 1.5f, sheen, 0.3f * r.life);
    }
}

Rgb HarbourScene::boat_color(Rgb albedo) const
{
    // Lightning lifts the whole harbour evenly (see render()), so the boats
    // drop the stronger flash Look::lit() gives a foreground subject.
    const Rgb c = subject(albedo);
    const float f = env().flash * 0.6f;
    return {std::max(0.0f, c.r - 0.55f * f), std::max(0.0f, c.g - 0.58f * f), std::max(0.0f, c.b - 0.70f * f)};
}

void HarbourScene::draw_boat(float x, float waterline, float s, float face, float roll, Rgb hull, Rgb trim, bool lights,
                             const std::vector<uint8_t>* mask)
{
    const auto& L = env();
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f;
    // The boat is drawn bow to the right and mirrored (and foreshortened while
    // it swings round) by `face`: 1 heading right, -1 heading left.
    // Rows are spread over the two pixels they fall between, so bobbing glides
    // instead of hopping a whole pixel.
    const auto put = [&](float px, float py, Rgb c, float a = 1.0f) {
        const int ix = static_cast<int>((x + (px - x) * face) * sx);
        const float fy = py * sy - 0.5f;
        const int iy0 = static_cast<int>(std::floor(fy));
        const float f = fy - static_cast<float>(iy0);
        for (int r = 0; r < 2; ++r) {
            const int iy = iy0 + r;
            const float w = r == 0 ? 1.0f - f : f;
            if (w <= 0.0f || ix < 0 || iy < 0 || ix >= cv_.w || iy >= cv_.h || (mask && (*mask)[static_cast<size_t>(iy * cv_.w + ix)]))
                continue;
            cv_.blend(ix, iy, c, a * w);
        }
    };
    const auto box = [&](float x0, float y0, float w, float h, Rgb c, float a = 1.0f) {
        for (float yy = y0; yy < y0 + h; yy += 1.0f / sy)
            for (float xx = x0; xx < x0 + w; xx += 1.0f / sx) put(xx, yy, c, a);
    };
    // Hull: wider at the gunwale, a raised bow on the right.
    const float len = 15.0f * s, depth = std::max(1.0f, 3.5f * s);
    for (float yy = 0; yy < depth; yy += 1.0f / sy) {
        const float inset = (yy / depth) * 2.5f * s;
        box(x - len / 2 + inset + roll * (depth - yy) * 0.3f, waterline - depth + yy, len - 2 * inset, 1.0f / sy,
            yy < 1.0f / sy ? trim : hull);
    }
    box(x + len / 2 - 2.5f * s, waterline - depth - 1.2f * s, 2.5f * s, 1.2f * s, hull);  // bow
    // Wheelhouse, mast with a cross-tree, and a pennant.
    const float top = waterline - depth;
    const Rgb house = boat_color({0.90f, 0.88f, 0.84f});
    box(x - 2.0f * s + roll, top - 5.0f * s, 5.5f * s, 5.0f * s, house);
    box(x - 1.0f * s + roll, top - 4.0f * s, 3.5f * s, std::max(1.0f / sy, 1.4f * s),
        mix(boat_color({0.2f, 0.26f, 0.32f}), {1.0f, 0.8f, 0.45f}, lights ? L.lamp() : 0.0f));
    const float mastX = x - 5.0f * s, mastTop = top - 13.0f * s;
    for (float yy = mastTop; yy < top; yy += 1.0f / sy)
        put(mastX + roll * (top - yy) / (13.0f * s) * 1.5f, yy, boat_color({0.30f, 0.26f, 0.22f}));
    box(mastX - 2.0f * s + roll * 1.2f, mastTop + 3.0f * s, 4.0f * s, 1.0f / sy, boat_color({0.30f, 0.26f, 0.22f}));
    box(mastX + roll * 1.5f, mastTop, 2.5f * s, std::max(1.0f / sy, 1.0f * s), boat_color({0.85f, 0.25f, 0.2f}));
    if (lights && L.lamp() > 0.05f) {
        const int ix = static_cast<int>((x + (mastX + roll * 1.5f - x) * face) * sx), iy = static_cast<int>(mastTop * sy);
        if (!mask || !(*mask)[static_cast<size_t>(std::clamp(iy, 0, cv_.h - 1) * cv_.w + std::clamp(ix, 0, cv_.w - 1))])
            cv_.glow(ix + 0.5f, iy + 0.5f, 3.0f * sx, {1.0f, 0.92f, 0.75f}, 0.5f * L.lamp());
    }
    // A dark reflection of the hull just below the waterline.
    for (float yy = 0; yy < depth * 1.5f; yy += 1.0f / sy) {
        const float wob = 0.8f * std::sin(t_ * 2.0f + yy * 3.0f + x);
        box(x - len / 2 + 1.0f + wob, waterline + yy, len - 2.0f, 1.0f / sy, scale(hull, 0.35f), 0.35f * (1.0f - yy / (depth * 1.5f)));
    }
}

void HarbourScene::draw_moored()
{
    const auto& L = env();
    const float swell = 0.6f + 1.6f * L.storm + 0.5f * std::fabs(L.wind);
    // Two boats stay moored at the quay, rocking out of step.
    for (int i = 0; i < 2; ++i) {
        const float x = i == 0 ? 18.0f : 40.0f, wl = i == 0 ? 108.0f : 104.0f;
        const float bob = 0.6f * swell * std::sin(t_ * (0.9f + 0.2f * i) + i * 2.0f);
        const float roll = 0.8f * swell * std::sin(t_ * (0.7f + 0.15f * i) + i);
        draw_boat(x, wl + bob, i == 0 ? 1.15f : 1.0f, 1.0f, roll, boat_color(i == 0 ? Rgb{0.16f, 0.26f, 0.46f} : Rgb{0.24f, 0.46f, 0.36f}),
                  boat_color({0.90f, 0.88f, 0.84f}), true, nullptr);
    }
}

void HarbourScene::draw_boats(float dt)
{
    const auto& L = env();
    const float swell = 0.6f + 1.6f * L.storm + 0.5f * std::fabs(L.wind);
    // The third boat comes in, moors, then heads out to sea and back again.
    voyage_ = std::fmod(voyage_ + dt, Cycle);
    float u;            // 0 at its mooring .. 1 on the horizon
    float face = 1.0f;  // bow direction: comes in heading left, swings round at the quay, leaves heading right
    if (voyage_ < 28.0f) {
        u = 1.0f - ease(voyage_ / 28.0f);
        face = -1.0f;
    }
    else if (voyage_ < 70.0f) {
        u = 0.0f;
        const float c = -std::cos(kPi * ease((voyage_ - 60.0f) / 10.0f));
        face = (c < 0.0f ? -1.0f : 1.0f) * std::max(0.45f, std::fabs(c));
    }
    else if (voyage_ < 98.0f) {
        u = ease((voyage_ - 70.0f) / 28.0f);
    }
    else {
        return;
    }
    // Out through the harbour mouth first, then along the horizon behind the
    // pier, so the hull never crosses the pier wall.
    const float wl = 101.0f - 27.5f * (1.0f - std::pow(1.0f - u, 3.0f));
    const float x = 66.0f + 48.0f * std::pow(u, 2.5f);
    const float s = 0.95f * (1.0f - 0.78f * (101.0f - wl) / 27.5f);
    const float bob = (u < 0.05f ? 0.6f : 0.3f) * swell * std::sin(t_ * 1.1f + 4.0f) * s;
    const bool moving = (voyage_ < 28.0f) || (voyage_ >= 70.0f && voyage_ < 98.0f);
    // A V-shaped wake spreads behind a boat under way.
    if (moving && u > 0.02f && u < 0.95f) {
        const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f;
        for (int i = 1; i < 10; ++i) {
            const float back = i * 2.2f * s, spread = i * 0.55f * s;
            for (float side : {-1.0f, 1.0f}) {
                const int px = static_cast<int>((x - face * (7.0f * s + back)) * sx),
                          py = static_cast<int>((wl + side * spread * 0.4f + 0.5f) * sy);
                if (px >= 0 && py >= 0 && px < cv_.w && py < cv_.h && water_[static_cast<size_t>(py * cv_.w + px)])
                    cv_.add(px, py, {0.85f, 0.9f, 0.95f}, 0.28f * (s / 0.95f) * (1.0f - i / 10.0f) * (1.0f - L.fog * 0.5f));
            }
        }
    }
    const float fade = 1.0f - smoothstep(0.82f, 1.0f, u);
    if (fade > 0.02f)
        draw_boat(x, wl + bob, s, face, 0.4f * swell * std::sin(t_ * 0.8f), mix(from_env(L.haze), boat_color({0.62f, 0.18f, 0.16f}), fade),
                  mix(from_env(L.haze), boat_color({0.92f, 0.90f, 0.86f}), fade), true, wl < QuayWater - 1.0f ? &pier_ : nullptr);
}

void HarbourScene::draw_gulls(float dt)
{
    const auto& L = env();
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f, k = std::min(sx, sy);
    if (L.storm > 0.5f)
        return;  // gulls shelter in a storm
    const Rgb body = subject({0.92f, 0.92f, 0.90f}), wing = subject({0.70f, 0.72f, 0.75f});
    for (auto& g : gulls_) {
        g.x += (g.vx + L.wind * 3.0f) * dt;
        g.y += 0.8f * std::sin(t_ * 0.3f + g.phase) * dt;
        // Flap for a while, then glide.
        g.glide = std::fmod(g.glide + dt, 7.0f);
        const bool flapping = g.glide < 3.0f;
        g.phase += dt * (flapping ? 7.0f : 0.0f);
        if (g.x > 136.0f)
            g.x = -8.0f;
        if (g.x < -8.0f)
            g.x = 136.0f;
        const float flap = flapping ? 1.6f * std::sin(g.phase) : -0.4f;
        const float dir = g.vx > 0 ? 1.0f : -1.0f;
        cv_.capsule((g.x - 3.2f) * sx, (g.y - flap) * sy, 0.35f * k, g.x * sx, g.y * sy, 0.5f * k, wing);
        cv_.capsule((g.x + 3.2f) * sx, (g.y - flap) * sy, 0.35f * k, g.x * sx, g.y * sy, 0.5f * k, wing);
        cv_.capsule((g.x - 1.2f * dir) * sx, g.y * sy, 0.6f * k, (g.x + 1.4f * dir) * sx, g.y * sy, 0.6f * k, body);
    }
}

void HarbourScene::draw_lighthouse()
{
    const auto& L = env();
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f, k = std::min(sx, sy);
    const float lamp = std::max(L.lamp(), 0.6f * L.fog);
    if (lamp < 0.03f)
        return;
    // The lamp turns: a beam sweeps out to one side, shortening as it turns
    // towards or away from the viewer, and flashes when it faces us.
    const float side = std::cos(beam_), facing = std::sin(beam_);
    const float length = 80.0f * std::fabs(side);
    if (length > 4.0f) {
        const float dir = side > 0 ? 1.0f : -1.0f;
        for (float d = 2.0f; d < length; d += 0.5f) {
            const float half = 0.8f + 0.12f * d, a = 0.30f * lamp * (1.0f - d / length) * std::fabs(side) * (1.0f + 1.0f * L.fog);
            const float cx = LighthouseX + dir * d;
            for (float o = -half; o <= half; o += 0.5f) {
                const int x = static_cast<int>(cx * sx), y = static_cast<int>((LampY + 1.0f + o) * sy);
                if (x >= 0 && y >= 0 && x < cv_.w && y < cv_.h && sky_[static_cast<size_t>(y * cv_.w + x)])
                    cv_.add(x, y, {1.0f, 0.92f, 0.70f}, a * (1.0f - std::fabs(o) / (half + 0.5f)) * 0.5f);
            }
        }
    }
    const float flash = facing > 0.0f ? std::pow(facing, 12.0f) : 0.0f;
    cv_.glow(LighthouseX * sx, (LampY + 1.5f) * sy, (3.0f + 9.0f * flash) * k, {1.0f, 0.90f, 0.62f}, lamp * (0.45f + 0.5f * flash));
}

bool HarbourScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt = 0.0f;
    begin_frame(dt);
    const float raw = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.1f);
    const auto& L = env();
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f, k = std::min(sx, sy);
    if (environment_changed(backdropKey_) || backdrop_.empty())
        build_backdrop();
    cv_.px = backdrop_;

    if (L.clouds > 0.03f || L.overcast > 0.03f) {
        const int skyRows = std::min(cv_.h, static_cast<int>(std::ceil(Horizon * sy)));
        Environment::paint_clouds({cv_.px.data(), cv_.w, skyRows, 1.0f}, L, t_, 4 * sy, 50 * sy, 0.0f, 5);
        for (size_t i = 0; i < static_cast<size_t>(skyRows * cv_.w); ++i)
            if (!sky_[i])
                std::copy_n(&backdrop_[i * 3], 3, &cv_.px[i * 3]);
    }
    beam_ += dt * 0.7f;
    draw_lighthouse();
    draw_water(raw);
    draw_boats(dt);
    draw_gulls(dt);
    // A tree at the root of the pier shows the season up close.
    Environment::paint_tree(surface(), L, 87.0f * sx, PierTop * sy, 19.0f * sy, 23, t_, 0.1f);
    // Quay lamps.
    if (L.lamp() > 0.03f)
        for (float x : {14.5f, 32.5f}) {
            cv_.glow(x * sx, 77.5f * sy, 5.0f * k, {1.0f, 0.72f, 0.38f}, 0.35f * L.lamp());
            cv_.set(static_cast<int>(x * sx), static_cast<int>(77.5f * sy), {1.0f, 0.85f, 0.55f});
        }
    // Morning mist lies on the water and lifts as the sun climbs. The moored
    // boats sit in front of it, so fog keeps them clear against the far pier.
    const float mist = 0.30f * L.twilight * (L.morning ? 1.0f : 0.35f);
    Environment::paint_fog(surface(), L, t_, 58.0f * sy, static_cast<float>(cv_.h), mist);
    draw_moored();
    // Winter mornings are cold: a cool cast over everything.
    if (L.winter > 0.02f)
        for (size_t i = 0; i < cv_.px.size(); i += 3) {
            cv_.px[i] *= 1.0f - 0.10f * L.winter;
            cv_.px[i + 1] *= 1.0f - 0.02f * L.winter;
            cv_.px[i + 2] *= 1.0f + 0.10f * L.winter;
        }
    // A lightning strike lifts the whole harbour a little, not just the boats.
    if (L.flash > 0.0f)
        for (size_t i = 0; i < cv_.px.size(); i += 3) {
            cv_.px[i] += 0.55f * 0.25f * L.flash;
            cv_.px[i + 1] += 0.58f * 0.25f * L.flash;
            cv_.px[i + 2] += 0.70f * 0.25f * L.flash;
        }
    // Rain falls into the water (the rings show it), so no splashes line the
    // quay and pier.
    paint_weather(raw, Horizon * sy, [&](float) { return cv_.h + 6.0f; });  // below the frame
    present(canvas);
    wait_until_next_frame();
    return true;
}

Scenes::SceneDescriptor HarbourScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "scenery";
    d.tags = {"scenery",   "coast",    "sea",      "harbour",     "boats",     "calm",
              "fits:dawn", "fits:day", "fits:fog", "fits:cloudy", "fits:rain", "fits:storm"};
    d.intensity = 0.15f;
    d.motion = 0.18f;
    d.music_affinity = 0.05f;
    d.performance_cost = 0.3f;
    environment_->add_variants(d);
    return d;
}

}  // namespace Hobbies
