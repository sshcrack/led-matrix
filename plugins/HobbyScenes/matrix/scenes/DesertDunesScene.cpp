#include "DesertDunesScene.h"

#include "Backdrop.h"

namespace Hobbies {
namespace {

// Camera: design rows (128 × 128 design space) and dune-field units.
constexpr float Horizon = 58.0f;  // design row at eye level
constexpr float Focal = 110.0f;   // design px per field unit at distance 1
constexpr float Eye = 7.0f;       // camera height above the sand floor
constexpr float NearZ = 6.0f, FarZ = 260.0f, ZGrowth = 1.012f;
constexpr int Samples = 2;          // sub-samples per pixel along each axis
constexpr int ColumnsPerFrame = 6;  // backdrop columns rebuilt per frame after a change of light
constexpr float CrossfadeSeconds = 3.0f;
constexpr float CaravanZ = 30.0f;  // distance the caravan walks at
constexpr int Camels = 4;
constexpr float CamelGap = 9.0f;
constexpr float CaravanSpeed = 2.4f;   // design px per second
constexpr float NoCaravan = -1000.0f;  // caravan_ while no caravan is out

// Wind direction across the field: crests run across it, slip faces face downwind.
constexpr float WindCos = 0.9004471f, WindSin = 0.4349655f;  // 0.45 rad

// Height of the sand at field position (x, z). Transverse dunes with sinuous crests,
// a gentle windward slope and a steep slip face; `detail` fades the finest wiggles
// in the distance. Big hazy ridges far away shape the skyline.
float dune_height(float x, float z, float detail)
{
    const float u = x * WindCos + z * WindSin, v = -x * WindSin + z * WindCos;
    const float wander = 3.0f * std::sin(v / 7.0f) + 1.6f * std::sin(v / 17.0f + 2.0f) + 0.8f * detail * std::sin(v / 3.1f + 1.0f);
    float p = (u + wander) / 13.0f;
    p -= std::floor(p);
    constexpr float crest = 0.72f;
    float profile;
    if (p < crest) {
        const float r = p / crest;
        profile = 0.5f * r + 0.5f * std::sin(0.5f * kPi * r);
    }
    else {
        const float r = (1.0f - p) / (1.0f - crest);
        profile = std::pow(r, 1.6f);
    }
    const float amp = 2.0f + 1.1f * std::sin(v / 21.0f + 0.5f) * std::sin(u / 29.0f) + 0.5f * std::sin(v / 9.0f + u / 11.0f);
    const float swell = 0.8f * std::sin(x / 23.0f + 1.3f) * std::sin(z / 31.0f);
    const float distant = smoothstep(70.0f, 170.0f, z);
    const float ridges = distant * (6.0f + 4.0f * std::sin(x / 41.0f + 0.7f) * std::sin(z / 57.0f + 1.1f));
    return amp * profile + swell + ridges;
}

// Fine wind ripples on the near sand; they only bend the light.
float ripples(float x, float z)
{
    const float u = x * WindCos + z * WindSin, v = -x * WindSin + z * WindCos;
    return 0.012f * std::sin(u * 18.0f + 2.0f * std::sin(v * 1.3f));
}

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
    previous_.clear();
    fade_ = 0.0f;
    build_.next = -1;
    grains_.clear();
    caravan_ = NoCaravan;
    caravanWait_ = 6.0f;
    spawn_ = 0.0f;
}

void DesertDunesScene::start_build()
{
    const auto& L = env();
    Canvas bg;
    bg.resize(cv_.w, cv_.h);
    const Environment::Surface s{bg.px.data(), bg.w, bg.h, 1.0f};
    const float W = static_cast<float>(bg.w), sy = bg.h / 128.0f, k = std::min(bg.w, bg.h) / 128.0f;
    const float horizon = Horizon * sy;

    Environment::paint_sky(s, L, horizon + 4 * sy);
    // Desert nights are clear and dark: plenty of stars and a large moon.
    Environment::paint_stars(s, L, horizon, 1.7f, 31, 90);
    Environment::paint_sun(s, L, Environment::sun_point(L, W, horizon), 7.0f * k);
    Environment::paint_moon(s, L, Environment::moon_point(L, W, horizon * 0.8f), 8.5f * k);

    auto& b = build_;
    b.skyStage = bg.px;
    b.px = std::move(bg.px);
    b.ground.assign(static_cast<size_t>(cv_.w), static_cast<float>(cv_.h));
    b.cover.assign(static_cast<size_t>(cv_.w), static_cast<float>(cv_.h));
    b.skyline.assign(static_cast<size_t>(cv_.w), static_cast<float>(cv_.h));
    b.crests.clear();

    // Light comes from the sun by day and the moon at night. The field is mirrored
    // so the long windward slopes always face the light and the slip faces fall
    // into shadow, whichever side the light is on.
    const bool moon = L.night > 0.5f;
    const float lightX = moon ? Environment::moon_point(L, W, horizon).x / W : L.sun_x;
    b.mirror = lightX > 0.5f ? -1.0f : 1.0f;
    const float elevation = (moon ? 28.0f : std::max(L.sun_elevation, 14.0f)) * kPi / 180.0f;
    const float azimuth = (lightX - 0.5f) * 3.2f;
    b.lx = std::sin(azimuth) * std::cos(elevation) * b.mirror;
    b.ly = std::sin(elevation);
    b.lz = std::cos(azimuth) * std::cos(elevation);
    b.tanElevation = std::tan(elevation);
    const float veiled = (1.0f - 0.7f * L.overcast) * (1.0f - 0.35f * L.storm);
    b.direct = veiled * (moon ? 0.35f + 0.65f * L.moon_light : 1.0f);
    b.ambient = 0.35f + 0.3f * (1.0f - veiled);
    b.fog = 1.0f - 0.5f * L.night;

    Rgb sand = lit({0.92f, 0.68f, 0.42f});
    if (L.night > 0.0f) {
        // Moonlit sand turns blue-grey.
        const float luma = 0.3f * sand.r + 0.55f * sand.g + 0.15f * sand.b;
        sand = mix(sand, scale(Rgb{0.58f, 0.66f, 0.98f}, luma * (0.85f + 0.6f * L.moon_light)), 0.7f * L.night);
    }
    // Under a sandstorm the light is dim but the airborne dust brightens the sand around it.
    b.sand = scale(sand, 1.0f + 0.7f * L.storm);
    // Shadows are a cool plum lit by the sky, never black.
    const float luma = 0.3f * sand.r + 0.55f * sand.g + 0.15f * sand.b;
    b.shade = mix(scale(Rgb{0.42f, 0.30f, 0.44f}, luma * 1.6f), scale(from_env(L.zenith), 0.6f), 0.2f);
    b.next = 0;
}

void DesertDunesScene::build_columns(int count)
{
    auto& b = build_;
    const int W = cv_.w, H = cv_.h;
    const float sx = W / 128.0f, sy = H / 128.0f;
    const int rows = H * Samples;
    // Running sums of the dune colour per pixel row and how many sub-samples hit the dunes.
    std::vector<float> sum(static_cast<size_t>(H * 3));
    std::vector<int> hits(static_cast<size_t>(H));
    const int end = std::min(W, b.next + count);
    for (int x = b.next; x < end; ++x) {
        std::fill(sum.begin(), sum.end(), 0.0f);
        std::fill(hits.begin(), hits.end(), 0);
        // The haze takes on the colour of the sky right behind the dunes.
        const size_t hz = (static_cast<size_t>(std::clamp((Horizon - 2.0f) * sy, 0.0f, H - 1.0f)) * W + x) * 3;
        const Rgb haze{b.skyStage[hz], b.skyStage[hz + 1], b.skyStage[hz + 2]};
        for (int sub = 0; sub < Samples; ++sub) {
            const float ray = ((x + (sub + 0.5f) / Samples) / sx - 64.0f) / Focal * b.mirror;
            int top = rows;  // first sub-row drawn so far, from the bottom up
            float lastZ = 0.0f;
            for (float z = NearZ; z < FarZ && top > 0; z *= ZGrowth) {
                const float fx = ray * z;
                const float detail = std::clamp(1.0f - (z - 40.0f) / 80.0f, 0.0f, 1.0f);
                const float h = dune_height(fx, z, detail);
                const float row = (Horizon + (Eye - h) * Focal / z) * sy * Samples;
                if (sub == 0 && z >= CaravanZ && lastZ < CaravanZ)
                    b.ground[static_cast<size_t>(x)] = row / Samples;
                if (row >= top) {
                    continue;
                }
                const int y0 = std::max(0, static_cast<int>(std::floor(row)));
                // A big jump in distance means this sample shows over the crest of a nearer dune.
                if (sub == 0 && lastZ > 0.0f && z > lastZ * 1.25f && b.crests.size() < 800)
                    b.crests.push_back({x + 0.5f, static_cast<float>(top) / Samples, lastZ});

                // Light: surface normal from the slope, ripples on the near sand, shadows cast by nearer crests.
                const float nearFade = std::clamp((18.0f - z) / 8.0f, 0.0f, 1.0f);
                const float e = 0.05f;
                const float hr = h + nearFade * ripples(fx, z);
                const float gx = (dune_height(fx + e, z, detail) + nearFade * ripples(fx + e, z) - hr) / e;
                const float gz = (dune_height(fx, z + e, detail) + nearFade * ripples(fx, z + e) - hr) / e;
                const float inv = 1.0f / std::sqrt(gx * gx + 1.0f + gz * gz);
                const float nx = -gx * inv, ny = inv, nz = -gz * inv;
                float light = std::max(0.0f, nx * b.lx + ny * b.ly + nz * b.lz);
                if (light > 0.0f) {
                    const float flat = 1.0f / std::sqrt(b.lx * b.lx + b.lz * b.lz);
                    const float hx = b.lx * flat, hz2 = b.lz * flat;
                    for (float t = 0.9f; t < 30.0f && light > 0.0f; t *= 1.45f) {
                        const float above = dune_height(fx + hx * t, z + hz2 * t, detail) - (h + t * b.tanElevation);
                        light *= std::clamp(1.0f - above * 1.4f, 0.0f, 1.0f);
                    }
                }
                light *= b.direct;
                const float sky = 0.55f + 0.25f * ny;
                Rgb c = scale(b.sand, (b.ambient + 0.9f * light) * sky * 1.45f);
                c = mix(c, b.shade, (1.0f - light) * 0.35f);
                const float fog = std::pow(std::clamp((z - 15.0f) / 180.0f, 0.0f, 1.0f), 0.7f) * b.fog;
                c = mix(c, haze, 0.55f * fog);
                for (int y = y0; y < top; ++y) {
                    const size_t r = static_cast<size_t>(y / Samples);
                    sum[r * 3] += c.r;
                    sum[r * 3 + 1] += c.g;
                    sum[r * 3 + 2] += c.b;
                    ++hits[r];
                }
                top = y0;
                lastZ = z;
                if (sub == 0 && z < CaravanZ * 0.85f)
                    b.cover[static_cast<size_t>(x)] = static_cast<float>(top) / Samples;
            }
            if (sub == 0)
                b.skyline[static_cast<size_t>(x)] = static_cast<float>(top) / Samples;
        }
        // Average the sub-samples; pixels the dunes never touched stay exactly the sky.
        constexpr int n = Samples * Samples;
        for (int y = 0; y < H; ++y) {
            const int k = hits[static_cast<size_t>(y)];
            if (k == 0)
                continue;
            float* p = &b.px[static_cast<size_t>((y * W + x) * 3)];
            for (int c = 0; c < 3; ++c) p[c] = (sum[static_cast<size_t>(y * 3 + c)] + p[c] * static_cast<float>(n - k)) / n;
        }
    }
    b.next = end;
}

void DesertDunesScene::finish_build()
{
    auto& b = build_;
    sky_.assign(static_cast<size_t>(cv_.w * cv_.h), 0);
    Canvas staged;
    staged.resize(cv_.w, cv_.h);
    staged.px = b.px;
    for (size_t i = 0; i < sky_.size(); ++i) sky_[i] = unchanged(staged, b.skyStage, i);
    if (!backdrop_.empty()) {
        previous_ = std::move(backdrop_);
        fade_ = 1.0f;
    }
    backdrop_ = std::move(b.px);
    groundRow_ = std::move(b.ground);
    coverRow_ = std::move(b.cover);
    skyline_ = std::move(b.skyline);
    crests_ = std::move(b.crests);
    // Sand blows off the crests toward the slip faces.
    windDir_ = b.mirror;
    b.next = -1;
}

void DesertDunesScene::draw_shimmer()
{
    // Hot air over the far dunes bends the light: rows near the horizon waver sideways.
    const auto& L = env();
    const float heat = L.daylight * (1.0f - L.overcast) * (1.0f - L.storm) * std::clamp((L.temperature - 8.0f) / 22.0f, 0.35f, 1.0f);
    if (heat < 0.05f)
        return;
    const float sy = cv_.h / 128.0f, sx = cv_.w / 128.0f;
    const int y0 = static_cast<int>((Horizon - 6) * sy), y1 = static_cast<int>((Horizon + 6) * sy);
    row_.resize(static_cast<size_t>(cv_.w * 3));
    for (int y = std::max(0, y0); y < std::min(cv_.h, y1); ++y) {
        const float fade = 1.0f - std::fabs((y - (y0 + y1) * 0.5f) / ((y1 - y0) * 0.5f));
        const float shift = heat * fade * 1.6f * sx * std::sin(t_ * 2.3f + y * 1.7f) * std::sin(t_ * 0.9f + y * 0.6f);
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
    // Sand blows off the crests toward the slip faces in thin wisps; a storm lifts much more.
    const auto& L = env();
    if (crests_.empty())
        return;
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f;
    const float wind = 0.35f + std::fabs(L.wind) + 1.5f * L.storm;
    spawn_ += raw_dt * (5.0f * wind + 60.0f * L.storm) * render_quality_scale();
    while (spawn_ >= 1.0f && grains_.size() < 220) {
        spawn_ -= 1.0f;
        const auto& c = crests_[static_cast<size_t>(rnd() * (crests_.size() - 1))];
        const float nearness = std::clamp(25.0f / c.z, 0.35f, 1.6f);
        const bool storm = L.storm * rnd() > 0.3f;
        Grain g{};
        g.x = c.x;
        g.y = storm ? (Horizon - 8 + rnd() * 70) * sy : c.y - rnd() * 1.2f * sy;
        g.vx = windDir_ * (7.0f + 9.0f * rnd()) * nearness * sx * (1.0f + 2.5f * L.storm);
        g.vy = (-1.5f + 3.0f * rnd()) * nearness * sy;
        g.max_life = g.life = 0.7f + 1.1f * rnd();
        grains_.push_back(g);
    }
    if (spawn_ > 1.0f)
        spawn_ = 0.0f;
    const Rgb dust = lit({0.95f, 0.80f, 0.58f}, 0.3f);
    for (auto& g : grains_) {
        g.life -= raw_dt;
        g.x += g.vx * raw_dt;
        g.y += g.vy * raw_dt;
        g.vy += 2.0f * sy * raw_dt;
        const float a = std::sin(kPi * std::clamp(g.life / g.max_life, 0.0f, 1.0f));
        const float px = g.x - 0.5f;
        const int col = static_cast<int>(std::floor(px));
        const float u = px - static_cast<float>(col);
        cv_.blend(col, static_cast<int>(g.y), dust, 0.4f * a * (1.0f - u));
        cv_.blend(col + 1, static_cast<int>(g.y), dust, 0.4f * a * u);
    }
    const float w = static_cast<float>(cv_.w);
    std::erase_if(grains_, [w](const Grain& g) { return g.life <= 0.0f || g.x < -4.0f || g.x > w + 4.0f; });
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
    if (caravan_ - CamelGap * Camels - 10.0f > 134.0f) {
        caravan_ = NoCaravan;
        caravanWait_ = 35.0f + 40.0f * rnd();
        return;
    }
    const auto& L = env();
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f;
    // The caravan keeps to an even path over the dunes instead of every dip (design rows).
    const auto ground = [&](float x) {
        float total = 0.0f;
        int n = 0;
        for (int j = -3; j <= 3; ++j) {
            const int col = static_cast<int>((x + j * 2.5f) * sx);
            if (col < 0 || col >= cv_.w)
                continue;
            total += groundRow_[static_cast<size_t>(col)];
            ++n;
        }
        return n ? total / static_cast<float>(n) / sy : Horizon + 20.0f;
    };
    // At night the caravan is a dark silhouette against the moonlit sand.
    const Rgb hide = mix(subject({0.42f, 0.29f, 0.18f}), {0.02f, 0.02f, 0.04f}, 0.85f * L.night), dark = scale(hide, 0.7f);
    const Rgb cloth = mix(subject({0.62f, 0.16f, 0.12f}), {0.03f, 0.02f, 0.03f}, 0.8f * L.night);
    const Rgb wrap = mix(subject({0.82f, 0.78f, 0.70f}), {0.04f, 0.04f, 0.06f}, 0.8f * L.night);
    const Rgb shadow = scale(from_env(L.zenith), 0.25f);
    const float side = (L.night > 0.5f ? Environment::moon_point(L, cv_.w, Horizon * sy).x / cv_.w : L.sun_x) < 0.5f ? 1.0f : -1.0f;
    const auto P = [&](float x, float y) { return std::pair{x * sx, y * sy}; };
    for (int n = Camels - 1; n >= 0; --n) {
        // Fixed, uneven gaps; the lead camel walks a little ahead.
        static constexpr std::array<float, 4> Lag{0.0f, 3.0f, 4.5f, 8.0f};
        const float x = caravan_ - CamelGap * static_cast<float>(n) - Lag[static_cast<size_t>(n % 4)];
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
    if (L.storm < 0.02f || skyline_.empty())
        return;
    // A haze hides the distance while veils of sand stream off the crests and a thin
    // streak runs through the middle distance. Every so often the wind drops for a
    // while and the ridges and the caravan show. Daylight glows through the dust.
    const Rgb dust = mix(lit({0.78f, 0.55f, 0.32f}, 0.4f), scale(Rgb{0.80f, 0.56f, 0.32f}, 0.25f + 0.6f * L.daylight), 0.6f);
    const Rgb gust = scale(dust, 1.7f);
    const Rgb skyDust = scale(dust, 1.5f);
    const float lull = smoothstep(0.2f, 0.8f, std::sin(t_ * 0.3f));
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f;
    // Per column: gust strength over the skyline and over the caravan's ridge, and the sky glow.
    const size_t w = static_cast<size_t>(cv_.w);
    columnTops_.resize(3 * w);
    for (int x = 0; x < cv_.w; ++x) {
        const float dx = (x + 0.5f) / sx;
        const float off = (x + 0.5f) / cv_.w - L.sun_x;
        columnTops_[2 * w + static_cast<size_t>(x)] = 1.0f + 0.6f * L.daylight * std::exp(-off * off / 0.03f);
        for (int li = 0; li < 2; ++li)
            columnTops_[static_cast<size_t>(li) * w + static_cast<size_t>(x)] =
                smoothstep(0.5f, 1.0f, 0.5f + 0.5f * std::sin(dx * 0.07f - windDir_ * t_ * 3.2f + li * 2.1f)) *
                (0.55f + 0.45f * std::sin(dx * 0.023f - windDir_ * t_ * 0.7f + li));
    }
    for (int y = 0; y < cv_.h; ++y) {
        const float dy = (y + 0.5f) / sy;
        const float distance = 1.0f - 0.75f * smoothstep(Horizon + 8.0f, 128.0f, dy);
        // Dust fills the sky evenly and glows where the hidden sun is.
        for (int x = 0; x < cv_.w; ++x)
            if (sky_[static_cast<size_t>(y * cv_.w + x)])
                cv_.blend(x, y, scale(skyDust, columnTops_[2 * w + static_cast<size_t>(x)]),
                          L.storm * (0.35f + 0.3f * smoothstep(0.0f, Horizon + 2.0f, dy)));
        const float streakRow = dy - Horizon - 22.0f - 2.0f * std::sin(t_ * 0.3f);
        const float streak = std::exp(-streakRow * streakRow / 5.0f);
        const float haze = L.storm * 0.26f * distance * (1.0f - 0.55f * lull);
        for (int x = 0; x < cv_.w; ++x) {
            cv_.blend(x, y, dust, std::min(0.8f, haze));
            if (streak > 0.02f) {
                const float dx = (x + 0.5f) / sx;
                const float s = 0.5f + 0.5f * std::sin(dx * 0.15f - windDir_ * t_ * 4.0f) * std::sin(dx * 0.04f - windDir_ * t_ * 1.3f);
                cv_.blend(x, y, gust, L.storm * 0.55f * streak * s * (1.0f - 0.6f * lull));
            }
            // Thin veils of sand lift off the skyline and the caravan's ridge in pulses.
            float veil = 0.0f;
            for (int li = 0; li < 2; ++li) {
                const float pulse = columnTops_[static_cast<size_t>(li) * w + static_cast<size_t>(x)];
                if (pulse <= 0.0f)
                    continue;
                const float ridge = (li == 0 ? skyline_ : groundRow_)[static_cast<size_t>(x)] / sy;
                const float d = dy - (ridge - 1.5f - 2.5f * pulse);
                veil = std::max(veil, pulse * std::exp(-d * d / (2.0f + 3.0f * pulse)));
            }
            cv_.blend(x, y, gust, std::min(0.75f, L.storm * 0.9f * veil * (1.0f - 0.75f * lull)));
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
    const bool changed = environment_changed(backdropKey_);
    if (backdrop_.empty()) {
        start_build();
        build_columns(cv_.w);
        finish_build();
    }
    else if (changed && build_.next < 0)
        start_build();
    if (build_.next >= 0) {
        build_columns(ColumnsPerFrame);
        if (build_.next >= cv_.w)
            finish_build();
    }
    cv_.px = backdrop_;
    if (fade_ > 0.0f && previous_.size() == backdrop_.size()) {
        for (size_t i = 0; i < cv_.px.size(); ++i) cv_.px[i] += (previous_[i] - cv_.px[i]) * fade_;
        fade_ = std::max(0.0f, fade_ - raw / CrossfadeSeconds);
    }

    paint_clouds_behind(cv_, backdrop_, sky_, L, t_, Horizon * sy, 4 * sy, 38 * sy, 13);
    draw_shimmer();

    // The caravan walks on the sand at a fixed distance; nearer dunes hide it where they rise in front.
    draw_caravan(dt);
    for (int x = 0; x < cv_.w; ++x) {
        const int y0 = std::max(0, static_cast<int>(std::ceil(coverRow_[static_cast<size_t>(x)])));
        for (int y = y0; y < cv_.h; ++y) std::copy_n(&backdrop_[static_cast<size_t>((y * cv_.w + x) * 3)], 3, cv_.at(x, y));
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
