#include "shared/post.h"
#include "shared/utils/image_fetch.h"
#include <vector>
#include <iostream>
#include <optional>
#include <Magick++.h>
#include "shared/utils/canvas_image.h"
#include "shared/utils/utils.h"
#include "shared/utils/consts.h"
#include "spdlog/spdlog.h"
#include "picosha2.h"

using namespace spdlog;
using namespace std;


string root_dir = Constants::root_dir;

optional<vector<Magick::Image>> Post::process_images(int width, int height) {
    debug("Preprocessing img {}", img_url);
    if (!filesystem::exists(root_dir)) {
        try {
            auto res = filesystem::create_directory(root_dir);
            if (!res) {
                error("Could not create directory at {}.", root_dir);
                exit(-1);
            }
        } catch (exception &ex) {
            error("Could not create directory at {} with exception: {}", root_dir, ex.what());
            exit(-1);
        }
    }

    const tmillis_t start_loading = GetTimeInMillis();
    const string file_path = filesystem::path(root_dir + get_filename());
    filesystem::path processed_img = to_processed_path(file_path);

    // Downloading image first
    if (!exists(processed_img)) {
        try_remove(file_path);
        auto res = utils::download_image(get_image_url(), file_path);
        if (!res.has_value()) {
            error("Could not download image: {}", res.error());
            try_remove(file_path);

            return nullopt;
        }
    }

    constexpr bool contain_img = true;
    auto res = LoadImageAndScale(file_path, width, height, true, true, contain_img);
    if (!res) {
        error("Error loading image: {}", res.error());
        try_remove(file_path);

        return nullopt;
    }

    vector<Magick::Image> frames = std::move(res.value());
    try_remove(file_path);


    debug("Loading/Scaling Image took {}s.", (GetTimeInMillis() - start_loading) / 1000.0);

    return frames;
}

Post::Post(const string &img_url) {
    this->img_url = img_url;

    auto last_index = img_url.find_last_of('.');
    string file_ext = ".unknown";
    if (last_index != std::string::npos) {
        file_ext = img_url.substr(img_url.find_last_of('.'));
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
