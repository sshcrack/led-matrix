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
    bool block_internal_hosts = false;
public:
    virtual ~Post() = default;
    explicit Post(const string &img_url, bool maybe_fetch_type = true);
    // For URLs supplied directly by API clients: refuse loopback/link-local destinations.
    Post(const string &img_url, bool maybe_fetch_type, bool block_internal_hosts);

    string get_filename();

    string get_image_url();

    optional<vector<Magick::Image>> process_images(int width, int height, bool store_processed_file = false);
};