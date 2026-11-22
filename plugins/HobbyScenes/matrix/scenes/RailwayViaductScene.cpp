#include "RailwayViaductScene.h"

#include <tuple>

namespace Hobbies {
namespace {

constexpr float Portal = 30.0f;
constexpr float Rail = 79.0f;
constexpr float WaterTop = 87.0f;
constexpr float Horizon = 74.0f;
constexpr float CycleSeconds = 120.0f;

// Draw in a 128×128 design space. Scale the composition to the actual canvas.
struct Artwork {
    Canvas& canvas;
    float sx, sy;

    explicit Artwork(Canvas& cv) : canvas(cv), sx(cv.w / 128.0f), sy(cv.h / 128.0f) {}

    void rect(float x, float y, float width, float height, Rgb color) const
    {
        const int left = std::max(0, static_cast<int>(std::lround(x * sx)));
        const int right = std::min(canvas.w, static_cast<int>(std::lround((x + width) * sx)));
        const int top = std::max(0, static_cast<int>(std::lround(y * sy)));
        const int bottom = std::min(canvas.h, static_cast<int>(std::lround((y + height) * sy)));
        for (int row = top; row < bottom; ++row)
            for (int col = left; col < right; ++col) canvas.set(col, row, color);
    }

    void oval(float x, float y, float rx, float ry, Rgb color, float alpha = 1.0f) const
    {
        canvas.ellipse(x * sx, y * sy, rx * sx, ry * sy, 0.0f, color, alpha);
    }

