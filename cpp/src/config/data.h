#pragma once

#include "image_types/general.h"
#include <nlohmann/json.hpp>
using namespace std;
using json = nlohmann::json;


namespace ConfigData {
    struct Group {
        string name;
        vector<ImageTypes::General*> images;

    public:
        void randomize();
    };

    struct Root {
        map<string, Group> groups;
        string curr;
    };

    void to_json(json& j, const Root& p);
    void to_json(json& j, const Group& p);
    void to_json(json& j, const ImageTypes::General*& p);

    void from_json(const json& j, Root& p);
    void from_json(const json& j, Group& p);
    void from_json(const json& j, ImageTypes::General*& p);
}