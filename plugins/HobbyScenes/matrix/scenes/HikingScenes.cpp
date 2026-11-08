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

bool RidgeTrailScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt, rdt;
    begin_audio_frame(dt, rdt);
    const float W = static_cast<float>(cv_.w), H = static_cast<float>(cv_.h), k = W / 128.0f;
    const float am = std::min(1.0f, audio_.amount);
    const float s = 1.2f * k;  // hiker scale

    const float cycles = walk_cycles_per_second(audio_, 0.62f);
    walk_ += dt * cycles;
    // One walk cycle covers two steps of ~6.5 units each.
    scroll_ += dt * cycles * 13.0f * s;

    // Sky, sun and clouds.
    fill_gradient({0.22f, 0.32f, 0.58f}, {0.96f, 0.62f, 0.40f}, {1.0f, 0.82f, 0.52f}, 0.55f);
    const float sunX = W * 0.74f, sunY = H * 0.42f;
    cv_.glow(sunX, sunY, 34 * k, {1.0f, 0.62f, 0.30f}, 0.30f + 0.45f * audio_.beat * am + 0.25f * audio_.drop * am);
    cv_.ellipse(sunX, sunY, (8.5f + 1.2f * audio_.bass * am) * k, (8.5f + 1.2f * audio_.bass * am) * k, 0, {1.0f, 0.92f, 0.66f});
    for (int i = 0; i < 4; ++i) {
        const float period = W + 70 * k;
        const float cx = fract((i * 0.27f * period - scroll_ * 0.03f - t_ * 0.8f * k) / period) * period - 35 * k;
        const float cy = H * (0.10f + 0.07f * i);
        cv_.ellipse(cx, cy, (20 + 4 * i) * k, 2.4f * k, 0, {1.0f, 0.78f, 0.66f}, 0.45f);
        cv_.ellipse(cx + 6 * k, cy - 1.6f * k, 9 * k, 2.2f * k, 0, {1.0f, 0.85f, 0.72f}, 0.35f);
    }

    // Birds (periodic, and on drops).
    nextFlock_ -= dt;
    if (nextFlock_ <= 0.0f || (audio_.dropHit && birds_.size() < 10)) {
        spawn_flock(W, H, k);
        nextFlock_ = 25.0f + rnd() * 20.0f;
    }
    for (auto& b : birds_) {
        b.x -= b.speed * dt;
        b.phase += dt * (1.6f + 1.5f * audio_.level * am);
        bird(cv_, b.x, b.y + std::sin(b.phase * 1.3f) * 0.8f * k, k, 0.5f + 0.5f * std::sin(b.phase * kTau), {0.25f, 0.16f, 0.22f});
    }
    std::erase_if(birds_, [&](const Bird& b) { return b.x < -12 * k; });

    // Far and mid ridges; pines stand on the mid ridge.
    const Rgb farCol = mix({0.62f, 0.46f, 0.58f}, {0.78f, 0.50f, 0.52f}, audio_.level * am);
    hills(cv_, scroll_ * 0.08f, H * 0.64f, 30 * k, 1.3f, farCol);
    const float midScroll = scroll_ * 0.28f, midBase = H * 0.76f, midAmp = 28 * k;
    hills(cv_, midScroll, midBase, midAmp, 3.7f, {0.36f, 0.36f, 0.42f});
    {
        const float spacing = 7.0f * k;
        const int first = static_cast<int>(std::floor((midScroll - 8 * k) / spacing));
        for (int i = first; i * spacing < midScroll + W + 8 * k; ++i) {
            const float h = hash01(static_cast<uint32_t>(i) * 2654435761u);
            if (h < 0.45f)
                continue;
            const float wx = i * spacing + h * 3 * k;
            const float x = wx - midScroll;
            const float base = midBase - hill(wx, 3.7f) * midAmp + 2 * k;
            pine(cv_, x, base, (6 + 6 * h) * k, {0.20f, 0.24f, 0.28f});
        }
    }

    // Foreground trail ridge.
    const float nearScroll = scroll_;
    auto groundAt = [&](float x) { return H * 0.86f - hill((x + nearScroll) * 0.55f, 7.0f) * 15 * k; };
    for (int x = 0; x < cv_.w; ++x) {
        const float top = groundAt(x + 0.5f);
        const int y0 = std::max(0, static_cast<int>(std::floor(top)));
        for (int y = y0; y < cv_.h; ++y) {
            const float d = (y + 0.5f) - top;  // depth below the surface
            Rgb c;
            if (d < 1.2f)
                c = {0.52f, 0.58f, 0.26f};  // sunlit grass edge
            else if (d > 2.0f * k && d < 4.6f * k)
                c = {0.66f, 0.50f, 0.34f};  // dirt path
            else
                c = mix({0.34f, 0.46f, 0.20f}, {0.16f, 0.24f, 0.12f}, d / (H * 0.3f));
            cv_.blend(x, y, c, y == y0 ? 1.0f - (top - std::floor(top)) : 1.0f);
        }
    }
    // Wildflowers below the path, glowing with the treble.
    {
        static constexpr Rgb kFlower[] = {{1.0f, 0.85f, 0.30f}, {0.95f, 0.45f, 0.65f}, {0.70f, 0.55f, 1.0f}, {1.0f, 0.96f, 0.92f}};
        const float spacing = 3.0f * k;
        const int first = static_cast<int>(std::floor(nearScroll / spacing)) - 1;
        const float bloom = 0.65f + 0.9f * audio_.treble * am + 0.4f * audio_.onset * am;
        for (int i = first; i * spacing < nearScroll + W + spacing; ++i) {
            const uint32_t id = static_cast<uint32_t>(i) * 2246822519u;
            if (hash01(id) < 0.45f)
                continue;
            const float x = i * spacing - nearScroll + hash01(id + 1) * spacing;
            const float y = groundAt(x) + (5.5f + hash01(id + 2) * 12.0f) * k;
            const Rgb c = kFlower[static_cast<int>(hash01(id + 3) * 3.99f)];
            cv_.add(static_cast<int>(x), static_cast<int>(y), c, 0.55f * bloom);
            cv_.add(static_cast<int>(x), static_cast<int>(y) - 1, {0.30f, 0.55f, 0.20f}, 0.25f);
        }
    }

    // The hiker, leaning into climbs, with a trekking pole.
    const float hx = W * 0.36f;
    const float slope = (groundAt(hx + 3 * k) - groundAt(hx - 3 * k)) / (6 * k);
    const float lean = std::clamp(0.06f - slope * 0.7f, -0.12f, 0.35f);
    Look look;
    look.backpack = true;
    look.cap = true;
    look.shirt = {0.78f, 0.26f, 0.18f};
    look.pants = {0.26f, 0.28f, 0.22f};
    look.capColor = {0.20f, 0.36f, 0.50f};
    look.pack = {0.22f, 0.40f, 0.30f};
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
                {0.70f, 0.72f, 0.78f});

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

