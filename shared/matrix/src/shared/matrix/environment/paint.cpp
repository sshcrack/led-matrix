#include <shared/matrix/environment/paint.h>

#include <algorithm>
#include <cmath>

namespace Environment {
namespace {
constexpr float kTau = 6.2831853f;

float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
float hash01(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return static_cast<float>(x & 0xFFFFFFU) / static_cast<float>(0xFFFFFF);
}
float fract(float x) { return x - std::floor(x); }
} // namespace

Point2 sun_point(const Look &l, float width, float horizon_y)
{
    // Scenery horizons are hidden behind hills, so a low sun still sits a
    // third up the sky; below the horizon it sinks out of view quickly.
    const float e = l.sun_elevation;
    const float up = e >= 0.0f ? 0.30f + 0.62f * std::min(1.0f, e / 50.0f) : 0.30f * (e + 6.0f) / 6.0f;
    return {l.sun_x * width, horizon_y - up * horizon_y};
}

Point2 moon_point(const Look &l, float width, float horizon_y)
{
    // Waxing moons hang in the west (right) in the evening, waning ones east.
    const float x = l.moon_phase < 0.5f ? 0.74f : 0.26f;
    return {x * width, horizon_y * 0.26f};
}

void soft_ellipse(const Surface &s, float cx, float cy, float rx, float ry, Rgb c, float alpha)
{
    if (alpha <= 0.0f || rx <= 0.0f || ry <= 0.0f)
        return;
    const int x0 = std::max(0, static_cast<int>(cx - rx - 1)), x1 = std::min(s.w - 1, static_cast<int>(cx + rx + 1));
    const int y0 = std::max(0, static_cast<int>(cy - ry - 1)), y1 = std::min(s.h - 1, static_cast<int>(cy + ry + 1));
    const float rmin = std::min(rx, ry);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            const float u = (x + 0.5f - cx) / rx, v = (y + 0.5f - cy) / ry;
            const float d = (std::sqrt(u * u + v * v) - 1.0f) * rmin;
            const float a = clamp01(0.5f - d);
            if (a > 0.0f)
                s.blend(x, y, c, a * alpha);
        }
}

namespace {
void soft_line(const Surface &s, float ax, float ay, float bx, float by, float r0, float r1, Rgb c)
{
    const float len = std::max(std::fabs(bx - ax), std::fabs(by - ay));
    const int steps = std::max(1, static_cast<int>(len * 1.5f));
    for (int i = 0; i <= steps; ++i) {
        const float f = static_cast<float>(i) / steps;
        const float r = std::max(0.45f, r0 + (r1 - r0) * f);
        soft_ellipse(s, ax + (bx - ax) * f, ay + (by - ay) * f, r, r, c, 1.0f);
    }
}
}  // namespace

