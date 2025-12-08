#pragma once

#include "nlohmann/json.hpp"
#include <expected>
#include "fmt/format.h"

#include "shared/matrix/plugin/property.h"

using json = nlohmann::json;

namespace ShaderProviders {
    class General {
        std::vector<std::shared_ptr<Plugins::PropertyBase>> properties;
        std::string uuid;

    public:
        explicit General();

        virtual ~General();

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

        std::vector<std::shared_ptr<Plugins::PropertyBase>> get_properties() {
            return properties;
        }

        virtual std::expected<std::string, std::string> get_next_shader() = 0;

        virtual void flush() = 0;

        virtual void tick() {}

        [[nodiscard]] virtual std::string get_name() const = 0;

        [[nodiscard]] virtual json to_json() const;

        static std::unique_ptr<General, void (*)(General *)> from_json(const json &j);
    };
}
