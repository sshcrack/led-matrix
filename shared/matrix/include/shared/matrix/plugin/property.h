#pragma once

#include <string>
#include <utility>
#include <optional>
#include <vector>
#include <type_traits>
#include <cmath>
#include <sstream>
#include <stdexcept>

#include "graphics.h"
#include "shared/matrix/utils/utils.h"
#include <nlohmann/json.hpp>
#include <magic_enum/magic_enum.hpp>

using json = nlohmann::json;

namespace Plugins {
    struct PropertyUiMetadata {
        std::string label;
        std::string description;
        std::string group;
        std::string unit;
        std::string control;
        std::optional<double> step;
        nlohmann::json presets = nlohmann::json::array();
        bool advanced = false;
        std::string visible_if_property;
        nlohmann::json visible_if_value;
    };
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
        PropertyUiMetadata ui_metadata_{};

        void append_ui_metadata(nlohmann::json &j) const {
            if (!ui_metadata_.label.empty()) j["label"] = ui_metadata_.label;
            if (!ui_metadata_.description.empty()) j["description"] = ui_metadata_.description;
            if (!ui_metadata_.group.empty()) j["group"] = ui_metadata_.group;
            if (!ui_metadata_.unit.empty()) j["unit"] = ui_metadata_.unit;
            if (!ui_metadata_.control.empty()) j["control"] = ui_metadata_.control;
            if (ui_metadata_.step.has_value()) j["step"] = *ui_metadata_.step;
            if (ui_metadata_.presets.is_array() && !ui_metadata_.presets.empty()) j["presets"] = ui_metadata_.presets;
            if (ui_metadata_.advanced) j["advanced"] = true;
            if (!ui_metadata_.visible_if_property.empty()) {
                j["visible_if"] = {
                    {"property", ui_metadata_.visible_if_property},
                    {"equals", ui_metadata_.visible_if_value}
                };
            }
        }

    public:
        explicit PropertyBase(std::string propertyName) : name(std::move(propertyName))
        {
        }

        virtual ~PropertyBase() = default;

        virtual void load_from_json(const nlohmann::json &j) = 0;

        virtual void dump_to_json(nlohmann::json &j) const = 0;

        virtual void add_additional_data(nlohmann::json &j) const = 0;

        [[nodiscard]] virtual std::string get_type_id() const = 0;
        [[nodiscard]] virtual std::vector<std::string> validate_schema() const = 0;
        [[nodiscard]] virtual bool is_required() const = 0;

        PropertyBase &label(std::string value) { ui_metadata_.label = std::move(value); return *this; }
        PropertyBase &description(std::string value) { ui_metadata_.description = std::move(value); return *this; }
        PropertyBase &group(std::string value) { ui_metadata_.group = std::move(value); return *this; }
        PropertyBase &unit(std::string value) { ui_metadata_.unit = std::move(value); return *this; }
        PropertyBase &control(std::string value) { ui_metadata_.control = std::move(value); return *this; }
        PropertyBase &step(double value) { ui_metadata_.step = value; return *this; }
        PropertyBase &presets(nlohmann::json value) { ui_metadata_.presets = std::move(value); return *this; }
        PropertyBase &advanced(bool value = true) { ui_metadata_.advanced = value; return *this; }
        PropertyBase &visible_if(std::string property, nlohmann::json value) {
            ui_metadata_.visible_if_property = std::move(property);
            ui_metadata_.visible_if_value = std::move(value);
            return *this;
        }
        [[nodiscard]] const PropertyUiMetadata &ui_metadata() const { return ui_metadata_; }

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
                throw std::runtime_error("Property " + getName() + " not registered");

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
                                throw std::runtime_error("Invalid enum value '" + enum_str + "' for property '" + name + "'");
                            }
                        } else {
                            throw std::runtime_error("Enum property '" + name + "' must be a string");
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

        void add_additional_data(nlohmann::json &j) const override
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
            append_ui_metadata(j);
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

        [[nodiscard]] bool is_required() const override { return required; }

        [[nodiscard]] std::vector<std::string> validate_schema() const override
        {
            std::vector<std::string> issues;
            if (name.empty())
                issues.emplace_back("property name is empty");
            if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos)
                issues.emplace_back("property name contains a path separator");
            for (unsigned char c : name) {
                if (c < 0x20) {
                    issues.emplace_back("property name contains a control character");
                    break;
                }
            }

            if constexpr (std::is_arithmetic_v<T>) {
                if (min_value && max_value && *min_value > *max_value)
                    issues.emplace_back("minimum is greater than maximum");
                if (min_value && value < *min_value)
                    issues.emplace_back("default value is below minimum");
                if (max_value && value > *max_value)
                    issues.emplace_back("default value is above maximum");
                if constexpr (std::is_floating_point_v<T>) {
                    if (!std::isfinite(value)) issues.emplace_back("default value is not finite");
                    if (min_value && !std::isfinite(*min_value)) issues.emplace_back("minimum is not finite");
                    if (max_value && !std::isfinite(*max_value)) issues.emplace_back("maximum is not finite");
                }
            } else if constexpr (std::is_same_v<T, std::string>) {
                if (min_value && max_value && *min_value > *max_value)
                    issues.emplace_back("minimum string is greater than maximum string");
                if (min_value && value < *min_value)
                    issues.emplace_back("default value is below minimum");
                if (max_value && value > *max_value)
                    issues.emplace_back("default value is above maximum");
            } else if constexpr (std::is_base_of_v<EnumBase, T>) {
                const auto values = value.get_all_values();
                if (values.empty()) issues.emplace_back("enum has no values");
                if (value.get_value_string().empty()) issues.emplace_back("enum default is not a valid enumerator");
            }

            if (ui_metadata_.step.has_value() && *ui_metadata_.step <= 0.0)
                issues.emplace_back("UI step must be greater than zero");
            if (!ui_metadata_.presets.is_array())
                issues.emplace_back("UI presets must be an array");
            if (!ui_metadata_.visible_if_property.empty() && ui_metadata_.visible_if_property == name)
                issues.emplace_back("visibility condition references the property itself");
            return issues;
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