void paint_tree(const Surface &s, const Look &l, float x, float base, float h, uint32_t seed, float t, float depth)
{
    const float sway = std::sin(t * 0.9f + seed) * (0.4f + 1.2f * std::fabs(l.wind) + 1.5f * l.storm) * h / 40.0f;
    const Rgb bark = l.lit({0.30f, 0.22f, 0.16f}, depth), barkDark = l.lit({0.20f, 0.14f, 0.10f}, depth);
    const Rgb snowC = l.lit({0.94f, 0.95f, 0.99f}, depth);
    const float topX = x + sway * 0.5f, topY = base - h * 0.52f;
    soft_line(s, x, base, topX, topY, h * 0.055f, h * 0.035f, bark);
    // Leaves: thinning in late autumn, none in winter.
    const float leaves = clamp01(1.0f - l.winter - 0.25f * l.autumn);
    // Main limbs: spread wide when bare, kept inside the crown when leafy.
    const float reach = 1.0f - 0.38f * clamp01(leaves * 1.6f);
    struct Limb { float ax, ay, bx, by; };
    Limb limbs[6];
    for (int i = 0; i < 6; ++i) {
        const float side = i % 2 ? 1.0f : -1.0f;
        const float spread = (0.20f + 0.16f * hash01(seed * 7u + i)) * h * side;
        const float up = (0.30f + 0.18f * hash01(seed * 11u + i)) * h;
        const float startY = topY + h * (0.10f - 0.04f * i);
        limbs[i] = {topX, startY, topX + (spread + sway) * reach, startY - up * reach};
        soft_line(s, limbs[i].ax, limbs[i].ay, limbs[i].bx, limbs[i].by, h * 0.028f, h * 0.010f, i % 3 ? bark : barkDark);
    }
    if (leaves < 0.6f)
        for (const auto &b : limbs)  // twigs and snow on the bare crown
            for (int j = 0; j < 3; ++j) {
                const float f = 0.45f + 0.25f * j;
                const float px = b.ax + (b.bx - b.ax) * f, py = b.ay + (b.by - b.ay) * f;
                const float dir = (j % 2 ? 1.0f : -1.0f) * h * 0.07f;
                soft_line(s, px, py, px + dir, py - h * 0.08f, h * 0.008f, h * 0.004f, barkDark);
                if (l.snow_cover > 0.05f)
                    s.blend(static_cast<int>(px), static_cast<int>(py) - 1, snowC, l.snow_cover * 0.8f);
            }
    if (leaves <= 0.02f)
        return;
    const Rgb leaf = l.lit(l.foliage({0.24f, 0.44f, 0.16f}), depth);
    const Rgb leafLight = l.lit(l.foliage({0.40f, 0.60f, 0.22f}), depth), leafDark = l.lit(l.foliage({0.12f, 0.26f, 0.10f}), depth);
    const float cx = topX, cy = base - h * 0.70f, R = h * 0.34f;
    const float lightSide = l.sun_x < 0.5f ? -1.0f : 1.0f;
    for (int i = 0; i < 11; ++i) {
        const float a = kTau * hash01(seed * 13u + i), r = R * (0.25f + 0.6f * hash01(seed * 17u + i));
        const float bx = cx + std::cos(a) * r * 1.15f + sway * 0.6f, by = cy + std::sin(a) * r * 0.75f;
        const float br = R * (0.42f + 0.22f * hash01(seed * 19u + i));
        soft_ellipse(s, bx, by + br * 0.18f, br, br * 0.85f, leafDark, leaves);
        soft_ellipse(s, bx, by, br * 0.92f, br * 0.78f, leaf, leaves);
        soft_ellipse(s, bx + lightSide * br * 0.30f, by - br * 0.28f, br * 0.45f, br * 0.36f, leafLight, leaves * 0.8f);
        if (l.snow_cover > 0.05f)
            soft_ellipse(s, bx, by - br * 0.62f, br * 0.6f, br * 0.22f, snowC, l.snow_cover * 0.75f);
    }
    // Spring blossom.
    if (l.spring > 0.05f)
        for (int i = 0; i < 46; ++i) {
            const float a = kTau * hash01(seed * 23u + i), r = R * 1.05f * std::sqrt(hash01(seed * 29u + i));
            const Rgb c = hash01(seed * 31u + i) < 0.5f ? Rgb{1.0f, 0.78f, 0.86f} : Rgb{1.0f, 0.95f, 0.97f};
            s.blend(static_cast<int>(cx + std::cos(a) * r * 1.15f + sway * 0.6f), static_cast<int>(cy + std::sin(a) * r * 0.8f),
                    l.lit(c, depth), l.spring * 0.9f);
        }
}

void paint_sky(const Surface &s, const Look &l, float horizon_y)
{
    const int rows = std::min(s.h, static_cast<int>(std::ceil(horizon_y)));
    const Point2 sun = sun_point(l, static_cast<float>(s.w), horizon_y);
    const float halo = (0.20f + 0.55f * l.twilight) * (1.0f - l.overcast) * clamp01((l.sun_elevation + 7.0f) / 7.0f);
    const float halo_r = horizon_y * (0.55f + 0.4f * l.twilight);
    const float flash = l.flash * 0.55f;
    for (int y = 0; y < rows; ++y) {
        const Rgb row = l.sky(static_cast<float>(y) / std::max(1.0f, horizon_y - 1.0f));
        for (int x = 0; x < s.w; ++x) {
            Rgb c = row;
            if (halo > 0.01f) {
                const float dx = (x + 0.5f - sun.x) * 0.8f, dy = y + 0.5f - sun.y;
                const float d = std::sqrt(dx * dx + dy * dy) / halo_r;
                if (d < 1.0f)
                    c = c + l.sun_color * (halo * (1.0f - d) * (1.0f - d) * 0.6f);
            }
            if (flash > 0.0f)
                c = c + Rgb{0.62f, 0.64f, 0.80f} * flash;
            s.set(x, y, c);
        }
    }
}

