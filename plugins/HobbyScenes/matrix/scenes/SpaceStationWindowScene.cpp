#include "SpaceStationWindowScene.h"

#include <future>

namespace Hobbies {

// Equirectangular planet textures built from 3D noise on the sphere (no seams
// at the date line, no pinching at the poles).
struct Planet {
    static constexpr int W = 512, H = 256;
    std::vector<float> surface;  // RGB albedo by day
    std::vector<float> clouds, lights, water;
};

namespace {

// Planet in design space (128 × 128): a big disc whose top shows as a curved
// limb across the lower part of the window.
constexpr float CenterBelow = 111.0f;  // planet centre below the screen centre
constexpr float Radius = 128.0f;
constexpr float WindowInset = 9.0f;  // cabin wall around the window
constexpr float WindowCorner = 20.0f;
constexpr float FrameWidth = 7.0f;
constexpr float SpinRate = 0.0022f;   // planet turns per second (texture widths)
constexpr float CloudRate = 0.0006f;  // extra cloud drift
constexpr float LimbInside = 7.0f, LimbOutside = 14.0f;

float sq(float x)
{
    return x * x;
}

uint32_t hash3(int x, int y, int z)
{
    uint32_t h = static_cast<uint32_t>(x) * 374761393U + static_cast<uint32_t>(y) * 668265263U + static_cast<uint32_t>(z) * 1274126177U;
    h = (h ^ (h >> 13)) * 1274126177U;
    return h ^ (h >> 16);
}

float lattice(int x, int y, int z)
{
    return static_cast<float>(hash3(x, y, z) & 0xFFFFFFU) / 16777215.0f;
}

float value_noise(float x, float y, float z)
{
    const float fx0 = std::floor(x), fy0 = std::floor(y), fz0 = std::floor(z);
    const int ix = static_cast<int>(fx0), iy = static_cast<int>(fy0), iz = static_cast<int>(fz0);
    float fx = x - fx0, fy = y - fy0, fz = z - fz0;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    fz = fz * fz * (3 - 2 * fz);
    const auto row = [&](int dy, int dz) {
        const float a = lattice(ix, iy + dy, iz + dz), b = lattice(ix + 1, iy + dy, iz + dz);
        return a + (b - a) * fx;
    };
    const float y0 = row(0, 0) + (row(1, 0) - row(0, 0)) * fy;
    const float y1 = row(0, 1) + (row(1, 1) - row(0, 1)) * fy;
    return y0 + (y1 - y0) * fz;
}

float fbm(float x, float y, float z, int octaves, float base)
{
    float sum = 0, amp = 1, freq = base, norm = 0;
    for (int o = 0; o < octaves; ++o) {
        sum += amp * value_noise(x * freq + o * 17.1f, y * freq + o * 5.3f, z * freq + o * 11.7f);
        norm += amp;
        amp *= 0.5f;
        freq *= 2.03f;
    }
    return sum / norm;
}

Rgb lerp_rgb(Rgb a, Rgb b, float t)
{
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

Planet make_planet()
{
    Planet p;
    const int n = Planet::W * Planet::H;
    p.surface.resize(static_cast<size_t>(n) * 3);
    p.clouds.resize(static_cast<size_t>(n));
    p.lights.resize(static_cast<size_t>(n));
    p.water.resize(static_cast<size_t>(n));
    const Rgb green{0.16f, 0.30f, 0.12f}, forest{0.10f, 0.20f, 0.09f}, sand{0.62f, 0.50f, 0.32f};
    const Rgb tundra{0.38f, 0.36f, 0.30f}, ice{0.74f, 0.76f, 0.80f};
    const Rgb deep{0.02f, 0.07f, 0.20f}, shallow{0.07f, 0.30f, 0.42f};
    for (int ty = 0; ty < Planet::H; ++ty) {
        const float lat = (0.5f - (ty + 0.5f) / Planet::H) * kPi;
        const float alat = std::abs(lat) * 180.0f / kPi;
        const float band =
            0.10f * std::exp(-sq((alat - 4) / 6)) + 0.12f * std::exp(-sq((alat - 55) / 10)) - 0.08f * std::exp(-sq((alat - 25) / 8));
        for (int tx = 0; tx < Planet::W; ++tx) {
            const float lon = (tx + 0.5f) / Planet::W * kTau;
            const float x = std::cos(lat) * std::cos(lon), y = std::sin(lat), z = std::cos(lat) * std::sin(lon);
            const float warp = fbm(x, y, z, 3, 1.3f), warp2 = fbm(x, y, z, 3, 1.43f);
            const float height = fbm(x + 0.6f * warp, y + 0.6f * warp2, z - 0.5f * warp, 6, 1.6f);
            // Inland seas and lakes cut into the larger continents.
            const float lakes = smoothstep(0.64f, 0.67f, fbm(x + 5, y - 2, z + 1, 4, 3.2f)) * smoothstep(0.55f, 0.60f, height);
            const float land = smoothstep(0.50f, 0.52f, height) * (1.0f - lakes);
            const float coast = smoothstep(0.475f, 0.495f, height) * (1 - land) * (1.0f - lakes);
            const float detail = fbm(x, y, z, 4, 6.0f);
            const float moist = fbm(x + 3, y, z, 4, 2.2f);
            const float desert = smoothstep(0.5f, 0.65f, moist * 0.6f + 0.4f * std::clamp(1 - std::abs(alat - 24) / 20, 0.0f, 1.0f)) *
                                 (1.0f - smoothstep(36, 48, alat));
            const float patches = fbm(x - 2, y + 1, z, 3, 4.5f);
            const Rgb ochre{0.52f, 0.36f, 0.22f}, steppe{0.40f, 0.40f, 0.24f};
            const Rgb dryland = lerp_rgb(sand, ochre, smoothstep(0.4f, 0.65f, patches));
            Rgb c = lerp_rgb(lerp_rgb(green, forest, smoothstep(0.3f, 0.7f, detail)), steppe, 0.6f * smoothstep(0.5f, 0.7f, patches));
            c = lerp_rgb(c, dryland, desert);
            c = lerp_rgb(c, tundra, smoothstep(52, 62, alat + 8 * detail));
            c = lerp_rgb(lerp_rgb(deep, shallow, coast), c, land);
            const float icecap = smoothstep(68, 74, alat + 6 * detail);
            c = lerp_rgb(c, ice, icecap);

            const float swirl = fbm(x, y * 2.5f, z, 3, 1.5f);
            const float cloud = fbm(x + 1.2f * swirl, y + 0.6f * swirl, z - 1.2f * swirl, 6, 3.6f) +
                                0.08f * (fbm(x - 6, y * 2.0f, z + 2, 3, 14.0f) - 0.5f);  // breaks up the big decks

            const float cities = fbm(x + 7, y, z, 3, 30.0f), cluster = fbm(x - 4, y, z, 3, 2.0f);
            const int i = ty * Planet::W + tx;
            const float spark = std::pow(hash01(static_cast<uint32_t>(i) * 2654435761U + 5U), 10.0f);
            const float coastal = smoothstep(0.45f, 0.53f, height) * (1 - smoothstep(0.53f, 0.6f, height));
            const float road = 1.0f - std::abs(2.0f * fbm(x + 9, y, z - 3, 3, 9.0f) - 1.0f);  // thin ridges link the towns
            float light =
                land * (1 - icecap) * (0.5f * smoothstep(0.62f, 0.75f, cities) + 1.5f * spark + 0.18f * smoothstep(0.94f, 0.98f, road));
            light *= smoothstep(0.5f, 0.65f, cluster + 0.15f * coastal) * (1 - smoothstep(55, 65, alat)) * (1 - 0.7f * desert);

            p.surface[static_cast<size_t>(i) * 3] = c.r;
            p.surface[static_cast<size_t>(i) * 3 + 1] = c.g;
            p.surface[static_cast<size_t>(i) * 3 + 2] = c.b;
            p.clouds[static_cast<size_t>(i)] = 0.9f * smoothstep(0.57f, 0.77f, cloud + band);
            p.lights[static_cast<size_t>(i)] = light;
            p.water[static_cast<size_t>(i)] = (1 - land) * (1 - icecap);
        }
    }
    return p;
}

// Built once per process on a worker thread, so the first frame never waits
// for it unless the scene starts right away.
std::shared_future<std::shared_ptr<const Planet>>& planet_future()
{
    static std::shared_future<std::shared_ptr<const Planet>> future =
        std::async(std::launch::async, [] { return std::make_shared<const Planet>(make_planet()); }).share();
    return future;
}

// Bilinear lookup with the texture wrapping around in longitude.
struct Tap {
    size_t i00, i10, i01, i11;
    float fx, fy;
};
Tap tap(float tx, float ty)
{
    tx -= std::floor(tx / Planet::W) * Planet::W;
    const int x0 = std::min(static_cast<int>(tx), Planet::W - 1), y0 = static_cast<int>(ty);
    const int x1 = (x0 + 1) % Planet::W, y1 = std::min(y0 + 1, Planet::H - 1);
    return {static_cast<size_t>(y0 * Planet::W + x0),
            static_cast<size_t>(y0 * Planet::W + x1),
            static_cast<size_t>(y1 * Planet::W + x0),
            static_cast<size_t>(y1 * Planet::W + x1),
            tx - x0,
            ty - y0};
}
float sample(const std::vector<float>& t, const Tap& s)
{
    const float top = t[s.i00] + (t[s.i10] - t[s.i00]) * s.fx;
    const float bottom = t[s.i01] + (t[s.i11] - t[s.i01]) * s.fx;
    return top + (bottom - top) * s.fy;
}
Rgb sample_rgb(const std::vector<float>& t, const Tap& s)
{
    Rgb c;
    float* out[3] = {&c.r, &c.g, &c.b};
    for (size_t k = 0; k < 3; ++k) {
        const float top = t[s.i00 * 3 + k] + (t[s.i10 * 3 + k] - t[s.i00 * 3 + k]) * s.fx;
        const float bottom = t[s.i01 * 3 + k] + (t[s.i11 * 3 + k] - t[s.i01 * 3 + k]) * s.fx;
        *out[k] = top + (bottom - top) * s.fy;
    }
    return c;
}

void normalize3(float v[3])
{
    const float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (len > 1e-6f)
        for (int k = 0; k < 3; ++k) v[k] /= len;
}

}  // namespace

SpaceStationWindowScene::SpaceStationWindowScene()
{
    Environment::Adapt adapt;
    adapt.weather = false;  // above the weather
    adapt.season = false;
    use_environment({Environment::TimeOfDay::Dusk, Environment::Weather::Clear, Environment::Season::Summer}, adapt);
    planet_future();  // start building the planet now
}

void SpaceStationWindowScene::initialize(int width, int height)
{
    HobbySceneBase::initialize(width, height);
    planet_ = planet_future().get();
    build_geometry();
    flashes_.clear();
    satLife_ = 0;
    satWait_ = 4.0f + 6.0f * rnd();
    aurora_ = auroraTarget_ = 0;
    auroraWait_ = 3.0f + 5.0f * rnd();
}

void SpaceStationWindowScene::build_geometry()
{
    const int w = cv_.w, h = cv_.h;
    const float k = std::min(w, h) / 128.0f;
    scale_ = k;
    cx_ = w * 0.5f;
    cy_ = h * 0.5f + CenterBelow * k;
    radius_ = Radius * k;
    const size_t n = static_cast<size_t>(w * h);
    glass_.assign(n, 0.0f);
    frame_.assign(n * 3, 0.0f);
    rimLight_.assign(n, 0.0f);
    earth_.clear();
    limb_.clear();
    stars_.clear();

    // Window: a rounded rectangle in a bevelled metal frame set in the cabin wall.
    const float hx = w * 0.5f - WindowInset * k, hy = h * 0.5f - WindowInset * k, corner = WindowCorner * k;
    const auto window_sdf = [&](float x, float y) {
        const float qx = std::abs(x - w * 0.5f) - (hx - corner), qy = std::abs(y - h * 0.5f) - (hy - corner);
        const float ox = std::max(qx, 0.0f), oy = std::max(qy, 0.0f);
        return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - corner;
    };
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const size_t i = static_cast<size_t>(y * w + x);
            const float d = window_sdf(x + 0.5f, y + 0.5f) / k;  // design px outside the glass
            glass_[i] = std::clamp(0.5f - d * k, 0.0f, 1.0f);
            const float vertical = (y + 0.5f) / h;
            Rgb c{0.055f, 0.058f, 0.068f};  // cabin wall
            c = scale(c, 1.15f - 0.35f * vertical);
            // Frame ring: lighter metal, rounded so its top catches the cabin light.
            const float ring = std::clamp(1.0f - std::abs(d - FrameWidth * 0.5f) / (FrameWidth * 0.5f), 0.0f, 1.0f);
            const float roundness = std::sqrt(ring);
            const Rgb metal{0.21f, 0.22f, 0.245f};
            c = lerp_rgb(c, scale(metal, 0.55f + 0.25f * (1.0f - vertical)), smoothstep(0.0f, 0.25f, ring) * (0.6f + 0.4f * roundness));
            // Dark seal between glass and frame, then the bright inner lip.
            c = scale(c, 1.0f - 0.6f * std::exp(-sq((d - 0.6f) / 0.5f)));
            c = lerp_rgb(c, Rgb{0.36f, 0.38f, 0.42f}, 0.3f * std::exp(-sq((d - 1.6f) / 0.6f)));
            frame_[i * 3] = c.r;
            frame_[i * 3 + 1] = c.g;
            frame_[i * 3 + 2] = c.b;
            // Earthshine comes up through the window and lights the inner lip,
            // most on the top edge, which faces the planet.
            rimLight_[i] = std::exp(-sq((d - 1.8f) / 1.4f)) * (0.35f + 0.65f * (1.0f - vertical)) * (d > 0.0f ? 1.0f : 0.0f);
        }
    // Bolts around the frame.
    const auto bolt = [&](float bx, float by) {
        const int x0 = static_cast<int>(bx - 2 * k), x1 = static_cast<int>(bx + 2 * k) + 1;
        const int y0 = static_cast<int>(by - 2 * k), y1 = static_cast<int>(by + 2 * k) + 1;
        for (int y = std::max(0, y0); y < std::min(h, y1); ++y)
            for (int x = std::max(0, x0); x < std::min(w, x1); ++x) {
                const float dx = (x + 0.5f - bx) / k, dy = (y + 0.5f - by) / k;
                const float a = std::clamp(1.4f - std::sqrt(dx * dx + dy * dy), 0.0f, 1.0f);
                const float lightness = 0.5f + 0.5f * std::clamp(-(dx + dy) * 0.7f, -1.0f, 1.0f);
                const size_t i = static_cast<size_t>(y * w + x);
                for (size_t c = 0; c < 3; ++c) frame_[i * 3 + c] += (0.15f + 0.3f * lightness - frame_[i * 3 + c]) * a;
            }
    };
    const float ringOffset = FrameWidth * 0.5f * k, straightX = hx - corner, straightY = hy - corner;
    for (float t : {-1.0f, 0.0f, 1.0f}) {
        bolt(w * 0.5f + t * straightX * 0.85f, h * 0.5f - hy - ringOffset);
        bolt(w * 0.5f + t * straightX * 0.85f, h * 0.5f + hy + ringOffset);
        bolt(w * 0.5f - hx - ringOffset, h * 0.5f + t * straightY * 0.85f);
        bolt(w * 0.5f + hx + ringOffset, h * 0.5f + t * straightY * 0.85f);
    }
    for (int sx : {-1, 1})
        for (int sy : {-1, 1}) {
            const float r = corner + ringOffset;
            bolt(w * 0.5f + sx * (straightX + r * 0.7071f), h * 0.5f + sy * (straightY + r * 0.7071f));
        }
    // Status lights on the wall below the window.
    leds_ = {{w * 0.5f - 6 * k, h - 2.5f * k}, {w * 0.5f - 2 * k, h - 2.5f * k}, {w * 0.5f + 2 * k, h - 2.5f * k}};

