#include "WeatherScene.h"
#include "../Constants.h"
#include "picosha2.h"
#include "shared/matrix/utils/canvas_image.h"
#include "shared/matrix/utils/image_fetch.h"
#include <shared/matrix/utils/utils.h>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

static void pre_process_image(Magick::Image *img)
{
    const int w = img->columns() * 0.9f;
    const int h = img->rows() * 0.9f;

    const int x = (img->columns() - w) / 2;
    const int y = (img->rows() - h) / 2;

    img->crop(Magick::Geometry(w, h, x, y));
}

bool Scenes::WeatherScene::reloadImages()
{
    const auto weather_dir_path = fs::path(weather_dir);
    if (!exists(weather_dir_path))
    {
        fs::create_directory(weather_dir);
    }

    std::string hash;
    picosha2::hash256_hex_string(data.icon_url, hash);

    string file_path = weather_dir_path / ("weather_icon_" + hash + ".png");
    fs::path processed_img = to_processed_path(file_path);

    if (!fs::exists(processed_img) && !data.icon_url.empty())
    {
        try_remove(file_path);
        auto res = utils::download_image(data.icon_url, file_path);
        if (!res)
        {
            spdlog::warn("Could not download main image {}", res.error());
        }
    }

    bool contain_img = true;
    auto res = LoadImageAndScale(
        file_path,
        MAIN_ICON_SIZE, MAIN_ICON_SIZE,
        true, true,
        contain_img, true,
        pre_process_image);

    auto img = WeatherScene::Images();
    bool have_any_image = false;

    if (res.has_value())
    {
        auto arr = std::move(res.value());
        img.currentIcon = arr.front();
        have_any_image = true;
        try_remove(file_path);
    }
    else
    {
        spdlog::error("Error loading main image: {}", res.error());
    }

    for (const auto &forecast_day : data.forecast)
    {
        if (!forecast_day.icon_url.empty())
        {
            std::string forecast_hash;
            picosha2::hash256_hex_string(forecast_day.icon_url, forecast_hash);

            string forecast_file = weather_dir_path / ("forecast_icon" + forecast_hash + ".png");
            fs::path forecast_processed = to_processed_path(forecast_file);

            if (!fs::exists(forecast_processed))
            {
                try_remove(forecast_file);
                auto dl_res = utils::download_image(forecast_day.icon_url, forecast_file);
                if (!dl_res)
                {
                    spdlog::warn("Could not download forecast image {}", dl_res.error());
                    continue;
                }
            }

            auto f_res = LoadImageAndScale(forecast_file,
                                           FORECAST_ICON_SIZE, FORECAST_ICON_SIZE,
                                           true, true,
                                           contain_img, true,
                                           pre_process_image);

            if (f_res)
            {
                img.forecastIcons.push_back(f_res.value().at(0));
                have_any_image = true;
            }
            else
            {
                spdlog::warn("Could not load forecast image: {}", f_res.error());
            }

            try_remove(forecast_file);
        }
    }

    if (have_any_image) {
        images = img;
    }
    parser.unmark_changed();

    return have_any_image;
}
