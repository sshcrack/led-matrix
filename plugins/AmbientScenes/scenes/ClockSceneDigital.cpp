// ClockScene: digital time and date rendering
#include "ClockSceneInternal.h"

namespace AmbientScenes
{

    void ClockScene::draw_digital_clock(rgb_matrix::FrameCanvas *canvas, int y_position)
    {
        const auto now = std::time(nullptr);
        std::tm local_time_storage{};
        const std::tm *local_time = compat_localtime_r(&now, &local_time_storage);

        const int hour = local_time->tm_hour;
        const int minute = local_time->tm_min;
        const int second = local_time->tm_sec;

        // Scale the 3x5 pixel font to the available matrix. 128x128 gets a
        // prominent 9-pixel scale while smaller matrices degrade gracefully.
        const int scale = std::max(2, std::min(matrix_width / 18, matrix_height / 12));
        const int digit_w = 3 * scale;
        const int spacing = scale;
        const int colon_w = scale;
        const int total_w = digit_w * 4 + spacing * 3 + colon_w;
        const int x_start = (matrix_width - total_w) / 2;
        const int y = show_analog->get() ? y_position : (matrix_height - 5 * scale) / 2 - (show_date->get() ? scale : 0);

        const auto hc = hour_color->get();
        const auto mc = minute_color->get();
        int x = x_start;
        draw_scaled_digit(canvas, hour / 10, x, y, scale, hc.r, hc.g, hc.b); x += digit_w + spacing;
        draw_scaled_digit(canvas, hour % 10, x, y, scale, hc.r, hc.g, hc.b); x += digit_w + spacing;

        const bool colon_on = !show_seconds->get() || (second % 2 == 0);
        if (colon_on)
        {
            for (int py : {scale, 3 * scale})
                for (int yy = 0; yy < scale; ++yy)
                    for (int xx = 0; xx < scale; ++xx)
                        canvas->SetPixel(x + xx, y + py + yy, 180, 210, 235);
        }
        x += colon_w + spacing;

        draw_scaled_digit(canvas, minute / 10, x, y, scale, mc.r, mc.g, mc.b); x += digit_w + spacing;
        draw_scaled_digit(canvas, minute % 10, x, y, scale, mc.r, mc.g, mc.b);
    }

    void ClockScene::draw_small_digit(rgb_matrix::FrameCanvas *canvas, int digit, int x, int y,
                                      uint8_t r, uint8_t g, uint8_t b)
    {
        // Define small 3x5 digit patterns
        static const bool digit_patterns[10][5][3] = {
            // 0
            {
                {1, 1, 1},
                {1, 0, 1},
                {1, 0, 1},
                {1, 0, 1},
                {1, 1, 1}},
            // 1
            {
                {0, 1, 0},
                {1, 1, 0},
                {0, 1, 0},
                {0, 1, 0},
                {1, 1, 1}},
            // 2
            {
                {1, 1, 1},
                {0, 0, 1},
                {1, 1, 1},
                {1, 0, 0},
                {1, 1, 1}},
            // 3
            {
                {1, 1, 1},
                {0, 0, 1},
                {1, 1, 1},
                {0, 0, 1},
                {1, 1, 1}},
            // 4
            {
                {1, 0, 1},
                {1, 0, 1},
                {1, 1, 1},
                {0, 0, 1},
                {0, 0, 1}},
            // 5
            {
                {1, 1, 1},
                {1, 0, 0},
                {1, 1, 1},
                {0, 0, 1},
                {1, 1, 1}},
            // 6
            {
                {1, 1, 1},
                {1, 0, 0},
                {1, 1, 1},
                {1, 0, 1},
                {1, 1, 1}},
            // 7
            {
                {1, 1, 1},
                {0, 0, 1},
                {0, 0, 1},
                {0, 0, 1},
                {0, 0, 1}},
            // 8
            {
                {1, 1, 1},
                {1, 0, 1},
                {1, 1, 1},
                {1, 0, 1},
                {1, 1, 1}},
            // 9
            {
                {1, 1, 1},
                {1, 0, 1},
                {1, 1, 1},
                {0, 0, 1},
                {1, 1, 1}}};

        // Ensure digit is in range
        if (digit < 0 || digit > 9)
            digit = 0;

        // Draw the digit
        for (int row = 0; row < 5; row++)
        {
            for (int col = 0; col < 3; col++)
            {
                if (digit_patterns[digit][row][col])
                {
                    canvas->SetPixel(x + col, y + row, r, g, b);
                }
            }
        }
    }

    void ClockScene::draw_scaled_digit(rgb_matrix::FrameCanvas *canvas, int digit, int x, int y, int scale,
                                       uint8_t r, uint8_t g, uint8_t b)
    {
        static const bool patterns[10][5][3] = {
            {{1,1,1},{1,0,1},{1,0,1},{1,0,1},{1,1,1}},
            {{0,1,0},{1,1,0},{0,1,0},{0,1,0},{1,1,1}},
            {{1,1,1},{0,0,1},{1,1,1},{1,0,0},{1,1,1}},
            {{1,1,1},{0,0,1},{1,1,1},{0,0,1},{1,1,1}},
            {{1,0,1},{1,0,1},{1,1,1},{0,0,1},{0,0,1}},
            {{1,1,1},{1,0,0},{1,1,1},{0,0,1},{1,1,1}},
            {{1,1,1},{1,0,0},{1,1,1},{1,0,1},{1,1,1}},
            {{1,1,1},{0,0,1},{0,0,1},{0,0,1},{0,0,1}},
            {{1,1,1},{1,0,1},{1,1,1},{1,0,1},{1,1,1}},
            {{1,1,1},{1,0,1},{1,1,1},{0,0,1},{1,1,1}}
        };
        digit = std::clamp(digit, 0, 9);
        for (int row = 0; row < 5; ++row)
            for (int col = 0; col < 3; ++col)
                if (patterns[digit][row][col])
                    for (int yy = 0; yy < scale; ++yy)
                        for (int xx = 0; xx < scale; ++xx)
                            set_pixel_with_brightness(canvas, x + col * scale + xx, y + row * scale + yy,
                                                      r, g, b, 0.82f + 0.18f * (1.0f - yy / float(std::max(1, scale))));
    }

    void ClockScene::draw_date(rgb_matrix::FrameCanvas *canvas, int y_position)
    {
        auto now = std::time(nullptr);
        std::tm local_time_storage{};
        std::tm *local_time = compat_localtime_r(&now, &local_time_storage);

        int day = local_time->tm_mday;
        int month = local_time->tm_mon + 1; // tm_mon is 0-11

        // Calculate horizontal centering based on matrix width
        int x_start = (canvas->width() - 11) / 2; // Adjust for MM/DD format

        // Draw month digits
        draw_small_digit(canvas, month / 10, x_start, y_position, 200, 200, 200);
        draw_small_digit(canvas, month % 10, x_start + 4, y_position, 200, 200, 200);

        // Draw separator
        canvas->SetPixel(x_start + 7, y_position + 2, 150, 150, 150);

        // Draw day digits
        draw_small_digit(canvas, day / 10, x_start + 9, y_position, 200, 200, 200);
        draw_small_digit(canvas, day % 10, x_start + 13, y_position, 200, 200, 200);
    }

}
