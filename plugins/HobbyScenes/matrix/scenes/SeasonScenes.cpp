#include "SeasonScenes.h"

#include "FigureDraw.h"
#include "SceneryDraw.h"

using namespace Hobbies;

namespace {
float ground_hash(float x) { return hash01(static_cast<uint32_t>(std::max(0.0f, x) * 7.0f + 11.0f)); }
}  // namespace

// ================================================================ forest path
ForestPathScene::ForestPathScene()
{
    // Designed as a bright mid-morning in late spring light.
    use_environment({Environment::TimeOfDay::Day, Environment::Weather::Clear, Environment::Season::Summer, 30.0f, 0.62f});
}

float ForestPathScene::path_center(float y) const
{
    const float H = static_cast<float>(cv_.h), W = static_cast<float>(cv_.w);
    const float v = std::clamp((y - horizon_) / (H - horizon_), 0.0f, 1.0f);
    return W * (0.50f + 0.11f * std::sin(v * 2.8f - 1.2f) * (0.4f + 0.6f * v));
}

float ForestPathScene::path_half(float y) const
{
    const float H = static_cast<float>(cv_.h), k = static_cast<float>(cv_.w) / 128.0f;
    const float v = std::clamp((y - horizon_) / (H - horizon_), 0.0f, 1.0f);
    return (0.8f + 26.0f * std::pow(v, 1.5f)) * k;
}

void ForestPathScene::initialize(int width, int height)
{
    HobbySceneBase::initialize(width, height);
    const float W = static_cast<float>(width), H = static_cast<float>(height), k = W / 128.0f;
    horizon_ = std::floor(H * 0.50f);
    backdrop_.clear();
    drift_.reset();
    // Mid-ground trees on both sides of the path, far to near. Their bases
    // stay above the deer's clearing so the deer always walks in front.
    trees_.clear();
    for (uint32_t i = 0; trees_.size() < 26 && i < 400; ++i) {
        const float depth = hash01(i * 13u + 2u);  // 1 far .. 0 near
        const float base = horizon_ + (H * 0.66f - horizon_) * std::pow(1.0f - depth, 1.4f) + 1.0f;
        const float x = (hash01(i * 7u + 5u) * 1.1f - 0.05f) * W;
        const float clear = path_half(base) + (2.0f + 7.0f * (1.0f - depth)) * k;
        if (std::fabs(x - path_center(base)) < clear)
            continue;
        trees_.push_back({x, base, (11.0f + 30.0f * (1.0f - depth) + 6.0f * hash01(i * 7u)) * k, i * 977u + 13u,
                          hash01(i * 29u + 3u) < 0.25f});
    }
    std::sort(trees_.begin(), trees_.end(), [](const Tree& a, const Tree& b) { return a.base < b.base; });
    // Start with the deer grazing so short previews meet it.
    deerState_ = 2;
    deerX_ = W * 0.22f;
    deerDir_ = 1.0f;
    deerGraze_ = 1.0f;
    deerTimer_ = 9.0f;
}