    // Earth's axis leans across the view, so the land drifts in at a slant.
    const float ax = -0.9f, az = 0.5f;
    const float cxr = std::cos(ax), sxr = std::sin(ax), czr = std::cos(az), szr = std::sin(az);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const size_t i = static_cast<size_t>(y * w + x);
            if (glass_[i] <= 0.0f)
                continue;
            const float dx = (x + 0.5f - cx_) / radius_, dy = (y + 0.5f - cy_) / radius_;
            const float r = std::sqrt(dx * dx + dy * dy);
            const float outside = (r - 1.0f) * Radius;  // design px
            if (outside > -LimbInside && outside < LimbOutside)
                limb_.push_back({static_cast<int>(i), dx / r, dy / r, outside, std::atan2(dy, dx)});
            const float cover = std::clamp((1.0f - r) * radius_ + 0.5f, 0.0f, 1.0f);
            if (cover <= 0.0f) {
                // Stars: a fixed scatter over open space.
                const float hsh = hash01(static_cast<uint32_t>(i) * 747796405U + 11U);
                if (hsh > 1.0f - 0.004f / (k * k) && r > 1.02f)
                    stars_.push_back({static_cast<int>(i), 0.35f + 0.65f * hash01(static_cast<uint32_t>(i) + 99U),
                                      kTau * hash01(static_cast<uint32_t>(i) + 7U)});
                continue;
            }
            const float rr = std::min(r, 1.0f);
            const float nx = dx / std::max(r, 1e-6f) * rr, ny = dy / std::max(r, 1e-6f) * rr;
            const float nz = std::sqrt(std::max(0.0f, 1.0f - rr * rr));
            // Earth frame: rotate about x, then about z.
            const float y1 = cxr * ny - sxr * nz, z1 = sxr * ny + cxr * nz;
            const float ex = czr * nx - szr * y1, ey = szr * nx + czr * y1, ez = z1;
            const float lat = std::asin(std::clamp(ey, -1.0f, 1.0f)), lon = std::atan2(ez, ex);
            const float tx = lon / kTau * Planet::W - 0.5f;
            const float ty = std::clamp((0.5f - lat / kPi) * Planet::H - 0.5f, 0.0f, Planet::H - 1.001f);
            earth_.push_back({static_cast<int>(i), nx, ny, nz, tx, ty, cover});
        }
    const float vy = (h * 0.5f - cy_) / radius_;
    view_[0] = 0.0f;
    view_[1] = vy;
    view_[2] = std::sqrt(std::max(0.0f, 1.0f - vy * vy));
}

