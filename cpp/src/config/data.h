#pragma once

#include "image_types/general.h"
#include <nlohmann/json.hpp>
using namespace std;
using json = nlohmann::json;


namespace ConfigData {
    struct Preset {
        string name;
        vector<ImageTypes::General*> categories;

    public:
        void randomize();
    };

    struct Root {
        map<string, Preset> presets;
        string curr;
    };

    void to_json(json& j, const Root& p);
    void to_json(json& j, const Preset& p);
    void to_json(json& j, const ImageTypes::General*& p);

    void from_json(const json& j, Root& p);
    void from_json(const json& j, Preset& p);
    void from_json(const json& j, ImageTypes::General*& p);
}