void paint_stars(const Surface &s, const Look &l, float max_y, float t, uint32_t seed, int count)
{
    const float vis = l.night * (1.0f - 0.92f * l.overcast) * (1.0f - l.fog);
    if (vis <= 0.02f)
        return;
    for (int i = 0; i < count; ++i) {
        const uint32_t id = seed * 7919u + static_cast<uint32_t>(i) * 2654435761u;
        const float x = hash01(id) * s.w, y = hash01(id + 1) * max_y;
        const float big = hash01(id + 2) < 0.12f ? 1.0f : 0.45f;
        const float tw = 0.4f + 0.6f * (0.5f + 0.5f * std::sin(t * (0.6f + 1.7f * hash01(id + 3)) + kTau * hash01(id + 4)));
        // Stars fade towards the horizon glow.
        const float fade = 1.0f - 0.6f * clamp01(y / std::max(1.0f, max_y));
        s.add(static_cast<int>(x), static_cast<int>(y), {0.82f, 0.86f, 1.0f}, vis * big * tw * fade);
    }
}

void paint_sun(const Surface &s, const Look &l, Point2 p, float r)
{
    // A pale disc can show through fog, never through rain clouds.
    const float vis = clamp01((l.sun_elevation + 1.5f) / 3.0f) * clamp01(1.0f - 1.25f * l.overcast + 0.5f * l.fog) *
                      (1.0f - 0.6f * l.fog) * (1.0f - l.rain) * (1.0f - 0.8f * l.snow);
    if (vis <= 0.01f)
        return;
    const Rgb core = mix(l.sun_color, {1.0f, 0.98f, 0.9f}, 0.55f);
    // A low sun is squashed a little by refraction and dimmer through the haze.
    const float squash = 1.0f - 0.12f * l.twilight;
    soft_ellipse(s, p.x, p.y, r, r * squash, core, vis);
}

void paint_moon(const Surface &s, const Look &l, Point2 p, float r)
{
    const float vis = clamp01(l.night * 1.6f - 0.45f) * (1.0f - 0.85f * l.overcast) * (1.0f - 0.7f * l.fog);
    if (vis <= 0.01f || r <= 0.0f)
        return;
    // Terminator: the lit part is where the projected x lies beyond k*sqrt(1-y²).
    const float phase = l.moon_phase;
    const float k = std::cos(phase * kTau);  // 1 new .. -1 full .. 1 new
    const bool waxing = phase < 0.5f;
    const Rgb lit{0.92f, 0.94f, 1.0f}, dark{0.10f, 0.12f, 0.20f};
    // Earthshine glow around the disc.
    for (int y = static_cast<int>(p.y - r * 2.2f); y <= static_cast<int>(p.y + r * 2.2f); ++y)
        for (int x = static_cast<int>(p.x - r * 2.2f); x <= static_cast<int>(p.x + r * 2.2f); ++x) {
            const float dx = (x + 0.5f - p.x) / r, dy = (y + 0.5f - p.y) / r;
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d > 2.2f)
                continue;
            if (d > 1.0f) {
                s.add(x, y, {0.35f, 0.42f, 0.65f}, vis * 0.10f * (2.2f - d) / 1.2f * (1.0f - k * 0.4f));
                continue;
            }
            const float edge = clamp01((1.0f - d) * r + 0.5f);
            const float limb = std::sqrt(std::max(0.0f, 1.0f - dy * dy));
            const float xs = waxing ? dx : -dx;  // lit side on +xs
            const float lit_amount = clamp01((xs - k * limb) * r * 0.9f + 0.5f);
            Rgb c = mix(dark, lit, lit_amount);
            // A couple of maria for texture.
            if (lit_amount > 0.5f && ((dx + 0.25f) * (dx + 0.25f) + (dy + 0.2f) * (dy + 0.2f) < 0.09f ||
                                      (dx - 0.2f) * (dx - 0.2f) + (dy - 0.3f) * (dy - 0.3f) < 0.05f))
                c = c * 0.86f;
            s.blend(x, y, c, edge * vis);
        }
}