void SpaceStationWindowScene::draw_stars()
{
    const float exposure = 0.1f + 0.75f * (1.0f - sunlit_);  // bright Earth in view hides faint stars
    for (const auto& s : stars_) {
        const float b = s.brightness * exposure * (0.85f + 0.15f * std::sin(t_ * 1.3f + s.phase));
        float* p = &cv_.px[static_cast<size_t>(s.index) * 3];
        p[0] += b * 0.9f;
        p[1] += b * 0.93f;
        p[2] += b;
    }
}

void SpaceStationWindowScene::draw_earth(const float sun[3])
{
    const Planet& pl = *planet_;
    const auto& L = env();
    const bool fine = render_quality_scale() > 0.8f;
    const float spin = spin_ * Planet::W, drift = (spin_ + cloudDrift_) * Planet::W;
    float half[3] = {sun[0], sun[1], sun[2] + 1.0f};
    normalize3(half);
    const Rgb cloudWhite{0.88f, 0.9f, 0.95f}, atmosphere{0.35f, 0.6f, 1.0f};
    const Rgb city{1.0f, 0.72f, 0.35f}, moonlit{0.6f, 0.7f, 1.0f};
    const float nightClouds = 0.05f + 0.08f * L.moon_light;
    float daySum = 0.0f;
    for (const auto& e : earth_) {
        Tap s = tap(e.tx + spin, e.ty), c = tap(e.tx + drift, e.ty);
        if (!fine)
            s.fx = s.fy = c.fx = c.fy = 0.0f;
        const Rgb ground = sample_rgb(pl.surface, s);
        const float cl = sample(pl.clouds, c);
        const float ndl = e.nx * sun[0] + e.ny * sun[1] + e.nz * sun[2];
        const float day = smoothstep(-0.06f, 0.18f, ndl);
        daySum += day;

        Rgb lit{};
        if (day > 0.0f) {
            const Rgb col = lerp_rgb(ground, scale(cloudWhite, 0.62f + 0.38f * cl * cl), cl);
            const float lambert = ndl > 0.0f ? std::pow(ndl, 0.7f) : 0.0f;
            lit = scale(col, 0.25f + 0.85f * lambert);
            const float term = smoothstep(-0.04f, 0.01f, ndl) * (1.0f - smoothstep(0.01f, 0.07f, ndl));
            lit = {lit.r * (1.0f + 0.6f * term), lit.g * (1.0f - 0.05f * term), lit.b * (1.0f - 0.35f * term)};
            // Sun glint on open water.
            const float ndh = e.nx * half[0] + e.ny * half[1] + e.nz * half[2];
            if (fine && ndh > 0.93f) {
                const float glint = std::pow(ndh, 80.0f) * sample(pl.water, s) * (1.0f - cl) * day * 0.9f;
                lit.r += glint;
                lit.g += glint * 0.9f;
                lit.b += glint * 0.75f;
            }
            // Bright haze toward the limb.
            const float edge = 1.0f - e.nz, haze = edge * edge * std::sqrt(edge);
            lit = lerp_rgb(lit, scale(atmosphere, 0.7f / 0.6f), 0.6f * haze);
        }
        Rgb dark{};
        if (day < 1.0f) {
            const float li = sample(pl.lights, s) * 1.3f * (1.0f - 0.85f * cl);
            // Cloud tops stay faintly visible: moonlight, plus a violet-grey dusk just past the terminator.
            const float dusk = smoothstep(-0.15f, -0.02f, ndl) * 0.07f;
            const float glowCl = cl * (nightClouds + dusk);
            dark = {city.r * li + moonlit.r * glowCl + 0.55f * dusk * cl, city.g * li + moonlit.g * glowCl,
                    city.b * li + moonlit.b * glowCl + 0.15f * dusk * cl};
            // A dim blue-grey rim of atmosphere outlines the night side.
            const float edge = 1.0f - e.nz;
            const float rim = edge * edge * 0.12f + 0.02f;
            dark.r += 0.25f * rim + 0.008f;
            dark.g += 0.012f;
            dark.b += 0.028f * (1.0f - cl);
            dark.g += 0.35f * rim;
            dark.b += 0.6f * rim;
        }
        const Rgb out = lerp_rgb(dark, lit, day);
        float* p = &cv_.px[static_cast<size_t>(e.index) * 3];
        p[0] += (out.r - p[0]) * e.cover;
        p[1] += (out.g - p[1]) * e.cover;
        p[2] += (out.b - p[2]) * e.cover;
    }
    sunlit_ = earth_.empty() ? 0.0f : daySum / static_cast<float>(earth_.size());
}

