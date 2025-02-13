#pragma once

#include <nlohmann/json.hpp>
#include <variant>
#include "shared/post.h"

using json = nlohmann::json;
namespace ImageProviders {
    class General {
    public:
        explicit General(const json &arguments);

        virtual ~General();  // Changed from pure virtual to virtual

        virtual optional<std::variant<std::unique_ptr<Post, void (*)(Post *)>, std::shared_ptr<Post>>>
        get_next_image() = 0;

        virtual void flush() = 0;

        [[nodiscard]] virtual string get_name() const = 0;

        virtual json to_json() = 0;

        static std::unique_ptr<ImageProviders::General, void (*)(ImageProviders::General *)> from_json(const json &j);
    };
}
