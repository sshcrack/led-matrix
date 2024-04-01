#pragma once

#include "shared/post.h"
#include <string>
#include <optional>
#include <Magick++.h>
#include <vector>

using namespace std;

class ScrapedPost {
protected:
    string thumbnail;
    string post_url;
    optional<Post> cached_post;
public:
    optional<Post> fetch_link();

    string get_post_url();

    ScrapedPost(string thumbnail, string url) {
        this->thumbnail = std::move(thumbnail);
        this->post_url = std::move(url);
    }

    static std::vector<ScrapedPost> get_posts(int page);

    static std::optional<int> get_pages();
};