void SpaceStationWindowScene::draw_limb(const float sun[3], float dt)
{
    // Aurora: now and then a band of curtains wakes up along the night-side limb.
    auroraWait_ -= dt;
    if (auroraTarget_ > 0.0f) {
        auroraTime_ -= dt;
        if (auroraTime_ <= 0.0f) {
            auroraTarget_ = 0.0f;
            auroraWait_ = (env().night > 0.5f ? 15.0f : 30.0f) + 30.0f * rnd();
        }
    }
    else if (auroraWait_ <= 0.0f) {
        auroraTarget_ = 0.6f + 0.4f * rnd();
        auroraTime_ = 18.0f + 18.0f * rnd();
        auroraCenter_ = -0.5f * kPi + (rnd() - 0.5f) * 0.7f;
        auroraWidth_ = 0.2f + 0.15f * rnd();
    }
    aurora_ += (auroraTarget_ - aurora_) * std::min(1.0f, dt * 0.25f);
    auroraCenter_ += dt * 0.004f;

    const Rgb blue{0.30f, 0.55f, 1.0f}, orange{1.0f, 0.45f, 0.15f}, airglow{0.3f, 0.9f, 0.4f};
    const Rgb green{0.2f, 1.0f, 0.45f}, violet{0.7f, 0.3f, 0.9f};
    const bool aurora = aurora_ > 0.01f;
    for (const auto& l : limb_) {
        const float ndl = l.nx * sun[0] + l.ny * sun[1];
        const float dayside = smoothstep(-0.25f, 0.2f, ndl);
        const float d = l.d, out = std::max(d, 0.0f);
        // The atmosphere is brightest at the crest and thins toward the window's sides.
        const float off = (l.angle + 0.5f * kPi) / 0.6f, taper = std::max(0.35f, 1.0f - 0.65f * off * off);
        Rgb add{};
        if (d > -0.5f) {
            const float band = (std::exp(-sq((d - 0.6f) / (1.2f + 0.8f * taper))) * 0.6f + std::exp(-out / 4.0f) * 0.12f) * dayside * taper;
            const float sunsetBand = smoothstep(-0.3f, -0.05f, ndl) * (1.0f - smoothstep(-0.05f, 0.25f, ndl));
            const float red = std::exp(-sq((d - 0.3f) / 1.5f)) * sunsetBand * 0.75f;
            const float glow =
                std::exp(-sq((d - 2.0f) / 1.0f)) * (1.0f - dayside) * 0.10f + std::exp(-sq(d / 1.6f)) * (1.0f - dayside) * 0.06f;
            add = {blue.r * band + orange.r * red + airglow.r * glow, blue.g * band + orange.g * red + airglow.g * glow,
                   blue.b * band + orange.b * red + airglow.b * glow};
        }
        if (aurora) {
            // Curtains fold along the limb and reach different heights.
            const float a0 = l.angle;
            const float fold = 0.12f * std::sin(a0 * 5.0f + t_ * 0.15f);
            const float top = 0.55f + 0.45f * (0.5f + 0.5f * std::sin(a0 * 21.0f + t_ * 0.2f + 1.3f * std::sin(a0 * 8.0f)));
            const float v = (d + 2.5f) / 16.0f + fold;  // 0 at the curtain's foot .. 1 at its top
            if (v > 0.0f && v < top) {
                const float spread = (a0 - auroraCenter_) / auroraWidth_;
                const float r1 = 0.5f + 0.5f * std::sin(a0 * 47.0f + 2.3f * std::sin(a0 * 13.0f + t_ * 0.3f) + t_ * 0.5f);
                const float r2 = 0.5f + 0.5f * std::sin(a0 * 83.0f - t_ * 0.4f + 1.7f * std::sin(a0 * 7.0f - t_ * 0.1f));
                const float rays = 0.4f + 0.6f * r1 * (0.4f + 0.6f * r2);
                const float a = aurora_ * (1.0f - smoothstep(-0.4f, -0.12f, ndl)) * std::exp(-spread * spread) * rays *
                                smoothstep(0.0f, 0.12f, v) * (1.0f - smoothstep(0.3f * top, top, v)) * 0.95f;
                const Rgb c = lerp_rgb(green, violet, smoothstep(0.35f, 0.95f, v / top));
                add.r += c.r * a;
                add.g += c.g * a;
                add.b += c.b * a;
            }
        }
        float* p = &cv_.px[static_cast<size_t>(l.index) * 3];
        p[0] += add.r;
        p[1] += add.g;
        p[2] += add.b;
    }
}

