#pragma once

#include <string>
#include <optional>
#include <Magick++.h>
#include <vector>

using namespace std;
class Post
{
protected:
    string image_url;
    string file_name;
public:
    explicit Post(const string& src) {
        this->image_url = src;
        this->file_name = src.substr(src.find_last_of('/') +1);
    }

    string get_filename();
    string get_image_url();
    optional<vector<Magick::Image>> process_images(int width, int height);
};

class ScrapedPost: public Post
{
protected:
    string thumbnail;
    string post_url;
public:
    bool fetch_link();
    bool has_fetched_image();
    string get_post_url();

    ScrapedPost(string thumbnail, string url): Post(thumbnail) {
        this->thumbnail = std::move(thumbnail);
        this->post_url = std::move(url);
    }

    static std::vector<ScrapedPost> get_posts(int page);
    static std::optional<int> get_pages();
};