#pragma once

#include <string>
#include <utility>
#include "shared/utils/utils.h"

class PropertyBase {
protected:
    std::string name;

public:
    explicit PropertyBase(std::string propertyName) : name(std::move(propertyName)) {
    }

    virtual ~PropertyBase() = default;

    virtual void load_from_json(const nlohmann::json &j) = 0;

    virtual void dump_to_json(nlohmann::json &j) const = 0;

    [[nodiscard]] virtual std::string get_type_id() const = 0;

    [[nodiscard]] const std::string &getName() const {
        return name;
    }
};

template<typename T>
class Property final : public PropertyBase {
    T value;
    bool required;
    bool registered;

public:
    explicit Property(const std::string &propertyName, const T &defaultValue,
                      const bool required = false)
        : PropertyBase(propertyName), value(defaultValue), required(required), registered(false) {
    }

    const T &get() const {
        if (!registered)
            throw std::runtime_error("Property not registered");

        return value;
    }


    explicit operator T() const {
        return value;
    }

    void load_from_json(const nlohmann::json &j) override {
        if (required) {
            value = j.at(name);
        } else {
            value = j.value(name, value);
        }

        registered = true;
    }

    void dump_to_json(nlohmann::json &j) const override {
        j[name] = value;
    }

    [[nodiscard]] std::string get_type_id() const override {
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
        else
            return typeid(T).name();
    }
};