void ForestPathScene::build_backdrop()
{
    const auto& L = env();
    const int w = cv_.w, h = cv_.h;
    const float W = static_cast<float>(w), H = static_cast<float>(h), k = W / 128.0f;
    Canvas bg;
    bg.resize(w, h);
    const Environment::Surface s{bg.px.data(), w, h, 1.0f};
    Environment::paint_sky(s, L, horizon_);
    Environment::paint_sun(s, L, Environment::sun_point(L, W, horizon_), 6.0f * k);
    Environment::paint_moon(s, L, Environment::moon_point(L, W, horizon_), 4.5f * k);

    // Far hills and a soft wooded skyline.
    bg.fill_below([&](float x) { return horizon_ - 6.0f * k - hill(x, 1.7f) * 9.0f * k; }, plant({0.32f, 0.46f, 0.42f}, 0.9f, 0.7f, 0.6f));
    const Rgb farLeaf = plant({0.20f, 0.38f, 0.22f}, 0.7f, 0.6f, 0.85f), farPine = terrain({0.12f, 0.24f, 0.18f}, 0.7f, 0.45f);
    const Rgb farBark = lit({0.30f, 0.24f, 0.20f}, 0.7f);
    for (float x = 0.0f; x < W + 3 * k; x += 2.6f * k) {
        const uint32_t id = static_cast<uint32_t>(x * 53.0f + 5.0f);
        const float hgt = (7.0f + 6.0f * hash01(id)) * k;
        if (hash01(id + 9) < 0.35f)
            pine(bg, x, horizon_ + 1.0f, hgt, farPine);
        else
            broadleaf(bg, x, horizon_ + 1.0f, hgt, farLeaf, farBark, env().winter * 0.9f, id, 0.0f);
    }

    // Meadow floor, lighter in the distance, with grass texture.
    const Rgb far = meadow({0.45f, 0.58f, 0.32f}, 0.45f), nearC = meadow({0.20f, 0.36f, 0.14f});
    for (int y = static_cast<int>(horizon_); y < h; ++y) {
        const float v = (y - horizon_) / (H - horizon_);
        const Rgb base = mix(far, nearC, std::pow(v, 0.7f));
        for (int x = 0; x < w; ++x) {
            const float n = hash01(static_cast<uint32_t>(y * w + x) * 3u + 1u);
            bg.set(x, y, scale(base, 0.88f + 0.22f * n * v));
        }
    }
    // Fallen leaves on the floor in autumn.
    const float litter = L.autumn * (1.0f - L.snow_cover);
    if (litter > 0.05f)
        for (int i = 0; i < static_cast<int>(200 * k * k * litter); ++i) {
            const float y = horizon_ + (H - horizon_) * std::sqrt(hash01(i * 3u + 7u));
            const float x = hash01(i * 5u + 1u) * W;
            static const Rgb cols[] = {{0.78f, 0.38f, 0.10f}, {0.70f, 0.22f, 0.08f}, {0.86f, 0.62f, 0.16f}};
            bg.blend(static_cast<int>(x), static_cast<int>(y), lit(cols[i % 3], 0.2f), 0.55f);
        }
    // Spring and summer flowers in the grass.
    const float flowers = (L.spring + L.summer * 0.6f) * (1.0f - L.snow_cover);
    if (flowers > 0.05f)
        for (int i = 0; i < static_cast<int>(110 * k * k * flowers); ++i) {
            const float y = horizon_ + (H - horizon_) * (0.2f + 0.8f * std::sqrt(hash01(i * 11u + 3u)));
            const float x = hash01(i * 17u + 5u) * W;
            if (std::fabs(x - path_center(y)) < path_half(y) + 2 * k)
                continue;
            static const Rgb cols[] = {{1.0f, 0.95f, 0.85f}, {0.98f, 0.82f, 0.25f}, {0.80f, 0.55f, 0.95f}, {1.0f, 0.55f, 0.65f}};
            bg.blend(static_cast<int>(x), static_cast<int>(y), lit(cols[i % 4], 0.1f), 0.85f);
        }

    // The path: packed earth, two lighter wheel ruts, puddles when wet.
    const Rgb dirt = terrain({0.52f, 0.42f, 0.30f}, 0.0f, 0.85f), rut = terrain({0.62f, 0.52f, 0.38f}, 0.0f, 0.9f);
    const Rgb farDirt = terrain({0.52f, 0.42f, 0.30f}, 0.6f, 0.85f);
    for (int y = static_cast<int>(horizon_) + 1; y < h; ++y) {
        const float fy = y + 0.5f, cx = path_center(fy), half = path_half(fy);
        const float v = (fy - horizon_) / (H - horizon_);
        for (int x = static_cast<int>(cx - half - 1); x <= static_cast<int>(cx + half + 1); ++x) {
            const float u = (x + 0.5f - cx) / std::max(0.5f, half);
            const float edge = std::clamp((1.0f - std::fabs(u)) * half + 0.5f, 0.0f, 1.0f);
            Rgb c = mix(farDirt, dirt, v);
            const float r = std::exp(-std::pow((std::fabs(u) - 0.45f) / 0.12f, 2.0f));
            c = mix(c, rut, r * 0.5f * v);
            // Puddles: hashed blobs that mirror the sky.
            if (L.wetness > 0.05f && v > 0.25f) {
                const float p = std::sin(fy * 0.35f) * std::sin(u * 3.0f + fy * 0.07f);
                if (p > 0.55f)
                    c = mix(scale(c, 0.6f), from_env(L.sky(0.35f)), L.wetness * 0.75f);
                else
                    c = scale(c, 1.0f - 0.25f * L.wetness);
            }
            bg.blend(x, y, c, edge);
        }
    }

    // Mid-ground trees, far to near.
    const float light = L.sun_x < 0.5f ? -1.0f : 1.0f;
    const Rgb snow = lit({0.92f, 0.94f, 0.98f}, 0.2f);
    for (const auto& t : trees_) {
        const float depth = std::clamp(1.0f - (t.base - horizon_) / (H * 0.66f - horizon_), 0.0f, 1.0f);
        // Contact shadow.
        bg.ellipse(t.x, t.base, t.height * 0.16f, t.height * 0.03f + 0.5f, 0, lit({0.10f, 0.16f, 0.08f}, depth), 0.45f);
        if (t.pine) {
            pine(bg, t.x, t.base, t.height * 1.1f, terrain({0.10f, 0.24f, 0.16f}, depth * 0.8f, 0.5f));
            continue;
        }
        // Autumn is not one orange: some trees go red, some yellow, a few stay green.
        static const Rgb tints[] = {{1.12f, 0.72f, 0.75f}, {1.0f, 1.12f, 0.55f}, {1.0f, 1.0f, 1.0f}, {0.85f, 1.2f, 0.9f}};
        const Rgb tint = mix({1, 1, 1}, tints[t.seed % 4], L.autumn);
        Rgb crown = plant({0.26f, 0.48f, 0.20f}, depth * 0.8f, 0.0f, t.seed % 4 == 3 ? 0.4f : 1.0f);
        crown = {crown.r * tint.r, crown.g * tint.g, crown.b * tint.b};
        broadleaf(bg, t.x, t.base, t.height, crown,
                  lit({0.30f, 0.22f, 0.16f}, depth * 0.8f), L.winter * 0.95f, t.seed, light, snow, L.snow_cover,
                  hash01(t.seed) < 0.5f ? L.spring : 0.0f);
    }
    backdrop_ = std::move(bg.px);
}

