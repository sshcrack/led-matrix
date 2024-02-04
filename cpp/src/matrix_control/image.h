#pragma once

#include <vector>
#include <Magick++.h>
#include "led-matrix.h"
#include "../header_fix/fix-content-streamer.h"
#include <magick/image.h>
using namespace std;

bool LoadImageAndScale(const string& path,
                       int canvas_width, int canvas_height,
                       bool fill_width, bool fill_height,
                       bool contain_img,
                       vector<Magick::Image> *result,
                       string *err_msg);

void StoreInStream(const Magick::Image &img, int64_t delay_time_us,
                   bool do_center,
                   rgb_matrix::FrameCanvas *scratch,
                   rgb_matrix::StreamWriter *output);