void paint_clouds(const Surface &s, const Look &l, float t, float y0, float y1, float scroll, uint32_t seed)
{
    const float amount = l.clouds;
    if (amount <= 0.03f && l.overcast <= 0.03f)
        return;
    const float W = static_cast<float>(s.w), k = W / 128.0f;
    // Light on the clouds: white by day, sunset-tinted, dark blue-grey at night.
    const float day = l.daylight;
    const Rgb top = mix(mix(Rgb{0.16f, 0.18f, 0.28f} + Rgb{0.10f, 0.11f, 0.16f} * l.moon_light, Rgb{0.97f, 0.97f, 1.0f}, day),
                        mix(l.sun_color, Rgb{1.0f, 0.75f, 0.70f}, 0.4f), 0.55f * l.twilight * (1.0f - l.overcast));
    const float storm_dark = 1.0f - 0.45f * l.storm - 0.25f * l.overcast;
    const Rgb under = mix(top * 0.62f, l.horizon * 0.8f, 0.3f) * storm_dark;
    const Rgb lit_top = top * (1.0f - 0.35f * l.overcast) * (1.0f - 0.3f * l.storm);

    // Overcast sheet first: a textured grey layer across the cloud band.
    if (l.overcast > 0.05f) {
        // From the top of the panel down past the cloud band.
        const float bottom = y1 + (y1 - y0) * 0.5f;
        for (int y = 0; y < std::min(s.h, static_cast<int>(bottom)); ++y) {
            const float v = std::max(0.0f, (y - y0) / std::max(1.0f, y1 - y0));
            for (int x = 0; x < s.w; ++x) {
                // Billows that wander across the sheet, not row-wise stripes.
                const float band = 0.5f + 0.5f * std::sin(y * 0.30f + 2.2f * std::sin((x + scroll * 0.2f) * 0.043f + t * 0.04f) + t * 0.05f);
                const float n = 0.5f + 0.5f * std::sin((x + scroll * 0.2f) * 0.07f + t * 0.08f + y * 0.21f) *
                                           std::sin((x + scroll * 0.2f) * 0.031f - t * 0.05f);
                const Rgb c = mix(lit_top * 0.92f, under, clamp01(v * 0.8f + 0.25f * n));
                s.blend(x, y, c, l.overcast * clamp01(1.15f - v * 0.75f) * (0.55f + 0.3f * band * n));
            }
        }
    }

    // Under an overcast sheet or in fog, cumulus give way to a few flat,
    // darker stratus bands.
    const float puffy = clamp01(1.0f - 1.3f * l.overcast - 1.2f * l.fog);
    const int count = static_cast<int>(2.0f + amount * 7.0f);
    const float period = W + 90.0f * k;
    const float drift = (1.2f + 2.5f * std::fabs(l.wind)) * (l.wind < 0.0f ? -1.0f : 1.0f) * k;
    for (int i = 0; i < count; ++i) {
        const uint32_t id = seed * 131u + static_cast<uint32_t>(i) * 977u;
        const float speed = 0.6f + 0.8f * hash01(id + 3);
        const float cx = fract((hash01(id) * period + t * drift * speed - scroll * (0.05f + 0.05f * speed)) / period) * period - 45.0f * k;
        const float cy = y0 + (y1 - y0) * hash01(id + 1);
        const float size = (0.65f + 0.7f * hash01(id + 2)) * (0.8f + 0.5f * amount) * k;
        const float alpha = 0.55f + 0.4f * amount;
        // Flat-bottomed cumulus: darker base, a few lit puffs on top.
        soft_ellipse(s, cx, cy + 2.0f * size, 22.0f * size * (1.0f + 0.35f * (1.0f - puffy)), 4.0f * size * (0.85f + 0.15f * puffy), under,
                     alpha * (0.30f + 0.55f * puffy));
        if (puffy <= 0.05f)
            continue;
        for (int b = 0; b < 4; ++b) {
            const float bx = cx + (-13.0f + 9.0f * b + 4.0f * hash01(id + 10 + b)) * size;
            const float br = (5.0f + 4.0f * hash01(id + 20 + b)) * size;
            soft_ellipse(s, bx, cy - br * 0.3f, br * 1.2f, br * 0.75f, mix(under, lit_top, 0.65f), alpha * 0.9f * puffy);
            soft_ellipse(s, bx - br * 0.15f, cy - br * 0.55f, br * 0.8f, br * 0.45f, lit_top, alpha * 0.8f * puffy);
        }
    }
}

