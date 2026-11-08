#include "HolidayScenes.h"

#include "SceneryDraw.h"

using namespace Hobbies;

namespace {
Environment::Adapt winter_night()
{
    Environment::Adapt adapt;
    adapt.time = false;    // the lights only work after dark
    adapt.season = false;  // always winter
    adapt.weather_allowed = {Environment::Weather::Clear, Environment::Weather::PartlyCloudy, Environment::Weather::Overcast,
                             Environment::Weather::Fog, Environment::Weather::Snow, Environment::Weather::Drizzle};
    return adapt;
}

// Gabled old-town house front: body [x0,x1) from `top` down to `base`, roof
// triangle above with snow on it.
void gabled_house(Canvas& cv, float x0, float x1, float top, float base, Rgb wall, Rgb roof, Rgb snow, float snowAmount)
{
    const float peak = top - (x1 - x0) * 0.55f, cx = (x0 + x1) * 0.5f;
    for (int y = std::max(0, static_cast<int>(peak)); y < static_cast<int>(base); ++y)
        for (int x = static_cast<int>(x0); x < static_cast<int>(x1); ++x) {
            if (y >= top) {
                cv.set(x, y, wall);
                continue;
            }
            const float half = (x1 - x0) * 0.5f * (y - peak) / (top - peak);
            const float d = half - std::fabs(x + 0.5f - cx);
            if (d < -0.5f)
                continue;
            // Roof edge (verge) with snow lying on it, a thicker cap at the top.
            const float k = (x1 - x0) / 20.0f;
            const bool verge = d < 2.4f * k;
            const bool cap = y < peak + 3.0f * k;
            cv.blend(x, y, verge || cap ? mix(roof, snow, snowAmount * (verge && d < 1.2f * k ? 1.0f : 0.8f)) : wall,
                     std::clamp(d + 0.5f, 0.0f, 1.0f));
        }
}
}  // namespace

// ================================================================ market
ChristmasMarketScene::ChristmasMarketScene()
{
    use_environment({Environment::TimeOfDay::Night, Environment::Weather::Snow, Environment::Season::Winter, 0.0f, 0.5f}, winter_night());
}

void ChristmasMarketScene::initialize(int width, int height)
{
    AudioHobbyScene::initialize(width, height);
    horizon_ = std::floor(static_cast<float>(height) * 0.40f);
    backdrop_.clear();
    steam_.clear();
    people_.clear();
    const float W = static_cast<float>(width), k = W / 128.0f;
    for (int i = 0; i < 6; ++i) {
        const float dir = i % 2 ? -1.0f : 1.0f;
        people_.push_back({(0.08f + 0.17f * i) * W, (3.5f + 1.5f * rnd()) * k, rnd(), (1.00f + 0.18f * rnd()) * k, dir, i % 4,
                           i == 1 ? 6.0f : 0.0f});
    }
}

