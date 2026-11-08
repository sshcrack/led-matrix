#include "SportScenes.h"

using namespace Hobbies;

namespace {
// Fold a tempo into [lo, hi) by octaves.
float fold_bpm(float bpm, float lo, float hi)
{
    while (bpm >= hi) bpm *= 0.5f;
    while (bpm < lo) bpm *= 2.0f;
    return bpm;
}

struct Court {
    float k, floor, rimY, rimX0, rimX1, boardX, s, r, gravity;
    static Court of(float W, float H)
    {
        const float k = W / 128.0f;
        return {k, H * 0.86f, H * 0.43f, W * 0.775f, W * 0.835f, W * 0.845f, 1.9f * k, 2.3f * k, 80.0f * k};
    }
};

void draw_ball(Canvas& cv, float x, float y, float r, float spin, Rgb tint)
{
    cv.ellipse(x, y, r, r, 0, tint);
    const float ca = std::cos(spin) * r, sa = std::sin(spin) * r;
    const Rgb seam = scale(tint, 0.35f);
    cv.capsule(x - ca, y - sa, 0.25f, x + ca, y + sa, 0.25f, seam, 0.8f);
    cv.capsule(x + sa * 0.6f, y - ca * 0.6f, 0.2f, x - sa * 0.6f, y + ca * 0.6f, 0.2f, seam, 0.6f);
}
}  // namespace

// ================================================================ basketball
BasketballScene::BasketballScene()
{
    // On a court only winter (snow) looks different; it still follows the real season.
    Environment::Adapt adapt;
    adapt.season_variants = {Environment::Season::Winter};
    use_environment({Environment::TimeOfDay::Dusk, Environment::Weather::Clear, Environment::Season::Summer, -1.5f, 0.70f}, adapt);
}

void BasketballScene::build_background()
{
    const auto& L = env();
    const int w = cv_.w, h = cv_.h;
    const float W = static_cast<float>(w), H = static_cast<float>(h);
    const Court c = Court::of(W, H);
    const float k = c.k;
    Canvas bg;
    bg.resize(w, h);
    const Environment::Surface surf{bg.px.data(), w, h, 1.0f};
    const float horizon = c.floor - 22 * k;
    Environment::paint_sky(surf, L, horizon);
    Environment::paint_stars(surf, L, horizon * 0.8f, 0.0f, 31, 50);
    Environment::paint_sun(surf, L, Environment::sun_point(L, W, horizon), 7.0f * k);
    Environment::paint_moon(surf, L, Environment::moon_point(L, W, horizon), 4.0f * k);
    const float lamp = L.lamp();

    // City skyline with a few lit windows.
    float x = -2.0f;
    uint32_t id = 17;
    while (x < W) {
        const float bw = (6 + hash01(id) * 10) * k;
        const float bh = (8 + hash01(id + 1) * 26) * k;
        const float top = c.floor - 22 * k - bh;
        // Concrete and brick towers fading into the haze; snow on the roofs.
        const Rgb col = lit(mix({0.36f, 0.38f, 0.46f}, {0.48f, 0.42f, 0.44f}, hash01(id + 2)), 0.85f);
        const Rgb roof = terrain({0.36f, 0.38f, 0.46f}, 0.85f, 1.0f);
        const Rgb glass = lit({0.30f, 0.38f, 0.50f}, 0.8f);
        for (int yy = std::max(0, static_cast<int>(top)); yy < static_cast<int>(c.floor); ++yy)
            for (int xx = std::max(0, static_cast<int>(x)); xx < std::min(w, static_cast<int>(x + bw)); ++xx) {
                bg.set(xx, yy, yy < static_cast<int>(top) + 1 ? roof : col);
                const bool window = (xx - static_cast<int>(x)) % 3 == 1 && (yy - static_cast<int>(top)) % 4 == 2;
                if (!window)
                    continue;
                // More windows light up the darker it gets.
                const float r = hash01(static_cast<uint32_t>(xx * 131 + yy * 7919));
                if (r > 1.0f - 0.16f * lamp)
                    bg.set(xx, yy, mix(glass, {0.95f, 0.72f, 0.42f}, 0.4f + 0.6f * lamp));
                else
                    bg.set(xx, yy, glass);
            }
        x += bw;
        id += 5;
    }
    // Chain-link fence in front of the city.
    const int fenceTop = static_cast<int>(c.floor - 30 * k);
    for (int yy = std::max(0, fenceTop); yy < static_cast<int>(c.floor); ++yy)
        for (int xx = 0; xx < w; ++xx)
            if ((xx + yy) % 5 == 0 || (xx - yy + 1000) % 5 == 0)
                bg.blend(xx, yy, lit({0.75f, 0.75f, 0.82f}, 0.2f), 0.14f);  // faint: the player reads in front
    bg.capsule(0, fenceTop, 0.4f * k, W, fenceTop, 0.4f * k, lit({0.45f, 0.43f, 0.50f}));
    // Floodlight poles.
    for (float lx : {W * 0.05f, W * 0.55f}) bg.capsule(lx, c.floor, 0.6f * k, lx, H * 0.12f, 0.5f * k, lit({0.28f, 0.26f, 0.32f}));
    // Court: asphalt (darker and glossy when wet, swept clear of snow) with a
    // painted key under the hoop.
    const Rgb asphaltTop = terrain(mix({0.40f, 0.40f, 0.46f}, {0.24f, 0.24f, 0.30f}, L.wetness), 0.0f, 0.2f);  // swept court
    const Rgb asphaltLow = terrain(mix({0.22f, 0.22f, 0.28f}, {0.12f, 0.12f, 0.17f}, L.wetness), 0.0f, 0.2f);
    const Rgb key = lit({0.62f, 0.22f, 0.22f}), courtLine = lit({0.92f, 0.92f, 0.92f});
    for (int yy = static_cast<int>(c.floor); yy < h; ++yy)
        for (int xx = 0; xx < w; ++xx) {
            const float d = (yy - c.floor) / std::max(1.0f, H - c.floor);
            Rgb col = mix(asphaltTop, asphaltLow, d);
            if (xx > W * 0.62f && yy < c.floor + 3 * k)
                col = mix(col, key, 0.6f * (1.0f - 0.7f * L.snow_cover));
            if (yy == static_cast<int>(c.floor))
                col = courtLine;
            bg.set(xx, yy, col);
        }
    // Snow: banks shovelled against the fence and on the top rail.
    if (L.snow_cover > 0.05f) {
        // Snow stays bright under city light and dusk alike.
        const Rgb snowC = mix(lit({0.95f, 0.97f, 1.0f}), {0.86f, 0.89f, 0.97f}, 0.4f), snowShade = mix(lit({0.74f, 0.80f, 0.94f}), {0.62f, 0.66f, 0.80f}, 0.4f);
        for (int xx = 0; xx < w; ++xx) {
            const float bank = (4.0f + 3.5f * (0.5f + 0.5f * std::sin(xx / k * 0.09f)) + 1.5f * std::sin(xx / k * 0.31f)) * k * L.snow_cover;
            for (int yy = static_cast<int>(c.floor - bank); yy <= static_cast<int>(c.floor + 1); ++yy)
                bg.blend(xx, yy, yy < c.floor - bank * 0.5f ? snowC : snowShade, 0.95f);
        }
        bg.capsule(0, fenceTop - 0.8f * k, 0.5f * k, W, fenceTop - 0.8f * k, 0.5f * k, snowShade, 0.55f * L.snow_cover);
        for (float lx : {W * 0.05f, W * 0.55f})
            bg.ellipse(lx, H * 0.12f - 1.2f * k, 1.8f * k, 0.8f * k, 0, snowC, L.snow_cover);
    }
    // Wet asphalt mirrors the lit windows and the floodlights.
    if (L.wetness > 0.05f)
        for (int yy = static_cast<int>(c.floor) + 1; yy < h; ++yy)
            for (int xx = 0; xx < w; ++xx) {
                const int my = static_cast<int>(2 * c.floor - yy);
                if (my < 0)
                    continue;
                const float* src = bg.at(xx, my);
                bg.blend(xx, yy, {src[0], src[1], src[2]}, 0.35f * L.wetness * (1.0f - (yy - c.floor) / (H - c.floor)));
            }
    // Hoop: pole, arm and edge-on backboard.
    const float poleX = W * 0.93f;
    const Rgb steel = lit({0.30f, 0.30f, 0.36f});
    bg.capsule(poleX, c.floor, 1.1f * k, poleX, c.rimY - 12 * k, 1.0f * k, steel);
    bg.capsule(poleX, c.rimY - 8 * k, 0.7f * k, c.boardX, c.rimY - 6 * k, 0.7f * k, steel);
    bg.capsule(c.boardX, c.rimY - 14 * k, 0.9f * k, c.boardX, c.rimY + 3 * k, 0.9f * k, lit({0.92f, 0.94f, 0.98f}));
    if (L.snow_cover > 0.05f)
        bg.ellipse(c.boardX, c.rimY - 14.8f * k, 1.6f * k, 0.8f * k, 0, lit({0.95f, 0.96f, 1.0f}), L.snow_cover);
    background_ = std::move(bg.px);
}

