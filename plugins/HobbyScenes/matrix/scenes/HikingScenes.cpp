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
    const float s = 1.45f * k;  // hiker scale

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
    // Lantern light spilling out of the tent door onto the rock.
    cv_.ellipse(tentX + 4 * k, tentBase + 1.5f * k, 12 * k, 2.2f * k, 0, scale({1.0f, 0.55f, 0.22f}, lantern), 0.22f);
    const FigureOut fig = figure(cv_, sx, ground, s, 1.0f, sit, look);
    // Warm rim light on the side facing the tent.
    cv_.glow(sx - 3 * s, ground - 8 * s, 7 * s, {1.0f, 0.55f, 0.25f}, 0.16f * lantern);
    // A mug held in both hands, steam curling up into the cold air.
    const Point mug{(fig.hand[0].x + fig.hand[1].x) * 0.5f + 0.6f * s, (fig.hand[0].y + fig.hand[1].y) * 0.5f - 0.6f * s};
    cv_.ellipse(mug.x, mug.y, 0.9f * s, 1.1f * s, 0, {0.78f, 0.30f, 0.22f});
    for (int i = 0; i < 3; ++i) {
        const float u = fract(t_ * 0.35f + i / 3.0f);
        const float sway = std::sin(t_ * 1.3f + i * 2.0f + u * 4.0f) * 1.2f * s * u;
        cv_.glow(mug.x + sway, mug.y - (1.5f + 7.0f * u) * s, (0.8f + 1.2f * u) * s, {0.75f, 0.78f, 0.85f}, 0.30f * (1.0f - u));
    }

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

// ================================================================ alpine lake
void AlpineLakeScene::build_backdrop()
{
    const int w = cv_.w, h = cv_.h;
    const float W = static_cast<float>(w), H = static_cast<float>(h), k = W / 128.0f;
    horizon_ = std::floor(H * 0.52f);
    Canvas bg;
    bg.resize(w, h);
    bg.fill_gradient({0.30f, 0.50f, 0.82f}, {0.70f, 0.79f, 0.93f}, {0.98f, 0.86f, 0.74f}, 0.32f);
    const float sunX = W * 0.24f, sunY = H * 0.17f;
    bg.glow(sunX, sunY, 42 * k, {1.0f, 0.86f, 0.62f}, 0.40f);
    bg.ellipse(sunX, sunY, 6 * k, 6 * k, 0, {1.0f, 0.97f, 0.86f});

    // Far snowy range, lit from the left (the sun's side).
    auto farTop = [&](float x) {
        const float u = x / W;
        return horizon_ - H * (0.10f + 0.25f * std::pow(1.0f - std::fabs(std::sin(u * 3.3f + 0.4f)), 1.6f) +
                               0.07f * std::pow(1.0f - std::fabs(std::sin(u * 8.1f + 1.3f)), 2.0f));
    };
    for (int x = 0; x < w; ++x) {
        const float fx = x + 0.5f, top = farTop(fx);
        const float slope = farTop(fx + 1.5f) - farTop(fx - 1.5f);
        const float shade = 1.0f - 0.28f * smoothstep(-0.3f, 0.9f, slope);
        // Snow reaches further down in the gullies (where the ridge is lower).
        const float snowLine = H * 0.20f + 1.6f * k * std::sin(x * 0.21f + 0.5f) + 0.8f * k * std::sin(x * 0.53f) + 0.25f * (top - farTop(fx - 4.0f * k));
        for (int y = std::max(0, static_cast<int>(std::floor(top))); y < static_cast<int>(horizon_); ++y) {
            const float hgt = horizon_ - (y + 0.5f);
            Rgb c = hgt > snowLine ? Rgb{0.96f, 0.97f, 1.0f} : mix({0.40f, 0.45f, 0.58f}, {0.48f, 0.52f, 0.64f}, hash01(static_cast<uint32_t>(y * w + x)));
            c = scale(c, shade);
            c = mix(c, {0.72f, 0.78f, 0.90f}, 0.5f * (1.0f - std::min(1.0f, hgt / (H * 0.3f))));
            bg.blend(x, y, c, y == static_cast<int>(std::floor(top)) ? 1.0f - (top - std::floor(top)) : 1.0f);
        }
    }
    // Forested foothills with a waterfall.
    auto nearTop = [&](float x) {
        const float u = x / W;
        return horizon_ - H * (0.05f + 0.10f * std::pow(1.0f - std::fabs(std::sin(u * 5.2f + 2.2f)), 1.3f) + 0.012f * std::sin(u * 40.0f));
    };
    bg.fill_below(
        [&](float x) { return nearTop(x); }, {0.20f, 0.34f, 0.30f});
    for (int y = static_cast<int>(horizon_ - H * 0.16f); y < static_cast<int>(horizon_); ++y)
        for (int x = 0; x < w; ++x)
            if (y >= nearTop(x + 0.5f))
                bg.blend(x, y, {0.62f, 0.72f, 0.82f}, 0.35f * (1.0f - (horizon_ - y) / (H * 0.16f)));
    for (float x = 1.5f * k; x < W; x += 3.0f * k) {
        const uint32_t id = static_cast<uint32_t>(x * 97.0f);
        pine(bg, x + hash01(id) * k, nearTop(x) + 2.0f * k, (3.5f + 3.0f * hash01(id + 1)) * k, {0.13f, 0.25f, 0.21f});
    }
    for (float x = 0.8f * k; x < W; x += 2.2f * k) {
        const uint32_t id = static_cast<uint32_t>(x * 131.0f + 7.0f);
        pine(bg, x + hash01(id) * k, horizon_ + 0.5f, (2.5f + 3.5f * hash01(id + 1)) * k, {0.07f, 0.15f, 0.12f});
    }
    backdrop_ = std::move(bg.px);
}

