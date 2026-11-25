#include "AllotmentScene.h"

#include "Backdrop.h"

namespace Hobbies {
namespace {

// Layout in a 128×128 design space, scaled to the canvas.
constexpr float Horizon = 56.0f;
constexpr float HedgeY = 68.0f;    // hedge between the far field and the allotment
constexpr float FenceY = 76.0f;    // foot of the fence at the back of the beds
constexpr float TractorY = 62.0f;  // wheel line on the far field
constexpr float TractorCycle = 150.0f;
// The scarecrow is drawn in its own figure space (ScarecrowX, ScarecrowBase)
// and placed, smaller, among the strawberries by the fence.
constexpr float ScarecrowX = 109.0f, ScarecrowBase = 104.0f, ArmY = 82.0f;
constexpr float ScarecrowAtX = 80.0f, ScarecrowAtBase = 92.0f, ScarecrowScale = 0.75f;
constexpr float ChimneyX = 25.0f, ChimneyTop = 55.0f;

struct Bed {
    float top, height, x0, x1;
};
constexpr std::array<Bed, 3> Beds{{{80.0f, 7.0f, 46.0f, 117.0f}, {93.0f, 9.0f, 40.0f, 123.0f}, {108.0f, 12.0f, 33.0f, 129.0f}}};

// Where a crow may come down: scarecrow arms, fence top, shed ridge, the beds.
constexpr std::array<Point, 9> Perches{{{100.5f, FenceY - 8.0f},
                                        {116.5f, FenceY - 8.0f},
                                        {52.0f, 69.0f},
                                        {84.0f, 69.0f},
                                        {15.0f, 57.5f},
                                        {60.0f, 86.0f},
                                        {78.0f, 101.0f},
                                        {54.0f, 118.0f},
                                        {92.0f, 117.0f}}};

}  // namespace

AllotmentScene::AllotmentScene()
{
    use_environment({Environment::TimeOfDay::Day, Environment::Weather::Clear, Environment::Season::Summer, 38.0f, 0.65f});
}

void AllotmentScene::initialize(int width, int height)
{
    HobbySceneBase::initialize(width, height);
    backdrop_.clear();
    smoke_.clear();
    drift_.reset();
    for (auto& c : crows_) c = {};
    crowWait_ = 2.0f;
    tractor_ = 12.0f;  // the tractor is already out in the field
}

void AllotmentScene::build_backdrop()
{
    const auto& L = env();
    Canvas bg;
    bg.resize(cv_.w, cv_.h);
    const Artwork art(bg);
    const float sx = art.sx, sy = art.sy, k = std::min(sx, sy), W = static_cast<float>(bg.w);
    const Environment::Surface s{bg.px.data(), bg.w, bg.h, 1.0f};
    const auto cap = [&](float x0, float y0, float r0, float x1, float y1, float r1, Rgb c, float a = 1.0f) {
        bg.capsule(x0 * sx, y0 * sy, r0 * k, x1 * sx, y1 * sy, r1 * k, c, a);
    };
    const auto dot = [&](float x, float y, Rgb c, float a = 1.0f) { bg.blend(static_cast<int>(x * sx), static_cast<int>(y * sy), c, a); };
    const float sp = L.spring, su = L.summer, au = L.autumn, wi = L.winter;
    const float frost = std::clamp(wi - 0.3f * L.snow_cover, 0.0f, 1.0f) * std::clamp((6.0f - L.temperature) / 8.0f, 0.3f, 1.0f);
    const auto frosted = [&](Rgb c, float amount) { return mix(c, lit({0.82f, 0.86f, 0.92f}), 0.45f * frost * amount); };
    const float wet = std::clamp(std::max(L.wetness, L.rain) * (1.0f - L.snow_cover), 0.0f, 1.0f);

    // Sky with sun and moon, and a soft line of hills.
    Environment::paint_sky(s, L, Horizon * sy);
    Environment::paint_stars(s, L, 50 * sy, 2.0f, 29, 60);
    Environment::paint_sun(s, L, Environment::sun_point(L, W, Horizon * sy), 6.0f * k);
    Environment::paint_moon(s, L, Environment::moon_point(L, W, Horizon * sy), 4.5f * k);
    const std::vector<float> skyStage = bg.px;
    bg.fill_below([&](float x) { return sy * (Horizon - 6.0f * hill(x / sx, 2.1f)); }, plant({0.32f, 0.44f, 0.30f}, 0.85f, 0.8f, 0.4f));

    // The far field: ploughed in spring, green then golden in summer, stubble
    // in autumn, frosted in winter. Furrows run towards the hedge.
    const Rgb ploughed = terrain({0.36f, 0.25f, 0.17f}, 0.45f), green = meadow({0.36f, 0.56f, 0.20f}, 0.45f);
    const Rgb wheat = terrain({0.86f, 0.70f, 0.32f}, 0.45f), stubble = terrain({0.72f, 0.60f, 0.34f}, 0.45f);
    const Rgb winterField = frosted(terrain({0.40f, 0.34f, 0.26f}, 0.45f), 1.4f);
    const float ripe = std::clamp(L.summer * 1.2f - 0.2f, 0.0f, 1.0f);
    for (int y = static_cast<int>(Horizon * sy); y < static_cast<int>(HedgeY * sy); ++y)
        for (int x = 0; x < bg.w; ++x) {
            if (y < static_cast<int>(sy * (Horizon - 6.0f * hill(x / sx, 2.1f))))
                continue;
            const float d = (y / sy - Horizon) / (HedgeY - Horizon);
            const int spacing = wi > 0.5f ? 6 : 3;
            const bool furrow = static_cast<int>(x / sx + (x / sx - 64.0f) * d * 0.6f) % spacing == 0;
            Rgb c = ploughed;
            c = mix(c, mix(green, wheat, ripe * 0.7f), su);
            c = mix(c, stubble, au);
            c = mix(c, winterField, wi);
            if (furrow)
                c = scale(c, 0.86f + 0.08f * su + 0.06f * wi);
            if (std::fabs(y / sy - (TractorY - 0.3f)) < 0.6f)
                c = mix(c, terrain({0.42f, 0.34f, 0.24f}, 0.45f), 0.35f);  // the farm track
            // At night the pale field sinks back behind the garden.
            c = scale(c, 1.0f - 0.25f * L.night);
            bg.set(x, y, c);
        }
    const std::vector<float> fieldStage = bg.px;
    // A tree on the hedge line behind the shed.
    Environment::paint_tree(s, L, 34.0f * sx, HedgeY * sy, 26.0f * sy, 37, 0.0f, 0.35f);
    // The hedge: a dark band of round bushes.
    for (int i = 0; i < 40; ++i) {
        const float x = i * 3.4f + 2.0f * hash01(i * 3u), r = 2.4f + 1.3f * hash01(i * 3u + 1u);
        art.oval(x, HedgeY - 1.5f + hash01(i * 3u + 2u), r, r * 0.8f, plant({0.16f, 0.30f, 0.14f}, 0.3f, 0.7f, 0.6f));
    }

    // Grass between the beds, darker at the bottom.
    const Rgb grassA = meadow({0.28f, 0.50f, 0.20f}), grassB = meadow({0.34f, 0.56f, 0.22f});
    for (int y = static_cast<int>((HedgeY + 1.5f) * sy); y < bg.h; ++y)
        for (int x = 0; x < bg.w; ++x) {
            Rgb c = mix(grassA, grassB, hash01(static_cast<uint32_t>(x * 131 + y * 977)));
            // Muted, worn patches so the turf is not one flat colour.
            const float fx = x / sx, fy = y / sy;
            const float patch = std::sin(fx * 0.23f + fy * 0.11f) * std::sin(fx * 0.09f - fy * 0.31f + 1.3f);
            c = mix(c, meadow({0.42f, 0.48f, 0.24f}), 0.55f * smoothstep(0.25f, 0.6f, patch));
            c = mix(c, scale(grassA, 0.8f), 0.4f * smoothstep(0.3f, 0.7f, -patch));
            bg.set(x, y, frosted(scale(c, 1.0f - 0.18f * (y / sy - HedgeY) / (128 - HedgeY)), 1.0f));
        }
    // A trodden path up the middle, between the shed and the beds.
    for (int y = static_cast<int>(FenceY * sy); y < bg.h; ++y) {
        const float t = (y / sy - FenceY) / (128 - FenceY);
        art.rect(30.0f - 7.0f * t, y / sy, 3.0f + 6.0f * t, 1.0f / sy,
                 scale(frosted(terrain({0.50f, 0.42f, 0.30f}), 1.0f), 1.0f - 0.35f * wet));
    }
    // After rain, puddles in the low spots of the path mirror the sky.
    if (wet > 0.05f)
        for (const auto [px, py, pr] : {std::array<float, 3>{28.5f, 115.0f, 3.2f}, std::array<float, 3>{29.6f, 101.0f, 1.8f}}) {
            const Rgb skyC = from_env(L.sky(0.4f)), mud = scale(terrain({0.30f, 0.26f, 0.20f}), 0.6f);
            const Rgb water = mix(skyC, {1, 1, 1}, 0.12f);
            // Three overlapping, flat lobes sunk into the mud: a dark far lip
            // (the bank's shadow) and the sky's reflection, broken by mud.
            for (int lobe = 0; lobe < 3; ++lobe) {
                const float ox = (lobe - 1) * pr * 0.55f, oy = (lobe == 1 ? -0.2f : 0.2f) * (1 + lobe % 2),
                            r = pr * (0.55f + 0.2f * (lobe % 2));
                art.oval(px + ox * 0.7f, py + oy * 0.5f - 0.4f, r, std::max(0.7f, r * 0.42f), mud, 0.9f * wet);
                art.oval(px + ox * 0.7f, py + oy * 0.5f, r * 0.85f, std::max(0.6f, r * 0.38f), water, 0.9f * wet);
            }
            for (int i = 0; i < 4; ++i)
                dot(px - pr + 2.0f * pr * hash01(static_cast<uint32_t>(px) * 7u + i), py + 0.4f * (hash01(i * 3u + 1u) - 0.5f), mud,
                    0.7f * wet);
        }

    // Sunflowers in front of the fence (drawn after the fence below).
    // The fence first: posts and two rails along the back of the plot.
    const Rgb wood = lit({0.46f, 0.36f, 0.26f}), woodDark = lit({0.30f, 0.23f, 0.17f});
    for (float x = 36.0f; x < 128.0f; x += 8.0f) art.rect(x, FenceY - 8.0f, 1.0f, 8.0f, woodDark);
    art.rect(36.0f, FenceY - 7.0f, 92.0f, 1.0f, frosted(wood, 1.0f));
    art.rect(36.0f, FenceY - 4.0f, 92.0f, 1.0f, wood);
    // The sunflowers are drawn each frame (they sway in gusts): draw_sunflowers().

    // The beds: raised wooden edges round dark soil; the crops change through
    // the year.
    const Rgb soil = scale(frosted(terrain({0.30f, 0.21f, 0.15f}, 0.0f, 0.8f), 1.2f), 1.0f - 0.3f * wet), soilDark = scale(soil, 0.75f);
    const Rgb edge = frosted(lit({0.44f, 0.34f, 0.24f}), 1.0f);
    for (const auto& b : Beds) {
        // Each bed narrows a little away from the viewer, its ends slanting
        // towards the middle, and its boards are uneven.
        const float mid = (b.x0 + b.x1) * 0.5f;
        const auto row = [&](float y, float h, float grow, Rgb c) {
            const float f = 0.88f + 0.12f * std::clamp((y - b.top) / b.height, 0.0f, 1.0f);
            art.rect(mid + (b.x0 - grow - mid) * f, y, (b.x1 - b.x0 + 2.0f * grow) * f, h, c);
        };
        for (float y = b.top; y < b.top + b.height; y += 1.0f / sy) row(y, 1.0f / sy, 0.0f, soil);
        for (float y = b.top + 1.5f; y < b.top + b.height - 0.5f; y += 2.0f) row(y, 0.6f, -0.5f, soilDark);  // furrows
        row(b.top - 0.6f, 0.6f, 0.6f, scale(edge, 0.8f));
        for (int plank = 0; plank < 6; ++plank) {
            const float u0 = plank / 6.0f, u1 = (plank + 1) / 6.0f, x0 = b.x0 - 1.0f + (b.x1 - b.x0 + 2.0f) * u0;
            art.rect(x0, b.top + b.height, (b.x1 - b.x0 + 2.0f) * (u1 - u0), 1.2f,
                     scale(edge, 0.82f + 0.3f * hash01(static_cast<uint32_t>(b.top) * 7u + plank)));
        }
    }
    // Spring: rows of seedlings in every bed.
    if (sp > 0.02f)
        for (const auto& b : Beds)
            for (float y = b.top + 2.5f; y < b.top + b.height - 1.0f; y += 4.0f)
                for (float x = b.x0 + 3.0f; x < b.x1 - 2.0f; x += 3.2f) {
                    const uint32_t id = static_cast<uint32_t>(x * 13 + y * 71);
                    // Clumps of different sizes with irregular bare earth between.
                    const float n = std::sin(x * 0.19f + b.top * 0.7f) * std::sin(x * 0.07f + y * 0.9f + b.top * 0.3f);
                    if (n < -0.2f || hash01(id) < 0.15f)
                        continue;
                    const float jx = x + 2.0f * (hash01(id + 1u) - 0.5f), size = 0.4f + 0.9f * hash01(id + 2u) * (0.6f + 0.4f * n);
                    const Rgb shoot = plant({0.40f, 0.62f, 0.26f}, 0.0f, 0.2f, 0.3f);
                    if (b.top > 100.0f && x > b.x0 + 0.62f * (b.x1 - b.x0)) {
                        // Freshly planted: dibbed holes and only the odd sprout.
                        dot(jx, y, scale(soil, 0.55f), sp);
                        if (hash01(id + 3u) < 0.25f)
                            cap(jx, y, 0.2f, jx, y - 1.0f, 0.2f, shoot, sp);
                        continue;
                    }
                    if (b.top > 90.0f && b.top < 100.0f) {
                        // Onion sets: short upright shoots.
                        cap(jx - 0.4f, y, 0.2f, jx - 0.6f, y - 1.5f - size, 0.2f, shoot, sp);
                        cap(jx + 0.4f, y, 0.2f, jx + 0.7f, y - 1.2f - size, 0.2f, shoot, sp);
                        continue;
                    }
                    // A tiny pair of leaves on a stalk.
                    dot(jx, y, scale(shoot, 0.7f), sp);
                    bg.ellipse((jx - 0.7f) * sx, (y - 0.8f) * sy, 0.7f * size * k, 0.45f * k, -0.4f, shoot, sp);
                    bg.ellipse((jx + 0.7f) * sx, (y - 0.8f) * sy, 0.7f * size * k, 0.45f * k, 0.4f, shoot, sp);
                }
    // Back bed, left: strawberry plants, berries red in early summer.
    {
        const auto& b = Beds[0];
        const float leafy = std::clamp(0.6f * sp + su + 0.7f * au, 0.0f, 1.0f);
        const Rgb leaf = mix(plant({0.30f, 0.56f, 0.22f}), lit({0.56f, 0.30f, 0.16f}), 0.6f * au);
        if (leafy > 0.02f)
            for (float x = b.x0 + 3.0f; x < b.x0 + 42.0f; x += 5.0f) {
                if (std::fabs(x - ScarecrowAtX) < 2.5f)
                    continue;
                const uint32_t id = static_cast<uint32_t>(x * 29);
                const float y = b.top + 3.5f + hash01(id), r = 1.4f + 0.6f * hash01(id + 1u) * leafy;
                art.oval(x, y + 0.5f, r, r * 0.7f, scale(leaf, 0.6f), leafy);
                art.oval(x, y, r, r * 0.7f, leaf, leafy);
                for (int i = 0; i < 2; ++i)
                    dot(x - 1.0f + 2.0f * hash01(id + 2u + i), y + 0.8f, lit({0.95f, 0.16f, 0.14f}), su * hash01(id + 5u + i) * 1.5f);
            }
    }
    // Back bed: runner beans climbing cane wigwams, red flowers in summer.
    {
        const auto& b = Beds[0];
        const float grown = std::clamp(su + au + 0.3f * sp, 0.0f, 1.0f);
        const Rgb cane = lit({0.62f, 0.52f, 0.34f}),
                  vine = mix(plant({0.24f, 0.48f, 0.18f}), lit({0.42f, 0.32f, 0.18f}), au / std::max(0.01f, su + au));
        for (float x = b.x0 + 46.0f; x < b.x1 - 4.0f; x += 13.0f) {
            const float top = b.top - 13.0f;
            for (float o : {-3.0f, 0.0f, 3.0f}) cap(x + o, b.top + 3.0f, 0.35f, x, top, 0.3f, cane, std::max(grown, 0.6f * wi));
            if (grown > 0.02f) {
                for (int i = 0; i < 14; ++i) {
                    const float t = hash01(static_cast<uint32_t>(x * 7) + i * 3u), side = hash01(static_cast<uint32_t>(x * 11) + i) - 0.5f;
                    const float vy = b.top + 2.0f - t * 14.0f * (0.4f + 0.6f * std::clamp(su + au, 0.0f, 1.0f));
                    art.oval(x + side * 6.0f * (1.0f - t * 0.7f), vy, 1.4f, 1.1f, vine, grown);
                }
                for (int i = 0; i < 4; ++i)
                    dot(x - 2.5f + 5.0f * hash01(static_cast<uint32_t>(x) + i * 5u),
                        b.top - 2.0f - 8.0f * hash01(static_cast<uint32_t>(x) + i * 9u), lit({0.95f, 0.20f, 0.12f}), su);
            }
        }
    }
    // Middle bed: lettuces and cabbages; purple in autumn, frosted in winter.
    {
        const auto& b = Beds[1];
        const float size = std::clamp(0.3f * sp + su + 0.9f * au + 0.7f * wi, 0.0f, 1.0f);
        Rgb head = plant({0.40f, 0.66f, 0.26f}, 0.0f, 0.3f, 0.2f);
        head = mix(head, lit({0.42f, 0.36f, 0.52f}), au * 0.6f);
        head = frosted(mix(head, lit({0.36f, 0.52f, 0.46f}), wi), 1.5f);
        for (int row = 0; row < 2; ++row)
            for (float x = b.x0 + 4.0f + row * 3.5f; x < b.x1 - 3.0f; x += 7.0f) {
                const uint32_t id = static_cast<uint32_t>(x * 3) + row;
                const float y = b.top + 2.5f + row * 4.0f + 0.8f * (hash01(id + 9u) - 0.5f);
                const float r = (0.8f + 2.0f * hash01(id) * hash01(id + 4u) + 0.6f * hash01(id + 5u)) * size;
                if (r < 0.4f || hash01(id + 7u) < 0.18f)  // a few already cut
                    continue;
                art.oval(x + 0.4f, y + 0.6f, r, r * 0.8f, scale(head, 0.55f), 0.8f);
                art.oval(x, y, r, r * 0.8f, head);
                art.oval(x - r * 0.3f, y - r * 0.3f, r * 0.45f, r * 0.35f, mix(head, {1, 1, 1}, 0.18f));
            }
    }
    // Front bed: carrot tops all summer, then pumpkins ripening orange in
    // autumn; a straw mulch in winter.
    {
        const auto& b = Beds[2];
        const Rgb leafy = plant({0.36f, 0.62f, 0.22f}, 0.0f, 0.3f, 0.4f);
        const float carrots = std::clamp(su + 0.4f * au, 0.0f, 1.0f);
        if (carrots > 0.02f)
            for (float y = b.top + 4.0f; y < b.top + b.height; y += 5.0f)
                for (float x = b.x0 + 3.0f; x < b.x0 + 46.0f; x += 3.2f) {
                    if (au > 0.3f && x > b.x0 + 18.0f && x < b.x0 + 34.0f)
                        continue;  // already lifted
                    // Feathery carrot tops: a small fan of fronds.
                    const uint32_t id = static_cast<uint32_t>(x * 17 + y * 5);
                    const float cx = x + 0.8f * (hash01(id) - 0.5f), h = 2.5f + 1.5f * hash01(id + 1u);
                    for (int f = -1; f <= 1; ++f)
                        cap(cx, y, 0.25f, cx + f * 1.2f, y - h + std::fabs(f) * 0.6f, 0.4f, scale(leafy, 0.85f + 0.15f * f), carrots);
                    dot(cx, y + 0.3f, lit({0.92f, 0.50f, 0.14f}), 0.7f * carrots);  // a shoulder of orange
                }
        const float vines = std::clamp(su + au, 0.0f, 1.0f);
        if (vines > 0.02f) {
            const Rgb vine = mix(plant({0.24f, 0.46f, 0.16f}), lit({0.46f, 0.40f, 0.20f}), au / std::max(0.01f, su + au));
            for (int i = 0; i < 26; ++i) {
                const float x = b.x0 + 50.0f + 44.0f * hash01(i * 5u), y = b.top + 2.0f + (b.height - 4.0f) * hash01(i * 5u + 1u);
                art.oval(x, y, 2.0f, 1.3f, vine, vines);
            }
            // Pumpkins: small and green in summer, big and orange in autumn.
            constexpr std::array<std::array<float, 3>, 5> Pumpkins{
                {{54.0f, 6.5f, 1.25f}, {63.5f, 4.5f, 0.75f}, {70.0f, 8.0f, 1.0f}, {81.0f, 5.0f, 0.6f}, {87.5f, 8.5f, 1.15f}}};
            for (size_t i = 0; i < Pumpkins.size(); ++i) {
                const auto [ox, oy, size] = Pumpkins[i];
                const float x = b.x0 + ox, y = b.top + oy;
                const float ripeness = au / std::max(0.01f, su + au), r = (1.3f + 1.5f * ripeness) * vines * size;
                const Rgb skin = mix(lit({0.40f, 0.56f, 0.20f}), lit({0.86f, 0.44f, 0.12f}), ripeness * (0.8f + 0.2f * hash01(i * 3u)));
                art.oval(x + 0.3f, y + 0.5f, r * 1.2f, r, scale(skin, 0.55f));
                art.oval(x, y, r * 1.2f, r, skin);
                art.oval(x - r * 0.3f, y, r * 0.15f, r * 0.85f, scale(skin, 0.8f));  // ribs
                art.oval(x + r * 0.35f, y, r * 0.15f, r * 0.85f, scale(skin, 0.8f));
                art.oval(x - r * 0.45f, y - r * 0.4f, r * 0.3f, r * 0.2f, mix(skin, {1, 1, 0.8f}, 0.12f));
                art.rect(x - 0.3f, y - r - 0.8f, 0.8f, 1.0f, lit({0.30f, 0.26f, 0.14f}));
            }
            // A vine trails from the pumpkins into the lifted carrot rows.
            if (au > 0.3f)
                for (int i = 0; i < 6; ++i)
                    art.oval(b.x0 + 46.0f - i * 3.2f, b.top + 6.0f + 1.2f * std::sin(i * 1.3f), 1.5f - 0.12f * i, 1.0f, vine, au);
            // A few leaves lie over the pumpkins, half hiding some.
            for (const auto [lx, ly] :
                 {std::array<float, 2>{51.5f, 5.0f}, std::array<float, 2>{72.5f, 6.5f}, std::array<float, 2>{89.0f, 7.0f}})
                art.oval(b.x0 + lx, b.top + ly, 2.2f, 1.4f, vine, vines);
        }
        if (wi > 0.02f)
            for (int i = 0; i < 50; ++i) {
                const float x = b.x0 + (b.x1 - b.x0) * hash01(i * 3u + 7u), y = b.top + b.height * hash01(i * 3u + 8u);
                dot(x, y, frosted(lit({0.72f, 0.62f, 0.38f}), 1.0f), 0.45f * wi);
            }
    }

    // Grass grows up over the bed boards here and there, and leaves spill
    // across them, so the edges are not ruled lines.
    if (L.snow_cover < 0.5f)
        for (size_t bi = 0; bi < Beds.size(); ++bi) {
            const auto& b = Beds[bi];
            for (int i = 0; i < 9; ++i) {
                const uint32_t id = static_cast<uint32_t>(bi * 97 + i * 13);
                const float x = b.x0 + (b.x1 - b.x0) * hash01(id), y = b.top + b.height + 1.4f;
                const Rgb tuft = frosted(meadow({0.30f, 0.52f, 0.20f}), 1.0f);
                for (int blade = -1; blade <= 1; ++blade)
                    cap(x + blade * 0.6f, y, 0.3f, x + blade * 1.2f, y - 2.0f - 1.2f * hash01(id + 1u + blade), 0.2f, tuft);
            }
        }
    if (su + au > 0.2f)
        for (int i = 0; i < 5; ++i) {
            const auto& b = Beds[1 + static_cast<size_t>(i % 2)];
            const float x = b.x0 + 8.0f + (b.x1 - b.x0 - 16.0f) * hash01(i * 23u + 5u);
            bg.ellipse(x * sx, (b.top + b.height + 0.4f) * sy, 1.8f * k, 0.9f * k, 0.4f * (hash01(i * 23u) - 0.5f),
                       mix(plant({0.32f, 0.56f, 0.22f}), lit({0.50f, 0.40f, 0.20f}), au / std::max(0.01f, su + au)),
                       std::clamp(su + au, 0.0f, 1.0f));
        }

    // The shed: weathered boards, a felt roof, a window and a stove pipe.
    {
        const float x0 = 3.0f, x1 = 27.0f, top = 66.0f, base = 92.0f;
        const Rgb boards = lit({0.38f, 0.48f, 0.42f}), boardDark = lit({0.26f, 0.34f, 0.30f});
        art.rect(x0, top, x1 - x0, base - top, boards);
        for (float x = x0 + 2.0f; x < x1; x += 3.0f) art.rect(x, top, 0.6f, base - top, boardDark);
        art.rect(x1 - 1.0f, top, 1.0f, base - top, boardDark);
        // Pitched roof, its eave darker; snow lies on it.
        const Rgb felt = terrain({0.24f, 0.22f, 0.22f}, 0.0f, 1.0f), eave = lit({0.16f, 0.15f, 0.15f});
        for (int r = 0; r < 9; ++r) art.rect(x0 - 2.0f + r * 1.5f, top - 1.0f - r, x1 - x0 + 4.0f - r * 3.0f, 1.0f, felt);
        art.rect(x0 - 2.0f, top - 1.0f, x1 - x0 + 4.0f, 1.0f, eave);
        art.rect(x0, top, x1 - x0, 0.8f, scale(boardDark, 0.6f));  // shadow under the eave
        cap(x0 - 2.0f, top - 0.6f, 0.35f, (x0 + x1) * 0.5f, top - 9.6f, 0.35f, scale(eave, 1.0f + 0.4f * L.snow_cover), 0.8f);
        cap((x0 + x1) * 0.5f, top - 9.6f, 0.35f, x1 + 2.0f, top - 0.6f, 0.35f, scale(eave, 1.0f + 0.4f * L.snow_cover), 0.8f);
        art.rect(ChimneyX - 0.5f, ChimneyTop, 1.5f, 8.0f, lit({0.22f, 0.22f, 0.24f}));
        art.rect(ChimneyX - 1.0f, ChimneyTop - 0.6f, 2.5f, 0.8f, lit({0.18f, 0.18f, 0.20f}));
        Environment::paint_window(s, L, static_cast<int>(6.0f * sx), static_cast<int>(71.0f * sy), std::max(1, static_cast<int>(6.0f * sx)),
                                  std::max(1, static_cast<int>(5.0f * sy)), 3);
        art.rect(5.0f, 76.0f, 8.0f, 1.0f, lit({0.30f, 0.24f, 0.18f}));    // sill
        art.rect(16.0f, 74.0f, 7.0f, 18.0f, lit({0.30f, 0.36f, 0.32f}));  // door
        dot(21.5f, 83.0f, lit({0.80f, 0.72f, 0.40f}));                    // handle
        // A water butt by the corner and a few pots on the path.
        art.rect(x1 + 0.5f, 84.0f, 5.0f, 8.0f, lit({0.22f, 0.30f, 0.26f}));
        art.rect(x1 + 0.2f, 84.0f, 5.6f, 0.8f, lit({0.16f, 0.22f, 0.20f}));
        for (int i = 0; i < 3; ++i) art.oval(6.0f + i * 5.0f, 95.0f, 1.6f, 1.2f, lit({0.72f, 0.38f, 0.22f}));
        // A shrub by the corner of the shed: pink-white blossom in spring.
        const Rgb shrub = plant({0.24f, 0.42f, 0.20f}, 0.0f, 0.6f, 0.8f);
        for (int i = 0; i < 6; ++i) art.oval(-0.5f + 3.0f * hash01(i * 11u), 85.0f + 6.0f * hash01(i * 11u + 1u), 2.2f, 1.8f, shrub);
        if (sp > 0.05f)
            for (int i = 0; i < 14; ++i)
                dot(-1.0f + 5.0f * hash01(i * 17u), 84.0f + 8.0f * hash01(i * 17u + 1u),
                    lit(hash01(i * 17u + 2u) < 0.5f ? Rgb{1.0f, 0.78f, 0.86f} : Rgb{1.0f, 0.95f, 0.96f}), sp);
    }

    // Lying snow rounds over the bed edges in drifts, with the odd bare patch.
    if (L.snow_cover > 0.1f) {
        const Rgb snowC = lit({0.90f, 0.93f, 0.98f});
        for (const auto& b : Beds)
            for (float x = b.x0; x < b.x1; x += 2.2f) {
                const uint32_t id = static_cast<uint32_t>(x * 37 + b.top * 11);
                if (hash01(id) < 0.3f)
                    continue;
                art.oval(x, b.top + b.height + 0.1f, 1.6f + 1.4f * hash01(id + 1u), 1.0f, snowC, 0.85f * L.snow_cover);
                art.oval(x + 1.0f, b.top - 0.3f, 1.4f + hash01(id + 2u), 0.7f, snowC, 0.7f * L.snow_cover);
            }
        for (const auto& b : Beds) {
            // A shallow blue shadow under each bed's snowy front lip, and the
            // odd dead stalk poking through.
            for (float x = b.x0 - 1.0f; x < b.x1 + 1.0f; x += 4.0f)
                if (hash01(static_cast<uint32_t>(x * 3 + b.top)) > 0.3f)
                    art.rect(x, b.top + b.height + 1.1f, 2.5f + 1.5f * hash01(static_cast<uint32_t>(x * 5 + b.top)), 0.7f,
                             lit({0.56f, 0.62f, 0.76f}));
            // A board end pokes through the snow at one corner, bare soil beside it.
            art.rect(b.x1 - 1.0f, b.top + b.height - 0.5f, 1.4f, 2.0f, lit({0.38f, 0.28f, 0.20f}));
            art.oval(b.x1 - 3.5f, b.top + b.height - 1.5f, 2.0f, 0.8f, scale(terrain({0.30f, 0.22f, 0.16f}, 0.0f, 0.0f), 0.8f), 0.8f);
            for (int i = 0; i < 7; ++i) {
                const float x = b.x0 + 4.0f + (b.x1 - b.x0 - 8.0f) * hash01(static_cast<uint32_t>(b.top) * 31u + i);
                const float y = b.top + 1.0f + (b.height - 2.0f) * hash01(static_cast<uint32_t>(b.top) * 37u + i);
                cap(x, y, 0.2f, x + 0.4f * (hash01(i * 5u) - 0.5f), y - 1.6f - 1.4f * hash01(i * 7u), 0.15f, lit({0.30f, 0.24f, 0.16f}));
            }
        }
        for (int i = 0; i < 9; ++i) {
            const auto& b = Beds[static_cast<size_t>(i % 3)];
            const float x = b.x0 + 6.0f + (b.x1 - b.x0 - 12.0f) * hash01(i * 13u + 3u),
                        y = b.top + 2.0f + (b.height - 4.0f) * hash01(i * 13u + 4u);
            art.oval(x, y, 1.6f + hash01(i * 13u + 5u), 0.7f, scale(terrain({0.30f, 0.22f, 0.16f}, 0.0f, 0.0f), 0.9f), 0.7f * L.snow_cover);
        }
    }

    // A low sun lights the garden from its side: warm on the near side, the
    // far side falling into cool shadow, and a rim on the shed's sunward edge.
    const float low = std::clamp(L.twilight * 2.0f, 0.0f, 1.0f) * (1.0f - 0.6f * L.night) * (1.0f - 0.7f * L.overcast);
    if (low > 0.02f) {
        const Rgb warm = from_env(L.sun_color);
        const float sunX = L.sun_x;
        for (int y = static_cast<int>(HedgeY * sy) - static_cast<int>(12 * sy); y < bg.h; ++y)
            for (int x = 0; x < bg.w; ++x) {
                const size_t i = static_cast<size_t>(y * bg.w + x);
                if (static_cast<int>(i / static_cast<size_t>(bg.w)) < static_cast<int>(Horizon * sy) || unchanged(bg, skyStage, i))
                    continue;
                const float toward = 1.0f - std::fabs(x / W - sunX);
                float* px = &bg.px[i * 3];
                const float shade = 1.0f - 0.30f * low * (1.0f - toward);
                px[0] = px[0] * shade * (1.0f - 0.06f * low) + warm.r * 0.05f * low * toward * toward;
                px[1] = px[1] * shade + warm.g * 0.04f * low * toward * toward;
                px[2] = px[2] * shade * (1.0f + 0.10f * low * (1.0f - toward)) + warm.b * 0.02f * low * toward * toward;
            }
        // The sunward roof slope, flower heads and crop tops catch the light.
        const Rgb rim = mix(warm, {1, 1, 1}, 0.2f);
        if (sunX < 0.5f)
            cap(1.0f, 65.4f, 0.4f, 15.0f, 56.4f, 0.4f, rim, 0.7f * low);
        else
            cap(15.0f, 56.4f, 0.4f, 29.0f, 65.4f, 0.4f, rim, 0.7f * low);
        for (const auto& b : Beds)
            for (float x = b.x0 + 2.0f; x < b.x1 - 2.0f; x += 3.0f)
                if (hash01(static_cast<uint32_t>(x * 5 + b.top)) > 0.5f)
                    dot(x, b.top + 1.0f, rim, 0.5f * low * (1.0f - std::fabs(x / 128.0f - sunX)));
        // The sunward half of each bed's front boards catches the light, the
        // far end falls into a thin cool shadow; cabbage tops catch it too.
        for (const auto& b : Beds) {
            const float len = b.x1 - b.x0;
            const float from = sunX < 0.5f ? b.x0 - 1.0f : b.x0 + 0.65f * len;
            cap(from, b.top + b.height + 0.3f, 0.3f, from + 0.35f * len + 1.0f, b.top + b.height + 0.3f, 0.3f, rim, 0.35f * low);
            const float endX = sunX < 0.5f ? b.x1 - 0.5f : b.x0 - 0.5f;
            cap(endX, b.top, 0.5f, endX, b.top + b.height + 1.0f, 0.5f, lit({0.12f, 0.12f, 0.20f}), 0.5f * low);
        }
        for (int row = 0; row < 2; ++row)
            for (float x = Beds[1].x0 + 4.0f + row * 3.5f; x < Beds[1].x1 - 3.0f; x += 7.0f)
                dot(x + (sunX < 0.5f ? -1.0f : 1.0f), Beds[1].top + 1.0f + row * 4.0f, rim, 0.55f * low);
        art.rect(5.0f, 77.0f, 8.0f, 0.5f, mix(rim, lit({0.3f, 0.24f, 0.18f}), 0.4f));  // light under the sill
        // The path in the shade of the shed stays cool.
        for (int y = static_cast<int>(FenceY * sy); y < bg.h; ++y) {
            const float t = (y / sy - FenceY) / (128 - FenceY);
            for (int x = static_cast<int>((30.0f - 7.0f * t) * sx); x < static_cast<int>((33.0f - t) * sx); ++x)
                bg.blend(x, y, lit({0.30f, 0.32f, 0.40f}), 0.4f * low);
        }
        // The shed throws a soft shadow away from the sun.
        const float shedAway = sunX < 0.5f ? 1.0f : -1.0f;
        for (int y = static_cast<int>(88.0f * sy); y < static_cast<int>(98.0f * sy); ++y) {
            const float t = (y / sy - 88.0f) / 10.0f;
            const float from = shedAway > 0 ? 27.0f + 2.0f * t : 3.0f - 10.0f * t, to = shedAway > 0 ? 36.0f + 8.0f * t : 3.0f;
            for (int x = static_cast<int>(std::max(0.0f, from) * sx); x < static_cast<int>(to * sx); ++x)
                bg.blend(x, y, lit({0.14f, 0.16f, 0.24f}), 0.3f * low * (1.0f - 0.6f * t));
        }
        dot(7.0f, 72.0f, rim, 0.8f * low);  // a catchlight on the window
        const float edgeX = sunX < 0.5f ? 3.0f : 26.0f;
        art.rect(edgeX, 66.0f, 1.0f, 26.0f, mix(lit({0.38f, 0.48f, 0.42f}), warm, 0.55f * low));
        for (float x = 36.0f; x < 128.0f; x += 8.0f) {
            art.rect(sunX < 0.5f ? x : x + 0.6f, FenceY - 8.0f, 0.5f, 8.0f, mix(lit({0.30f, 0.23f, 0.17f}), warm, 0.2f * low));
            // Long soft shadows away from the low sun across the grass.
            const float away = sunX < 0.5f ? 1.0f : -1.0f;
            cap(x + 0.5f, FenceY, 0.4f, x + 0.5f + away * 9.0f, FenceY + 3.5f, 0.3f, lit({0.12f, 0.14f, 0.20f}), 0.25f * low);
        }
    }

    // Rain: the wet eave and roof edge pick up a few dull highlights.
    if (wet > 0.1f)
        for (int i = 0; i < 6; ++i) dot(2.0f + 25.0f * hash01(i * 19u + 3u), 65.0f, lit({0.62f, 0.66f, 0.72f}), 0.5f * wet);

    // Moonlight picks out the roof ridge and the top fence rail at night.
    if (L.night > 0.3f && L.moon_light > 0.05f) {
        const Rgb moon = lit({0.55f, 0.60f, 0.75f});
        const float a = 0.35f * L.night * std::min(1.0f, L.moon_light * 2.0f);
        cap(1.0f, 65.4f, 0.35f, 15.0f, 56.4f, 0.35f, moon, a);
        cap(15.0f, 56.4f, 0.35f, 29.0f, 65.4f, 0.35f, moon, a * 0.6f);
        art.rect(36.0f, FenceY - 7.0f, 92.0f, 0.5f, mix(lit({0.46f, 0.36f, 0.26f}), moon, a));
        for (int y = static_cast<int>(FenceY * sy); y < bg.h; ++y) {
            const float t = (y / sy - FenceY) / (128 - FenceY);
            bg.blend(static_cast<int>((33.0f - t) * sx), y, moon, a * 0.8f);  // the moonward edge of the path
        }
        art.rect(15.5f, 73.5f, 8.0f, 0.5f, mix(lit({0.30f, 0.36f, 0.32f}), moon, a));  // door frame
        art.rect(Beds[2].x0, Beds[2].top + Beds[2].height, Beds[2].x1 - Beds[2].x0, 0.5f, mix(lit({0.44f, 0.34f, 0.24f}), moon, a));
        art.rect(23.0f, 73.5f, 0.5f, 18.5f, mix(lit({0.30f, 0.36f, 0.32f}), moon, a));
    }

    // Open sky mask for the clouds.
    const size_t n = static_cast<size_t>(bg.w * bg.h);
    sky_.assign(n, 0);
    for (size_t i = 0; i < n; ++i)
        if (static_cast<int>(i / static_cast<size_t>(bg.w)) < static_cast<int>(std::ceil(Horizon * sy)))
            sky_[i] = unchanged(bg, skyStage, i);
    field_.assign(n, 0);
    for (size_t i = 0; i < n; ++i)
        if (static_cast<int>(i / static_cast<size_t>(bg.w)) < static_cast<int>(std::ceil(HedgeY * sy)))
            field_[i] = unchanged(bg, fieldStage, i);
    backdrop_ = std::move(bg.px);
}

void AllotmentScene::draw_scarecrow()
{
    const auto& L = env();
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f, k = std::min(sx, sy);
    // Figure space to the scene: the figure stands at ScarecrowAt*, scaled.
    constexpr float S = ScarecrowScale;
    const auto X = [&](float x) { return (ScarecrowAtX + (x - ScarecrowX) * S) * sx; };
    const auto Y = [&](float y) { return (ScarecrowAtBase + (y - ScarecrowBase) * S) * sy; };
    const auto cap = [&](float x0, float y0, float r0, float x1, float y1, float r1, Rgb c, float a = 1.0f) {
        cv_.capsule(X(x0), Y(y0), r0 * S * k, X(x1), Y(y1), r1 * S * k, c, a);
    };
    const auto oval = [&](float x, float y, float rx, float ry, Rgb c, float a = 1.0f) {
        cv_.ellipse(X(x), Y(y), rx * S * k, ry * S * k, 0.0f, c, a);
    };
    const float wind = 0.4f + std::fabs(L.wind) + 0.8f * L.storm;
    const float flap = std::sin(t_ * (2.0f + wind)) * wind;
    // A garden prop rather than a figure: faded colours, only a faint
    // moonlit lift at night.
    const auto prop = [&](Rgb c) {
        const Rgb flashLift = scale(Rgb{0.55f, 0.58f, 0.70f}, 0.45f * L.flash);
        const Rgb m = mix(lit(c), subject(c), 0.15f);
        return Rgb{std::max(0.0f, m.r - flashLift.r), std::max(0.0f, m.g - flashLift.g), std::max(0.0f, m.b - flashLift.b)};
    };
    const Rgb pole = prop({0.46f, 0.36f, 0.24f}), coat = prop({0.38f, 0.36f, 0.24f}), straw = prop({0.82f, 0.70f, 0.38f});
    const Rgb sack = prop({0.54f, 0.48f, 0.38f}), hat = prop({0.42f, 0.32f, 0.22f});
    const float x = ScarecrowX;
    cap(x, ScarecrowBase, 0.6f, x, 72.0f, 0.5f, pole);
    cap(x - 8.6f, ArmY, 0.45f, x + 8.6f, ArmY, 0.45f, pole);
    // Coat: shoulders along the arm, body down to a ragged hem that flaps.
    for (int i = 0; i <= 8; ++i) {
        const float u = (i - 4) / 4.0f, hem = 94.0f + 1.5f * std::sin(i * 1.7f) + 0.8f * flap * std::sin(t_ * 3.0f + i);
        const float top = ArmY - 0.8f + 0.6f * std::fabs(u);
        cap(x + u * 2.8f, top, 0.9f, x + u * 3.4f + 0.6f * flap, hem - 1.5f, 0.7f, coat);
    }
    // Sleeves to the elbows, straw poking out of the cuffs.
    for (float side : {-1.0f, 1.0f}) {
        const float hang = side > 0 ? 2.2f : 0.4f;  // the right sleeve sags off the bar
        cap(x + side * 2.5f, ArmY, 1.1f, x + side * 5.4f, ArmY + hang + 0.3f * flap * side, 0.9f, coat);
        for (int s = 0; s < 3; ++s)
            cap(x + side * 6.2f, ArmY, 0.3f, x + side * (7.6f + 0.4f * s), ArmY - 1.0f + s * 1.0f + 0.3f * flap, 0.25f, straw);
    }
    cap(x - 1.0f, 91.5f, 0.3f, x - 1.5f + 0.5f * flap, 95.0f, 0.2f, straw);
    cap(x + 1.0f, 91.5f, 0.3f, x + 1.4f + 0.5f * flap, 94.5f, 0.2f, straw);
    // Sack head with a stitched face and a straw hat.
    oval(x, 76.5f, 2.6f, 2.9f, sack);
    oval(x, 74.8f, 2.5f, 1.2f, scale(sack, 0.6f), 0.8f);  // shade under the brim
    cap(x - 4.5f, 73.6f, 0.5f, x + 4.5f, 73.6f + 0.3f * flap, 0.5f, hat);
    oval(x, 72.2f, 2.4f, 1.6f, hat);
}

void AllotmentScene::draw_sunflowers()
{
    const auto& L = env();
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f, k = std::min(sx, sy);
    const float sp = L.spring, su = L.summer, au = L.autumn;
    const float sunflowers = std::clamp(su + au + 0.4f * sp, 0.0f, 1.0f);
    if (sunflowers <= 0.02f)
        return;
    const auto cap = [&](float x0, float y0, float r0, float x1, float y1, float r1, Rgb c, float a = 1.0f) {
        cv_.capsule(x0 * sx, y0 * sy, r0 * k, x1 * sx, y1 * sy, r1 * k, c, a);
    };
    const auto dot = [&](float x, float y, Rgb c, float a = 1.0f) { cv_.blend(static_cast<int>(x * sx), static_cast<int>(y * sy), c, a); };
    // Gusts now and then: all the heads lean together, then settle at their
    // own pace.
    const float low = std::clamp(L.twilight * 2.0f, 0.0f, 1.0f) * (1.0f - 0.6f * L.night) * (1.0f - 0.7f * L.overcast);
    const Rgb rim = mix(from_env(L.sun_color), {1, 1, 1}, 0.2f);
    // lit() lifts everything in a lightning flash; the garden behind gets a
    // gentler lift, so take most of it back off the flowers.
    const auto calm = [&](Rgb c) {
        const Rgb f = scale(Rgb{0.55f, 0.58f, 0.70f}, 0.45f * L.flash);
        return Rgb{std::max(0.0f, c.r - f.r), std::max(0.0f, c.g - f.g), std::max(0.0f, c.b - f.b)};
    };
    for (int i = 0; i < 5; ++i) {
        const float gust = std::pow(std::max(0.0f, std::sin(t_ * 0.35f - i * (0.25f + 0.08f * i))), 6.0f);
        const float lean = 0.1f * L.wind + (0.15f + 0.85f * L.storm) * gust * (L.wind < 0.0f ? -1.0f : 1.0f) * 1.2f +
                           0.04f * std::sin(t_ * 0.9f + i * 1.7f) * (0.3f + L.storm);
        const float x = 41.0f + i * 7.0f + 1.5f * hash01(i * 7u), base = FenceY + 1.0f;
        const float h = (11.0f + 3.0f * hash01(i * 7u + 1u)) * (0.35f + 0.65f * std::clamp(su + au, 0.0f, 1.0f));
        const float tx = x + lean * h * 0.35f, ty = base - h * (1.0f - 0.03f * std::fabs(lean));
        const Rgb stem = calm(plant({0.30f, 0.50f, 0.18f}, 0.0f, 0.3f, 0.6f));
        cap(x, base, 0.5f - 0.15f * au, tx, ty, 0.4f - 0.12f * au, stem, sunflowers);
        for (int leaf = 0; leaf < 2; ++leaf) {
            const float f = 0.35f + 0.3f * leaf, side = leaf ? 1.0f : -1.0f;
            cv_.ellipse((x + (tx - x) * f + side * 1.8f) * sx, (base - h * f) * sy, 1.8f * k, 0.9f * k, side * 0.5f + 0.3f * lean, stem,
                        sunflowers);
        }
        // Heads: bright in summer, heavy and brown and turned down in autumn.
        const float bloom = su + au;
        if (bloom > 0.05f) {
            const float droop = au / std::max(0.01f, bloom);
            const float hx = tx + 1.2f * droop, hy = ty + 1.5f * droop;
            const Rgb petals = calm(mix(lit({1.0f, 0.80f, 0.12f}), lit({0.34f, 0.22f, 0.10f}), droop));
            for (int pi = 0; pi < 10; ++pi) {
                const float a = kTau * pi / 10.0f;
                dot(hx + std::cos(a) * 2.2f, hy + std::sin(a) * 2.0f * (1.0f - 0.4f * droop), petals, bloom * (1.0f - 0.5f * droop));
            }
            cv_.ellipse(hx * sx, hy * sy, 1.5f * k, 1.4f * (1.0f - 0.3f * droop) * k, 0.0f,
                        calm(lit(mix(Rgb{0.30f, 0.18f, 0.08f}, Rgb{0.14f, 0.09f, 0.05f}, droop))), bloom);
            if (low > 0.02f)
                dot(hx + (L.sun_x < 0.5f ? -2.0f : 2.0f), hy - 1.0f, rim, 0.7f * low * bloom);
        }
    }
}

void AllotmentScene::draw_tractor(float dt)
{
    const auto& L = env();
    tractor_ = std::fmod(tractor_ + dt, TractorCycle);
    const float crossing = 42.0f;
    if (tractor_ > crossing || L.daylight < 0.25f || L.snow_cover > 0.5f || L.storm > 0.3f)
        return;
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f, k = std::min(sx, sy);
    const float x = -12.0f + 152.0f * (tractor_ / crossing), y = TractorY;
    const auto cap = [&](float x0, float y0, float r0, float x1, float y1, float r1, Rgb c) {
        cv_.capsule(x0 * sx, y0 * sy, r0 * k, x1 * sx, y1 * sy, r1 * k, c);
    };
    // A fresh, darker track behind the wheels.
    for (float back = 3.0f; back < 26.0f; back += 1.0f / sx)
        cv_.blend(static_cast<int>((x - back) * sx), static_cast<int>((y - 0.3f) * sy), lit({0.24f, 0.20f, 0.14f}, 0.4f),
                  0.35f * (1.0f - back / 26.0f));
    // Dust in dry weather, drifting behind.
    if (L.rain < 0.1f && L.snow_cover < 0.1f && L.wetness < 0.3f)
        for (int i = 1; i < 6; ++i) {
            const float age = i * 0.6f;
            cv_.glow((x - 3.0f - i * 2.2f) * sx, (y - 0.8f - 0.4f * i) * sy, (1.2f + 0.5f * i) * k,
                     from_env(L.lit({0.72f, 0.62f, 0.46f}, 0.4f)), 0.10f * (1.0f - age / 4.0f));
        }
    const float far = 0.35f + 0.5f * L.fog;
    const Rgb body = lit({0.20f, 0.46f, 0.22f}, far), tyre = lit({0.10f, 0.10f, 0.10f}, far), cab = lit({0.70f, 0.74f, 0.76f}, far);
    const float bump = 0.15f * std::sin(t_ * 9.0f);
    cap(x - 1.5f, y - 2.6f + bump, 1.2f, x + 2.5f, y - 2.2f + bump, 1.0f, body);    // bonnet and body
    cv_.ellipse((x - 1.8f) * sx, (y - 1.6f) * sy, 1.8f * k, 1.8f * k, 0.0f, tyre);  // big rear wheel
    cv_.ellipse((x + 2.8f) * sx, (y - 0.9f) * sy, 1.0f * k, 1.0f * k, 0.0f, tyre);
    cv_.set(static_cast<int>((x - 1.8f) * sx), static_cast<int>((y - 1.6f) * sy), lit({0.85f, 0.70f, 0.20f}, 0.35f));  // wheel hub
    for (float yy = y - 6.0f; yy < y - 3.0f; yy += 1.0f / sy)
        cv_.blend(static_cast<int>((x - 2.4f) * sx), static_cast<int>(yy * sy), cab, 0.9f);
    cap(x - 3.0f, y - 6.2f + bump, 0.3f, x - 0.3f, y - 6.2f + bump, 0.3f, body);  // cab roof
    cap(x + 1.6f, y - 3.4f, 0.2f, x + 1.6f, y - 5.0f, 0.2f, tyre);                // exhaust
    if (L.lamp() > 0.1f)
        cv_.glow((x + 3.6f) * sx, (y - 2.4f) * sy, 2.5f * k, {1.0f, 0.92f, 0.7f}, 0.6f * L.lamp());
    // The hedge, the shed, the tree and the sunflowers stand in front of it.
    const int y0 = std::max(0, static_cast<int>((y - 10.0f) * sy)), y1 = std::min(cv_.h, static_cast<int>(std::ceil(HedgeY * sy)));
    for (int row = y0; row < y1; ++row)
        for (int col = 0; col < cv_.w; ++col) {
            const size_t i = static_cast<size_t>(row * cv_.w + col);
            if (!field_[i])
                std::copy_n(&backdrop_[i * 3], 3, &cv_.px[i * 3]);
        }
}

void AllotmentScene::update_crows(float dt)
{
    const auto& L = env();
    const bool weather = L.storm < 0.3f && L.rain < 0.6f && L.daylight > 0.3f;
    bool any = false;
    for (const auto& c : crows_) any |= c.state != 0;
    if (!any) {
        crowWait_ -= dt;
        if (crowWait_ <= 0.0f && weather) {
            // One to three crows come in from the same side.
            const int count = 1 + static_cast<int>(rnd() * 2.99f);
            const bool fromLeft = rnd() < 0.5f;
            std::array<size_t, Perches.size()> order{};
            for (size_t i = 0; i < order.size(); ++i) order[i] = i;
            for (size_t i = order.size() - 1; i > 0; --i) std::swap(order[i], order[static_cast<size_t>(rnd() * (i + 0.999f))]);
            // At most one crow on the scarecrow, so the birds don't frame it.
            size_t next = 0;
            bool onScarecrow = false;
            for (int i = 0; i < count; ++i) {
                while (next < order.size() - 1 && onScarecrow && order[next] < 2) ++next;
                onScarecrow |= order[next] < 2;
                auto& c = crows_[static_cast<size_t>(i)];
                const Point p = Perches[order[next++]];
                c = {fromLeft ? -8.0f - 10.0f * i : 136.0f + 10.0f * i, 20.0f + 12.0f * rnd(), p.x, p.y, 0.0f, rnd() * kTau, 1, !fromLeft};
            }
        }
        return;
    }
    for (auto& c : crows_) {
        c.phase += dt;
        if (c.state == 1 || c.state == 3) {
            const float dx = c.tx - c.x, dy = c.ty - c.y, d = std::sqrt(dx * dx + dy * dy);
            const float step = std::min(d, (c.state == 1 ? 26.0f : 32.0f) * dt);
            if (d > 0.01f) {
                c.x += dx / d * step;
                c.y += dy / d * step;
                c.left = dx < 0.0f;
            }
            if (c.state == 1 && d < 0.3f) {
                c.state = 2;
                c.timer = 9.0f + 10.0f * rnd();
            }
            else if (c.state == 3 && d < 0.5f) {
                c.state = 0;
            }
        }
        else if (c.state == 2) {
            c.timer -= dt;
            // Now and then a short hop on the ground, or a turn on a perch.
            if (rnd() < dt * 0.25f) {
                if (c.y > 85.0f)
                    c.x = std::clamp(c.x + (c.left ? -1.5f : 1.5f), 34.0f, 124.0f);
                else
                    c.left = !c.left;
            }
            if (c.timer <= 0.0f) {
                c.state = 3;
                const bool goLeft = rnd() < 0.5f;
                c.tx = goLeft ? -12.0f : 140.0f;
                c.ty = 10.0f + 15.0f * rnd();
            }
        }
    }
    bool still = false;
    for (const auto& c : crows_) still |= c.state != 0;
    if (!still)
        crowWait_ = 30.0f + 40.0f * rnd();
}

void AllotmentScene::draw_crow(const Crow& c)
{
    if (c.state == 0)
        return;
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f, k = std::min(sx, sy);
    const Rgb black = subject({0.08f, 0.08f, 0.10f}), sheen = subject({0.22f, 0.24f, 0.32f}), beak = subject({0.30f, 0.28f, 0.26f});
    const float f = c.left ? -1.0f : 1.0f;
    const auto cap = [&](float x0, float y0, float r0, float x1, float y1, float r1, Rgb col) {
        cv_.capsule(x0 * sx, y0 * sy, r0 * k, x1 * sx, y1 * sy, r1 * k, col);
    };
    if (c.state == 2) {
        // Standing: body tilted forward, pecking now and then on the ground.
        const bool ground = c.y > 85.0f;
        const float peck = ground && std::fmod(c.phase + c.x, 3.0f) < 0.5f ? 1.0f : 0.0f;
        cap(c.x - f * 1.6f, c.y - 1.2f, 0.4f, c.x + f * 0.8f, c.y - 2.2f, 1.1f, black);                              // tail and body
        cap(c.x + f * 1.4f, c.y - 2.8f + 1.4f * peck, 0.8f, c.x + f * 1.4f, c.y - 2.8f + 1.4f * peck, 0.8f, black);  // head
        cap(c.x + f * 2.1f, c.y - 2.8f + 1.4f * peck, 0.3f, c.x + f * 3.0f, c.y - 2.6f + 1.6f * peck, 0.15f, beak);
        cap(c.x + f * 0.3f, c.y - 2.6f, 0.4f, c.x - f * 0.6f, c.y - 1.8f, 0.3f, sheen);  // folded wing
        cap(c.x, c.y - 1.0f, 0.15f, c.x, c.y, 0.15f, black);                             // legs
        return;
    }
    // Flying: wings beat, slower when gliding in to land.
    const float beat = std::sin(c.phase * (c.state == 1 ? 9.0f : 12.0f));
    cap(c.x - f * 1.8f, c.y + 0.2f, 0.5f, c.x + f * 1.2f, c.y, 0.9f, black);
    cap(c.x + f * 1.8f, c.y - 0.2f, 0.7f, c.x + f * 1.8f, c.y - 0.2f, 0.7f, black);
    for (float side : {-0.6f, 0.6f})
        cap(c.x + side * 0.4f, c.y - 0.2f, 0.4f, c.x + side * 4.0f - f * 0.5f, c.y - 2.6f * beat, 0.25f, black);
}

void AllotmentScene::draw_smoke(float dt)
{
    const auto& L = env();
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f, k = std::min(sx, sy);
    // The stove is lit on cold days and in the evening.
    const float lit_stove = std::clamp((10.0f - L.temperature) / 8.0f, 0.0f, 1.0f) * (L.storm > 0.5f ? 0.0f : 1.0f);
    smokeClock_ += dt * 1.2f * lit_stove;
    while (smokeClock_ >= 1.0f) {
        smokeClock_ -= 1.0f;
        smoke_.push_back({ChimneyX + 0.2f, ChimneyTop - 1.0f, 0.0f});
    }
    for (auto& p : smoke_) {
        p.age += dt;
        p.x += (-1.6f + 2.0f * L.wind) * dt;  // away from the tree, into open sky
        p.y -= 1.4f * dt;
    }
    std::erase_if(smoke_, [](const Puff& p) { return p.age > 6.0f; });
    const Rgb c = from_env(L.lit({0.58f, 0.58f, 0.60f}, 0.1f));
    for (const auto& p : smoke_) {
        const float a = 0.34f * (1.0f - p.age / 6.0f) * std::min(1.0f, 0.4f + p.age * 1.5f);
        cv_.glow(p.x * sx, p.y * sy, (1.6f + p.age * 1.1f) * k, c, a);
    }
}

bool AllotmentScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt = 0.0f;
    begin_frame(dt);
    const float raw = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.1f);
    const auto& L = env();
    const float sx = cv_.w / 128.0f, sy = cv_.h / 128.0f, k = std::min(sx, sy);
    if (environment_changed(backdropKey_) || backdrop_.empty())
        build_backdrop();
    cv_.px = backdrop_;
    // Lightning lights the whole garden for a moment, not just the figures.
    if (L.flash > 0.01f)
        for (int y = static_cast<int>(Horizon * sy); y < cv_.h; ++y)
            for (int x = 0; x < cv_.w; ++x) cv_.add(x, y, {0.55f, 0.58f, 0.70f}, 0.18f * L.flash);
    if (L.flash > 0.01f) {
        // Short stretches of the roof edge and the puddles catch the flash.
        cv_.capsule(6.0f * sx, 62.2f * sy, 0.35f * k, 12.0f * sx, 58.4f * sy, 0.35f * k, {0.85f, 0.88f, 1.0f}, 0.45f * L.flash);
        cv_.capsule(19.0f * sx, 58.9f * sy, 0.35f * k, 23.0f * sx, 61.5f * sy, 0.35f * k, {0.85f, 0.88f, 1.0f}, 0.35f * L.flash);
        for (const auto [px, py, pr] : {std::array<float, 3>{28.5f, 115.0f, 3.0f}, std::array<float, 3>{29.6f, 101.0f, 1.6f}})
            cv_.ellipse(px * sx, py * sy, pr * k, 0.5f * k, 0.0f, {0.80f, 0.84f, 1.0f}, 0.5f * L.flash * L.wetness);
    }
    paint_clouds_behind(cv_, backdrop_, sky_, L, t_, Horizon * sy, 4 * sy, 40 * sy, 21);

    draw_tractor(dt);
    draw_sunflowers();
    draw_smoke(dt);
    update_crows(dt);
    // Crows that perch behind the scarecrow (fence, shed) first, then the
    // scarecrow, then the rest.
    for (const auto& c : crows_)
        if (c.state == 2 && c.y < 72.0f)
            draw_crow(c);
    draw_scarecrow();
    for (const auto& c : crows_)
        if (!(c.state == 2 && c.y < 72.0f))
            draw_crow(c);

    // Butterflies over the beds on summer days.
    if (L.summer > 0.5f && L.daylight > 0.6f && L.rain < 0.1f)
        for (uint32_t i = 0; i < 2; ++i) {
            const float x = 70.0f + 40.0f * std::sin(t_ * 0.11f * (1.0f + i) + i * 2.0f),
                        y = 92.0f + 12.0f * std::sin(t_ * 0.17f + i * 4.0f);
            const float wing = std::fabs(std::sin(t_ * 14.0f + i));
            const Rgb c = subject(i ? Rgb{0.86f, 0.84f, 0.74f} : Rgb{0.90f, 0.56f, 0.20f});
            const float gap = 0.5f + 0.6f * wing;
            cv_.ellipse((x - gap) * sx, y * sy, (0.3f + 0.6f * wing) * k, 0.8f * k, 0.0f, c, 0.8f);
            cv_.ellipse((x + gap) * sx, y * sy, (0.3f + 0.6f * wing) * k, 0.8f * k, 0.0f, c, 0.8f);
        }
    // The big puddle on the path shimmers; drops ring it in the rain.
    const float wet = std::clamp(std::max(L.wetness, L.rain) * (1.0f - L.snow_cover), 0.0f, 1.0f);
    if (wet > 0.05f) {
        const Rgb shine = mix(from_env(L.sky(0.2f)), {1, 1, 1}, 0.3f);
        const float sway = 2.2f * std::sin(t_ * 0.7f);
        cv_.capsule((27.4f + 0.6f * sway) * sx, 114.8f * sy, 0.35f * k, (29.0f + 0.6f * sway) * sx, 114.8f * sy, 0.35f * k, shine,
                    0.4f * wet);
        if (L.rain > 0.1f) {
            const float age = std::fmod(t_ * 1.3f, 1.0f), rx = 26.5f + 4.0f * hash01(static_cast<uint32_t>(t_ * 1.3f));
            cv_.ellipse(rx * sx, 115.2f * sy, (0.5f + 1.6f * age) * k, (0.3f + 0.5f * age) * k, 0.0f, shine, 0.35f * (1.0f - age) * L.rain);
        }
    }
    // Leaves in autumn, petals in spring, fireflies on summer nights.
    drift_.update(raw, L, cv_.w, cv_.h, [&](float) { return cv_.h + 6.0f; }, 0.4f);
    drift_.draw(surface(), L);
    // The shed window warms the boards round it and the grass in front.
    if (L.lamp() > 0.05f) {
        cv_.glow(9.0f * sx, 73.5f * sy, 7.0f * k, {1.0f, 0.66f, 0.32f}, 0.30f * L.lamp());
        // One soft pool of light on the ground in front of the shed, running
        // out along the path.
        for (int y = static_cast<int>(92.0f * sy); y < static_cast<int>(112.0f * sy); ++y)
            for (int x = 0; x < static_cast<int>(40.0f * sx); ++x) {
                const float dx = (x / sx - 14.0f - 0.6f * (y / sy - 92.0f)) / 16.0f, dy = (y / sy - 92.0f) / 18.0f;
                const float f = std::max(0.0f, 1.0f - dx * dx - dy * dy);
                if (f > 0.0f)
                    cv_.add(x, y, {1.0f, 0.62f, 0.30f}, 0.16f * L.lamp() * f * f);
            }
    }

    // Fog thickest over the far field; it thins towards the nearest bed.
    const int clearFrom = static_cast<int>(88.0f * sy);
    if (L.fog > 0.02f)
        nearRows_.assign(cv_.px.begin() + static_cast<std::ptrdiff_t>(clearFrom * cv_.w * 3), cv_.px.end());
    Environment::paint_fog(surface(), L, t_, 48.0f * sy, static_cast<float>(cv_.h));
    if (L.fog > 0.02f) {
        // The far field, tree and sunflowers sink further into the haze.
        const Rgb haze = from_env(L.haze);
        for (int y = static_cast<int>(40.0f * sy); y < static_cast<int>(80.0f * sy); ++y) {
            const float a = 0.62f * L.fog * smoothstep(40.0f * sy, 54.0f * sy, static_cast<float>(y)) *
                            (1.0f - smoothstep(66.0f * sy, 80.0f * sy, static_cast<float>(y)));
            for (int x = 0; x < cv_.w; ++x) cv_.blend(x, y, haze, a);
        }
    }
    if (L.fog > 0.02f)
        for (int y = clearFrom; y < cv_.h; ++y) {
            const float keep = 0.85f * smoothstep(static_cast<float>(clearFrom), static_cast<float>(cv_.h), static_cast<float>(y));
            for (int i = y * cv_.w * 3; i < (y + 1) * cv_.w * 3; ++i)
                cv_.px[static_cast<size_t>(i)] +=
                    (nearRows_[static_cast<size_t>(i - clearFrom * cv_.w * 3)] - cv_.px[static_cast<size_t>(i)]) * keep;
        }
    if (L.fog > 0.02f) {
        // The near beds' soil lips stay firm.
        for (size_t bi = 1; bi < Beds.size(); ++bi) {
            const auto& b = Beds[bi];
            const int row = static_cast<int>((b.top + b.height - 0.3f) * sy);
            for (int col = static_cast<int>(b.x0 * sx); col < std::min(cv_.w, static_cast<int>(b.x1 * sx)); ++col)
                cv_.blend(col, row, lit({0.20f, 0.14f, 0.10f}), (bi == 2 ? 0.8f : 0.5f) * L.fog);
        }
        // The shed's near corner and doorway stay dark through the mist.
        const Rgb dark = lit({0.18f, 0.22f, 0.20f});
        for (int col = static_cast<int>(1.0f * sx); col < static_cast<int>(29.0f * sx); ++col)
            cv_.blend(col, static_cast<int>(65.2f * sy), lit({0.12f, 0.11f, 0.11f}), 0.6f * L.fog);  // the eave
        for (int y = static_cast<int>(66.0f * sy); y < static_cast<int>(92.0f * sy); ++y) {
            cv_.blend(static_cast<int>(26.0f * sx), y, dark, 0.7f * L.fog);
            cv_.blend(static_cast<int>(23.0f * sx), y, dark, 0.45f * L.fog);
            if (y >= static_cast<int>(74.0f * sy))
                cv_.blend(static_cast<int>(16.0f * sx), y, dark, 0.6f * L.fog);
        }
    }
    precip_.set_max(static_cast<int>(200 * render_quality_scale()));
    paint_weather(raw, Horizon * sy, [&](float) { return cv_.h + 6.0f; });
    present(canvas);
    wait_until_next_frame();
    return true;
}

Scenes::SceneDescriptor AllotmentScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "scenery";
    d.tags = {"scenery",  "garden",    "countryside", "farm",        "seasons",     "calm",       "nature",
              "fits:day", "fits:dawn", "fits:spring", "fits:summer", "fits:autumn", "fits:clear", "fits:cloudy"};
    d.intensity = 0.15f;
    d.motion = 0.15f;
    d.music_affinity = 0.05f;
    d.performance_cost = 0.3f;
    environment_->add_variants(d);
    return d;
}

}  // namespace Hobbies