void SpaceStationWindowScene::draw_satellite(float dt, float sunlit)
{
    const float k = scale_;
    if (satLife_ <= 0.0f) {
        satWait_ -= dt;
        if (satWait_ > 0.0f)
            return;
        const bool fromLeft = rnd() < 0.5f;
        satX_ = fromLeft ? -2.0f : cv_.w + 2.0f;
        satY_ = (14.0f + 26.0f * rnd()) * k;
        const float speed = (4.0f + 3.0f * rnd()) * k;
        satVx_ = fromLeft ? speed : -speed;
        satVy_ = (rnd() - 0.5f) * 0.4f * speed;
        satLife_ = (cv_.w + 4.0f) / speed;
        satFlare_ = rnd() < 0.4f ? (0.3f + 0.4f * rnd()) * satLife_ : -1.0f;
        satWait_ = 18.0f + 30.0f * rnd();
    }
    satLife_ -= dt;
    if (satFlare_ > 0.0f)
        satFlare_ -= dt;
    satX_ += satVx_ * dt;
    satY_ += satVy_ * dt;
    const float dx = (satX_ - cx_) / radius_, dy = (satY_ - cy_) / radius_;
    if (dx * dx + dy * dy < 1.0f)
        return;  // behind the planet
    const float flare = satFlare_ > -1.0f ? std::exp(-sq(satFlare_ / 0.8f)) : 0.0f;
    const float progress = std::clamp(satLife_ * std::abs(satVx_) / (cv_.w + 4.0f), 0.0f, 1.0f);
    const float b = (0.35f + 0.12f * sunlit) * std::sin(kPi * progress) + 1.0f * flare;
    const int x = static_cast<int>(satX_), y = static_cast<int>(satY_);
    if (x < 0 || y < 0 || x >= cv_.w || y >= cv_.h)
        return;
    cv_.add(x, y, Rgb{1.0f, 0.97f, 0.9f}, b);
    if (flare > 0.05f)
        cv_.glow(satX_, satY_, 2.5f * k, Rgb{1.0f, 0.97f, 0.9f}, flare * 0.7f);
}

