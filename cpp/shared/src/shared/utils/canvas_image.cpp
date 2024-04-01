#include <vector>
#include "led-matrix.h"
#include "content-streamer.h"
#include <Magick++.h>
#include <filesystem>
#include <iostream>
#include "spdlog/spdlog.h"

using namespace spdlog;
using namespace std;

filesystem::path to_processed_path(const filesystem::path& path) {
    filesystem::path processed = path;
    processed.replace_extension("p" + processed.extension().string());

    return processed;
}

// Load still image or animation.
// Scale, so that it fits in "width" and "height" and store in "result".
bool LoadImageAndScale(const string& str_path,
                       int canvas_width, int canvas_height,
                       bool fill_width, bool fill_height,
                       bool contain_img,
                       vector<Magick::Image> *result,
                       string *err_msg) {

    filesystem::path path = filesystem::path(str_path);
    filesystem::path img_processed = to_processed_path(path);

    debug("Checking if exists");
    // Checking if first exists

    if(filesystem::exists(img_processed)) {
        try {
            readImages(result, img_processed);
        } catch (exception& ex) {
            *err_msg = ex.what();
            return false;
        }


        if(!result->empty()) {
            return true;
        }

        error("Error loading file, trying again");
    }

    int target_width = canvas_width;
    int target_height = canvas_height;

    vector<Magick::Image> frames;
    try {
        readImages(&frames, path);
    } catch (std::exception& e) {
        if (e.what()) *err_msg = e.what();
        return false;
    }
    if (frames.empty()) {
        fprintf(stderr, "No image found.");
        return false;
    }

    // Put together the animation from single frames. GIFs can have nasty
    // disposal modes, but they are handled nicely by coalesceImages()
    if (frames.size() > 1) {
        Magick::coalesceImages(result, frames.begin(), frames.end());
    } else {
        result->push_back(frames[0]);   // just a single still image.
    }

    const int img_width = (*result)[0].columns();
    const int img_height = (*result)[0].rows();
    const float width_fraction = (float)target_width / img_width;
    const float height_fraction = (float)target_height / img_height;
    if (fill_width && fill_height) {
        // Scrolling diagonally. Fill as much as we can get in available space.
        // Largest scale fraction determines that.

        // Covers if contain_img is false (chooses largest fraction) and if contain_img is true (chooses smallest fraction)
        const bool which_factor = contain_img ? width_fraction < height_fraction : width_fraction > height_fraction;

        const float factor = which_factor
                                      ? width_fraction
                                      : height_fraction;
        target_width = (int) roundf(factor * img_width);
        target_height = (int) roundf(factor * img_height);
    }
    else if (fill_height) {
        // Horizontal scrolling: Make things fit in vertical space.
        // While the height constraint stays the same, we can expand to full
        // width as we scroll along that axis.
        target_width = (int) roundf(height_fraction * img_width);
    }
    else if (fill_width) {
        // dito, vertical. Make things fit in horizontal space.
        target_height = (int) roundf(width_fraction * img_height);
    }


    int offset_x = 0;
    int offset_y = 0;

    if(canvas_height < target_height) {
        offset_y = (target_height - canvas_height) / 2;
    } else if(canvas_width < target_width) {
        offset_x = (target_width - canvas_width) / 2;
    }



    debug("Scaling to {}x{} and cropping to {}x{} with {},{} offset", target_width, target_height, canvas_width, canvas_height, offset_x, offset_y);
    for (auto & img : *result) {
        //img.scale(Magick::Geometry(target_width, target_height));
        //img.crop(Magick::Geometry(canvas_width,canvas_height, offset_x, offset_y));
    }

    std::flush(std::cout);
    std::cout << "Accessing" << std::endl;
    std::cout << "Done" << std::endl;
    std::flush(std::cout);


    try {
        writeImages(result->begin(), result->end(), img_processed);
    } catch (std::exception& e) {
        if (e.what()) *err_msg = e.what();
        return false;
    }


    return true;
}


void StoreInStream(const Magick::Image &img, int64_t delay_time_us,
                   bool do_center,
                   rgb_matrix::FrameCanvas *scratch,
                   rgb_matrix::StreamWriter *output) {
    scratch->Clear();
    const int x_offset = do_center ? (scratch->width() - img.columns()) / 2 : 0;
    const int y_offset = do_center ? (scratch->height() - img.rows()) / 2 : 0;
    for (size_t y = 0; y < img.rows(); ++y) {
        for (size_t x = 0; x < img.columns(); ++x) {
            const Magick::Color &c = img.pixelColor(x, y);
            if (c.alphaQuantum() < 255) {
                scratch->SetPixel(x + x_offset, y + y_offset,
                                  ScaleQuantumToChar(c.redQuantum()),
                                  ScaleQuantumToChar(c.greenQuantum()),
                                  ScaleQuantumToChar(c.blueQuantum()));
            }
        }
    }
    output->Stream(*scratch, delay_time_us);
}