void BasketballScene::initialize(int width, int height)
{
    AudioHobbyScene::initialize(width, height);
    background_.clear();
    px_ = pTarget_ = width * 0.32f;
    state_ = State::Dribble;
    stateT_ = 0.0f;
    bouncesLeft_ = 5;
    swish_ = rattle_ = 0.0f;
    onFire_ = false;
}

void BasketballScene::release(float handX, float handY)
{
    const Court c = Court::of(static_cast<float>(cv_.w), static_cast<float>(cv_.h));
    make_ = onFire_ || rnd() < 0.75f;
    const float tx = make_ ? (c.rimX0 + c.rimX1) * 0.5f : c.rimX0 + 0.6f * c.k;
    const float ty = make_ ? c.rimY - 0.5f * c.k : c.rimY - c.r * 0.5f;
    bx_ = handX;
    by_ = handY - c.r;
    const float dist = std::fabs(tx - bx_);
    flightDur_ = 0.85f + dist / (cv_.w * 1.6f);
    bvx_ = (tx - bx_) / flightDur_;
    bvy_ = (ty - by_ - 0.5f * c.gravity * flightDur_ * flightDur_) / flightDur_;
    flightT_ = 0.0f;
    shotX0_ = bx_;
    shotY0_ = by_;
    shotVx0_ = bvx_;
    shotVy0_ = bvy_;
    for (auto& p : trail_) p = {bx_, by_};
}

