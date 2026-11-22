#include "NightRailwayScene.h"

namespace Hobbies {
namespace {

constexpr float Portal = 30.0f;
constexpr float Rail = 79.0f;
constexpr float CycleSeconds = 120.0f;

// Draw in a 128×128 design space. Scale the composition to the actual canvas.
struct Artwork {
    Canvas& canvas;
    float sx, sy;

    explicit Artwork(Canvas& cv) : canvas(cv), sx(cv.w / 128.0f), sy(cv.h / 128.0f) {}

    void rect(float x, float y, float width, float height, Rgb color) const
    {
        const int left = std::max(0, static_cast<int>(std::lround(x * sx)));
        const int right = std::min(canvas.w, static_cast<int>(std::lround((x + width) * sx)));
        const int top = std::max(0, static_cast<int>(std::lround(y * sy)));
        const int bottom = std::min(canvas.h, static_cast<int>(std::lround((y + height) * sy)));
        for (int row = top; row < bottom; ++row)
            for (int col = left; col < right; ++col) canvas.set(col, row, color);
    }

    void oval(float x, float y, float rx, float ry, Rgb color, float alpha = 1.0f) const
    {
        canvas.ellipse(x * sx, y * sy, rx * sx, ry * sy, 0.0f, color, alpha);
    }

