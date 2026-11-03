// ClockScene: analog face and hands
#include "ClockSceneInternal.h"

namespace AmbientScenes
{

    void ClockScene::draw_analog_clock(rgb_matrix::FrameCanvas *canvas, int center_x, int center_y, int radius)
    {
        // Draw the clock face based on selected style
        draw_clock_face(canvas, center_x, center_y, radius);

        // Calculate hand angles
        float hour_angle = (current_hour / (12.0f)) * 2.0f * M_PI - M_PI_2;
        float minute_angle = (current_minute / 60.0f) * 2.0f * M_PI - M_PI_2;
        float second_angle = (current_second / 60.0f) * 2.0f * M_PI - M_PI_2;

        // Calculate hand endpoints
        int hour_x = center_x + static_cast<int>(sin(hour_angle) * radius * 0.5);
        int hour_y = center_y - static_cast<int>(cos(hour_angle) * radius * 0.5);

        int minute_x = center_x + static_cast<int>(sin(minute_angle) * radius * 0.7);
        int minute_y = center_y - static_cast<int>(cos(minute_angle) * radius * 0.7);

        int second_x = center_x + static_cast<int>(sin(second_angle) * radius * 0.8);
        int second_y = center_y - static_cast<int>(cos(second_angle) * radius * 0.8);

        auto h_color = hour_color->get();
        auto m_color = minute_color->get();
        auto s_color = second_color->get();

        // Draw hour hand (thicker)
        if (use_antialiasing->get())
        {
            draw_antialiased_line(canvas, center_x, center_y, hour_x, hour_y,
                                  h_color.r, h_color.g, h_color.b,
                                  use_glow_effect->get());

            // Make hour hand thicker by drawing additional pixels
            int perpendicular_dx = static_cast<int>(-cos(hour_angle));
            int perpendicular_dy = static_cast<int>(-sin(hour_angle));

            draw_antialiased_line(canvas, center_x + perpendicular_dx, center_y + perpendicular_dy,
                                  hour_x + perpendicular_dx, hour_y + perpendicular_dy,
                                  h_color.r, h_color.g, h_color.b,
                                  false);
        }
        else
        {
            draw_clock_hand(canvas, center_x, center_y, hour_angle, radius * 0.5,
                            h_color.r, h_color.g, h_color.b, 2);
        }

        // Draw minute hand
        if (use_antialiasing->get())
        {
            draw_antialiased_line(canvas, center_x, center_y, minute_x, minute_y,
                                  m_color.r, m_color.g, m_color.b,
                                  use_glow_effect->get());
        }
        else
        {
            draw_clock_hand(canvas, center_x, center_y, minute_angle, radius * 0.7,
                            m_color.r, m_color.g, m_color.b, 1);
        }

        // Draw second hand (if enabled)
        if (show_seconds->get())
        {
            if (use_antialiasing->get())
            {
                draw_antialiased_line(canvas, center_x, center_y, second_x, second_y,
                                      s_color.r, s_color.g, s_color.b,
                                      use_glow_effect->get());
            }
            else
            {
                draw_clock_hand(canvas, center_x, center_y, second_angle, radius * 0.8,
                                s_color.r, s_color.g, s_color.b, 1);
            }
        }

        // Center dot with controlled glow
        canvas->SetPixel(center_x, center_y, 255, 255, 255);
        if (use_glow_effect->get())
        {
            apply_glow(canvas, center_x, center_y, 255, 255, 255, 0.8f);
        }
    }