bool SummitNightScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt, rdt;
    begin_audio_frame(dt, rdt);
    const float W = static_cast<float>(cv_.w), H = static_cast<float>(cv_.h), k = W / 128.0f;
    const float am = std::min(1.0f, audio_.amount);

    fill_gradient({0.01f, 0.02f, 0.07f}, {0.04f, 0.06f, 0.17f}, {0.10f, 0.10f, 0.24f}, 0.62f);

    // Aurora curtain: faint without music, follows the mids while it plays.
    const float aurora = 0.10f + 0.55f * audio_.mid * am + 0.35f * audio_.drop * am;
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
        cv_.add(static_cast<int>(x), static_cast<int>(st.y), {0.80f, 0.86f, 1.0f}, tw * st.size + sparkle);
    }

    // Shooting stars on a timer, and on drops.
    nextShoot_ -= dt;
    if ((nextShoot_ <= 0.0f || audio_.dropHit) && shootLife_ <= 0.0f) {
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

    // Valley: layered ridges fading into mist, village lights in the bottom.
    hills(cv_, 20.0f, H * 0.66f, 22 * k, 1.9f, {0.09f, 0.11f, 0.24f});
    hills(cv_, 140.0f, H * 0.74f, 18 * k, 4.4f, {0.07f, 0.08f, 0.19f});
    for (int py = static_cast<int>(H * 0.70f); py < static_cast<int>(H * 0.82f); ++py)
        for (int x = 0; x < cv_.w; ++x) {
            const float n = 0.5f + 0.5f * std::sin(x * 0.06f + t_ * 0.15f) * std::sin(x * 0.021f - t_ * 0.07f + py * 0.2f);
            const float fall = 1.0f - std::fabs(py - H * 0.76f) / (H * 0.06f);
            cv_.blend(x, py, {0.20f, 0.22f, 0.36f}, 0.35f * n * std::max(0.0f, fall));
        }
    hills(cv_, 60.0f, H * 0.84f, 8 * k, 6.2f, {0.04f, 0.05f, 0.12f});
    for (auto& l : village_) {
        const float b = 0.45f + 0.20f * std::sin(t_ * 0.7f + l.phase) + 0.55f * audio_.treble * am * (0.5f + 0.5f * std::sin(t_ * 9 + l.phase * 5));
        const Rgb c = mix({1.0f, 0.70f, 0.35f}, {1.0f, 0.92f, 0.70f}, l.hue);
        cv_.add(static_cast<int>(l.x), static_cast<int>(l.y), c, b);
        cv_.glow(l.x + 0.5f, l.y + 0.5f, 2.5f * k, c, 0.12f * b);
    }

    // Summit rock in the foreground.
    auto rockTop = [&](float x) { return H * 0.80f + H * 0.32f * smoothstep(W * 0.38f, W * 0.98f, x) - 1.5f * k * std::sin(x * 0.21f); };
    cv_.fill_below(rockTop, {0.05f, 0.05f, 0.09f});

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
    look.shirt = {0.42f, 0.22f, 0.16f};
    look.pants = {0.14f, 0.15f, 0.22f};
    look.shoes = {0.04f, 0.04f, 0.05f};
    look.capColor = {0.20f, 0.10f, 0.08f};
    cv_.ellipse(sx - 1.2f * s, ground - 2.0f * s, 3.4f * s, 2.6f * s, 0.0f, {0.07f, 0.07f, 0.11f});
    figure(cv_, sx, ground, s, 1.0f, sit, look);
    // Warm rim light on the side facing the tent.
    cv_.glow(sx - 3 * s, ground - 8 * s, 7 * s, {1.0f, 0.55f, 0.25f}, 0.10f * lantern);

    present(canvas);
    wait_until_next_frame();
    return true;
}

