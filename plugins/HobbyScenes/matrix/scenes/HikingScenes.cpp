#include "HikingScenes.h"

using namespace Hobbies;

namespace {
// Fold any tempo into a comfortable walking range (one step per beat).
float walk_cycles_per_second(const AudioPulse& a, float calm)
{
    if (a.bpm <= 0.0f)
        return calm;
    float b = a.bpm;
    while (b > 140.0f) b *= 0.5f;
    while (b < 70.0f) b *= 2.0f;
    return calm + (b / 120.0f - calm) * std::min(1.0f, a.amount);
}

void bird(Canvas& cv, float x, float y, float s, float flap, Rgb c)
{
    const float lift = (0.4f + 0.9f * flap) * s;
    cv.capsule(x - 2.2f * s, y - lift, 0.25f, x, y, 0.4f, c);
    cv.capsule(x, y, 0.4f, x + 2.2f * s, y - lift, 0.25f, c);
}
}  // namespace

// ================================================================ ridge trail
void RidgeTrailScene::initialize(int width, int height)
{
    AudioHobbyScene::initialize(width, height);
    birds_.clear();
    scroll_ = walk_ = 0.0f;
    nextFlock_ = 12.0f + rnd() * 10.0f;
}

void RidgeTrailScene::spawn_flock(float W, float H, float k)
{
    const int n = 5 + static_cast<int>(rnd() * 4);
    const float y0 = H * (0.14f + rnd() * 0.16f);
    for (int i = 0; i < n; ++i) {
        // Loose V formation entering from the right.
        const float row = static_cast<float>((i + 1) / 2);
        const float side = i % 2 == 0 ? 1.0f : -1.0f;
        birds_.push_back({W + 6 * k + row * 5 * k, y0 + side * row * 2.5f * k, rnd(), 13.0f * k});
    }
}

RidgeTrailScene::RidgeTrailScene()
{
    use_environment({Environment::TimeOfDay::Dusk, Environment::Weather::Clear, Environment::Season::Summer});
}

