#pragma once

#include <string>
#include <utility>
#include <optional>
#include <vector>
#include <type_traits>

#include "graphics.h"
#include "shared/matrix/utils/utils.h"
#include <nlohmann/json.hpp>
#include <magic_enum/magic_enum.hpp>

using json = nlohmann::json;

namespace Plugins {
    // Base class for all enum types used in properties
    class EnumBase {
    public:
        virtual ~EnumBase() = default;
        
        // Get the display name for the current enum value
        virtual std::string get_display_name() const = 0;
        
        // Get all valid enum values as a vector of pairs (value_string, display_name)
        virtual std::vector<std::pair<std::string, std::string>> get_all_values() const = 0;
        
        // Get the current enum value as a string
        virtual std::string get_value_string() const = 0;
        
        // Set the enum value from a string
        virtual bool set_from_string(const std::string& value_str) = 0;
        
        // Get the enum type name
        virtual std::string get_enum_name() const = 0;
    };
    
    // Template implementation for enum types
    template<typename EnumType>
    class EnumProperty : public EnumBase {
        static_assert(std::is_enum_v<EnumType>, "EnumProperty can only be used with enum types");
        
    private:
        EnumType value;
        
    public:
        explicit EnumProperty(EnumType default_value) : value(default_value) {}
        
        EnumType get() const { return value; }
        void set(EnumType new_value) { value = new_value; }
        
        std::string get_display_name() const override {
            // Convert enum to string and make it more readable
            std::string name = std::string(magic_enum::enum_name(value));
            // Convert UPPER_CASE to Title Case
            if (!name.empty()) {
                std::string result;
                bool capitalize_next = true;
                for (char c : name) {
                    if (c == '_') {
                        result += ' ';
                        capitalize_next = true;
                    } else if (capitalize_next) {
                        result += std::toupper(c);
                        capitalize_next = false;
                    } else {
                        result += std::tolower(c);
                    }
                }
                return result;
            }
            return name;
        }
        
        std::vector<std::pair<std::string, std::string>> get_all_values() const override {
            std::vector<std::pair<std::string, std::string>> result;
            for (auto enum_value : magic_enum::enum_values<EnumType>()) {
                std::string value_str = std::string(magic_enum::enum_name(enum_value));
                
                // Create display name (convert UPPER_CASE to Title Case)
                std::string display_name;
                bool capitalize_next = true;
                for (char c : value_str) {
                    if (c == '_') {
                        display_name += ' ';
                        capitalize_next = true;
                    } else if (capitalize_next) {
                        display_name += std::toupper(c);
                        capitalize_next = false;
                    } else {
                        display_name += std::tolower(c);
                    }
                }
                
                result.emplace_back(value_str, display_name);
            }
            return result;
        }
        
        std::string get_value_string() const override {
            return std::string(magic_enum::enum_name(value));
        }
        
        bool set_from_string(const std::string& value_str) override {
            auto enum_value = magic_enum::enum_cast<EnumType>(value_str);
            if (enum_value.has_value()) {
                value = enum_value.value();
                return true;
            }
            return false;
        }
        
        std::string get_enum_name() const override {
            return std::string(magic_enum::enum_type_name<EnumType>());
        }
    };

    class PropertyBase
    {
    protected:
        std::string name;

    public:
        explicit PropertyBase(std::string propertyName) : name(std::move(propertyName))
        {
        }

        virtual ~PropertyBase() = default;

        virtual void load_from_json(const nlohmann::json &j) = 0;

        virtual void dump_to_json(nlohmann::json &j) const = 0;

        virtual void min_max_to_json(nlohmann::json &j) const = 0;

        [[nodiscard]] virtual std::string get_type_id() const = 0;

        [[nodiscard]] const std::string &getName() const
        {
            return name;
        }
    };

    template <typename T>
    class Property final : public PropertyBase
    {
        T value;
        bool required;
        bool registered;
        std::optional<T> min_value;
        std::optional<T> max_value;

    public:
        explicit Property(const std::string &propertyName, const T &defaultValue,
                          const bool required = false,
                          const std::optional<T> &min = std::nullopt,
                          const std::optional<T> &max = std::nullopt)
            : PropertyBase(propertyName), value(defaultValue), required(required),
              registered(false), min_value(min), max_value(max)
        {
        }

        const T &get() const
        {
            if (!registered)
                throw std::runtime_error("Property not registered");

            return value;
        }

        explicit operator T() const
        {
            return value;
        }

        /// This method does not save the properties automatically to config. You'll have to call
        /// the save function on the config object yourself!
        void set_value(const T &new_default)
        {
            value = new_default;
        }

