#pragma once

#include <string>
#include <atomic>

namespace Constants {
    const static std::string root_dir = "images/";
    const static std::string post_img_url = "/files/icons/full/";
    extern std::atomic<int> width;
    extern std::atomic<int> height;
}