void SpaceStationWindowScene::draw_lightning(float dt, const float sun[3])
{
    flashWait_ -= dt;
    if (flashWait_ <= 0.0f && !earth_.empty()) {
        flashWait_ = 1.5f + 4.0f * rnd();
        // Look for a cloudy spot on the night side.
        const float drift = (spin_ + cloudDrift_) * Planet::W;
        for (int tries = 0; tries < 12; ++tries) {
            const auto& e = earth_[static_cast<size_t>(rnd() * (earth_.size() - 1))];
            const float ndl = e.nx * sun[0] + e.ny * sun[1] + e.nz * sun[2];
            if (ndl > -0.12f || e.nz < 0.25f || sample(planet_->clouds, tap(e.tx + drift, e.ty)) < 0.45f)
                continue;
            const float x = static_cast<float>(e.index % cv_.w) + 0.5f, y = static_cast<float>(e.index / cv_.w) + 0.5f;
            const int strikes = 1 + static_cast<int>(rnd() * 3.0f);
            for (int s = 0; s < strikes; ++s)
                flashes_.push_back({x + (rnd() - 0.5f) * 4 * scale_, y + (rnd() - 0.5f) * 2 * scale_, -0.25f * s - 0.1f * rnd(),
                                    (1.8f + 1.8f * rnd()) * scale_ * e.nz});
            break;
        }
    }
    for (auto& f : flashes_) {
        f.life += dt;
        if (f.life < 0.0f || f.life > 0.35f)
            continue;
        const float flicker = (std::sin(f.life * 90.0f) > -0.2f ? 1.0f : 0.3f) * (1.0f - f.life / 0.35f);
        cv_.glow(f.x, f.y, f.size + 1.5f, Rgb{0.8f, 0.87f, 1.0f}, 0.9f * flicker);
    }
    std::erase_if(flashes_, [](const Flash& f) { return f.life > 0.35f; });
}

