#include <vector>
#include <Magick++.h>
#include "led-matrix.h"
#include <magick/image.h>

bool LoadImageAndScale(const char *filename,
                       int target_width, int target_height,
                       bool fill_width, bool fill_height,
                       bool contain_img,
                       std::vector<Magick::Image> *result,
                       std::string *err_msg);

void StoreInStream(const Magick::Image &img, int64_t delay_time_us,
                   bool do_center,
                   rgb_matrix::FrameCanvas *scratch,
                   rgb_matrix::StreamWriter *output);