void AlpineLakeScene::initialize(int width, int height)
{
    AudioHobbyScene::initialize(width, height);
    build_backdrop();
    ripples_.clear();
    stone_ = {};
    throwT_ = -1.0f;
    nextThrow_ = 10.0f + rnd() * 8.0f;
    nextFish_ = 4.0f;
}

void AlpineLakeScene::start_throw()
{
    if (throwT_ < 0.0f && stone_.t < 0.0f)
        throwT_ = 0.0f;
}

bool AlpineLakeScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt, rdt;
    begin_audio_frame(dt, rdt);
    const float W = static_cast<float>(cv_.w), H = static_cast<float>(cv_.h), k = W / 128.0f;
    const float am = std::min(1.0f, audio_.amount);
    cv_.px = backdrop_;
    const int hz = static_cast<int>(horizon_);
    auto shore = [&](float x) { return H * 0.84f + 2.5f * k * std::sin(x * 0.045f + 0.6f) - 3.0f * k * smoothstep(W * 0.55f, W, x); };

    // Thin clouds drifting past the peaks.
    for (int i = 0; i < 3; ++i) {
        const float period = W + 60 * k;
        const float cx = fract((i * 0.37f * period + t_ * (1.2f + 0.3f * i) * k) / period) * period - 30 * k;
        cv_.ellipse(cx, H * (0.12f + 0.08f * i), (16 + 5 * i) * k, 1.6f * k, 0, {1.0f, 0.97f, 0.95f}, 0.45f);
    }
    // Waterfall on the foothills.
    const float wfX = W * 0.70f;
    for (int y = static_cast<int>(horizon_ - H * 0.09f); y < hz; ++y) {
        const float b = 0.45f + 0.35f * std::sin(y * 1.4f - t_ * 9.0f);
        cv_.add(static_cast<int>(wfX), y, {0.85f, 0.92f, 1.0f}, b);
        cv_.add(static_cast<int>(wfX) + 1, y, {0.85f, 0.92f, 1.0f}, b * 0.4f);
    }

    // Ripples expand and fade.
    for (auto& r : ripples_) {
        r.r += rdt * 7.0f * k;
        r.life -= rdt * 0.40f;
    }
    std::erase_if(ripples_, [](const Ripple& r) { return r.life <= 0.0f; });
    nextFish_ -= dt;
    const bool fishBeat = audio_.beatHit && am > 0.3f && rnd() < 0.3f;
    if ((nextFish_ <= 0.0f || fishBeat) && ripples_.size() < 10) {
        ripples_.push_back({W * (0.08f + 0.80f * rnd()), horizon_ + (3 + rnd() * (H * 0.84f - horizon_ - 10)) , 0.5f, 1.0f});
        nextFish_ = 6.0f + rnd() * 8.0f;
    }

    // The lake mirrors everything above the waterline.
    const float sunX = W * 0.24f;
    const float shimmer = 1.0f + 1.5f * audio_.treble * am;
    const uint32_t glintFrame = static_cast<uint32_t>(t_ * 6.0f);
    for (int y = hz; y < cv_.h; ++y) {
        const float d = (y - horizon_) / std::max(1.0f, H - horizon_);
        const float off = (std::sin(y * 0.9f - t_ * 1.6f) * (0.25f + 1.4f * d) * shimmer + 0.4f * std::sin(y * 0.31f + t_ * 0.7f)) * k;
        const int sy = std::max(0, 2 * hz - y - 1);
        for (int x = 0; x < cv_.w; ++x) {
            float ox = off, lift = 0.0f;
            for (const auto& r : ripples_) {
                const float dx = x + 0.5f - r.x, dy = (y + 0.5f - r.y) / 0.32f;
                const float band = std::fabs(std::sqrt(dx * dx + dy * dy) - r.r);
                if (band < 1.4f * k) {
                    const float a = (1.0f - band / (1.4f * k)) * r.life;
                    ox += 1.5f * k * a;
                    lift += 0.22f * a;
                }
            }
            const int sx = std::clamp(static_cast<int>(x + ox), 0, cv_.w - 1);
            const float* src = cv_.at(sx, sy);
            Rgb c = mix({src[0], src[1], src[2]}, {0.16f, 0.30f, 0.44f}, 0.28f + 0.40f * d);
            c = scale(c, 0.92f - 0.25f * d);
            float* p = cv_.at(x, y);
            p[0] = c.r + lift;
            p[1] = c.g + lift;
            p[2] = c.b + lift;
            // Sun path on the water: soft warm column with sparse glints.
            const float sunDx = std::fabs(x + 0.5f - sunX) / ((2.5f + 6.0f * d) * k);
            if (sunDx < 1.0f) {
                cv_.add(x, y, {1.0f, 0.90f, 0.70f}, 0.12f * (1.0f - sunDx));
                if ((y & 1) == 0 && hash01(static_cast<uint32_t>(y * 7919 + (x >> 1) * 131) + glintFrame * 2654435761u) > 0.93f - 0.08f * audio_.treble * am)
                    cv_.add(x, y, {1.0f, 0.96f, 0.86f}, 0.35f * (1.0f - sunDx));
            }
        }
    }
    // Morning mist on the water.
    for (int py = hz - static_cast<int>(2 * k); py < hz + static_cast<int>(8 * k); ++py)
        for (int x = 0; x < cv_.w; ++x) {
            const float n = 0.5f + 0.5f * std::sin(x * 0.06f + t_ * 0.25f) * std::sin(x * 0.023f - t_ * 0.11f + py * 0.15f);
            const float fall = 1.0f - std::fabs(py - (horizon_ + 2 * k)) / (6 * k);
            cv_.blend(x, py, {0.92f, 0.94f, 0.97f}, 0.40f * n * std::max(0.0f, fall));
        }

    // Foreground shore, boulder, pack and the resting hiker.
    for (int x = 0; x < cv_.w; ++x) {
        const float top = shore(x + 0.5f);
        const int y0 = std::max(0, static_cast<int>(std::floor(top)));
        for (int y = y0; y < cv_.h; ++y) {
            const float dd = (y + 0.5f - top) / (H * 0.18f);
            Rgb c = y - top < 1.2f ? Rgb{0.55f, 0.62f, 0.34f} : mix({0.36f, 0.42f, 0.24f}, {0.18f, 0.22f, 0.14f}, dd);
            const float n = hash01(static_cast<uint32_t>(y * 4099 + x * 37));
            if (n > 0.985f)
                c = mix(c, {0.62f, 0.60f, 0.56f}, 0.6f);  // a few pebbles
            else if (n > 0.97f)
                c = mix(c, {0.95f, 0.85f, 0.40f}, 0.45f);  // and wildflowers
            else
                c = scale(c, 0.93f + 0.12f * n);
            cv_.blend(x, y, c, y == y0 ? 1.0f - (top - std::floor(top)) : 1.0f);
        }
    }
    const float bx = W * 0.79f, bTop = shore(bx) - 6.0f * k;
    cv_.ellipse(bx, bTop + 5.0f * k, 10 * k, 6 * k, 0, {0.40f, 0.40f, 0.44f});
    cv_.ellipse(bx - 2.5f * k, bTop + 3.0f * k, 6 * k, 3 * k, -0.2f, {0.55f, 0.55f, 0.58f});
    cv_.ellipse(W * 0.93f, shore(W * 0.93f) - 3.0f * k, 2.6f * k, 3.6f * k, 0.15f, {0.80f, 0.42f, 0.16f});
    cv_.ellipse(W * 0.93f, shore(W * 0.93f) - 5.5f * k, 2.2f * k, 1.0f * k, 0.15f, {0.62f, 0.30f, 0.12f});
    cv_.capsule(W * 0.965f, shore(W * 0.965f) + 1, 0.3f * k, W * 0.90f, shore(W * 0.9f) - 14 * k, 0.3f * k, {0.72f, 0.74f, 0.80f});

    // Skipping a stone: wind up, throw, then the stone hops away across the lake.
    nextThrow_ -= dt;
    if (nextThrow_ <= 0.0f || (audio_.dropHit && am > 0.2f)) {
        start_throw();
        nextThrow_ = 18.0f + rnd() * 14.0f;
    }
    const float s = 1.45f * k;
    Pose sit;
    sit.lean = 0.10f;
    sit.thigh[0] = sit.thigh[1] = 1.40f;
    sit.shin[0] = 0.30f;
    sit.shin[1] = 0.15f;
    sit.upperArm[0] = 0.55f;
    sit.foreArm[0] = 1.1f;   // hand resting on the knee
    sit.upperArm[1] = 0.75f;
    sit.foreArm[1] = 1.25f;
    bool release = false;
    if (throwT_ >= 0.0f) {
        const float before = throwT_;
        throwT_ += dt;
        const float tt = throwT_;
        if (tt < 0.45f) {
            const float e = smoothstep(0.0f, 0.45f, tt);
            sit.upperArm[1] = 0.75f + (-1.6f - 0.75f) * e;
            sit.foreArm[1] = 1.25f + (-0.9f - 1.25f) * e;
            sit.lean = 0.10f - 0.12f * e;
        }
        else {
            const float e = smoothstep(0.45f, 0.62f, tt);
            sit.upperArm[1] = -1.6f + (1.9f + 1.6f) * e;
            sit.foreArm[1] = -0.9f + (1.7f + 0.9f) * e;
            sit.lean = -0.02f + 0.25f * e;
        }
        release = before < 0.55f && tt >= 0.55f;
        if (tt > 1.4f)
            throwT_ = -1.0f;
    }
    Look look;
    look.cap = true;
    look.backpack = false;
    look.shirt = {0.18f, 0.55f, 0.56f};
    look.pants = {0.30f, 0.26f, 0.22f};
    look.capColor = {0.95f, 0.72f, 0.20f};
    float drop = 0.0f;
    for (int i = 0; i < 2; ++i) drop = std::max(drop, std::cos(sit.thigh[i]) * kThigh + std::cos(sit.shin[i]) * kShin);
    const FigureOut fig = figure(cv_, bx - 1.0f * k, bTop + 1.0f * k + (drop + 0.5f) * s, s, -1.0f, sit, look);

    if (release) {
        stone_.hops.clear();
        stone_.hops.push_back(fig.hand[1]);
        float x = W * (0.50f + 0.05f * rnd()), y = horizon_ + (H * 0.84f - horizon_) * 0.62f, step = 14.0f * k;
        for (int i = 0; i < 5; ++i) {
            stone_.hops.push_back({x, y});
            x -= step;
            y -= step * 0.18f;
            step *= 0.68f;
        }
        stone_.t = 0.0f;
    }
    if (stone_.t >= 0.0f) {
        const int seg = static_cast<int>(stone_.t);
        const float dur = seg == 0 ? 0.55f : 0.30f * std::pow(0.82f, static_cast<float>(seg));
        stone_.t += dt / dur;
        if (static_cast<int>(stone_.t) > seg) {
            const Point land = stone_.hops[static_cast<size_t>(seg + 1)];
            ripples_.push_back({land.x, land.y, 0.4f, seg + 2 == static_cast<int>(stone_.hops.size()) ? 1.0f : 0.8f});
        }
        if (stone_.t >= static_cast<float>(stone_.hops.size() - 1))
            stone_.t = -1.0f;
        else {
            const int sg = static_cast<int>(stone_.t);
            const float u = stone_.t - sg;
            const Point a = stone_.hops[static_cast<size_t>(sg)], b = stone_.hops[static_cast<size_t>(sg + 1)];
            const float arc = (sg == 0 ? 12.0f : 3.0f * std::pow(0.65f, static_cast<float>(sg))) * k * std::sin(u * kPi);
            cv_.ellipse(a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u - arc, 0.7f * k, 0.6f * k, 0, {0.30f, 0.30f, 0.32f});
        }
    }

    present(canvas);
    wait_until_next_frame();
    return true;
}

Scenes::SceneDescriptor AlpineLakeScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.family = "hiking";
    d.tags = {"hiking", "mountains", "lake", "scenery", "landscape", "morning", "calm", "audio-reactive"};
    d.intensity = 0.15f;
    d.motion = 0.2f;
    d.music_affinity = 0.35f;
    d.performance_cost = 0.35f;
    return d;
}
