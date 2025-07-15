#pragma once

#include "nlohmann/json.hpp"
#include <variant>
#include <expected>
#include "fmt/format.h"

#include "shared/matrix/plugin/property.h"
#include "shared/matrix/post.h"

using json = nlohmann::json;

namespace ImageProviders {
    class General {
        std::vector<std::shared_ptr<Plugins::PropertyBase> > properties;
        std::string uuid;

    public:
        explicit General();

        virtual ~General(); // Changed from pure virtual to virtual

        void add_property(const std::shared_ptr<Plugins::PropertyBase> &property) {
            std::string name = property->getName();
            for (const auto &item: properties) {
                if (item->getName() == name) {
                    throw std::runtime_error(fmt::format("Property with name '{}' already exists", name));
                }
            }

            properties.push_back(property);
        }

        virtual void update_default_properties() {}
        virtual void register_properties() = 0;
        virtual void load_properties(const nlohmann::json &j);

        std::vector<std::shared_ptr<Plugins::PropertyBase> > get_properties() {
            return properties;
        }

        virtual std::expected<std::optional<std::variant<std::unique_ptr<Post, void(*)(Post *)>, std::shared_ptr<
            Post> > >, string>
        get_next_image() = 0;

        virtual void flush() = 0;

        [[nodiscard]] virtual string get_name() const = 0;

        [[nodiscard]] virtual json to_json() const;

        static std::unique_ptr<General, void (*)(General *)> from_json(const json &j);
    };
}