Scenes::SceneDescriptor SummitNightScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.family = "hiking";
    d.tags = {"hiking", "hills", "night", "stars", "camping", "aurora", "calm", "audio-reactive"};
    d.intensity = 0.15f;
    d.motion = 0.15f;
    d.music_affinity = 0.4f;
    d.performance_cost = 0.3f;
    return d;
}

// ================================================================ cloud shadows
float CloudShadowsScene::hill_top(float x) const
{
    const float W = static_cast<float>(cv_.w), H = static_cast<float>(cv_.h);
    const float u = x / W;
    const float main = 0.62f * std::exp(-std::pow((u - 0.60f) / 0.30f, 2.0f));
    const float side = 0.30f * std::exp(-std::pow((u + 0.04f) / 0.26f, 2.0f));
    // Smooth union so the two hills meet in a saddle, not a crease.
    const float joined = 0.04f * std::log(std::exp(main / 0.04f) + std::exp(side / 0.04f));
    return H * (0.94f - joined - 0.010f * std::sin(u * 31.0f));
}

Point CloudShadowsScene::trail_at(float s) const
{
    if (trail_.empty())
        return {0, 0};
    if (s <= 0.0f)
        return trail_.front();
    for (size_t i = 1; i < trail_.size(); ++i)
        if (s <= trailLen_[i]) {
            const float t = (s - trailLen_[i - 1]) / std::max(1e-3f, trailLen_[i] - trailLen_[i - 1]);
            return {trail_[i - 1].x + (trail_[i].x - trail_[i - 1].x) * t, trail_[i - 1].y + (trail_[i].y - trail_[i - 1].y) * t};
        }
    return trail_.back();
}

