#pragma once
// Helpers for scenes that paint a fixed composition into a cached backdrop
// and animate a little on top of it (sky, water, weather): drawing in a
// 128×128 design space, masks of what still shows open sky or water, clouds
// that pass behind the landscape, and light on masked water.
#include <shared/matrix/environment/look.h>
#include <shared/matrix/environment/paint.h>

#include "Draw.h"

namespace Hobbies {

// Draws in a 128×128 design space, scaled to the actual canvas.
struct Artwork {
    Canvas& canvas;
    float sx, sy;

    explicit Artwork(Canvas& cv) : canvas(cv), sx(cv.w / 128.0f), sy(cv.h / 128.0f) {}

    // Opaque rectangle, edges rounded to whole pixels.
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

    // A small conifer: trunk and four tiers of branches, shaded on the left.
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

// True when pixel i still has the colour it had in `stage`, a copy of the
// pixels taken before later layers were painted. Comparing against such a
// copy gives masks of what still shows open sky or water.
inline bool unchanged(const Canvas& canvas, const std::vector<float>& stage, size_t i)
{
    return canvas.px[i * 3] == stage[i * 3] && canvas.px[i * 3 + 1] == stage[i * 3 + 1] && canvas.px[i * 3 + 2] == stage[i * 3 + 2];
}

// Drifting clouds over the rows above `horizon` (px) that pass behind the
// landscape: every pixel outside the `sky` mask is restored from `backdrop`.
inline void paint_clouds_behind(Canvas& cv, const std::vector<float>& backdrop, const std::vector<uint8_t>& sky,
                                const Environment::Look& look, float t, float horizon, float y0, float y1, uint32_t seed)
{
    if (look.clouds <= 0.03f && look.overcast <= 0.03f)
        return;
    const int rows = std::min(cv.h, static_cast<int>(std::ceil(horizon)));
    Environment::paint_clouds({cv.px.data(), cv.w, rows, 1.0f}, look, t, y0, y1, 0.0f, seed);
    for (size_t i = 0; i < static_cast<size_t>(rows * cv.w); ++i)
        if (!sky[i])
            std::copy_n(&backdrop[i * 3], 3, &cv.px[i * 3]);
}

// Additive light on the pixels of a mask (open water), in design units.
struct MaskedLight {
    Canvas& cv;
    const std::vector<uint8_t>& mask;
    float sx, sy;

    MaskedLight(Canvas& canvas, const std::vector<uint8_t>& m) : cv(canvas), mask(m), sx(canvas.w / 128.0f), sy(canvas.h / 128.0f) {}

    [[nodiscard]] bool on(int x, int y) const
    {
        return x >= 0 && y >= 0 && x < cv.w && y < cv.h && mask[static_cast<size_t>(y * cv.w + x)];
    }
    // A horizontal streak `length` long starting at x.
    void streak(float x, float y, float length, Rgb color, float alpha) const
    {
        const int row = static_cast<int>(y * sy);
        for (int col = static_cast<int>(x * sx); col < static_cast<int>((x + length) * sx); ++col)
            if (on(col, row))
                cv.add(col, row, color, alpha);
    }
    // A point spread over the two pixels it falls between, so it can move by
    // less than a pixel without stepping.
    void soft(float x, float y, Rgb color, float alpha) const
    {
        const float fx = x * sx - 0.5f;
        const int col = static_cast<int>(std::floor(fx)), row = static_cast<int>(y * sy);
        const float u = fx - static_cast<float>(col);
        if (on(col, row))
            cv.add(col, row, color, alpha * (1.0f - u));
        if (on(col + 1, row))
            cv.add(col + 1, row, color, alpha * u);
    }
    // The path the low sun or the moon lays on the water below `horizon`
    // (design units), centred on design x `light_x`, widening from `half_far`
    // to `half_near` towards the viewer. Each glint swells and fades on its
    // own and the rows sway sub-pixel, so the path shimmers without stepping.
    void light_path(float t, float light_x, float horizon, int first_row, float half_far, float half_near, Rgb color,
                    float visibility) const
    {
        if (visibility <= 0.02f)
            return;
        for (int y = first_row; y < 128; y += 2) {
            const float d = (y - horizon) / (128 - horizon);
            const float half = half_far + (half_near - half_far) * d;
            const float wobble = 1.5f * std::sin(y * 0.7f + t * 1.3f);
            for (int j = 0; j < static_cast<int>(2 * half); ++j) {
                const float x = light_x - half + j + 0.5f;
                const float fall = 1 - std::fabs(x - light_x) / half;
                const uint32_t id = static_cast<uint32_t>(y * 977 + j * 131);
                const float tw = 0.5f + 0.5f * std::sin(t * (1.0f + 1.6f * hash01(id)) + kTau * hash01(id + 7u));
                soft(x + wobble, static_cast<float>(y), color, visibility * fall * (0.06f + 0.30f * tw * tw * tw));
            }
        }
    }
};

}  // namespace Hobbies