bool BasketballScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt, rdt;
    begin_audio_frame(dt, rdt);
    const float W = static_cast<float>(cv_.w), H = static_cast<float>(cv_.h);
    const Court c = Court::of(W, H);
    const float k = c.k, s = c.s;
    const float am = std::min(1.0f, audio_.amount);
    const auto& L = env();
    if (environment_changed(backgroundKey_) || background_.empty())
        build_background();
    cv_.px = background_;
    Environment::paint_clouds(surface(), L, t_, H * 0.04f, H * 0.30f, 0.0f, 13);
    if (audio_.dropHit)
        onFire_ = true;

    // Floodlights come on at dusk (and in gloomy weather) and pump with the bass.
    const float on = L.lamp();
    const float lamp = (0.55f + 0.35f * audio_.bass * am + 0.5f * audio_.drop * am) * on;
    for (float lx : {W * 0.05f, W * 0.55f}) {
        cv_.glow(lx, H * 0.12f, 26 * k, {1.0f, 0.90f, 0.70f}, 0.20f * lamp);
        cv_.ellipse(lx, H * 0.12f, 2.2f * k, 1.2f * k, 0, mix(lit({0.55f, 0.55f, 0.55f}), mix({0.9f, 0.85f, 0.7f}, {1, 1, 1}, lamp - 0.5f), on));
        // Rain and snow sparkle in the beams.
        if (L.rain + L.snow > 0.05f && on > 0.1f)
            cv_.glow(lx + 4 * k, H * 0.30f, 22 * k, {0.75f, 0.78f, 0.85f}, 0.06f * on * (L.rain + L.snow));
    }
    // Light pool on the court.
    for (float lx : {W * 0.05f, W * 0.55f})
        cv_.ellipse(lx + 12 * k, c.floor + 4 * k, 30 * k, 3 * k, 0, {0.50f, 0.48f, 0.52f}, 0.12f * lamp * (1.0f + L.wetness));

    // Dribble tempo: one floor contact per beat while music plays.
    float period = 0.55f;
    if (audio_.bpm > 0.0f)
        period = 60.0f / fold_bpm(audio_.bpm, 85.0f, 170.0f);
    const float catchOffset = 4.5f * s;

    stateT_ += dt;
    Pose pose;
    Look look;
    look.shirt = {0.95f, 0.95f, 0.98f};
    look.pants = {0.20f, 0.30f, 0.75f};
    look.shoes = {0.95f, 0.30f, 0.20f};
    look.skin = {0.45f, 0.29f, 0.20f};
    look.hair = {0.08f, 0.05f, 0.04f};
    if (L.winter > 0.5f || L.snow_cover > 0.5f)
        look.shirt = {0.88f, 0.26f, 0.16f};  // a red puffer jacket in the cold, bright against the city
    light_look(look);
    // The floodlights still light the player at night.
    if (lamp > 0.0f) {
        auto boost = [&](Rgb& col, Rgb base) { col = mix(col, scale(base, 0.75f), 0.55f * on); };
        boost(look.shirt, L.winter > 0.5f || L.snow_cover > 0.5f ? Rgb{0.88f, 0.26f, 0.16f} : Rgb{0.95f, 0.95f, 0.98f});
        boost(look.pants, {0.20f, 0.30f, 0.75f});
        boost(look.skin, {0.45f, 0.29f, 0.20f});
        boost(look.shoes, {0.95f, 0.30f, 0.20f});
    }
    float lift = 0.0f;
    float handAboveHead = 0.0f;

    Pose crouch;
    crouch.lean = 0.22f;
    crouch.thigh[0] = 0.55f;
    crouch.thigh[1] = 0.35f;
    crouch.shin[0] = -0.35f;
    crouch.shin[1] = -0.45f;
    crouch.upperArm[0] = 0.9f;
    crouch.foreArm[0] = 1.6f;  // guard arm out front
    Pose shoot;
    shoot.lean = 0.02f;
    shoot.thigh[0] = 0.15f;
    shoot.thigh[1] = 0.05f;
    shoot.shin[0] = -0.1f;
    shoot.shin[1] = -0.05f;
    shoot.upperArm[0] = 2.5f;
    shoot.foreArm[0] = 2.9f;
    shoot.upperArm[1] = 2.75f;
    shoot.foreArm[1] = 3.05f;

    float dribbleU = 0.0f;
    switch (state_) {
    case State::Dribble: {
        const float before = dribble_;
        dribble_ += dt / period;
        if (audio_.bpm > 0.0f && am > 0.2f) {
            // Ease the floor contact (u = 0.5) onto the beat.
            const float target = fract(audio_.beatPhase + 0.5f);
            const float err = std::remainder(target - fract(dribble_), 1.0f);
            dribble_ += err * std::min(1.0f, rdt * 4.0f) * am;
        }
        if (std::floor(dribble_) > std::floor(before))
            --bouncesLeft_;
        dribbleU = fract(dribble_);
        pose = crouch;
        const float d = 1.0f - std::fabs(1.0f - 2.0f * dribbleU);
        pose.upperArm[1] = 0.55f;
        pose.foreArm[1] = 0.35f + 0.45f * (1.0f - d);
        if (bouncesLeft_ <= 0 && dribbleU < 0.08f) {
            state_ = State::Gather;
            stateT_ = 0.0f;
        }
        break;
    }
    case State::Gather: {
        const float g = std::min(1.0f, stateT_ / 0.55f);
        pose = blend(crouch, shoot, smoothstep(0.0f, 0.7f, g));
        // Dip, then jump.
        if (g < 0.3f) {
            pose.thigh[0] += 0.4f * g / 0.3f;
            pose.thigh[1] += 0.4f * g / 0.3f;
            pose.shin[0] -= 0.4f * g / 0.3f;
            pose.shin[1] -= 0.4f * g / 0.3f;
        }
        lift = g > 0.35f ? 5.5f * std::sin((g - 0.35f) / 0.65f * kPi * 0.5f) : 0.0f;
        handAboveHead = g;
        break;
    }
    case State::Flight:
    case State::Loose: {
        // Land after the release, then walk to a new spot.
        const float land = std::min(1.0f, stateT_ / 0.35f);
        if (state_ == State::Flight && land < 1.0f) {
            pose = shoot;
            pose.foreArm[1] = 3.45f;  // follow-through
            lift = 5.5f * (1.0f - land * land);
        }
        else {
            const float dx = pTarget_ - px_;
            if (std::fabs(dx) > 0.5f * k) {
                facing_ = dx > 0 ? 1.0f : -1.0f;
                const float step = std::min(std::fabs(dx), dt * 22.0f * k);
                px_ += facing_ * step;
                walk_ += step / (13.0f * s);
                pose = walk_pose(fract(walk_), 0.42f, 0.08f);
            }
            else {
                facing_ = 1.0f;
                pose = crouch;
                pose.upperArm[1] = 0.9f;
                pose.foreArm[1] = 1.3f;  // hands ready
            }
        }
        break;
    }
    }

    // Draw the player first so the ball passes in front.
    const float groundX = px_;
    const FigureOut fig = figure(cv_, groundX, c.floor + 0.5f * k, s, facing_, pose, look, lift);
    const Point hand = fig.hand[1];

    // Ball.
    float ballX = bx_, ballY = by_;
    Rgb ballCol = {0.92f, 0.45f, 0.14f};
    if (state_ == State::Dribble) {
        const float d = 1.0f - std::fabs(1.0f - 2.0f * dribbleU);
        ballX = hand.x + 0.4f * k;
        ballY = hand.y + c.r + (c.floor - c.r - (hand.y + c.r)) * std::pow(d, 0.85f);
        spin_ += rdt * 3.0f;
    }
    else if (state_ == State::Gather) {
        ballX = hand.x + 0.4f * k;
        ballY = hand.y - c.r * (0.4f + 0.6f * handAboveHead);
        if (stateT_ >= 0.55f) {
            release(hand.x, hand.y);
            state_ = State::Flight;
            stateT_ = 0.0f;
            pTarget_ = W * (0.16f + rnd() * 0.30f);
            ballX = bx_;
            ballY = by_;
        }
    }
    else {
        if (state_ == State::Flight) {
            // Evaluate the arc analytically so the ball reaches exactly the
            // rim point release() aimed at, whatever the frame rate or speed.
            flightT_ = std::min(flightT_ + dt, flightDur_);
            bx_ = shotX0_ + shotVx0_ * flightT_;
            by_ = shotY0_ + shotVy0_ * flightT_ + 0.5f * c.gravity * flightT_ * flightT_;
            bvx_ = shotVx0_;
            bvy_ = shotVy0_ + c.gravity * flightT_;
        }
        else {
            bvy_ += c.gravity * dt;
            bx_ += bvx_ * dt;
            by_ += bvy_ * dt;
        }
        spin_ += (bvx_ >= 0 ? 1.0f : -1.0f) * rdt * 7.0f;
        if (state_ == State::Flight) {
            if (flightT_ >= flightDur_) {
                if (make_) {
                    bvx_ *= 0.08f;
                    bvy_ = std::max(bvy_ * 0.5f, 20.0f * k);
                    swish_ = 1.0f;
                }
                else {
                    bvx_ = -std::fabs(bvx_) * 0.45f - 8.0f * k;
                    bvy_ = -std::fabs(bvy_) * 0.40f;
                    rattle_ = 1.0f;
                }
                state_ = State::Loose;
                onFire_ = false;
            }
        }
        if (state_ == State::Loose) {
            const float catchX = pTarget_ + catchOffset;
            if (by_ + c.r >= c.floor) {
                by_ = c.floor - c.r;
                bvy_ = -std::fabs(bvy_) * 0.62f;
                if (std::fabs(bvy_) < 14.0f * k)
                    bvy_ = -32.0f * k;  // keep it bouncing toward the player
                bvx_ = std::clamp((catchX - bx_) / 0.9f, -60.0f * k, 60.0f * k);
            }
            const bool arrived = std::fabs(px_ - pTarget_) < 1.0f * k;
            if ((arrived && std::fabs(bx_ - (px_ + catchOffset)) < 3.0f * k && by_ > c.floor - 14 * k && bvy_ < 0) || stateT_ > 6.0f) {
                state_ = State::Dribble;
                facing_ = 1.0f;
                px_ = pTarget_;
                dribble_ = std::floor(dribble_) + 0.25f;
                bouncesLeft_ = audio_.bpm > 0.0f && am > 0.3f ? 8 : 4 + static_cast<int>(rnd() * 4);
                stateT_ = 0.0f;
            }
        }
        ballX = bx_;
        ballY = by_;
    }
    if (onFire_) {
        trail_[static_cast<size_t>(trailHead_)] = {ballX, ballY};
        trailHead_ = (trailHead_ + 1) % static_cast<int>(trail_.size());
        for (size_t i = 0; i < trail_.size(); ++i) {
            const float age = static_cast<float>((static_cast<size_t>(trailHead_) + trail_.size() - i - 1) % trail_.size()) / trail_.size();
            cv_.glow(trail_[i].x, trail_[i].y, c.r * (2.2f - age), {1.0f, 0.45f + 0.3f * age, 0.1f}, 0.35f * (1.0f - age));
        }
        ballCol = {1.0f, 0.55f, 0.18f};
    }
    // Shadow, then the ball.
    const float height = std::max(0.0f, c.floor - ballY);
    cv_.ellipse(ballX, c.floor + 1.0f * k, c.r * (1.2f - std::min(0.6f, height / H)), 0.7f * k, 0, {0.05f, 0.05f, 0.08f}, 0.5f);

    // Rim and net; the ball goes through between the back and front strands.
    swish_ = std::max(0.0f, swish_ - rdt * 1.6f);
    rattle_ = std::max(0.0f, rattle_ - rdt * 3.0f);
    const float shake = rattle_ * std::sin(t_ * 60.0f) * 0.6f * k;
    const float netLen = 9.0f * k * (1.0f + 0.35f * swish_);
    const float sway = std::sin(t_ * 1.7f) * 0.4f * k + std::sin(t_ * 9.0f) * swish_ * 1.2f * k;
    auto strand = [&](float u, float a) {
        const float top = c.rimX0 + (c.rimX1 - c.rimX0) * u;
        const float bottom = c.rimX0 + (c.rimX1 - c.rimX0) * (0.25f + 0.5f * u) + sway;
        cv_.capsule(top, c.rimY + shake, 0.25f, bottom, c.rimY + netLen, 0.25f, {0.95f, 0.95f, 0.95f}, a);
    };
    for (float u : {0.15f, 0.5f, 0.85f}) strand(u, 0.45f);
    draw_ball(cv_, ballX, ballY, c.r, spin_, ballCol);
    for (float u : {0.0f, 0.33f, 0.67f, 1.0f}) strand(u, 0.85f);
    for (int i = 1; i <= 2; ++i) {
        const float y = c.rimY + netLen * i / 3.0f;
        const float n = 1.0f - 0.25f * i;
        const float cx = (c.rimX0 + c.rimX1) * 0.5f + sway * i / 3.0f;
        const float hw = (c.rimX1 - c.rimX0) * 0.5f * n;
        cv_.capsule(cx - hw, y, 0.2f, cx + hw, y, 0.2f, {0.9f, 0.9f, 0.9f}, 0.6f);
    }
    cv_.capsule(c.rimX0, c.rimY + shake, 0.55f * k, c.rimX1, c.rimY + shake, 0.55f * k, {0.95f, 0.38f, 0.10f});
    if (swish_ > 0.0f)
        cv_.glow((c.rimX0 + c.rimX1) * 0.5f, c.rimY + 3 * k, 12 * k, {1.0f, 0.75f, 0.35f}, 0.25f * swish_ * (0.4f + am));

    paint_weather(rdt, c.floor - 22 * k, [&](float) { return c.floor + 1.0f; });
    present(canvas);
    wait_until_next_frame();
    return true;
}