    void pine(float x, float base, float height, Rgb shade, Rgb lit) const
    {
        rect(x, base - height, 1.0f, height, shade);
        for (int branch = 1; branch <= 4; ++branch) {
            const float width = height * (0.08f + 0.036f * branch);
            const float y = base - height + height * branch * 0.207f;
            const float r = std::max(0.48f, height * 0.055f) * sy;
            canvas.capsule((x - width) * sx, (y + width * 0.4f) * sy, r, x * sx, y * sy, r, shade);
            canvas.capsule(x * sx, y * sy, r, (x + width) * sx, (y + width * 0.4f) * sy, r, lit);
        }
    }
};

float far_bank(float x)
{
    return 79 - 15 * hill(x, 1.7f);
}

}  // namespace

RailwayViaductScene::RailwayViaductScene()
{
    use_environment({Environment::TimeOfDay::Night, Environment::Weather::Clear, Environment::Season::Summer});
}

void RailwayViaductScene::initialize(int width, int height)
{
    HobbySceneBase::initialize(width, height);
    backdrop_.clear();
    ripples_.clear();
    drift_.reset();
    cycle_ = 0.0f;
}

void RailwayViaductScene::build_backdrop()
{
    const auto& L = env();
    Canvas bg;
    bg.resize(cv_.w, cv_.h);
    const Artwork art(bg);
    const Environment::Surface s{bg.px.data(), bg.w, bg.h, 1.0f};
    const float W = static_cast<float>(bg.w), k = std::min(art.sx, art.sy);
    const float horizon = Horizon * art.sy;

    Environment::paint_sky(s, L, horizon);
    Environment::paint_stars(s, L, 60 * art.sy, 1.3f, 29, 80);
    Environment::paint_sun(s, L, Environment::sun_point(L, W, horizon), 6.0f * k);
    Environment::paint_moon(s, L, Environment::moon_point(L, W, horizon), 5.0f * k);
    const std::vector<float> skyStage = bg.px;

    // Two ridges of mixed forest fade into the haze behind the river.
    bg.fill_below([&](float x) { return art.sy * (72 - 33 * hill(x / art.sx, 2.8f)); }, plant({0.27f, 0.40f, 0.31f}, 0.75f, 0.8f, 0.5f));
    bg.fill_below([&](float x) { return art.sy * (74 - 24 * hill(x / art.sx, 4.9f)); }, plant({0.19f, 0.33f, 0.22f}, 0.5f, 0.8f, 0.5f));

    // The far bank: pines, two broadleaf trees that show the season and a cottage.
    const Rgb pineShade = terrain({0.07f, 0.18f, 0.13f}, 0.35f, 0.5f), pineLit = terrain({0.10f, 0.24f, 0.16f}, 0.35f, 0.5f);
    const Rgb bank = meadow({0.20f, 0.34f, 0.18f}, 0.35f);
    bg.fill_below([&](float x) { return art.sy * (far_bank(x / art.sx) + 1); }, bank);
    for (int x = 4; x < 128; x += 4) {
        if ((x > 52 && x < 68) || (x > 94 && x < 110))
            continue;
        art.pine(x, far_bank(x), 4 + 8 * hash01(x), pineShade, pineLit);
    }
    for (const auto& [x, height, seed] : {std::tuple{58.0f, 13.0f, 5u}, std::tuple{64.0f, 9.0f, 9u}})
        Environment::paint_tree(s, L, x * art.sx, far_bank(x) * art.sy + 1, height * art.sy, seed, 0.0f, 0.35f);
    {
        const float cx = 100, base = far_bank(cx) + 1;
        art.rect(cx - 4, base - 5, 8, 5, lit({0.78f, 0.70f, 0.58f}, 0.35f));
        const Rgb roof = terrain({0.48f, 0.22f, 0.17f}, 0.35f, 1.0f);
        for (int row = 0; row < 3; ++row) art.rect(cx - 5 + row, base - 6 - row, 10 - 2 * row, 1, roof);
        art.rect(cx + 2, base - 10, 1, 2, lit({0.40f, 0.36f, 0.34f}, 0.35f));
        const int wx = static_cast<int>(std::lround((cx - 2) * art.sx)), wy = static_cast<int>(std::lround((base - 4) * art.sy));
        Environment::paint_window(s, L, wx, wy, std::max(1, static_cast<int>(2 * art.sx)), std::max(1, static_cast<int>(2 * art.sy)), 3);
        if (L.lamp() > 0.02f)
            bg.glow((cx - 1) * art.sx, (base - 3) * art.sy, 5 * k, {1.0f, 0.66f, 0.3f}, 0.18f * L.lamp());
    }
    art.pine(91, far_bank(91), 10, pineShade, pineLit);
    art.pine(112, far_bank(112), 8, pineShade, pineLit);

    // The river reflects the sky: brightest far away, darker close by.
    const Rgb waterDeep = lit({0.10f, 0.20f, 0.27f}, 0.1f);
    const Rgb waterFar = mix(lit({0.16f, 0.30f, 0.38f}, 0.3f), from_env(L.sky(0.8f)), 0.35f);
    for (int y = static_cast<int>(WaterTop); y < 128; ++y)
        art.rect(0, static_cast<float>(y), 128, 1, mix(waterFar, waterDeep, (y - WaterTop) / (128 - WaterTop)));
    const std::vector<float> waterStage = bg.px;

    const Rgb stone = lit({0.50f, 0.47f, 0.44f}, 0.15f);
    const Rgb stoneEdge = lit({0.66f, 0.62f, 0.57f}, 0.15f);
    const Rgb stoneShadow = lit({0.30f, 0.30f, 0.33f}, 0.15f);
    art.rect(26, Rail, 102, 6, stone);
    for (int pier = 36; pier < 128; pier += 22) {
        for (int row = 0; row < 9; ++row) {
            // The underside of each rounded span is open above the river.
            const float reach = 10 - std::sqrt(std::max(0.0f, 100 - (8 - row) * (8 - row) * 1.55f));
            art.rect(pier - 2 - reach, 85 + row, 5 + 2 * reach, 1, stone);
        }
        art.rect(pier - 2, 94, 5, 10, stone);
        art.rect(pier + 2, 90, 1, 13, stoneEdge);
        art.rect(pier - 3, 103, 7, 2, lit({0.26f, 0.27f, 0.29f}, 0.15f));
    }
    // Snow settles on the parapet; the rails show as a darker line under it.
    art.rect(26, Rail - 1, 102, 1, terrain({0.66f, 0.62f, 0.57f}, 0.15f, 1.0f));
    art.rect(26, Rail, 102, 1, stoneEdge);
    art.rect(26, Rail + 3, 102, 1, stoneShadow);

    // The left hillside surrounds the tunnel mouth. A red signal marks the exit.
    const Rgb slopeTop = plant({0.25f, 0.40f, 0.21f}, 0.05f, 0.9f, 0.6f), slopeFoot = plant({0.13f, 0.25f, 0.14f}, 0.0f, 0.9f, 0.6f);
    for (int y = 55; y < 128; ++y) {
        const float edge = 14 + 15 * smoothstep(54, 73, y) + 11 * smoothstep(94, 128, y);
        art.rect(0, y, edge, 1, mix(slopeTop, slopeFoot, (y - 55) / 73.0f));
    }
    const Rgb mouth{0.015f, 0.02f, 0.035f};
    art.oval(23, Rail - 8, 8, 9, stoneEdge);
    art.rect(15, Rail - 8, 16, 8, stone);
    art.oval(23, Rail - 7, 6, 7, mouth);
    art.rect(17, Rail - 7, 13, 7, mouth);
    art.rect(34, Rail - 9, 1, 9, stoneShadow);
    bg.glow(34.5f * art.sx, (Rail - 9) * art.sy, 5 * k, {0.65f, 0.12f, 0.06f}, 0.12f + 0.2f * L.night);
    art.rect(34, Rail - 10, 1, 2, {0.9f, 0.3f, 0.16f});

    const Rgb nearShade = terrain({0.05f, 0.15f, 0.10f}, 0.0f, 0.5f), nearLit = terrain({0.08f, 0.21f, 0.13f}, 0.0f, 0.5f);
    art.pine(4, 75, 26, nearShade, nearLit);
    art.pine(12, 65, 15, nearShade, nearLit);
    art.pine(118, 128, 22, nearShade, nearLit);
    art.pine(126, 128, 33, nearShade, nearLit);

    // Remember which pixels still show open sky (for the drifting clouds) and
    // open water (for the animated reflections).
    const auto unchanged = [&](const std::vector<float>& stage, size_t i) {
        return bg.px[i * 3] == stage[i * 3] && bg.px[i * 3 + 1] == stage[i * 3 + 1] && bg.px[i * 3 + 2] == stage[i * 3 + 2];
    };
    sky_.assign(static_cast<size_t>(bg.w * bg.h), 0);
    water_.assign(static_cast<size_t>(bg.w * bg.h), 0);
    const int skyRows = std::min(bg.h, static_cast<int>(std::ceil(horizon)));
    for (int y = 0; y < bg.h; ++y)
        for (int x = 0; x < bg.w; ++x) {
            const size_t i = static_cast<size_t>(y * bg.w + x);
            if (y < skyRows)
                sky_[i] = unchanged(skyStage, i);
            else if (y >= static_cast<int>(WaterTop * art.sy))
                water_[i] = unchanged(waterStage, i);
        }
    backdrop_ = std::move(bg.px);
}

void RailwayViaductScene::draw_water(float raw_dt, float front, bool train)
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
    const float calm = 1.0f - 0.6f * L.fog;

