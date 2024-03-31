#pragma once
#include "../../../../plugins/PixelJoint/scraper/post.h"
#include "nlohmann/json.hpp"
#include "fmt/core.h"

using json = nlohmann::json;
namespace ImageProviders {
    class General {
    private:
        const json& initial_arguments;
    public:
        explicit General(const json& arguments);
        virtual optional<Post> get_next_image() = 0;
        virtual void flush() = 0;

        virtual json to_json() = 0;

        static General* from_json(const json& j);
    };
}