bool ForestPathScene::render(rgb_matrix::FrameCanvas* canvas)
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
    Environment::paint_stars(s, L, horizon_ * 0.7f, t_, 7, 60);
    Environment::paint_clouds(s, L, t_, 0.0f, horizon_ * 0.55f);

    // Lantern on a post beside the path, lit at dusk.
    const float lx = W * 0.79f, lb = H * 0.84f;
    cv_.capsule(lx, lb, 0.8f * k, lx, lb - 16 * k, 0.6f * k, lit({0.28f, 0.20f, 0.14f}));
    cv_.capsule(lx, lb - 16 * k, 0.5f * k, lx + 3 * k, lb - 16 * k, 0.4f * k, lit({0.28f, 0.20f, 0.14f}));
    cv_.ellipse(lx + 3 * k, lb - 13.5f * k, 1.3f * k, 1.8f * k, 0, mix(lit({0.25f, 0.24f, 0.22f}), {1.0f, 0.80f, 0.45f}, L.lamp()));
    cv_.glow(lx + 3 * k, lb - 13.5f * k, 14 * k, {1.0f, 0.72f, 0.35f}, 0.35f * L.lamp());
    cv_.glow(lx + 2 * k, lb, 20 * k, {1.0f, 0.70f, 0.35f}, 0.20f * L.lamp());
    // Moonlight picks the path out of the dark wood.
    cv_.glow(W * 0.5f, H * 0.86f, 26 * k, {0.55f, 0.62f, 0.85f}, 0.16f * L.night);

    // Deer: wanders in from the left, grazes, looks up, crosses the path.
    const float deerY = H * 0.72f, deerS = 0.75f * k;
    switch (deerState_) {
        case 0:
            deerTimer_ -= dt;
            if (deerTimer_ <= 0.0f && L.storm < 0.3f) {
                deerState_ = 1;
                deerDir_ = 1.0f;
                deerX_ = -16 * k;
            }
            break;
        case 1:
        case 3:
            deerX_ += deerDir_ * 7.0f * k * dt;
            deerPhase_ += dt * 1.1f;
            deerGraze_ = std::max(0.0f, deerGraze_ - dt * 1.5f);
            if (deerState_ == 1 && deerX_ >= W * 0.22f) {
                deerState_ = 2;
                deerTimer_ = 9.0f + rnd() * 6.0f;
            }
            if (deerX_ > W + 16 * k) {
                deerState_ = 0;
                deerTimer_ = 14.0f + rnd() * 18.0f;
            }
            break;
        case 2: {
            deerTimer_ -= dt;
            // Head down most of the time, up to look around every few seconds.
            const bool alert = std::fmod(deerTimer_, 4.5f) < 1.4f;
            deerGraze_ += ((alert ? 0.0f : 1.0f) - deerGraze_) * (1.0f - std::exp(-dt * 3.0f));
            if (deerTimer_ <= 0.0f) {
                deerState_ = 3;  // walks on right, across the path
                deerTimer_ = 0.0f;
            }
            break;
        }
        default:
            break;
    }
    if (deerState_ != 0) {
        const float walking = deerState_ == 1 || deerState_ == 3 ? 1.0f : 0.0f;
        cv_.ellipse(deerX_, deerY, 6.5f * deerS, 1.0f * deerS, 0, lit({0.08f, 0.12f, 0.06f}), 0.4f);
        deer(cv_, deerX_, deerY, deerS, deerDir_, deerPhase_, walking, deerGraze_, subject({0.56f, 0.36f, 0.20f}),
             subject({0.80f, 0.68f, 0.52f}), subject({0.10f, 0.07f, 0.06f}));
    }

    // Foreground: two big trunks framing the view, canopy across the top.
    const float sway = std::sin(t_ * 0.6f) * (0.4f + 1.2f * std::fabs(L.wind)) * k;
    const Rgb bark = lit({0.24f, 0.18f, 0.13f}), barkLit = lit({0.38f, 0.29f, 0.21f});
    auto trunk = [&](float x, float wTop, float wBot, float lean) {
        cv_.capsule(x + lean, -4.0f, wTop, x, H + 2.0f, wBot, bark);
        cv_.capsule(x + lean + wTop * 0.45f * (L.sun_x < 0.5f ? -1 : 1), -4.0f, wTop * 0.35f,
                    x + wBot * 0.45f * (L.sun_x < 0.5f ? -1 : 1), H + 2.0f, wBot * 0.3f, barkLit, 0.6f);
    };
    trunk(4.0f * k, 2.6f * k, 4.2f * k, 2.0f * k);
    trunk(W - 6.0f * k, 2.2f * k, 3.6f * k, -2.5f * k);
    const float leafy = 1.0f - 0.95f * L.winter;
    const Rgb leaf = plant({0.22f, 0.42f, 0.17f}, 0.0f, 0.0f, 1.0f);
    const Rgb leafDark = scale(leaf, 0.6f), leafHi = mix(leaf, {1.0f, 1.0f, 0.9f}, 0.14f);
    // Bare boughs reaching in from the trunks once the leaves are gone.
    for (int b = 0; b < 4 && leafy < 0.9f; ++b) {
        const float y0 = (10.0f + 9.0f * b) * k;
        const float a = 1.0f - leafy;
        cv_.capsule(4.5f * k, y0, 1.1f * k, (16.0f + 8.0f * b) * k + sway, y0 - (5.0f + 2.0f * b) * k, 0.4f * k, bark, a);
        cv_.capsule(W - 6.5f * k, y0 + 4 * k, 1.0f * k, W - (18.0f + 7.0f * b) * k + sway, y0 - 3.0f * k, 0.4f * k, bark, a);
        // Twigs so the boughs do not read as straight sticks.
        for (int t = 0; t < 2; ++t) {
            const float f = 0.4f + 0.3f * t;
            const float lx0 = 4.5f * k + ((16.0f + 8.0f * b) * k + sway - 4.5f * k) * f, ly0 = y0 - (5.0f + 2.0f * b) * k * f;
            cv_.capsule(lx0, ly0, 0.4f * k, lx0 + (2.0f + t) * k, ly0 - (3.5f - t) * k, 0.25f * k, bark, a * 0.9f);
            const float rx0 = W - 6.5f * k - ((18.0f + 7.0f * b) * k - 6.5f * k - sway) * f, ry0 = y0 + 4 * k - 7.0f * k * f;
            cv_.capsule(rx0, ry0, 0.4f * k, rx0 - (2.0f + t) * k, ry0 - (3.5f - t) * k, 0.25f * k, bark, a * 0.9f);
        }
        if (L.snow_cover > 0.05f) {
            cv_.capsule(4.5f * k, y0 - 0.8f * k, 0.5f * k, (16.0f + 8.0f * b) * k + sway, y0 - (5.8f + 2.0f * b) * k, 0.25f * k,
                        lit({0.92f, 0.94f, 0.98f}), L.snow_cover * 0.8f);
        }
    }
    if (leafy > 0.02f) {
        struct Blob {
            float x, y, r;
        };
        const Blob canopy[] = {{2, 4, 15}, {18, -2, 13}, {30, 3, 9}, {10, 18, 9}, {128, 2, 15}, {112, -3, 13}, {100, 2, 9}, {120, 17, 9}};
        for (const auto& b : canopy)
            cv_.ellipse(b.x * k + sway, b.y * k + 2 * k, b.r * k, b.r * 0.8f * k, 0, leafDark, leafy);
        for (const auto& b : canopy)
            cv_.ellipse(b.x * k + sway - k, b.y * k, b.r * 0.85f * k, b.r * 0.66f * k, 0, leaf, leafy);
        for (const auto& b : canopy)
            cv_.ellipse(b.x * k + sway + (L.sun_x - 0.5f) * 4 * k, b.y * k - b.r * 0.3f * k, b.r * 0.4f * k, b.r * 0.25f * k, 0,
                        leafHi, leafy * 0.8f);
        if (L.spring > 0.05f)
            for (int i = 0; i < 70; ++i) {
                const auto& b = canopy[i % 8];
                const float a = kTau * hash01(i * 3u + 1u), d = std::sqrt(hash01(i * 5u + 2u)) * b.r * 0.85f;
                cv_.blend(static_cast<int>(b.x * k + sway + std::cos(a) * d * k), static_cast<int>(b.y * k + std::sin(a) * d * 0.7f * k),
                          lit(i % 3 ? Rgb{1.0f, 0.82f, 0.90f} : Rgb{1.0f, 0.96f, 0.98f}), L.spring * leafy);
            }
    }

    // Weather and season in the air.
    const auto groundAt = [&](float x) { return horizon_ + (H - horizon_) * (0.2f + 0.8f * ground_hash(x)); };
    drift_.update(rdt, L, cv_.w, cv_.h, groundAt, 1.2f);
    drift_.draw(s, L);
    Environment::paint_fog(s, L, t_, horizon_ - 12 * k, H, 0.0f);
    paint_weather(rdt, horizon_, groundAt);
    present(canvas);
    wait_until_next_frame();
    return true;
}

