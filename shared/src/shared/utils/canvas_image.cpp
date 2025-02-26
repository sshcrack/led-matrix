#include "shared/utils/canvas_image.h"
#include "led-matrix.h"
#include "content-streamer.h"
#include <Magick++.h>
#include <filesystem>
#include <iostream>
#include "spdlog/spdlog.h"

using namespace spdlog;
using namespace std;

filesystem::path to_processed_path(const filesystem::path &path) {
    filesystem::path processed = path;
    processed.replace_extension("p" + processed.extension().string());

    return processed;
}

// Load still image or animation.
// Scale, so that it fits in "width" and "height" and store in "result".
std::expected<vector<Magick::Image>, string>
LoadImageAndScale(const filesystem::path &path, int canvas_width, int canvas_height, const bool fill_width, const bool fill_height,
                  const bool contain_img, const bool store_resized_img) {
    const filesystem::path img_processed = to_processed_path(path);
    vector<Magick::Image> result;

    // Use RAII to ensure Magick resources are cleaned up
    try {
        // Check for processed image first
        if (filesystem::exists(img_processed)) {
            readImages(&result, img_processed);
            if (!result.empty()) {
                return result;
            }
        }

        vector<Magick::Image> frames;
        spdlog::trace("Reading images from {}", path.c_str());
        readImages(&frames, path);

        if (frames.empty()) {
            return unexpected("No image found.");
        }

        // Coalesce only if we have multiple frames
        if (frames.size() > 1) {
            Magick::coalesceImages(&result, frames.begin(), frames.end());
        } else {
            result.push_back(std::move(frames[0]));
        }

        int target_width = canvas_width;
        int target_height = canvas_height;

        const int img_width = result[0].columns();
        const int img_height = result[0].rows();
        const float width_fraction = (float) target_width / img_width;
        const float height_fraction = (float) target_height / img_height;
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
        } else if (fill_height) {
            // Horizontal scrolling: Make things fit in vertical space.
            // While the height constraint stays the same, we can expand to full
            // width as we scroll along that axis.
            target_width = (int) roundf(height_fraction * img_width);
        } else if (fill_width) {
            // dito, vertical. Make things fit in horizontal space.
            target_height = (int) roundf(width_fraction * img_height);
        }


        int offset_x = 0;
        int offset_y = 0;

        if (canvas_height < target_height) {
            offset_y = (target_height - canvas_height) / 2;
        } else if (canvas_width < target_width) {
            offset_x = (target_width - canvas_width) / 2;
        }


        debug("Scaling to {}x{} and cropping to {}x{} with {},{} offset", target_width, target_height, canvas_width,
              canvas_height, offset_x, offset_y);
        for (auto &img: result) {
            img.scale(Magick::Geometry(target_width, target_height));
            img.crop(Magick::Geometry(canvas_width, canvas_height, offset_x, offset_y));
        }

        if (store_resized_img) {
            try {
                writeImages(result.begin(), result.end(), img_processed);
            } catch (std::exception &e) {
                spdlog::warn("Failed to write processed image: {}", e.what());
            }
        }

        return result;
    } catch (std::exception &e) {
        // Clean up any partial results
        result.clear();
        return unexpected(e.what());
    }
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

bool SetImageTransparent(rgb_matrix::Canvas *c, int canvas_offset_x, int canvas_offset_y,
                         const uint8_t *buffer, size_t size,
                         const int width, const int height,
                         uint8_t filterR, uint8_t filterG, uint8_t filterB) {
    if (3 * width * height != (int) size) // Sanity check
        return false;

    int image_display_w = width;
    int image_display_h = height;

    size_t skip_start_row = 0; // Bytes to skip before each row
    if (canvas_offset_x < 0) {
        skip_start_row = -canvas_offset_x * 3;
        image_display_w += canvas_offset_x;
        if (image_display_w <= 0) return false; // Done. outside canvas.
        canvas_offset_x = 0;
    }
    if (canvas_offset_y < 0) {
        // Skip buffer to the first row we'll be showing
        buffer += 3 * width * -canvas_offset_y;
        image_display_h += canvas_offset_y;
        if (image_display_h <= 0) return false; // Done. outside canvas.
        canvas_offset_y = 0;
    }
    const int w = std::min(c->width(), canvas_offset_x + image_display_w);
    const int h = std::min(c->height(), canvas_offset_y + image_display_h);

    // Bytes to skip for wider than canvas image at the end of a row
    const size_t skip_end_row = (canvas_offset_x + image_display_w > w)
                                    ? (canvas_offset_x + image_display_w - w) * 3
                                    : 0;

    // Let's make this a combined skip per row and ajust where we start.
    const size_t next_row_skip = skip_start_row + skip_end_row;
    buffer += skip_start_row;

    for (int y = canvas_offset_y; y < h; ++y) {
        for (int x = canvas_offset_x; x < w; ++x) {
            if (buffer[0] != filterR || buffer[1] != filterG || buffer[2] != filterB)
                c->SetPixel(x, y, buffer[0], buffer[1], buffer[2]);

            buffer += 3;
        }
        buffer += next_row_skip;
    }
    return true;
}