void paint_fog(const Surface &s, const Look &l, float t, float y0, float y1, float base)
{
    const float strength = clamp01(l.fog + base);
    if (strength <= 0.02f)
        return;
    const Rgb c = l.haze;
    const int a = std::max(0, static_cast<int>(y0)), b = std::min(s.h, static_cast<int>(std::ceil(y1)));
    const float span = std::max(1.0f, y1 - y0);
    std::vector<float> wave(static_cast<std::size_t>(s.w));
    for (int x = 0; x < s.w; ++x) wave[static_cast<std::size_t>(x)] = std::sin(x * 0.055f + t * 0.22f);
    for (int y = a; y < b; ++y) {
        const float v = (y - y0) / span;
        // Densest in the distance just under the top of the band, thinning
        // towards the viewer so the foreground keeps its contrast (depth).
        const float near = clamp01((v - 0.25f) / 0.75f);
        const float profile = clamp01(v * 4.0f) * (1.0f - 0.62f * near * near * (3.0f - 2.0f * near));
        const float row = std::sin(y * 0.23f - t * 0.13f);
        for (int x = 0; x < s.w; ++x) {
            const float n = 0.5f + 0.5f * wave[static_cast<std::size_t>(x)] * row;
            s.blend(x, y, c, strength * profile * (0.45f + 0.45f * n));
        }
    }
}

void Precipitation::update(float dt, const Look &l, int width, int height, const GroundFn &ground)
{
    // When rain or snow starts on a fresh scene, fill the whole panel at once
    // instead of watching the first flakes fall from the top.
    if (!warm_ && l.rain + l.snow > 0.05f) {
        warm_ = true;
        for (int i = 0; i < 60; ++i)
            update(0.1f, l, width, height, ground);
    }
    dt = std::clamp(dt, 0.0f, 0.1f);
    const float k = static_cast<float>(width) / 128.0f;
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    const float rain = l.rain, snow = l.snow;
    // Spawn rate in particles per second for the whole panel.
    const float rate = (rain * 250.0f + snow * 80.0f) * k;
    carry_ = rate > 0.0f ? carry_ + rate * dt : 0.0f;
    while (carry_ >= 1.0f) {
        carry_ -= 1.0f;
        if (static_cast<int>(drops_.size()) >= max_) {
            carry_ = 0.0f;  // saturated: drop the backlog instead of spawning it later
            break;
        }
        const bool is_snow = u(rng_) * (rain + snow) < snow;
        Drop d{};
        d.snow = is_snow;
        d.x = (u(rng_) * 1.4f - 0.2f) * width;
        d.y = -2.0f - u(rng_) * 6.0f;
        if (is_snow) {
            // depth: most flakes are far, small, faint and slow
            d.size = 0.15f + 0.85f * u(rng_);
            d.vy = (6.0f + 12.0f * d.size) * k;
            d.vx = l.wind * (8.0f + 8.0f * d.size) * k;
        } else {
            d.size = 0.15f + 0.85f * u(rng_);  // depth: near drops fall faster
            d.vy = (70.0f + 70.0f * d.size) * k;
            d.vx = l.wind * 40.0f * k;
        }
        d.life = u(rng_) * kTau;  // snow sway phase
        drops_.push_back(d);
    }
    for (auto &d : drops_) {
        if (d.splash) {
            d.life -= dt * 5.0f;
            continue;
        }
        d.life += dt;
        d.x += (d.vx + (d.snow ? std::sin(d.life * 1.7f) * 6.0f * k : 0.0f)) * dt;
        d.y += d.vy * dt;
        const float g = ground ? ground(d.x) : static_cast<float>(height);
        if (d.y >= g) {
            if (!d.snow && u(rng_) < 0.45f) {
                d.splash = true;
                d.y = g - 0.5f;
                d.life = 1.0f;
            } else {
                d.life = -100.0f;  // remove
                d.y = static_cast<float>(height + 10);
            }
        }
    }
    std::erase_if(drops_, [&](const Drop &d) {
        return (d.splash && d.life <= 0.0f) || d.y > height + 4 || d.x < -20 * k || d.x > width + 20 * k;
    });
}

