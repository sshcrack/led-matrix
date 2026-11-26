#include "RainScene.h"

#include <algorithm>
#include <cmath>

using namespace AudioKit;

namespace AmbientScenes {
namespace {
constexpr float MAX_STEP = 0.1f;
// Rows where the puddle starts (fraction of the height); drops land below it,
// nearer drops lower down.
constexpr float PUDDLE_TOP = 0.80f;
constexpr float FALL_SPEED = 95.0f;  // px/s for a near drop at speed 1
constexpr float RIPPLE_LIFE = 0.9f;
constexpr float DROPLET_LIFE = 0.35f;
constexpr float GRAVITY = 160.0f;
// A street lamp above the top edge lights a cone of rain.
constexpr float LAMP_X = 0.62f;
constexpr float LAMP_Y = 0.13f;
}  // namespace

RainScene::RainScene() = default;

void RainScene::register_properties()
{
    density_->label("Density").description("Number of raindrops in view.").group("Rain");
    speed_->label("Fall speed").description("How fast the rain falls.").group("Rain").step(0.05);
    wind_->label("Wind").description("Slant of the rain; negative leans left.").group("Rain").step(0.05);
    splashes_->label("Splashes").description("Drops splash and ripple on the puddle.").group("Rain");
    look_palette_->label("Look palette").group("Appearance");
    look_brightness_->label("Look brightness").group("Appearance").step(0.05);
    add_property(density_);
    add_property(speed_);
    add_property(wind_);
    add_property(splashes_);
    glass_drops_->label("Drops on glass").description("A few drops run slowly down the window in front of the rain.").group("Rain");
    add_property(glass_drops_);
    add_property(look_palette_);
    add_property(look_brightness_);
}

float RainScene::ground_y(float z) const
{
    const float h = static_cast<float>(matrix_height);
    return h * PUDDLE_TOP + z * (h * (1.0f - PUDDLE_TOP) - 2.0f);
}

RGB RainScene::tint(float position, float brightness) const
{
    const auto palette = look_palette_->get().get();
    if (palette != Ambiance::Palette::ORIGINAL) {
        // Palette looks are dim by design; lift them less so drops stay visible.
        brightness = std::clamp(brightness, 0.0f, 1.0f) * std::sqrt(look_brightness_->get());
        const auto c = Ambiance::sample(palette, position, 1.0f);
        return RGB{static_cast<float>(c.r), static_cast<float>(c.g), static_cast<float>(c.b)} * (1.4f * brightness);
    }
    brightness = std::clamp(brightness, 0.0f, 1.0f) * look_brightness_->get();
    static constexpr RGB dark{14, 20, 34}, mid{78, 104, 140}, light{205, 220, 240};
    position = std::clamp(position, 0.0f, 1.0f);
    const RGB c = position < 0.5f ? lerp(dark, mid, position * 2) : lerp(mid, light, position * 2 - 1);
    return c * brightness;
}

void RainScene::spawn(Drop& d, bool anywhere)
{
    d.z = rnd();
    d.z *= d.z;  // more distant drops than near ones
    const float w = static_cast<float>(matrix_width);
    // Wind carries drops sideways, so start them upwind to keep the panel covered.
    const float slant = wind_->get() * 0.35f * ground_y(d.z);
    d.x = rnd() * (w + std::abs(slant)) - std::max(0.0f, slant);
    d.y = anywhere ? rnd() * ground_y(d.z) : -rnd() * 30.0f;
}

float RainScene::light_at(float x, float y) const
{
    const int ix = static_cast<int>(x), iy = static_cast<int>(y);
    if (ix < 0 || iy < 0 || ix >= matrix_width || iy >= matrix_height)
        return 0.0f;
    return light_[static_cast<size_t>(iy * matrix_width + ix)];
}

void RainScene::build_backdrop()
{
    const int w = matrix_width, h = matrix_height;
    backdrop_.assign(static_cast<size_t>(w * h), {});
    light_.assign(static_cast<size_t>(w * h), 0.0f);
    const auto palette = look_palette_->get().get();
    if (palette == Ambiance::Palette::ORIGINAL) {
        lamp_ = RGB{255, 178, 105} * look_brightness_->get();
    }
    else {
        // Dim looks keep the lamp a little brighter than the rest, so the scene keeps its focus.
        const auto c = Ambiance::sample(palette, 1.0f, 1.0f);
        lamp_ = {static_cast<float>(c.r), static_cast<float>(c.g), static_cast<float>(c.b)};
    }
    const float top = h * PUDDLE_TOP;
    const float lx = w * LAMP_X;
    const float ly = h * LAMP_Y;
    // Palette looks are dim overall, so their lamp light carries more of the picture.
    const bool dim = palette != Ambiance::Palette::ORIGINAL;
    const float glow = dim ? 0.55f : 0.11f;
    const float pool_half = 2.5f + std::max(0.0f, top - ly) * 0.6f;
    const float pool_reach = std::exp(-(top - ly) / (h * 0.7f));
    for (int y = 0; y < h; ++y) {
        // The cone widens downwards from the lamp head and fades with distance.
        const float py = y + 0.5f;
        const float below = py - ly;
        const float half = 2.5f + std::max(0.0f, below) * 0.6f;
        const float reach = below < 0 ? std::exp(-below * below * 0.08f) * 0.5f : std::exp(-below / (h * 0.7f));
        for (int x = 0; x < w; ++x) {
            const float dx = (x + 0.5f - lx) / half;
            const float cone = std::exp(-dx * dx * 1.4f) * reach;
            if (py < top)
                light_[static_cast<size_t>(y * w + x)] = cone;

            float level;
            float position = 0.05f;
            if (y < top) {
                // Dark sky with a faint mist just above the puddle.
                const float mist = smoothstep(top - h * 0.22f, top, y + 0.5f);
                // Dim looks let the mist rise into the wet street, so the street has no hard top edge.
                level = 0.06f + (dim ? 0.22f : 0.10f) * mist;
                position = 0.05f + 0.25f * mist;
            }
            else {
                // Puddle: a dim sheen that brightens towards the far edge, broken by a slow pattern.
                const float depth = (y - top) / (h - top);
                const float sheen = 0.5f + 0.5f * std::sin(x * 0.21f + depth * 9.0f) * std::sin(x * 0.07f - depth * 4.0f);
                // Dim looks light the wet street more, so the lower part of the picture carries detail.
                level = (dim ? 0.30f : 0.13f) * (1.0f - depth * 0.6f) * (0.7f + 0.3f * sheen);
                position = 0.2f + 0.2f * (1.0f - depth);
            }
            RGB c = tint(position, level);
            if (py < top) {
                c = c + lamp_ * (glow * cone);  // the lamp's glow in the wet air
            }
            else {
                // Reflected lamp light on the puddle, strongest straight below it.
                // The cone continues onto the wet ground as a pool, plus a narrow reflection streak.
                const float pool = (x + 0.5f - lx) / (pool_half + (py - top) * 0.5f);
                const float near = (py - top) / (h - top);
                const float streak = (x + 0.5f - lx) / (2.0f + near * 5.0f);
                c = c + lamp_ * (glow * pool_reach * std::exp(-pool * pool * 1.4f) +
                                 (dim ? 0.22f : 0.09f) * (1.0f - 0.5f * near) * std::exp(-streak * streak));
            }
            if (dim) {
                // A far light on the left, out of focus behind the rain, and its blur on the wet street.
                const float fx = x + 0.5f - w * 0.17f, fy = py - h * 0.44f;
                // Out-of-focus lights become discs: a soft edge a little brighter than the middle.
                const float fd = len(fx, fy) / (h * 0.065f);
                const float far = smoothstep(1.15f, 0.85f, fd) * (0.65f + 0.35f * smoothstep(0.4f, 0.95f, fd));
                const float ry = py - top;
                const float far_reflection = ry > 0 ? std::exp(-fx * fx / (h * h * 0.002f)) * std::exp(-ry / (h * 0.08f)) : 0.0f;
                c = c + lamp_ * (0.34f * far + 0.08f * far_reflection);
            }
            backdrop_[static_cast<size_t>(y * w + x)] = c;
        }
    }
    draw_lamp_post();
}

void RainScene::draw_lamp_post()
{
    // Post to the right of the head with an arm across; its lamp-facing edge
    // catches a little light. Drawn straight into the cached backdrop.
    const int w = matrix_width, h = matrix_height;
    const float lx = w * LAMP_X, ly = h * LAMP_Y;
    const int post = static_cast<int>(lx + w * 0.085f);
    // Seen from the window it runs down out of the frame (dim looks), or stands out in the street.
    const bool dim = look_palette_->get().get() != Ambiance::Palette::ORIGINAL;
    const int foot = dim ? h : static_cast<int>(h * 0.9f);
    const RGB dark{3, 4, 6};
    // In dim looks the lit side of the post and the arm are brighter, so they read against the haze.
    const float side = dim ? 0.30f : 0.10f, arm = dim ? 0.24f : 0.04f;
    auto put = [&](int x, int y, const RGB& c) {
        if (x >= 0 && y >= 0 && x < w && y < h)
            backdrop_[static_cast<size_t>(y * w + x)] = c;
    };
    for (int y = static_cast<int>(ly) - 3; y < foot; ++y) {
        put(post, y, dark + lamp_ * side);
        put(post + 1, y, dark);
    }
    for (int x = static_cast<int>(lx) - 2; x <= post; ++x) put(x, static_cast<int>(ly) - 3, dark + lamp_ * arm);
    // Hood over the bulb.
    for (int x = static_cast<int>(lx) - 4; x <= static_cast<int>(lx) + 3; ++x) {
        put(x, static_cast<int>(ly) - 2, dark + lamp_ * 0.12f);
        if (x > static_cast<int>(lx) - 4 && x < static_cast<int>(lx) + 3)
            put(x, static_cast<int>(ly) - 1, dark + lamp_ * 0.25f);
    }
    // Bulb and its halo.
    for (int y = static_cast<int>(ly) - 8; y <= static_cast<int>(ly) + 8; ++y)
        for (int x = static_cast<int>(lx) - 9; x <= static_cast<int>(lx) + 9; ++x) {
            if (x < 0 || y < 0 || x >= w || y >= h)
                continue;
            const float d2 = (x + 0.5f - lx) * (x + 0.5f - lx) + (y + 0.5f - ly - 0.5f) * (y + 0.5f - ly - 0.5f);
            auto& p = backdrop_[static_cast<size_t>(y * w + x)];
            p = p + lamp_ * (0.9f * std::exp(-d2 * 0.9f) + 0.16f * std::exp(-d2 * 0.05f));
        }
}

void RainScene::initialize(int width, int height)
{
    Scene::initialize(width, height);
    buf_.resize(width, height);
    drops_.assign(static_cast<size_t>(density_->get()), {});
    for (auto& d : drops_) spawn(d, true);
    droplets_.clear();
    ripples_.clear();
    beads_.assign(5, {});
    for (auto& b : beads_) spawn_bead(b, true);
    time_ = 0;
    build_backdrop();
}

bool RainScene::render(rgb_matrix::FrameCanvas* canvas)
{
    const float dt = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, MAX_STEP);
    time_ += dt;

