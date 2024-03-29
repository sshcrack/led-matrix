#pragma once

#include "general.h"
namespace ImageTypes {
    class Collection: public General {
    private:
        vector<Post> images;
        vector<Post> already_shown;

    public:
        optional<Post> get_next_image() override;
        void flush() override;
        json to_json();

        explicit Collection(const json& arguments);
    };
}