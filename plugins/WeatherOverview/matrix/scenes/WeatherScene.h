#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"
#include "../WeatherParser.h"

namespace Scenes {
    // Struct for animated particles (rain, snow, etc.)
    struct Particle {
        float x;
        float y;
        float speed;
        float size;
        float opacity;
        bool active;
    };
    
    // Struct for shooting stars
    struct ShootingStar {
        float x;
        float y;
        float dx;
        float dy;
        float tail_length;
        float brightness;
        bool active;
    };
    
    // Color themes for the weather display
    enum class ColorTheme {
        AUTO = 0,       // Automatic based on weather/time
        BLUE = 1,       // Blue theme
        GREEN = 2,      // Green theme
        PURPLE = 3,     // Purple theme
        ORANGE = 4,     // Orange theme
        GRAYSCALE = 5   // Grayscale theme
    };

    class WeatherScene final : public Scene {
    private:
        bool animation_active = false;
        int animation_frame = 0;
        int total_animation_frame_size = 180;

        vector<std::pair<int, int>> stars;
        
        // Shooting stars
        std::vector<ShootingStar> shooting_stars;
        std::chrono::steady_clock::time_point last_shooting_star_time = std::chrono::steady_clock::now();

        // Color transition variables for smoother day/night changes
        RGB current_display_color{0, 0, 0};
        RGB target_color{0, 0, 0};
        float transition_progress = 0.0f;

        // Animation system particles
        std::vector<Particle> particles;
        int active_particles = 0;
        bool has_precipitation = false;

        // Each scene has its own parser
        WeatherParser parser;
        
        // Reference to current weather data for animations
        WeatherData data = {};

        struct Images {
            Magick::Image currentIcon;
            std::vector<Magick::Image> forecastIcons;
        };

        std::optional<Images> images;
        
        // Get theme color based on selected theme
        static RGB getThemeColor(ColorTheme theme, const WeatherData &data);

        // Rendering methods
        void renderCurrentWeather(const RGBMatrixBase *matrix, const WeatherData &data);
        void renderForecast(const RGBMatrixBase *matrix, const WeatherData &data) const;
        void renderSunriseSunset(const RGBMatrixBase *matrix, const WeatherData &data) const;
        void renderClock(const RGBMatrixBase *matrix) const;
        void resetStars();
        
        // Animation methods
        void updateAnimationState(const WeatherData &data);
        void renderAnimations(const RGBMatrixBase *matrix, const WeatherData &data);
        void initializeParticles();
        void updateParticles(const WeatherData &data);
        
        // Shooting star methods
        void updateShootingStars();
        void renderShootingStars();
        void tryCreateShootingStar();
        
        // Shared rendering utilities
        static RGB interpolateColor(const RGB &start, const RGB &end, float progress) ;
        void applyBackgroundEffects(const RGBMatrixBase *matrix, const RGB &base_color);
        
        // Visual styling helpers
        void drawWeatherBorder(const RGBMatrixBase *matrix, const RGB &color, int brightness_mod) const;
        void drawPrecipitationIndicator(const RGBMatrixBase *matrix, float probability, int x, int y) const;

        // Location properties
        PropertyPointer<std::string> location_lat = MAKE_PROPERTY("location_lat", std::string, "52.5200");
        PropertyPointer<std::string> location_lon = MAKE_PROPERTY("location_lon", std::string, "13.4050");
        
        // Visual properties
        PropertyPointer<bool> enable_animations = MAKE_PROPERTY("enable_animations", bool, true);
        PropertyPointer<bool> enable_clock = MAKE_PROPERTY("enable_clock", bool, true);
        PropertyPointer<int> animation_intensity = MAKE_PROPERTY("animation_intensity", int, 5);
        PropertyPointer<bool> show_border = MAKE_PROPERTY("show_border", bool, true);
        PropertyPointer<bool> gradient_background = MAKE_PROPERTY("gradient_background", bool, true);
        PropertyPointer<bool> show_sunrise_sunset = MAKE_PROPERTY("show_sunrise_sunset", bool, true);
        PropertyPointer<int> color_theme = MAKE_PROPERTY("color_theme", int, 0); // Default to AUTO
        PropertyPointer<bool> reset_stars_on_exit = MAKE_PROPERTY("reset_stars_on_exit", bool, true);
        PropertyPointer<int> shooting_star_chance = MAKE_PROPERTY("shooting_star_chance", int, 2); // Default 2%
        PropertyPointer<int> shooting_star_frame_threshold = MAKE_PROPERTY_MINMAX("shooting_star_random_chance_frame_count", int, 0, 0, get_target_fps()); // Default 2%

    public:
        bool render(RGBMatrixBase *matrix) override;

        [[nodiscard]] string get_name() const override;

        void after_render_stop(RGBMatrixBase *matrix) override;

        void register_properties() override {
            add_property(location_lat);
            add_property(location_lon);
            add_property(enable_animations);
            add_property(enable_clock);
            add_property(animation_intensity);
            add_property(show_border);
            add_property(gradient_background);
            add_property(show_sunrise_sunset);
            add_property(color_theme);
            add_property(reset_stars_on_exit);
            add_property(shooting_star_chance);
            add_property(shooting_star_frame_threshold);
        }

        using Scene::Scene;

        ~WeatherScene() override = default;


        tmillis_t get_default_duration() override {
            return 30000;
        }

        int get_default_weight() override {
            return 6;
        }
    };

    class WeatherSceneWrapper final : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
