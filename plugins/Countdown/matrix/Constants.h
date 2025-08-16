#pragma once

#include "led-matrix.h"
#include <filesystem>

// Fonts used by the Countdown scene
extern rgb_matrix::Font HEADER_FONT;
extern rgb_matrix::Font BODY_FONT;
extern rgb_matrix::Font SMALL_FONT;

const static std::filesystem::path countdown_font_dir = std::filesystem::path("plugins") / "Countdown" / "fonts";