Scenes::SceneDescriptor BasketballScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.family = "sports";
    d.tags = {"sports", "basketball", "streetball", "dusk", "city", "audio-reactive"};
    d.intensity = 0.45f;
    d.motion = 0.6f;
    d.music_affinity = 0.5f;
    d.performance_cost = 0.3f;
    environment_->add_variants(d);
    return d;
}

// ================================================================ soccer
Point SoccerScene::goal_center(int attackingTeam) const
{
    return {attackingTeam == 0 ? x1_ : x0_, (y0_ + y1_) * 0.5f};
}

SoccerScene::SoccerScene()
{
    // A floodlit night match: the weather and season follow reality, the
    // time stays night (the floodlights are the scene).
    Environment::Adapt adapt;
    adapt.time = false;
    adapt.season_variants = {Environment::Season::Winter};
    use_environment({Environment::TimeOfDay::Night, Environment::Weather::Clear, Environment::Season::Summer}, adapt);
}

void SoccerScene::build_background()
{
    const auto& L = env();
    const float lamp = L.lamp();
    const int w = cv_.w, h = cv_.h;
    const float W = static_cast<float>(w), H = static_cast<float>(h), k = W / 128.0f;
    x0_ = 8 * k;
    x1_ = W - 8 * k;
    y0_ = 17 * k;
    y1_ = H - 14 * k;
    Canvas bg;
    bg.resize(w, h);
    // Seen from above, the stands and running track surround the pitch. At
    // night only the floodlights light it; by day the sun does.
    auto light = [&](Rgb albedo) {
        // Floodlit colour (the scene's designed look) or daylight.
        return mix(lit(albedo), albedo, lamp * 0.92f);
    };
    const Rgb stand = light({0.20f, 0.20f, 0.26f});
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) bg.set(x, y, mix(stand, {0.06f, 0.06f, 0.10f}, lamp));
    const Rgb apron = from_env(env().ground(to_env(light({0.10f, 0.26f, 0.12f})), 0.9f));
    for (int y = static_cast<int>(y0_ - 4 * k); y < static_cast<int>(y1_ + 4 * k); ++y)
        for (int x = static_cast<int>(x0_ - 5 * k); x < static_cast<int>(x1_ + 5 * k); ++x) bg.set(x, y, apron);
    // Mowed stripes with a floodlit centre; a snowy pitch keeps faint stripes.
    const float cx = (x0_ + x1_) * 0.5f, cy = (y0_ + y1_) * 0.5f;
    const Rgb stripeA = light(from_env(L.grass({0.16f, 0.46f, 0.20f}))), stripeB = light(from_env(L.grass({0.13f, 0.40f, 0.17f})));
    const Rgb snow = mix(lit({0.85f, 0.88f, 0.94f}), {0.80f, 0.82f, 0.88f}, lamp);
    for (int y = static_cast<int>(y0_ - 3 * k); y < static_cast<int>(y1_ + 3 * k); ++y)
        for (int x = static_cast<int>(x0_ - 4 * k); x < static_cast<int>(x1_ + 4 * k); ++x) {
            const bool stripe = static_cast<int>((x - x0_) / (9.6f * k) + 100) % 2 == 0;
            const float dx = (x - cx) / W, dy = (y - cy) / H;
            const float flood = 1.15f - 0.9f * (dx * dx + dy * dy);
            // An even blanket, a little thinner where play tramples the middle;
            // blotchy patches would fight the players for attention.
            const float edge = std::clamp(std::max(std::fabs(dx) * 2.2f, std::fabs(dy) * 2.0f), 0.0f, 1.0f);
            const float cover = L.snow_cover * (0.80f + 0.12f * edge);
            Rgb c = mix(stripe ? stripeA : stripeB, scale(snow, stripe ? 1.0f : 0.94f), cover);
            bg.set(x, y, scale(c, 1.0f + (flood - 1.0f) * lamp));
        }
    // White lines, painted red on a snowy pitch so they still show.
    const Rgb line = mix(mix(lit({0.92f, 0.95f, 0.92f}), {0.88f, 0.92f, 0.88f}, lamp), light({0.92f, 0.20f, 0.14f}), L.snow_cover * 0.85f);
    auto seg = [&](float ax, float ay, float bx, float by) { bg.capsule(ax, ay, 0.35f * k, bx, by, 0.35f * k, line, 0.85f); };
    seg(x0_, y0_, x1_, y0_);
    seg(x0_, y1_, x1_, y1_);
    seg(x0_, y0_, x0_, y1_);
    seg(x1_, y0_, x1_, y1_);
    seg(cx, y0_, cx, y1_);
    const float ph = (y1_ - y0_);
    for (int side = 0; side < 2; ++side) {
        const float gx = side == 0 ? x0_ : x1_, dir = side == 0 ? 1.0f : -1.0f;
        const float boxD = 16 * k, boxH = ph * 0.55f, smallD = 5.5f * k, smallH = ph * 0.26f;
        seg(gx, cy - boxH / 2, gx + dir * boxD, cy - boxH / 2);
        seg(gx, cy + boxH / 2, gx + dir * boxD, cy + boxH / 2);
        seg(gx + dir * boxD, cy - boxH / 2, gx + dir * boxD, cy + boxH / 2);
        seg(gx, cy - smallH / 2, gx + dir * smallD, cy - smallH / 2);
        seg(gx, cy + smallH / 2, gx + dir * smallD, cy + smallH / 2);
        seg(gx + dir * smallD, cy - smallH / 2, gx + dir * smallD, cy + smallH / 2);
        bg.ellipse(gx + dir * 11 * k, cy, 0.6f * k, 0.6f * k, 0, line);
        // D arc outside the box.
        for (int i = -6; i <= 6; ++i) {
            const float a = i / 6.0f * 0.9f;
            bg.blend(static_cast<int>(gx + dir * (boxD + std::cos(a) * 4.5f * k - 1.5f * k)), static_cast<int>(cy + std::sin(a) * 8 * k), line, 0.8f);
        }
        // Goal: posts, crossbar seen from above, netting behind the line.
        const float gh = ph * 0.14f;
        for (int y = static_cast<int>(cy - gh); y <= static_cast<int>(cy + gh); ++y)
            for (int x = 0; x < static_cast<int>(3.5f * k); ++x) {
                const int px = static_cast<int>(gx - dir * (1 + x));
                if ((x + y) % 2 == 0)
                    bg.blend(px, y, {0.85f, 0.85f, 0.90f}, 0.45f);
            }
        seg(gx, cy - gh, gx - dir * 3.5f * k, cy - gh);
        seg(gx, cy + gh, gx - dir * 3.5f * k, cy + gh);
        bg.capsule(gx, cy - gh, 0.6f * k, gx, cy + gh, 0.6f * k, {1, 1, 1});
    }
    // Centre circle.
    for (int i = 0; i < 64; ++i) {
        const float a = i / 64.0f * kTau;
        bg.blend(static_cast<int>(cx + std::cos(a) * 10 * k), static_cast<int>(cy + std::sin(a) * 10 * k), line, 0.85f);
    }
    bg.ellipse(cx, cy, 0.6f * k, 0.6f * k, 0, line);
    background_ = std::move(bg.px);
}