bool RidgeTrailScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt, rdt;
    begin_audio_frame(dt, rdt);
    const auto& L = env();
    const float W = static_cast<float>(cv_.w), H = static_cast<float>(cv_.h), k = W / 128.0f;
    const float am = std::min(1.0f, audio_.amount);
    const float s = 1.45f * k;  // hiker scale
    // Wet, snowy or stormy ground slows the hiker down a little.
    const float pace = 1.0f - 0.18f * std::max(L.snow_cover, L.rain);

    const float cycles = walk_cycles_per_second(audio_, 0.62f) * pace;
    walk_ += dt * cycles;
    // One walk cycle covers two steps of ~6.5 units each.
    scroll_ += dt * cycles * 13.0f * s;

    // Sky, sun/moon, stars and clouds follow the time of day and weather.
    const float horizon = H * 0.70f;
    paint_sky_layers(horizon, scroll_ * 0.03f, 8.5f);
    const auto sun = Environment::sun_point(L, W, horizon);
    if (L.sun_elevation > -2.0f)
        cv_.glow(sun.x, sun.y, 34 * k, from_env(L.sun_color), (0.45f * audio_.beat + 0.25f * audio_.drop) * am * (1.0f - L.overcast));

    // Birds (periodic, and on drops) - they stay home in the night and in storms.
    nextFlock_ -= dt;
    const bool birdWeather = L.night < 0.6f && L.storm < 0.3f && L.rain < 0.5f;
    if (birdWeather && (nextFlock_ <= 0.0f || (audio_.dropHit && birds_.size() < 10))) {
        spawn_flock(W, H, k);
        nextFlock_ = 25.0f + rnd() * 20.0f;
    }
    const Rgb birdCol = lit({0.22f, 0.16f, 0.20f}, 0.5f);
    for (auto& b : birds_) {
        b.x -= b.speed * dt;
        b.phase += dt * (1.6f + 1.5f * audio_.level * am);
        bird(cv_, b.x, b.y + std::sin(b.phase * 1.3f) * 0.8f * k, k, 0.5f + 0.5f * std::sin(b.phase * kTau), birdCol);
    }
    std::erase_if(birds_, [&](const Bird& b) { return b.x < -12 * k; });

    // Far and mid ridges fade into the haze; pines stand on the mid ridge.
    const Rgb farCol = mix(terrain({0.50f, 0.50f, 0.62f}, 0.80f, 0.8f), lit({0.78f, 0.50f, 0.52f}, 0.8f), 0.3f * audio_.level * am);
    hills(cv_, scroll_ * 0.08f, H * 0.64f, 30 * k, 1.3f, farCol);
    const float midScroll = scroll_ * 0.28f, midBase = H * 0.76f, midAmp = 28 * k;
    hills(cv_, midScroll, midBase, midAmp, 3.7f, plant({0.34f, 0.42f, 0.36f}, 0.5f, 0.8f, 0.55f));
    Environment::paint_fog(surface(), L, t_, H * 0.48f, H * 0.80f);
    {
        const Rgb pineCol = terrain({0.16f, 0.27f, 0.22f}, 0.38f, 0.35f);
        const float spacing = 7.0f * k;
        const int first = static_cast<int>(std::floor((midScroll - 8 * k) / spacing));
        for (int i = first; i * spacing < midScroll + W + 8 * k; ++i) {
            const float h = hash01(static_cast<uint32_t>(i) * 2654435761u);
            if (h < 0.45f)
                continue;
            const float wx = i * spacing + h * 3 * k;
            const float x = wx - midScroll;
            const float base = midBase - hill(wx, 3.7f) * midAmp + 2 * k;
            if (h > 0.86f)  // now and then a broadleaf tree that shows the season
                Environment::paint_tree(surface(), L, x, base, (10 + 6 * h) * k, static_cast<uint32_t>(i) * 31u, t_, 0.38f);
            else
                pine(cv_, x, base, (6 + 6 * h) * k, pineCol);
        }
    }

    // Foreground trail ridge: grass changes with the season, snow settles on
    // it, the path turns dark and glossy when wet.
    const float nearScroll = scroll_;
    auto groundAt = [&](float x) { return H * 0.86f - hill((x + nearScroll) * 0.55f, 7.0f) * 15 * k; };
    const Rgb edgeC = meadow({0.52f, 0.62f, 0.26f});
    // The trodden path keeps little snow, and moonlight picks it out at night.
    Rgb pathC = terrain(mix({0.66f, 0.50f, 0.34f}, {0.36f, 0.28f, 0.22f}, L.wetness), 0.0f, 0.35f);
    const float moonlit = std::max(L.night, 0.6f * L.storm);
    pathC = {pathC.r + 0.10f * moonlit, pathC.g + 0.10f * moonlit, pathC.b + 0.13f * moonlit};
    const Rgb pathEdge = scale(pathC, 1.25f);
    const Rgb grassTop = meadow({0.34f, 0.48f, 0.20f}), grassDeep = meadow({0.16f, 0.26f, 0.12f}, 0.0f, 0.9f);
    for (int x = 0; x < cv_.w; ++x) {
        const float top = groundAt(x + 0.5f);
        const int y0 = std::max(0, static_cast<int>(std::floor(top)));
        for (int y = y0; y < cv_.h; ++y) {
            const float d = (y + 0.5f) - top;  // depth below the surface
            Rgb c;
            if (d < 1.2f)
                c = edgeC;
            else if (d > 1.8f * k && d < 2.8f * k)
                c = pathEdge;  // the worn upper edge catches the light
            else if (d >= 2.8f * k && d < 6.6f * k)
                c = pathC;
            else
                c = mix(grassTop, grassDeep, d / (H * 0.3f));
            cv_.blend(x, y, c, y == y0 ? 1.0f - (top - std::floor(top)) : 1.0f);
        }
    }
    // Wildflowers in spring and summer, fallen leaves in autumn; with music
    // they glow with the treble. Nothing pokes out of deep snow.
    {
        static constexpr Rgb kFlower[] = {{1.0f, 0.85f, 0.30f}, {0.95f, 0.45f, 0.65f}, {0.70f, 0.55f, 1.0f}, {1.0f, 0.96f, 0.92f}};
        static constexpr Rgb kLeaf[] = {{0.85f, 0.35f, 0.10f}, {0.95f, 0.62f, 0.15f}, {0.65f, 0.20f, 0.10f}, {0.80f, 0.50f, 0.18f}};
        const float flowers = (L.spring * 1.2f + L.summer) * (1.0f - L.snow_cover);
        const float leaves = L.autumn * (1.0f - L.snow_cover);
        const float spacing = 3.0f * k;
        const int first = static_cast<int>(std::floor(nearScroll / spacing)) - 1;
        const float bloom = 0.65f + 0.9f * audio_.treble * am + 0.4f * audio_.onset * am;
        for (int i = first; i * spacing < nearScroll + W + spacing; ++i) {
            const uint32_t id = static_cast<uint32_t>(i) * 2246822519u;
            if (hash01(id) < 0.45f)
                continue;
            const float x = i * spacing - nearScroll + hash01(id + 1) * spacing;
            const float y = groundAt(x) + (5.5f + hash01(id + 2) * 12.0f) * k;
            const int pick = static_cast<int>(hash01(id + 3) * 3.99f);
            if (flowers > 0.02f) {
                cv_.blend(static_cast<int>(x), static_cast<int>(y), lit(kFlower[pick]), std::min(1.0f, flowers) * 0.85f);
                cv_.add(static_cast<int>(x), static_cast<int>(y), kFlower[pick], 0.25f * flowers * (bloom - 0.65f) * (1.0f - 0.5f * L.night));
                cv_.blend(static_cast<int>(x), static_cast<int>(y) - 1, meadow({0.30f, 0.55f, 0.20f}), 0.4f * flowers);
            }
            if (leaves > 0.02f)
                cv_.blend(static_cast<int>(x), static_cast<int>(y), lit(kLeaf[pick]), leaves * 0.9f);
        }
    }

    // The hiker, leaning into climbs, with a trekking pole. A rain jacket in
    // the wet, a beanie in the snow and a headlamp after dark.
    const float hx = W * 0.36f;
    const float slope = (groundAt(hx + 3 * k) - groundAt(hx - 3 * k)) / (6 * k);
    const float lean = std::clamp(0.06f - slope * 0.7f, -0.12f, 0.35f);
    Look look;
    look.backpack = true;
    look.cap = true;
    look.shirt = L.rain > 0.25f ? Rgb{0.95f, 0.78f, 0.12f} : Rgb{0.78f, 0.26f, 0.18f};
    look.pants = {0.26f, 0.28f, 0.22f};
    look.capColor = L.snow_cover > 0.5f || L.winter > 0.5f ? Rgb{0.80f, 0.20f, 0.25f} : Rgb{0.20f, 0.36f, 0.50f};
    look.pack = {0.22f, 0.40f, 0.30f};
    light_look(look);
    const float phase = fract(walk_);
    const Pose pose = walk_pose(phase, 0.40f + 0.06f * audio_.bass * am, lean);
    const FigureOut out = figure(cv_, hx + 2.0f * k, groundAt(hx + 2.0f * k) + 0.6f * k, s, 1.0f, pose, look);
    // Pole: planted when the near arm is forward (phase 0.25), the tip then
    // stays fixed on the ground while the hiker walks past it, then lifts and
    // swings forward to the next plant.
    constexpr float kPlanted = 0.55f;
    const float poleU = fract(phase - 0.22f);
    const float travel = kPlanted * 13.0f * s;  // ground covered while planted
    const float front = hx + 6.0f * k, back = front - travel;
    float tipX, tipLift = 0.0f;
    if (poleU < kPlanted)
        tipX = front - travel * (poleU / kPlanted);
    else {
        const float v = (poleU - kPlanted) / (1.0f - kPlanted);
        tipX = back + (front - back) * smoothstep(0.0f, 1.0f, v);
        tipLift = std::sin(v * kPi) * 2.5f * k;
    }
    cv_.capsule(out.hand[1].x, out.hand[1].y - 1.0f * k, 0.25f * k, tipX, groundAt(tipX) + 1.0f * k - tipLift, 0.25f * k,
                lit({0.70f, 0.72f, 0.78f}));
    if (L.lamp() > 0.05f) {
        // Headlamp: a bright dot and a soft cone onto the trail ahead.
        const float lamp = L.lamp();
        const Point head = out.head;
        cv_.add(static_cast<int>(head.x + 1.5f * k), static_cast<int>(head.y - 0.5f * k), {1.0f, 0.97f, 0.85f}, lamp);
        for (int i = 1; i < 10; ++i) {
            const float u = i / 10.0f;
            const float bx = head.x + (2.0f + 24.0f * u) * k;
            const float by = head.y + (groundAt(bx) - head.y) * std::min(1.0f, u * 1.1f);
            cv_.glow(bx, by, (1.5f + 4.0f * u) * k, {1.0f, 0.92f, 0.70f}, 0.10f * lamp * (1.0f - 0.6f * u));
        }
        cv_.glow(head.x + 26.0f * k, groundAt(head.x + 26.0f * k), 7.0f * k, {1.0f, 0.90f, 0.65f}, 0.18f * lamp);
    }

    paint_weather(rdt, horizon, groundAt);
    present(canvas);
    wait_until_next_frame();
    return true;
}