    // Slow ripples catch the sky.
    const Rgb sheen = mix(from_env(L.sky(0.55f)), {0.8f, 0.88f, 1.0f}, 0.2f + 0.3f * L.night);
    for (uint32_t i = 0; i < 48; ++i) {
        const float y = 89 + 39 * hash01(4 * i);
        const float x = 4 + 120 * hash01(4 * i + 1) + 2.5f * std::sin(t_ * 0.35f + i * 1.9f);
        const float pulse = 0.55f + 0.45f * std::sin(t_ * (0.8f + 0.5f * hash01(4 * i + 2)) + i * 2.3f);
        streak(x, y, 2 + 7 * hash01(4 * i + 3), sheen, (0.05f + 0.07f * L.night) * pulse * calm);
    }

    // The sun or moon leaves a glittering path below it.
    const bool moon = L.night > 0.5f;
    const float lightX = (moon ? Environment::moon_point(L, W, Horizon * sy).x : Environment::sun_point(L, W, Horizon * sy).x) / sx;
    const float pathVis = (moon ? 0.7f * L.moon_light + 0.25f * L.night : std::clamp((L.sun_elevation + 2.0f) / 4.0f, 0.0f, 1.0f)) *
                          (1.0f - 0.9f * L.overcast) * calm;
    if (pathVis > 0.02f) {
        const Rgb pathColor = moon ? Rgb{0.78f, 0.85f, 1.0f} : mix(from_env(L.sun_color), {1.0f, 0.95f, 0.85f}, 0.5f);
        const uint32_t frame = static_cast<uint32_t>(t_ * 5.0f);
        for (int y = 89; y < 128; y += 2) {
            const float d = (y - WaterTop) / (128 - WaterTop);
            const float half = 3 + 8 * d;
            const float wobble = 1.5f * std::sin(y * 0.7f + t_ * 1.4f);
            for (float x = lightX - half; x < lightX + half; x += 1.0f) {
                const float fall = 1 - std::fabs(x - lightX) / half;
                const float glint = hash01(static_cast<uint32_t>(y * 977 + static_cast<int>(x) * 131) + frame * 2654435761u);
                streak(x + wobble, static_cast<float>(y), 1, pathColor, pathVis * fall * (glint > 0.7f ? 0.32f : 0.06f));
            }
        }
    }

