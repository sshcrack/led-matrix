#pragma once

#include "shared/config/image_providers/general.h"
#include "../scraper/scraped_post.h.disabled"
#include "wrappers.h"

namespace ImageProviders {
    class Pages : public General {
    private:
        int page_end, page_begin;
        vector<ScrapedPost> curr_posts;
        vector<int> total_pages;

    public:
        void flush() override;

        optional<Post> get_next_image() override;

        json to_json() override;

        explicit Pages(const json &arguments);
    };

    class PagesWrapper : public Plugins::ImageProviderWrapper {
        ImageProviders::General *create_default() override;

        ImageProviders::General *from_json(const nlohmann::json &json) override;

        string get_name() override;
    };
}
