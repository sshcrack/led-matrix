#ifdef _WIN32
#include "shared/common/win_compat.h"
#endif
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

std::optional<std::vector<Magick::Image>> Post::process_images(const int width, const int height, const bool store_processed_file) {
    spdlog::trace("Preprocessing img {}", img_url);
    
    try {
        if (!std::filesystem::exists(Constants::post_dir)) {
            if (!std::filesystem::create_directory(Constants::post_dir)) {
                spdlog::error("Could not create directory at {}.", Constants::post_dir.string());
                return std::nullopt;
            }
        }

        const tmillis_t start_loading = GetTimeInMillis();
        const std::filesystem::path file_path = Constants::post_dir / get_filename();

        // Persistent caches keep the original so new matrix sizes can regenerate
        // full-quality transforms. Uncached downloads are removed after decoding.
        if (!exists(file_path)) {
            const auto res = utils::download_image(get_image_url(), file_path.string(), {},
                                                   utils::DownloadOptions{.block_internal_hosts = block_internal_hosts});
            if (!res) {
                spdlog::error("Could not download image: {}", res.error());
                return std::nullopt;
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


        if (!res) {
            try_remove(file_path);
            spdlog::error("Error loading image: {}", res.error());
            return std::nullopt;
        }

        if (!store_processed_file && !utils::is_local_file_url(get_image_url())) try_remove(file_path);

        spdlog::trace("Loading/Scaling Image took {}s.", (GetTimeInMillis() - start_loading) / 1000.0);
        return std::move(res.value());

    } catch (std::exception &e) {
        spdlog::error("Exception in process_images: {}", e.what());
        return std::nullopt;
    }
}

Post::Post(const std::string &url, const bool maybe_fetch_type) : Post(url, maybe_fetch_type, false) {}

Post::Post(const std::string &url, const bool maybe_fetch_type, const bool block_internal) {
    this->img_url = url;
    this->block_internal_hosts = block_internal;

    // Derive the suffix from the URL path, not dots in the hostname or query.
    const auto url_path = img_url.substr(0, img_url.find_first_of("?#"));
    std::string file_ext = std::filesystem::path(url_path).extension().string();
    if (file_ext.empty()) file_ext = ".unknown";

    if (maybe_fetch_type && utils::is_http_url(img_url) && (MimeTypes::getType("file" + file_ext) == "application/octet-stream")) {
        const auto content_type = utils::fetch_content_type(
            img_url, utils::DownloadOptions{.block_internal_hosts = block_internal_hosts});
        if (!content_type) {
            spdlog::warn("Could not fetch Content-Type for {}", img_url);
        } else {
            const auto ext = MimeTypes::getExtension(*content_type);
            if (!ext.empty())
                file_ext = "." + ext;
            spdlog::trace("Content-Type for {} is {}", img_url, file_ext);
        }
    }

    picosha2::hash256_hex_string(this->img_url, this->file_name);
    this->file_name += file_ext;
}

std::string Post::get_filename() {
    return file_name;
}

std::string Post::get_image_url() {
    return img_url;
}
