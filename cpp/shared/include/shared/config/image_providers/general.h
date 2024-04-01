#pragma once
#include <nlohmann/json.hpp>
#include "shared/post.h"

using json = nlohmann::json;
namespace ImageProviders {
    class General {
    public:
        explicit General(const json& arguments);
        virtual optional<Post> get_next_image() = 0;
        virtual void flush() = 0;

        virtual json to_json() = 0;

        static General* from_json(const json& j);
    };
}
