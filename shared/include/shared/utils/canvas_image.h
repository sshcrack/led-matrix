#pragma once

#include <vector>
#include <Magick++.h>
#include <expected>
#include "led-matrix.h"
#include "content-streamer.h"
#include <filesystem>
#include <magick/image.h>

using namespace std;

bool SetImageTransparent(rgb_matrix::FrameCanvas *c, int x_offset, int y_offset,
                         const Magick::Image& img, uint8_t bgR, uint8_t bgG, uint8_t bgB);

std::expected<vector<Magick::Image>, string>
LoadImageAndScale(const filesystem::path &path, int canvas_width, int canvas_height, bool fill_width, bool fill_height,
                  bool contain_img, bool store_resized_img = false);

void StoreInStream(const Magick::Image &img, int64_t delay_time_us,
                   bool do_center,
                   rgb_matrix::FrameCanvas *scratch,
                   rgb_matrix::StreamWriter *output);

filesystem::path to_processed_path(const filesystem::path &path);