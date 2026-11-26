#include "DesertDunesScene.h"

#include "Backdrop.h"

namespace Hobbies {
namespace {

constexpr float Horizon = 66.0f;  // design rows (128 × 128 design space)
constexpr int CaravanLayer = 2;
constexpr int Camels = 4;
constexpr float CamelGap = 9.0f;
constexpr float CaravanSpeed = 2.4f;   // design px per second
constexpr float NoCaravan = -1000.0f;  // caravan_ while no caravan is out

// One row of dunes. Wind from the left builds a gentle windward slope and a
// steep lee face to the right of each crest. A slow warp of x varies the
// spacing, so the crests never line up like tiles.
struct DuneLayer {
    float base, amp, period, offset, depth, ripple;
    uint32_t seed;

    // Crest position (0..1 within a period) and height (0..1) of dune i.
    [[nodiscard]] float crest(int i) const { return 0.55f + 0.25f * hash01(static_cast<uint32_t>(i) * 13u + seed + 1u); }
    [[nodiscard]] float height(int i) const { return 0.4f + 0.6f * hash01(static_cast<uint32_t>(i) * 7u + seed); }

    [[nodiscard]] float warp(float x) const { return x + 0.28f * period * std::sin(x / period * 0.85f + static_cast<float>(seed)); }
    // Position in dune periods at design x: whole part = dune index, fraction = place in that dune.
    [[nodiscard]] float phase(float x) const { return (warp(x) + offset) / period; }

    // Top edge (design row) at design x.
    [[nodiscard]] float top(float x) const
    {
        const float p = phase(x);
        const int i = static_cast<int>(std::floor(p));
        const float u = p - static_cast<float>(i), c = crest(i);
        float h;
        if (u < c) {
            const float v = u / c;
            h = 0.5f * v + 0.5f * std::sin(0.5f * kPi * v);  // rises to a sharp crest
        }
        else {
            const float v = 1.0f - (u - c) / (1.0f - c);
            h = v * std::sqrt(v);  // steep lee face that flattens into the trough
        }
        const float swell = 0.5f + 0.5f * std::sin(x * 0.031f + static_cast<float>(seed));
        return base - amp * (height(i) * h * 0.8f + 0.2f * swell);
    }