Scenes::SceneDescriptor RidgeTrailScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.family = "hiking";
    d.tags = {"hiking", "hills", "scenery", "landscape", "sunset", "calm", "audio-reactive"};
    d.intensity = 0.3f;
    d.motion = 0.5f;
    d.music_affinity = 0.45f;
    d.performance_cost = 0.3f;
    environment_->add_variants(d);
    return d;
}

// ================================================================ summit night
void SummitNightScene::initialize(int width, int height)
{
    AudioHobbyScene::initialize(width, height);
    const float W = static_cast<float>(width), H = static_cast<float>(height);
    stars_.clear();
    for (int i = 0; i < 120; ++i)
        stars_.push_back({rnd() * W, rnd() * H * 0.62f, rnd() < 0.15f ? 1.0f : 0.45f, rnd() * kTau, 0.5f + rnd() * 1.8f});
    village_.clear();
    for (int i = 0; i < 16; ++i) {
        const float cluster = i < 10 ? 0.70f : 0.88f;
        village_.push_back({W * (cluster + (rnd() - 0.5f) * 0.16f), H * (0.775f + rnd() * 0.04f), rnd() * kTau, rnd()});
    }
    shootLife_ = 0.0f;
    nextShoot_ = 7.0f;
}

SummitNightScene::SummitNightScene()
{
    // Its identity is the night: only the weather and season follow reality.
    Environment::Adapt adapt;
    adapt.time = false;
    adapt.season_variants = {Environment::Season::Winter};
    // Fog and thunderstorms would bury the aurora the scene is about.
    adapt.weather_allowed = {Environment::Weather::Clear, Environment::Weather::PartlyCloudy, Environment::Weather::Overcast,
                             Environment::Weather::Drizzle, Environment::Weather::Rain, Environment::Weather::Snow};
    use_environment({Environment::TimeOfDay::Night, Environment::Weather::Clear, Environment::Season::Summer}, adapt);
}

