#pragma once

#include <string>
#include <utility>

class PropertyBase {
protected:
    std::string name;
public:
    explicit PropertyBase(std::string propertyName) : name(std::move(propertyName)) {}

    virtual void load_from_json(const nlohmann::json &j) = 0;

    virtual void dump_to_json(nlohmann::json &j) const = 0;

    [[nodiscard]] const std::string &getName() const {
        return name;
    }
};

template<typename T>
class Property : public PropertyBase {
private:
    T value;
    bool required;
    bool registered;

public:
    explicit Property(const std::string &propertyName, const T &defaultValue,
                      bool required = false)
            : value(defaultValue), PropertyBase(std::move(propertyName)), required(required) {}

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
};