Scenes::SceneDescriptor ForestPathScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.family = "seasons";
    d.tags = {"scenery", "landscape", "forest", "nature", "seasons", "calm", "animals"};
    d.intensity = 0.15f;
    d.motion = 0.2f;
    d.music_affinity = 0.05f;
    d.performance_cost = 0.3f;
    environment_->add_variants(d);
    return d;
}

// ================================================================ cottage
CottageScene::CottageScene()
{
    use_environment({Environment::TimeOfDay::Dusk, Environment::Weather::Clear, Environment::Season::Autumn, 1.0f, 0.22f});
}

void CottageScene::register_properties()
{
    HobbySceneBase::register_properties();
    decorations_->label("Decorations")
        .description("Auto decorates for the occasions that are on (real calendar or the global override); pick one to pin it.")
        .group("Environment");
    add_property(decorations_);
}

bool CottageScene::decorated(std::string_view occasion) const
{
    switch (decorations_->get().get()) {
        case CottageDecorations::AUTO: return env().occasion(occasion);
        case CottageDecorations::NONE: return false;
        case CottageDecorations::CHRISTMAS: return occasion == "christmas";
        case CottageDecorations::HALLOWEEN: return occasion == "halloween";
        case CottageDecorations::EASTER: return occasion == "easter";
        case CottageDecorations::NEW_YEAR: return occasion == "new_years_eve";
    }
    return false;
}

void CottageScene::initialize(int width, int height)
{
    HobbySceneBase::initialize(width, height);
    horizon_ = std::floor(static_cast<float>(height) * 0.60f);
    backdrop_.clear();
    smoke_.clear();
    sparks_.clear();
    drift_.reset();
    nextFirework_ = 1.5f;
}

namespace {
// Cottage layout on a 128 px panel (scaled by k).
constexpr float kWallL = 24, kWallR = 72, kWallTop = 79, kBase = 100, kRidge = 58, kRidgeL = 36, kRidgeR = 60;
constexpr float kEaveL = 19, kEaveR = 77, kDoorL = 44, kDoorR = 52, kChimX = 61;
}  // namespace