void ChristmasMarketScene::build_backdrop()
{
    const auto& L = env();
    const int w = cv_.w, h = cv_.h;
    const float W = static_cast<float>(w), H = static_cast<float>(h), k = W / 128.0f;
    Canvas bg;
    bg.resize(w, h);
    const Environment::Surface s{bg.px.data(), w, h, 1.0f};
    Environment::paint_sky(s, L, horizon_);
    Environment::paint_moon(s, L, Environment::moon_point(L, W, horizon_), 4.5f * k);

    // Church tower behind the houses (a clock face, no numbers).
    const Rgb stone = lit({0.52f, 0.48f, 0.46f}, 0.4f), snow = lit({0.92f, 0.94f, 0.98f}, 0.2f);
    const float tx = W * 0.70f;
    for (int y = static_cast<int>(H * 0.10f); y < static_cast<int>(H * 0.62f); ++y)
        for (int x = static_cast<int>(tx - 5 * k); x < static_cast<int>(tx + 5 * k); ++x) bg.set(x, y, stone);
    for (int y = static_cast<int>(H * 0.10f - 18 * k); y < static_cast<int>(H * 0.10f); ++y) {
        const float half = 5.5f * k * (y - (H * 0.10f - 18 * k)) / (18 * k);
        for (int x = static_cast<int>(tx - half); x <= static_cast<int>(tx + half); ++x)
            bg.set(x, y, mix(lit({0.20f, 0.36f, 0.32f}, 0.4f), snow, 0.25f * L.snow_cover));
    }
    bg.ellipse(tx, H * 0.16f, 2.6f * k, 2.6f * k, 0, lit({0.90f, 0.82f, 0.55f}, 0.3f));
    bg.glow(tx, H * 0.16f, 6 * k, {1.0f, 0.85f, 0.5f}, 0.25f * L.lamp());
    bg.capsule(tx, H * 0.16f, 0.25f * k, tx, H * 0.16f - 1.8f * k, 0.25f * k, lit({0.2f, 0.15f, 0.1f}));
    bg.capsule(tx, H * 0.16f, 0.25f * k, tx + 1.3f * k, H * 0.16f, 0.25f * k, lit({0.2f, 0.15f, 0.1f}));

    // A row of gabled houses round the square.
    static const Rgb walls[] = {{0.78f, 0.62f, 0.48f}, {0.62f, 0.66f, 0.70f}, {0.80f, 0.72f, 0.56f},
                                {0.66f, 0.50f, 0.46f}, {0.74f, 0.70f, 0.62f}, {0.58f, 0.62f, 0.54f}};
    const Rgb roof = lit({0.30f, 0.18f, 0.14f}, 0.3f);
    float x = -6.0f * k;
    for (int i = 0; x < W; ++i) {
        const float wdt = (17.0f + 7.0f * hash01(i * 7u + 3u)) * k;
        const float top = H * (0.24f + 0.10f * hash01(i * 11u + 1u));
        gabled_house(bg, x, x + wdt, top, H * 0.62f, lit(walls[i % 6], 0.25f), roof, snow, L.snow_cover);
        x += wdt;
    }
    // Square: cobbles, with snow or a wet sheen.
    const Rgb cobble = terrain({0.42f, 0.40f, 0.40f}, 0.0f, 0.9f), joint = terrain({0.26f, 0.25f, 0.26f}, 0.0f, 0.7f);
    for (int y = static_cast<int>(H * 0.62f); y < h; ++y) {
        const float v = (y - H * 0.62f) / (H * 0.38f);
        const int row = static_cast<int>(y / (2.0f + 2.0f * v) / k);
        for (int px = 0; px < w; ++px) {
            const int col = static_cast<int>((px + (row % 2) * 2.0f * k) / ((3.0f + 3.0f * v) * k));
            const bool j = hash01(static_cast<uint32_t>(row * 977 + col)) < 0.22f;
            Rgb c = j ? joint : cobble;
            c = scale(c, 0.85f + 0.15f * v);
            if (L.wetness > 0.02f)
                c = mix(c, scale(c, 0.6f), L.wetness);
            bg.set(px, y, c);
        }
    }
    backdrop_ = std::move(bg.px);
}