    const size_t wanted = static_cast<size_t>(std::max(1.0f, density_->get() * (0.55f + 0.45f * render_quality_scale())));
    if (drops_.size() != wanted) {
        const size_t old = drops_.size();
        drops_.resize(wanted);
        for (size_t i = old; i < drops_.size(); ++i) spawn(drops_[i], true);
    }

    std::copy(backdrop_.begin(), backdrop_.end(), buf_.px.begin());

    const float speed = speed_->get();
    // Now and then a gust leans the rain further for a few seconds.
    const float gust = std::pow(0.5f + 0.5f * std::sin(time_ * 0.21f), 6.0f);
    const float slant = wind_->get() * 0.35f * (1.0f + 0.8f * gust);
    // Streaks grow with speed, like a camera shutter would smear them.
    const float smear = std::max(0.85f, std::sqrt(speed));
    // Dense rain would sparkle all over the puddle; let only the nearest drops splash.
    const float splash_depth = std::clamp(0.3f + (static_cast<float>(drops_.size()) - 120.0f) / 500.0f, 0.3f, 0.78f);

    // Dim looks draw the rain a little longer and brighter, so it shows outside the lamp light too.
    const float rain_gain = look_palette_->get().get() != Ambiance::Palette::ORIGINAL ? 1.4f : 1.0f;
    int splashes_this_frame = 0;  // staggers impacts so the ground never sparkles all at once
    for (auto& d : drops_) {
        const float v = FALL_SPEED * speed * (0.45f + 0.8f * d.z);
        d.y += v * dt;
        d.x += v * slant * dt;
        const float gy = ground_y(d.z);
        if (d.y >= gy) {
            if (splashes_->get() && d.z > splash_depth && splashes_this_frame++ < 2 && d.x >= 0 && d.x < matrix_width) {
                const int n = d.z > 0.7f ? 3 : 2;
                for (int i = 0; i < n; ++i) {
                    const float side = (i % 2 ? 1.0f : -1.0f) * (0.4f + rnd() * 0.6f);
                    // Droplets jump up more than sideways, so a splash reads as a small crown.
                    droplets_.push_back({d.x, gy - 0.5f, side * (8.0f + 12.0f * d.z) + v * slant * 0.3f,
                                         -(30.0f + 32.0f * d.z) * (0.6f + rnd() * 0.5f), DROPLET_LIFE, d.z});
                }
                ripples_.push_back({d.x, gy, 0.0f, d.z});
            }
            spawn(d, false);
            continue;
        }

        // Streak: bright head, tail fading upwards, drawn with sub-pixel splats.
        // Far rain stays short and faint; only near drops draw long streaks.
        const float length = (1.5f + 12.0f * d.z * d.z) * smear * rain_gain;
        const float brightness = 0.12f + 0.68f * d.z * d.z + 0.1f * d.z;
        const float lit = light_at(d.x, d.y);
        const RGB head = tint(0.55f + 0.45f * d.z, brightness * (0.75f - 0.35f * lit) * rain_gain) + lamp_ * (lit * (0.30f + 0.5f * d.z));
        const int samples = std::max(2, static_cast<int>(length * 1.6f));
        for (int i = 0; i < samples; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(samples - 1);
            const float y = d.y - t * length;
            if (y < 0 || y >= gy)
                continue;
            const float fade = (1.0f - t) * (1.0f - t);
            buf_.splat(d.x - t * length * slant - 0.5f, y - 0.5f, head * fade);
        }
    }