void Precipitation::draw(const Surface &s, const Look &l) const
{
    const float k = static_cast<float>(s.w) / 128.0f;
    // Rain is lit by the sky: pale grey by day, faint blue at night, white in a flash.
    const Rgb rain_c = mix(mix(Rgb{0.42f, 0.48f, 0.62f}, Rgb{0.78f, 0.82f, 0.90f}, l.daylight), Rgb{1, 1, 1}, l.flash);
    const Rgb snow_c = mix(Rgb{0.62f, 0.66f, 0.80f}, Rgb{0.98f, 0.98f, 1.0f}, std::max(l.daylight, 0.35f));
    for (const auto &d : drops_) {
        if (d.splash) {
            const float a = 0.6f * d.life;
            s.blend(static_cast<int>(d.x - k), static_cast<int>(d.y), rain_c, a * 0.7f);
            s.blend(static_cast<int>(d.x + k), static_cast<int>(d.y), rain_c, a * 0.7f);
            s.blend(static_cast<int>(d.x), static_cast<int>(d.y - k), rain_c, a);
            continue;
        }
        if (d.snow) {
            const float near = d.size * d.size;
            s.blend(static_cast<int>(d.x), static_cast<int>(d.y), snow_c, 0.16f + 0.62f * near);
            if (d.size > 0.92f) {
                s.blend(static_cast<int>(d.x) + 1, static_cast<int>(d.y), snow_c, 0.40f);
                s.blend(static_cast<int>(d.x), static_cast<int>(d.y) + 1, snow_c, 0.40f);
            }
            continue;
        }
        // Rain streak along its velocity, fading towards the tail. size is
        // depth: most drops are far, faint and short, a few near ones bright.
        const float near = d.size * d.size;
        const float len = (2.0f + 4.0f * near) * k;
        const float vlen = std::sqrt(d.vx * d.vx + d.vy * d.vy);
        const float ux = d.vx / vlen, uy = d.vy / vlen;
        const int steps = static_cast<int>(len) + 1;
        for (int i = 0; i < steps; ++i) {
            const float f = static_cast<float>(i) / steps;
            s.blend(static_cast<int>(d.x - ux * i), static_cast<int>(d.y - uy * i), rain_c, (0.07f + 0.30f * near) * (1.0f - f));
        }
    }
}

void SeasonalDrift::update(float dt, const Look &l, int width, int height, const GroundFn &ground, float density)
{
    dt = std::clamp(dt, 0.0f, 0.1f);
    const float k = static_cast<float>(width) / 128.0f;
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    // Leaves fall in autumn (more with wind), petals in spring; rain and snow
    // keep them on the trees.
    const float calm = 1.0f - std::min(1.0f, l.rain + l.snow);
    const float leaves = l.autumn * (0.8f + 1.2f * std::fabs(l.wind)) * calm;
    const float petals = l.spring * 0.9f * calm;
    carry_ += (leaves + petals) * 2.2f * density * k * dt;
    while (carry_ >= 1.0f && bits_.size() < 90) {
        carry_ -= 1.0f;
        Bit b{};
        const bool leaf = u(rng_) * (leaves + petals) < leaves;
        b.kind = leaf ? Kind::Leaf : Kind::Petal;
        b.x = (u(rng_) * 1.3f - 0.15f) * width;
        b.y = -2.0f - u(rng_) * height * 0.3f;
        b.vy = (leaf ? 7.0f + 6.0f * u(rng_) : 4.0f + 4.0f * u(rng_)) * k;
        b.vx = l.wind * 12.0f * k;
        b.phase = u(rng_) * kTau;
        b.life = 0.0f;
        static const Rgb leafC[] = {{0.85f, 0.42f, 0.10f}, {0.78f, 0.22f, 0.08f}, {0.90f, 0.66f, 0.16f}, {0.55f, 0.30f, 0.12f}};
        static const Rgb petalC[] = {{1.0f, 0.78f, 0.86f}, {1.0f, 0.92f, 0.95f}, {0.98f, 0.66f, 0.80f}};
        b.color = leaf ? leafC[static_cast<int>(u(rng_) * 3.99f)] : petalC[static_cast<int>(u(rng_) * 2.99f)];
        bits_.push_back(b);
    }
    carry_ = std::min(carry_, 2.0f);
    // Fireflies: warm, dry summer nights.
    const float flies = l.summer * l.night * calm * (1.0f - l.fog) * std::clamp((l.temperature - 8.0f) / 8.0f, 0.0f, 1.0f);
    const int want = static_cast<int>(flies * 14.0f * density * k);
    int have = 0;
    for (const auto &b : bits_) have += b.kind == Kind::Firefly;
    if (have < want) {
        Bit b{};
        b.kind = Kind::Firefly;
        b.x = u(rng_) * width;
        b.y = height * (0.55f + 0.4f * u(rng_));
        b.vx = (u(rng_) - 0.5f) * 6.0f * k;
        b.vy = (u(rng_) - 0.5f) * 3.0f * k;
        b.phase = u(rng_) * kTau;
        b.color = {0.85f, 1.0f, 0.35f};
        bits_.push_back(b);
    }
    bool dropFly = have > want;
    for (auto &b : bits_) {
        b.life += dt;
        if (b.kind == Kind::Firefly) {
            if (dropFly) {
                b.life = 1e9f;
                dropFly = false;
                continue;
            }
            b.vx += (u(rng_) - 0.5f) * 8.0f * k * dt;
            b.vy += (u(rng_) - 0.5f) * 6.0f * k * dt;
            b.vx = std::clamp(b.vx, -4.0f * k, 4.0f * k);
            b.vy = std::clamp(b.vy, -3.0f * k, 3.0f * k);
            b.x += b.vx * dt;
            b.y += b.vy * dt;
            if (b.y < height * 0.45f)
                b.vy = std::fabs(b.vy);
            continue;
        }
        if (b.landed)
            continue;
        // Flutter: sideways sway that also brakes the fall.
        const float sway = std::sin(b.life * 2.4f + b.phase);
        b.x += (b.vx + sway * 7.0f * k) * dt;
        b.y += b.vy * (0.65f + 0.35f * std::fabs(std::cos(b.life * 2.4f + b.phase))) * dt;
        const float g = ground ? ground(b.x) : static_cast<float>(height) + 4.0f;
        if (b.y >= g) {
            b.y = g;
            b.landed = true;
            b.life = 0.0f;
        }
    }
    std::erase_if(bits_, [&](const Bit &b) {
        if (b.kind == Kind::Firefly)
            return b.life > 1e8f || b.x < -8 * k || b.x > width + 8 * k || b.y > height + 4;
        return (b.landed && b.life > 6.0f) || b.x < -12 * k || b.x > width + 12 * k || b.y > height + 4;
    });
}