bool ChristmasMarketScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt, rdt;
    begin_audio_frame(dt, rdt);
    const auto& L = env();
    const float W = static_cast<float>(cv_.w), H = static_cast<float>(cv_.h), k = W / 128.0f;
    if (environment_changed(backdropKey_) || backdrop_.empty())
        build_backdrop();
    cv_.px = backdrop_;
    const auto s = surface();
    Environment::paint_stars(s, L, horizon_ * 0.55f, t_, 5, 50);
    Environment::paint_clouds(s, L, t_, 0.0f, horizon_ * 0.45f);
    const float beat = audio_.beat * std::min(1.0f, audio_.amount);

    // Lit windows in the house fronts.
    float hx = -6.0f * k;
    for (int i = 0; hx < W; ++i) {
        const float wdt = (17.0f + 7.0f * hash01(i * 7u + 3u)) * k;
        const float top = H * (0.24f + 0.10f * hash01(i * 11u + 1u));
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 2; ++col) {
                const float wy = top + (3.0f + 6.0f * row) * k;
                if (wy + 4 * k > H * 0.60f)
                    continue;
                const float wx = hx + wdt * (0.22f + 0.40f * col);
                Environment::paint_window(s, L, static_cast<int>(wx), static_cast<int>(wy), static_cast<int>(3 * k),
                                          static_cast<int>(4 * k), static_cast<uint32_t>(i * 13 + row * 3 + col), t_);
            }
        hx += wdt;
    }

    // Light strings over the square.
    Environment::paint_string_lights(s, L, {0, H * 0.36f}, {W * 0.52f, H * 0.44f}, 4.0f * k, t_ * (1.0f + beat), 12, 1);
    Environment::paint_string_lights(s, L, {W * 0.48f, H * 0.44f}, {W, H * 0.34f}, 4.0f * k, t_ * (1.0f + beat), 12, 2);
    Environment::paint_string_lights(s, L, {0, H * 0.48f}, {W, H * 0.47f}, 6.0f * k, t_ * (1.0f + beat), 18, 3);

    // The tree: tiers of fir, garlands of lights, a star on top.
    const float tx = W * 0.5f, tb = H * 0.80f, th = 50.0f * k;
    // Warm light pools on the cobbles under the tree and the stalls.
    cv_.glow(tx, tb + 2 * k, 26 * k, {1.0f, 0.72f, 0.38f}, 0.20f * L.lamp());
    cv_.glow(20 * k, H * 0.92f, 24 * k, {1.0f, 0.66f, 0.32f}, 0.22f * L.lamp());
    cv_.glow(107 * k, H * 0.92f, 24 * k, {1.0f, 0.66f, 0.32f}, 0.22f * L.lamp());
    // Half width of pine() at height v (0 top .. 1 base), to keep lights on the tree.
    auto treeHalf = [&](float v) { return th * 0.36f * (0.25f + 0.75f * v) * (0.55f + 0.45f * fract(v * 3.0f)); };
    cv_.ellipse(tx, tb, 12 * k, 1.6f * k, 0, lit({0.10f, 0.10f, 0.12f}), 0.5f);
    for (int y = static_cast<int>(tb); y < static_cast<int>(tb + 4 * k); ++y)
        for (int x = static_cast<int>(tx - 1.5f * k); x < static_cast<int>(tx + 1.5f * k); ++x) cv_.set(x, y, lit({0.30f, 0.20f, 0.12f}));
    pine(cv_, tx, tb, th, lit({0.10f, 0.32f, 0.18f}, 0.2f));
    if (L.snow_cover > 0.05f)
        for (int tier = 0; tier < 3; ++tier) {
            const float v = (tier + 0.98f) / 3.0f, y = tb - th + th * v;
            const float half = treeHalf(v) * 0.9f;
            cv_.capsule(tx - half, y, 0.55f * k, tx + half, y, 0.55f * k, lit({0.9f, 0.93f, 0.98f}, 0.2f), L.snow_cover * 0.75f);
        }
    static const Rgb bulbs[] = {{1.0f, 0.82f, 0.45f}, {1.0f, 0.25f, 0.18f}, {1.0f, 0.82f, 0.45f}, {0.35f, 0.55f, 1.0f}, {1.0f, 0.75f, 0.2f}};
    for (int g = 0; g < 6; ++g) {
        const float v0 = 0.12f + 0.145f * g;
        const int count = 5 + g * 2;
        for (int b = 0; b < count; ++b) {
            const float u = (b + 0.5f) / count;
            // A garland swoops down across the tree.
            const float v = v0 + 0.05f * u + 0.03f * std::sin(u * kPi);
            const float half = treeHalf(v) * 0.85f;
            const float bx = tx - half + 2.0f * half * u, by = tb - th + th * v;
            const float tw = 0.6f + 0.4f * std::sin(t_ * (1.5f + 0.3f * g) + b * 1.9f + g) + 0.5f * beat;
            const Rgb c = bulbs[(b + g) % 5];
            cv_.set(static_cast<int>(bx), static_cast<int>(by), mix(c, {1, 1, 1}, 0.2f));
            cv_.glow(bx, by, 2.5f * k, c, 0.22f * std::max(0.2f, tw));
        }
    }
    const float sy = tb - th - 1.5f * k;
    cv_.glow(tx, sy, 9 * k, {1.0f, 0.85f, 0.4f}, 0.45f + 0.3f * beat);
    for (int i = 0; i < 5; ++i) {
        const float a = -kPi / 2 + kTau * i / 5.0f;
        cv_.capsule(tx, sy, 0.6f * k, tx + std::cos(a) * 2.8f * k, sy + std::sin(a) * 2.8f * k, 0.3f * k, {1.0f, 0.88f, 0.45f});
    }

    // Stalls: plank huts with lit counters, garlands and a snowy roof.
    auto stall = [&](float x0, float x1, float base, uint32_t seed, bool mulled) {
        const float top = base - 18 * k;
        const float cx = (x0 + x1) * 0.5f;
        const Rgb wood = lit({0.40f, 0.26f, 0.16f}), woodDark = lit({0.28f, 0.18f, 0.11f});
        const Rgb glowC{1.0f, 0.70f, 0.35f};
        for (int y = static_cast<int>(top); y < static_cast<int>(base); ++y)
            for (int x = static_cast<int>(x0); x < static_cast<int>(x1); ++x) {
                const bool plank = static_cast<int>((x - x0) / (3 * k)) % 2 == 0;
                Rgb c = plank ? wood : woodDark;
                // Inside lit warm above the counter.
                if (y < base - 7 * k && y > top + 3 * k && x > x0 + 2 * k && x < x1 - 2 * k)
                    c = mix(scale(c, 0.6f), {0.95f, 0.62f, 0.30f}, 0.55f + 0.2f * L.lamp());
                cv_.set(x, y, c);
            }
        // Goods: gingerbread hearts hanging, or mugs for mulled wine.
        for (int i = 0; i < 6; ++i) {
            const float gx = x0 + (4.0f + i * (x1 - x0 - 8.0f) / 5.0f);
            if (mulled) {
                cv_.ellipse(gx, base - 8.5f * k, 1.1f * k, 1.3f * k, 0, i % 2 ? Rgb{0.80f, 0.15f, 0.15f} : Rgb{0.95f, 0.92f, 0.85f});
            } else {
                const float gy = top + (6.0f + 2.0f * (i % 2)) * k;
                cv_.capsule(gx, top + 3 * k, 0.15f * k, gx, gy - 1 * k, 0.15f * k, {0.9f, 0.85f, 0.7f}, 0.6f);
                cv_.ellipse(gx - 0.6f * k, gy, 0.9f * k, 0.9f * k, 0, {0.62f, 0.34f, 0.16f});
                cv_.ellipse(gx + 0.6f * k, gy, 0.9f * k, 0.9f * k, 0, {0.62f, 0.34f, 0.16f});
                cv_.ellipse(gx, gy + 0.9f * k, 0.9f * k, 0.9f * k, 0, {0.62f, 0.34f, 0.16f});
                cv_.set(static_cast<int>(gx), static_cast<int>(gy), {0.95f, 0.30f, 0.35f});
            }
        }
        // Counter.
        for (int y = static_cast<int>(base - 7 * k); y < static_cast<int>(base - 5.5f * k); ++y)
            for (int x = static_cast<int>(x0 - 1 * k); x < static_cast<int>(x1 + 1 * k); ++x) cv_.set(x, y, lit({0.52f, 0.36f, 0.22f}));
        // Roof with snow and a garland.
        for (int y = static_cast<int>(top - 7 * k); y < static_cast<int>(top + 1); ++y) {
            const float f = (y - (top - 7 * k)) / (7 * k);
            const float half = (x1 - x0) * 0.5f * (0.45f + 0.62f * f) + 1.5f * k;
            for (int x = static_cast<int>(cx - half); x <= static_cast<int>(cx + half); ++x)
                cv_.set(x, y, mix(lit({0.34f, 0.20f, 0.12f}), lit({0.92f, 0.94f, 0.98f}), L.snow_cover * (f < 0.45f ? 1.0f : 0.3f)));
        }
        Environment::paint_string_lights(s, L, {x0 - 1.5f * k, top + 0.5f * k}, {x1 + 1.5f * k, top + 0.5f * k}, 2.0f * k,
                                         t_ * (1.0f + beat), 9, seed);
        cv_.glow(cx, base - 6 * k, 22 * k, glowC, 0.18f * L.lamp());
    };
    stall(3 * k, 38 * k, H * 0.86f, 4, false);
    stall(90 * k, 125 * k, H * 0.86f, 5, true);

    // Steam from the mulled wine.
    steamCarry_ += rdt * 2.5f;
    while (steamCarry_ >= 1.0f) {
        steamCarry_ -= 1.0f;
        steam_.push_back({(96.0f + rnd() * 24.0f) * k, H * 0.86f - 9 * k, 1.0f * k, 0.0f});
    }
    for (auto& p : steam_) {
        p.life += rdt;
        p.y -= 4.0f * k * rdt;
        p.x += (std::sin(p.life * 2.0f + p.r) * 1.2f + L.wind * 3.0f) * k * rdt;
        p.r += 0.8f * k * rdt;
        cv_.ellipse(p.x, p.y, p.r, p.r, 0, lit({0.85f, 0.85f, 0.88f}, 0.2f), 0.25f * std::max(0.0f, 1.0f - p.life / 3.0f));
    }
    std::erase_if(steam_, [](const Puff& p) { return p.life > 3.0f; });

    // People strolling in winter coats; some stop at a stall for a while.
    static const Rgb coats[] = {{0.55f, 0.12f, 0.14f}, {0.18f, 0.24f, 0.40f}, {0.30f, 0.32f, 0.26f}, {0.62f, 0.50f, 0.36f}};
    static const Rgb hats[] = {{0.90f, 0.88f, 0.84f}, {0.75f, 0.20f, 0.20f}, {0.20f, 0.45f, 0.40f}, {0.30f, 0.25f, 0.22f}};
    std::sort(people_.begin(), people_.end(), [](const Stroller& a, const Stroller& b) { return a.scale < b.scale; });
    for (auto& p : people_) {
        const bool atStall = p.pause > 0.0f;
        if (atStall)
            p.pause -= dt;
        else {
            p.x += p.dir * p.speed * dt;
            p.phase = std::fmod(p.phase + dt * p.speed / (6.0f * k), 1.0f);
            const bool nearStall = (p.x > 8 * k && p.x < 30 * k) || (p.x > 96 * k && p.x < 120 * k);
            if (nearStall && rnd() < dt * 0.05f)
                p.pause = 5.0f + rnd() * 6.0f;
        }
        if (p.x > W + 10 * k)
            p.x = -10 * k;
        if (p.x < -10 * k)
            p.x = W + 10 * k;
        const float ground = H * (0.93f + 0.06f * (p.scale / k - 1.0f) / 0.18f);
        Hobbies::Look look;
        look.shirt = coats[p.look];
        look.pants = {0.14f, 0.14f, 0.16f};
        look.shoes = {0.10f, 0.08f, 0.08f};
        look.cap = true;
        look.capColor = hats[p.look];
        // Lit by the stalls and the tree, not by the night sky: warm albedo.
        const Hobbies::Look albedo = look;
        light_look(look);
        auto warm = [&](Rgb lit_c, Rgb a) { return mix(lit_c, Rgb{a.r * 0.95f, a.g * 0.78f, a.b * 0.55f}, 0.75f * L.lamp()); };
        look.shirt = warm(look.shirt, albedo.shirt);
        look.skin = warm(look.skin, albedo.skin);
        look.capColor = warm(look.capColor, albedo.capColor);
        look.pants = warm(look.pants, albedo.pants);
        look.hair = warm(look.hair, albedo.hair);
        const Pose pose = atStall ? blend(walk_pose(0.0f, 0.0f, 0.0f), walk_pose(0.25f, 0.1f, 0.0f), 0.2f) : walk_pose(p.phase, 0.32f, 0.04f);
        const float facing = atStall ? (p.x < W * 0.5f ? -1.0f : 1.0f) : p.dir;
        cv_.ellipse(p.x, ground, 4 * p.scale, 0.8f * p.scale, 0, {0.05f, 0.05f, 0.06f}, 0.35f);
        const auto out = figure(cv_, p.x, ground, p.scale, facing, pose, look);
        // A steaming mug in hand for some.
        if (p.look % 2 == 0)
            cv_.ellipse(out.hand[1].x + 0.4f * facing * p.scale, out.hand[1].y, 0.8f * p.scale, 0.9f * p.scale, 0, {0.85f, 0.15f, 0.15f});
    }

    Environment::paint_fog(s, L, t_, horizon_, H, 0.0f);
    paint_weather(rdt, horizon_, [&](float x) { return H * (0.66f + 0.32f * hash01(static_cast<uint32_t>(std::max(0.0f, x) * 5.0f))); });
    present(canvas);
    wait_until_next_frame();
    return true;
}