        void load_from_json(const nlohmann::json &j) override
        {
            if (required)
            {
                if (j.contains(name))
                {
                    if constexpr (std::is_base_of_v<EnumBase, T>) {
                        // Handle enum types
                        if (j.at(name).is_string()) {
                            std::string enum_str = j.at(name).get<std::string>();
                            if (!value.set_from_string(enum_str)) {
                                throw std::runtime_error("Invalid enum value '" + enum_str + "' for property '" + name + "'");
                            }
                        } else {
                            throw std::runtime_error("Enum property '" + name + "' must be a string");
                        }
                    } else {
                        value = j.at(name).get<T>();
                    }
                }
                else
                {
                    throw std::runtime_error("Required property '" + name + "' not found in JSON");
                }
            }
            else
            {
                if (j.contains(name))
                {
                    if constexpr (std::is_base_of_v<EnumBase, T>) {
                        // Handle enum types
                        if (j.at(name).is_string()) {
                            std::string enum_str = j.at(name).get<std::string>();
                            if (!value.set_from_string(enum_str)) {
                                // If invalid, keep default value and log warning (could add logging here)
                            }
                        }
                    } else {
                        value = j.at(name).get<T>();
                    }
                }
                // If key doesn't exist, keep the default value
            }

            // Validate against min/max constraints (only for comparable non-enum types)
            if constexpr (std::is_arithmetic_v<T> || std::is_same_v<T, std::string>)
            {
                if constexpr (!std::is_base_of_v<EnumBase, T>) {
                    if (min_value.has_value() && value < min_value.value())
                    {
                        value = min_value.value();
                    }

                    if (max_value.has_value() && value > max_value.value())
                    {
                        value = max_value.value();
                    }
                }
            }

            registered = true;
        }

        void min_max_to_json(nlohmann::json &j) const override
        {
            if constexpr (std::is_base_of_v<EnumBase, T>) {
                // For enum types, add enum metadata instead of min/max
                j["enum_name"] = value.get_enum_name();
                j["enum_values"] = nlohmann::json::array();
                for (const auto& [value_str, display_name] : value.get_all_values()) {
                    nlohmann::json enum_option;
                    enum_option["value"] = value_str;
                    enum_option["display_name"] = display_name;
                    j["enum_values"].push_back(enum_option);
                }
            } else {
                // Standard min/max for non-enum types
                if (min_value.has_value())
                {
                    j["min"] = min_value.value();
                }

                if (max_value.has_value())
                {
                    j["max"] = max_value.value();
                }
            }
        }

        void dump_to_json(nlohmann::json &j) const override
        {
            if constexpr (std::is_base_of_v<EnumBase, T>) {
                // For enum types, store as string
                j[name] = value.get_value_string();
            } else {
                j[name] = value;
            }
        }

        [[nodiscard]] std::string get_type_id() const override
        {
            if constexpr (std::is_base_of_v<EnumBase, T>)
                return "enum";
            else if constexpr (std::is_same_v<T, std::string>)
                return "string";
            else if constexpr (std::is_same_v<T, int>)
                return "int";
            else if constexpr (std::is_same_v<T, std::vector<std::string>>)
                return "string[]";
            else if constexpr (std::is_same_v<T, double>)
                return "double";
            else if constexpr (std::is_same_v<T, bool>)
                return "bool";
            else if constexpr (std::is_same_v<T, float>)
                return "float";
            else if constexpr (std::is_same_v<T, tmillis_t>)
                return "millis";
            else if constexpr (std::is_same_v<T, nlohmann::json>)
                return "json";
            else if constexpr (std::is_same_v<T, int16_t>)
                return "int16_t";
            else if constexpr (std::is_same_v<T, uint8_t>)
                return "uint8_t";
            else if constexpr (std::is_same_v<T, rgb_matrix::Color>)
                return "color";
            else
                return typeid(T).name();
        }

        [[nodiscard]] bool has_min() const
        {
            return min_value.has_value();
        }

        [[nodiscard]] bool has_max() const
        {
            return max_value.has_value();
        }

        [[nodiscard]] const std::optional<T> &get_min() const
        {
            return min_value;
        }

        [[nodiscard]] const std::optional<T> &get_max() const
        {
            return max_value;
        }
    };
}

// Define JSON serialization for rgb_matrix::Color in the nlohmann namespace for better ADL
namespace nlohmann {
  template <>
  struct adl_serializer<rgb_matrix::Color> {
    static void to_json(json& j, const rgb_matrix::Color& c) {
      j = (static_cast<uint32_t>(c.r) << 16) | (static_cast<uint32_t>(c.g) << 8) | static_cast<uint32_t>(c.b);
    }

    static void from_json(const json& j, rgb_matrix::Color& c) {
      const uint32_t color = j.get<uint32_t>();
      c.r = (color >> 16) & 0xFF;
      c.g = (color >> 8) & 0xFF;
      c.b = color & 0xFF;
    }
  };
}