#pragma once

#include "Scene.h"
#include "wrappers.h"
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
    
    // Color themes for the weather display
    enum class ColorTheme {
        AUTO = 0,       // Automatic based on weather/time
        BLUE = 1,       // Blue theme
        GREEN = 2,      // Green theme
        PURPLE = 3,     // Purple theme
        ORANGE = 4,     // Orange theme
        GRAYSCALE = 5   // Grayscale theme
    };

    class WeatherScene : public Scene {
    private:
        int scroll_position = 0;
        int scroll_direction = 1;
        int scroll_pause_counter = 0;
        bool animation_active = false;
        tmillis_t last_animation_time = 0;
        int animation_frame = 0;

        int star_update_count = 0;

        vector<std::pair<int, int>> stars;

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
        WeatherData data;

        struct Images {
            Magick::Image currentIcon;
            std::vector<Magick::Image> forecastIcons;
        };

        std::optional<Images> images;
        
        // Get theme color based on selected theme
        RGB getThemeColor(ColorTheme theme, const WeatherData &data) const;

        // Rendering methods
        void renderCurrentWeather(const RGBMatrixBase *matrix, const WeatherData &data);
        void renderForecast(const RGBMatrixBase *matrix, const WeatherData &data) const;
        void renderSunriseSunset(const RGBMatrixBase *matrix, const WeatherData &data) const;
        void resetStars();
        
        // Animation methods
        void updateAnimationState(const WeatherData &data);
        void renderAnimations(const RGBMatrixBase *matrix, const WeatherData &data);
        void initializeParticles();
        void updateParticles(const WeatherData &data);
        
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
        PropertyPointer<bool> show_air_quality = MAKE_PROPERTY("show_air_quality", bool, true);
        PropertyPointer<int> animation_intensity = MAKE_PROPERTY("animation_intensity", int, 5);
        PropertyPointer<bool> show_border = MAKE_PROPERTY("show_border", bool, true);
        PropertyPointer<bool> gradient_background = MAKE_PROPERTY("gradient_background", bool, true);
        PropertyPointer<bool> show_sunrise_sunset = MAKE_PROPERTY("show_sunrise_sunset", bool, true);
        PropertyPointer<int> color_theme = MAKE_PROPERTY("color_theme", int, 0); // Default to AUTO

    public:
        bool render(RGBMatrixBase *matrix) override;

        [[nodiscard]] string get_name() const override;

        void after_render_stop(RGBMatrixBase *matrix) override;

        void register_properties() override {
            add_property(location_lat);
            add_property(location_lon);
            add_property(enable_animations);
            add_property(show_air_quality);
            add_property(animation_intensity);
            add_property(show_border);
            add_property(gradient_background);
            add_property(show_sunrise_sunset);
            add_property(color_theme);
        }

        using Scene::Scene;

        ~WeatherScene() override = default;
    };

    class WeatherSceneWrapper final : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
