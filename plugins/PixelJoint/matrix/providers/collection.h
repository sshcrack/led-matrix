#pragma once

#include "shared/matrix/config/image_providers/general.h"
#include "shared/matrix/wrappers.h"

namespace ImageProviders {
    class Collection final : public General {
        vector<std::shared_ptr<Post> > images;
        vector<std::shared_ptr<Post> > already_shown;

        PropertyPointer<std::vector<string>> images_raw = MAKE_PROPERTY("images", std::vector<string>, {});
    public:
        std::expected<std::optional<std::variant<std::unique_ptr<Post, void(*)(Post *)>, std::shared_ptr<Post>>>, string>
        get_next_image() override;

        ~Collection() override = default;

        void flush() override;

        [[nodiscard]] string get_name() const override;

        explicit Collection();

        void register_properties() override;
        void load_properties(const nlohmann::json &j) override;
    };


    class CollectionWrapper final : public Plugins::ImageProviderWrapper {
        std::unique_ptr<General, void (*)(General *)>
        create() override;
    };
}
