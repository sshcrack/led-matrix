#pragma once

#include <string>
#include <utility>

class PropertyBase {
public:
    virtual void load_from_json(const nlohmann::json &j) = 0;

    virtual void dump_to_json(nlohmann::json &j) const = 0;
};

template<typename T>
class Property : public PropertyBase {
private:
    T value;
    bool required;
    bool registered;
    std::string name;

public:
    explicit Property(const std::string &propertyName, const T &defaultValue,
                      bool required = false)
            : value(defaultValue), name(std::move(propertyName)), required(required) {}

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

    [[nodiscard]] const std::string &getName() const {
        return name;
    }
};

