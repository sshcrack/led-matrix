#pragma once

#include <string>
#include <optional>
#include "Magick++.h"

using std::string;
using std::vector;
using std::optional;

class Post {
protected:
    string img_url;
    string file_name;
public:
    explicit Post(const string &img_url, bool maybe_fetch_type = true);

    string get_filename();

    string get_image_url();

    optional<vector<Magick::Image>> process_images(int width, int height, bool store_processed_file = false);
};