void SoccerScene::kickoff(int team)
{
    const float cx = (x0_ + x1_) * 0.5f;
    for (auto& p : players_) p.pos = {cx + (p.home.x - cx) * 1.0f, p.home.y};
    // The kicking team's centre-mid takes the ball at the spot.
    for (size_t i = 0; i < players_.size(); ++i)
        if (players_[i].team == team && !players_[i].keeper && std::fabs(players_[i].home.y - (y0_ + y1_) * 0.5f) < 1.0f &&
            std::fabs(players_[i].home.x - cx) < (x1_ - x0_) * 0.2f) {
            holder_ = static_cast<int>(i);
            players_[i].pos = {cx - (team == 0 ? 1.5f : -1.5f), (y0_ + y1_) * 0.5f};
        }
    ballPos_ = players_[static_cast<size_t>(holder_)].pos;
    ball_ = Ball::Held;
    holdT_ = 0.0f;
    holdMin_ = 1.0f + rnd();
}

void SoccerScene::initialize(int width, int height)
{
    AudioHobbyScene::initialize(width, height);
    build_background();
    backgroundKey_ = {};
    players_.clear();
    // Fractions of the pitch for the team attacking right.
    static constexpr float kHome[][2] = {{0.03f, 0.50f}, {0.22f, 0.28f}, {0.22f, 0.72f}, {0.42f, 0.50f}, {0.62f, 0.25f}, {0.62f, 0.75f}};
    for (int team = 0; team < 2; ++team)
        for (int i = 0; i < 6; ++i) {
            float fx = kHome[i][0], fy = kHome[i][1];
            if (team == 1) {
                fx = 1.0f - fx;
                fy = 1.0f - fy;
            }
            // Keep both centre-mids just inside their own half at kick-off.
            if (i == 3)
                fx = team == 0 ? 0.44f : 0.56f;
            const Point home{x0_ + (x1_ - x0_) * fx, y0_ + (y1_ - y0_) * fy};
            players_.push_back({home, home, team, i == 0, rnd()});
        }
    flashes_.clear();
    celebrate_ = netRipple_ = flashBoost_ = 0.0f;
    kickoff(rnd() < 0.5f ? 0 : 1);
}

