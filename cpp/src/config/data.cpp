//
// Created by hendrik on 2/2/24.
//

#include "data.h"
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

namespace ConfigData {
    void to_json(json& j, const ImageType& p)  {
        j = json{{"name", p.img_type, "arguments", p.arguments}};
    }

    void to_json(json& j, const Root& p) {
        json t;
        to_json(t, p.groups);

        j = json{{"groups", t}, {"curr", p.curr}};
    }

    void from_json(const json& j, Root& p) {
        j.at("curr").get_to(p.curr);
        j.at("groups").get_to(p.groups);
    }

    void from_json(const json& j, ImageType& p) {
        j.at("arguments").get_to(p.arguments);
        j.at("name").get_to(p.img_type);
    }
}