void CloudShadowsScene::build_world()
{
    const int w = cv_.w, h = cv_.h;
    const float W = static_cast<float>(w), H = static_cast<float>(h), k = W / 128.0f;

    // Switchbacks up the main hill, then along the ridge off the right edge.
    trail_.clear();
    const float peakX = W * 0.60f;
    const int turns = 6;
    trail_.push_back({W * 0.36f, H + 6 * k});
    for (int i = 0; i <= turns; ++i) {
        const float v = static_cast<float>(i) / turns;
        const float cx = W * 0.40f + (peakX - W * 0.40f) * v;
        const float half = W * (0.15f - 0.12f * v);
        const float x = cx + (i % 2 == 0 ? -half : half);
        const float y = H * 0.95f + (hill_top(peakX) + 4 * k - H * 0.95f) * v;
        trail_.push_back({x, std::max(y, hill_top(x) + 2 * k)});
    }
    trail_.push_back({peakX, hill_top(peakX) + 1 * k});
    summitS_ = 0.0f;
    const size_t summitIndex = trail_.size() - 1;
    for (float x = peakX + 6 * k; x < W + 10 * k; x += 6 * k) trail_.push_back({x, hill_top(x) + 1.5f * k});
    trailLen_.assign(trail_.size(), 0.0f);
    for (size_t i = 1; i < trail_.size(); ++i)
        trailLen_[i] = trailLen_[i - 1] + std::sqrt(std::pow(trail_[i].x - trail_[i - 1].x, 2.0f) + std::pow(trail_[i].y - trail_[i - 1].y, 2.0f));
    summitS_ = trailLen_[summitIndex];

    // Static landscape: sky, distant range, lit hill with grass texture, trail.
    Canvas bg;
    bg.resize(w, h);
    bg.fill_gradient({0.30f, 0.55f, 0.88f}, {0.62f, 0.80f, 0.96f}, {0.80f, 0.90f, 0.98f}, 0.45f);
    hills(bg, 30.0f, H * 0.62f, 30 * k, 2.6f, {0.56f, 0.66f, 0.80f});
    hills(bg, 200.0f, H * 0.70f, 20 * k, 5.1f, {0.46f, 0.58f, 0.66f});
    isLand_.assign(static_cast<size_t>(w * h), 0);
    for (int x = 0; x < w; ++x) {
        const float top = hill_top(x + 0.5f);
        // Wide, smoothed slope: no crease at the peak or in the saddle.
        const float span = 10.0f * k;
        const float slope = (hill_top(x + 0.5f + span) - hill_top(x + 0.5f - span)) / (2.0f * span);
        for (int y = std::max(0, static_cast<int>(std::floor(top))); y < h; ++y) {
            const float depth = (y + 0.5f - top) / std::max(1.0f, H - top);
            // Sun from the right: slope shading fades out down the hillside.
            const float light = 1.0f + 0.22f * std::tanh(slope * 2.5f) * std::exp(-(y + 0.5f - top) / (H * 0.3f));
            const float n = hash01(static_cast<uint32_t>(y * w + x) * 747796405u);
            Rgb c = mix({0.46f, 0.72f, 0.28f}, {0.24f, 0.48f, 0.18f}, depth * 0.8f + 0.12f * n);
            c = scale(c, light * (0.94f + 0.08f * n));
            const float a = y == static_cast<int>(std::floor(top)) ? 1.0f - (top - std::floor(top)) : 1.0f;
            bg.blend(x, y, c, a);
            isLand_[static_cast<size_t>(y * w + x)] = 1;
        }
    }
    // Dry-stone wall across the lower meadow.
    for (int x = 0; x < w; ++x) {
        const float y = H * 0.88f + 3 * k * std::sin(x * 0.04f + 1.0f);
        if (y > hill_top(x + 0.5f) + 2)
            bg.capsule(x, y, 0.7f * k, x + 1.0f, y, 0.7f * k, {0.62f, 0.62f, 0.56f});
    }
    for (size_t i = 1; i < trail_.size(); ++i)
        bg.capsule(trail_[i - 1].x, trail_[i - 1].y, 0.75f * k, trail_[i].x, trail_[i].y, 0.75f * k, {0.80f, 0.72f, 0.55f});
    // Summit cairn.
    const Point top = trail_[summitIndex];
    bg.ellipse(top.x + 3 * k, top.y - 1.0f * k, 2.4f * k, 1.4f * k, 0, {0.55f, 0.55f, 0.52f});
    bg.ellipse(top.x + 3 * k, top.y - 2.8f * k, 1.6f * k, 1.1f * k, 0, {0.62f, 0.62f, 0.58f});
    bg.ellipse(top.x + 3 * k, top.y - 4.2f * k, 1.0f * k, 0.8f * k, 0, {0.70f, 0.70f, 0.66f});
    ground_ = std::move(bg.px);

    walkers_.clear();
    static constexpr Rgb kJackets[] = {{0.92f, 0.30f, 0.20f}, {0.98f, 0.78f, 0.18f}, {0.25f, 0.45f, 0.90f}, {0.85f, 0.35f, 0.70f}};
    const float total = trailLen_.back();
    for (int i = 0; i < 4; ++i) walkers_.push_back({total * (0.08f + 0.24f * i), (5.0f + rnd() * 1.5f) * k, rnd(), 0.0f, kJackets[i]});

    sheep_.clear();
    for (int i = 0; i < 6; ++i) {
        const float x = W * (0.04f + 0.30f * rnd());
        sheep_.push_back({x, 0.0f, rnd() < 0.5f ? -1.0f : 1.0f, rnd() * 10.0f, 0.0f});
        sheep_.back().y = hill_top(x) + (6 + rnd() * (H - hill_top(x) - 12)) ;
    }
}

