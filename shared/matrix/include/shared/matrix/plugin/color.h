#pragma once

#include <cstdint>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace Plugins {
  class Color {
    uint32_t color;

  public:
    Color() : color(0) {} // Default constructor
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
}

// Define JSON serialization in the nlohmann namespace for better ADL
namespace nlohmann {
  template <>
  struct adl_serializer<Plugins::Color> {
    static void to_json(json& j, const Plugins::Color& c) {
      j = c.to_rgb();
    }

    static void from_json(const json& j, Plugins::Color& c) {
      const uint32_t color = j.get<uint32_t>();
      c = Plugins::Color(color);
    }
  };
}