    const float top_of_puddle = matrix_height * PUDDLE_TOP;
    // Dim looks would lose their splashes against the lit puddle; lift them a little.
    const float splash_gain = look_palette_->get().get() != Ambiance::Palette::ORIGINAL ? 1.2f : 1.0f;
    for (auto it = droplets_.begin(); it != droplets_.end();) {
        it->life -= dt;
        if (it->life <= 0) {
            it = droplets_.erase(it);
            continue;
        }
        it->vy += GRAVITY * dt;
        it->x += it->vx * dt;
        it->y += it->vy * dt;
        // Fade in over the first frames so a splash spreads instead of flashing.
        const float a = it->life / DROPLET_LIFE * std::min(1.0f, (DROPLET_LIFE - it->life) / 0.08f);
        const float lit = std::max(light_at(it->x, top_of_puddle - 1.0f), 0.0f);
        buf_.splat(it->x - 0.5f, it->y - 0.5f, (tint(0.75f, 0.22f + 0.3f * it->z) + lamp_ * (0.45f * lit)) * (a * splash_gain));
        ++it;
    }

    for (auto it = ripples_.begin(); it != ripples_.end();) {
        it->age += dt;
        if (it->age >= RIPPLE_LIFE) {
            it = ripples_.erase(it);
            continue;
        }
        const float k = it->age / RIPPLE_LIFE;
        const float rx = 1.0f + k * (5.0f + 7.0f * it->z);
        const float ry = rx * (0.22f + 0.12f * it->z);  // flattened by perspective
        const float lit = light_at(it->x, top_of_puddle - 1.0f);
        const RGB c =
            (tint(0.7f, 0.2f + 0.3f * it->z) + lamp_ * (0.6f * lit)) * ((1.0f - k) * (1.0f - k) * (splash_gain > 1.0f ? 0.8f : 1.0f));
        const int points = std::max(8, static_cast<int>(rx * 5.0f));
        for (int i = 0; i < points; ++i) {
            const float a = Tau * static_cast<float>(i) / static_cast<float>(points);
            buf_.splat(it->x + std::cos(a) * rx - 0.5f, it->y + std::sin(a) * ry - 0.5f, c * (9.0f / static_cast<float>(points)));
        }
        ++it;
    }