bool SummitNightScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt, rdt;
    begin_audio_frame(dt, rdt);
    const auto& L = env();
    const float W = static_cast<float>(cv_.w), H = static_cast<float>(cv_.h), k = W / 128.0f;
    const float am = std::min(1.0f, audio_.amount);
    // Clouds and fog hide the sky show; snow whitens the dark land.
    const float clearSky = (1.0f - 0.92f * L.overcast) * (1.0f - 0.8f * L.fog);
    const Rgb nightSnow{0.26f, 0.29f, 0.42f};
    auto snowy = [&](Rgb c, float exposure) { return mix(c, nightSnow, L.snow_cover * exposure); };

    fill_gradient({0.01f, 0.02f, 0.07f}, {0.04f, 0.06f, 0.17f}, {0.10f, 0.10f, 0.24f}, 0.62f);
    if (L.overcast > 0.01f || L.fog > 0.01f) {
        // Under cloud the sky is a flat dark grey-blue lit faintly by the village.
        const Rgb grey{0.07f, 0.075f, 0.11f};
        for (int y = 0; y < static_cast<int>(H * 0.75f); ++y)
            for (int x = 0; x < cv_.w; ++x)
                cv_.blend(x, y, mix(grey, {0.12f, 0.11f, 0.14f}, y / (H * 0.75f)), std::max(L.overcast, L.fog) * 0.9f);
    }

    // Aurora curtain: faint without music, follows the mids while it plays.
    const float aurora = (0.10f + 0.55f * audio_.mid * am + 0.35f * audio_.drop * am) * clearSky;
    for (int x = 0; x < cv_.w; ++x) {
        const float cy = H * (0.20f + 0.05f * std::sin(x * 0.05f + t_ * 0.25f) + 0.025f * std::sin(x * 0.13f - t_ * 0.45f));
        const float col = aurora * (0.45f + 0.55f * (0.5f + 0.5f * std::sin(x * 0.09f + t_ * 0.6f)));
        const int top = static_cast<int>(cy - 16 * k), bottom = static_cast<int>(cy + 3 * k);
        for (int y = std::max(0, top); y < std::min(cv_.h, bottom); ++y) {
            const float v = (y - cy);
            const float a = v < 0 ? 1.0f + v / (16 * k) : 1.0f - v / (3 * k);
            cv_.add(x, y, mix({0.20f, 0.95f, 0.55f}, {0.55f, 0.30f, 0.95f}, -v / (16 * k)), col * a * a);
        }
    }

    for (auto& st : stars_) {
        const float tw = 0.35f + 0.65f * (0.5f + 0.5f * std::sin(t_ * st.rate + st.phase));
        const float sparkle = audio_.treble * am * (st.size > 0.5f ? 0.6f : 0.3f);
        const float x = fract((st.x - t_ * 0.6f * k) / W) * W;
        cv_.add(static_cast<int>(x), static_cast<int>(st.y), {0.80f, 0.86f, 1.0f}, (tw * st.size + sparkle) * clearSky);
    }

    // Shooting stars on a timer, and on drops.
    nextShoot_ -= dt;
    if ((nextShoot_ <= 0.0f || audio_.dropHit) && shootLife_ <= 0.0f && clearSky > 0.5f) {
        shootX_ = W * (0.35f + rnd() * 0.5f);
        shootY_ = H * (0.05f + rnd() * 0.2f);
        shootLife_ = 1.0f;
        nextShoot_ = 9.0f + rnd() * 10.0f;
    }
    if (shootLife_ > 0.0f) {
        shootLife_ -= rdt * 1.5f;
        const float prog = 1.0f - shootLife_;
        const float hx = shootX_ - prog * 40 * k, hy = shootY_ + prog * 20 * k;
        cv_.capsule(hx + 14 * k, hy - 7 * k, 0.1f, hx, hy, 0.6f, {0.92f, 0.96f, 1.0f}, std::max(0.0f, shootLife_));
    }

    Environment::paint_clouds(surface(), L, t_, H * 0.04f, H * 0.34f, 0.0f, 3);

    // Valley: layered ridges fading into mist, village lights in the bottom.
    hills(cv_, 20.0f, H * 0.66f, 22 * k, 1.9f, snowy({0.09f, 0.11f, 0.24f}, 0.9f));
    hills(cv_, 140.0f, H * 0.74f, 18 * k, 4.4f, snowy({0.07f, 0.08f, 0.19f}, 0.8f));
    const float valleyMist = 0.35f + 0.45f * L.fog;
    for (int py = static_cast<int>(H * 0.70f); py < static_cast<int>(H * 0.82f); ++py)
        for (int x = 0; x < cv_.w; ++x) {
            const float n = 0.5f + 0.5f * std::sin(x * 0.06f + t_ * 0.15f) * std::sin(x * 0.021f - t_ * 0.07f + py * 0.2f);
            const float fall = 1.0f - std::fabs(py - H * 0.76f) / (H * 0.06f);
            cv_.blend(x, py, {0.20f, 0.22f, 0.36f}, valleyMist * n * std::max(0.0f, fall));
        }
    hills(cv_, 60.0f, H * 0.84f, 8 * k, 6.2f, snowy({0.04f, 0.05f, 0.12f}, 0.7f));
    for (auto& l : village_) {
        const float b = 0.45f + 0.20f * std::sin(t_ * 0.7f + l.phase) + 0.55f * audio_.treble * am * (0.5f + 0.5f * std::sin(t_ * 9 + l.phase * 5));
        const Rgb c = mix({1.0f, 0.70f, 0.35f}, {1.0f, 0.92f, 0.70f}, l.hue);
        cv_.add(static_cast<int>(l.x), static_cast<int>(l.y), c, b);
        cv_.glow(l.x + 0.5f, l.y + 0.5f, 2.5f * k, c, 0.12f * b);
    }

    // Summit rock in the foreground.
    auto rockTop = [&](float x) { return H * 0.80f + H * 0.32f * smoothstep(W * 0.38f, W * 0.98f, x) - 1.5f * k * std::sin(x * 0.21f); };
    if (L.fog > 0.05f)
        Environment::paint_fog(surface(), L, t_, H * 0.45f, H * 0.86f);
    cv_.fill_below(rockTop, snowy({0.05f, 0.05f, 0.09f}, 1.0f));
    if (L.snow_cover > 0.05f)
        // A thin bright crust along the rock edge, lit by the tent.
        for (int x = 0; x < cv_.w; ++x)
            cv_.blend(x, static_cast<int>(rockTop(x + 0.5f)), {0.40f, 0.42f, 0.55f}, 0.6f * L.snow_cover);

    // Lit tent: the lantern flickers and breathes with the bass.
    const float tentX = W * 0.20f, tentBase = rockTop(tentX) + 1.0f * k;
    const float flicker = 0.08f * std::sin(t_ * 11.0f) * std::sin(t_ * 7.3f + 1.0f);
    const float lantern = std::clamp(0.62f + flicker + 0.40f * audio_.bass * am + 0.25f * audio_.beat * am, 0.0f, 1.2f);
    cv_.glow(tentX, tentBase - 5 * k, 30 * k, {1.0f, 0.55f, 0.20f}, 0.18f * lantern);
    const float tw = 15 * k, th = 15 * k;
    for (int y = static_cast<int>(tentBase - th); y < static_cast<int>(tentBase); ++y) {
        const float v = (y + 0.5f - (tentBase - th)) / th;  // 0 apex .. 1 base
        const float half = tw * (0.12f + 0.88f * std::sqrt(v));
        for (int x = static_cast<int>(tentX - half - 1); x <= static_cast<int>(tentX + half + 1); ++x) {
            const float edge = std::clamp(half - std::fabs(x + 0.5f - tentX) + 0.5f, 0.0f, 1.0f);
            const float glowIn = lantern * (0.55f + 0.45f * v) * (1.0f - 0.35f * std::fabs(x + 0.5f - tentX) / tw);
            cv_.blend(x, y, scale({1.0f, 0.58f, 0.22f}, glowIn), edge);
        }
    }
    if (L.snow_cover > 0.05f)
        // Snow on the tent roof.
        for (int y = static_cast<int>(tentBase - th); y < static_cast<int>(tentBase - th * 0.55f); ++y) {
            const float v = (y + 0.5f - (tentBase - th)) / th;
            const float half = tw * (0.12f + 0.88f * std::sqrt(v));
            for (int x = static_cast<int>(tentX - half); x <= static_cast<int>(tentX + half); ++x)
                if (std::fabs(x + 0.5f - tentX) > half - 1.6f * k)
                    cv_.blend(x, y, {0.62f, 0.62f, 0.70f}, 0.8f * L.snow_cover);
        }
    cv_.capsule(tentX, tentBase - th + 1, 0.3f * k, tentX, tentBase, 0.5f * k, scale({0.55f, 0.22f, 0.06f}, lantern));
    cv_.capsule(tentX, tentBase - th, 0.3f * k, tentX, tentBase - th - 2 * k, 0.2f * k, {0.25f, 0.22f, 0.22f});

    // The hiker sits on a rock and looks up at the sky.
    const float sx = W * 0.40f, ground = rockTop(sx);
    const float s = 1.15f * k;
    Pose sit;
    sit.lean = -0.10f;
    sit.thigh[0] = sit.thigh[1] = 1.45f;
    sit.shin[0] = 0.25f;
    sit.shin[1] = 0.10f;
    sit.upperArm[0] = sit.upperArm[1] = 0.85f;
    sit.foreArm[0] = sit.foreArm[1] = 1.75f;
    sit.headTilt = 0.4f;
    Look look;
    look.backpack = false;
    look.cap = true;
    look.skin = {0.55f, 0.36f, 0.26f};
    look.hair = {0.06f, 0.04f, 0.04f};
    look.shirt = L.rain > 0.25f ? Rgb{0.40f, 0.36f, 0.10f} : Rgb{0.42f, 0.22f, 0.16f};
    look.pants = {0.14f, 0.15f, 0.22f};
    look.shoes = {0.04f, 0.04f, 0.05f};
    look.capColor = {0.20f, 0.10f, 0.08f};
    cv_.ellipse(sx - 1.2f * s, ground - 2.0f * s, 3.4f * s, 2.6f * s, 0.0f, {0.07f, 0.07f, 0.11f});
    // Lantern light spilling out of the tent door onto the rock.
    cv_.ellipse(tentX + 4 * k, tentBase + 1.5f * k, 12 * k, 2.2f * k, 0, scale({1.0f, 0.55f, 0.22f}, lantern), 0.22f);
    const FigureOut fig = figure(cv_, sx, ground, s, 1.0f, sit, look);
    // Warm rim light on the side facing the tent.
    cv_.glow(sx - 3 * s, ground - 8 * s, 7 * s, {1.0f, 0.55f, 0.25f}, 0.16f * lantern);
    // A mug held in both hands, steam curling up into the cold air.
    const Point mug{(fig.hand[0].x + fig.hand[1].x) * 0.5f + 0.6f * s, (fig.hand[0].y + fig.hand[1].y) * 0.5f - 0.6f * s};
    cv_.ellipse(mug.x, mug.y, 0.9f * s, 1.1f * s, 0, {0.78f, 0.30f, 0.22f});
    for (int i = 0; i < 3; ++i) {
        const float u = fract(t_ * 0.35f + i / 3.0f);
        const float sway = std::sin(t_ * 1.3f + i * 2.0f + u * 4.0f) * 1.2f * s * u;
        cv_.glow(mug.x + sway, mug.y - (1.5f + 7.0f * u) * s, (0.8f + 1.2f * u) * s, {0.75f, 0.78f, 0.85f}, 0.30f * (1.0f - u));
    }

    paint_weather(rdt, H * 0.7f, rockTop);
    present(canvas);
    wait_until_next_frame();
    return true;
}

