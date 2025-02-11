#pragma once

#include <nlohmann/json.hpp>
#include "shared/post.h"

using json = nlohmann::json;
namespace ImageProviders {
    class General {
    public:
        explicit General(const json &arguments);

        virtual ~General() = default;

        virtual optional<std::unique_ptr<Post, void (*)(Post *)>> get_next_image() = 0;

        virtual void flush() = 0;

        [[nodiscard]] virtual string get_name() const = 0;

        virtual json to_json() = 0;

        static std::unique_ptr<ImageProviders::General, void (*)(ImageProviders::General *)> from_json(const json &j);
    };
}
