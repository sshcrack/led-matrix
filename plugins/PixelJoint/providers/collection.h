#pragma once

#include "shared/config/image_providers/general.h"
#include "wrappers.h"

namespace ImageProviders {
    class Collection : public General {
    private:
        vector<Post> images;
        vector<Post> already_shown;

    public:
        optional<Post> get_next_image() override;

        void flush() override;

        json to_json() override;
        string get_name() const override;

        explicit Collection(const json &arguments);
    };


    class CollectionWrapper : public Plugins::ImageProviderWrapper {
        ImageProviders::General * create_default() override;
        ImageProviders::General * from_json(const nlohmann::json &json) override;
    };
}