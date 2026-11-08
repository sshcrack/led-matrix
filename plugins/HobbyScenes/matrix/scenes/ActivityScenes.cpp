#include "ActivityScenes.h"

#include "SceneryDraw.h"

using namespace Hobbies;

namespace {
// Two-bone IK: the joint between `a` and `b` with bone lengths l1, l2, bent
// towards `bend` (+1 / -1 picks the side).
Point knee(Point a, Point b, float l1, float l2, float bend)
{
    const float dx = b.x - a.x, dy = b.y - a.y;
    float d = std::sqrt(dx * dx + dy * dy);
    d = std::clamp(d, 0.01f, l1 + l2 - 0.01f);
    const float along = (l1 * l1 - l2 * l2 + d * d) / (2.0f * d);
    const float h = std::sqrt(std::max(0.0f, l1 * l1 - along * along));
    const float ux = dx / d, uy = dy / d;
    return {a.x + ux * along - uy * h * bend, a.y + uy * along + ux * h * bend};
}

void wheel(Canvas& cv, float cx, float cy, float r, float angle, Rgb tire, Rgb spoke, float k)
{
    const int steps = static_cast<int>(r * 7.0f) + 12;
    for (int i = 0; i < steps; ++i) {
        const float a = kTau * i / steps;
        cv.blend(static_cast<int>(cx + std::cos(a) * r), static_cast<int>(cy + std::sin(a) * r), tire, 0.95f);
        cv.blend(static_cast<int>(cx + std::cos(a) * (r - 0.8f * k)), static_cast<int>(cy + std::sin(a) * (r - 0.8f * k)), tire, 0.5f);
    }
    for (int sp = 0; sp < 4; ++sp) {
        const float a = angle + kPi * sp / 4.0f;
        cv.capsule(cx - std::cos(a) * r * 0.85f, cy - std::sin(a) * r * 0.85f, 0.2f * k, cx + std::cos(a) * r * 0.85f,
                   cy + std::sin(a) * r * 0.85f, 0.2f * k, spoke, 0.35f);
    }
    cv.ellipse(cx, cy, 0.8f * k, 0.8f * k, 0, spoke);
}
}  // namespace

// ================================================================ cycling
CyclingScene::CyclingScene()
{
    use_environment({Environment::TimeOfDay::Day, Environment::Weather::Clear, Environment::Season::Summer, 24.0f, 0.70f});
}

void CyclingScene::initialize(int width, int height)
{
    AudioHobbyScene::initialize(width, height);
    gulls_.clear();
    scroll_ = 0.0f;
    nextGulls_ = 2.0f;
}

