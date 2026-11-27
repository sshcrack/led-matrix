#include "MurmurationScene.h"

#include "Backdrop.h"

namespace Hobbies {
namespace {

// Design rows (128 × 128 design space).
constexpr float ShoreY = 97.0f;  // far shore; water below
constexpr float CrossfadeSeconds = 3.0f;
constexpr float SimStep = 1.0f / 30.0f;

// Flocking (design px, seconds). Every bird steers by its seven nearest
// neighbours: alignment with their heading, cohesion toward their middle,
// separation from any that come too close. Speed stays near cruise.
constexpr int Neighbours = 7;
constexpr float Cruise = 18.0f;     // px/s
constexpr float Separation = 2.3f;  // px, closer than this pushes apart
constexpr float SeparationGain = 70.0f;
constexpr float AlignGain = 3.6f;     // 1/s
constexpr float CohesionGain = 0.5f;  // 1/s²
constexpr float CentreGain = 0.35f;   // 1/s², pulls back birds that stray far from the flock's middle
constexpr float CentreFree = 18.0f;   // px from the middle where that pull starts
constexpr float MaxAccel = 42.0f;     // px/s², keeps turns smooth
constexpr float SpeedRelax = 2.5f;    // 1/s
constexpr float DepthSpring = 0.03f;  // keeps the flock from drifting far in depth
constexpr float TurnRate = 3.2f;      // rad/s for a turn passed on by a wave

// Spatial grid for the neighbour search (covers the panel plus a margin).
constexpr float CellSize = 3.0f;
constexpr float GridX0 = -110.0f, GridY0 = -90.0f;
constexpr int GridW = 120, GridH = 95;

// Leaving the view.
constexpr float RemoveLeft = -60.0f, RemoveRight = 188.0f, RemoveTop = -50.0f;

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
    birdCount_->label("Birds")
        .description("Size of the largest flocks. Each flock that arrives is a different size up to this.")
        .group("Flock");
    falconOn_->label("Falcon").description("A falcon now and then hunts the flock, which swerves away from it.").group("Flock");
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
    birds_.clear();
    fliers_.clear();
    cellStart_.assign(static_cast<size_t>(GridW * GridH + 1), 0);
    // The first flock arrives soon after the scene starts.
    phase_ = Phase::Empty;
    phaseLeft_ = 2.0f + 3.0f * rnd();
    stragglerWait_ = 1.0f + 2.0f * rnd();
    falconWait_ = 30.0f + 30.0f * rnd();
    waveWait_ = 3.0f;
    clock_ = 0.0f;
    simCarry_ = 0.0f;
    for (auto& p : pathPhase_) p = rnd() * kTau;
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
    const Rgb farHills = from_env(L.lit(to_env({0.30f, 0.34f, 0.36f}), 0.85f));
    bg.fill_below([&](float x) { return (ShoreY - 2.5f - 7.0f * hill(x / sx * 0.8f, 1.7f)) * sy; }, farHills);
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
    // Where the reeds stand, so roosting birds settle into them and not onto open water.
    reedTop_.assign(static_cast<size_t>(cv_.w), static_cast<float>(cv_.h));
    for (int x = 0; x < cv_.w; ++x)
        for (int y = shoreRow + 1; y < cv_.h; ++y)
            if (!water_[static_cast<size_t>(y * cv_.w + x)]) {
                reedTop_[static_cast<size_t>(x)] = static_cast<float>(y);
                break;
            }
}

float MurmurationScene::reed_top(float x) const
{
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f;
    const int col = static_cast<int>(std::floor(x * sx));
    if (col < 0 || col >= static_cast<int>(reedTop_.size()))
        return 128.0f;
    return reedTop_[static_cast<size_t>(col)] / sy;
}

void MurmurationScene::start_flock()
{
    // Fewer birds when the Pi is short on time; every flock is a different size.
    const float q = std::clamp((render_quality_scale() - 0.5f) / 0.5f, 0.0f, 1.0f);
    const float largest = static_cast<float>(std::clamp(birdCount_->get(), 150, 1200)) * (0.5f + 0.5f * q);
    const int n = std::max(60, static_cast<int>(largest * (0.4f + 0.6f * rnd())));
    // The flock streams in from one side, densest at its head.
    const float side = rnd() < 0.5f ? -1.0f : 1.0f;
    const float ex = side < 0.0f ? -16.0f : 144.0f, ey = 16.0f + 34.0f * rnd();
    birds_.clear();
    birds_.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        const float along = 55.0f * rnd() * rnd();
        Bird b{};
        b.x = ex + side * along;
        b.y = ey + (rnd() - 0.5f) * (6.0f + 0.3f * along);
        b.z = (rnd() - 0.5f) * 12.0f;
        b.cruise = Cruise * (0.92f + 0.16f * rnd());
        b.vx = -side * b.cruise;
        b.vy = (rnd() - 0.5f) * 2.0f;
        b.wander = rnd() * kTau;
        b.wave = waveId_;
        birds_.push_back(b);
    }
    phase_ = Phase::Perform;
    phaseLeft_ = 45.0f + 65.0f * rnd();
    waveWait_ = 3.0f + 3.0f * rnd();
}