Scenes::SceneDescriptor ChristmasMarketScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.family = "christmas";
    d.tags = {"christmas", "holidays", "occasion:christmas", "fits:winter", "fits:night", "scenery", "cozy", "city", "calm",
              "audio-reactive"};
    d.intensity = 0.3f;
    d.motion = 0.3f;
    d.music_affinity = 0.25f;
    d.performance_cost = 0.35f;
    environment_->add_variants(d);
    return d;
}

// ================================================================ sleigh night
ChristmasSleighScene::ChristmasSleighScene()
{
    use_environment({Environment::TimeOfDay::Night, Environment::Weather::Clear, Environment::Season::Winter, 0.0f, 0.5f}, winter_night());
}

void ChristmasSleighScene::initialize(int width, int height)
{
    HobbySceneBase::initialize(width, height);
    horizon_ = std::floor(static_cast<float>(height) * 0.62f);
    backdrop_.clear();
    smoke_.clear();
    trail_.clear();
    // Start with the sleigh already on its way so short previews show it.
    sleighT_ = 0.30f;
    sleighWait_ = 0.0f;
}

namespace {
struct SleighHouse {
    float x, w, h;
};
constexpr SleighHouse kHouses[] = {{8, 15, 11}, {27, 13, 10}, {44, 15, 12}, {82, 15, 11}, {100, 12, 9}, {114, 15, 11}};
constexpr float kChurchX = 66;
}  // namespace