void CloudShadowsScene::initialize(int width, int height)
{
    AudioHobbyScene::initialize(width, height);
    build_world();
    drift_ = 0.0f;
}

bool CloudShadowsScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt, rdt;
    begin_audio_frame(dt, rdt);
    const float W = static_cast<float>(cv_.w), H = static_cast<float>(cv_.h), k = W / 128.0f;
    const float am = std::min(1.0f, audio_.amount);
    cv_.px = ground_;

    // Wind: clouds and their shadows travel together, faster with the music.
    drift_ += dt * (3.0f + 9.0f * audio_.level * am + 6.0f * audio_.drop * am) * k;
    const float period = W + 80 * k;
    struct Cloud {
        float x, y, rx, ry, shadowY;
    };
    Cloud clouds[4];
    for (int i = 0; i < 4; ++i) {
        const float x = fract((i * 0.29f * period + drift_ * (0.9f + 0.1f * i)) / period) * period - 40 * k;
        clouds[i] = {x, H * (0.07f + 0.045f * (i % 3)), (13 + 3 * (i % 2)) * k, 3.2f * k, H * (0.48f + 0.13f * i)};
    }
    for (const auto& c : clouds) {
        cv_.ellipse(c.x, c.y, c.rx, c.ry, 0, {0.98f, 0.99f, 1.0f}, 0.85f);
        cv_.ellipse(c.x - 4 * k, c.y - 2.0f * k, c.rx * 0.5f, c.ry * 0.9f, 0, {1.0f, 1.0f, 1.0f}, 0.85f);
        cv_.ellipse(c.x + 5 * k, c.y - 1.2f * k, c.rx * 0.45f, c.ry * 0.8f, 0, {0.94f, 0.96f, 1.0f}, 0.85f);
    }
    // Shadows: soft ellipses offset down-wind, only on the land.
    for (const auto& c : clouds) {
        const float sx = c.x + 8 * k, rx = c.rx * 1.7f, ry = c.ry * 2.6f;
        for (int y = std::max(0, static_cast<int>(c.shadowY - ry)); y <= std::min(cv_.h - 1, static_cast<int>(c.shadowY + ry)); ++y)
            for (int x = std::max(0, static_cast<int>(sx - rx)); x <= std::min(cv_.w - 1, static_cast<int>(sx + rx)); ++x) {
                if (!isLand_[static_cast<size_t>(y * cv_.w + x)])
                    continue;
                const float u = (x + 0.5f - sx) / rx, v = (y + 0.5f - c.shadowY) / ry;
                const float d = u * u + v * v;
                if (d >= 1.0f)
                    continue;
                const float shade = 1.0f - 0.36f * smoothstep(1.0f, 0.45f, d);
                float* p = cv_.at(x, y);
                p[0] *= shade;
                p[1] *= shade * 1.02f;
                p[2] *= shade * 1.06f;
            }
    }

    // Sheep wander the lower meadow and hop on beats.
    for (auto& sh : sheep_) {
        sh.phase += dt;
        if (std::sin(sh.phase * 0.4f) > 0.3f)
            sh.x += sh.dir * dt * 1.2f * k;
        if (sh.x < 3 * k)
            sh.dir = 1.0f;
        else if (sh.x > W * 0.40f)
            sh.dir = -1.0f;
        else if (rnd() < rdt * 0.05f)
            sh.dir = -sh.dir;
        sh.y = std::max(sh.y, hill_top(sh.x) + 3 * k);
        if (audio_.beatHit && am > 0.3f && rnd() < 0.5f)
            sh.hop = 1.0f;
        sh.hop = std::max(0.0f, sh.hop - rdt * 4.0f);
        const float lift = std::sin(sh.hop * kPi) * 2.0f * k;
        cv_.ellipse(sh.x + 0.5f * k, sh.y + 0.8f * k, 2.2f * k, 0.6f * k, 0, {0.20f, 0.34f, 0.14f}, 0.5f);
        cv_.ellipse(sh.x, sh.y - lift, 2.0f * k, 1.35f * k, 0, {0.96f, 0.96f, 0.92f});
        cv_.ellipse(sh.x + sh.dir * 2.0f * k, sh.y - lift - 0.4f * k, 0.8f * k, 0.7f * k, 0, {0.12f, 0.10f, 0.10f});
    }

    // Hikers climb the switchbacks, pause at the cairn, then go over the ridge.
    const float total = trailLen_.back();
    for (auto& wk : walkers_) {
        const float before = wk.s;
        if (wk.rest > 0.0f)
            wk.rest -= dt;
        else
            wk.s += dt * wk.speed * (1.0f + 0.3f * audio_.level * am);
        if (before < summitS_ && wk.s >= summitS_) {
            wk.s = summitS_;
            wk.rest = 3.5f;
        }
        if (wk.s > total) {
            wk.s = 0.0f;
            wk.speed = (5.0f + rnd() * 1.5f) * k;
        }
        const Point p = trail_at(wk.s), q = trail_at(wk.s + 1.0f);
        const float facing = q.x >= p.x ? 1.0f : -1.0f;
        wk.phase += dt * (wk.rest > 0.0f ? 0.0f : 0.9f);
        Look look;
        look.backpack = true;
        look.shirt = wk.jacket;
        look.pack = {0.30f, 0.30f, 0.34f};
        Pose pose = walk_pose(fract(wk.phase), 0.45f, 0.12f);
        if (wk.rest > 0.0f) {
            Pose cheer;
            cheer.upperArm[0] = cheer.upperArm[1] = 2.7f;
            cheer.foreArm[0] = cheer.foreArm[1] = 3.0f + 0.2f * std::sin(t_ * 8);
            pose = cheer;
        }
        figure(cv_, p.x, p.y + 0.5f * k, 0.42f * k, facing, pose, look);
    }

    present(canvas);
    wait_until_next_frame();
    return true;
}

Scenes::SceneDescriptor CloudShadowsScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.family = "hiking";
    d.tags = {"hiking", "hills", "scenery", "landscape", "day", "sheep", "calm", "audio-reactive"};
    d.intensity = 0.2f;
    d.motion = 0.25f;
    d.music_affinity = 0.3f;
    d.performance_cost = 0.25f;
    return d;
}
