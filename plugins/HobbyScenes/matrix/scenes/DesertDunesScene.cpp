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
constexpr float CaravanZ = 36.0f;    // distance the caravan walks at
constexpr float CamelScale = 0.72f;  // camel size at that distance
constexpr int Camels = 4;
constexpr float CamelGap = 6.5f;
constexpr float CaravanSpeed = 1.9f;   // design px per second
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

// Slope (d/dx, d/dz) of the fine wind ripples on the near sand; they only bend the light.
std::pair<float, float> ripple_slope(float x, float z)
{
    const float u = x * WindCos + z * WindSin, v = -x * WindSin + z * WindCos;
    const float c = 0.012f * std::cos(u * 18.0f + 2.0f * std::sin(v * 1.3f));
    const float dv = 2.6f * std::cos(v * 1.3f);
    return {c * (18.0f * WindCos - dv * WindSin), c * (18.0f * WindSin + dv * WindCos)};
}

// The look without a lightning flash: a sandstorm's lightning would light the sand and
// camels a cold white for a frame, so the desert ignores it.
Environment::Look steady(const Environment::Look& look)
{
    Environment::Look calm = look;
    calm.flash = 0.0f;
    return calm;
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
    // Fine dust pales the sky just above the dunes by day.
    const float dustBand = 0.4f * L.daylight * (1.0f - L.overcast) * (1.0f - L.storm);
    if (dustBand > 0.0f)
        for (int y = 0; y < std::min(bg.h, static_cast<int>(horizon + 4 * sy)); ++y)
            for (int x = 0; x < bg.w; ++x)
                bg.blend(x, y, Rgb{0.93f, 0.88f, 0.78f}, dustBand * smoothstep(horizon - 26 * sy, horizon, y + 0.5f));
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
    // The view drifts slowly across the dune sea as the light moves through the day.
    b.pan = 70.0f * lightX;
    const float veiled = (1.0f - 0.7f * L.overcast) * (1.0f - 0.15f * L.storm);
    b.direct = veiled * (moon ? 0.5f + 0.8f * L.moon_light : 1.0f);
    b.ambient = (0.35f + 0.3f * (1.0f - veiled)) * (1.0f - 0.6f * L.night);
    b.fog = (1.0f - 0.6f * L.night) * (1.0f - 0.25f * L.daylight);

    const auto calm = steady(L);
    Rgb sand = from_env(calm.lit(to_env({0.92f, 0.68f, 0.42f})));
    if (L.night > 0.0f) {
        // Moonlit sand turns blue-grey.
        const float luma = 0.3f * sand.r + 0.55f * sand.g + 0.15f * sand.b;
        sand = mix(sand, scale(Rgb{0.58f, 0.66f, 0.98f}, luma * (0.6f + 0.5f * L.moon_light)), 0.75f * L.night);
    }
    // Under a sandstorm the light is dim but the airborne dust brightens the sand around it.
    b.sand = scale(sand, 1.0f + 0.3f * L.storm);
    // Shadows are a cool plum lit by the sky, never black.
    const float luma = 0.3f * sand.r + 0.55f * sand.g + 0.15f * sand.b;
    b.shade = mix(mix(scale(Rgb{0.42f, 0.30f, 0.44f}, luma * 1.6f), scale(from_env(L.zenith), 0.6f), 0.2f), Rgb{0.02f, 0.03f, 0.08f},
                  0.6f * L.night);
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
        // By day it keeps some of the sand's warmth.
        const Rgb haze = mix(Rgb{b.skyStage[hz], b.skyStage[hz + 1], b.skyStage[hz + 2]}, b.sand, 0.3f * env().daylight);
        for (int sub = 0; sub < Samples; ++sub) {
            const float ray = ((x + (sub + 0.5f) / Samples) / sx - 64.0f) / Focal * b.mirror;
            const float pan = b.pan;
            int top = rows;  // first sub-row drawn so far, from the bottom up
            float lastZ = 0.0f;
            for (float z = NearZ; z < FarZ && top > 0; z *= ZGrowth) {
                const float fx = ray * z + pan;
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
                // The slope is measured over a few pixels, so sharp crests catch the light as soft rims.
                const float nearFade = std::clamp((18.0f - z) / 8.0f, 0.0f, 1.0f);
                const float e = std::clamp(0.025f * z, 0.05f, 0.25f);
                const auto [rx, rz] = ripple_slope(fx, z);
                const float gx = (dune_height(fx + e, z, detail) - dune_height(fx - e, z, detail)) / (2.0f * e) + nearFade * rx;
                const float gz = (dune_height(fx, z + e, detail) - dune_height(fx, z - e, detail)) / (2.0f * e) + nearFade * rz;
                const float inv = 1.0f / std::sqrt(gx * gx + 1.0f + gz * gz);
                const float nx = -gx * inv, ny = inv, nz = -gz * inv;
                // A soft terminator, so slip faces darken over a few pixels below the crest.
                const float facing = nx * b.lx + ny * b.ly + nz * b.lz;
                float light = facing > 0.2f ? facing : 0.2f * smoothstep(-0.15f, 0.2f, facing);
                if (light > 0.0f) {
                    const float flat = 1.0f / std::sqrt(b.lx * b.lx + b.lz * b.lz);
                    const float hx = b.lx * flat, hz2 = b.lz * flat;
                    for (float t = 0.9f; t < 30.0f && light > 0.0f; t *= 1.45f) {
                        const float above = dune_height(fx + hx * t, z + hz2 * t, detail) - (h + t * b.tanElevation);
                        light *= std::clamp(1.0f - above * 1.4f, 0.0f, 1.0f);
                    }
                }
                // Far away the faces blend into soft bands instead of a field of dark specks.
                light = std::lerp(light, 0.6f, 0.7f * smoothstep(60.0f, 170.0f, z)) * b.direct;
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
        const float shift = heat * fade * 2.6f * sx * std::sin(t_ * 1.6f + y * 1.7f) * std::sin(t_ * 0.6f + y * 0.6f);
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
    // Sand blows off the crests toward the slip faces in thin wisps, and low streams of
    // sand skim over the near slopes; a storm lifts much more.
    const auto& L = env();
    if (crests_.empty())
        return;
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f;
    const float wind = 0.35f + std::fabs(L.wind) + 1.5f * L.storm;
    spawn_ += raw_dt * (7.0f * wind + 60.0f * L.storm) * render_quality_scale();
    while (spawn_ >= 1.0f && grains_.size() < 240) {
        spawn_ -= 1.0f;
        Grain g{};
        if (rnd() < 0.35f) {
            // A stream of sand skimming over the near slopes.
            g.x = rnd() * cv_.w;
            g.y = (Horizon + 22.0f + rnd() * (128.0f - Horizon - 24.0f)) * sy;
            const float nearness = 0.6f + 0.8f * (g.y / cv_.h);
            g.vx = windDir_ * (12.0f + 10.0f * rnd()) * nearness * sx * (1.0f + 1.5f * L.storm);
            g.vy = 0.4f * (rnd() - 0.5f) * sy;
            g.trail = (2.0f + 3.0f * rnd()) * nearness * sx;
            g.max_life = g.life = 1.2f + 1.6f * rnd();
        }
        else {
            // A wisp off a crest; nearer crests are favoured so the wisps show.
            const Crest* c = &crests_[static_cast<size_t>(rnd() * (crests_.size() - 1))];
            for (int i = 0; i < 2; ++i) {
                const Crest* o = &crests_[static_cast<size_t>(rnd() * (crests_.size() - 1))];
                if (o->z < c->z)
                    c = o;
            }
            const float nearness = std::clamp(25.0f / c->z, 0.35f, 1.6f);
            const bool storm = L.storm * rnd() > 0.3f;
            g.x = c->x;
            g.y = storm ? (Horizon - 8 + rnd() * 70) * sy : c->y - rnd() * 1.2f * sy;
            g.vx = windDir_ * (7.0f + 9.0f * rnd()) * nearness * sx * (1.0f + 2.5f * L.storm);
            g.vy = (-1.5f + 3.0f * rnd()) * nearness * sy;
            g.trail = nearness * sx;
            g.max_life = g.life = 0.7f + 1.1f * rnd();
        }
        grains_.push_back(g);
    }
    if (spawn_ > 1.0f)
        spawn_ = 0.0f;
    const Rgb dust = from_env(steady(L).lit(to_env({0.98f, 0.84f, 0.62f}), 0.2f));
    for (auto& g : grains_) {
        g.life -= raw_dt;
        g.x += g.vx * raw_dt;
        g.y += g.vy * raw_dt;
        g.vy += 2.0f * sy * raw_dt;
        const float a = 0.4f * std::sin(kPi * std::clamp(g.life / g.max_life, 0.0f, 1.0f));
        // A short trail behind the grain, fading toward its tail.
        const int steps = std::max(1, static_cast<int>(g.trail + 0.5f));
        for (int i = 0; i < steps; ++i) {
            const float px = g.x - windDir_ * static_cast<float>(i) - 0.5f;
            const int col = static_cast<int>(std::floor(px));
            const float u = px - static_cast<float>(col);
            const float ai = a * (1.0f - static_cast<float>(i) / static_cast<float>(steps + 1));
            cv_.blend(col, static_cast<int>(g.y), dust, ai * (1.0f - u));
            cv_.blend(col + 1, static_cast<int>(g.y), dust, ai * u);
        }
    }
    const float w = static_cast<float>(cv_.w);
    std::erase_if(grains_, [w](const Grain& g) { return g.life <= 0.0f || g.x < -8.0f || g.x > w + 8.0f; });
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
    const auto calm = steady(L);
    const auto subject = [&](Rgb c) { return from_env(calm.subject(to_env(c))); };
    const Rgb hide = mix(subject({0.42f, 0.29f, 0.18f}), {0.02f, 0.02f, 0.04f}, 0.85f * L.night), dark = scale(hide, 0.7f);
    const Rgb cloth = mix(subject({0.46f, 0.20f, 0.14f}), {0.03f, 0.02f, 0.03f}, 0.8f * L.night);
    const Rgb wrap = mix(subject({0.74f, 0.68f, 0.58f}), {0.04f, 0.04f, 0.06f}, 0.8f * L.night);
    const Rgb shadow = scale(from_env(L.zenith), 0.25f);
    const float side = (L.night > 0.5f ? Environment::moon_point(L, cv_.w, Horizon * sy).x / cv_.w : L.sun_x) < 0.5f ? 1.0f : -1.0f;
    // Camel parts in design units around (x, gy), scaled to the distance the caravan walks at.
    constexpr float k = CamelScale;
    for (int n = Camels - 1; n >= 0; --n) {
        // Fixed, uneven gaps; the lead camel walks a little ahead.
        static constexpr std::array<float, 4> Lag{0.0f, 2.2f, 3.2f, 5.8f};
        const float x = caravan_ - CamelGap * static_cast<float>(n) - Lag[static_cast<size_t>(n % 4)];
        if (x < -8.0f || x > 136.0f)
            continue;
        const float gy = ground(x);
        const auto P = [&](float ox, float oy) { return std::pair{(x + k * ox) * sx, (gy + k * oy) * sy}; };
        const float phase = x * 1.3f + static_cast<float>(n) * 1.9f;  // each animal in its own step
        const float bob = 0.25f * std::sin(phase * 2.0f);
        // A contact shadow under the feet and a faint cast shadow away from the light.
        {
            const auto [cx, cy] = P(side * 1.2f, 0.1f);
            cv_.ellipse(cx, cy, 3.4f * k * sx, 0.45f * sy, 0.0f, shadow, 0.5f * (1.0f - 0.5f * L.overcast));
        }
        // Legs, far pair darker; each pair swings in opposite phase.
        for (int leg = 0; leg < 4; ++leg) {
            const float hip = (leg < 2 ? 1.5f : -1.5f) + (leg % 2 ? 0.3f : 0.0f);
            const float swing = 0.9f * std::sin(phase + (leg % 2 ? kPi : 0.0f) + (leg < 2 ? 0.0f : kPi * 0.5f));
            const auto [ax, ay] = P(hip, -4.0f);
            const auto [bx, by] = P(hip + swing, 0.0f);
            cv_.capsule(ax, ay, 0.38f * k * sx, bx, by, 0.3f * k * sx, leg % 2 ? dark : hide);
        }
        {
            const auto [bx, by] = P(0.0f, -4.9f + bob);
            cv_.ellipse(bx, by, 2.7f * k * sx, 1.3f * k * sy, 0.0f, hide);
            const auto [hx, hy] = P(-0.4f, -6.1f + bob);
            cv_.ellipse(hx, hy, 1.4f * k * sx, 1.0f * k * sy, 0.0f, hide);  // hump
            const auto [ax, ay] = P(2.1f, -5.2f + bob);
            const auto [nx, ny] = P(3.5f, -7.6f + bob);
            cv_.capsule(ax, ay, 0.55f * k * sx, nx, ny, 0.42f * k * sx, hide);  // neck
            const auto [qx, qy] = P(4.5f, -7.4f + bob);
            cv_.capsule(nx, ny, 0.5f * k * sx, qx, qy, 0.38f * k * sx, hide);  // head
        }
        if (n == 0) {
            // The lead camel carries a rider in a red robe and pale headcloth.
            const auto [ax, ay] = P(-0.4f, -6.8f + bob);
            const auto [bx, by] = P(-0.3f, -9.0f + bob);
            cv_.capsule(ax, ay, 0.75f * k * sx, bx, by, 0.55f * k * sx, cloth);
            cv_.ellipse(bx, by - 1.0f * k * sy, 0.7f * k * sx, 0.7f * k * sy, 0.0f, wrap);
            // After dark the rider carries a lantern, a small warm light moving over the dunes.
            if (L.night > 0.05f) {
                const auto [lx, ly] = P(3.0f, -6.0f + bob);
                const float flicker = 0.85f + 0.15f * std::sin(t_ * 9.0f) * std::sin(t_ * 5.3f);
                cv_.glow(lx, ly, 2.6f * sx, Rgb{1.0f, 0.62f, 0.25f}, 0.35f * L.night * flicker);
                cv_.blend(static_cast<int>(lx), static_cast<int>(ly), Rgb{1.0f, 0.85f, 0.5f}, 0.9f * L.night);
            }
        }
        else {
            // Pack loads on the others.
            const auto [px, py] = P(-0.6f, -6.9f + bob);
            cv_.ellipse(px, py, 1.5f * k * sx, 0.8f * k * sy, 0.0f, n % 2 ? wrap : scale(cloth, 0.8f));
        }
    }
}

void DesertDunesScene::draw_sandstorm()
{
    const auto& L = env();
    if (L.storm < 0.02f || skyline_.empty())
        return;
    // The air is thick with lit dust that hides the distance most. Sheets of blowing sand
    // sweep diagonally across the whole view at different speeds, and veils lift off the
    // skyline. Every so often the wind drops for a while and the dunes and caravan show.
    const Rgb dust =
        mix(from_env(steady(L).lit(to_env({0.80f, 0.57f, 0.33f}), 0.4f)), scale(Rgb{0.85f, 0.60f, 0.34f}, 0.3f + 0.65f * L.daylight), 0.6f);
    const Rgb gust = scale(dust, 1.6f);
    const Rgb skyDust = scale(dust, 1.55f);
    const float lull = smoothstep(0.2f, 0.8f, std::sin(t_ * 0.3f));
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f;
    const size_t w = static_cast<size_t>(cv_.w);
    // Per column: gust strength over the skyline and the glow around the hidden sun.
    columnTops_.resize(2 * w);
    for (int x = 0; x < cv_.w; ++x) {
        const float dx = (x + 0.5f) / sx;
        const float off = (x + 0.5f) / cv_.w - L.sun_x;
        columnTops_[w + static_cast<size_t>(x)] = 1.0f + 1.1f * L.daylight * std::exp(-off * off / 0.05f);
        columnTops_[static_cast<size_t>(x)] = smoothstep(0.5f, 1.0f, 0.5f + 0.5f * std::sin(dx * 0.07f - windDir_ * t_ * 3.2f)) *
                                              (0.55f + 0.45f * std::sin(dx * 0.023f - windDir_ * t_ * 0.7f));
    }
    // Sheets depend only on the position along the diagonal, so they come from a small table.
    constexpr int Bands = 256;
    std::array<float, Bands> sheets{};
    for (int i = 0; i < Bands; ++i) {
        const float d = static_cast<float>(i);  // design px along the diagonal, 0..255
        const float fast = smoothstep(0.55f, 1.0f, 0.5f + 0.5f * std::sin(d * 0.09f - t_ * 2.6f));
        const float slow = smoothstep(0.45f, 1.0f, 0.5f + 0.5f * std::sin(d * 0.045f - t_ * 1.1f + 1.7f));
        sheets[static_cast<size_t>(i)] = 0.6f * fast * (0.5f + 0.5f * std::sin(d * 0.013f - t_ * 0.4f)) + 0.45f * slow;
    }
    const float strength = L.storm * (1.0f - 0.7f * lull);
    for (int y = 0; y < cv_.h; ++y) {
        const float dy = (y + 0.5f) / sy;
        // Thick in the distance, thin over the nearest dunes so they stay darker than the air.
        const float distance = 1.0f - 0.8f * smoothstep(Horizon + 4.0f, 128.0f, dy);
        const float haze = L.storm * (0.06f + 0.3f * distance) * (1.0f - 0.5f * lull);
        for (int x = 0; x < cv_.w; ++x) {
            const size_t i = static_cast<size_t>(y * cv_.w + x);
            if (sky_[i])
                cv_.blend(x, y, scale(skyDust, columnTops_[w + static_cast<size_t>(x)]),
                          L.storm * (0.55f + 0.2f * smoothstep(0.0f, Horizon, dy)));
            else
                cv_.blend(x, y, dust, haze);
            // Diagonal sheets, running downwind and down toward the viewer.
            const float dx = (x + 0.5f) / sx;
            const float along = windDir_ > 0.0f ? dx + 0.5f * dy : 128.0f - dx + 0.5f * dy;
            const float sheet = sheets[static_cast<size_t>(std::clamp(along, 0.0f, Bands - 1.0f))];
            // Veils lift off the skyline in pulses.
            const float pulse = columnTops_[static_cast<size_t>(x)];
            const float d = dy - (skyline_[static_cast<size_t>(x)] / sy - 1.5f - 2.5f * pulse);
            const float veil = pulse * std::exp(-d * d / (2.0f + 3.0f * pulse));
            cv_.blend(x, y, gust, std::min(0.6f, strength * (0.4f * sheet + 0.8f * veil)));
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
