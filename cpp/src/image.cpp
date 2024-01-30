#include <vector>
#include <iostream>
#include <Magick++.h>
#include "spdlog/spdlog.h"

using namespace spdlog;

// Load still image or animation.
// Scale, so that it fits in "width" and "height" and store in "result".
bool LoadImageAndScale(const char *filename,
                       int target_width, int target_height,
                       bool fill_width, bool fill_height,
                       std::vector<Magick::Image> *result,
                       std::string *err_msg) {

    debug("reading and scaling image {}...", filename);
    std::vector<Magick::Image> frames;
    try {
        readImages(&frames, filename);
    } catch (std::exception &e) {
        if (e.what()) *err_msg = e.what();
        return false;
    }
    if (frames.empty()) {
        error("No image found.");
        return false;
    }

    debug("Doing frame stuff");
    // Put together the animation from single frames. GIFs can have nasty
    // disposal modes, but they are handled nicely by coalesceImages()
    if (frames.size() > 1) {
        Magick::coalesceImages(result, frames.begin(), frames.end());
    } else {
        result->push_back(frames[0]);   // just a single still image.
    }

    debug("res");
    const int img_width = (*result)[0].columns();
    const int img_height = (*result)[0].rows();
    const float width_fraction = (float) target_width / img_width;
    const float height_fraction = (float) target_height / img_height;
    debug("fill width, fill height");
    if (fill_width && fill_height) {
        // Scrolling diagonally. Fill as much as we can get in available space.
        // Largest scale fraction determines that.
        const float larger_fraction = (width_fraction > height_fraction)
                                      ? width_fraction
                                      : height_fraction;
        target_width = (int) roundf(larger_fraction * img_width);
        target_height = (int) roundf(larger_fraction * img_height);
    } else if (fill_height) {
        // Horizontal scrolling: Make things fit in vertical space.
        // While the height constraint stays the same, we can expand to full
        // width as we scroll along that axis.
        target_width = (int) roundf(height_fraction * img_width);
    } else if (fill_width) {
        // dito, vertical. Make things fit in horizontal space.
        target_height = (int) roundf(width_fraction * img_height);
    }

    debug("Scale");
    for (auto & i : *result) {
        i.scale(Magick::Geometry(target_width, target_height));
    }

    return true;
}