void CottageScene::build_backdrop()
{
    const auto& L = env();
    const int w = cv_.w, h = cv_.h;
    const float W = static_cast<float>(w), H = static_cast<float>(h), k = std::min(W, H) / 128.0f;  // fit the cottage on wide, short panels too
    Canvas bg;
    bg.resize(w, h);
    const Environment::Surface s{bg.px.data(), w, h, 1.0f};
    Environment::paint_sky(s, L, horizon_);
    Environment::paint_sun(s, L, Environment::sun_point(L, W, horizon_), 6.0f * k);
    Environment::paint_moon(s, L, Environment::moon_point(L, W, horizon_), 4.5f * k);

    // Two ranges of hills and the near slope the cottage sits on.
    bg.fill_below([&](float x) { return horizon_ - 4 * k - hill(x, 4.2f) * 10 * k; }, plant({0.38f, 0.48f, 0.48f}, 0.9f, 0.8f, 0.5f));
    for (float x = 0; x < W; x += 3.0f * k) {
        const uint32_t id = static_cast<uint32_t>(x * 41.0f + 3.0f);
        const float top = horizon_ - 1 * k - hill(x, 1.1f) * 5 * k;
        if (hash01(id) < 0.55f)
            pine(bg, x, top + 2 * k, (4.0f + 3.0f * hash01(id + 1)) * k, terrain({0.16f, 0.28f, 0.22f}, 0.7f, 0.5f));
    }
    bg.fill_below([&](float x) { return horizon_ - hill(x, 1.1f) * 5 * k; }, meadow({0.36f, 0.50f, 0.28f}, 0.6f));
    auto slope = [&](float x) { return H * 0.74f - 5.0f * k * std::cos((x / W - 0.38f) * 3.0f); };
    const Rgb nearFar = meadow({0.34f, 0.52f, 0.24f}, 0.25f), nearNear = meadow({0.18f, 0.34f, 0.12f});
    for (int x = 0; x < w; ++x) {
        const float top = slope(x + 0.5f);
        for (int y = static_cast<int>(top); y < h; ++y) {
            const float v = (y - top) / (H - top);
            const float n = hash01(static_cast<uint32_t>(y * w + x) * 5u + 3u);
            bg.blend(x, y, scale(mix(nearFar, nearNear, v), 0.9f + 0.18f * n * v), y == static_cast<int>(top) ? 1.0f - (top - std::floor(top)) : 1.0f);
        }
    }

    // Garden tree behind the fence.
    const float light = L.sun_x < 0.5f ? -1.0f : 1.0f;
    const Rgb snow = lit({0.92f, 0.94f, 0.98f}, 0.1f);
    broadleaf(bg, 104 * k, 98 * k, 42 * k, plant({0.30f, 0.48f, 0.18f}, 0.1f, 0.0f, 1.0f), lit({0.30f, 0.22f, 0.16f}), L.winter * 0.95f,
              71, light, snow, L.snow_cover);

    // Chimney (behind the roof edge), roof with tile rows, half-timbered walls.
    const Rgb stone = terrain({0.52f, 0.44f, 0.40f}, 0.0f, 0.0f);
    for (int y = static_cast<int>(52 * k); y < static_cast<int>(70 * k); ++y)
        for (int x = static_cast<int>((kChimX - 3) * k); x < static_cast<int>((kChimX + 3) * k); ++x)
            bg.set(x, y, scale(stone, (y / std::max(1, static_cast<int>(2 * k)) + x / std::max(1, static_cast<int>(3 * k))) % 2 ? 0.85f : 1.0f));
    for (int x = static_cast<int>((kChimX - 3.5f) * k); x < static_cast<int>((kChimX + 3.5f) * k); ++x)
        bg.set(x, static_cast<int>(52 * k), terrain({0.40f, 0.34f, 0.30f}, 0.0f, 1.0f));
    const Rgb tile = terrain({0.50f, 0.22f, 0.15f}, 0.0f, 1.0f), tileDark = terrain({0.36f, 0.15f, 0.11f}, 0.0f, 0.9f);
    for (int y = static_cast<int>(kRidge * k); y < static_cast<int>(kWallTop * k); ++y) {
        const float t = (y / k - kRidge) / (kWallTop - kRidge);
        const float x0 = (kRidgeL + (kEaveL - kRidgeL) * t) * k, x1 = (kRidgeR + (kEaveR - kRidgeR) * t) * k;
        const bool row = static_cast<int>((y / k - kRidge)) % 3 == 2;
        for (int x = static_cast<int>(x0); x <= static_cast<int>(x1); ++x) {
            const float a = std::min({1.0f, x + 1.0f - x0, x1 + 1.0f - x});
            bg.blend(x, y, row ? tileDark : tile, a);
        }
    }
    // Ridge cap and eave shadow.
    for (int x = static_cast<int>(kRidgeL * k); x <= static_cast<int>(kRidgeR * k); ++x)
        bg.set(x, static_cast<int>(kRidge * k), terrain({0.40f, 0.18f, 0.12f}, 0.0f, 1.0f));
    // The front faces away from a low sun, so it sits in shade at dawn and dusk.
    const float facade = 1.0f - 0.45f * L.twilight * (1.0f - L.overcast);
    const Rgb plaster = scale(lit({0.88f, 0.82f, 0.68f}), facade), timber = scale(lit({0.34f, 0.22f, 0.14f}), facade);
    for (int y = static_cast<int>(kWallTop * k); y < static_cast<int>(kBase * k); ++y)
        for (int x = static_cast<int>(kWallL * k); x < static_cast<int>(kWallR * k); ++x) {
            const float wx = x / k, wy = y / k;
            const bool beam = wx < kWallL + 1.5f || wx > kWallR - 1.6f || std::fabs(wy - 89.5f) < 0.8f || std::fabs(wx - 40.5f) < 0.8f ||
                              std::fabs(wx - 55.5f) < 0.8f;
            Rgb c = beam ? timber : plaster;
            if (wy < kWallTop + 2.5f)
                c = scale(c, 0.6f);  // under the eave
            bg.set(x, y, c);
        }
    // Door with a step.
    for (int y = static_cast<int>(87 * k); y < static_cast<int>(kBase * k); ++y)
        for (int x = static_cast<int>(kDoorL * k); x < static_cast<int>(kDoorR * k); ++x)
            bg.set(x, y, scale(lit(((x / std::max(1, static_cast<int>(2 * k))) % 2) ? Rgb{0.40f, 0.24f, 0.14f} : Rgb{0.34f, 0.20f, 0.12f}), facade));
    bg.set(static_cast<int>((kDoorR - 2) * k), static_cast<int>(94 * k), lit({0.85f, 0.70f, 0.30f}));
    for (int x = static_cast<int>((kDoorL - 1) * k); x <= static_cast<int>(kDoorR * k); ++x)
        bg.set(x, static_cast<int>(kBase * k), terrain({0.55f, 0.52f, 0.48f}));

    // Stone path from the door to the gate.
    for (int y = static_cast<int>(kBase * k) + 1; y < h; ++y) {
        const float v = (y / k - kBase) / (128.0f - kBase);
        const float cx = (48.0f - 4.0f * v) * k, half = (3.0f + 5.0f * v) * k;
        for (int x = static_cast<int>(cx - half); x <= static_cast<int>(cx + half); ++x) {
            const bool gap = hash01(static_cast<uint32_t>((y / std::max(1, static_cast<int>(3 * k))) * 31 + x / std::max(1, static_cast<int>(4 * k)))) < 0.18f;
            bg.blend(x, y, terrain(gap ? Rgb{0.36f, 0.34f, 0.30f} : Rgb{0.60f, 0.57f, 0.52f}, 0.0f, 0.9f), 0.9f);
        }
    }

    // Picket fence with a gate gap at the path.
    const float fenceY = 112 * k;
    const Rgb wood = terrain({0.74f, 0.66f, 0.54f}, 0.0f, 0.4f);
    for (float x = 1 * k; x < W; x += 4 * k) {
        if (x > 38 * k && x < 56 * k)
            continue;
        bg.capsule(x, fenceY + 6 * k, 0.6f * k, x, fenceY - 3 * k, 0.6f * k, wood);
    }
    for (float ry : {fenceY - 1 * k, fenceY + 3 * k}) {
        bg.capsule(0, ry, 0.45f * k, 38 * k, ry, 0.45f * k, wood);
        bg.capsule(56 * k, ry, 0.45f * k, W, ry, 0.45f * k, wood);
    }
    backdrop_ = std::move(bg.px);
}