    void pine(float x, float base, float height, Rgb shade, Rgb lit) const
    {
        rect(x, base - height, 1.0f, height, shade);
        for (int branch = 1; branch <= 4; ++branch) {
            const float width = height * (0.08f + 0.036f * branch);
            const float y = base - height + height * branch * 0.207f;
            const float r = std::max(0.48f, height * 0.055f) * sy;
            canvas.capsule((x - width) * sx, (y + width * 0.4f) * sy, r, x * sx, y * sy, r, shade);
            canvas.capsule(x * sx, y * sy, r, (x + width) * sx, (y + width * 0.4f) * sy, r, lit);
        }
    }
};

}  // namespace

void NightRailwayScene::initialize(int width, int height)
{
    HobbySceneBase::initialize(width, height);
    draw_landscape();
    backdrop_ = cv_.px;
}

void NightRailwayScene::draw_landscape()
{
    const Artwork art(cv_);
    fill_gradient({0.025f, 0.039f, 0.112f}, {0.21f, 0.27f, 0.44f}, {0.03f, 0.075f, 0.12f}, 0.46f);
    for (uint32_t i = 0; i < 42; ++i) {
        const float x = 2.0f + 124.0f * hash01(i * 3 + 1);
        const float y = 3.0f + 46.0f * hash01(i * 3 + 2);
        const float light = 0.2f + 0.39f * hash01(i * 3 + 3);
        art.rect(x, y, 1, 1, {light * 0.78f, light * 0.87f, light});
    }
    cv_.glow(93 * art.sx, 25 * art.sy, 19 * std::min(art.sx, art.sy), {0.22f, 0.30f, 0.51f}, 0.43f);
    art.oval(93, 25, 6, 6, {0.75f, 0.83f, 0.94f});
    art.oval(91, 25, 1.7f, 1.1f, {0.56f, 0.65f, 0.83f}, 0.65f);
    art.oval(95, 28, 1.6f, 0.9f, {0.59f, 0.69f, 0.85f}, 0.57f);

    cv_.fill_below([&](float x) { return art.sy * (72 - 33 * hill(x / art.sx, 2.8f)); }, {0.145f, 0.195f, 0.33f});
    cv_.fill_below([&](float x) { return art.sy * (74 - 24 * hill(x / art.sx, 4.9f)); }, {0.09f, 0.15f, 0.24f});
    for (int x = 4; x < 128; x += 4) {
        const float base = 79 - 15 * hill(static_cast<float>(x), 1.7f);
        art.pine(x, base, 4 + 8 * hash01(x), {0.045f, 0.104f, 0.161f}, {0.062f, 0.127f, 0.185f});
    }

    // A strip of water stays visible between the stone piers.
    art.rect(0, 87, 128, 41, {0.059f, 0.13f, 0.205f});
    for (uint32_t i = 0; i < 60; ++i) {
        const float y = 90 + 38 * hash01(4 * i);
        const float x = 38 + 76 * hash01(4 * i + 1);
        const float light = (1.0f - std::min(1.0f, std::abs(x - 93) / 58)) * (0.4f + 0.6f * hash01(4 * i + 2));
        art.rect(x, y, 2 + 9 * hash01(4 * i + 3), 1, mix({0.06f, 0.15f, 0.22f}, {0.16f, 0.29f, 0.39f}, light));
    }

    const Rgb stone{0.17f, 0.20f, 0.27f};
    const Rgb stoneEdge{0.29f, 0.32f, 0.39f};
    art.rect(26, Rail, 102, 6, stone);
    for (int pier = 36; pier < 128; pier += 22) {
        for (int row = 0; row < 9; ++row) {
            // The underside of each rounded span is open above the river.
            const float reach = 10 - std::sqrt(std::max(0.0f, 100 - (8 - row) * (8 - row) * 1.55f));
            art.rect(pier - 2 - reach, 85 + row, 5 + 2 * reach, 1, stone);
        }
        art.rect(pier - 2, 94, 5, 10, stone);
        art.rect(pier + 2, 90, 1, 13, stoneEdge);
        art.rect(pier - 3, 103, 7, 2, {0.092f, 0.126f, 0.19f});
    }
    art.rect(26, Rail, 102, 1, stoneEdge);
    art.rect(26, Rail + 3, 102, 1, {0.104f, 0.135f, 0.2f});

    // The left hillside surrounds the mouth. A small red signal marks the exit.
    for (int y = 55; y < 128; ++y) {
        const float edge = 14 + 15 * smoothstep(54, 73, y) + 11 * smoothstep(94, 128, y);
        art.rect(0, y, edge, 1, mix({0.075f, 0.142f, 0.19f}, {0.032f, 0.074f, 0.12f}, (y - 55) / 73.0f));
    }
    art.oval(23, Rail - 8, 8, 9, stoneEdge);
    art.rect(15, Rail - 8, 16, 8, stone);
    art.oval(23, Rail - 7, 6, 7, {0.021f, 0.032f, 0.064f});
    art.rect(17, Rail - 7, 13, 7, {0.021f, 0.032f, 0.064f});
    art.rect(34, Rail - 9, 1, 9, stoneEdge);
    cv_.glow(34.5f * art.sx, (Rail - 9) * art.sy, 5 * std::min(art.sx, art.sy), {0.65f, 0.12f, 0.06f}, 0.3f);
    art.rect(34, Rail - 10, 1, 2, {0.9f, 0.3f, 0.16f});

    art.pine(4, 75, 26, {0.032f, 0.077f, 0.122f}, {0.051f, 0.111f, 0.156f});
    art.pine(12, 65, 15, {0.039f, 0.086f, 0.133f}, {0.061f, 0.123f, 0.167f});
    art.pine(118, 128, 22, {0.027f, 0.062f, 0.107f}, {0.046f, 0.098f, 0.15f});
    art.pine(126, 128, 33, {0.029f, 0.065f, 0.112f}, {0.048f, 0.1f, 0.147f});
}

void NightRailwayScene::draw_train(float front)
{
    const Artwork art(cv_);
    const auto part = [&](float x, float y, float width, float height, Rgb color) {
        const float left = std::max(Portal, x);
        if (x + width > left)
            art.rect(left, Rail + y, x + width - left, height, color);
    };
    // Light the rail ahead of the engine without painting through the hillside.
    for (int i = 0; i < 12; ++i) {
        const int x = static_cast<int>(front) + i;
        const float strength = (1 - i / 12.0f) * 0.31f;
        if (x >= Portal)
            cv_.add(static_cast<int>(x * art.sx), static_cast<int>(Rail * art.sy), {1.0f, 0.67f, 0.29f}, strength);
    }
    for (int car = 2; car >= 0; --car) {
        const float right = front - car * 14;
        part(right - 13, -6, 13, 5, {0.33f, 0.2f, 0.24f});
        part(right - 12, -7, 11, 1, {0.47f, 0.34f, 0.37f});
        part(right - 13, -2, 13, 1, {0.65f, 0.38f, 0.26f});
        part(right - 11, -1, 2, 1, {0.08f, 0.10f, 0.15f});
        part(right - 4, -1, 2, 1, {0.08f, 0.10f, 0.15f});
        for (int window = 0; window < 3; ++window) part(right - 11 + 3 * window, -5, 2, 2, {0.94f, 0.66f, 0.34f});
    }
    part(front - 12, -9, 5, 3, {0.39f, 0.25f, 0.28f});
    part(front - 11, -8, 2, 2, {0.96f, 0.73f, 0.45f});
    part(front - 4, -8, 2, 2, {0.37f, 0.24f, 0.28f});
    part(front - 1, -5, 2, 2, {1.0f, 0.91f, 0.65f});
    part(front - 41, -4, 1, 1, {0.84f, 0.22f, 0.14f});
}

bool NightRailwayScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt = 0.0f;
    begin_frame(dt);
    t_ = std::fmod(t_, CycleSeconds);
    const float front = 16 + 20 * t_;
    cv_.px = backdrop_;
    if (front > Portal - 12 && front < 170)
        draw_train(front);
    present(canvas);
    return true;
}

Scenes::SceneDescriptor NightRailwayScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "scenery";
    d.tags = {"scenery", "landscape", "calm", "fits:night"};
    d.intensity = 0.16f;
    d.motion = 0.14f;
    d.music_affinity = 0.05f;
    d.performance_cost = 0.18f;
    return d;
}

}  // namespace Hobbies
