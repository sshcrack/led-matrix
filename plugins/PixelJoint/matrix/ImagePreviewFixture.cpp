#include "ImagePreviewFixture.h"

#include <Magick++.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>

#include "shared/matrix/post.h"
#include "shared/matrix/utils/consts.h"

namespace fs = std::filesystem;

namespace {
// The host never resolves: only the seeded cache file is ever read.
constexpr const char* FixtureUrl = "https://preview.invalid/pixeljoint-preview.png";

fs::path cache_path()
{
    Post post(FixtureUrl, false);
    return Constants::post_dir / post.get_filename();
}

void write_fixture_image(const fs::path& path)
{
    constexpr size_t size = 48;
    Magick::Image image(Magick::Geometry(size, size), Magick::ColorRGB(0.03, 0.04, 0.10));
    image.modifyImage();
    auto* pixels = image.getPixels(0, 0, size, size);
    for (size_t y = 0; y < size; ++y) {
        for (size_t x = 0; x < size; ++x) {
            const double nx = (static_cast<double>(x) - size * 0.5) / (size * 0.5);
            const double ny = (static_cast<double>(y) - size * 0.5) / (size * 0.5);
            const double radius = std::sqrt(nx * nx + ny * ny);
            const double band = std::floor(radius * 6.0) / 6.0;
            const bool inside = radius < 0.92;
            auto& pixel = pixels[y * size + x];
            pixel.red = static_cast<MagickLib::Quantum>((inside ? 0.25 + band * 0.7 : 0.03) * MaxRGB);
            pixel.green = static_cast<MagickLib::Quantum>((inside ? 0.75 - band * 0.5 : 0.04) * MaxRGB);
            pixel.blue = static_cast<MagickLib::Quantum>((inside ? 0.45 + (1.0 - band) * 0.4 : 0.10) * MaxRGB);
            pixel.opacity = 0;
        }
    }
    image.syncPixels();
    image.write(path.string());
}

class Provider final : public Previews::DataProvider {
public:
    [[nodiscard]] std::string_view id() const override { return ImagePreviewFixture::InputId; }

    void begin(const Previews::RunContext&) override
    {
        active_ = ImagePreviewFixture::enabled();
        if (!active_)
            return;
        std::error_code ec;
        fs::create_directories(Constants::post_dir, ec);
        if (ec)
            throw std::runtime_error("Could not create " + Constants::post_dir.string() + ": " + ec.message());
        write_fixture_image(cache_path());
    }

    // ImageScene deletes a decoded post file (store_processed_file=false), so
    // seed it again before the collection cycles back to it.
    void update(const Scenes::SceneFrameContext&) override
    {
        if (!active_)
            return;
        std::error_code ec;
        const auto path = cache_path();
        if (!fs::exists(path, ec))
            write_fixture_image(path);
    }

    void end() noexcept override
    {
        if (!active_)
            return;
        std::error_code ec;
        fs::remove(cache_path(), ec);
    }

private:
    bool active_ = false;
};
}  // namespace

namespace ImagePreviewFixture {
bool enabled()
{
    const char* value = std::getenv("LED_MATRIX_PREVIEW_OFFLINE");
    return value != nullptr && *value != '\0' && std::string_view(value) != "0";
}

nlohmann::json providers_json()
{
    return nlohmann::json::array({{{"type", "collection"}, {"arguments", {{"images", {FixtureUrl}}}}}});
}

std::unique_ptr<Previews::DataProvider> make_provider()
{
    return std::make_unique<Provider>();
}
}  // namespace ImagePreviewFixture