bool CyclingScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt, rdt;
    begin_audio_frame(dt, rdt);
    const auto& L = env();
    const float W = static_cast<float>(cv_.w), H = static_cast<float>(cv_.h), k = W / 128.0f;
    // Rides a little faster to music with a steady tempo.
    const float tempo = audio_.bpm > 0.0f ? std::clamp(audio_.bpm / 120.0f, 0.75f, 1.35f) : 1.0f;
    const float speed = 24.0f * k * (1.0f + (tempo - 1.0f) * std::min(1.0f, audio_.amount));
    scroll_ += speed * dt;

    const float horizon = std::floor(H * 0.52f);
    paint_sky_layers(horizon, scroll_ * 0.02f, 6.0f, 0.04f, 0.45f);

    // Far headland with a lighthouse, barely moving.
    const float head = std::fmod(scroll_ * 0.03f + W * 0.75f, W * 2.2f);
    const float hx = W * 1.4f - head;
    const Rgb landFar = terrain({0.36f, 0.44f, 0.42f}, 0.9f, 0.6f);
    for (int x = 0; x < cv_.w; ++x) {
        const float u = (x + 0.5f - hx) / (W * 0.45f);
        if (u < -1.0f || u > 1.0f)
            continue;
        const float top = horizon - (7.0f * k) * std::sqrt(std::max(0.0f, 1.0f - u * u)) * (0.75f + 0.25f * std::sin(u * 5.0f));
        for (int y = static_cast<int>(top); y < static_cast<int>(horizon); ++y) cv_.blend(x, y, landFar, y == static_cast<int>(top) ? 0.6f : 1.0f);
    }
    const float lhx = hx + W * 0.16f, lhb = horizon - 5.0f * k;
    if (lhx > -10 && lhx < W + 10) {
        cv_.capsule(lhx, lhb, 1.1f * k, lhx, lhb - 8 * k, 0.8f * k, lit({0.92f, 0.90f, 0.86f}, 0.8f));
        cv_.capsule(lhx, lhb - 2.5f * k, 1.15f * k, lhx, lhb - 4 * k, 0.95f * k, lit({0.80f, 0.20f, 0.18f}, 0.8f));
        if (L.lamp() > 0.05f) {
            const float a = t_ * 1.6f;
            const float len = 30.0f * k * std::fabs(std::cos(a));
            const float dir = std::sin(a) > 0 ? 1.0f : -1.0f;
            for (int i = 0; i < static_cast<int>(len); ++i)
                cv_.add(static_cast<int>(lhx + dir * i), static_cast<int>(lhb - 8 * k + i * 0.04f), {1.0f, 0.92f, 0.65f},
                        0.35f * L.lamp() * (1.0f - i / len));
            cv_.glow(lhx, lhb - 8.5f * k, 4 * k, {1.0f, 0.9f, 0.6f}, 0.8f * L.lamp());
        }
    }

    // Sea with a glitter path under the sun (or moon).
    const float seaBottom = H * 0.66f;
    const Rgb deep = lit({0.10f, 0.30f, 0.46f}, 0.5f);
    const Rgb seaFar = mix(from_env(L.sky(0.95f)), deep, 0.45f);
    const auto sp = Environment::sun_point(L, W, horizon);
    const float glitter = std::clamp(L.daylight * (1.0f - L.overcast) * (1.0f - L.fog), 0.0f, 1.0f);
    const float moonGlit = L.moon_light * (1.0f - L.overcast) * 0.7f;
    const auto mp = Environment::moon_point(L, W, horizon);
    for (int y = static_cast<int>(horizon); y < static_cast<int>(seaBottom); ++y) {
        const float v = (y - horizon) / (seaBottom - horizon);
        const Rgb base = mix(seaFar, deep, std::sqrt(v));
        for (int x = 0; x < cv_.w; ++x) {
            const float wave = std::sin(x * 0.35f + y * 1.7f + t_ * 1.6f) * std::sin(x * 0.13f - t_ * 0.9f + y);
            Rgb c = scale(base, 1.0f + 0.10f * wave);
            const float near = std::exp(-std::pow((x - sp.x) / ((3.0f + 12.0f * v) * k), 2.0f));
            if (wave > 0.35f)
                c = mix(c, from_env(L.sun_color), near * glitter * 0.8f);
            const float nearM = std::exp(-std::pow((x - mp.x) / ((2.0f + 8.0f * v) * k), 2.0f));
            if (wave > 0.45f)
                c = mix(c, {0.85f, 0.88f, 0.95f}, nearM * moonGlit);
            cv_.set(x, y, c);
        }
    }

    // Dunes with marram grass, then the road and the verge.
    const Rgb dune = terrain({0.78f, 0.70f, 0.52f}, 0.3f, 0.8f), duneGrass = meadow({0.46f, 0.56f, 0.30f}, 0.3f, 0.8f);
    cv_.fill_below([&](float x) { return seaBottom - 3.0f * k - hill(x + scroll_ * 0.45f, 3.3f) * 5.0f * k; }, dune);
    for (int x = 0; x < cv_.w; ++x) {
        const float wx = x + scroll_ * 0.45f;
        const float top = seaBottom - 3.0f * k - hill(wx, 3.3f) * 5.0f * k;
        const uint32_t id = static_cast<uint32_t>(std::floor(wx));
        if (hash01(id) < 0.55f) {
            const float sway = std::sin(t_ * 1.5f + wx * 0.2f) * (0.5f + std::fabs(L.wind));
            cv_.capsule(x, top + 1, 0.3f * k, x + sway * k, top - (2.0f + 3.0f * hash01(id + 3)) * k, 0.2f * k, duneGrass, 0.8f);
        }
    }
    const float roadTop = H * 0.76f, roadBot = H * 0.88f;
    const Rgb verge = meadow({0.34f, 0.50f, 0.22f}, 0.15f);
    for (int y = static_cast<int>(seaBottom); y < static_cast<int>(roadTop); ++y)
        for (int x = 0; x < cv_.w; ++x)
            if (y > seaBottom + 2.0f * k - hill(x + scroll_ * 0.45f, 3.3f) * 2.0f * k)
                cv_.set(x, y, scale(verge, 0.9f + 0.12f * hash01(static_cast<uint32_t>(y * 977 + static_cast<int>(x + scroll_ * 0.8f)))));
    // Roadside trees on the verge: they carry the season (blossom, autumn
    // gold, bare winter branches).
    {
        const float par = 0.8f, spacing = 46.0f * k, sc = scroll_ * par;
        for (int i = static_cast<int>(std::floor((sc - 20 * k) / spacing)); i * spacing < sc + W + 20 * k; ++i) {
            const uint32_t id = static_cast<uint32_t>(i) * 2654435761u;
            if (hash01(id) < 0.3f)
                continue;
            const float x = i * spacing + hash01(id + 1) * 14 * k - sc;
            Environment::paint_tree(surface(), L, x, roadTop - 1.0f * k, (20.0f + 8.0f * hash01(id + 2)) * k, id % 97u, t_, 0.25f);
        }
    }
    // Asphalt: wet roads darken and mirror the sky.
    const Rgb asphalt = lit({0.26f, 0.26f, 0.28f});
    for (int y = static_cast<int>(roadTop); y < static_cast<int>(roadBot); ++y) {
        const float v = (y - roadTop) / (roadBot - roadTop);
        for (int x = 0; x < cv_.w; ++x) {
            Rgb c = scale(asphalt, 0.92f + 0.12f * hash01(static_cast<uint32_t>(y * 131 + static_cast<int>(x + scroll_))));
            if (L.wetness > 0.02f)
                c = mix(scale(c, 0.7f), from_env(L.sky(0.6f + 0.4f * v)), L.wetness * 0.35f);
            cv_.set(x, y, c);
        }
    }
    // Snow piled at the road edges.
    if (L.snow_cover > 0.05f)
        for (int x = 0; x < cv_.w; ++x) {
            cv_.blend(x, static_cast<int>(roadTop), lit({0.9f, 0.92f, 0.96f}), L.snow_cover * 0.7f);
            cv_.blend(x, static_cast<int>(roadBot) - 1, lit({0.9f, 0.92f, 0.96f}), L.snow_cover * 0.5f);
        }
    // Centre dashes.
    const float dashY = (roadTop + roadBot) * 0.5f - 0.5f * k;
    for (int x = 0; x < cv_.w; ++x)
        if (std::fmod(x + scroll_, 16.0f * k) < 8.0f * k)
            cv_.blend(x, static_cast<int>(dashY), lit({0.92f, 0.90f, 0.80f}), 0.85f);
    // Fence posts along the far edge.
    for (float wx = std::floor(scroll_ / (20 * k)) * 20 * k; wx < scroll_ + W + 20 * k; wx += 20 * k) {
        const float x = wx - scroll_;
        cv_.capsule(x, roadTop + 0.5f * k, 0.55f * k, x, roadTop - 5 * k, 0.5f * k, lit({0.50f, 0.42f, 0.32f}));
        cv_.set(static_cast<int>(x), static_cast<int>(roadTop - 4 * k), lit({0.9f, 0.9f, 0.9f}));
    }

    // Seagulls on clear days.
    nextGulls_ -= dt;
    if (nextGulls_ <= 0.0f && L.daylight > 0.4f && L.rain < 0.3f && L.snow < 0.3f) {
        nextGulls_ = 12.0f + rnd() * 12.0f;
        for (int i = 0; i < 2 + static_cast<int>(rnd() * 2); ++i)
            gulls_.push_back({W + (4 + i * 7) * k, H * (0.18f + 0.1f * rnd()), rnd() * kTau, (5.0f + 2.0f * rnd()) * k});
    }
    for (auto& g : gulls_) {
        g.x -= g.speed * dt;
        g.phase += dt * 4.0f;
        const float flap = std::sin(g.phase) * 1.4f * k;
        const Rgb gc = lit({0.95f, 0.95f, 0.95f}, 0.2f);
        cv_.capsule(g.x - 2.4f * k, g.y - flap, 0.3f * k, g.x, g.y, 0.35f * k, gc);
        cv_.capsule(g.x, g.y, 0.35f * k, g.x + 2.4f * k, g.y - flap, 0.3f * k, gc);
    }
    std::erase_if(gulls_, [&](const Gull& g) { return g.x < -10 * k; });

    // Fog over the coast, before the rider so he stays crisp in front of it.
    Environment::paint_fog(surface(), L, t_, horizon - 8 * k, H, 0.0f);

    // ---- the cyclist ----
    const float bk = 1.35f * k;  // rider scale
    const float wheelR = 5.2f * bk, groundY = H * 0.855f, cx = W * 0.40f;
    const float bob = std::sin(crank_ * 2.0f) * 0.25f * bk;
    wheel_ += speed * dt / wheelR;
    crank_ += speed * dt / (wheelR * 2.4f);
    const Point rear{cx - 7.6f * bk, groundY - wheelR}, front{cx + 7.6f * bk, groundY - wheelR};
    const Point bb{cx - 1.0f * bk, groundY - wheelR + 0.6f * bk};
    const Point seat{cx - 3.6f * bk, groundY - wheelR - 8.0f * bk}, headTube{cx + 5.0f * bk, groundY - wheelR - 7.6f * bk};
    // Contact shadow.
    cv_.ellipse(cx, groundY + 0.5f * bk, 12 * bk, 1.0f * bk, 0, lit({0.10f, 0.10f, 0.12f}), 0.4f);
    const Rgb tire = subject({0.08f, 0.08f, 0.09f}), spokes = subject({0.75f, 0.75f, 0.78f});
    const bool rainGear = L.rain > 0.3f || L.storm > 0.2f;
    const Rgb frame = subject({0.15f, 0.55f, 0.75f});
    const Rgb jersey = subject(rainGear ? Rgb{0.98f, 0.82f, 0.10f} : L.winter > 0.5f ? Rgb{0.55f, 0.12f, 0.16f} : Rgb{0.90f, 0.30f, 0.20f});
    const Rgb shorts = subject({0.10f, 0.10f, 0.14f}), skin = subject({0.82f, 0.60f, 0.46f}), helmet = subject({0.95f, 0.95f, 0.95f});
    const Rgb legFar = scale(L.winter > 0.5f || rainGear ? shorts : skin, 0.72f), legNear = L.winter > 0.5f || rainGear ? shorts : skin;
    const float crankR = 2.6f * bk;
    const Point pedal[2] = {{bb.x + std::cos(crank_ + kPi) * crankR, bb.y + std::sin(crank_ + kPi) * crankR},
                            {bb.x + std::cos(crank_) * crankR, bb.y + std::sin(crank_) * crankR}};
    const Point hip{seat.x + 0.4f * bk, seat.y - 1.2f * bk + bob};
    const float thigh = 6.4f * bk, shin = 6.2f * bk;
    // Far leg behind the bike.
    {
        const Point kn = knee(hip, pedal[0], thigh, shin, -1.0f);
        cv_.capsule(hip.x, hip.y, 1.1f * bk, kn.x, kn.y, 0.85f * bk, scale(shorts, 0.75f));
        cv_.capsule(kn.x, kn.y, 0.8f * bk, pedal[0].x, pedal[0].y, 0.6f * bk, legFar);
        cv_.capsule(pedal[0].x - 0.6f * bk, pedal[0].y, 0.5f * bk, pedal[0].x + 1.4f * bk, pedal[0].y, 0.5f * bk, scale(tire, 1.5f));
    }
    wheel(cv_, rear.x, rear.y, wheelR, wheel_, tire, spokes, k);
    wheel(cv_, front.x, front.y, wheelR, wheel_, tire, spokes, k);
    auto tube = [&](Point a, Point b, float r) { cv_.capsule(a.x, a.y, r * bk, b.x, b.y, r * bk, frame); };
    tube(rear, bb, 0.45f);
    tube(rear, seat, 0.4f);
    tube(seat, bb, 0.5f);
    tube(bb, headTube, 0.55f);
    tube(seat, {headTube.x, headTube.y + 0.6f * bk}, 0.45f);
    tube(headTube, front, 0.45f);
    cv_.capsule(seat.x - 1.6f * bk, seat.y - 0.8f * bk, 0.5f * bk, seat.x + 1.2f * bk, seat.y - 0.8f * bk, 0.45f * bk, tire);
    const Point bar{headTube.x + 1.2f * bk, headTube.y - 1.6f * bk};
    cv_.capsule(headTube.x, headTube.y, 0.4f * bk, bar.x, bar.y, 0.4f * bk, tire);
    cv_.capsule(bar.x, bar.y, 0.45f * bk, bar.x + 1.2f * bk, bar.y + 1.4f * bk, 0.4f * bk, tire);
    cv_.capsule(bb.x, bb.y, 0.4f * bk, pedal[1].x, pedal[1].y, 0.4f * bk, spokes);
    // Torso leaning to the bars, near arm, head with helmet.
    const Point shoulder{hip.x + 7.2f * bk, hip.y - 5.4f * bk};
    cv_.capsule(hip.x, hip.y, 1.4f * bk, shoulder.x, shoulder.y, 1.5f * bk, jersey);
    const Point headC{shoulder.x + 2.3f * bk, shoulder.y - 2.0f * bk};
    cv_.ellipse(headC.x, headC.y, 1.7f * bk, 1.7f * bk, 0, skin);
    cv_.ellipse(headC.x - 0.3f * bk, headC.y - 0.9f * bk, 2.3f * bk, 1.3f * bk, -0.25f, helmet);
    {
        const Point kn = knee(hip, pedal[1], thigh, shin, -1.0f);
        cv_.capsule(hip.x, hip.y, 1.15f * bk, kn.x, kn.y, 0.9f * bk, shorts);
        cv_.capsule(kn.x, kn.y, 0.85f * bk, pedal[1].x, pedal[1].y, 0.65f * bk, legNear);
        cv_.capsule(pedal[1].x - 0.6f * bk, pedal[1].y, 0.55f * bk, pedal[1].x + 1.5f * bk, pedal[1].y, 0.5f * bk, scale(tire, 1.5f));
    }
    const Point grip{bar.x + 0.6f * bk, bar.y + 0.4f * bk};
    const Point elbow = knee(shoulder, grip, 3.0f * bk, 3.0f * bk, 1.0f);
    cv_.capsule(shoulder.x, shoulder.y, 0.75f * bk, elbow.x, elbow.y, 0.65f * bk, jersey);
    cv_.capsule(elbow.x, elbow.y, 0.6f * bk, grip.x, grip.y, 0.55f * bk, L.winter > 0.5f || rainGear ? jersey : skin);
    // Lights after dark: headlight beam on the road, red tail light.
    if (L.lamp() > 0.05f) {
        const float a = L.lamp();
        cv_.glow(bar.x + 1.0f * bk, bar.y + 0.6f * bk, 3 * bk, {1.0f, 0.95f, 0.8f}, 0.9f * a);
        for (int i = 0; i < static_cast<int>(26 * k); ++i) {
            const float f = i / (26.0f * k);
            const float y = bar.y + (groundY - bar.y) * std::min(1.0f, f * 1.6f);
            for (int j = -static_cast<int>(f * 3 * bk); j <= static_cast<int>(f * 3 * bk); ++j)
                cv_.add(static_cast<int>(bar.x + 2 * bk + i), static_cast<int>(y + j), {1.0f, 0.95f, 0.8f}, 0.10f * a * (1.0f - f));
        }
        cv_.glow(seat.x - 2.0f * bk, seat.y + 1.5f * bk, 2.5f * bk, {1.0f, 0.1f, 0.05f}, 0.9f * a);
    }

    // Foreground verge with flowers or snow.
    const Rgb vergeNear = meadow({0.24f, 0.40f, 0.16f});
    for (int y = static_cast<int>(roadBot); y < cv_.h; ++y)
        for (int x = 0; x < cv_.w; ++x) {
            const int wx = static_cast<int>(x + scroll_ * 1.25f);
            cv_.set(x, y, scale(vergeNear, 0.85f + 0.2f * hash01(static_cast<uint32_t>(y * 733 + wx))));
            const float bloom = (L.spring + L.summer) * (1.0f - L.snow_cover);
            if (bloom > 0.1f && hash01(static_cast<uint32_t>(y * 1931 + wx * 7)) < 0.03f * bloom)
                cv_.set(x, y, lit(hash01(static_cast<uint32_t>(wx)) < 0.5f ? Rgb{1.0f, 0.85f, 0.3f} : Rgb{0.95f, 0.45f, 0.6f}));
        }

    paint_weather(rdt, horizon, [&](float) { return H * (0.70f + 0.28f * rnd()); });
    present(canvas);
    wait_until_next_frame();
    return true;
}

