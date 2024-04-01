#pragma once

#include "config/image_providers/general.h"

namespace ImageProviders {
    class Collection : public General {
    private:
        vector<Post> images;
        vector<Post> already_shown;

    public:
        optional<Post> get_next_image() override;

        void flush() override;

        json to_json() override;

        explicit Collection(const json &arguments);
    };
}