void ChristmasSleighScene::build_backdrop()
{
    const auto& L = env();
    const int w = cv_.w, h = cv_.h;
    const float W = static_cast<float>(w), H = static_cast<float>(h), k = W / 128.0f;
    Canvas bg;
    bg.resize(w, h);
    const Environment::Surface s{bg.px.data(), w, h, 1.0f};
    Environment::paint_sky(s, L, horizon_);
    // A big moon low over the village.
    Environment::Look moonLook = L;
    moonLook.moon_phase = std::clamp(L.moon_phase, 0.35f, 0.65f);  // keep it round enough to frame the sleigh
    Environment::paint_moon(s, moonLook, {W * 0.70f, H * 0.24f}, 11.0f * k);

    const Rgb snowFar = lit({0.78f, 0.82f, 0.92f}, 0.6f), snowNear = lit({0.88f, 0.90f, 0.98f}, 0.1f);
    bg.fill_below([&](float x) { return horizon_ - 10 * k - hill(x, 2.7f) * 10 * k; }, mix(snowFar, from_env(L.haze), 0.3f));
    for (float x = 0; x < W; x += 2.6f * k) {
        const uint32_t id = static_cast<uint32_t>(x * 31.0f);
        pine(bg, x, horizon_ - 9 * k - hill(x, 2.7f) * 10 * k + 2 * k, (4.0f + 3.0f * hash01(id)) * k,
             mix(lit({0.10f, 0.20f, 0.16f}, 0.6f), from_env(L.haze), 0.3f));
    }
    // Village slope.
    auto slope = [&](float x) { return horizon_ + 2.0f * k * std::sin(x / W * 3.0f + 0.4f); };
    bg.fill_below(slope, lit({0.82f, 0.86f, 0.95f}, 0.3f));
    // Church with a spire.
    const Rgb wall = lit({0.62f, 0.58f, 0.56f}, 0.3f), roof = lit({0.92f, 0.94f, 0.98f}, 0.3f);
    const float cx = kChurchX * k, cb = slope(cx) + 1;
    for (int y = static_cast<int>(cb - 12 * k); y < static_cast<int>(cb); ++y)
        for (int x = static_cast<int>(cx - 7 * k); x < static_cast<int>(cx + 7 * k); ++x) bg.set(x, y, wall);
    for (int y = static_cast<int>(cb - 20 * k); y < static_cast<int>(cb - 12 * k); ++y)
        for (int x = static_cast<int>(cx - 2.5f * k); x < static_cast<int>(cx + 2.5f * k); ++x) bg.set(x, y, wall);
    for (int y = static_cast<int>(cb - 30 * k); y < static_cast<int>(cb - 20 * k); ++y) {
        const float half = 3.0f * k * (y - (cb - 30 * k)) / (10 * k);
        for (int x = static_cast<int>(cx - half); x <= static_cast<int>(cx + half); ++x) bg.set(x, y, lit({0.25f, 0.32f, 0.38f}, 0.3f));
    }
    for (int y = static_cast<int>(cb - 16 * k); y < static_cast<int>(cb - 12 * k); ++y)
        for (int x = static_cast<int>(cx - 7.5f * k); x < static_cast<int>(cx + 7.5f * k); ++x) {
            const float d = 7.5f * k - std::fabs(x + 0.5f - cx) - (cb - 12 * k - y) * 1.8f;
            if (d > 0)
                bg.set(x, y, roof);
        }
    // Cottages with snowy roofs.
    for (const auto& hs : kHouses) {
        const float x0 = hs.x * k, x1 = (hs.x + hs.w) * k, base = slope((x0 + x1) * 0.5f) + 1;
        const float top = base - hs.h * k;
        for (int y = static_cast<int>(top); y < static_cast<int>(base); ++y)
            for (int x = static_cast<int>(x0); x < static_cast<int>(x1); ++x) bg.set(x, y, lit({0.50f, 0.38f, 0.30f}, 0.3f));
        for (int y = static_cast<int>(top - hs.w * 0.4f * k); y < static_cast<int>(top + 1); ++y) {
            const float f = (y - (top - hs.w * 0.4f * k)) / (hs.w * 0.4f * k);
            const float half = (hs.w * 0.5f + 1.2f) * k * f;
            for (int x = static_cast<int>((x0 + x1) * 0.5f - half); x <= static_cast<int>((x0 + x1) * 0.5f + half); ++x)
                bg.set(x, y, roof);
        }
    }
    // Foreground: a snowy rise with a fence and a few firs.
    auto fg = [&](float x) { return H * 0.84f - 6.0f * k * std::sin(x / W * 2.6f + 0.3f); };
    for (int x = 0; x < w; ++x) {
        const float top = fg(x + 0.5f);
        for (int y = static_cast<int>(top); y < h; ++y) {
            const float v = (y - top) / (H - top + 1);
            bg.blend(x, y, mix(snowNear, lit({0.62f, 0.68f, 0.86f}), 0.2f + 0.4f * v), y == static_cast<int>(top) ? 0.7f : 1.0f);
        }
    }
    const Rgb fence = lit({0.30f, 0.22f, 0.16f});
    for (float x = 2 * k; x < W * 0.55f; x += 7 * k) bg.capsule(x, fg(x) + 2 * k, 0.6f * k, x, fg(x) - 5 * k, 0.5f * k, fence);
    bg.capsule(0, fg(0) - 3 * k, 0.4f * k, W * 0.55f, fg(W * 0.55f) - 3 * k, 0.4f * k, fence);
    for (float px : {104.0f, 116.0f, 124.0f}) {
        pine(bg, px * k, fg(px * k) + 2 * k, (14.0f + 4.0f * hash01(static_cast<uint32_t>(px))) * k, lit({0.08f, 0.22f, 0.14f}));
        for (int i = 0; i < 3; ++i) {
            const float y = fg(px * k) - (4.0f + 4.5f * i) * k;
            bg.capsule(px * k - (4 - i) * k, y, 0.5f * k, px * k + (4 - i) * k, y, 0.5f * k, snowNear, 0.7f);
        }
    }
    backdrop_ = std::move(bg.px);
}

