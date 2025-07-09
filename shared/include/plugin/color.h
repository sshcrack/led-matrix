#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
class Color {
    private:
        uint32_t color;

    public:
        Color(const uint8_t r, const uint8_t g, const uint8_t b) : color((r << 16) | (g << 8) | b) {}
        explicit Color(const uint32_t rgb) : color(rgb) {}
        [[nodiscard]] uint8_t r() const {
          return (color >> 16) & 0xFF;
        }

        [[nodiscard]] uint8_t g() const {
          return (color >> 8) & 0xFF;
        }

        [[nodiscard]] uint8_t b() const {
          return color & 0xFF;
        }

        [[nodiscard]] uint32_t to_rgb() const {
          return color;
        }
};

inline void to_json(json& j, const Color& c) {
    j = c.to_rgb();
}

inline void from_json(const json& j, Color& c) {
  const uint32_t color = j.get<uint32_t>();
  c = Color(color);
}