#ifndef MAIN_PAGES_H
#define MAIN_PAGES_H


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
        explicit Pages(const json& arguments);
    };
}

#endif //MAIN_PAGES_H
