#include <led-matrix.h>
#include <pixel-mapper.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;

void scalar(rgb_matrix::FrameCanvas *canvas, std::vector<rgb_matrix::Color> &colors) {
    for (int y = 0; y < canvas->height(); ++y)
        for (int x = 0; x < canvas->width(); ++x) {
            const auto &p = colors[y * canvas->width() + x];
            canvas->SetPixel(x, y, p.r, p.g, p.b);
        }
}

bool equal(rgb_matrix::FrameCanvas *a, rgb_matrix::FrameCanvas *b) {
    const char *left, *right;
    std::size_t left_size, right_size;
    a->Serialize(&left, &left_size);
    b->Serialize(&right, &right_size);
    return left_size == right_size && std::memcmp(left, right, left_size) == 0;
}

class AliasMapper final : public rgb_matrix::PixelMapper {
public:
    const char *GetName() const override { return "test-alias"; }
    bool GetSizeMapping(int width, int height, int *w, int *h) const override {
        *w = width; *h = height; return true;
    }
    void MapVisibleToMatrix(int, int, int x, int y, int *out_x, int *out_y) const override {
        *out_x = x / 2; *out_y = y;
    }
};

std::unique_ptr<rgb_matrix::RGBMatrix> create(int rows, int parallel, bool inverse, const char *mapper) {
    rgb_matrix::RGBMatrix::Options options;
    options.rows = rows;
    options.cols = 64;
    options.chain_length = 2;
    options.parallel = parallel;
    options.inverse_colors = inverse;
    options.pixel_mapper_config = mapper;
    options.led_rgb_sequence = inverse ? "BGR" : "RGB";
    rgb_matrix::RuntimeOptions runtime;
    runtime.do_gpio_init = false;
    runtime.daemon = -1;
    runtime.drop_privileges = -1;
    return std::unique_ptr<rgb_matrix::RGBMatrix>(rgb_matrix::RGBMatrix::CreateFromOptions(options, runtime));
}

std::vector<rgb_matrix::Color> pixels(int width, int height) {
    std::vector<rgb_matrix::Color> result(width * height);
    std::uint32_t seed = 42;
    for (auto &p : result) {
        seed = seed * 1664525U + 1013904223U;
        p = {static_cast<std::uint8_t>(seed), static_cast<std::uint8_t>(seed >> 8),
             static_cast<std::uint8_t>(seed >> 16)};
    }
    return result;
}
} // namespace

int main(int argc, char **argv) {
    if (argc > 1 && std::string_view(argv[1]) == "--benchmark") {
        auto matrix = create(64, 2, false, "Rotate:270");
        if (!matrix) return 1;
        auto *canvas = matrix->CreateFrameCanvas();
        auto colors = pixels(matrix->width(), matrix->height());
        for (int mode = 0; mode < 3; ++mode) {
            auto run = [&] {
                if (mode == 0) scalar(canvas, colors);
                else if (mode == 1) canvas->SetPixels(0, 0, canvas->width(), canvas->height(), colors.data());
                else for (int y = 0; y < canvas->height(); ++y)
                    for (int x = 0; x < canvas->width(); ++x) {
                        std::uint8_t r, g, b;
                        canvas->GetPixel(x, y, &r, &g, &b);
                    }
            };
            for (int i = 0; i < 10; ++i) run();
            const auto start = Clock::now();
            constexpr int count = 180;
            for (int i = 0; i < count; ++i) run();
            std::cout << "mode=" << mode << " mean_ms="
                << std::chrono::duration<double, std::milli>(Clock::now() - start).count() / count << '\n';
        }
        return 0;
    }
    std::uint64_t readback_hash = 14695981039346656037ULL;
    for (const int rows : {16, 32, 64}) for (const int parallel : {1, 2, 3})
    for (const bool inverse : {false, true}) for (const char *mapper : {"", "Rotate:270", "Mirror:H"}) {
        auto matrix = create(rows, parallel, inverse, mapper);
        if (!matrix) return 1;
        auto *reference = matrix->CreateFrameCanvas();
        auto *bulk = matrix->CreateFrameCanvas();
        auto colors = pixels(matrix->width(), matrix->height());
        for (const bool cie : {true, false}) for (const int brightness : {1, 17, 100, 42})
        for (const int pwm : {11, 1, 8, 11}) {
            matrix->SetBrightness(brightness);
            for (auto *canvas : {reference, bulk}) {
                canvas->SetPWMBits(pwm);
                canvas->set_luminance_correct(cie);
            }
            // Reuse canvases/settings: lower inactive planes must stay intact.
            scalar(reference, colors);
            bulk->SetPixels(0, 0, bulk->width(), bulk->height(), colors.data());
            if (!equal(reference, bulk)) {
                std::cerr << "bitplane mismatch rows=" << rows << " parallel=" << parallel
                    << " inverse=" << inverse << " mapper=" << mapper << " brightness="
                    << brightness << " cie=" << cie << " pwm=" << pwm << '\n';
                return 2;
            }
            // Sample readback over all color settings. The hash is compared
            // with the unmodified library as a separate compatibility oracle.
            for (int i = 0; i < 256; ++i) {
                std::uint8_t r = 0, g = 0, b = 0;
                reference->GetPixel(i % reference->width(), i / reference->width(), &r, &g, &b);
                for (auto c : {r, g, b}) readback_hash = (readback_hash ^ c) * 1099511628211ULL;
            }
        }
        // Partial/clipped writes preserve the old rectangle semantics.
        for (int y = 0; y < 3; ++y) for (int x = 0; x < 7; ++x) {
            const auto &c = colors[y * 7 + x];
            reference->SetPixel(x - 2, y - 1, c.r, c.g, c.b);
        }
        bulk->SetPixels(-2, -1, 7, 3, colors.data());
        if (!equal(reference, bulk)) return 3;
        // Change mapper after warming its grouping cache on existing canvases.
        if (!matrix->ApplyPixelMapper(rgb_matrix::FindPixelMapper("Mirror", 2, parallel, "V"))) return 4;
        colors = pixels(matrix->width(), matrix->height());
        scalar(reference, colors);
        bulk->SetPixels(0, 0, bulk->width(), bulk->height(), colors.data());
        if (!equal(reference, bulk)) return 5;
        AliasMapper alias;
        if (!matrix->ApplyPixelMapper(&alias)) return 6;
        scalar(reference, colors);
        bulk->SetPixels(0, 0, bulk->width(), bulk->height(), colors.data());
        if (!equal(reference, bulk)) return 7;
    }
    // Recorded with unmodified upstream v0.1.11, including its exact gamma
    // inverse rounding/tie behavior. Changes require an intentional migration.
    if (readback_hash != 1623568448239114217ULL) {
        std::cerr << "readback differs from upstream: " << readback_hash << '\n';
        return 8;
    }
    std::cout << "bitplanes equivalent; readback_hash=" << readback_hash << '\n';
}
