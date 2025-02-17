#pragma once

#include "shared/config/image_providers/general.h"
#include "wrappers.h"

namespace ImageProviders {
    class Collection : public General {
        vector<std::shared_ptr<Post> > images;
        vector<std::shared_ptr<Post> > already_shown;

    public:
        optional<std::variant<std::unique_ptr<Post, void (*)(Post *)>, std::shared_ptr<Post> > >
        get_next_image() override;

        ~Collection() override = default;

        void flush() override;

        json to_json() override;

        string get_name() const override;

        explicit Collection(const json &arguments);
    };


    class CollectionWrapper : public Plugins::ImageProviderWrapper {
        std::unique_ptr<General, void (*)(General *)> create_default() override;

        std::unique_ptr<General, void (*)(General *)>
        from_json(const nlohmann::json &json) override;
    };
}