bool CottageScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt;
    begin_frame(dt);
    const float rdt = dt / std::max(0.05f, speed_->get());
    const auto& L = env();
    const float W = static_cast<float>(cv_.w), H = static_cast<float>(cv_.h), k = std::min(W, H) / 128.0f;
    if (environment_changed(backdropKey_) || backdrop_.empty())
        build_backdrop();
    cv_.px = backdrop_;
    const auto s = surface();
    Environment::paint_stars(s, L, horizon_ * 0.75f, t_, 9, 70);
    Environment::paint_clouds(s, L, t_, 0.0f, horizon_ * 0.5f);

    // New Year's Eve: fireworks over the hills after dark.
    if (decorated("new_years_eve") && L.night > 0.4f) {
        nextFirework_ -= rdt;
        if (nextFirework_ <= 0.0f) {
            nextFirework_ = 1.2f + rnd() * 2.0f;
            const float fx = (0.15f + 0.7f * rnd()) * W, fy = (0.12f + 0.25f * rnd()) * H;
            const int hue = static_cast<int>(rnd() * 4.0f);
            for (int i = 0; i < 26; ++i) {
                const float a = kTau * i / 26.0f, v = (9.0f + 5.0f * rnd()) * k;
                sparks_.push_back({fx, fy, std::cos(a) * v, std::sin(a) * v, 1.6f, hue});
            }
        }
    }
    static const Rgb fwCols[] = {{1.0f, 0.35f, 0.30f}, {0.40f, 0.90f, 0.50f}, {1.0f, 0.85f, 0.35f}, {0.55f, 0.65f, 1.0f}};
    for (auto& p : sparks_) {
        p.life -= rdt;
        p.vy += 6.0f * k * rdt;
        p.vx *= std::exp(-rdt * 0.8f);
        p.x += p.vx * rdt;
        p.y += p.vy * rdt;
        cv_.add(static_cast<int>(p.x), static_cast<int>(p.y), fwCols[p.hue], std::clamp(p.life, 0.0f, 1.0f));
    }
    std::erase_if(sparks_, [&](const Spark& p) { return p.life <= 0.0f || p.y > horizon_; });

    // Chimney smoke: more when it is cold, drifting with the wind.
    const float cold = std::clamp((14.0f - L.temperature) / 14.0f, 0.0f, 1.0f) * 0.7f + 0.3f * L.winter + 0.25f;
    smokeCarry_ += rdt * 3.0f * std::min(1.0f, cold);
    while (smokeCarry_ >= 1.0f) {
        smokeCarry_ -= 1.0f;
        smoke_.push_back({kChimX * k + (rnd() - 0.5f) * 2 * k, 51 * k, 1.4f * k, 0.0f});
    }
    const Rgb smokeC = mix(lit({0.78f, 0.78f, 0.80f}, 0.3f), from_env(L.haze), 0.3f);
    for (auto& p : smoke_) {
        p.life += rdt;
        p.y -= (5.0f - 2.0f * std::min(1.0f, p.life / 4.0f)) * k * rdt;
        p.x += (L.wind * 6.0f + 1.0f + std::sin(p.life * 1.3f) * 1.2f) * k * rdt;
        p.r += 0.9f * k * rdt;
        cv_.ellipse(p.x, p.y, p.r, p.r * 0.85f, 0, smokeC, 0.38f * std::max(0.0f, 1.0f - p.life / 6.0f));
    }
    std::erase_if(smoke_, [](const Puff& p) { return p.life > 6.0f; });

    // Windows (lit at dusk) with mullions and window boxes.
    const Rgb timber = lit({0.30f, 0.19f, 0.12f});
    for (float wx : {28.0f, 60.0f}) {
        Environment::paint_window(s, L, static_cast<int>(wx * k), static_cast<int>(83 * k), static_cast<int>(8 * k), static_cast<int>(8 * k),
                                  static_cast<uint32_t>(wx), t_);
        for (int i = 0; i < static_cast<int>(8 * k); ++i) {
            cv_.set(static_cast<int>((wx + 4) * k), static_cast<int>(83 * k) + i, timber);
            cv_.set(static_cast<int>(wx * k) + i, static_cast<int>(87 * k), timber);
        }
        for (int i = -1; i <= static_cast<int>(8 * k); ++i) cv_.set(static_cast<int>(wx * k) + i, static_cast<int>(92 * k), timber);
        // Flowers in the box from spring to early autumn.
        const float bloom = (L.spring + L.summer + 0.4f * L.autumn) * (1.0f - L.snow_cover);
        for (int i = 0; i < 4 && bloom > 0.2f; ++i)
            cv_.blend(static_cast<int>((wx + 1 + i * 2) * k), static_cast<int>(91 * k),
                      lit(i % 2 ? Rgb{0.95f, 0.30f, 0.35f} : Rgb{1.0f, 0.80f, 0.30f}), bloom);
    }
    // Small gable window under the ridge.
    Environment::paint_window(s, L, static_cast<int>(46 * k), static_cast<int>(64 * k), static_cast<int>(4 * k), static_cast<int>(5 * k), 9, t_);

    // Garden lamp by the gate.
    const float gx = 37 * k, gb = 116 * k;
    cv_.capsule(gx, gb, 0.6f * k, gx, gb - 13 * k, 0.5f * k, lit({0.18f, 0.18f, 0.20f}));
    cv_.ellipse(gx, gb - 14 * k, 1.4f * k, 1.6f * k, 0, mix(lit({0.30f, 0.30f, 0.28f}), {1.0f, 0.85f, 0.55f}, L.lamp()));
    cv_.glow(gx, gb - 14 * k, 13 * k, {1.0f, 0.75f, 0.40f}, 0.35f * L.lamp());

    // Snowman once there is enough snow.
    if (L.snow_cover > 0.35f) {
        const float a = std::clamp((L.snow_cover - 0.35f) / 0.3f, 0.0f, 1.0f);
        const Rgb snowC = lit({0.94f, 0.95f, 0.98f}), shadow = lit({0.70f, 0.75f, 0.86f});
        const float sx = 14 * k, sb = 108 * k;
        cv_.ellipse(sx + 0.6f * k, sb - 4 * k, 5 * k, 4.5f * k, 0, shadow, a);
        cv_.ellipse(sx, sb - 4.2f * k, 4.6f * k, 4.2f * k, 0, snowC, a);
        cv_.ellipse(sx, sb - 10.5f * k, 3.4f * k, 3.2f * k, 0, snowC, a);
        cv_.ellipse(sx, sb - 15.5f * k, 2.4f * k, 2.3f * k, 0, snowC, a);
        cv_.capsule(sx + 1 * k, sb - 15.5f * k, 0.4f * k, sx + 3.4f * k, sb - 15.2f * k, 0.2f * k, lit({1.0f, 0.5f, 0.1f}), a);
        cv_.capsule(sx - 2.6f * k, sb - 13 * k, 0.6f * k, sx + 2.6f * k, sb - 13 * k, 0.6f * k, lit({0.80f, 0.15f, 0.15f}), a);
        cv_.capsule(sx - 3 * k, sb - 10.5f * k, 0.3f * k, sx - 7 * k, sb - 14 * k, 0.25f * k, lit({0.30f, 0.20f, 0.12f}), a);
        cv_.capsule(sx + 3 * k, sb - 10.5f * k, 0.3f * k, sx + 7 * k, sb - 13 * k, 0.25f * k, lit({0.30f, 0.20f, 0.12f}), a);
    }

    // Occasions.
    if (decorated("christmas")) {
        Environment::paint_string_lights(s, L, {kEaveL * k, kWallTop * k}, {kEaveR * k, kWallTop * k}, 2.0f * k, t_, 14);
        Environment::paint_string_lights(s, L, {1 * k, 111 * k}, {37 * k, 111 * k}, 1.5f * k, t_, 8, 1);
        Environment::paint_string_lights(s, L, {57 * k, 111 * k}, {W, 111 * k}, 1.5f * k, t_, 14, 2);
        // Wreath on the door.
        const Rgb green = lit({0.10f, 0.40f, 0.16f});
        for (int i = 0; i < 12; ++i) {
            const float a = kTau * i / 12.0f;
            cv_.set(static_cast<int>(48 * k + std::cos(a) * 2.2f * k), static_cast<int>(90.5f * k + std::sin(a) * 2.2f * k), green);
        }
        cv_.set(static_cast<int>(48 * k), static_cast<int>(92.7f * k), lit({0.9f, 0.1f, 0.1f}));
    }
    if (decorated("halloween")) {
        // Carved jack-o'-lanterns in the front garden, big enough to read.
        const Rgb fire{1.0f, 0.78f, 0.22f};
        const float flick = 0.85f + 0.15f * std::sin(t_ * 9.0f) * std::sin(t_ * 5.3f);
        auto pumpkin = [&](float px, float py, float r) {
            const Rgb skin = subject({0.95f, 0.42f, 0.05f}), rib = subject({0.62f, 0.24f, 0.03f});
            cv_.ellipse(px * k, (py + r * 0.8f) * k, r * 1.2f * k, 0.8f * k, 0, {0.02f, 0.02f, 0.03f}, 0.4f);
            cv_.ellipse(px * k, py * k, r * 1.15f * k, r * 0.85f * k, 0, skin);
            for (float off : {-0.45f, 0.0f, 0.45f})
                cv_.capsule((px + off * r) * k, (py - 0.7f * r) * k, 0.25f * k, (px + off * r * 1.1f) * k, (py + 0.7f * r) * k, 0.25f * k, rib, 0.6f);
            cv_.capsule(px * k, (py - 0.8f * r) * k, 0.5f * k, (px + 0.4f) * k, (py - 0.8f * r - 1.6f) * k, 0.4f * k, subject({0.25f, 0.35f, 0.10f}));
            const float g = (0.55f + 0.45f * L.lamp()) * flick;
            for (float side : {-1.0f, 1.0f}) {  // triangle eyes
                const float ex = px + side * 0.42f * r, ey = py - 0.18f * r;
                cv_.add(static_cast<int>(ex * k), static_cast<int>(ey * k), fire, g);
                cv_.add(static_cast<int>(ex * k), static_cast<int>(ey * k) + 1, fire, g);
                cv_.add(static_cast<int>(ex * k) - 1, static_cast<int>(ey * k) + 1, fire, g);
            }
            for (int i = -2; i <= 2; ++i)  // jagged grin
                cv_.add(static_cast<int>((px + i * 0.22f * r) * k), static_cast<int>((py + 0.35f * r) * k) + (i % 2 ? 1 : 0), fire, g);
            cv_.glow(px * k, py * k, r * 3.0f * k, {1.0f, 0.50f, 0.12f}, 0.22f * g);
        };
        pumpkin(13, 116, 5.5f);
        pumpkin(26, 121, 4.0f);
        pumpkin(98, 118, 5.0f);
        // Bats wheeling over the roof.
        for (int i = 0; i < 3; ++i) {
            const float a = t_ * (0.35f + 0.08f * i) + i * 2.1f;
            const float bx = (64 + 34 * std::cos(a) + 10 * i) * k, by = (30 + 10 * std::sin(a * 1.7f) + 6 * i) * k;
            const float flap = std::sin(t_ * 14.0f + i) * 1.4f * k;
            const Rgb bat{0.05f, 0.04f, 0.06f};
            cv_.capsule(bx - 3 * k, by - flap, 0.35f * k, bx, by, 0.4f * k, bat);
            cv_.capsule(bx, by, 0.4f * k, bx + 3 * k, by - flap, 0.35f * k, bat);
        }
    }
    if (decorated("easter")) {
        static const Rgb eggs[] = {{0.95f, 0.45f, 0.55f}, {0.45f, 0.70f, 0.98f}, {0.98f, 0.85f, 0.30f}, {0.55f, 0.90f, 0.50f}, {0.80f, 0.55f, 0.95f}};
        const float pos[][2] = {{8, 118}, {22, 122}, {70, 119}, {88, 124}, {116, 117}, {62, 125}};
        for (int i = 0; i < 6; ++i) cv_.ellipse(pos[i][0] * k, pos[i][1] * k, 1.2f * k, 1.6f * k, 0, lit(eggs[i % 5]));
    }

    const auto groundAt = [&](float x) { return H * (0.76f + 0.22f * ground_hash(x)); };
    drift_.update(rdt, L, cv_.w, cv_.h, groundAt, 0.8f);
    drift_.draw(s, L);
    Environment::paint_fog(s, L, t_, horizon_ - 14 * k, H, 0.0f);
    paint_weather(rdt, horizon_, groundAt);
    present(canvas);
    wait_until_next_frame();
    return true;
}

