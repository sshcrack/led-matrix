#include "NewYearScene.h"

#include <chrono>
#include <ctime>
#include <limits>

#include "Backdrop.h"

namespace Hobbies {
namespace {

// Layout in a 128×128 design space, scaled to the canvas.
constexpr float Horizon = 86.0f;   // where rockets leave the skyline
constexpr float Parapet = 112.0f;  // top of the rooftop wall

constexpr std::array<Rgb, 7> Palette{{
    {1.00f, 0.22f, 0.18f},  // red
    {0.20f, 0.55f, 1.00f},  // blue
    {0.25f, 1.00f, 0.40f},  // green
    {1.00f, 0.78f, 0.25f},  // gold
    {0.85f, 0.35f, 1.00f},  // violet
    {1.00f, 0.45f, 0.75f},  // pink
    {0.80f, 0.92f, 1.00f},  // silver
}};

enum Hair { Short, Beanie, Bun, Hood };
struct Who {
    float x, head, width, sway;
    Hair hair;
    int arm;  // 0 none, 1 sparkler, 2 glass, 3 arm round a neighbour
    float tilt;
};
constexpr std::array<Who, 5> People{{{17, 100, 5.5f, 0.0f, Beanie, 3, 0.6f},
                                     {30, 101, 5.0f, 1.7f, Bun, 2, -0.5f},
                                     {66, 99, 6.0f, 3.1f, Short, 1, 0.0f},
                                     {97, 100, 5.5f, 4.4f, Hood, 0, 0.0f},
                                     {110, 103, 4.2f, 5.2f, Beanie, 0, -0.3f}}};
constexpr Point Moon{18.0f, 16.0f};
// How long a burst lights its surroundings, and how bright at time `age`.
constexpr float LightLife = 1.2f;
float light_at(float age, float strength)
{
    return strength * std::min(1.0f, age / 0.1f) * std::exp(-age * 3.2f);
}

float far_top(float x)
{
    // Building tops of the far skyline: flat roofs from a stable hash per lot.
    const int lot = static_cast<int>(x / 7.0f);
    return 63.0f + 15.0f * hash01(static_cast<uint32_t>(lot) * 7919u + 11u);
}

float near_top(float x)
{
    const int lot = static_cast<int>((x + 5.0f) / 15.0f);
    return 78.0f + 14.0f * hash01(static_cast<uint32_t>(lot) * 104729u + 3u);
}

// Minutes since local midnight and whether today is 31 Dec / 1 Jan.
struct Clock {
    int minutes = 0;
    bool eve = false, newYearsDay = false;
};

Clock local_clock()
{
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    return {tm.tm_hour * 60 + tm.tm_min, tm.tm_mon == 11 && tm.tm_mday == 31, tm.tm_mon == 0 && tm.tm_mday == 1};
}

float ramp(float x, float x0, float x1, float y0, float y1)
{
    return y0 + (y1 - y0) * std::clamp((x - x0) / (x1 - x0), 0.0f, 1.0f);
}

}  // namespace

NewYearRooftopScene::NewYearRooftopScene()
{
    // Always a winter night; fog would hide the fireworks.
    Environment::Adapt adapt;
    adapt.time = false;
    adapt.season = false;
    adapt.weather_allowed = {Environment::Weather::Clear, Environment::Weather::PartlyCloudy, Environment::Weather::Overcast,
                             Environment::Weather::Snow, Environment::Weather::Drizzle};
    use_environment({Environment::TimeOfDay::Night, Environment::Weather::Clear, Environment::Season::Winter, std::nullopt, std::nullopt},
                    adapt);
}

void NewYearRooftopScene::register_properties()
{
    HobbySceneBase::register_properties();
    show_->label("Fireworks")
        .description(
            "Auto follows the clock on New Year's Eve: a few rockets in the evening, a big finale after midnight. "
            "On other days it plays a steady show.")
        .group("Fireworks");
    add_property(show_);
}

void NewYearRooftopScene::initialize(int width, int height)
{
    HobbySceneBase::initialize(width, height);
    backdrop_.clear();
    rockets_.clear();
    sparks_.clear();
    smoke_.clear();
    bursts_.clear();
    flash_ = lull_ = 0.0f;
    salvoLeft_ = 0.0f;
    launchClock_ = 0.0f;
    sparkler_ = 0.0f;
    // Open with a burst in the sky and two rockets on their way, so the scene
    // never starts on an empty sky.
    launch(1.0f);
    rockets_.back().y = rockets_.back().burstY + 1.0f;
    rockets_.back().vy = -14.0f;
    launch(1.0f);
    rockets_.back().y = Horizon - 20.0f;
}

float NewYearRooftopScene::launch_rate() const
{
    switch (show_->get().get()) {
        case NewYearShow::FINALE:
            return 2.6f;
        case NewYearShow::STEADY:
            return 0.95f;
        case NewYearShow::AUTO:
            break;
    }
    if (frame_context().deterministic)
        return 0.95f;
    const Clock c = local_clock();
    const float m = static_cast<float>(c.minutes);
    if (c.eve) {
        if (m < 18 * 60)
            return 0.25f;
        if (m < 23 * 60 + 30)
            return ramp(m, 18 * 60, 23 * 60 + 30, 0.35f, 0.75f);
        return ramp(m, 23 * 60 + 30, 23 * 60 + 59, 0.75f, 1.8f);
    }
    if (c.newYearsDay) {
        if (m < 25)
            return 3.6f;
        if (m < 90)
            return ramp(m, 25, 90, 3.6f, 0.8f);
        return ramp(m, 90, 180, 0.8f, 0.2f);
    }
    return 0.95f;
}

void NewYearRooftopScene::build_backdrop()
{
    const auto& L = env();
    Canvas bg;
    bg.resize(cv_.w, cv_.h);
    const float sx = bg.w / 128.0f, sy = bg.h / 128.0f, k = std::min(sx, sy);
    const Environment::Surface s{bg.px.data(), bg.w, bg.h, 1.0f};
    const Artwork art(bg);
    const auto rect = [&](float x, float y, float w, float h, Rgb c) { art.rect(x, y, w, h, c); };

    Environment::paint_sky(s, L, Horizon * sy);
    Environment::paint_stars(s, L, 70 * sy, 1.7f, 41, 70);
    Environment::paint_moon(s, L, {Moon.x * sx, Moon.y * sy}, 4.0f * k);
    // The city lights the haze above the skyline orange.
    for (int y = 0; y < static_cast<int>(Horizon * sy); ++y) {
        const float glow = std::pow(std::clamp(y / (Horizon * sy), 0.0f, 1.0f), 3.0f) * (0.10f + 0.10f * L.overcast);
        for (int x = 0; x < bg.w; ++x) bg.add(x, y, {1.0f, 0.55f, 0.28f}, glow);
    }
    const std::vector<float> skyStage = bg.px;

    // Far skyline with a TV tower and scattered lit windows.
    const Rgb farWall = lit({0.16f, 0.18f, 0.26f}, 0.8f);
    bg.fill_below([&](float x) { return far_top(x / sx) * sy; }, farWall);
    {
        const float tx = 94.0f;
        rect(tx - 1, 24, 2, Horizon - 24, farWall);
        rect(tx - 2.5f, 50, 5, 3, farWall);
        bg.ellipse(tx * sx, 32 * sy, 4.2f * k, 4.0f * k, 0.0f, farWall);
        rect(tx - 0.5f, 12, 1, 14, farWall);
        for (int i = -3; i <= 3; i += 2) bg.set(static_cast<int>((tx + i) * sx), static_cast<int>(32 * sy), {0.85f, 0.70f, 0.45f});
    }
    const std::array<Rgb, 4> windowTints{{{1.0f, 0.78f, 0.42f}, {1.0f, 0.66f, 0.32f}, {0.75f, 0.85f, 1.0f}, {0.55f, 0.65f, 1.0f}}};
    for (int y = 66; y < static_cast<int>(Horizon); y += 3)
        for (int x = 1; x < 128; x += 2) {
            if (y < far_top(static_cast<float>(x)) + 2)
                continue;
            const uint32_t id = static_cast<uint32_t>(y * 131 + x);
            if (hash01(id) > 0.30f)
                continue;
            bg.set(static_cast<int>(x * sx), static_cast<int>(y * sy),
                   scale(windowTints[static_cast<size_t>(hash01(id + 7) * 3.99f)], 0.35f + 0.35f * hash01(id + 3)));
        }

    // Nearer, darker blocks with bigger windows.
    const Rgb nearWall = lit({0.09f, 0.10f, 0.15f}, 0.4f);
    bg.fill_below([&](float x) { return near_top(x / sx) * sy; }, nearWall);
    for (int y = 82; y < static_cast<int>(Parapet); y += 4)
        for (int x = 2; x < 128; x += 4) {
            if (y < near_top(static_cast<float>(x)) + 3 || near_top(static_cast<float>(x + 1)) + 3 > y)
                continue;
            const uint32_t id = static_cast<uint32_t>(y * 977 + x * 13);
            if (hash01(id) > 0.42f)
                continue;
            // Windows right behind the people's heads stay dim, so their
            // outlines read against the city.
            float behind = 1.0f;
            for (const auto& p : People)
                if (std::fabs(x + 1.0f - p.x) < p.width + 3.0f && y > p.head - 8.0f)
                    behind = 0.35f;
            rect(static_cast<float>(x), static_cast<float>(y), 2, 2,
                 scale(windowTints[static_cast<size_t>(hash01(id + 5) * 3.99f)], behind * (0.45f + 0.4f * hash01(id + 9))));
        }
    // Mark the open sky before the rooftop goes on top.
    sky_.assign(static_cast<size_t>(bg.w * bg.h), 0);
    for (size_t i = 0; i < sky_.size(); ++i)
        sky_[i] = bg.px[i * 3] == skyStage[i * 3] && bg.px[i * 3 + 1] == skyStage[i * 3 + 1] && bg.px[i * 3 + 2] == skyStage[i * 3 + 2];

    // The rooftop: a dark brick parapet with snow on its cap.
    const float snowCap = L.snow_cover;
    const Rgb brick{0.075f, 0.055f, 0.06f}, mortar{0.05f, 0.04f, 0.045f};
    for (int y = static_cast<int>(Parapet); y < 128; ++y)
        for (int x = 0; x < 128; ++x) {
            const int row = (y - static_cast<int>(Parapet)) / 3;
            const bool joint = (y - static_cast<int>(Parapet)) % 3 == 2 || (x + row * 3) % 7 == 0;
            rect(static_cast<float>(x), static_cast<float>(y), 1, 1, joint ? mortar : brick);
        }
    rect(0, Parapet, 128, 1, {0.14f, 0.13f, 0.14f});
    // Snow lies unevenly on the cap: drifts of one to three pixels.
    for (int x = 0; x < 128; ++x) {
        const float drift =
            snowCap * (1.0f + 1.2f * (0.5f + 0.5f * std::sin(x * 0.21f + 1.3f)) + 0.8f * hash01(static_cast<uint32_t>(x) * 37u));
        if (drift > 0.3f)
            rect(static_cast<float>(x), Parapet + 1 - std::round(drift), 1, std::round(drift),
                 mix({0.40f, 0.42f, 0.50f}, {0.62f, 0.65f, 0.75f}, hash01(x * 11u)));
    }
    backdrop_ = std::move(bg.px);
}

void NewYearRooftopScene::launch(float speedScale)
{
    Rocket r{};
    // Pick the spot, of a few candidates, furthest from fresh bursts, rockets
    // still climbing and the moon, so big shells open in their own space.
    // Candidates keep a margin for the shell's radius from the top and sides.
    float best = -std::numeric_limits<float>::infinity();
    for (int attempt = 0; attempt < 6; ++attempt) {
        const float x = 22.0f + 84.0f * rnd(), y = 20.0f + 26.0f * rnd();
        float clear = std::hypot(x - Moon.x, y - Moon.y) - 8.0f;
        for (const auto& b : bursts_)
            if (b.age < 0.9f)
                clear = std::min(clear, std::hypot(x - b.x, y - b.y) - b.r);
        for (const auto& o : rockets_) clear = std::min(clear, std::hypot(x - o.x, y - o.burstY) - 14.0f);
        if (clear > best) {
            best = clear;
            r.x = x;
            r.burstY = y;
        }
    }
    r.y = Horizon;
    r.vy = -(std::sqrt(2.0f * 28.0f * (Horizon - r.burstY)) * (0.95f + 0.1f * rnd())) * speedScale;
    r.kind = static_cast<int>(rnd() * 6.0f);
    // Avoid repeating the colour of the shell before, so neighbours stay apart.
    do {
        r.color = Palette[static_cast<size_t>(rnd() * (Palette.size() - 0.01f))];
    } while (bursts_.size() > 0 && r.color.r == lastColor_.r && r.color.g == lastColor_.g && r.color.b == lastColor_.b);
    r.second = Palette[static_cast<size_t>(rnd() * (Palette.size() - 0.01f))];
    if (r.kind == 2)
        r.color = {1.0f, 0.70f, 0.30f};  // willows are gold
    lastColor_ = r.color;
    r.trail = 0.0f;
    rockets_.push_back(r);
}

void NewYearRooftopScene::burst(const Rocket& r)
{
    const float q = render_quality_scale();
    const float size = 0.75f + 0.45f * hash01(static_cast<uint32_t>(r.x * 97 + r.burstY * 13));
    const auto add = [&](float vx, float vy, float life, Rgb c, float drag, float gravity, uint8_t kind) {
        if (sparks_.size() < static_cast<size_t>(1400 * q))
            sparks_.push_back({r.x, r.y, vx, vy, life, life, c, drag, gravity, kind, 0.0f});
    };
    const int n = static_cast<int>((r.kind == 3 ? 48 : 96) * size * (0.6f + 0.4f * q));
    const float tilt = rnd() * kTau, squash = 0.45f + 0.3f * rnd();
    for (int i = 0; i < n; ++i) {
        // Shells burst as a sphere: project a random 3D direction, so the
        // disc is filled and densest at its rim, like a real peony.
        const float z = 2.0f * rnd() - 1.0f, a = kTau * rnd(), planar = std::sqrt(1.0f - z * z);
        const float v = 30.0f * size * (0.92f + 0.08f * rnd());
        float vx = std::cos(a) * planar * v, vy = std::sin(a) * planar * v;
        switch (r.kind) {
            case 0:
                add(vx, vy, 1.5f + 0.6f * rnd(), r.color, 1.9f, 9.0f, 0);
                break;  // peony
            case 1:
                add(vx, vy, 1.6f + 0.5f * rnd(), z > 0.0f ? r.color : r.second, 1.9f, 9.0f, 0);
                break;  // two colours
            case 2:
                add(vx * 0.9f, vy * 0.9f, 3.0f + 0.8f * rnd(), r.color, 2.2f, 6.5f, 1);
                break;  // willow
            case 3: {   // tilted ring
                const float ra = kTau * i / n, ux = std::cos(ra) * v, uy = std::sin(ra) * v * squash;
                add(ux * std::cos(tilt) - uy * std::sin(tilt), ux * std::sin(tilt) + uy * std::cos(tilt), 1.6f, r.color, 1.7f, 7.0f, 0);
                if (i % 3 == 0)  // a small core of the second colour
                    add(vx * 0.35f, vy * 0.35f, 1.2f, r.second, 1.9f, 8.0f, 0);
                break;
            }
            case 4:
                add(vx, vy, 1.4f + 0.5f * rnd(), r.color, 1.9f, 9.0f, 2);
                break;  // crackle
            default:
                add(vx * 0.8f, vy * 0.8f, 1.9f + 0.8f * rnd(), {1.0f, 0.93f, 0.8f}, 2.0f, 8.0f, 4);
                break;  // glitter
        }
    }
    bursts_.push_back({r.x, r.y, 0.0f, 14.0f * size, r.kind == 5 ? Rgb{1.0f, 0.95f, 0.85f} : r.color, 0.45f + 0.4f * size});
    if (smoke_.size() < 40)
        smoke_.push_back({r.x, r.y, 9.0f * size, 18.0f, 18.0f, mix(r.color, {0.55f, 0.52f, 0.5f}, 0.8f)});
}

void NewYearRooftopScene::update_fireworks(float dt)
{
    const auto& L = env();
    // Launch on a Poisson clock; sometimes a quick salvo of several rockets.
    if (salvoLeft_ > 0.0f) {
        launchClock_ -= dt;
        if (launchClock_ <= 0.0f) {
            launch(1.0f);
            salvoLeft_ -= 1.0f;
            launchClock_ = 0.22f + 0.25f * rnd();
            if (salvoLeft_ <= 0.0f)
                lull_ = 1.2f + 1.0f * rnd();  // a breath after each volley
        }
    }
    else if (lull_ > 0.0f) {
        lull_ -= dt;
    }
    else if (rnd() < launch_rate() * dt) {
        launch(1.0f);
        if (rnd() < 0.12f) {
            salvoLeft_ = static_cast<float>(2 + static_cast<int>(rnd() * 4.0f));
            launchClock_ = 0.2f;
        }
    }

    for (auto& r : rockets_) {
        r.y += r.vy * dt;
        r.vy += 28.0f * dt;
        r.x += L.wind * 2.0f * dt;
        r.trail -= dt;
        if (r.trail <= 0.0f) {
            r.trail = 0.035f;
            sparks_.push_back({r.x, r.y + 1.0f, (rnd() - 0.5f) * 3.0f, 4.0f, 0.45f, 0.45f, {1.0f, 0.62f, 0.28f}, 2.0f, 6.0f, 3, 0.0f});
        }
        if (r.y <= r.burstY || r.vy > -10.0f) {
            burst(r);
            r.y = -1000.0f;
        }
    }
    std::erase_if(rockets_, [](const Rocket& r) { return r.y < -500.0f; });

    std::vector<Spark> embers;
    for (auto& p : sparks_) {
        const float d = std::exp(-p.drag * dt);
        p.vx *= d;
        p.vy = p.vy * d + p.gravity * dt;
        p.x += (p.vx + L.wind * 3.0f) * dt;
        p.y += p.vy * dt;
        p.life -= dt;
        if (p.kind == 1) {
            p.trail -= dt;
            if (p.trail <= 0.0f && p.life > 0.4f) {
                p.trail = 0.09f;
                embers.push_back({p.x, p.y, 0.0f, 1.5f, 0.7f, 0.7f, {1.0f, 0.55f, 0.20f}, 1.0f, 3.0f, 3, 0.0f});
            }
        }
    }
    std::erase_if(sparks_, [](const Spark& p) { return p.life <= 0.0f || p.y > Horizon + 4.0f; });
    const size_t cap = static_cast<size_t>(1600 * render_quality_scale());
    for (const auto& e : embers)
        if (sparks_.size() < cap)
            sparks_.push_back(e);

    for (auto& m : smoke_) {
        m.life -= dt;
        m.r += 0.55f * dt;
        m.x += (L.wind * 3.0f + 1.2f) * dt;
        m.y -= 0.35f * dt;
    }
    std::erase_if(smoke_, [](const Puff& m) { return m.life <= 0.0f; });
    for (auto& b : bursts_) b.age += dt;
    std::erase_if(bursts_, [](const Bloom& b) { return b.age > LightLife; });
    // The rooftop, smoke and glass catch the combined light of the bursts.
    // Both amount and colour ease over a few frames, so they never jump.
    float total = 0.0f;
    Rgb tint{};
    for (const auto& b : bursts_) {
        const float l = light_at(b.age, b.strength);
        total += l;
        tint = {tint.r + b.color.r * l, tint.g + b.color.g * l, tint.b + b.color.b * l};
    }
    flash_ += (std::min(1.0f, total) - flash_) * std::min(1.0f, dt * 12.0f);
    if (total > 0.01f)
        flashColor_ = mix(flashColor_, scale(tint, 1.0f / total), std::min(1.0f, dt * 8.0f));
}

void NewYearRooftopScene::draw_sky_effects()
{
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f;
    const int skyRows = std::min(cv_.h, static_cast<int>(std::ceil(Horizon * sy)) + 1);
    // Each burst lights the sky around itself and, faintly, the city below
    // it; the light stays where the shell opened and fades there.
    const float reach = 55.0f * std::min(sx, sy), inv = 1.0f / reach;
    const int rows = std::min(cv_.h, static_cast<int>(Parapet * sy));
    for (const auto& b : bursts_) {
        const float l = light_at(b.age, b.strength);
        if (l < 0.02f)
            continue;
        const float fx = b.x * sx, fy = b.y * sy;
        for (int y = std::max(0, static_cast<int>(fy - reach)); y < std::min(rows, static_cast<int>(fy + reach) + 1); ++y)
            for (int x = std::max(0, static_cast<int>(fx - reach)); x < std::min(cv_.w, static_cast<int>(fx + reach) + 1); ++x) {
                const float dx = x - fx, dy = y - fy;
                const float close = 1.0f - std::sqrt(dx * dx + dy * dy) * inv;
                if (close > 0.0f)
                    cv_.add(x, y, b.color, l * (sky_[static_cast<size_t>(y * cv_.w + x)] ? 0.18f * close * close : 0.05f * close));
            }
    }
    // Smoke from earlier rockets hangs over the city, lit by the bursts.
    // Stacked puffs during a busy show must not fog the whole sky.
    const float density = std::min(1.0f, 10.0f / std::max<float>(1.0f, static_cast<float>(smoke_.size())));
    for (const auto& m : smoke_) {
        const float fade = density * std::min(1.0f, m.life / 4.0f) * std::min(1.0f, (m.maxLife - m.life) / 0.8f);
        const Rgb tint = scale(mix(m.tint, flashColor_, 0.4f * flash_), 0.30f + 0.4f * flash_);
        const float rx = m.r * sx, ry = m.r * 0.7f * sy, cx = m.x * sx, cy = m.y * sy;
        for (int y = std::max(0, static_cast<int>(cy - ry)); y <= std::min(skyRows - 1, static_cast<int>(cy + ry)); ++y)
            for (int x = std::max(0, static_cast<int>(cx - rx)); x <= std::min(cv_.w - 1, static_cast<int>(cx + rx)); ++x) {
                if (!sky_[static_cast<size_t>(y * cv_.w + x)])
                    continue;
                const float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry, d2 = dx * dx + dy * dy;
                if (d2 < 1.0f)
                    cv_.blend(x, y, tint, 0.22f * fade * (1.0f - d2) * (1.0f - d2));
            }
    }
    // The first instant of each burst is a bright, quickly fading bloom.
    for (const auto& b : bursts_) {
        const float u = 1.0f - b.age / 0.35f;
        if (u > 0.0f)
            cv_.glow(b.x * sx, b.y * sy, b.r * 0.65f * sx * (0.6f + 0.4f * (1.0f - u)), mix(b.color, {1, 1, 1}, 0.4f), 0.4f * u * u);
    }
}

void NewYearRooftopScene::draw_fireworks()
{
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f;
    const auto plot = [&](float x, float y, Rgb c, float a) {
        const int px = static_cast<int>(x * sx), py = static_cast<int>(y * sy);
        if (px < 0 || py < 0 || px >= cv_.w || py >= cv_.h || !sky_[static_cast<size_t>(py * cv_.w + px)])
            return;
        cv_.add(px, py, c, a);
    };
    const uint32_t frame = static_cast<uint32_t>(t_ * 12.0f);
    // In a busy sky the white crackle is shorter and dimmer, so overlapping
    // shells keep their shapes.
    const bool busy = sparks_.size() > 600;
    for (size_t idx = 0; idx < sparks_.size(); ++idx) {
        const auto& p = sparks_[idx];
        const float u = p.life / p.maxLife;  // 1 fresh .. 0 gone
        float a = std::min(1.0f, u * 3.0f);
        Rgb c = mix(p.color, {1.0f, 0.97f, 0.9f}, std::max(0.0f, u - 0.75f) * 3.0f);
        if (p.kind == 3) {  // trail embers
            a = u * 0.55f;
        }
        else if (p.kind == 2 && u < (busy ? 0.22f : 0.35f)) {  // crackle: white twinkles at the end
            // Each spark pulses on its own smooth phase instead of popping per frame.
            const float tw = 0.5f + 0.5f * std::sin(t_ * 18.0f + hash01(static_cast<uint32_t>(idx) * 2654435761u) * kTau);
            c = {1.0f, 1.0f, 0.95f};
            a = (busy ? 0.55f : 0.8f) * tw * tw;
        }
        else if (p.kind == 4) {  // glitter twinkles all the way
            a *= hash01(static_cast<uint32_t>(idx) * 40503u + frame * 7u) > 0.45f ? 1.0f : 0.15f;
        }
        plot(p.x, p.y, c, a);
        if (p.kind != 3)
            plot(p.x - p.vx * 0.035f, p.y - p.vy * 0.035f, c, a * 0.45f);
    }
    for (const auto& r : rockets_) {
        plot(r.x, r.y, {1.0f, 0.88f, 0.6f}, 1.0f);
        plot(r.x, r.y + 1.0f, {1.0f, 0.6f, 0.3f}, 0.6f);
    }
    // The TV tower's aircraft light.
    if (std::fmod(t_, 1.6f) < 0.25f)
        cv_.glow(94.0f * sx, 12.0f * sy, 2.5f * sx, {1.0f, 0.12f, 0.08f}, 0.9f);
}

void NewYearRooftopScene::draw_rooftop(float dt)
{
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f, k = std::min(sx, sy);
    // Five people seen from behind, dark against the city. Their outline
    // catches the warm city glow and flares in the colour of each burst.
    const Rgb body{0.02f, 0.02f, 0.03f};
    const Rgb rim = mix({0.85f, 0.55f, 0.30f}, flashColor_, std::min(1.0f, flash_ * 1.4f));
    const float rimA = 0.7f + 0.3f * std::min(1.0f, flash_ * 1.5f);
    const float sparkleOn = std::fmod(t_, 40.0f) < 26.0f ? 1.0f : 0.0f;
    sparkler_ += (sparkleOn - sparkler_) * std::min(1.0f, dt * 1.5f);
    const float toast = std::clamp(std::sin(t_ * 0.45f) * 3.0f - 2.0f, 0.0f, 1.0f);
    Point sparklerTip{}, glass{};
    const auto person = [&](int pass, float x, float headY, float width, float sway, Hair hair, int arm, float tilt) {
        const Rgb col = pass == 0 ? rim : body;
        const float a = pass == 0 ? rimA : 1.0f;
        const float dy = pass == 0 ? -0.8f : 0.0f;  // the rim peeks out above the silhouette
        const float cx = x + 0.4f * std::sin(t_ * 0.6f + sway), hy = headY + dy, hx = cx + tilt;
        // Shoulders and coat, widening to the bottom of the frame.
        cv_.capsule((cx - width + 2.2f) * sx, (hy + 7.0f) * sy, 2.2f * k, (cx + width - 2.2f) * sx, (hy + 7.0f) * sy, 2.2f * k, col, a);
        // The rim light only catches heads, shoulders and raised arms.
        for (int y = static_cast<int>((hy + 7.0f) * sy); y < (pass == 0 ? static_cast<int>((hy + 9.0f) * sy) : cv_.h); ++y) {
            const float half = width + 0.08f * (y / sy - hy - 7.0f);
            for (int xx = static_cast<int>((cx - half) * sx); xx <= static_cast<int>((cx + half) * sx); ++xx) cv_.blend(xx, y, col, a);
        }
        cv_.capsule(hx * sx, (hy + 2.5f) * sy, 1.1f * k, cx * sx, (hy + 5.0f) * sy, 1.3f * k, col, a);
        cv_.ellipse(hx * sx, hy * sy, 2.6f * k, 2.9f * k, 0.0f, col, a);
        switch (hair) {
            case Beanie:
                cv_.ellipse(hx * sx, (hy - 1.4f) * sy, 2.8f * k, 2.0f * k, 0.0f, col, a);
                cv_.ellipse(hx * sx, (hy - 3.9f) * sy, 1.1f * k, 1.1f * k, 0.0f, col, a);
                break;
            case Bun:
                cv_.ellipse(hx * sx, (hy - 3.0f) * sy, 1.4f * k, 1.2f * k, 0.0f, col, a);
                break;
            case Hood:
                cv_.ellipse(hx * sx, (hy + 0.8f) * sy, 3.3f * k, 3.3f * k, 0.0f, col, a);
                break;
            case Short:
                break;
        }
        if (arm == 1 && sparkler_ > 0.05f) {  // raised arm with a sparkler
            const Point shoulder{cx + width - 2.0f, hy + 7.0f};
            const Point hand{shoulder.x + 5.5f, shoulder.y - 2.0f - 8.0f * sparkler_ + 0.8f * std::sin(t_ * 2.3f)};
            cv_.capsule(shoulder.x * sx, shoulder.y * sy, 1.3f * k, hand.x * sx, hand.y * sy, 1.0f * k, col, a);
            sparklerTip = {hand.x + 2.5f, hand.y - 3.0f};
            if (pass == 1)
                cv_.capsule(hand.x * sx, hand.y * sy, 0.35f * k, sparklerTip.x * sx, sparklerTip.y * sy, 0.35f * k, {0.25f, 0.22f, 0.2f});
        }
        if (arm == 2 && toast > 0.02f) {  // raising a glass
            const Point shoulder{cx - width + 2.0f, hy + 7.0f};
            const Point hand{shoulder.x - 2.5f, shoulder.y - 1.0f - 8.0f * toast};
            cv_.capsule(shoulder.x * sx, shoulder.y * sy, 1.3f * k, hand.x * sx, hand.y * sy, 1.0f * k, col, a);
            glass = hand;
        }
        if (arm == 3) {  // an arm around the neighbour's shoulders
            cv_.capsule((cx + width - 2.0f) * sx, (hy + 7.0f) * sy, 1.3f * k, (cx + width + 4.5f) * sx, (hy + 6.0f) * sy, 1.1f * k, col, a);
        }
    };
    // A soft warm haze from the street below sits behind the heads, so the
    // silhouettes read even between bursts.
    for (const auto& p : People) cv_.glow(p.x * sx, (p.head + 3.0f) * sy, 9.0f * k, {1.0f, 0.62f, 0.32f}, 0.10f);
    for (int pass = 0; pass < 2; ++pass)
        for (const auto& p : People) person(pass, p.x, p.head, p.width, p.sway, p.hair, p.arm, p.tilt);

    if (glass.x > 0.0f) {
        cv_.set(static_cast<int>(glass.x * sx), static_cast<int>((glass.y - 1.5f) * sy),
                mix({0.95f, 0.8f, 0.45f}, flashColor_, 0.3f * flash_));
        cv_.set(static_cast<int>(glass.x * sx), static_cast<int>((glass.y - 2.5f) * sy), {0.75f, 0.75f, 0.7f});
    }
    if (sparklerTip.x > 0.0f) {
        cv_.glow(sparklerTip.x * sx, sparklerTip.y * sy, 6.0f * k, {1.0f, 0.75f, 0.4f}, 0.28f * sparkler_);
        const uint32_t frame = static_cast<uint32_t>(t_ * 30.0f);
        for (uint32_t i = 0; i < 10; ++i) {
            if (hash01(i * 977u + frame * 31u) > sparkler_)
                continue;
            const float ang = kTau * hash01(i * 131u + frame * 17u), len = 1.0f + 5.0f * hash01(i * 71u + frame * 5u);
            cv_.add(static_cast<int>((sparklerTip.x + std::cos(ang) * len) * sx),
                    static_cast<int>((sparklerTip.y + std::sin(ang) * len) * sy), {1.0f, 0.92f, 0.7f}, 0.75f);
        }
    }
}

bool NewYearRooftopScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt = 0.0f;
    begin_frame(dt);
    // Fewer, sparser flakes than the default so snow does not read as sparks.
    precip_.set_max(static_cast<int>(110 * render_quality_scale()));
    const float raw = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.1f);
    if (environment_changed(backdropKey_) || backdrop_.empty())
        build_backdrop();
    cv_.px = backdrop_;

    update_fireworks(dt);
    draw_sky_effects();
    draw_fireworks();
    draw_rooftop(dt);
    paint_weather(raw, Horizon * cv_.h / 128.0f, [&](float) { return static_cast<float>(cv_.h); });
    present(canvas);
    wait_until_next_frame();
    return true;
}

Scenes::SceneDescriptor NewYearRooftopScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "new_year";
    d.tags = {"new-year", "holidays", "occasion:new_years_eve", "fits:winter", "fits:night", "scenery", "city", "fireworks", "night"};
    d.intensity = 0.45f;
    d.motion = 0.45f;
    d.music_affinity = 0.2f;
    d.performance_cost = 0.3f;
    environment_->add_variants(d, {{"finale",
                                    "Midnight finale",
                                    "The dense fireworks finale right after midnight.",
                                    nlohmann::json{{"show", "FINALE"}},
                                    {},
                                    0.7f,
                                    0.7f,
                                    std::nullopt,
                                    std::nullopt,
                                    false}});
    return d;
}

}  // namespace Hobbies