Scenes::SceneDescriptor CyclingScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.family = "activities";
    d.tags = {"cycling", "sport", "coast", "sea", "fits:spring", "fits:summer", "fits:autumn", "scenery", "landscape", "calm", "audio-reactive"};
    d.intensity = 0.3f;
    d.motion = 0.55f;
    d.music_affinity = 0.3f;
    d.performance_cost = 0.3f;
    environment_->add_variants(d);
    return d;
}

// ================================================================ skiing
SkiingScene::SkiingScene()
{
    Environment::Adapt adapt;
    adapt.season = false;  // always winter
    adapt.weather_allowed = {Environment::Weather::Clear, Environment::Weather::PartlyCloudy, Environment::Weather::Overcast,
                             Environment::Weather::Fog, Environment::Weather::Snow};
    use_environment({Environment::TimeOfDay::Day, Environment::Weather::Clear, Environment::Season::Winter, 22.0f, 0.70f}, adapt);
}

void SkiingScene::initialize(int width, int height)
{
    AudioHobbyScene::initialize(width, height);
    spray_.clear();
    scroll_ = 0.0f;
    jumpT_ = -1.0f;
    nextJump_ = 3.0f;
}

float SkiingScene::ground(float x) const
{
    const float W = static_cast<float>(cv_.w), H = static_cast<float>(cv_.h), k = W / 128.0f;
    const float wx = x + scroll_;
    return H * 0.52f + (x / W) * H * 0.26f + 1.6f * k * std::sin(wx * 0.09f) + 0.8f * k * std::sin(wx * 0.031f + 1.0f);
}

