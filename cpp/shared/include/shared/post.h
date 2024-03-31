#pragma once

#include <string>
#include <optional>
#include <Magick++.h>
#include "picosha2.h"

using std::string;
using std::vector;
using std::optional;

class Post {
protected:
    string img_url;
    string file_name;
public:
    explicit Post(const string &img_url) {
        this->img_url = img_url;

        auto last_index = img_url.find_last_of('.');
        string file_ext = ".unknown";
        if (last_index != std::string::npos) {
            file_ext = img_url.substr(img_url.find_last_of('.'));
        }

        picosha2::hash256_hex_string(this->img_url, this->file_name);
        this->file_name += file_ext;
    }

    string get_filename() {
        return this->file_name;
    }

    string get_image_url() {
        return this->img_url;
    };

    optional<vector<Magick::Image>> process_images(int width, int height);
};