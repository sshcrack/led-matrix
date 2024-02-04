#pragma once

#include "image_types/general.h"
#include <nlohmann/json.hpp>
using namespace std;
using json = nlohmann::json;


namespace ConfigData {
    struct Groups {
        string name;
        vector<ImageTypes::General*> images;
    };

    struct Root {
        map<string, Groups> groups;
        string curr;
    };

    void to_json(json& j, const Root& p);
    void to_json(json& j, const Groups& p);
    void to_json(json& j, const ImageTypes::General*& p);

    void from_json(const json& j, Root& p);
    void from_json(const json& j, Groups& p);
    void from_json(const json& j, ImageTypes::General*& p);
}