void SoccerScene::decide()
{
    const Player& h = players_[static_cast<size_t>(holder_)];
    const Point goal = goal_center(h.team);
    const float toGoal = std::fabs(goal.x - h.pos.x);
    const float pw = x1_ - x0_;
    if (!h.keeper && (forceShot_ || (toGoal < pw * 0.30f && rnd() < 0.55f))) {
        // Shoot.
        const float k = cv_.w / 128.0f;
        ball_ = Ball::Shot;
        from_ = ballPos_;
        to_ = {goal.x + (h.team == 0 ? 2.5f : -2.5f) * k, goal.y + (rnd() - 0.5f) * (y1_ - y0_) * 0.22f};
        goal_ = forceShot_ ? rnd() < 0.85f : rnd() < 0.40f;
        if (!goal_)
            to_.x = goal.x + (h.team == 0 ? -1.5f : 1.5f) * k;  // the keeper gets there first
        ballT_ = 0.0f;
        ballDur_ = std::max(0.35f, std::sqrt(std::pow(to_.x - from_.x, 2.0f) + std::pow(to_.y - from_.y, 2.0f)) / (95.0f * k));
        forceShot_ = false;
        netSide_ = h.team == 0 ? 1 : 0;
        return;
    }
    // Pass to a teammate, preferring one further forward.
    int best = -1;
    float bestScore = -1e9f;
    for (size_t i = 0; i < players_.size(); ++i) {
        const auto& p = players_[i];
        if (p.team != h.team || static_cast<int>(i) == holder_ || p.keeper)
            continue;
        const float progress = (p.pos.x - h.pos.x) * (h.team == 0 ? 1.0f : -1.0f);
        const float dist = std::sqrt(std::pow(p.pos.x - h.pos.x, 2.0f) + std::pow(p.pos.y - h.pos.y, 2.0f));
        const float score = progress * 0.6f - std::fabs(dist - pw * 0.25f) * 0.4f + rnd() * pw * 0.25f;
        if (score > bestScore) {
            bestScore = score;
            best = static_cast<int>(i);
        }
    }
    if (best < 0)
        return;
    receiver_ = best;
    // Sometimes the nearest opponent to the passing lane cuts it out.
    if (rnd() < 0.18f) {
        const Point mid{(h.pos.x + players_[static_cast<size_t>(best)].pos.x) * 0.5f, (h.pos.y + players_[static_cast<size_t>(best)].pos.y) * 0.5f};
        float nd = 1e9f;
        for (size_t i = 0; i < players_.size(); ++i) {
            const auto& p = players_[i];
            if (p.team == h.team || p.keeper)
                continue;
            const float d = std::pow(p.pos.x - mid.x, 2.0f) + std::pow(p.pos.y - mid.y, 2.0f);
            if (d < nd) {
                nd = d;
                receiver_ = static_cast<int>(i);
            }
        }
    }
    ball_ = Ball::Pass;
    from_ = ballPos_;
    ballT_ = 0.0f;
    const auto& r = players_[static_cast<size_t>(receiver_)];
    const float k = cv_.w / 128.0f;
    ballDur_ = std::max(0.4f, std::sqrt(std::pow(r.pos.x - from_.x, 2.0f) + std::pow(r.pos.y - from_.y, 2.0f)) / (48.0f * k));
}