void MurmurationScene::spawn_stragglers(bool near_only)
{
    // A few birds passing straight across: far specks, or nearer birds whose wingbeats show.
    const bool near = near_only || rnd() < 0.35f;
    const int count = near ? 1 + static_cast<int>(rnd() * 3.0f) : 1 + static_cast<int>(rnd() * 6.0f);
    const float dir = rnd() < 0.5f ? 1.0f : -1.0f;
    const float y0 = near ? 30.0f + 45.0f * rnd() : 12.0f + 60.0f * rnd();
    const float speed = near ? 26.0f + 10.0f * rnd() : 15.0f + 6.0f * rnd();
    const float vy = (rnd() - 0.5f) * 3.0f;
    for (int i = 0; i < count; ++i) {
        Flier f{};
        f.x = (dir > 0.0f ? -8.0f : 136.0f) - dir * (static_cast<float>(i) * (near ? 7.0f : 4.0f) + 3.0f * rnd());
        f.y = y0 + (rnd() - 0.5f) * (near ? 8.0f : 6.0f);
        f.vx = dir * speed * (0.95f + 0.1f * rnd());
        f.vy = vy;
        f.span = near ? 3.5f + 1.8f * rnd() : 1.0f;
        f.phase = rnd() * kTau;
        f.beat = 4.0f + 1.5f * rnd();
        f.flapping = true;
        f.mode = 0.4f + 0.8f * rnd();
        fliers_.push_back(f);
    }
}

void MurmurationScene::spawn_falcon()
{
    // It cuts across the flock from above.
    float cx = 0.0f, cy = 0.0f;
    for (const auto& b : birds_) {
        cx += b.x;
        cy += b.y;
    }
    cx /= static_cast<float>(birds_.size());
    cy /= static_cast<float>(birds_.size());
    Flier f{};
    f.x = cx < 64.0f ? 140.0f : -12.0f;
    f.y = std::max(-6.0f, cy - 22.0f - 10.0f * rnd());
    const float dx = cx - f.x, dy = cy - f.y, len = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
    f.vx = dx / len * 34.0f;
    f.vy = dy / len * 34.0f;
    f.span = 6.0f;
    f.beat = 3.0f;
    f.flapping = true;
    f.mode = 1.0f;
    f.falcon = true;
    fliers_.push_back(f);
}