void SpaceStationWindowScene::draw_frame(float earthshine)
{
    const Rgb shine = lerp_rgb(Rgb{0.35f, 0.38f, 0.45f}, Rgb{0.45f, 0.6f, 0.85f}, sunlit_);
    const size_t n = glass_.size();
    for (size_t i = 0; i < n; ++i) {
        const float g = glass_[i];
        if (g >= 1.0f)
            continue;
        float* p = &cv_.px[i * 3];
        const float lift = rimLight_[i] * earthshine;
        p[0] = p[0] * g + (frame_[i * 3] + shine.r * lift) * (1.0f - g);
        p[1] = p[1] * g + (frame_[i * 3 + 1] + shine.g * lift) * (1.0f - g);
        p[2] = p[2] * g + (frame_[i * 3 + 2] + shine.b * lift) * (1.0f - g);
    }
    // Status lights: two steady greens and a slow amber blink.
    const float k = scale_;
    for (size_t i = 0; i < leds_.size(); ++i) {
        const bool amber = i == 2;
        const float on = amber ? (std::fmod(t_, 4.0f) < 0.6f ? 1.0f : 0.15f) : 0.8f;
        const Rgb c = amber ? Rgb{1.0f, 0.6f, 0.1f} : Rgb{0.2f, 1.0f, 0.35f};
        cv_.add(static_cast<int>(leds_[i].x), static_cast<int>(leds_[i].y), c, 0.6f * on);
        cv_.glow(leds_[i].x + 0.5f, leds_[i].y + 0.5f, 2.0f * k, c, 0.08f * on);
    }
}

