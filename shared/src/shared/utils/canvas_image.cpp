#include "shared/utils/canvas_image.h"
#include "led-matrix.h"
#include "content-streamer.h"
#include <Magick++.h>
#include <filesystem>
#include <iostream>
#include "spdlog/spdlog.h"
#include <functional>
#include <optional>

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
LoadImageAndScale(const filesystem::path &path, int canvas_width, int canvas_height, const bool fill_width,
                  const bool fill_height,
                  const bool contain_img, const bool store_resized_img, std::optional<std::function<void(Magick::Image*)>> pre_process) {
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

        // Determine the appropriate scaling filter based on image size
        bool use_nearest_neighbor = img_width < canvas_width || img_height < canvas_height;

        debug("Scaling to {}x{} using {} and cropping to {}x{} with {},{} offset",
              target_width, target_height,
              use_nearest_neighbor ? "nearest neighbor" : "default filter",
              canvas_width, canvas_height, offset_x, offset_y);

        for (auto &img: result) {
            // Set filter type based on whether we're scaling up or down
            if (use_nearest_neighbor) {
                img.filterType(Magick::PointFilter); // Use nearest neighbor for upscaling
            }

            if (pre_process.has_value())
                pre_process.value()(&img);

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

void StoreInStream(const Magick::Image &img, const int64_t delay_time_us,
                   const bool do_center,
                   rgb_matrix::FrameCanvas *scratch,
                   rgb_matrix::StreamWriter *output) {
    scratch->Clear();
    const int x_offset = do_center ? (scratch->width() - img.columns()) / 2 : 0;
    const int y_offset = do_center ? (scratch->height() - img.rows()) / 2 : 0;

    // Get direct access to pixel data
    const Magick::PixelPacket *pixels = img.getConstPixels(0, 0, img.columns(), img.rows());

    for (size_t y = 0; y < img.rows(); ++y) {
        const Magick::PixelPacket *row = pixels + (y * img.columns());
        for (size_t x = 0; x < img.columns(); ++x) {
            const auto &q = row[x];
            if (q.opacity != MaxRGB) {
                // Check for non-transparent pixels
                scratch->SetPixel(x + x_offset, y + y_offset,
                                  ScaleQuantumToChar(q.red),
                                  ScaleQuantumToChar(q.green),
                                  ScaleQuantumToChar(q.blue));
            }
        }
    }
    output->Stream(*scratch, delay_time_us);
}

bool SetImageTransparent(rgb_matrix::FrameCanvas *c, const int x_offset, const int y_offset,
                         const Magick::Image &img) {
    // Get direct access to pixel data
    const Magick::PixelPacket *pixels = img.getConstPixels(0, 0, img.columns(), img.rows());


    for (int y = 0; y < img.rows(); y++) {
        const Magick::PixelPacket *row = pixels + (y * img.columns());
        for (int x = 0; x < img.columns(); x++) {
            const auto &q = row[x];

            // In ImageMagick, opacity is inverted (0 = opaque, MaxRGB = transparent)
            // Calculate alpha in the 0.0-1.0 range (0 = transparent, 1 = opaque)
            const float alpha = 1.0f - (ScaleQuantumToChar(q.opacity) / 255.0f);

            if (alpha > 0.0f) {
                uint8_t r = 255, g, b;
                c->GetPixel(x + x_offset, y + y_offset, &r, &g, &b);

                // Proper alpha compositing formula: new = (source × alpha) + (destination × (1 - alpha))
                const uint8_t new_r = ScaleQuantumToChar(q.red) * alpha + r * (1.0f - alpha);
                const uint8_t new_g = ScaleQuantumToChar(q.green) * alpha + g * (1.0f - alpha);
                const uint8_t new_b = ScaleQuantumToChar(q.blue) * alpha + b * (1.0f - alpha);

                c->SetPixel(x + x_offset, y + y_offset, new_r, new_g, new_b);
            }
        }
    }
    return true;
}
