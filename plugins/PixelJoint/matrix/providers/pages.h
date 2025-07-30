#pragma once

#include "shared/matrix/config/image_providers/general.h"
#include "../scraper/scraped_post.h"
#include "shared/matrix/wrappers.h"

namespace ImageProviders {
    class Pages final : public General {
        vector<std::unique_ptr<pixeljoint::ScrapedPost, void (*)(pixeljoint::ScrapedPost *)>> curr_posts;
        vector<int> total_pages;

        int pages_end;
        PropertyPointer<int> pages_begin = MAKE_PROPERTY("pages_begin", int, 0);
        PropertyPointer<int> pages_end_raw = MAKE_PROPERTY("pages_end", int, -1);

    public:
        ~Pages() override = default;
        void flush() override;

        std::expected<std::optional<std::variant<std::unique_ptr<Post, void(*)(Post *)>, std::shared_ptr<Post>>>, string>
        get_next_image() override;

        [[nodiscard]] string get_name() const override;

        explicit Pages();

        void register_properties() override;
        void load_properties(const nlohmann::json &j) override;
    };

    class PagesWrapper : public Plugins::ImageProviderWrapper {
        std::unique_ptr<ImageProviders::General, void (*)(ImageProviders::General *)>
        create() override;
    };
}
