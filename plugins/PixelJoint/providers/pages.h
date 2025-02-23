#pragma once

#include "shared/config/image_providers/general.h"
#include "../scraper/scraped_post.h"
#include "wrappers.h"

namespace ImageProviders {
    class Pages : public General {
    private:
        int page_end, page_begin;
        vector<std::unique_ptr<pixeljoint::ScrapedPost, void (*)(pixeljoint::ScrapedPost *)>> curr_posts;
        vector<int> total_pages;

    public:
        ~Pages() override = default;
        void flush() override;

        std::expected<std::optional<std::variant<std::unique_ptr<Post, void(*)(Post *)>, std::shared_ptr<Post>>>, string>
        get_next_image() override;

        json to_json() override;

        string get_name() const override;

        explicit Pages(const json &arguments);
    };

    class PagesWrapper : public Plugins::ImageProviderWrapper {
        std::unique_ptr<ImageProviders::General, void (*)(ImageProviders::General *)> create_default() override;

        std::unique_ptr<ImageProviders::General, void (*)(ImageProviders::General *)>
        from_json(const nlohmann::json &json) override;
    };
}