bool ChristmasSleighScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt;
    begin_frame(dt);
    const float rdt = dt / std::max(0.05f, speed_->get());
    const auto& L = env();
    const float W = static_cast<float>(cv_.w), H = static_cast<float>(cv_.h), k = W / 128.0f;
    if (environment_changed(backdropKey_) || backdrop_.empty())
        build_backdrop();
    cv_.px = backdrop_;
    const auto s = surface();
    Environment::paint_stars(s, L, horizon_ * 0.7f, t_, 13, 90);

    // Lit windows and chimney smoke in the village.
    auto slope = [&](float x) { return horizon_ + 2.0f * k * std::sin(x / W * 3.0f + 0.4f); };
    int n = 0;
    for (const auto& hs : kHouses) {
        const float x0 = hs.x * k, base = slope((hs.x + hs.w * 0.5f) * k) + 1;
        for (int i = 0; i < 2; ++i)
            Environment::paint_window(s, L, static_cast<int>(x0 + (2.0f + i * (hs.w - 6.0f)) * k), static_cast<int>(base - hs.h * k + 3 * k),
                                      static_cast<int>(3 * k), static_cast<int>(3 * k), static_cast<uint32_t>(n * 5 + i + 2), t_);
        ++n;
    }
    const float cb = slope(kChurchX * k) + 1;
    for (int i = 0; i < 3; ++i)
        Environment::paint_window(s, L, static_cast<int>((kChurchX - 5 + i * 4) * k), static_cast<int>(cb - 9 * k), static_cast<int>(2 * k),
                                  static_cast<int>(4 * k), 40, t_);
    smokeCarry_ += rdt * 1.6f;
    while (smokeCarry_ >= 1.0f) {
        smokeCarry_ -= 1.0f;
        const auto& hs = kHouses[static_cast<int>(rnd() * 5.99f)];
        const float x = (hs.x + hs.w * 0.75f) * k;
        smoke_.push_back({x, slope(x) - (hs.h + hs.w * 0.4f) * k, 0.8f * k, 0.0f});
    }
    for (auto& p : smoke_) {
        p.life += rdt;
        p.y -= 3.0f * k * rdt;
        p.x += (L.wind * 4.0f + 0.8f) * k * rdt;
        p.r += 0.5f * k * rdt;
        cv_.ellipse(p.x, p.y, p.r, p.r, 0, lit({0.80f, 0.82f, 0.90f}, 0.2f), 0.35f * std::max(0.0f, 1.0f - p.life / 5.0f));
    }
    std::erase_if(smoke_, [](const Puff& p) { return p.life > 5.0f; });

    // The sleigh: an arc across the sky that passes in front of the moon.
    if (sleighT_ < 0.0f) {
        sleighWait_ -= rdt;
        if (sleighWait_ <= 0.0f && L.fog < 0.5f)
            sleighT_ = 0.0f;
    } else {
        sleighT_ += rdt / 14.0f;
        const float u = sleighT_;
        const float sx = -30.0f * k + (W + 60.0f * k) * u;
        const float sy = H * (0.36f - 0.22f * std::sin(u * kPi)) + 1.5f * k * std::sin(t_ * 3.0f);
        const float q = 1.7f * k;  // sleigh scale
        trail_.push_back({sx - 5 * q, sy + 1 * q, 0.0f});
        // Sleigh body: a curled runner, a red box, a sack and the driver.
        cv_.capsule(sx - 5 * q, sy + 2.2f * q, 0.35f * q, sx + 3 * q, sy + 2.2f * q, 0.35f * q, {0.75f, 0.62f, 0.30f});
        cv_.capsule(sx + 3 * q, sy + 2.2f * q, 0.35f * q, sx + 4.4f * q, sy + 0.6f * q, 0.3f * q, {0.75f, 0.62f, 0.30f});
        cv_.capsule(sx - 5 * q, sy + 2.2f * q, 0.35f * q, sx - 5.6f * q, sy + 1.0f * q, 0.3f * q, {0.75f, 0.62f, 0.30f});
        cv_.ellipse(sx - 1 * q, sy, 3.8f * q, 1.7f * q, 0, {0.62f, 0.08f, 0.08f});
        cv_.capsule(sx - 4.6f * q, sy - 1.2f * q, 0.6f * q, sx - 4.6f * q, sy + 0.8f * q, 0.6f * q, {0.55f, 0.06f, 0.06f});
        cv_.ellipse(sx - 3.0f * q, sy - 2.4f * q, 1.8f * q, 1.6f * q, 0, {0.52f, 0.40f, 0.24f});
        cv_.ellipse(sx + 0.4f * q, sy - 2.4f * q, 1.2f * q, 1.6f * q, 0, {0.70f, 0.10f, 0.10f});
        cv_.ellipse(sx + 0.6f * q, sy - 4.4f * q, 0.8f * q, 0.8f * q, 0, {0.85f, 0.62f, 0.50f});
        cv_.ellipse(sx + 0.3f * q, sy - 4.0f * q, 0.9f * q, 0.6f * q, 0, {0.95f, 0.95f, 0.95f});
        cv_.capsule(sx + 0.2f * q, sy - 5.0f * q, 0.7f * q, sx - 0.8f * q, sy - 5.8f * q, 0.3f * q, {0.70f, 0.10f, 0.10f});
        cv_.glow(sx, sy - 1 * q, 10 * q, {1.0f, 0.85f, 0.55f}, 0.10f);
        // Reindeer in a line ahead, galloping.
        const Rgb coat{0.42f, 0.30f, 0.20f}, belly{0.62f, 0.50f, 0.38f}, dark{0.22f, 0.15f, 0.10f};
        for (int r = 0; r < 3; ++r) {
            const float rx = sx + (8.5f + 6.5f * r) * q, ry = sy - (0.6f + 0.5f * r) * q + 0.5f * q * std::sin(t_ * 9.0f + r);
            cv_.capsule(sx + 4 * q, sy, 0.12f * q, rx - 1.0f * q, ry - 1.0f * q, 0.12f * q, {0.75f, 0.62f, 0.30f}, 0.7f);
            deer(cv_, rx, ry + 3.2f * q, 0.42f * q, 1.0f, t_ * 2.4f + r * 0.25f, 1.0f, 0.0f, coat, belly, dark);
            if (r == 2)
                cv_.glow(rx + 2.8f * q, ry - 1.8f * q, 1.6f * q, {1.0f, 0.2f, 0.15f}, 0.9f);  // a red nose leads the way
        }
        if (sleighT_ >= 1.0f) {
            sleighT_ = -1.0f;
            sleighWait_ = 18.0f + rnd() * 14.0f;
        }
    }
    for (auto& p : trail_) {
        p.life += rdt;
        const float a = std::max(0.0f, 1.0f - p.life / 1.6f);
        cv_.add(static_cast<int>(p.x + (hash01(static_cast<uint32_t>(p.x * 13)) - 0.5f) * 2 * k), static_cast<int>(p.y + p.life * 3 * k),
                {1.0f, 0.90f, 0.55f}, a * 0.8f);
    }
    std::erase_if(trail_, [](const Spark& p) { return p.life > 1.6f; });

    // The middle foreground fir is decorated: a few strands of lights and a star.
    {
        const float px = 116.0f, fx = px * k;
        const float base = H * 0.84f - 6.0f * k * std::sin(fx / W * 2.6f + 0.3f) + 2 * k;
        const float th = (14.0f + 4.0f * hash01(static_cast<uint32_t>(px))) * k;
        static const Rgb bulbs[] = {{1.0f, 0.80f, 0.42f}, {1.0f, 0.28f, 0.20f}, {0.40f, 0.60f, 1.0f}};
        for (int g = 0; g < 4; ++g)
            for (int b = 0; b < 3 + g; ++b) {
                const float u = (b + 0.5f) / (3 + g), v = 0.22f + 0.2f * g + 0.06f * u;
                const float half = th * 0.36f * (0.25f + 0.75f * v) * (0.55f + 0.45f * fract(v * 3.0f)) * 0.8f;
                const float bx = fx - half + 2 * half * u, by = base - th + th * v;
                const float tw = 0.55f + 0.45f * std::sin(t_ * 1.7f + b * 2.1f + g);
                cv_.set(static_cast<int>(bx), static_cast<int>(by), bulbs[(b + g) % 3]);
                cv_.glow(bx, by, 2.0f * k, bulbs[(b + g) % 3], 0.25f * tw);
            }
        cv_.glow(fx, base - th - 0.5f * k, 4 * k, {1.0f, 0.85f, 0.4f}, 0.7f);
    }

    // Lantern on the fence post.
    const float lx = 30 * k, ly = H * 0.84f - 6.0f * k * std::sin(30 * k / W * 2.6f + 0.3f) - 8 * k;
    cv_.capsule(lx, ly + 2 * k, 0.5f * k, lx, ly + 8 * k, 0.5f * k, lit({0.30f, 0.22f, 0.16f}));
    cv_.ellipse(lx, ly, 1.2f * k, 1.6f * k, 0, {1.0f, 0.82f, 0.45f});
    cv_.glow(lx, ly, 14 * k, {1.0f, 0.72f, 0.35f}, 0.35f * L.lamp());

    Environment::paint_fog(s, L, t_, horizon_ - 16 * k, H, 0.0f);
    paint_weather(rdt, horizon_, [&](float x) { return H * 0.84f - 6.0f * k * std::sin(x / W * 2.6f + 0.3f); });
    present(canvas);
    wait_until_next_frame();
    return true;
}

Scenes::SceneDescriptor ChristmasSleighScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.family = "christmas";
    d.tags = {"christmas", "holidays", "occasion:christmas", "fits:winter", "fits:night", "scenery", "landscape", "night",
              "calm", "moon"};
    d.intensity = 0.15f;
    d.motion = 0.2f;
    d.music_affinity = 0.05f;
    d.performance_cost = 0.25f;
    environment_->add_variants(d);
    return d;
}
