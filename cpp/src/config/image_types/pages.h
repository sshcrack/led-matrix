#pragma once

#include "general.h"
namespace ImageTypes {
    class Pages: public General {
    private:
        int page_end, page_begin;
        vector<ScrapedPost> curr_posts;
        vector<int> total_pages;

    public:
        void flush() override;
        optional<Post> get_next_image() override;
        json to_json() override;
        explicit Pages(const json& arguments);
    };
}
