#pragma once
#include "../../matrix_control/post.h"
#include "nlohmann/json.hpp"
#include "fmt/core.h"

using json = nlohmann::json;
namespace ImageTypes {
    class General {
    private:
        json initial_arguments;
    public:
        explicit General(const json& arguments);
        virtual optional<Post> get_next_image() = 0;
        virtual void flush() = 0;

        [[nodiscard]] virtual json to_json() const {
            throw runtime_error(fmt::format("Can not convert general to const (Initial arguments: {}", to_string(initial_arguments)));
        };

        static General* from_json(const json& j);
    };
}