void SeasonalDrift::draw(const Surface &s, const Look &l) const
{
    for (const auto &b : bits_) {
        const int x = static_cast<int>(b.x), y = static_cast<int>(b.y);
        if (b.kind == Kind::Firefly) {
            const float blink = std::max(0.0f, std::sin(b.life * 1.3f + b.phase));
            const float a = blink * blink;
            s.add(x, y, b.color, 0.9f * a);
            s.add(x + 1, y, b.color, 0.25f * a);
            s.add(x - 1, y, b.color, 0.25f * a);
            s.add(x, y - 1, b.color, 0.25f * a);
            s.add(x, y + 1, b.color, 0.25f * a);
            continue;
        }
        const Rgb c = l.lit(b.color, 0.1f);
        const float fade = b.landed ? clamp01(1.0f - (b.life - 4.0f) / 2.0f) : 1.0f;
        s.blend(x, y, c, 0.95f * fade);
        // Leaves tumble: a second pixel that flips side.
        if (b.kind == Kind::Leaf && !b.landed)
            s.blend(x + (std::sin(b.life * 4.0f + b.phase) > 0 ? 1 : -1), y, c * 0.8f, 0.7f);
    }
}

void paint_window(const Surface &s, const Look &l, int x, int y, int w, int h, uint32_t seed, float t)
{
    // Some rooms stay dark; the rest glow warm with a little variation.
    const bool on = hash01(seed * 7u + 1u) < 0.78f;
    // Rooms light up a little before the street lamps do.
    const float lamp = on ? std::min(1.0f, l.lamp() * 1.6f) : 0.0f;
    const float warmth = hash01(seed * 13u + 5u);
    const Rgb light = mix(Rgb{1.0f, 0.72f, 0.36f}, Rgb{1.0f, 0.84f, 0.55f}, warmth);
    const Rgb glass = l.lit(mix(Rgb{0.16f, 0.20f, 0.26f}, l.sky(0.4f), 0.45f), 0.0f);
    const float flicker = 0.92f + 0.08f * std::sin(t * (1.3f + warmth) + seed);
    for (int py = y; py < y + h; ++py)
        for (int px = x; px < x + w; ++px) {
            const float v = h > 1 ? static_cast<float>(py - y) / (h - 1) : 0.0f;
            // Lamplight is brighter low in the room; glass catches the sky at the top.
            const Rgb lit_c = light * ((0.75f + 0.25f * v) * flicker);
            const Rgb day_c = mix(glass * 1.25f, glass * 0.8f, v);
            s.set(px, py, mix(day_c, lit_c, lamp));
        }
    // Glow spilling out at night.
    if (lamp > 0.05f) {
        const float cx = x + w * 0.5f, cy = y + h * 0.5f;
        const float r = std::max(w, h) * 1.4f;
        for (int py = static_cast<int>(cy - r); py <= static_cast<int>(cy + r); ++py)
            for (int px = static_cast<int>(cx - r); px <= static_cast<int>(cx + r); ++px) {
                if (px >= x && px < x + w && py >= y && py < y + h)
                    continue;
                const float d = std::sqrt((px + 0.5f - cx) * (px + 0.5f - cx) + (py + 0.5f - cy) * (py + 0.5f - cy)) / r;
                if (d < 1.0f)
                    s.add(px, py, light, 0.10f * lamp * (1.0f - d) * (1.0f - d));
            }
    }
}