    // Lit carriage windows trail wavering reflections across the river.
    const float lamp = L.lamp();
    if (train && lamp > 0.03f) {
        for (int car = 0; car < 3; ++car)
            for (int window = 0; window < 3; ++window) {
                const float x = front - car * 14 - 11 + 3 * window;
                if (x < Portal)
                    continue;
                const uint32_t id = static_cast<uint32_t>(car * 3 + window);
                for (int y = 106; y < 128; y += 2) {
                    // Broken up by the current, so it reads as light on moving water.
                    const float flicker = hash01(id * 131 + static_cast<uint32_t>(y) * 17 + static_cast<uint32_t>(t_ * 4.0f) * 7919);
                    if (flicker < 0.45f)
                        continue;
                    const float fade = 1 - (y - 106) / 22.0f;
                    const float drift = 1.6f * std::sin(y * 0.7f + t_ * 1.7f + id * 1.3f);
                    streak(x + drift, static_cast<float>(y), 1 + (flicker > 0.8f), {1.0f, 0.68f, 0.34f}, 0.16f * lamp * fade * calm);
                }
            }
    }

    // Raindrops ring the surface.
    for (auto& r : ripples_) {
        r.r += raw_dt * 5.0f;
        r.life -= raw_dt * 1.1f;
    }
    std::erase_if(ripples_, [](const Ripple& r) { return r.life <= 0.0f; });
    if (rnd() < raw_dt * 30.0f * L.rain && ripples_.size() < 30)
        ripples_.push_back({4 + 120 * rnd(), 105 + 22 * rnd(), 0.3f, 1.0f});
    const Rgb ring = mix(sheen, {1, 1, 1}, 0.3f);
    for (const auto& r : ripples_) {
        streak(r.x - r.r - 1, r.y, 1.5f, ring, 0.25f * r.life);
        streak(r.x + r.r, r.y, 1.5f, ring, 0.25f * r.life);
    }
}

