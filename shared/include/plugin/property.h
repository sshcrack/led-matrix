#pragma once

#include <string>
#include <utility>
#include <optional>

#include "color.h"
#include "shared/utils/utils.h"

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
            value = j.at(name);
        }
        else
        {
            value = j.value(name, value);
        }

        // Validate against min/max constraints (only for comparable types)
        if constexpr (std::is_arithmetic_v<T> || std::is_same_v<T, std::string>)
        {
            if (min_value.has_value() && value < min_value.value())
            {
                value = min_value.value();
            }

            if (max_value.has_value() && value > max_value.value())
            {
                value = max_value.value();
            }
        }

        registered = true;
    }

    void min_max_to_json(nlohmann::json &j) const override
    {
        if (min_value.has_value())
        {
            j["min"] = min_value.value();
        }

        if (max_value.has_value())
        {
            j["max"] = max_value.value();
        }
    }

    void dump_to_json(nlohmann::json &j) const override
    {
        j[name] = value;
    }

    [[nodiscard]] std::string get_type_id() const override
    {
        if constexpr (std::is_same_v<T, std::string>)
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
        else if constexpr (std::is_same_v<T, Color>)
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
