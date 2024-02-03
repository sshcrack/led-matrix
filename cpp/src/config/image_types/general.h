#ifndef MAIN_GENERAL_H
#define MAIN_GENERAL_H
#include "../../matrix_control/post.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;
namespace ImageTypes {
    class General {
    public:
        explicit General(const json& arguments);
        virtual optional<Post> get_next_image() = 0;
        virtual void flush() = 0;
    };
}

#endif //MAIN_GENERAL_H