void MurmurationScene::update_phase(float dt)
{
    clock_ += dt;
    switch (phase_) {
        case Phase::Empty:
            phaseLeft_ -= dt;
            if (phaseLeft_ <= 0.0f)
                start_flock();
            break;
        case Phase::Perform:
            phaseLeft_ -= dt;
            // The flock wheels around a point that wanders over the water.
            ax_ = 64.0f + 26.0f * std::sin(0.11f * clock_ + pathPhase_[0]) + 10.0f * std::sin(0.27f * clock_ + pathPhase_[1]);
            ay_ = 40.0f + 8.0f * std::sin(0.13f * clock_ + pathPhase_[2]) + 5.0f * std::sin(0.31f * clock_ + pathPhase_[3]);
            // It also moves toward and away from the viewer, so the flock turns in depth and folds.
            az_ = 22.0f * std::sin(0.09f * clock_ + pathPhase_[1] + 1.0f);
            if (phaseLeft_ <= 0.0f) {
                phaseLeft_ = 0.0f;
                if (rnd() < 0.55f) {
                    // Dusk: the flock pours down into the reeds to roost.
                    phase_ = Phase::Roost;
                    descending_ = false;
                    roostX_ = rnd() < 0.6f ? 9.0f + 9.0f * rnd() : 110.0f + 9.0f * rnd();
                }
                else {
                    phase_ = Phase::Depart;
                    exitX_ = rnd() < 0.5f ? -160.0f : 290.0f;
                    exitY_ = -30.0f + 50.0f * rnd();
                }
            }
            break;
        case Phase::Depart:
        case Phase::Roost:
            phaseLeft_ += dt;
            az_ = 0.0f;
            ax_ = phase_ == Phase::Roost ? roostX_ : exitX_;
            ay_ = exitY_;
            if (phase_ == Phase::Roost) {
                // First gather over the reed patch, then pour straight down into it.
                float cx = 0.0f;
                for (const auto& b : birds_) cx += b.x;
                cx /= static_cast<float>(std::max<size_t>(1, birds_.size()));
                if (std::fabs(cx - roostX_) < 10.0f || phaseLeft_ > 30.0f)
                    descending_ = true;
                ay_ = descending_ ? std::min(reed_top(roostX_) + 6.0f, 124.0f) : 58.0f;
            }
            if (phase_ == Phase::Roost && phaseLeft_ > 70.0f) {
                phase_ = Phase::Depart;  // the last stragglers fly off instead
                exitX_ = ax_ < 64.0f ? -160.0f : 290.0f;
                exitY_ = -20.0f;
            }
            if (phaseLeft_ > 140.0f)
                birds_.clear();
            if (birds_.empty()) {
                phase_ = Phase::Empty;
                phaseLeft_ = 18.0f + 40.0f * rnd();
            }
            break;
    }

    if (phase_ == Phase::Perform && falconOn_->get() && birds_.size() > 40) {
        falconWait_ -= dt;
        if (falconWait_ <= 0.0f) {
            spawn_falcon();
            falconWait_ = 45.0f + 45.0f * rnd();
        }
    }
    stragglerWait_ -= dt;
    if (stragglerWait_ <= 0.0f) {
        spawn_stragglers(phase_ != Phase::Empty);
        stragglerWait_ = phase_ == Phase::Empty ? 4.0f + 8.0f * rnd() : 16.0f + 24.0f * rnd();
    }
}

