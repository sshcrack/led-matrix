#pragma once

#include "shared/common/utils/utils.h"
#include "led-matrix.h"
#include <expected>
#include <optional>
#include <vector>
#include "Magick++.h"


void SleepMillis(tmillis_t milli_seconds);
void floatPixelSet(rgb_matrix::FrameCanvas* canvas, int x, int y, float r, float g, float b);

std::expected<std::string,std::string> execute_process(const std::string& cmd, const std::vector<std::string>& args);
std::vector<uint8_t> magick_to_rgb(const Magick::Image& img);
