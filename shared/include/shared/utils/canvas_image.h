#pragma once

#include <vector>
#include <Magick++.h>
#include <expected>
#include "led-matrix.h"
#include "content-streamer.h"
#include <filesystem>
#include <magick/image.h>

using namespace std;

bool SetImageTransparent(rgb_matrix::Canvas *c, int canvas_offset_x, int canvas_offset_y,
                         const uint8_t *buffer, size_t size,
                         int width, int height,
                         uint8_t filterR, uint8_t filterG, uint8_t filterB);

std::expected<vector<Magick::Image>, string>
LoadImageAndScale(const string &path, int canvas_width, int canvas_height, bool fill_width, bool fill_height,
                  bool contain_img);

void StoreInStream(const Magick::Image &img, int64_t delay_time_us,
                   bool do_center,
                   rgb_matrix::FrameCanvas *scratch,
                   rgb_matrix::StreamWriter *output);

filesystem::path to_processed_path(const filesystem::path &path);