    if (look_palette_->get().get() != Ambiance::Palette::ORIGINAL) {
        // The lamp's reflection on the wet street breaks into short shimmering dashes.
        const float lx = matrix_width * LAMP_X;
        for (int y = static_cast<int>(top_of_puddle) + 1; y < matrix_height; y += 2) {
            const float near = (y - top_of_puddle) / (matrix_height - top_of_puddle);
            const float n = hash(static_cast<uint32_t>(y) * 2654435761u ^ static_cast<uint32_t>(time_ * 5.0f));
            const float shift =
                (hash(static_cast<uint32_t>(y) * 40503u ^ static_cast<uint32_t>(time_ * 3.0f)) - 0.5f) * (1.0f + 3.0f * near);
            const float half = 0.8f + 2.2f * near * n;
            for (float x = lx + shift - half; x <= lx + shift + half; x += 0.5f)
                buf_.splat(x - 0.5f, y - 0.5f, lamp_ * (0.05f * (0.4f + 0.6f * n) * (1.0f - 0.4f * near)));
        }
    }

    if (glass_drops_->get())
        draw_beads(dt);

    buf_.blit(canvas);
    wait_until_next_frame();
    return true;
}

void RainScene::spawn_bead(Bead& b, bool anywhere)
{
    b.x = 3.0f + rnd() * (matrix_width - 6.0f);
    b.y = anywhere ? rnd() * matrix_height : -4.0f - rnd() * 40.0f;
    b.r = (1.3f + rnd() * rnd() * 2.6f) * static_cast<float>(matrix_height) / 64.0f;
    b.stretch = 1.0f + rnd() * 0.3f;  // drops on glass hang slightly taller than wide
    b.speed = (4.0f + rnd() * 6.0f) * static_cast<float>(matrix_height) / 64.0f;
    b.pause = rnd() * 1.0f;
}