void MurmurationScene::simulate(float h)
{
    const size_t n = birds_.size();
    if (n == 0)
        return;

    // Bucket the birds into the grid (counting sort).
    std::fill(cellStart_.begin(), cellStart_.end(), 0);
    cellOf_.resize(n);
    cellBirds_.resize(n);
    for (size_t i = 0; i < n; ++i) {
        const int gx = std::clamp(static_cast<int>((birds_[i].x - GridX0) / CellSize), 0, GridW - 1);
        const int gy = std::clamp(static_cast<int>((birds_[i].y - GridY0) / CellSize), 0, GridH - 1);
        cellOf_[i] = gy * GridW + gx;
        ++cellStart_[static_cast<size_t>(cellOf_[i] + 1)];
    }
    for (size_t c = 1; c < cellStart_.size(); ++c) cellStart_[c] += cellStart_[c - 1];
    cellFill_.assign(cellStart_.begin(), cellStart_.end() - 1);
    for (size_t i = 0; i < n; ++i) cellBirds_[static_cast<size_t>(cellFill_[static_cast<size_t>(cellOf_[i])]++)] = static_cast<int>(i);

    // Now and then one bird starts a turn; its neighbours copy it and the turn runs through the flock.
    if (phase_ == Phase::Perform) {
        waveWait_ -= h;
        if (waveWait_ <= 0.0f && n > 20) {
            ++waveId_;
            waveAngle_ = (rnd() < 0.5f ? -1.0f : 1.0f) * (0.35f + 0.4f * rnd());
            auto& starter = birds_[static_cast<size_t>(rnd() * static_cast<float>(n - 1))];
            starter.wave = waveId_;
            starter.turn = waveAngle_;
            waveWait_ = 2.5f + 4.0f * rnd();
        }
    }

    const Flier* falcon = nullptr;
    for (const auto& f : fliers_)
        if (f.falcon)
            falcon = &f;

    // The flock's middle, for the weak pull that keeps it one flock.
    float midX = 0.0f, midY = 0.0f, midZ = 0.0f;
    for (const auto& b : birds_) {
        midX += b.x;
        midY += b.y;
        midZ += b.z;
    }
    midX /= static_cast<float>(n);
    midY /= static_cast<float>(n);
    midZ /= static_cast<float>(n);
    const float centre = phase_ == Phase::Perform ? CentreGain : 0.0f;

    const float radius = phase_ == Phase::Perform ? 20.0f : phase_ == Phase::Roost ? 3.0f : 0.0f;
    const float pull = phase_ == Phase::Perform ? 7.0f : phase_ == Phase::Roost ? 12.0f : 14.0f;
    next_.resize(n);
    for (size_t i = 0; i < n; ++i) {
        const Bird& b = birds_[i];
        Bird o = b;

        // The seven nearest birds, from the 3×3 cells around it (5×5 if that is not enough).
        int nearest[Neighbours];
        float dist2[Neighbours];
        int k = 0;
        const int gx = cellOf_[i] % GridW, gy = cellOf_[i] / GridW;
        for (int ring = 1; ring <= 2 && k < Neighbours; ++ring)
            for (int yy = gy - ring; yy <= gy + ring; ++yy)
                for (int xx = gx - ring; xx <= gx + ring; ++xx) {
                    if (xx < 0 || yy < 0 || xx >= GridW || yy >= GridH)
                        continue;
                    if (ring == 2 && std::abs(xx - gx) < 2 && std::abs(yy - gy) < 2)
                        continue;
                    const int c = yy * GridW + xx;
                    for (int s = cellStart_[static_cast<size_t>(c)]; s < cellStart_[static_cast<size_t>(c + 1)]; ++s) {
                        const int j = cellBirds_[static_cast<size_t>(s)];
                        if (static_cast<size_t>(j) == i)
                            continue;
                        const Bird& o2 = birds_[static_cast<size_t>(j)];
                        const float dx = o2.x - b.x, dy = o2.y - b.y, dz = o2.z - b.z;
                        const float d2 = dx * dx + dy * dy + dz * dz;
                        if (k == Neighbours && d2 >= dist2[Neighbours - 1])
                            continue;
                        int at = k < Neighbours ? k++ : Neighbours - 1;
                        while (at > 0 && dist2[at - 1] > d2) {
                            dist2[at] = dist2[at - 1];
                            nearest[at] = nearest[at - 1];
                            --at;
                        }
                        dist2[at] = d2;
                        nearest[at] = j;
                    }
                }

        float accX = 0.0f, accY = 0.0f, accZ = 0.0f;
        if (k > 0) {
            float mx = 0, my = 0, mz = 0, mvx = 0, mvy = 0, mvz = 0;
            for (int m = 0; m < k; ++m) {
                const Bird& nb = birds_[static_cast<size_t>(nearest[m])];
                mx += nb.x;
                my += nb.y;
                mz += nb.z;
                mvx += nb.vx;
                mvy += nb.vy;
                mvz += nb.vz;
                const float d = std::sqrt(dist2[m]);
                if (d < Separation && d > 1e-3f) {
                    const float push = SeparationGain * (Separation - d) / (Separation * d);
                    accX += (b.x - nb.x) * push;
                    accY += (b.y - nb.y) * push;
                    accZ += (b.z - nb.z) * push;
                }
                if (nb.wave == waveId_ && o.wave != waveId_ && nb.bank > 0.35f) {
                    o.wave = waveId_;
                    o.turn = waveAngle_;
                }
            }
            const float inv = 1.0f / static_cast<float>(k);
            accX += (mvx * inv - b.vx) * AlignGain + (mx * inv - b.x) * CohesionGain;
            accY += (mvy * inv - b.vy) * AlignGain + (my * inv - b.y) * CohesionGain;
            accZ += (mvz * inv - b.vz) * AlignGain + (mz * inv - b.z) * CohesionGain;
        }

        {
            const float mx = midX - b.x, my = midY - b.y, mz = midZ - b.z;
            const float md = std::sqrt(mx * mx + my * my + mz * mz);
            if (md > CentreFree) {
                const float f = centre * (md - CentreFree) / md;
                accX += mx * f;
                accY += my * f;
                accZ += mz * f;
            }
        }

        // Drawn to the flock's point over the water (or the roost, or away).
        const float tx = ax_ - b.x, ty = ay_ - b.y, tz = az_ - b.z, td = std::sqrt(tx * tx + ty * ty + tz * tz);
        if (td > radius && td > 1e-3f) {
            const float f = pull * std::min(1.0f, (td - radius) / 18.0f) / td;
            accX += tx * f;
            accY += ty * f;
            accZ += tz * f * 0.3f;  // the pull is mostly horizontal, as toward a roost
        }
        if (phase_ == Phase::Roost && descending_ && std::fabs(b.x - roostX_) < 12.0f)
            accY += 8.0f;  // the funnel pours down into the reeds
        if (phase_ != Phase::Roost && b.y > 72.0f)
            accY -= (b.y - 72.0f) * 4.0f;
        if (phase_ == Phase::Perform && b.y < 14.0f)
            accY += (14.0f - b.y) * 5.0f;
        accZ -= b.z * DepthSpring;
        // A small, slow individual drift.
        const float sp0 = std::max(1e-3f, std::sqrt(b.vx * b.vx + b.vy * b.vy));
        const float drift = 2.2f * std::sin(clock_ * 0.7f + b.wander);
        accX += -b.vy / sp0 * drift;
        accY += b.vx / sp0 * drift;
        if (falcon) {
            const float dx = b.x - falcon->x, dy = b.y - falcon->y, d = std::sqrt(dx * dx + dy * dy);
            if (d < 13.0f && d > 1e-3f) {
                const float flee = 90.0f * (1.0f - d / 13.0f) / d;
                accX += dx * flee;
                accY += dy * flee;
                o.bank = 1.0f;
            }
        }
        const float a = std::sqrt(accX * accX + accY * accY + accZ * accZ);
        if (a > MaxAccel) {
            const float s = MaxAccel / a;
            accX *= s;
            accY *= s;
            accZ *= s;
        }
        o.vx += accX * h;
        o.vy += accY * h;
        o.vz += accZ * h;
        // A turn passed on by a wave is made smoothly over a few steps.
        if (o.turn != 0.0f) {
            const float step = std::clamp(o.turn, -TurnRate * h, TurnRate * h);
            const float c = std::cos(step), s = std::sin(step);
            const float vx = o.vx * c - o.vy * s;
            o.vy = o.vx * s + o.vy * c;
            o.vx = vx;
            o.turn -= step;
            if (std::fabs(o.turn) < 1e-3f)
                o.turn = 0.0f;
        }
        // Nearly constant speed.
        const float sp = std::sqrt(o.vx * o.vx + o.vy * o.vy + o.vz * o.vz);
        if (sp > 1e-3f) {
            const float s = 1.0f + (o.cruise - sp) / sp * std::min(1.0f, SpeedRelax * h);
            o.vx *= s;
            o.vy *= s;
            o.vz *= s;
        }
        // Banking: how fast the heading turns.
        const float sp1 = std::max(1e-3f, std::sqrt(o.vx * o.vx + o.vy * o.vy));
        const float omega = std::fabs(b.vx * o.vy - b.vy * o.vx) / (sp0 * sp1 * h);
        o.bank = std::max(o.bank - 1.8f * h, std::min(1.0f, omega / 1.6f));
        o.x += o.vx * h;
        o.y += o.vy * h;
        o.z += o.vz * h;
        next_[i] = o;
    }
    birds_.swap(next_);

    if (phase_ == Phase::Depart || phase_ == Phase::Roost) {
        const bool roosting = phase_ == Phase::Roost;
        std::erase_if(birds_, [this, roosting](const Bird& b) {
            if (b.x < RemoveLeft || b.x > RemoveRight || b.y < RemoveTop || b.y > 127.0f)
                return true;
            // Birds that drop below the reed tops settle in and disappear among them.
            return roosting && b.y > reed_top(b.x) + 5.0f;
        });
    }
}