void RailwayViaductScene::draw_train(float front)
{
    const auto& L = env();
    const Artwork art(cv_);
    const float lamp = L.lamp();
    const auto part = [&](float x, float y, float width, float height, Rgb color) {
        const float left = std::max(Portal, x);
        if (x + width > left)
            art.rect(left, Rail + y, x + width - left, height, color);
    };
    // The headlight lights the rails ahead without painting through the hillside.
    for (int i = 0; i < 12 && lamp > 0.02f; ++i) {
        const int x = static_cast<int>(front) + i;
        if (x >= Portal)
            cv_.add(static_cast<int>(x * art.sx), static_cast<int>(Rail * art.sy), {1.0f, 0.67f, 0.29f}, (1 - i / 12.0f) * 0.31f * lamp);
    }
    const Rgb body = subject({0.55f, 0.20f, 0.22f}), roof = subject({0.46f, 0.44f, 0.46f});
    const Rgb stripe = subject({0.88f, 0.74f, 0.48f}), wheels{0.06f, 0.07f, 0.09f};
    const Rgb glass = subject(mix({0.16f, 0.20f, 0.26f}, from_env(L.sky(0.4f)), 0.45f));
    const Rgb window = mix(glass, {0.94f, 0.66f, 0.34f}, std::min(1.0f, lamp * 1.5f));
    for (int car = 2; car >= 0; --car) {
        const float right = front - car * 14;
        part(right - 13, -6, 13, 5, body);
        part(right - 12, -7, 11, 1, roof);
        part(right - 13, -2, 13, 1, stripe);
        part(right - 11, -1, 2, 1, wheels);
        part(right - 4, -1, 2, 1, wheels);
        for (int pane = 0; pane < 3; ++pane) part(right - 11 + 3 * pane, -5, 2, 2, window);
    }
    part(front - 12, -9, 5, 3, scale(body, 0.85f));
    part(front - 11, -8, 2, 2, mix(glass, {0.96f, 0.73f, 0.45f}, std::min(1.0f, lamp * 1.5f)));
    part(front - 4, -8, 2, 2, scale(body, 0.8f));
    part(front - 1, -5, 2, 2, mix(subject({0.80f, 0.80f, 0.76f}), {1.0f, 0.91f, 0.65f}, lamp));
    part(front - 41, -4, 1, 1, mix(subject({0.55f, 0.16f, 0.12f}), {0.84f, 0.22f, 0.14f}, std::max(lamp, 0.3f)));
    // In fog the headlight blooms.
    if (front > Portal && lamp > 0.02f)
        cv_.glow(front * art.sx, (Rail - 4) * art.sy, 9 * art.sx, {1.0f, 0.85f, 0.55f}, lamp * (0.06f + 0.2f * L.fog));
}

bool RailwayViaductScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt = 0.0f;
    begin_frame(dt);
    const float raw = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.1f);
    const auto& L = env();
    const float H = static_cast<float>(cv_.h), sy = H / 128.0f;
    if (environment_changed(backdropKey_) || backdrop_.empty())
        build_backdrop();
    cv_.px = backdrop_;

    // Clouds drift behind the ridges: paint them over the sky rows, then put
    // back every pixel the landscape covers.
    if (L.clouds > 0.03f || L.overcast > 0.03f) {
        const int skyRows = std::min(cv_.h, static_cast<int>(std::ceil(Horizon * sy)));
        Environment::paint_clouds({cv_.px.data(), cv_.w, skyRows, 1.0f}, L, t_, 3 * sy, 50 * sy, 0.0f, 7);
        for (size_t i = 0; i < static_cast<size_t>(skyRows * cv_.w); ++i)
            if (!sky_[i])
                std::copy_n(&backdrop_[i * 3], 3, &cv_.px[i * 3]);
    }

    cycle_ = std::fmod(cycle_ + dt, CycleSeconds);
    const float front = 16 + 20 * cycle_;
    const bool train = front > Portal - 12 && front < 170;
    draw_water(raw, front, train);
    if (train)
        draw_train(front);

    Environment::paint_fog(surface(), L, t_, 52 * sy, H);
    drift_.update(raw, L, cv_.w, cv_.h, [&](float) { return H * 0.84f; }, 0.7f);
    drift_.draw(surface(), L);
    paint_weather(raw, Horizon * sy);
    present(canvas);
    wait_until_next_frame();
    return true;
}

Scenes::SceneDescriptor RailwayViaductScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "scenery";
    d.tags = {"scenery", "landscape", "calm", "fits:dusk", "fits:night", "fits:fog", "fits:snow"};
    d.intensity = 0.16f;
    d.motion = 0.16f;
    d.music_affinity = 0.05f;
    d.performance_cost = 0.3f;
    environment_->add_variants(d);
    return d;
}

}  // namespace Hobbies
