#include "ClockSceneInternal.h"

namespace AmbientScenes
{

    ClockScene::ClockScene()
        : Scene(), current_hour(0.0f), current_minute(0.0f), current_second(0.0f), last_digit_update(0)
    {
    }

    void ClockScene::initialize(int width, int height)
    {
        Scene::initialize(width, height);

        // Initialize clock hands to current time
        auto now = std::time(nullptr);
        std::tm local_time_storage{};
        std::tm *local_time = compat_localtime_r(&now, &local_time_storage);

        current_hour = local_time->tm_hour % 12;
        current_minute = local_time->tm_min;
        current_second = local_time->tm_sec;
    }

    bool ClockScene::render(rgb_matrix::FrameCanvas *canvas)
    {
        // Get current time
        auto now = std::time(nullptr);
        std::tm local_time_storage{};
        std::tm *local_time = compat_localtime_r(&now, &local_time_storage);

        int hour = local_time->tm_hour % 12;
        int minute = local_time->tm_min;
        int second = local_time->tm_sec;

        // Update smooth transitions if enabled
        if (smooth_motion->get())
        {
            // Smooth transition to new positions
            const float transition_speed = 0.15f;
            current_hour = current_hour + (hour + minute / 60.0f - current_hour) * transition_speed;
            current_minute = current_minute + (minute + second / 60.0f - current_minute) * transition_speed;
            current_second = current_second + (second - current_second) * transition_speed;
        }
        else
        {
            current_hour = hour + minute / 60.0f;
            current_minute = minute;
            current_second = second;
        }

        // Calm vertical ambient gradient. This reads much better on a physical matrix
        // than a single flat near-black fill.
        const auto bg = bg_color->get();
        for (int y = 0; y < matrix_height; ++y)
        {
            const float t = matrix_height > 1 ? static_cast<float>(y) / (matrix_height - 1) : 0.0f;
            const float glow = 1.0f + 0.55f * (1.0f - std::abs(t * 2.0f - 1.0f));
            const uint8_t r = static_cast<uint8_t>(std::min(255.0f, bg.r * glow));
            const uint8_t g = static_cast<uint8_t>(std::min(255.0f, bg.g * glow));
            const uint8_t b = static_cast<uint8_t>(std::min(255.0f, bg.b * glow + 8.0f * (1.0f - t)));
            for (int x = 0; x < matrix_width; ++x)
                canvas->SetPixel(x, y, r, g, b);
        }

        // Calculate center and radius for analog clock
        int center_x = matrix_width / 2;
        int center_y = matrix_height / 2;
        int radius = std::min(center_x, center_y) - 1;

        // Adjust layout based on what's being shown
        bool showing_analog = show_analog->get();
        bool showing_digital = show_digital->get();
        bool showing_date = show_date->get();

        // Draw analog clock if enabled
        if (showing_analog)
        {
            // If we're showing both analog and digital, make the analog clock smaller
            if (showing_digital)
            {
                center_y = matrix_height / 3;
                radius = std::min(center_x, center_y) - 1;
            }
            draw_analog_clock(canvas, center_x, center_y, radius);
        }

        // Draw digital clock if enabled
        if (showing_digital)
        {
            int y_position;
            if (showing_analog)
            {
                y_position = matrix_height - 10; // Lower on the display
            }
            else
            {
                y_position = (matrix_height - 5) / 2; // Center vertically
            }
            draw_digital_clock(canvas, y_position);
        }

        // Draw date if enabled
        if (showing_date)
        {
            int y_position;
            if (showing_analog && showing_digital)
            {
                y_position = matrix_height - 5; // At the very bottom
            }
            else if (showing_digital)
            {
                y_position = matrix_height - 10; // Below digital clock
            }
            else if (showing_analog)
            {
                y_position = matrix_height - 5; // Below analog clock
            }
            else
            {
                y_position = (matrix_height - 5) / 2; // Center vertically
            }
            draw_date(canvas, y_position);
        }

        return true;
    }

    std::string ClockScene::get_name() const
    {
        return "clock";
    }

    void ClockScene::register_properties()
    {
        add_property(show_digital);
        add_property(show_analog);
        add_property(show_seconds);
        add_property(show_date);
        add_property(hour_color);
        add_property(minute_color);
        add_property(second_color);
        add_property(bg_color);
        add_property(smooth_motion);
        add_property(clock_style);
        add_property(use_antialiasing);
        add_property(use_glow_effect);
    }

    std::unique_ptr<Scenes::Scene> ClockSceneWrapper::create()
    {
        return std::make_unique<ClockScene>();
    }

}
