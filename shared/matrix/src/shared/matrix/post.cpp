#include "shared/matrix/post.h"
#include "shared/matrix/utils/image_fetch.h"
#include <vector>
#include <optional>
#include <shared/matrix/server/MimeTypes.h>

#include "Magick++.h"
#include "cpr/cpr.h"

#include "shared/matrix/utils/canvas_image.h"
#include "shared/matrix/utils/utils.h"
#include "shared/matrix/utils/consts.h"
#include "spdlog/spdlog.h"
#include "picosha2.h"

using namespace std;

optional<vector<Magick::Image>> Post::process_images(const int width, const int height, const bool store_processed_file) {
    spdlog::debug("Preprocessing img {}", img_url);
    
    try {
        if (!filesystem::exists(Constants::post_dir)) {
            if (!filesystem::create_directory(Constants::post_dir)) {
                spdlog::error("Could not create directory at {}.", Constants::post_dir.c_str());
                return nullopt;
            }
        }

        const tmillis_t start_loading = GetTimeInMillis();
        const filesystem::path file_path = Constants::post_dir / get_filename();
        const filesystem::path processed_img = to_processed_path(file_path);

        // Early return if processed image exists
        if (exists(processed_img)) {
            auto res = LoadImageAndScale(processed_img, width, height, true, true, true, false);
            if (res) {
                return std::move(res.value());
            }
        }

        // Download image if needed
        if (!exists(processed_img)) {
            try_remove(file_path);
            const auto res = utils::download_image(get_image_url(), file_path);
            if (!res) {
                spdlog::error("Could not download image: {}", res.error());
                return nullopt;
            }
        }

        constexpr bool contain_img = true;
        auto res = LoadImageAndScale(
            file_path,
            width, height,
            true, true,
            contain_img,
            store_processed_file || utils::is_local_file_url(get_image_url())
        );

        try_remove(file_path);

        if (!res) {
            spdlog::error("Error loading image: {}", res.error());
            return nullopt;
        }

        spdlog::debug("Loading/Scaling Image took {}s.", (GetTimeInMillis() - start_loading) / 1000.0);
        return std::move(res.value());

    } catch (std::exception &e) {
        spdlog::error("Exception in process_images: {}", e.what());
        return nullopt;
    }
}

Post::Post(const string &img_url, const bool maybe_fetch_type) {
    this->img_url = img_url;

    const auto last_index = img_url.find_last_of('.');
    string file_ext = ".unknown";
    if (last_index != std::string::npos) {
        file_ext = img_url.substr(img_url.find_last_of('.'));
    }

    if (maybe_fetch_type && (MimeTypes::getType("file" + file_ext) == "application/octet-stream")) {
        // Fetch Content-Type online
        cpr::Response res = cpr::Head(cpr::Url(img_url));
        if (res.status_code < 200 || res.status_code >= 300) {
            spdlog::warn("Could not fetch Content-Type for {}: {}", img_url, res.status_code);
        } else {
            const auto ext = MimeTypes::getExtension(res.header["content-type"]);
            if (!ext.empty())
                file_ext = "." + ext;
            spdlog::trace("Content-Type for {} is {}", img_url, file_ext);
        }
    }

    picosha2::hash256_hex_string(this->img_url, this->file_name);
    this->file_name += file_ext;
}

string Post::get_filename() {
    return file_name;
}

string Post::get_image_url() {
    return img_url;
}
