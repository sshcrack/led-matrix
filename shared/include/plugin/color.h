#pragma once
#include <cstdint>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
class Color {
    private:
        uint32_t color;

    public:
        Color(uint8_t r, uint8_t g, uint8_t b) : color((r << 16) | (g << 8) | b) {}
        Color(uint32_t rgb) : color(rgb) {}
        uint8_t r() const {
          return (color >> 16) & 0xFF;
        }

        uint8_t g() const {
          return (color >> 8) & 0xFF;
        }

        uint8_t b() const {
          return color & 0xFF;
        }

        uint32_t to_rgb() const {
          return color;
        }
};

void to_json(json& j, const Color& c) {
    j = c.to_rgb();
}

void from_json(const json& j, Color& c) {
  uint32_t color = j.get<uint32_t>();
  c = Color(color);
}