bool SkiingScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt, rdt;
    begin_audio_frame(dt, rdt);
    const auto& L = env();
    const float W = static_cast<float>(cv_.w), H = static_cast<float>(cv_.h), k = W / 128.0f;
    const float speed = 30.0f * k;
    scroll_ += speed * dt;

    const float horizon = std::floor(H * 0.50f);
    paint_sky_layers(horizon, scroll_ * 0.02f, 6.0f, 0.04f, 0.40f);
    // Snowy peaks, then a nearer wooded ridge.
    const float sunSide = L.sun_x < 0.5f ? 1.0f : -1.0f;
    const Rgb peakLit = lit({0.96f, 0.97f, 1.0f}, 0.6f), peakShade = lit({0.66f, 0.72f, 0.86f}, 0.6f);
    auto peakTop = [&](float x) {
        const float u = (x + scroll_ * 0.05f) / W;
        return horizon - H * (0.10f + 0.20f * std::pow(1.0f - std::fabs(std::sin(u * 3.1f + 0.7f)), 1.8f) +
                              0.05f * std::pow(1.0f - std::fabs(std::sin(u * 7.3f)), 2.0f));
    };
    for (int x = 0; x < cv_.w; ++x) {
        const float top = peakTop(x + 0.5f), slope = (peakTop(x + 5.0f * k) - peakTop(x - 5.0f * k)) / (10.0f * k) * sunSide;
        const Rgb c = mix(peakLit, peakShade, smoothstep(-0.2f, 0.6f, slope));
        for (int y = static_cast<int>(top); y < static_cast<int>(horizon) + 4; ++y) cv_.blend(x, y, c, y == static_cast<int>(top) ? 0.6f : 1.0f);
    }
    // A forested, snowy ridge: rows of pines thicken towards the run.
    const Rgb ridgeSnow = lit({0.84f, 0.88f, 0.96f}, 0.55f);
    auto ridgeTop = [&](float wx) { return horizon + 2.0f * k - hill(wx, 2.2f) * 8.0f * k; };
    cv_.fill_below([&](float x) { return ridgeTop(x + scroll_ * 0.2f); }, ridgeSnow);
    for (int row = 0; row < 4; ++row) {
        const float par = 0.2f + 0.05f * row, step = (3.2f - 0.4f * row) * k;
        const Rgb pc = mix(terrain({0.12f, 0.24f, 0.19f}, 0.6f - 0.12f * row, 0.4f), from_env(L.haze), 0.25f - 0.05f * row);
        for (float wx = std::floor(scroll_ * par / step) * step; wx < scroll_ * par + W + step; wx += step) {
            const float x = wx - scroll_ * par;
            const uint32_t id = static_cast<uint32_t>(wx * 7.0f / k) + row * 7919u;
            if (hash01(id + 5) < 0.25f)
                continue;
            const float base = ridgeTop(x + scroll_ * 0.2f) + (1.0f + 5.5f * row) * k;
            pine(cv_, x + hash01(id) * k, base, (3.5f + 1.2f * row + 3.0f * hash01(id + 1)) * k, pc);
        }
    }

    // Chairlift climbing up to the left behind the run.
    const float liftPar = 0.35f;
    auto cableY = [&](float x) { return H * 0.20f + (x / W) * H * 0.22f; };
    const Rgb steel = lit({0.30f, 0.30f, 0.34f}, 0.3f);
    for (int x = 0; x < cv_.w; ++x) cv_.blend(x, static_cast<int>(cableY(x + 0.5f)), steel, 0.7f);
    for (float wx = std::floor(scroll_ * liftPar / (56 * k)) * 56 * k; wx < scroll_ * liftPar + W + 56 * k; wx += 56 * k) {
        const float x = wx - scroll_ * liftPar;
        cv_.capsule(x, cableY(x) - 1 * k, 0.7f * k, x, H * 0.62f, 0.9f * k, steel);
        cv_.capsule(x - 2.5f * k, cableY(x) - 1 * k, 0.4f * k, x + 2.5f * k, cableY(x) - 1 * k, 0.4f * k, steel);
    }
    lift_ += dt * 4.0f * k;
    for (float s = std::fmod(lift_, 18.0f * k) - 18.0f * k; s < W + 18 * k; s += 18.0f * k) {
        const float x = W - s - std::fmod(scroll_ * liftPar, 18.0f * k);
        const float cy = cableY(x);
        cv_.capsule(x, cy, 0.25f * k, x, cy + 4 * k, 0.25f * k, steel);
        cv_.capsule(x - 2 * k, cy + 4.5f * k, 0.5f * k, x + 1.5f * k, cy + 4.5f * k, 0.5f * k, lit({0.20f, 0.30f, 0.55f}, 0.3f));
        cv_.capsule(x - 2 * k, cy + 4.5f * k, 0.4f * k, x - 2 * k, cy + 2.5f * k, 0.4f * k, lit({0.20f, 0.30f, 0.55f}, 0.3f));
    }

    // The run: groomed snow with corduroy lines, a blue tint in the shade.
    const Rgb snowLit = lit({0.96f, 0.97f, 1.0f}), snowShade = lit({0.78f, 0.84f, 0.96f});
    for (int x = 0; x < cv_.w; ++x) {
        const float g = ground(x + 0.5f);
        for (int y = std::max(0, static_cast<int>(g)); y < cv_.h; ++y) {
            const float v = (y - g) / (H - g + 1.0f);
            Rgb c = mix(snowLit, snowShade, 0.12f + 0.45f * v);
            if (static_cast<int>(y + (x + scroll_) * 0.26f) % 3 == 0)
                c = scale(c, 0.96f);
            cv_.blend(x, y, c, y == static_cast<int>(g) ? 1.0f - (g - std::floor(g)) : 1.0f);
        }
    }
    // Pines and floodlight poles standing on the run's edge.
    for (float wx = std::floor(scroll_ / (14 * k)) * 14 * k; wx < scroll_ + W + 14 * k; wx += 14 * k) {
        const float x = wx - scroll_ + hash01(static_cast<uint32_t>(wx)) * 6 * k;
        const uint32_t id = static_cast<uint32_t>(wx * 3.0f / k);
        if (hash01(id) < 0.6f)
            pine(cv_, x, ground(x) - 1.0f, (9.0f + 8.0f * hash01(id + 1)) * k, terrain({0.10f, 0.22f, 0.16f}, 0.0f, 0.55f));
    }
    for (float wx = std::floor(scroll_ / (60 * k)) * 60 * k; wx < scroll_ + W + 60 * k; wx += 60 * k) {
        const float x = wx - scroll_ + 30 * k;
        const float g = ground(x);
        cv_.capsule(x, g, 0.6f * k, x, g - 20 * k, 0.5f * k, steel);
        cv_.ellipse(x + 1 * k, g - 20 * k, 1.5f * k, 0.9f * k, 0, mix(steel, {1.0f, 0.95f, 0.8f}, L.lamp()));
        cv_.glow(x + 1 * k, g - 19 * k, 18 * k, {1.0f, 0.92f, 0.75f}, 0.25f * L.lamp());
    }

    // Fog over the slope, before the skier so they stay crisp in front of it.
    Environment::paint_fog(surface(), L, t_, horizon - 10 * k, H, 0.0f);

    // Skier: carving down with the occasional hop over a roller.
    nextJump_ -= dt;
    if (jumpT_ < 0.0f && (nextJump_ <= 0.0f || audio_.dropHit)) {
        jumpT_ = 0.0f;
        nextJump_ = 6.0f + rnd() * 6.0f;
    }
    float liftH = 0.0f;
    if (jumpT_ >= 0.0f) {
        jumpT_ += dt / 0.9f;
        liftH = std::sin(std::min(1.0f, jumpT_) * kPi) * 7.0f * k;
        if (jumpT_ >= 1.0f)
            jumpT_ = -1.0f;
    }
    carve_ += dt;
    const float sx = W * 0.42f, gy = ground(sx);
    const float tilt = std::atan2(ground(sx + 4 * k) - ground(sx - 4 * k), 8 * k);
    const float crouch = 0.25f + 0.15f * std::sin(carve_ * 2.2f) + 0.3f * audio_.beat * std::min(1.0f, audio_.amount);
    Pose p;
    p.lean = 0.35f + 0.1f * crouch;
    p.thigh[0] = p.thigh[1] = 0.6f + 0.6f * crouch;
    p.shin[0] = p.shin[1] = -0.15f - 0.25f * crouch;
    p.thigh[0] += 0.12f;
    p.shin[0] += 0.08f;
    p.upperArm[0] = p.upperArm[1] = 0.5f;
    p.foreArm[0] = p.foreArm[1] = 1.3f;
    Hobbies::Look look;
    look.shirt = {0.92f, 0.22f, 0.20f};
    look.pants = {0.12f, 0.14f, 0.22f};
    look.shoes = {0.15f, 0.15f, 0.18f};
    look.cap = true;
    look.capColor = {0.15f, 0.45f, 0.85f};
    light_look(look);
    const float s = 1.45f * k;
    const float feetY = gy - liftH - 0.6f * k;
    // Shadow on the snow while airborne.
    cv_.ellipse(sx, gy, 6 * k * (1.0f - 0.04f * liftH / k), 0.8f * k, tilt, lit({0.55f, 0.62f, 0.80f}), 0.5f);
    const FigureOut out = figure(cv_, sx, feetY, s, 1.0f, p, look);
    // Skis along the slope with upturned tips, poles trailing.
    const float ca = std::cos(tilt), sa = std::sin(tilt), sk = 1.35f * k;
    const Rgb skiC = subject({0.95f, 0.85f, 0.15f});
    cv_.capsule(sx - 7 * sk * ca, feetY + 0.6f * k - 7 * sk * sa, 0.45f * k, sx + 7 * sk * ca, feetY + 0.6f * k + 7 * sk * sa, 0.45f * k, skiC);
    cv_.capsule(sx + 7 * sk * ca, feetY + 0.6f * k + 7 * sk * sa, 0.45f * k, sx + 8.2f * sk * ca, feetY - 0.6f * k + 8.2f * sk * sa, 0.4f * k,
                skiC);
    for (int i = 0; i < 2; ++i)
        cv_.capsule(out.hand[i].x, out.hand[i].y, 0.3f * k, out.hand[i].x - 9 * k, out.hand[i].y + 8.5f * k, 0.25f * k,
                    subject({0.75f, 0.75f, 0.78f}), i == 0 ? 0.7f : 1.0f);
    // Snow spray off the tails while carving.
    if (liftH < 0.5f * k)
        for (int i = 0; i < 3; ++i)
            spray_.push_back({sx - 5 * k, gy - 0.5f * k, -(10.0f + 14.0f * rnd()) * k, -(6.0f + 10.0f * rnd()) * k, 0.0f});
    const Rgb sprayC = lit({1.0f, 1.0f, 1.0f});
    for (auto& f : spray_) {
        f.life += dt;
        f.vy += 30.0f * k * dt;
        f.x += (f.vx - speed * 0.3f) * dt;
        f.y += f.vy * dt;
        cv_.blend(static_cast<int>(f.x), static_cast<int>(f.y), sprayC, 0.8f * (1.0f - f.life / 0.8f));
    }
    std::erase_if(spray_, [&](const Flake& f) { return f.life > 0.8f || f.y > ground(f.x) + 1; });

    paint_weather(rdt, horizon, [&](float x) { return ground(x); });
    present(canvas);
    wait_until_next_frame();
    return true;
}

Scenes::SceneDescriptor SkiingScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.family = "activities";
    d.tags = {"skiing", "sport", "winter", "snow", "mountains", "fits:winter", "scenery", "audio-reactive"};
    d.intensity = 0.35f;
    d.motion = 0.6f;
    d.music_affinity = 0.3f;
    d.performance_cost = 0.3f;
    environment_->add_variants(d);
    return d;
}