void paint_string_lights(const Surface &s, const Look &l, Point2 a, Point2 b, float sag, float t, int bulbs,
                         uint32_t seed)
{
    static const Rgb colors[] = {{1.0f, 0.18f, 0.12f}, {0.20f, 0.95f, 0.30f}, {1.0f, 0.78f, 0.15f}, {0.25f, 0.45f, 1.0f}};
    auto at = [&](float f) -> Point2 {
        return {a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f + sag * 4.0f * f * (1.0f - f)};
    };
    const float span = std::max(std::fabs(b.x - a.x), std::fabs(b.y - a.y)) + 1.0f;
    const Rgb wire = l.lit({0.10f, 0.10f, 0.08f}, 0.0f);
    for (int i = 0; i <= static_cast<int>(span); ++i) {
        const Point2 p = at(i / span);
        s.blend(static_cast<int>(p.x), static_cast<int>(p.y), wire, 0.6f);
    }
    const float glow = 0.35f + 0.65f * l.lamp();
    for (int i = 0; i < bulbs; ++i) {
        const Point2 p = at((i + 0.5f) / bulbs);
        const Rgb c = colors[(i + seed) % 4];
        const float tw = 0.75f + 0.25f * std::sin(t * 2.1f + i * 1.7f + seed);
        const int x = static_cast<int>(p.x), y = static_cast<int>(p.y) + 1;
        s.blend(x, y, c, 0.9f * std::min(1.0f, glow + 0.2f));
        s.add(x, y, c, 0.3f * glow * tw);
        s.add(x - 1, y, c, 0.18f * l.lamp() * tw);
        s.add(x + 1, y, c, 0.18f * l.lamp() * tw);
        s.add(x, y + 1, c, 0.18f * l.lamp() * tw);
        s.add(x, y - 1, c, 0.10f * l.lamp() * tw);
    }
}

void Lightning::update_and_draw(const Surface &s, const Look &l, float dt, float horizon_y)
{
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    if (l.bolt) {
        path_.clear();
        float x = (0.15f + 0.7f * u(rng_)) * s.w, y = 0.0f;
        const float end = horizon_y * (0.75f + 0.25f * u(rng_));
        const float step = std::max(2.0f, s.w / 40.0f);
        path_.push_back({x, y});
        while (y < end) {
            y += step * (0.6f + 0.8f * u(rng_));
            x += (u(rng_) - 0.5f) * step * 1.8f;
            path_.push_back({x, std::min(y, end)});
        }
        life_ = 1.0f;
    }
    if (life_ <= 0.0f || path_.size() < 2)
        return;
    life_ -= dt * 4.0f;
    const float a = std::max(0.0f, life_) * (life_ > 0.5f ? 1.0f : 0.6f);
    const Rgb core{0.95f, 0.96f, 1.0f}, glow{0.55f, 0.60f, 0.95f};
    for (std::size_t i = 1; i < path_.size(); ++i) {
        const Point2 p = path_[i - 1], q = path_[i];
        const float dx = q.x - p.x, dy = q.y - p.y;
        const int n = static_cast<int>(std::max(std::fabs(dx), std::fabs(dy))) + 1;
        for (int j = 0; j <= n; ++j) {
            const float x = p.x + dx * j / n, y = p.y + dy * j / n;
            s.blend(static_cast<int>(x), static_cast<int>(y), core, a);
            s.add(static_cast<int>(x) - 1, static_cast<int>(y), glow, a * 0.3f);
            s.add(static_cast<int>(x) + 1, static_cast<int>(y), glow, a * 0.3f);
        }
    }
}

} // namespace Environment