void MurmurationScene::update_fliers(float dt)
{
    for (auto& f : fliers_) {
        f.x += f.vx * dt;
        f.y += f.vy * dt;
        if (f.falcon)
            f.vy -= 4.0f * dt;  // pulls up after the stoop
        // Starlings flap in short bursts and glide in between.
        f.mode -= dt;
        if (f.mode <= 0.0f) {
            f.flapping = !f.flapping;
            f.mode = f.flapping ? 0.5f + 0.8f * rnd() : 0.4f + 0.7f * rnd();
        }
        if (f.flapping)
            f.phase += dt * f.beat * kTau;
    }
    std::erase_if(fliers_, [](const Flier& f) { return f.x < -24.0f || f.x > 152.0f || f.y < -24.0f || f.y > 140.0f; });
}

void MurmurationScene::draw_flier(const Flier& f, Rgb color)
{
    // Body with head and tail, and two wings that beat between a raised "m" and a lowered stroke.
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f;
    const float S = f.span * sx, cx = f.x * sx, cy = f.y * sy, hx = f.vx >= 0.0f ? 1.0f : -1.0f;
    const float w = f.flapping ? std::cos(f.phase) : 0.25f;
    const float r = 0.42f * std::max(1.0f, sx);
    cv_.capsule(cx - hx * S * 0.24f, cy + 0.06f * S, r, cx + hx * S * 0.2f, cy - 0.04f * S, r * 1.1f, color, 0.95f);
    for (const float side : {-1.0f, 1.0f}) {
        const float ex = cx + side * S * 0.26f, ey = cy - w * S * 0.30f;
        const float tipX = cx + side * S * 0.52f, tipY = cy - w * S * 0.12f + (w > 0.0f ? 0.08f * S : 0.0f);
        cv_.capsule(cx, cy, r, ex, ey, r * 0.9f, color, 0.95f);
        cv_.capsule(ex, ey, r * 0.9f, tipX, tipY, r * 0.6f, color, 0.9f);
    }
}

