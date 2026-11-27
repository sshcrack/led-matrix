#include "MurmurationScene.h"

#include "Backdrop.h"

namespace Hobbies {
namespace {

// Design rows (128 × 128 design space).
constexpr float ShoreY = 97.0f;           // far shore; water below
constexpr float FlockFloor = ShoreY - 7;  // birds stay above the shore
constexpr float CrossfadeSeconds = 3.0f;
constexpr float TrailSeconds = 0.16f;  // how long a bird's darkness lingers
constexpr float InkGain = 1.0f;
constexpr float FlockPace = 1.7f;  // flock-shape seconds per scene second
constexpr float FalconRadius = 15.0f;

struct Shape {
    float cx, cy, cos_t, sin_t, length, thickness, bend, split, t;
};

// The flock's outline at time t: a lens that drifts, turns, stretches, bends
// into a comma and now and then pulls apart into two lobes.
Shape flock_shape(float t)
{
    Shape s{};
    s.cx = 64.0f + 18.0f * std::sin(0.029f * t + 1.0f) + 7.0f * std::sin(0.083f * t + 0.4f);
    s.cy = 44.0f + 10.0f * std::sin(0.041f * t + 2.0f) + 5.0f * std::sin(0.107f * t + 0.7f);
    const float theta = 0.5f * std::sin(0.047f * t) + 0.3f * std::sin(0.121f * t + 1.3f);
    s.cos_t = std::cos(theta);
    s.sin_t = std::sin(theta);
    s.length = 33.0f + 10.0f * std::sin(0.067f * t + 0.3f) + 4.0f * std::sin(0.19f * t);
    s.thickness = 12.0f + 5.0f * std::sin(0.089f * t + 2.1f) + 2.0f * std::sin(0.23f * t + 1.0f);
    s.bend = 0.45f * std::sin(0.058f * t + 0.8f);
    s.split = std::pow(std::max(0.0f, std::sin(0.019f * t - 1.2f)), 12.0f);
    s.t = t;
    return s;
}

// Phase of the turning wave that runs along the flock.
float wave_phase(const Shape& s, float u)
{
    return 2.4f * kPi * u - 1.9f * s.t;
}

}  // namespace

MurmurationScene::MurmurationScene()
{
    Environment::Adapt adapt;
    adapt.time = false;  // a murmuration happens at dusk
    adapt.weather_allowed = {Environment::Weather::Clear, Environment::Weather::PartlyCloudy, Environment::Weather::Overcast,
                             Environment::Weather::Fog};
    adapt.weather_variants = {Environment::Weather::PartlyCloudy, Environment::Weather::Fog};
    adapt.season_variants = {Environment::Season::Summer, Environment::Season::Winter};
    Environment::Defaults defaults{Environment::TimeOfDay::Dusk, Environment::Weather::Clear, Environment::Season::Autumn};
    defaults.sun_elevation = 2.5f;
    defaults.sun_x = 0.28f;
    use_environment(defaults, adapt);
    use_music_layer();
}

void MurmurationScene::register_properties()
{
    HobbySceneBase::register_properties();
    birdCount_->label("Birds").description("How many starlings fly in the flock.").group("Flock");
    falconOn_->label("Falcon").description("A falcon passes now and then and the flock opens around it.").group("Flock");
    add_property(birdCount_);
    add_property(falconOn_);
}

void MurmurationScene::initialize(int width, int height)
{
    HobbySceneBase::initialize(width, height);
    backdrop_.clear();
    previous_.clear();
    fade_ = 0.0f;
    ink_.assign(static_cast<size_t>(width * height), 0.0f);
    falcon_ = {};
    falcon_.wait = 14.0f + 12.0f * rnd();
    flockTime_ = 40.0f + 400.0f * rnd();
    birds_.clear();
}

void MurmurationScene::build_backdrop()
{
    const auto& L = env();
    Canvas bg;
    bg.resize(cv_.w, cv_.h);
    const Environment::Surface s{bg.px.data(), bg.w, bg.h, 1.0f};
    const Artwork art(bg);
    const float W = static_cast<float>(bg.w), sx = art.sx, sy = art.sy, k = std::min(sx, sy);
    const float horizon = ShoreY * sy;
    const int shoreRow = std::clamp(static_cast<int>(std::lround(horizon)), 1, bg.h);

    Environment::paint_sky(s, L, horizon);
    Environment::paint_sun(s, L, Environment::sun_point(L, W, horizon), 4.5f * k);
    Environment::paint_moon(s, L, Environment::moon_point(L, W, horizon * 0.9f), 3.5f * k);
    const std::vector<float> skyStage = bg.px;

    // Low far hills, then the near shore with a few stands of trees and one lone tree.
    const Rgb far = from_env(L.lit(to_env({0.30f, 0.34f, 0.36f}), 0.85f));
    bg.fill_below([&](float x) { return (ShoreY - 2.5f - 7.0f * hill(x / sx * 0.8f, 1.7f)) * sy; }, far);
    // Winter banks and water lose the sunset's warmth a little.
    const Rgb chill{0.62f, 0.70f, 0.86f};
    const Rgb shore = mix(from_env(L.lit(L.ground(L.grass(to_env({0.20f, 0.27f, 0.14f})), 0.8f), 0.55f)), chill, 0.25f * L.winter);
    bg.fill_below([&](float x) { return (ShoreY - 0.8f - 2.0f * hill(x / sx * 2.3f, 4.2f)) * sy; }, shore);
    const Rgb pineShade = from_env(L.lit(L.ground(to_env({0.07f, 0.12f, 0.08f}), 0.3f), 0.6f));
    const Rgb pineLit = from_env(L.lit(L.ground(to_env({0.11f, 0.17f, 0.10f}), 0.3f), 0.6f));
    for (int i = 0; i < 9; ++i) {
        const float x = 8.0f + 6.5f * i + 3.0f * hash01(91u + i);
        art.pine(x, ShoreY - 0.5f, 4.0f + 3.5f * hash01(37u + i), pineShade, pineLit);
    }
    for (int i = 0; i < 4; ++i) art.pine(116.0f + 3.2f * i, ShoreY - 0.5f, 3.5f + 2.5f * hash01(53u + i), pineShade, pineLit);
    Environment::paint_tree(s, L, 99.0f * sx, (ShoreY - 0.6f) * sy, 26.0f * sy, 5u, 0.0f, 0.3f);

    // Still water mirrors the sky and the shore, darker and bluer with depth.
    const Rgb deep = from_env(L.lit(to_env({0.05f, 0.09f, 0.12f})));
    for (int y = shoreRow; y < bg.h; ++y) {
        const int m = std::max(0, 2 * shoreRow - 1 - y);
        const float depth = static_cast<float>(y - shoreRow) / std::max(1, bg.h - shoreRow);
        for (int x = 0; x < bg.w; ++x) {
            const float* p = bg.at(x, m);
            const Rgb reflected = mix(Rgb{p[0], p[1], p[2]}, deep, 0.15f + 0.4f * depth + 0.25f * L.winter);
            bg.set(x, y, scale(Rgb{reflected.r * (1.0f - 0.18f * L.winter), reflected.g * (1.0f - 0.08f * L.winter), reflected.b}, 0.8f));
        }
    }
    // A pale line where the water meets the shore.
    for (int x = 0; x < bg.w; ++x) bg.blend(x, shoreRow, from_env(L.horizon), 0.35f);
    const std::vector<float> waterStage = bg.px;

    // Reeds in the foreground corners, dark against the bright water.
    const Rgb reed = scale(from_env(L.lit(L.ground(L.grass(to_env({0.36f, 0.40f, 0.18f})), 0.4f))), 0.55f);
    const Rgb head = scale(from_env(L.lit(to_env({0.30f, 0.18f, 0.09f}))), 0.6f);
    for (int i = 0; i < 64; ++i) {
        const float a = hash01(7u + i * 3u), b = hash01(11u + i * 5u), c = hash01(13u + i * 7u);
        const bool left = i < 36;
        const float x = left ? 36.0f * a * a : 128.0f - 30.0f * a * a;
        const float edge = left ? x / 36.0f : (128.0f - x) / 30.0f;  // reeds get shorter away from the corner
        const float height = (10.0f + 16.0f * b) * (1.0f - 0.6f * edge);
        const float lean = (c - 0.5f) * 3.0f + (left ? 0.8f : -0.8f);
        const float base = 129.0f;
        bg.capsule(x * sx, base * sy, 0.5f * sx, (x + lean) * sx, (base - height) * sy, 0.3f * sx, reed);
        if (c > 0.72f)
            bg.capsule((x + lean * 0.9f) * sx, (base - height * 0.92f) * sy, 0.75f * sx, (x + lean * 0.8f) * sx,
                       (base - height * 0.78f) * sy, 0.75f * sx, head);
    }

    sky_.assign(static_cast<size_t>(cv_.w * cv_.h), 0);
    water_.assign(static_cast<size_t>(cv_.w * cv_.h), 0);
    for (size_t i = 0; i < sky_.size(); ++i) {
        const int y = static_cast<int>(i / static_cast<size_t>(cv_.w));
        if (y < shoreRow)
            sky_[i] = unchanged(bg, skyStage, i);
        else
            water_[i] = unchanged(bg, waterStage, i);
    }
    if (!backdrop_.empty()) {
        previous_ = std::move(backdrop_);
        fade_ = 1.0f;
    }
    backdrop_ = std::move(bg.px);
}

void MurmurationScene::reset_flock()
{
    // Fewer birds when the Pi is short on time; the shape stays the same.
    const float q = std::clamp((render_quality_scale() - 0.5f) / 0.5f, 0.0f, 1.0f);
    const int wanted = static_cast<int>(std::lround(std::clamp(birdCount_->get(), 120, 800) * (0.55f + 0.45f * q)));
    if (!birds_.empty() && std::abs(wanted - static_cast<int>(birds_.size())) < 24)
        return;
    const Shape s = flock_shape(flockTime_);
    while (static_cast<int>(birds_.size()) > wanted) birds_.pop_back();
    while (static_cast<int>(birds_.size()) < wanted) {
        const float r = std::pow(rnd(), 0.7f), a = rnd() * kTau;
        Bird b{};
        b.u = r * std::cos(a);
        b.v = r * std::sin(a);
        b.follow = 0.9f + 1.6f * rnd();
        b.wobble = rnd() * kTau;
        b.size = 0.7f + 0.5f * rnd();
        b.x = s.cx + b.u * s.length * s.cos_t - b.v * s.thickness * s.sin_t;
        b.y = s.cy + b.u * s.length * s.sin_t + b.v * s.thickness * s.cos_t;
        birds_.push_back(b);
    }
}

void MurmurationScene::update_flock(float dt)
{
    flockTime_ += dt * FlockPace;
    const Shape s = flock_shape(flockTime_);

    // The falcon dives through the flock from one side every minute or so.
    auto& f = falcon_;
    if (!f.active && falconOn_->get()) {
        f.wait -= dt;
        if (f.wait <= 0.0f) {
            const bool fromLeft = rnd() < 0.5f;
            f.x = fromLeft ? -10.0f : 138.0f;
            f.y = s.cy - 14.0f + 10.0f * rnd();
            const float dx = s.cx - f.x, dy = s.cy + 4.0f * (rnd() - 0.5f) - f.y;
            const float len = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
            f.vx = dx / len * 24.0f;
            f.vy = dy / len * 24.0f;
            f.active = true;
        }
    }
    if (f.active) {
        f.x += f.vx * dt;
        f.y += f.vy * dt;
        f.vy -= 1.5f * dt;  // pulls up after the dive
        if (f.x < -14.0f || f.x > 142.0f || f.y < -14.0f) {
            f.active = false;
            f.wait = 45.0f + 40.0f * rnd();
        }
    }

    const float t = s.t;
    for (auto& b : birds_) {
        const float u = b.u + 0.09f * std::sin(wave_phase(s, b.u)) + s.split * 0.35f * (b.u < 0.0f ? -1.0f : 1.0f);
        // A slower fold across the flock makes it thicken and thin like a surface turning over.
        const float fold = 0.75f + 0.25f * std::sin(1.7f * kPi * b.u + 1.3f * t);
        const float along = u * s.length;
        const float across = b.v * s.thickness * fold * (1.0f - 0.45f * u * u) + s.bend * s.length * 0.5f * u * u;
        float tx = s.cx + along * s.cos_t - across * s.sin_t + 0.9f * std::sin(t * 1.7f + b.wobble * 3.0f);
        float ty = s.cy + along * s.sin_t + across * s.cos_t + 0.7f * std::cos(t * 1.3f + b.wobble * 5.0f);
        if (f.active) {
            const float dx = tx - f.x, dy = ty - f.y, d = std::sqrt(dx * dx + dy * dy);
            if (d < FalconRadius && d > 0.01f) {
                const float push = (1.0f - d / FalconRadius) * (1.0f - d / FalconRadius) * 14.0f;
                tx += dx / d * push;
                ty += dy / d * push;
            }
        }
        ty = std::min(ty, FlockFloor);
        // Birds further back in the flock react later, so turns sweep through it.
        const float k = 1.0f - std::exp(-dt * b.follow * (1.0f + 0.3f * b.u));
        b.x += (tx - b.x) * k;
        b.y += (ty - b.y) * k;
    }
}

void MurmurationScene::draw_birds(float raw_dt)
{
    const int W = cv_.w, H = cv_.h;
    const float sx = W / 128.0f, sy = H / 128.0f;
    const float decay = std::exp(-raw_dt / TrailSeconds);
    for (auto& v : ink_) v *= decay;
    // Fewer birds (reduced quality) each mark a little stronger, a huge flock a little softer,
    // so the outline stays readable and dense overlaps keep their texture.
    const float deposit =
        (1.0f - decay) * InkGain * std::clamp(std::sqrt(480.0f / std::max<float>(1.0f, static_cast<float>(birds_.size()))), 0.8f, 1.35f);
    const Shape s = flock_shape(flockTime_);
    const auto splat = [&](float px, float py, float a) {
        const int x0 = static_cast<int>(std::floor(px)), y0 = static_cast<int>(std::floor(py));
        const float fx = px - x0, fy = py - y0;
        const auto put = [&](int x, int y, float w) {
            if (x >= 0 && y >= 0 && x < W && y < H)
                ink_[static_cast<size_t>(y * W + x)] += a * w;
        };
        put(x0, y0, (1 - fx) * (1 - fy));
        put(x0 + 1, y0, fx * (1 - fy));
        put(x0, y0 + 1, (1 - fx) * fy);
        put(x0 + 1, y0 + 1, fx * fy);
    };
    for (const auto& b : birds_) {
        // Birds in the turning band show their wings and look darker.
        const float turn = 0.5f + 0.5f * std::cos(wave_phase(s, b.u));
        splat(b.x * sx - 0.5f, b.y * sy - 0.5f, deposit * b.size * (0.6f + 0.4f * turn * turn));
    }
    if (falcon_.active) {
        // Wing beats: a short flicker between spread and folded wings.
        const float flap = std::sin(s.t * 9.0f);
        const float fx = falcon_.x * sx, fy = falcon_.y * sy;
        const float span = (1.6f + 0.5f * flap) * sx;
        for (int i = -2; i <= 2; ++i) splat(fx + i * span * 0.5f - 0.5f, fy - std::abs(i) * flap * 0.4f * sy - 0.5f, deposit * 1.6f);
    }

    const auto& L = env();
    const Rgb bird = mix(scale(from_env(L.zenith), 0.2f), Rgb{0.015f, 0.012f, 0.02f}, 0.7f);
    const int shoreRow = std::clamp(static_cast<int>(std::lround(ShoreY * sy)), 1, H);
    for (int y = 0; y < shoreRow; ++y)
        for (int x = 0; x < W; ++x) {
            const float ink = ink_[static_cast<size_t>(y * W + x)];
            if (ink > 0.004f)
                cv_.blend(x, y, bird, 0.86f * (1.0f - std::exp(-1.5f * ink)));
        }
    // The flock's reflection where it flies low, rippled by the water.
    for (int y = shoreRow; y < H; ++y) {
        const int m = 2 * shoreRow - 1 - y;
        if (m < 0)
            break;
        const int shift = static_cast<int>(std::lround(0.8f * sx * std::sin(y * 1.3f + t_ * 1.1f)));
        for (int x = 0; x < W; ++x) {
            const size_t i = static_cast<size_t>(y * W + x);
            if (!water_[i])
                continue;
            const int src = std::clamp(x + shift, 0, W - 1);
            const float ink = ink_[static_cast<size_t>(m * W + src)];
            if (ink > 0.004f)
                cv_.blend(x, y, bird, 0.5f * (1.0f - std::exp(-1.5f * ink)));
        }
    }
}

bool MurmurationScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt = 0.0f;
    begin_frame(dt);
    const float raw = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.1f);
    const auto& L = env();
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f;
    if (environment_changed(backdropKey_) || backdrop_.empty())
        build_backdrop();
    cv_.px = backdrop_;
    if (fade_ > 0.0f && previous_.size() == backdrop_.size()) {
        for (size_t i = 0; i < cv_.px.size(); ++i) cv_.px[i] += (previous_[i] - cv_.px[i]) * fade_;
        fade_ = std::max(0.0f, fade_ - raw / CrossfadeSeconds);
    }

    MusicLayer::Placement bars;
    bars.base = ShoreY * sy;
    bars.max_height = 45.0f * sy;
    bars.bar = std::max(1, static_cast<int>(std::lround(2.0f * sx)));
    bars.gap = std::max(1, static_cast<int>(std::lround(sx)));
    bars.brightness = 0.6f;
    paint_music_layer(&sky_, bars);
    paint_clouds_behind(cv_, backdrop_, sky_, L, t_, ShoreY * sy, 4 * sy, 52 * sy, 17);

    // A slow shimmer on the open water.
    const int shoreRow = std::clamp(static_cast<int>(std::lround(ShoreY * sy)), 1, cv_.h);
    for (int y = shoreRow; y < cv_.h; ++y) {
        const float row = std::sin(y * 0.7f / sy + t_ * 0.5f);
        for (int x = 0; x < cv_.w; ++x) {
            if (!water_[static_cast<size_t>(y * cv_.w + x)])
                continue;
            const float g = 1.0f + 0.07f * row * std::sin(x * 0.45f / sx + y * 2.1f / sy - t_ * 1.2f);
            float* p = cv_.at(x, y);
            p[0] *= g;
            p[1] *= g;
            p[2] *= g;
        }
    }
    // Evening mist over the wetland.
    Environment::paint_fog(surface(), L, t_, (ShoreY - 8.0f) * sy, static_cast<float>(cv_.h), 0.08f);

    reset_flock();
    update_flock(dt);
    draw_birds(raw);

    present(canvas);
    wait_until_next_frame();
    return true;
}

Scenes::SceneDescriptor MurmurationScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "scenery";
    d.tags = {"scenery",   "landscape",   "wildlife",    "birds",      "water",       "calm",
              "fits:dusk", "fits:autumn", "fits:winter", "fits:clear", "fits:cloudy", "fits:fog"};
    d.intensity = 0.22f;
    d.motion = 0.35f;
    d.music_affinity = 0.05f;
    d.performance_cost = 0.22f;
    // The id is kept from the flocking demo this scene replaced, so saved presets and preferences still match.
    environment_->add_variants(d, {{"swarm",
                                    "Great murmuration",
                                    "A huge flock of starlings over the wetland",
                                    {{"birds", 780}},
                                    {"birds", "dense"},
                                    0.30f,
                                    0.42f,
                                    0.05f,
                                    0.26f}});
    Scenes::set_off_by_default(d, {"swarm"});
    music_layer_->describe(d);
    return d;
}

}  // namespace Hobbies