void RainScene::draw_beads(float dt)
{
    for (auto& b : beads_) {
        // Drops on glass stick, then slide on: stop-and-go at their own pace.
        if (b.pause > 0) {
            b.pause -= dt;
        }
        else {
            b.y += b.speed * dt;
            if (rnd() < dt * 0.12f)
                b.pause = 0.4f + rnd() * 1.2f;
        }
        if (b.y - b.r > matrix_height + 2)
            spawn_bead(b, false);
        // A broken wet track above the drop, then the drop as a small lens: it shows the street
        // behind it upside down, catches the lamp as a crescent on the lamp side and has a soft
        // shadow edge only on the far side.
        const int track = static_cast<int>(b.r * 5.0f);
        for (int i = 1; i < track; ++i) {
            if (hash(static_cast<uint32_t>(i) * 2246822519u ^ static_cast<uint32_t>(b.x * 131.0f)) < 0.35f)
                continue;
            buf_.splat(b.x - 0.5f, b.y - b.r * b.stretch - i - 0.5f,
                       tint(0.8f, 0.08f * (1.0f - static_cast<float>(i) / static_cast<float>(track))));
        }
        float to_lamp_x = matrix_width * LAMP_X - b.x, to_lamp_y = matrix_height * LAMP_Y - b.y;
        const float to_lamp = std::max(0.001f, len(to_lamp_x, to_lamp_y));
        to_lamp_x /= to_lamp;
        to_lamp_y /= to_lamp;
        const float ry = b.r * b.stretch;
        const int reach_x = static_cast<int>(std::ceil(b.r + 1.5f)), reach_y = static_cast<int>(std::ceil(ry + 1.5f));
        for (int y = static_cast<int>(b.y) - reach_y; y <= static_cast<int>(b.y) + reach_y; ++y)
            for (int x = static_cast<int>(b.x) - reach_x; x <= static_cast<int>(b.x) + reach_x; ++x) {
                if (x < 0 || y < 0 || x >= matrix_width || y >= matrix_height)
                    continue;
                const float ex = (x + 0.5f - b.x) / b.r, ey = (y + 0.5f - b.y) / ry;
                const float d = len(ex, ey);
                if (d > 1.6f)
                    continue;
                const float facing = (ex * to_lamp_x + ey * to_lamp_y) / std::max(d, 0.001f);  // +1 lamp side
                auto& p = buf_.at(x, y);
                const float edge = std::exp(-(d - 1.0f) * (d - 1.0f) * 8.0f);
                p = p * (1.0f - 0.4f * edge * std::max(0.0f, -facing));
                const float inside = smoothstep(1.1f, 0.6f, d);
                const int sx = std::clamp(static_cast<int>(b.x - ex * b.r * 2.2f), 0, matrix_width - 1);
                const int sy = std::clamp(static_cast<int>(b.y - ey * ry * 2.2f), 0, matrix_height - 1);
                const RGB behind = backdrop_[static_cast<size_t>(sy * matrix_width + sx)];
                const float crescent = smoothstep(0.55f, 0.9f, d) * smoothstep(1.1f, 0.95f, d) * smoothstep(0.2f, 0.8f, facing);
                p = p + (behind * 1.2f + tint(0.75f, 0.12f)) * inside + (lamp_ * 0.45f + tint(1.0f, 0.25f)) * crescent;
            }
    }
}

