// ClockScene: glow and anti-aliased line drawing
#include "ClockSceneInternal.h"

namespace AmbientScenes
{

    void ClockScene::set_pixel_with_brightness(FrameCanvas *canvas, int x, int y, uint8_t r, uint8_t g, uint8_t b, float brightness)
    {
        if (x < 0 || y < 0 || x >= matrix_width || y >= matrix_height)
        {
            return;
        }

        // Apply brightness factor (0.0 to 1.0)
        uint8_t br = static_cast<uint8_t>(r * brightness);
        uint8_t bg = static_cast<uint8_t>(g * brightness);
        uint8_t bb = static_cast<uint8_t>(b * brightness);

        canvas->SetPixel(x, y, br, bg, bb);
    }

    void ClockScene::apply_glow(FrameCanvas* canvas, int x, int y, uint8_t r, uint8_t g, uint8_t b, float intensity)
    {
        if (!use_glow_effect->get() || intensity <= 0.1f)
        {
            return;
        }

        // Apply a more controlled glow around the pixel
        for (int dy = -1; dy <= 1; dy++)
        {
            for (int dx = -1; dx <= 1; dx++)
            {
                if (dx == 0 && dy == 0)
                    continue; // Skip the center pixel

                int gx = x + dx;
                int gy = y + dy;

                // Check bounds
                if (gx < 0 || gx >= matrix_width ||
                    gy < 0 || gy >= matrix_height)
                {
                    continue;
                }

                // Calculate distance-based intensity
                float dist = sqrt(dx * dx + dy * dy);
                float glow_factor = intensity * (1.0f - dist / 1.5f);

                if (glow_factor > 0.1f)
                {
                    // Apply dimmer color for glow
                    uint8_t gr = static_cast<uint8_t>(r * glow_factor * 0.4f);
                    uint8_t gg = static_cast<uint8_t>(g * glow_factor * 0.4f);
                    uint8_t gb = static_cast<uint8_t>(b * glow_factor * 0.4f);

                    canvas->SetPixel(gx, gy, gr, gg, gb);
                }
            }
        }
    }

    void ClockScene::draw_antialiased_line(rgb_matrix::FrameCanvas *canvas, int x0, int y0, int x1, int y1,
                                           uint8_t r, uint8_t g, uint8_t b, bool should_apply_glow)
    {
        // If antialiasing is disabled, use a simple line algorithm
        if (!use_antialiasing->get())
        {
            // Draw line using Bresenham's algorithm
            int dx = abs(x1 - x0);
            int dy = abs(y1 - y0);
            int sx = x0 < x1 ? 1 : -1;
            int sy = y0 < y1 ? 1 : -1;
            int err = dx - dy;

            while (true)
            {
                canvas->SetPixel(x0, y0, r, g, b);
                if (should_apply_glow && use_glow_effect->get())
                {
                    apply_glow(canvas, x0, y0, r, g, b, 0.7f);
                }

                if (x0 == x1 && y0 == y1)
                    break;

                int e2 = 2 * err;
                if (e2 > -dy)
                {
                    err -= dy;
                    x0 += sx;
                }
                if (e2 < dx)
                {
                    err += dx;
                    y0 += sy;
                }
            }
            return;
        }

        // Simplified anti-aliased line drawing for LED matrix
        // Using a modified Bresenham's algorithm with brightness control
        int dx = abs(x1 - x0);
        int dy = abs(y1 - y0);
        int sx = x0 < x1 ? 1 : -1;
        int sy = y0 < y1 ? 1 : -1;
        int err = dx - dy;

        while (true)
        {
            // Draw the main pixel at full brightness
            canvas->SetPixel(x0, y0, r, g, b);

            // Apply glow if requested
            if (should_apply_glow && use_glow_effect->get())
            {
                // Use a smaller glow radius for better appearance on LED matrix
                for (int dy = -1; dy <= 1; dy++)
                {
                    for (int dx = -1; dx <= 1; dx++)
                    {
                        if (dx == 0 && dy == 0)
                            continue; // Skip center pixel

                        int gx = x0 + dx;
                        int gy = y0 + dy;

                        // Check bounds
                        if (gx >= 0 && gx < canvas->width() &&
                            gy >= 0 && gy < canvas->height())
                        {

                            // Calculate distance-based intensity
                            float dist = sqrt(dx * dx + dy * dy);
                            float intensity = 0.5f * (1.0f - dist / 2.0f);

                            // Apply dimmer color for glow
                            uint8_t gr = static_cast<uint8_t>(r * intensity);
                            uint8_t gg = static_cast<uint8_t>(g * intensity);
                            uint8_t gb = static_cast<uint8_t>(b * intensity);

                            canvas->SetPixel(gx, gy, gr, gg, gb);
                        }
                    }
                }
            }

            if (x0 == x1 && y0 == y1)
                break;

            int e2 = 2 * err;
            if (e2 > -dy)
            {
                err -= dy;
                x0 += sx;
            }
            if (e2 < dx)
            {
                err += dx;
                y0 += sy;
            }
        }
    }

}