void MurmurationScene::draw_birds()
{
    const int W = cv_.w, H = cv_.h;
    const float sx = W / 128.0f, sy = H / 128.0f;
    std::fill(ink_.begin(), ink_.end(), 0.0f);
    // A speck is shared between the two pixels it straddles horizontally, so slow drifts do not shimmer.
    const auto mark = [&](float x, float y, float a) {
        const float fx = x * sx - 0.5f;
        const int px = static_cast<int>(std::floor(fx)), py = static_cast<int>(std::floor(y * sy));
        if (py < 0 || py >= H)
            return;
        const float right = fx - static_cast<float>(px);
        if (px >= 0 && px < W)
            ink_[static_cast<size_t>(py * W + px)] += a * (1.0f - right);
        if (px + 1 >= 0 && px + 1 < W)
            ink_[static_cast<size_t>(py * W + px + 1)] += a * right;
    };
    // Each far bird is a single dark speck; banking birds show more wing.
    // Fog veils the sky, so each bird marks a little stronger to stay visible.
    const float strength = 1.0f + 0.4f * env().fog;
    for (const auto& b : birds_) mark(b.x, b.y, (0.5f + 0.6f * b.bank) * strength);
    for (const auto& f : fliers_)
        if (!f.falcon && f.span <= 1.5f)
            mark(f.x, f.y, 0.9f);

    const auto& L = env();
    const Rgb bird = mix(scale(from_env(L.zenith), 0.2f), Rgb{0.015f, 0.012f, 0.02f}, 0.7f);
    const int shoreRow = std::clamp(static_cast<int>(std::lround(ShoreY * sy)), 1, H);
    // Below the shore, birds only show over open water: the reeds hide birds that drop into them.
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const float ink = ink_[static_cast<size_t>(y * W + x)];
            if (ink > 0.004f && (y < shoreRow || water_[static_cast<size_t>(y * W + x)]))
                cv_.blend(x, y, bird, 0.9f * (1.0f - std::exp(-1.2f * ink)));
        }
    // Birds flying low show faintly in the water, rippled (not while they drop into the reeds).
    for (int y = shoreRow; y < H && phase_ != Phase::Roost; ++y) {
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
                cv_.blend(x, y, bird, 0.3f * (1.0f - std::exp(-1.2f * ink)));
        }
    }
    for (const auto& f : fliers_)
        if (f.falcon || f.span > 1.5f)
            draw_flier(f, bird);
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

    update_phase(dt);
    simCarry_ += dt;
    int steps = 0;
    while (simCarry_ >= SimStep && steps < 4) {
        simulate(SimStep);
        simCarry_ -= SimStep;
        ++steps;
    }
    if (steps == 4)
        simCarry_ = 0.0f;
    update_fliers(dt);
    draw_birds();

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
                                    {{"birds", 1100}},
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
