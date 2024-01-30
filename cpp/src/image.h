#include <vector>
#include <Magick++.h>
#include <magick/image.h>

bool LoadImageAndScale(const char *filename,
                       int target_width, int target_height,
                       bool fill_width, bool fill_height,
                       std::vector<Magick::Image> *result,
                       std::string *err_msg);