    void ClockScene::draw_clock_hand(rgb_matrix::FrameCanvas *canvas, int center_x, int center_y,
                                     float angle, int length, uint8_t r, uint8_t g, uint8_t b, int thickness)
    {
        float cos_angle = cos(angle);
        float sin_angle = sin(angle);

        // Draw line using Bresenham's algorithm
        int x1 = center_x;
        int y1 = center_y;
        int x2 = center_x + static_cast<int>(sin_angle * length);
        int y2 = center_y - static_cast<int>(cos_angle * length);

        int dx = abs(x2 - x1);
        int dy = abs(y2 - y1);
        int sx = x1 < x2 ? 1 : -1;
        int sy = y1 < y2 ? 1 : -1;
        int err = dx - dy;

        while (true)
        {
            // Draw the main pixel
            canvas->SetPixel(x1, y1, r, g, b);

            // For thicker lines (thickness > 1)
            if (thickness > 1)
            {
                canvas->SetPixel(x1 + 1, y1, r, g, b);
                canvas->SetPixel(x1, y1 + 1, r, g, b);
                canvas->SetPixel(x1 + 1, y1 + 1, r, g, b);
            }

            if (x1 == x2 && y1 == y2)
                break;

            int e2 = 2 * err;
            if (e2 > -dy)
            {
                err -= dy;
                x1 += sx;
            }
            if (e2 < dx)
            {
                err += dx;
                y1 += sy;
            }
        }
    }

    void ClockScene::draw_clock_face(rgb_matrix::FrameCanvas *canvas, int center_x, int center_y, int radius)
    {
        auto style = clock_style->get().get();

        // Draw based on selected style
        if (style == ClockStyle::CLASSIC)
        { // Classic style
            // Draw hour markers
            for (int i = 0; i < 12; i++)
            {
                float angle = i * M_PI / 6.0f;
                int marker_length = (i % 3 == 0) ? 3 : 2; // Longer markers at 12, 3, 6, 9

                int outer_x = center_x + static_cast<int>(sin(angle) * radius);
                int outer_y = center_y - static_cast<int>(cos(angle) * radius);
                int inner_x = center_x + static_cast<int>(sin(angle) * (radius - marker_length));
                int inner_y = center_y - static_cast<int>(cos(angle) * (radius - marker_length));

                // Draw marker line
                draw_antialiased_line(canvas, outer_x, outer_y, inner_x, inner_y, 200, 200, 200, false);
            }
        }
        else if (style == ClockStyle::MINIMAL)
        { // Minimal style
            // Just draw dots at hour positions
            for (int i = 0; i < 12; i++)
            {
                float angle = i * M_PI / 6.0f;
                int x = center_x + static_cast<int>(sin(angle) * radius);
                int y = center_y - static_cast<int>(cos(angle) * radius);

                // Larger dots at 3, 6, 9, 12
                if (i % 3 == 0)
                {
                    canvas->SetPixel(x, y, 200, 200, 200);
                    // Add adjacent pixels for larger markers
                    canvas->SetPixel(x - 1, y, 150, 150, 150);
                    canvas->SetPixel(x + 1, y, 150, 150, 150);
                    canvas->SetPixel(x, y - 1, 150, 150, 150);
                    canvas->SetPixel(x, y + 1, 150, 150, 150);
                }
                else
                {
                    canvas->SetPixel(x, y, 100, 100, 100);
                }
            }
        }
        else if (style == ClockStyle::ELEGANT)
        { // Elegant style
            // Draw a circle outline
            int segments = 60;
            for (int i = 0; i < segments; i++)
            {
                float angle = i * 2.0f * M_PI / segments;
                int x = center_x + static_cast<int>(sin(angle) * radius);
                int y = center_y - static_cast<int>(cos(angle) * radius);

                // Vary brightness to create a subtle gradient effect
                int brightness = 100 + 100 * (1 + sin(angle * 2)) / 2;
                canvas->SetPixel(x, y, brightness, brightness, brightness);
            }

            // Draw hour markers
            for (int i = 0; i < 12; i++)
            {
                float angle = i * M_PI / 6.0f;
                int x = center_x + static_cast<int>(sin(angle) * (radius - 1));
                int y = center_y - static_cast<int>(cos(angle) * (radius - 1));

                if (i % 3 == 0)
                { // 12, 3, 6, 9 positions
                    canvas->SetPixel(x, y, 255, 255, 255);

                    // Add adjacent pixels for larger markers
                    int dx = static_cast<int>(sin(angle));
                    int dy = static_cast<int>(-cos(angle));
                    canvas->SetPixel(x + dx, y + dy, 200, 200, 200);
                    canvas->SetPixel(x - dx, y - dy, 200, 200, 200);
                }
                else
                {
                    canvas->SetPixel(x, y, 180, 180, 180);
                }
            }
        }
    }

}