    // Design x of the crest of dune i (inverts the warp by a few fixed-point steps).
    [[nodiscard]] float crest_x(int i) const
    {
        const float target = (static_cast<float>(i) + crest(i)) * period - offset;
        float x = target;
        for (int k = 0; k < 6; ++k) x = target - 0.28f * period * std::sin(x / period * 0.85f + static_cast<float>(seed));
        return x;
    }
};

const std::array<DuneLayer, 4> kLayers{{
    {Horizon + 4.0f, 9.0f, 58.0f, 11.0f, 0.85f, 0.0f, 3u},
    {Horizon + 15.0f, 13.0f, 72.0f, 30.0f, 0.6f, 0.0f, 17u},
    {Horizon + 30.0f, 10.0f, 84.0f, 5.0f, 0.35f, 0.015f, 29u},
    {Horizon + 62.0f, 22.0f, 110.0f, 47.0f, 0.08f, 0.06f, 41u},
}};

}  // namespace

DesertDunesScene::DesertDunesScene()
{
    Environment::Adapt adapt;
    adapt.season = false;  // the desert looks the same all year
    // Clear skies and drifting cloud; a storm becomes a sandstorm.
    adapt.weather_allowed = {Environment::Weather::Clear, Environment::Weather::PartlyCloudy, Environment::Weather::Overcast,
                             Environment::Weather::Storm};
    use_environment({Environment::TimeOfDay::Dusk, Environment::Weather::Clear, Environment::Season::Summer}, adapt);
}

void DesertDunesScene::initialize(int width, int height)
{
    HobbySceneBase::initialize(width, height);
    backdrop_.clear();
    grains_.clear();
    caravan_ = NoCaravan;
    caravanWait_ = 6.0f;
    spawn_ = 0.0f;
}

void DesertDunesScene::build_backdrop()
{
    const auto& L = env();
    Canvas bg;
    bg.resize(cv_.w, cv_.h);
    const Artwork art(bg);
    const Environment::Surface s{bg.px.data(), bg.w, bg.h, 1.0f};
    const float W = static_cast<float>(bg.w), k = std::min(art.sx, art.sy);
    const float horizon = Horizon * art.sy;

    Environment::paint_sky(s, L, horizon + 6 * art.sy);
    // Desert nights are clear and dark: plenty of stars and a large moon.
    Environment::paint_stars(s, L, horizon, 1.7f, 31, 140);
    Environment::paint_sun(s, L, Environment::sun_point(L, W, horizon), 7.0f * k);
    Environment::paint_moon(s, L, Environment::moon_point(L, W, horizon * 0.8f), 8.5f * k);
    const std::vector<float> skyStage = bg.px;

    // Light comes from the sun by day and the moon at night. A low light
    // gives long, strong shadows on the lee faces; overcast skies soften them.
    const bool moon = L.night > 0.5f;
    const float lightX = moon ? Environment::moon_point(L, W, horizon).x / W : L.sun_x;
    const float side = lightX < 0.5f ? -1.0f : 1.0f;
    const float low = moon ? 0.8f : 1.6f - std::clamp(L.sun_elevation / 50.0f, 0.0f, 1.0f);
    const float contrast = low * (1.0f - 0.75f * L.overcast) * (moon ? 0.6f * L.moon_light + 0.2f : 1.0f);
    const Rgb sand{0.86f, 0.62f, 0.38f};
    // The air near the horizon tints the far ridges, so the rows separate by distance.
    const size_t hz = (static_cast<size_t>(std::clamp(horizon - 2.0f, 0.0f, bg.h - 1.0f)) * bg.w + bg.w / 2) * 3;
    const Rgb horizonSky{skyStage[hz], skyStage[hz + 1], skyStage[hz + 2]};

    for (const auto& layer : kLayers) {
        Rgb base = lit(sand, layer.depth);
        if (L.night > 0.0f) {
            // Moonlit sand turns silver-blue and keeps its shape against the dark sky.
            const float luma = 0.3f * base.r + 0.55f * base.g + 0.15f * base.b;
            const Rgb silver = scale(Rgb{0.62f, 0.72f, 1.0f}, luma * (1.3f + 0.9f * L.moon_light));
            base = mix(base, silver, 0.7f * L.night);
        }
        base = mix(base, horizonSky, 0.28f * layer.depth * layer.depth * (1.0f - L.night) * (1.0f - L.storm));
        // Shadows are a cool plum lit by the sky, never black; farther rows lose contrast.
        const float luma = 0.3f * base.r + 0.55f * base.g + 0.15f * base.b;
        const Rgb shade_color =
            mix(mix(scale(base, 0.72f), scale(Rgb{0.62f, 0.48f, 0.78f}, luma * 0.95f), 0.35f), scale(from_env(L.zenith), 0.9f), 0.15f);
        const float fade = layer.depth;
        const float lit_gain = 0.92f + 0.22f * contrast;
        const float shade_strength = (1.0f - 0.55f * fade);
        // Warm light rakes along the crests that face a low sun.
        const Rgb shoulder_light =
            mix(scale(base, moon ? 1.8f : 1.3f), scale(from_env(L.sun_color), 0.9f), moon ? 0.0f : 0.35f * L.twilight);
        // With the light on the right, the gentle windward slopes only dim a little, and less so with a high sun.
        const float windward_dim = 0.12f + 0.22f * std::clamp(low - 0.3f, 0.0f, 1.0f);
        // A high sun skims the steep slip faces, so they read darker than the gentle slopes.
        const float overhead = std::clamp((1.25f - low) / 0.6f, 0.0f, 1.0f);
        for (int x = 0; x < bg.w; ++x) {
            const float dx = (x + 0.5f) / art.sx;
            const float top = layer.top(dx);
            const float t = top * art.sy;
            const int y0 = std::max(0, static_cast<int>(std::floor(t)));
            const float p = layer.phase(dx);
            const int i = static_cast<int>(std::floor(p));
            for (int y = y0; y < bg.h; ++y) {
                const float dy = (y + 0.5f) / art.sy;
                const float under = dy - top;
                // Each dune's slip face is a wedge under its lee edge: deepest just past the crest,
                // thinning to nothing at the trough. Its edge leans from the crest down to the
                // right, so the face never starts with an upright line.
                float slip = 0.0f;
                for (int j = i - 1; j <= i; ++j) {
                    const float c = layer.crest(j), u = p - static_cast<float>(j);
                    const float full = layer.amp * layer.height(j) * 0.95f;
                    float depth;
                    if (u >= c && u < 1.0f)
                        depth = full * (1.0f - (u - c) / (1.0f - c)) * std::min(1.0f, (u - c) / 0.07f + 0.05f);
                    else
                        continue;
                    // The shadow lightens toward its foot, so big faces don't end as flat dark wedges.
                    slip = std::max(slip, (1.0f - smoothstep(depth - 1.5f, depth + 0.5f, under)) *
                                              (1.0f - 0.35f * std::clamp(under / std::max(full, 1.0f), 0.0f, 1.0f)));
                }
                const float dark = (side < 0.0f ? slip : windward_dim * (1.0f - slip) + 0.45f * overhead * slip) * shade_strength;
                const float glare = side < 0.0f ? 0.0f : 0.12f * (1.0f - overhead) * slip;
                // A pale lit shoulder just under each crest on the light's side.
                // At night a thin moonlit rim traces each crest.
                const float shoulder = (1.0f - slip) * std::exp(-under / (moon ? 1.6f : 2.5f)) *
                                       ((side < 0.0f ? 0.5f : 0.3f) * (0.4f + 0.6f * contrast) + (moon ? 0.6f : 0.0f));
                const float below = std::clamp(under / (layer.amp + 8.0f), 0.0f, 1.0f);
                float gain = lit_gain * (1.0f - 0.15f * below);
                // Wind ripples on the near slopes, fading with distance below the crest and broken up.
                if (layer.ripple > 0.0f)
                    gain *= 1.0f + layer.ripple * (0.6f + 0.6f * contrast) * std::clamp(under / 10.0f, 0.0f, 1.0f) *
                                       std::sin(dx * 0.55f + dy * 2.6f + 2.4f * std::sin(dx * 0.07f + dy * 0.05f)) *
                                       (0.5f + 0.5f * std::sin(dx * 0.13f - dy * 0.4f));
                Rgb c = mix(scale(base, gain * (1.0f + glare)), shade_color, dark);
                c = mix(c, shoulder_light, std::min(0.6f, shoulder));
                bg.blend(x, y, c, y == y0 ? 1.0f - (t - std::floor(t)) : 1.0f);
            }
        }
    }

    sky_.assign(static_cast<size_t>(bg.w * bg.h), 0);
    const int skyRows = std::min(bg.h, static_cast<int>(std::ceil(horizon + 8 * art.sy)));
    for (int y = 0; y < skyRows; ++y)
        for (int x = 0; x < bg.w; ++x) {
            const size_t i = static_cast<size_t>(y * bg.w + x);
            sky_[i] = unchanged(bg, skyStage, i);
        }
    backdrop_ = std::move(bg.px);
}

void DesertDunesScene::draw_shimmer()
{
    // Hot air over the far dunes bends the light: rows near the horizon waver sideways.
    const auto& L = env();
    const float heat = L.daylight * (1.0f - L.overcast) * (1.0f - L.storm) * std::clamp((L.temperature - 8.0f) / 22.0f, 0.35f, 1.0f);
    if (heat < 0.05f)
        return;
    const float sy = cv_.h / 128.0f, sx = cv_.w / 128.0f;
    const int y0 = static_cast<int>((Horizon - 4) * sy), y1 = static_cast<int>((Horizon + 16) * sy);
    row_.resize(static_cast<size_t>(cv_.w * 3));
    for (int y = std::max(0, y0); y < std::min(cv_.h, y1); ++y) {
        const float fade = 1.0f - std::fabs((y - (y0 + y1) * 0.5f) / ((y1 - y0) * 0.5f));
        const float shift = heat * fade * 1.4f * sx * std::sin(t_ * 2.3f + y * 1.7f) * std::sin(t_ * 0.9f + y * 0.6f);
        std::copy_n(cv_.at(0, y), row_.size(), row_.begin());
        for (int x = 0; x < cv_.w; ++x) {
            const float src = std::clamp(x - shift, 0.0f, cv_.w - 1.001f);
            const int i = static_cast<int>(src);
            const float f = src - static_cast<float>(i);
            float* p = cv_.at(x, y);
            for (int c = 0; c < 3; ++c) p[c] = row_[i * 3 + c] * (1 - f) + row_[(i + 1) * 3 + c] * f;
        }
    }
}

void DesertDunesScene::draw_grains(float raw_dt)
{
    // Sand blows off the crests downwind in thin plumes; a storm lifts much more.
    const auto& L = env();
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f;
    const float wind = 0.35f + std::fabs(L.wind) + 1.5f * L.storm;
    spawn_ += raw_dt * (5.0f * wind + 60.0f * L.storm) * render_quality_scale();
    while (spawn_ >= 1.0f && grains_.size() < 220) {
        spawn_ -= 1.0f;
        const int li = 1 + static_cast<int>(rnd() * 2.99f);
        const auto& layer = kLayers[static_cast<size_t>(li)];
        const int first = static_cast<int>(std::floor(layer.phase(0.0f)));
        const int i = first + static_cast<int>(rnd() * (128.0f / layer.period + 1.0f));
        const float cx = layer.crest_x(i);
        const float storm = L.storm * rnd();
        const float scale_near = 0.6f + 0.6f * (1.0f - layer.depth);
        Grain g{};
        g.x = cx + rnd() * 1.5f;
        g.y = storm > 0.3f ? Horizon - 10 + rnd() * 70 : layer.top(cx) + rnd() * 1.2f;
        g.vx = (7.0f + 9.0f * rnd()) * scale_near * (1.0f + 2.5f * L.storm);
        g.vy = -1.5f + 3.0f * rnd();
        g.max_life = g.life = 0.7f + 1.1f * rnd();
        grains_.push_back(g);
    }
    if (spawn_ > 1.0f)
        spawn_ = 0.0f;
    const Rgb dust = lit({0.95f, 0.78f, 0.55f}, 0.3f);
    for (auto& g : grains_) {
        g.life -= raw_dt;
        g.x += g.vx * raw_dt;
        g.y += g.vy * raw_dt;
        g.vy += 2.0f * raw_dt;
        const float a = std::sin(kPi * std::clamp(g.life / g.max_life, 0.0f, 1.0f));
        const float px = g.x * sx - 0.5f, py = g.y * sy;
        const int col = static_cast<int>(std::floor(px));
        const float u = px - static_cast<float>(col);
        cv_.blend(col, static_cast<int>(py), dust, 0.32f * a * (1.0f - u));
        cv_.blend(col + 1, static_cast<int>(py), dust, 0.32f * a * u);
    }
    std::erase_if(grains_, [](const Grain& g) { return g.life <= 0.0f || g.x > 132.0f; });
}

void DesertDunesScene::draw_caravan(float dt)
{
    if (caravan_ <= NoCaravan) {
        caravanWait_ -= dt;
        if (caravanWait_ > 0.0f)
            return;
        caravan_ = -6.0f;
    }
    caravan_ += CaravanSpeed * dt;
    if (caravan_ - CamelGap * Camels - 3.0f > 134.0f) {
        caravan_ = NoCaravan;
        caravanWait_ = 35.0f + 40.0f * rnd();
        return;
    }
    const auto& L = env();
    const auto& layer = kLayers[CaravanLayer];
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f;
    // The caravan keeps to an even path along the ridge instead of every dip.
    const auto ground = [&](float x) {
        float sum = 0.0f;
        for (int j = -3; j <= 3; ++j) sum += layer.top(x + j * 2.5f);
        return sum / 7.0f;
    };
    // At night the caravan is a dark silhouette against the moonlit sand.
    const Rgb hide = mix(subject({0.42f, 0.29f, 0.18f}), {0.03f, 0.03f, 0.05f}, 0.65f * L.night), dark = scale(hide, 0.7f);
    const Rgb cloth = subject({0.62f, 0.16f, 0.12f}), wrap = subject({0.82f, 0.78f, 0.70f});
    const Rgb shadow = scale(from_env(L.zenith), 0.25f);
    const float side = (L.night > 0.5f ? Environment::moon_point(L, cv_.w, Horizon * sy).x / cv_.w : L.sun_x) < 0.5f ? 1.0f : -1.0f;
    const auto P = [&](float x, float y) { return std::pair{x * sx, y * sy}; };
    for (int n = Camels - 1; n >= 0; --n) {
        // The lead camel walks a little ahead of the others.
        const float x = caravan_ - CamelGap * static_cast<float>(n) - (n > 0 ? 3.0f : 0.0f);
        if (x < -8.0f || x > 136.0f)
            continue;
        const float gy = ground(x);
        const float phase = x * 0.95f + static_cast<float>(n) * 1.9f;  // each animal in its own step
        // A faint shadow on the sand away from the light.
        cv_.ellipse((x + side * 1.5f) * sx, (gy + 0.2f) * sy, 3.2f * sx, 0.5f * sy, 0.0f, shadow, 0.35f * (1.0f - L.overcast));
        // Legs, far pair darker; each pair swings in opposite phase.
        for (int leg = 0; leg < 4; ++leg) {
            const float hip = (leg < 2 ? 1.5f : -1.5f) + (leg % 2 ? 0.3f : 0.0f);
            const float swing = 0.9f * std::sin(phase + (leg % 2 ? kPi : 0.0f) + (leg < 2 ? 0.0f : kPi * 0.5f));
            const auto [ax, ay] = P(x + hip, gy - 4.0f);
            const auto [bx, by] = P(x + hip + swing, gy);
            cv_.capsule(ax, ay, 0.38f * sx, bx, by, 0.3f * sx, leg % 2 ? dark : hide);
        }
        const float bob = 0.25f * std::sin(phase * 2.0f);
        cv_.ellipse(x * sx, (gy - 4.9f + bob) * sy, 2.7f * sx, 1.3f * sy, 0.0f, hide);
        cv_.ellipse((x - 0.4f) * sx, (gy - 6.1f + bob) * sy, 1.4f * sx, 1.0f * sy, 0.0f, hide);  // hump
        {
            const auto [ax, ay] = P(x + 2.1f, gy - 5.2f + bob);
            const auto [bx, by] = P(x + 3.5f, gy - 7.6f + bob);
            cv_.capsule(ax, ay, 0.55f * sx, bx, by, 0.42f * sx, hide);  // neck
            const auto [hx, hy] = P(x + 4.5f, gy - 7.4f + bob);
            cv_.capsule(bx, by, 0.5f * sx, hx, hy, 0.38f * sx, hide);  // head
        }
        if (n == 0) {
            // The lead camel carries a rider in a red robe and pale headcloth.
            const auto [ax, ay] = P(x - 0.4f, gy - 6.8f + bob);
            const auto [bx, by] = P(x - 0.3f, gy - 9.0f + bob);
            cv_.capsule(ax, ay, 0.75f * sx, bx, by, 0.55f * sx, cloth);
            cv_.ellipse(bx, by - 1.0f * sy, 0.7f * sx, 0.7f * sy, 0.0f, wrap);
        }
        else {
            // Pack loads on the others.
            cv_.ellipse((x - 0.6f) * sx, (gy - 6.9f + bob) * sy, 1.5f * sx, 0.8f * sy, 0.0f, n % 2 ? wrap : scale(cloth, 0.8f));
        }
    }
}

void DesertDunesScene::draw_sandstorm()
{
    const auto& L = env();
    if (L.storm < 0.02f)
        return;
    // A haze hides the distance while low gusts of sand sweep sideways through the middle
    // rows. Every so often the wind drops for a while and the ridges and the caravan show.
    // Daylight still glows through the dust, so it stays warm ochre instead of mud.
    const Rgb dust = mix(lit({0.78f, 0.55f, 0.32f}, 0.4f), scale(Rgb{0.80f, 0.56f, 0.32f}, 0.25f + 0.6f * L.daylight), 0.6f);
    const Rgb gust = scale(dust, 1.6f);
    const float lull = smoothstep(0.2f, 0.8f, std::sin(t_ * 0.3f));
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f;
    for (int y = 0; y < cv_.h; ++y) {
        const float dy = (y + 0.5f) / sy;
        const float distance = 1.0f - 0.75f * smoothstep(Horizon + 10.0f, 128.0f, dy);
        const float haze = L.storm * 0.34f * distance * (1.0f - 0.55f * lull);
        // Two drifting bands of blowing sand, one over the far crests and one lower down.
        const float b1 = dy - Horizon - 6.0f - 3.0f * std::sin(t_ * 0.5f);
        const float b2 = dy - Horizon - 30.0f - 4.0f * std::sin(t_ * 0.37f + 1.3f);
        const float bands = std::exp(-b1 * b1 / 60.0f) + 0.8f * std::exp(-b2 * b2 / 80.0f);
        const float bend = 1.2f * std::sin(dy * 0.15f + t_ * 0.5f);
        for (int x = 0; x < cv_.w; ++x) {
            const float dx = (x + 0.5f) / sx;
            const float sheet = smoothstep(0.45f, 1.0f, 0.5f + 0.5f * std::sin(dx * 0.06f - t_ * 2.6f + bend)) *
                                (0.6f + 0.4f * std::sin(dx * 0.021f - t_ * 0.9f));
            cv_.blend(x, y, dust, std::min(0.8f, haze));
            cv_.blend(x, y, gust, std::min(0.75f, L.storm * 0.85f * sheet * bands * (1.0f - 0.75f * lull)));
        }
    }
}

bool DesertDunesScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt = 0.0f;
    begin_frame(dt);
    const float raw = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.1f);
    const auto& L = env();
    const float sy = cv_.h / 128.0f;
    if (environment_changed(backdropKey_) || backdrop_.empty())
        build_backdrop();
    cv_.px = backdrop_;

    paint_clouds_behind(cv_, backdrop_, sky_, L, t_, Horizon * sy, 4 * sy, 40 * sy, 13);
    draw_shimmer();

    // The caravan walks on the middle ridge; restore the nearer dunes over it.
    draw_caravan(dt);
    {
        const Artwork art(cv_);
        const auto& near_layer = kLayers[CaravanLayer + 1];
        for (int x = 0; x < cv_.w; ++x) {
            const float t = near_layer.top((x + 0.5f) / art.sx) * art.sy;
            const int y0 = std::max(0, static_cast<int>(std::ceil(t)));
            for (int y = y0; y < cv_.h; ++y) std::copy_n(&backdrop_[static_cast<size_t>((y * cv_.w + x) * 3)], 3, cv_.at(x, y));
        }
    }

    draw_grains(raw);
    draw_sandstorm();
    present(canvas);
    wait_until_next_frame();
    return true;
}

Scenes::SceneDescriptor DesertDunesScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "scenery";
    d.tags = {"scenery", "landscape", "desert", "calm", "fits:clear", "fits:day", "fits:dusk", "fits:night"};
    d.intensity = 0.15f;
    d.motion = 0.18f;
    d.music_affinity = 0.05f;
    d.performance_cost = 0.25f;
    environment_->add_variants(d);
    return d;
}

}  // namespace Hobbies