Scenes::SceneDescriptor CottageScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.family = "seasons";
    d.tags = {"scenery", "landscape", "cozy", "home", "seasons", "holidays", "calm"};
    d.intensity = 0.15f;
    d.motion = 0.15f;
    d.music_affinity = 0.05f;
    d.performance_cost = 0.25f;
    auto occasion = [](std::string id, std::string label, std::string description, const char* deco, const char* season,
                       const char* time) {
        Scenes::SceneVariant v;
        v.id = std::move(id);
        v.label = std::move(label);
        v.description = std::move(description);
        v.properties = {{"decorations", deco}, {"season", season}, {"time_of_day", time}};
        v.tags = {"holiday"};
        return v;
    };
    environment_->add_variants(
        d, {occasion("christmas", "Christmas", "Lights, a wreath and a snowy winter night.", "CHRISTMAS", "WINTER", "NIGHT"),
            occasion("halloween", "Halloween", "Glowing pumpkins on an autumn night.", "HALLOWEEN", "AUTUMN", "NIGHT"),
            occasion("easter", "Easter", "Painted eggs in a spring garden.", "EASTER", "SPRING", "DAY"),
            occasion("new_year", "New Year's Eve", "Fireworks over a winter night.", "NEW_YEAR", "WINTER", "NIGHT")});
    return d;
}