Scenes::SceneDescriptor SummitNightScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.family = "hiking";
    d.tags = {"hiking", "hills", "night", "fits:dusk", "fits:night", "stars", "camping", "aurora", "calm", "audio-reactive"};
    d.intensity = 0.15f;
    d.motion = 0.15f;
    d.music_affinity = 0.4f;
    d.performance_cost = 0.3f;
    environment_->add_variants(d);
    return d;
}

// ================================================================ alpine lake
AlpineLakeScene::AlpineLakeScene()
{
    use_environment({Environment::TimeOfDay::Day, Environment::Weather::Clear, Environment::Season::Summer, 13.0f, 0.24f});
}

void AlpineLakeScene::build_backdrop()
{
    const auto& L = env();
    const int w = cv_.w, h = cv_.h;
    const float W = static_cast<float>(w), H = static_cast<float>(h), k = W / 128.0f;
    horizon_ = std::floor(H * 0.52f);
    Canvas bg;
    bg.resize(w, h);
    const Environment::Surface s{bg.px.data(), w, h, 1.0f};
    Environment::paint_sky(s, L, horizon_);
    Environment::paint_stars(s, L, horizon_ * 0.8f, 1.7f, 23, 70);
    Environment::paint_sun(s, L, Environment::sun_point(L, W, horizon_), 6.0f * k);
    Environment::paint_moon(s, L, Environment::moon_point(L, W, horizon_), 4.5f * k);

    // Far snowy range, lit from the sun's side.
    const float sunSide = L.sun_x < 0.5f ? 1.0f : -1.0f;
    auto farTop = [&](float x) {
        const float u = x / W;
        return horizon_ - H * (0.10f + 0.25f * std::pow(1.0f - std::fabs(std::sin(u * 3.3f + 0.4f)), 1.6f) +
                               0.07f * std::pow(1.0f - std::fabs(std::sin(u * 8.1f + 1.3f)), 2.0f));
    };
    const Rgb snowLit = lit({0.96f, 0.97f, 1.0f}, 0.45f), haze = from_env(L.haze);
    // Sunrise/sunset paints the peaks pink (alpenglow).
    const Rgb glowTint = mix(snowLit, mix(snowLit, from_env(L.sun_color), 0.55f), L.twilight * (1.0f - L.overcast));
    // Lower slopes lose their snow in summer unless it snows.
    const float snowReach = 0.20f + 0.10f * (L.winter + L.snow_cover) - 0.04f * L.summer;
    for (int x = 0; x < w; ++x) {
        const float fx = x + 0.5f, top = farTop(fx);
        // Slope over a wider window so the crease between two ridges does not
        // flip the shading in a hard vertical line.
        const float slope = (farTop(fx + 4.0f * k) - farTop(fx - 4.0f * k)) * (1.5f / (4.0f * k)) * sunSide;
        const float shade = 1.0f - 0.28f * smoothstep(-0.3f, 0.9f, slope);
        const float snowLine = H * (0.36f - snowReach) + 1.6f * k * std::sin(x * 0.21f + 0.5f) + 0.8f * k * std::sin(x * 0.53f) +
                               0.25f * (top - farTop(fx - 4.0f * k));
        for (int y = std::max(0, static_cast<int>(std::floor(top))); y < static_cast<int>(horizon_); ++y) {
            const float hgt = horizon_ - (y + 0.5f);
            Rgb c = hgt > snowLine ? glowTint
                                   : lit(mix({0.40f, 0.45f, 0.58f}, {0.48f, 0.52f, 0.64f}, hash01(static_cast<uint32_t>(y * w + x))), 0.45f);
            c = scale(c, shade);
            c = mix(c, haze, (0.35f + 0.4f * L.fog) * (1.0f - std::min(1.0f, hgt / (H * 0.3f))));
            bg.blend(x, y, c, y == static_cast<int>(std::floor(top)) ? 1.0f - (top - std::floor(top)) : 1.0f);
        }
    }
    // Forested foothills (larch turns gold in autumn) with a waterfall.
    auto nearTop = [&](float x) {
        const float u = x / W;
        return horizon_ - H * (0.05f + 0.10f * std::pow(1.0f - std::fabs(std::sin(u * 5.2f + 2.2f)), 1.3f) + 0.012f * std::sin(u * 40.0f));
    };
    bg.fill_below([&](float x) { return nearTop(x); }, plant({0.20f, 0.34f, 0.30f}, 0.3f, 0.6f, 0.6f));
    for (int y = static_cast<int>(horizon_ - H * 0.16f); y < static_cast<int>(horizon_); ++y)
        for (int x = 0; x < w; ++x)
            if (y >= nearTop(x + 0.5f))
                bg.blend(x, y, haze, (0.30f + 0.3f * L.fog) * (1.0f - (horizon_ - y) / (H * 0.16f)));
    const Rgb pineFar = terrain({0.13f, 0.25f, 0.21f}, 0.3f, 0.45f), pineNear = terrain({0.07f, 0.15f, 0.12f}, 0.15f, 0.4f);
    for (float x = 1.5f * k; x < W; x += 3.0f * k) {
        const uint32_t id = static_cast<uint32_t>(x * 97.0f);
        pine(bg, x + hash01(id) * k, nearTop(x) + 2.0f * k, (3.5f + 3.0f * hash01(id + 1)) * k, pineFar);
    }
    for (float x = 0.8f * k; x < W; x += 2.2f * k) {
        const uint32_t id = static_cast<uint32_t>(x * 131.0f + 7.0f);
        pine(bg, x + hash01(id) * k, horizon_ + 0.5f, (2.5f + 3.5f * hash01(id + 1)) * k, pineNear);
    }
    backdrop_ = std::move(bg.px);
}