Scenes::SceneDescriptor RainScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "rain";
    d.tags = {"ambient", "rain", "calm", "fits:rain", "fits:storm", "fits:cloudy", "flow"};
    d.intensity = 0.36f;
    d.motion = 0.62f;
    d.music_affinity = 0.18f;
    d.performance_cost = 0.30f;
    d.variants = {
        {"mist",
         "Light rain",
         "Sparse, slow drops with soft splashes",
         {{"density", 70}, {"speed", 0.7f}, {"wind", 0.08f}},
         {"calm", "flow"},
         0.24f,
         0.44f,
         0.10f,
         0.22f},
        {"downpour",
         "Downpour",
         "Dense, fast rain driven by the wind",
         {{"density", 330}, {"speed", 1.6f}, {"wind", 0.4f}},
         {"dense", "energetic"},
         0.62f,
         0.84f,
         0.24f,
         0.40f},
        {"window",
         "Rainy window",
         "Slow blue rain with a compact, low-brightness palette",
         {{"look_palette", "RAIN"}, {"look_brightness", 0.55f}, {"density", 90}, {"speed", 0.55f}, {"wind", 0.12f}, {"glass_drops", true}},
         {"calm", "rain", "blue"},
         0.22f,
         0.32f,
         0.0f,
         0.22f},
    };
    Scenes::set_off_by_default(d, {"downpour", "window"});
    return d;
}

std::unique_ptr<Scenes::Scene> RainSceneWrapper::create()
{
    return std::make_unique<RainScene>();
}

}  // namespace AmbientScenes