bool SpaceStationWindowScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt = 0.0f;
    begin_frame(dt);
    const float raw = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.1f);
    if (!planet_)
        planet_ = planet_future().get();
    const auto& L = env();
    spin_ = fract(spin_ + dt * SpinRate);
    cloudDrift_ = fract(cloudDrift_ + dt * CloudRate);

    // Where the sun is: the local time of day sets how far the view is from
    // the terminator (day: all sunlit, dusk/dawn: the terminator crosses the
    // middle, night: the dark side). The terminator also sways slowly.
    const float angle = std::clamp(90.0f - 1.8f * L.sun_elevation + 3.0f * std::sin(t_ * 0.02f), 0.0f, 180.0f) * kPi / 180.0f;
    const float phi = std::clamp((L.sun_x - 0.5f) * 4.0f, -1.0f, 1.0f) * 0.5f * kPi;
    float side[3] = {std::sin(phi) * 0.819f, std::sin(phi) * 0.573f - std::cos(phi) * 0.958f, -std::cos(phi) * 0.287f};
    const float along = side[0] * view_[0] + side[1] * view_[1] + side[2] * view_[2];
    for (int k = 0; k < 3; ++k) side[k] -= along * view_[k];
    normalize3(side);
    float sun[3];
    for (int k = 0; k < 3; ++k) sun[k] = view_[k] * std::cos(angle) + side[k] * std::sin(angle);

    std::fill(cv_.px.begin(), cv_.px.end(), 0.0f);
    draw_stars();
    draw_earth(sun);
    draw_limb(sun, raw);
    draw_lightning(raw, sun);
    draw_satellite(raw, sunlit_);
    draw_frame(0.2f + 0.35f * sunlit_);
    present(canvas);
    wait_until_next_frame();
    return true;
}

Scenes::SceneDescriptor SpaceStationWindowScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "scenery";
    d.tags = {"scenery", "space", "earth", "calm", "fits:dawn", "fits:day", "fits:dusk", "fits:night"};
    d.intensity = 0.12f;
    d.motion = 0.12f;
    d.music_affinity = 0.05f;
    d.performance_cost = 0.25f;
    environment_->add_variants(d);
    return d;
}

}  // namespace Hobbies
