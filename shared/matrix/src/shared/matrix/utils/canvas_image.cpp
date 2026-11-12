#include "shared/matrix/utils/canvas_image.h"
#include "led-matrix.h"
#include "content-streamer.h"
#include "Magick++.h"
#include <filesystem>
#include <iostream>
#include "spdlog/spdlog.h"
#include <functional>
#include <optional>
#include <picosha2.h>
#include <atomic>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

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
                  const bool contain_img, const bool store_resized_img, std::optional<std::function<void(Magick::Image*)>> pre_process,
                  const std::string &preprocessing_cache_key) {
    if (canvas_width <= 0 || canvas_height <= 0) return unexpected("Invalid canvas dimensions");
    filesystem::path img_processed;
    const bool cacheable = store_resized_img && (!pre_process || !preprocessing_cache_key.empty());
    vector<Magick::Image> result;

    // Use RAII to ensure Magick resources are cleaned up
    try {
        // Transform identity and source revision prevent stale geometry and repeated
        // preprocessing. Unversioned callbacks deliberately bypass the cache.
        if (cacheable) {
            const std::string identity = std::to_string(canvas_width) + ":" + std::to_string(canvas_height)
                + ":" + std::to_string(fill_width) + ":" + std::to_string(fill_height)
                + ":" + std::to_string(contain_img) + ":" + std::to_string(pre_process.has_value())
                + ":" + preprocessing_cache_key + ":" + std::to_string(filesystem::file_size(path))
                + ":" + std::to_string(filesystem::last_write_time(path).time_since_epoch().count());
            std::string digest;
            picosha2::hash256_hex_string(identity, digest);
            img_processed = path.parent_path() / (path.stem().string() + "." + digest + ".p" + path.extension().string());
            if (filesystem::exists(img_processed)) {
                try {
                    readImages(&result, img_processed.string());
                    if (!result.empty()) return result;
                } catch (const std::exception &e) {
                    spdlog::warn("Discarding invalid processed image {}: {}", img_processed.string(), e.what());
                }
                result.clear();
                std::error_code ec;
                filesystem::remove(img_processed, ec);
            }
        }

        vector<Magick::Image> frames;
        spdlog::trace("Reading images from {}", path.string());
        readImages(&frames, path.string());

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
        }
        if (canvas_width < target_width) {
            offset_x = (target_width - canvas_width) / 2;
        }

        // Determine the appropriate scaling filter based on image size
        bool use_nearest_neighbor = img_width < canvas_width || img_height < canvas_height;

        trace("Scaling to {}x{} using {} and cropping to {}x{} with {},{} offset",
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

        if (cacheable) {
            static std::atomic<unsigned long long> sequence{0};
#ifdef _WIN32
            const auto process_id = _getpid();
#else
            const auto process_id = getpid();
#endif
            auto temporary = img_processed;
            temporary.replace_filename(img_processed.stem().string() + ".tmp-" + std::to_string(process_id) + "-" + std::to_string(sequence++) + img_processed.extension().string());
            try {
                writeImages(result.begin(), result.end(), temporary.string());
                // Windows does not replace an existing destination on rename.
                std::error_code ec;
                filesystem::remove(img_processed, ec);
                filesystem::rename(temporary, img_processed);
            } catch (std::exception &e) {
                spdlog::warn("Failed to write processed image: {}", e.what());
                std::error_code ec;
                filesystem::remove(temporary, ec);
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
    if (!pixels) throw std::runtime_error("Image pixel data is unavailable");

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
    const auto hold_us = static_cast<uint32_t>(std::clamp<int64_t>(delay_time_us, 1, UINT32_MAX));
    if (!output->Stream(*scratch, hold_us)) throw std::runtime_error("Failed to serialize image frame");
}

bool SetImageTransparent(rgb_matrix::FrameCanvas *c, const int x_offset, const int y_offset,
                         const Magick::Image &img) {
    if (!c || !img.isValid() || img.columns() == 0 || img.rows() == 0) return false;
    // Get direct access to pixel data
    const Magick::PixelPacket *pixels = img.getConstPixels(0, 0, img.columns(), img.rows());


    if (!pixels) return false;
    for (int y = 0; y < static_cast<int>(img.rows()); y++) {
        const Magick::PixelPacket *row = pixels + (y * img.columns());
        for (int x = 0; x < static_cast<int>(img.columns()); x++) {
            const auto &q = row[x];

            // In ImageMagick, opacity is inverted (0 = opaque, MaxRGB = transparent)
            // Calculate alpha in the 0.0-1.0 range (0 = transparent, 1 = opaque)
            const float alpha = 1.0f - (ScaleQuantumToChar(q.opacity) / 255.0f);

            if (alpha > 0.0f) {
                uint8_t r = 0, g = 0, b = 0;
                if (!c->GetPixel(x + x_offset, y + y_offset, &r, &g, &b)) continue;

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