bool SoccerScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt, rdt;
    begin_audio_frame(dt, rdt);
    const float W = static_cast<float>(cv_.w), H = static_cast<float>(cv_.h), k = W / 128.0f;
    const float am = std::min(1.0f, audio_.amount);
    const float cx = (x0_ + x1_) * 0.5f, cy = (y0_ + y1_) * 0.5f;
    if (environment_changed(backgroundKey_))
        build_background();
    cv_.px = background_;
    if (audio_.dropHit)
        forceShot_ = true;

    // ---- ball logic
    if (ball_ == Ball::Held) {
        holdT_ += dt;
        const bool onBeat = audio_.bpm > 0.0f && am > 0.3f;
        const bool ready = onBeat ? ((audio_.beatHit && holdT_ > 0.7f && rnd() < 0.6f) || holdT_ > 2.5f) : holdT_ > holdMin_;
        if (ready || (forceShot_ && holdT_ > 0.3f)) {
            decide();
            holdMin_ = 1.0f + rnd() * 1.6f;
        }
    }
    else if (ball_ == Ball::Pass) {
        ballT_ += dt / ballDur_;
        const Point to = players_[static_cast<size_t>(receiver_)].pos;
        const float e = std::min(1.0f, ballT_);
        ballPos_ = {from_.x + (to.x - from_.x) * e, from_.y + (to.y - from_.y) * e};
        if (ballT_ >= 1.0f) {
            holder_ = receiver_;
            ball_ = Ball::Held;
            holdT_ = 0.0f;
        }
    }
    else if (ball_ == Ball::Shot) {
        ballT_ += dt / ballDur_;
        const float e = std::min(1.0f, ballT_);
        ballPos_ = {from_.x + (to_.x - from_.x) * e, from_.y + (to_.y - from_.y) * e};
        if (ballT_ >= 1.0f) {
            if (goal_) {
                ball_ = Ball::Goal;
                celebrate_ = 3.5f;
                netRipple_ = 1.0f;
                flashBoost_ = 1.0f;
            }
            else {
                // Keeper of the defending side gathers it.
                const int defending = players_[static_cast<size_t>(holder_)].team == 0 ? 1 : 0;
                for (size_t i = 0; i < players_.size(); ++i)
                    if (players_[i].team == defending && players_[i].keeper)
                        holder_ = static_cast<int>(i);
                ball_ = Ball::Held;
                holdT_ = 0.0f;
                holdMin_ = 1.2f;
            }
        }
    }
    else if (ball_ == Ball::Goal) {
        celebrate_ -= dt;
        if (celebrate_ <= 0.0f)
            kickoff(players_[static_cast<size_t>(holder_)].team == 0 ? 1 : 0);
    }
    if (ball_ == Ball::Held)
        ballPos_ = players_[static_cast<size_t>(holder_)].pos;

    // ---- players
    const int attacking = players_[static_cast<size_t>(holder_)].team;
    const float shiftX = (ballPos_.x - cx) * 0.45f;
    int presser = -1;
    {
        float nd = 1e9f;
        for (size_t i = 0; i < players_.size(); ++i) {
            const auto& p = players_[i];
            if (p.team == attacking || p.keeper)
                continue;
            const float d = std::pow(p.pos.x - ballPos_.x, 2.0f) + std::pow(p.pos.y - ballPos_.y, 2.0f);
            if (d < nd) {
                nd = d;
                presser = static_cast<int>(i);
            }
        }
    }
    for (size_t i = 0; i < players_.size(); ++i) {
        auto& p = players_[i];
        const int idx = static_cast<int>(i);
        Point target{p.home.x + shiftX, p.home.y + (ballPos_.y - p.home.y) * 0.25f};
        float speed = 13.0f * k;
        if (p.keeper) {
            const Point g = goal_center(p.team == 0 ? 1 : 0);
            target = {g.x + (p.team == 0 ? 2.5f : -2.5f) * k, std::clamp(ballPos_.y, cy - (y1_ - y0_) * 0.14f, cy + (y1_ - y0_) * 0.14f)};
            if (ball_ == Ball::Shot && !goal_)
                target.y = to_.y, speed = 30.0f * k;
        }
        else if (ball_ == Ball::Goal) {
            // Scorers run to the corner flag, the others trudge back.
            const bool scorers = p.team == players_[static_cast<size_t>(holder_)].team;
            target = scorers ? Point{p.team == 0 ? x1_ - 4 * k : x0_ + 4 * k, y0_ + 4 * k + (idx % 3) * 3 * k} : p.home;
            speed = scorers ? 16.0f * k : 6.0f * k;
        }
        else if (idx == holder_ && ball_ == Ball::Held) {
            const Point g = goal_center(p.team);
            target = {g.x, p.pos.y + std::sin(t_ * 0.9f + idx) * 8 * k};
            speed = 9.0f * k;
        }
        else if (idx == receiver_ && ball_ == Ball::Pass) {
            target = ballPos_;
            speed = 10.0f * k;
        }
        else if (idx == presser && ball_ == Ball::Held) {
            const Point& b = ballPos_;
            const float dir = attacking == 0 ? 1.0f : -1.0f;
            target = {b.x + dir * 3.5f * k, b.y};
            speed = 11.0f * k;
        }
        target.x = std::clamp(target.x, x0_ + 1 * k, x1_ - 1 * k);
        target.y = std::clamp(target.y, y0_ + 1 * k, y1_ - 1 * k);
        const float dx = target.x - p.pos.x, dy = target.y - p.pos.y;
        const float d = std::sqrt(dx * dx + dy * dy);
        if (d > 0.2f * k) {
            const float step = std::min(d, speed * dt);
            p.pos.x += dx / d * step;
            p.pos.y += dy / d * step;
            p.stride += step / (2.5f * k);
        }
    }
    if (ball_ == Ball::Held)
        ballPos_ = players_[static_cast<size_t>(holder_)].pos;

    // ---- stands: crowd with a travelling wave and camera flashes
    wave_ += dt * (0.12f + 0.25f * audio_.level * am);
    const float waveX = fract(wave_) * (W + 40 * k) - 20 * k;
    waveWeight_.resize(static_cast<size_t>(cv_.w));
    for (int x = 0; x < cv_.w; ++x) {
        const float u = (x - waveX) / (7 * k);
        waveWeight_[static_cast<size_t>(x)] = std::exp(-u * u);
    }
    auto crowd = [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y)
            for (int x = (y & 1); x < cv_.w; x += 2) {
                const uint32_t id = static_cast<uint32_t>(y * 977 + x * 131);
                const float hsh = hash01(id);
                Rgb c = hsh < 0.35f ? Rgb{0.85f, 0.20f, 0.18f} : hsh < 0.7f ? Rgb{0.25f, 0.40f, 0.90f} : Rgb{0.70f, 0.66f, 0.60f};
                const float w = waveWeight_[static_cast<size_t>(x)];
                const float bounce = 0.18f + 0.10f * hash01(id + 9) + 0.55f * w + 0.30f * audio_.beat * am * hash01(id + 3);
                cv_.blend(x, y, scale(c, std::min(1.0f, bounce)), 0.9f);
            }
    };
    crowd(0, static_cast<int>(y0_ - 6 * k));
    crowd(static_cast<int>(y1_ + 6 * k), cv_.h);

    const float flashRate = 0.6f + 6.0f * audio_.onset * am + 25.0f * flashBoost_;
    flashBoost_ = std::max(0.0f, flashBoost_ - rdt * 0.5f);
    if (rnd() < flashRate * rdt) {
        const bool top = rnd() < 0.5f;
        const int y = top ? static_cast<int>(rnd() * (y0_ - 6 * k)) : static_cast<int>(y1_ + 6 * k + rnd() * (H - y1_ - 6 * k));
        flashes_.push_back({static_cast<int>(rnd() * W), y, 1.0f});
    }
    for (auto& f : flashes_) {
        f.life -= rdt * 6.0f;
        cv_.add(f.x, f.y, {1, 1, 1}, std::max(0.0f, f.life));
        cv_.add(f.x + 1, f.y, {1, 1, 1}, std::max(0.0f, f.life) * 0.4f);
        cv_.add(f.x - 1, f.y, {1, 1, 1}, std::max(0.0f, f.life) * 0.4f);
    }
    std::erase_if(flashes_, [](const Flash& f) { return f.life <= 0.0f; });

    // Floodlight towers in the corners.
    const float lamp = 0.35f + 0.3f * audio_.bass * am + 0.5f * audio_.drop * am + 0.3f * flashBoost_;
    for (float lx : {2.5f * k, W - 2.5f * k})
        for (float ly : {2.5f * k, H - 2.5f * k}) {
            cv_.glow(lx, ly, 14 * k, {1.0f, 0.95f, 0.80f}, 0.35f * lamp);
            cv_.ellipse(lx, ly, 1.6f * k, 1.6f * k, 0, {1.0f, 0.98f, 0.9f});
        }

    // Net ripple after a goal.
    netRipple_ = std::max(0.0f, netRipple_ - rdt * 0.8f);
    if (netRipple_ > 0.0f) {
        const float gx = netSide_ == 1 ? x1_ : x0_, dir = netSide_ == 1 ? 1.0f : -1.0f;
        for (int i = 0; i < 6; ++i) {
            const float y = cy + (i - 2.5f) * 2.2f * k;
            const float bulge = netRipple_ * (1.5f + std::sin(t_ * 14.0f + i)) * k;
            cv_.capsule(gx + dir * 1.0f * k, y, 0.3f, gx + dir * (2.5f * k + bulge), y, 0.3f, {1, 1, 1}, 0.7f * netRipple_);
        }
    }

    // ---- weather on the pitch, under the players (seen from above, so the
    // players stay crisp and the haze, wet sheen and gusts lie on the grass).
    {
        const auto& Lw = env();
        // Fog drifts in uneven banks.
        if (Lw.fog > 0.02f) {
            const Rgb haze = from_env(Lw.haze);
            for (int y = 0; y < cv_.h; ++y)
                for (int x = 0; x < cv_.w; ++x) {
                    const float n = 0.5f + 0.5f * std::sin(x * 0.045f + t_ * 0.15f + 1.7f * std::sin(y * 0.03f)) *
                                               std::sin(y * 0.06f - t_ * 0.11f);
                    cv_.blend(x, y, haze, Lw.fog * (0.12f + 0.45f * n * n));
                }
        }
        // Storm gusts sweep darker sheets of rain across the pitch.
        if (Lw.storm > 0.05f)
            for (int y = 0; y < cv_.h; ++y)
                for (int x = 0; x < cv_.w; ++x) {
                    const float band = std::sin((x + y * 0.45f) * 0.07f - t_ * 2.6f);
                    if (band > 0.2f)
                        cv_.blend(x, y, {0.04f, 0.06f, 0.09f}, 0.45f * Lw.storm * (band - 0.2f));
                }
        // Raindrops splash on the grass as small expanding rings.
        if (Lw.rain > 0.05f) {
            const int n = static_cast<int>(26.0f * Lw.rain * k * k);
            for (int i = 0; i < n; ++i) {
                const float rate = 1.4f + hash01(static_cast<uint32_t>(i * 7 + 1));
                const float cyc = t_ * rate + hash01(static_cast<uint32_t>(i * 13 + 5));
                const auto gen = static_cast<uint32_t>(cyc);
                const float ph = cyc - static_cast<float>(gen);
                const float sx = x0_ + (x1_ - x0_) * hash01(gen * 31u + static_cast<uint32_t>(i) * 977u);
                const float sy = y0_ + (y1_ - y0_) * hash01(gen * 17u + static_cast<uint32_t>(i) * 613u + 3u);
                const float r = (0.3f + 1.2f * ph) * k;
                const float a = 0.32f * (1.0f - ph);
                for (int j = 0; j < 8; ++j) {
                    const float ang = j * kPi * 0.25f;
                    cv_.blend(static_cast<int>(sx + std::cos(ang) * r), static_cast<int>(sy + std::sin(ang) * r),
                              {0.80f, 0.88f, 1.0f}, a);
                }
            }
        }
    }

    // ---- draw players and the ball
    static constexpr Rgb kShirt[] = {{0.90f, 0.18f, 0.16f}, {0.20f, 0.45f, 0.95f}};
    static constexpr Rgb kKeeper[] = {{0.98f, 0.85f, 0.15f}, {0.25f, 0.85f, 0.35f}};
    // The keeper facing a shot that will be saved dives across the goal.
    const int shooterTeam = players_[static_cast<size_t>(holder_)].team;
    const float dive = ball_ == Ball::Shot && !goal_ ? smoothstep(0.45f, 0.9f, ballT_) : 0.0f;
    const auto& Le = env();
    const float ps = 1.3f * k;  // player size
    for (const auto& p : players_) {
        const Rgb shirt = p.keeper ? kKeeper[p.team] : kShirt[p.team];
        const float legs = std::sin(p.stride * kPi) * 1.2f * k;
        const float face = p.team == 0 ? 1.0f : -1.0f;
        if (p.keeper && p.team != shooterTeam && dive > 0.0f) {
            const float dir = to_.y >= p.pos.y ? 1.0f : -1.0f;
            const float cy2 = p.pos.y + dir * 1.5f * k * dive;
            cv_.ellipse(p.pos.x + 0.9f * k, cy2 + 1.1f * k, 1.6f * k, (2.0f + 1.8f * dive) * k, 0, {0.02f, 0.08f, 0.03f}, 0.45f);
            cv_.ellipse(p.pos.x, cy2, 1.3f * k, (2.0f + 1.8f * dive) * k, 0, shirt);  // stretched full length
            cv_.ellipse(p.pos.x, cy2 + dir * (2.6f * dive) * k, 0.95f * k, 0.95f * k, 0, {0.20f, 0.13f, 0.09f});
            continue;
        }
        // Four faint shadows, one away from each corner floodlight.
        for (const float sx : {-1.0f, 1.0f})
            for (const float sy : {-1.0f, 1.0f}) {
                const float ox = (p.pos.x < cx ? 1.0f : 0.6f) * sx, oy = (p.pos.y < cy ? 1.0f : 0.6f) * sy;
                cv_.ellipse(p.pos.x + ox * 2.0f * ps, p.pos.y + oy * 2.0f * ps, 1.9f * ps, 1.1f * ps, sx * sy * 0.7f,
                            {0.02f, 0.06f, 0.03f}, 0.22f);
            }
        cv_.capsule(p.pos.x - face * 0.5f * ps, p.pos.y - 0.8f * ps, 0.5f * ps, p.pos.x + legs * 1.3f, p.pos.y - 0.8f * ps, 0.5f * ps, {0.1f, 0.1f, 0.12f});
        cv_.capsule(p.pos.x - face * 0.5f * ps, p.pos.y + 0.8f * ps, 0.5f * ps, p.pos.x - legs * 1.3f, p.pos.y + 0.8f * ps, 0.5f * ps, {0.1f, 0.1f, 0.12f});
        cv_.ellipse(p.pos.x, p.pos.y, 1.5f * ps, 2.05f * ps, 0, shirt);
        cv_.ellipse(p.pos.x, p.pos.y, 0.95f * ps, 0.95f * ps, 0, {0.20f, 0.13f, 0.09f});
    }
    // Ball: lofted a little on long passes and shots, with a ground shadow.
    float hgt = 0.0f;
    if (ball_ == Ball::Pass || ball_ == Ball::Shot)
        hgt = std::sin(std::min(1.0f, ballT_) * kPi) * std::min(4.0f, ballDur_ * 3.0f) * k;
    Point bp = ballPos_;
    if (ball_ == Ball::Held) {
        const float face = players_[static_cast<size_t>(holder_)].team == 0 ? 1.0f : -1.0f;
        bp.x += face * 2.0f * k;
        bp.y += 0.8f * k;
    }
    cv_.ellipse(bp.x + 0.5f * k, bp.y + 0.6f * k, 1.3f * k, 0.8f * k, 0, {0.02f, 0.08f, 0.03f}, 0.5f);
    // A dark rim keeps the ball apart from lines and shirts; orange on snow.
    const float br = 1.7f * k + hgt * 0.08f;
    cv_.ellipse(bp.x, bp.y - hgt, br + 0.6f * k, br + 0.6f * k, 0, {0.05f, 0.05f, 0.06f}, 0.8f);
    cv_.ellipse(bp.x, bp.y - hgt, br, br, 0, mix({1.0f, 1.0f, 1.0f}, {1.0f, 0.55f, 0.12f}, env().snow_cover));

    // Floodlights on a wet pitch shine back; in fog their beams glow out.
    for (float lx : {2.5f * k, W - 2.5f * k})
        for (float ly : {2.5f * k, H - 2.5f * k}) {
            if (Le.wetness > 0.05f)
                cv_.glow(lx + (cx - lx) * 0.22f, ly + (cy - ly) * 0.22f, 22 * k, {0.85f, 0.90f, 1.0f}, 0.26f * Le.wetness);
            if (Le.fog > 0.05f)
                cv_.glow(lx + (cx - lx) * 0.12f, ly + (cy - ly) * 0.12f, 42 * k, {1.0f, 0.97f, 0.88f}, 0.30f * Le.fog);
        }
    paint_weather(rdt, H * 0.5f);
    present(canvas);
    wait_until_next_frame();
    return true;
}

Scenes::SceneDescriptor SoccerScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.family = "sports";
    d.tags = {"sports", "soccer", "football", "stadium", "night", "audio-reactive"};
    d.intensity = 0.45f;
    d.motion = 0.55f;
    d.music_affinity = 0.5f;
    d.performance_cost = 0.35f;
    environment_->add_variants(d);
    return d;
}