void AlpineLakeScene::initialize(int width, int height)
{
    AudioHobbyScene::initialize(width, height);
    backdrop_.clear();
    ripples_.clear();
    stone_ = {};
    throwT_ = -1.0f;
    nextThrow_ = 10.0f + rnd() * 8.0f;
    nextFish_ = 4.0f;
}

void AlpineLakeScene::start_throw()
{
    if (throwT_ < 0.0f && stone_.t < 0.0f)
        throwT_ = 0.0f;
}

bool AlpineLakeScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt, rdt;
    begin_audio_frame(dt, rdt);
    const float W = static_cast<float>(cv_.w), H = static_cast<float>(cv_.h), k = W / 128.0f;
    const float am = std::min(1.0f, audio_.amount);
    const auto& L = env();
    if (environment_changed(backdropKey_) || backdrop_.empty())
        build_backdrop();
    cv_.px = backdrop_;
    const int hz = static_cast<int>(horizon_);
    auto shore = [&](float x) { return H * 0.84f + 2.5f * k * std::sin(x * 0.045f + 0.6f) - 3.0f * k * smoothstep(W * 0.55f, W, x); };

    // Clouds drifting past the peaks, thin streaks on a clear day.
    if (L.clouds > 0.3f)
        Environment::paint_clouds(surface(), L, t_, H * 0.06f, H * 0.30f, 0.0f, 5);
    else
        for (int i = 0; i < 3; ++i) {
            const float period = W + 60 * k;
            const float cx = fract((i * 0.37f * period + t_ * (1.2f + 0.3f * i) * k) / period) * period - 30 * k;
            cv_.ellipse(cx, H * (0.12f + 0.08f * i), (16 + 5 * i) * k, 1.6f * k, 0, lit({1.0f, 0.97f, 0.95f}, 0.6f), 0.45f);
        }
    // Waterfall on the foothills (it freezes silent in deep winter).
    const float wfX = W * 0.70f;
    const float wfFlow = 1.0f - 0.85f * L.winter * L.snow_cover;
    const Rgb wfCol = lit({0.85f, 0.92f, 1.0f}, 0.3f);
    for (int y = static_cast<int>(horizon_ - H * 0.09f); y < hz; ++y) {
        const float b = 0.45f + 0.35f * std::sin(y * 1.4f - t_ * 9.0f * wfFlow);
        cv_.add(static_cast<int>(wfX), y, wfCol, b);
        cv_.add(static_cast<int>(wfX) + 1, y, wfCol, b * 0.4f);
    }

    // Ripples expand and fade.
    for (auto& r : ripples_) {
        r.r += rdt * 7.0f * k;
        r.life -= rdt * 0.40f;
    }
    std::erase_if(ripples_, [](const Ripple& r) { return r.life <= 0.0f; });
    nextFish_ -= dt;
    const bool fishBeat = audio_.beatHit && am > 0.3f && rnd() < 0.3f;
    if ((nextFish_ <= 0.0f || fishBeat) && ripples_.size() < 10) {
        ripples_.push_back({W * (0.08f + 0.80f * rnd()), horizon_ + (3 + rnd() * (H * 0.84f - horizon_ - 10)) , 0.5f, 1.0f});
        nextFish_ = 6.0f + rnd() * 8.0f;
    }
    // Raindrops ring the water.
    if (rnd() < rdt * 22.0f * L.rain && ripples_.size() < 24)
        ripples_.push_back({W * rnd(), horizon_ + 2 + rnd() * (H * 0.84f - horizon_ - 4), 0.2f, 0.55f});

    // The lake mirrors everything above the waterline. The sun (or moon)
    // leaves a glittering path when it is out.
    const Rgb waterCol = lit({0.16f, 0.30f, 0.44f}, 0.2f);
    const bool moonPath = L.night > 0.5f;
    const float sunX = moonPath ? Environment::moon_point(L, W, horizon_).x : Environment::sun_point(L, W, horizon_).x;
    const float pathVis = (moonPath ? 0.6f * L.night : std::clamp((L.sun_elevation + 2.0f) / 4.0f, 0.0f, 1.0f)) *
                          (1.0f - 0.9f * L.overcast) * (1.0f - L.fog);
    const Rgb pathCol = moonPath ? Rgb{0.80f, 0.86f, 1.0f} : mix(from_env(L.sun_color), {1.0f, 0.95f, 0.85f}, 0.5f);
    const float shimmer = 1.0f + 1.5f * audio_.treble * am;
    const uint32_t glintFrame = static_cast<uint32_t>(t_ * 6.0f);
    for (int y = hz; y < cv_.h; ++y) {
        const float d = (y - horizon_) / std::max(1.0f, H - horizon_);
        const float off = (std::sin(y * 0.9f - t_ * 1.6f) * (0.25f + 1.4f * d) * shimmer + 0.4f * std::sin(y * 0.31f + t_ * 0.7f)) * k;
        const int sy = std::max(0, 2 * hz - y - 1);
        for (int x = 0; x < cv_.w; ++x) {
            float ox = off, lift = 0.0f;
            for (const auto& r : ripples_) {
                const float dx = x + 0.5f - r.x, dy = (y + 0.5f - r.y) / 0.32f;
                const float band = std::fabs(std::sqrt(dx * dx + dy * dy) - r.r);
                if (band < 1.4f * k) {
                    const float a = (1.0f - band / (1.4f * k)) * r.life;
                    ox += 1.5f * k * a;
                    lift += 0.22f * a;
                }
            }
            const int sx = std::clamp(static_cast<int>(x + ox), 0, cv_.w - 1);
            const float* src = cv_.at(sx, sy);
            Rgb c = mix({src[0], src[1], src[2]}, waterCol, 0.28f + 0.40f * d);
            c = scale(c, 0.92f - 0.25f * d);
            float* p = cv_.at(x, y);
            p[0] = c.r + lift;
            p[1] = c.g + lift;
            p[2] = c.b + lift;
            // Sun path on the water: soft warm column with sparse glints.
            const float sunDx = std::fabs(x + 0.5f - sunX) / ((2.5f + 6.0f * d) * k);
            if (sunDx < 1.0f && pathVis > 0.01f) {
                cv_.add(x, y, pathCol, 0.12f * (1.0f - sunDx) * pathVis);
                if ((y & 1) == 0 && hash01(static_cast<uint32_t>(y * 7919 + (x >> 1) * 131) + glintFrame * 2654435761u) > 0.93f - 0.08f * audio_.treble * am)
                    cv_.add(x, y, pathCol, 0.35f * (1.0f - sunDx) * pathVis);
            }
        }
    }
    // Mist on the water: thick in the morning and in fog, thin otherwise.
    const Rgb mistCol = lit({0.92f, 0.94f, 0.97f}, 0.5f);
    const float mistAmount = 0.15f + 0.30f * (L.morning ? L.twilight : 0.0f) + 0.35f * L.fog;
    for (int py = hz - static_cast<int>(2 * k); py < hz + static_cast<int>(8 * k); ++py)
        for (int x = 0; x < cv_.w; ++x) {
            const float n = 0.5f + 0.5f * std::sin(x * 0.06f + t_ * 0.25f) * std::sin(x * 0.023f - t_ * 0.11f + py * 0.15f);
            const float fall = 1.0f - std::fabs(py - (horizon_ + 2 * k)) / (6 * k);
            cv_.blend(x, py, mistCol, mistAmount * n * std::max(0.0f, fall));
        }

    // Foreground shore, boulder, pack and the resting hiker.
    const Rgb shoreEdge = meadow({0.55f, 0.62f, 0.34f}), shoreTop = meadow({0.36f, 0.42f, 0.24f}), shoreDeep = meadow({0.18f, 0.22f, 0.14f});
    const Rgb pebble = lit({0.62f, 0.60f, 0.56f});
    const Rgb flower = lit(L.autumn > 0.5f ? Rgb{0.85f, 0.45f, 0.15f} : Rgb{0.95f, 0.85f, 0.40f});
    const float flowers = (1.0f - L.snow_cover) * (1.0f - L.winter);
    for (int x = 0; x < cv_.w; ++x) {
        const float top = shore(x + 0.5f);
        const int y0 = std::max(0, static_cast<int>(std::floor(top)));
        for (int y = y0; y < cv_.h; ++y) {
            const float dd = (y + 0.5f - top) / (H * 0.18f);
            Rgb c = y - top < 1.2f ? shoreEdge : mix(shoreTop, shoreDeep, dd);
            const float n = hash01(static_cast<uint32_t>(y * 4099 + x * 37));
            if (n > 0.985f)
                c = mix(c, pebble, 0.6f);  // a few pebbles
            else if (n > 0.97f)
                c = mix(c, flower, 0.45f * flowers);  // and wildflowers
            else
                c = scale(c, 0.93f + 0.12f * n);
            cv_.blend(x, y, c, y == y0 ? 1.0f - (top - std::floor(top)) : 1.0f);
        }
    }
    const float bx = W * 0.79f, bTop = shore(bx) - 6.0f * k;
    cv_.ellipse(bx, bTop + 5.0f * k, 10 * k, 6 * k, 0, lit({0.40f, 0.40f, 0.44f}));
    cv_.ellipse(bx - 2.5f * k, bTop + 3.0f * k, 6 * k, 3 * k, -0.2f, terrain({0.55f, 0.55f, 0.58f}, 0.0f, 0.9f));
    cv_.ellipse(W * 0.93f, shore(W * 0.93f) - 3.0f * k, 2.6f * k, 3.6f * k, 0.15f, lit({0.80f, 0.42f, 0.16f}));
    cv_.ellipse(W * 0.93f, shore(W * 0.93f) - 5.5f * k, 2.2f * k, 1.0f * k, 0.15f, lit({0.62f, 0.30f, 0.12f}));
    cv_.capsule(W * 0.965f, shore(W * 0.965f) + 1, 0.3f * k, W * 0.90f, shore(W * 0.9f) - 14 * k, 0.3f * k, lit({0.72f, 0.74f, 0.80f}));
    if (L.lamp() > 0.05f) {
        // A lantern on the boulder after dark, its light pooling on the shore and water.
        const float lamp = L.lamp() * (0.9f + 0.1f * std::sin(t_ * 9.0f) * std::sin(t_ * 5.3f));
        const float lx = bx + 6.5f * k, ly = bTop + 1.0f * k;
        cv_.glow(lx, ly, 22 * k, {1.0f, 0.62f, 0.25f}, 0.20f * lamp);
        cv_.ellipse(lx, ly, 1.1f * k, 1.5f * k, 0, scale({1.0f, 0.80f, 0.45f}, 0.4f + 0.6f * lamp));
        for (int y = hz + 2; y < static_cast<int>(shore(lx)); y += 2)
            cv_.add(static_cast<int>(lx + std::sin(y * 0.8f + t_ * 2.0f) * k), y, {1.0f, 0.65f, 0.30f},
                    0.20f * lamp * (y - hz) / (shore(lx) - hz));
    }

    // Skipping a stone: wind up, throw, then the stone hops away across the lake.
    nextThrow_ -= dt;
    if (nextThrow_ <= 0.0f || (audio_.dropHit && am > 0.2f)) {
        start_throw();
        nextThrow_ = 18.0f + rnd() * 14.0f;
    }
    const float s = 1.45f * k;
    Pose sit;
    sit.lean = 0.10f;
    sit.thigh[0] = sit.thigh[1] = 1.40f;
    sit.shin[0] = 0.30f;
    sit.shin[1] = 0.15f;
    sit.upperArm[0] = 0.55f;
    sit.foreArm[0] = 1.1f;   // hand resting on the knee
    sit.upperArm[1] = 0.75f;
    sit.foreArm[1] = 1.25f;
    bool release = false;
    if (throwT_ >= 0.0f) {
        const float before = throwT_;
        throwT_ += dt;
        const float tt = throwT_;
        if (tt < 0.45f) {
            const float e = smoothstep(0.0f, 0.45f, tt);
            sit.upperArm[1] = 0.75f + (-1.6f - 0.75f) * e;
            sit.foreArm[1] = 1.25f + (-0.9f - 1.25f) * e;
            sit.lean = 0.10f - 0.12f * e;
        }
        else {
            const float e = smoothstep(0.45f, 0.62f, tt);
            sit.upperArm[1] = -1.6f + (1.9f + 1.6f) * e;
            sit.foreArm[1] = -0.9f + (1.7f + 0.9f) * e;
            sit.lean = -0.02f + 0.25f * e;
        }
        release = before < 0.55f && tt >= 0.55f;
        if (tt > 1.4f)
            throwT_ = -1.0f;
    }
    Look look;
    look.cap = true;
    look.backpack = false;
    look.shirt = {0.18f, 0.55f, 0.56f};
    look.pants = {0.30f, 0.26f, 0.22f};
    look.capColor = {0.95f, 0.72f, 0.20f};
    if (L.rain > 0.25f)
        look.shirt = {0.20f, 0.42f, 0.75f};  // rain jacket
    light_look(look);
    float drop = 0.0f;
    for (int i = 0; i < 2; ++i) drop = std::max(drop, std::cos(sit.thigh[i]) * kThigh + std::cos(sit.shin[i]) * kShin);
    const FigureOut fig = figure(cv_, bx - 1.0f * k, bTop + 1.0f * k + (drop + 0.5f) * s, s, -1.0f, sit, look);

    if (release) {
        stone_.hops.clear();
        stone_.hops.push_back(fig.hand[1]);
        float x = W * (0.50f + 0.05f * rnd()), y = horizon_ + (H * 0.84f - horizon_) * 0.62f, step = 14.0f * k;
        for (int i = 0; i < 5; ++i) {
            stone_.hops.push_back({x, y});
            x -= step;
            y -= step * 0.18f;
            step *= 0.68f;
        }
        stone_.t = 0.0f;
    }
    if (stone_.t >= 0.0f) {
        const int seg = static_cast<int>(stone_.t);
        const float dur = seg == 0 ? 0.55f : 0.30f * std::pow(0.82f, static_cast<float>(seg));
        stone_.t += dt / dur;
        if (static_cast<int>(stone_.t) > seg) {
            const Point land = stone_.hops[static_cast<size_t>(seg + 1)];
            ripples_.push_back({land.x, land.y, 0.4f, seg + 2 == static_cast<int>(stone_.hops.size()) ? 1.0f : 0.8f});
        }
        if (stone_.t >= static_cast<float>(stone_.hops.size() - 1))
            stone_.t = -1.0f;
        else {
            const int sg = static_cast<int>(stone_.t);
            const float u = stone_.t - sg;
            const Point a = stone_.hops[static_cast<size_t>(sg)], b = stone_.hops[static_cast<size_t>(sg + 1)];
            const float arc = (sg == 0 ? 12.0f : 3.0f * std::pow(0.65f, static_cast<float>(sg))) * k * std::sin(u * kPi);
            cv_.ellipse(a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u - arc, 0.7f * k, 0.6f * k, 0, lit({0.30f, 0.30f, 0.32f}));
        }
    }

    paint_weather(rdt, horizon_, shore);
    present(canvas);
    wait_until_next_frame();
    return true;
}

Scenes::SceneDescriptor AlpineLakeScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.family = "hiking";
    d.tags = {"hiking", "mountains", "lake", "scenery", "landscape", "morning", "calm", "audio-reactive"};
    d.intensity = 0.15f;
    d.motion = 0.2f;
    d.music_affinity = 0.35f;
    d.performance_cost = 0.35f;
    environment_->add_variants(d);
    return d;
}
