#ifndef MAIN_DATA_H
#define MAIN_DATA_H

#include <nlohmann/json.hpp>
using namespace std;
using json = nlohmann::json;


namespace ConfigData {
    struct ImageType {
        string img_type;
        vector<json> arguments;
    };

    struct Groups {
        string name;
        vector<ImageType> images;
    };

    struct Root {
        map<string, Groups> groups;
        string curr;
    };

    void to_json(json& j, const Root& p);
    void to_json(json& j, const ImageType& p);

    void from_json(const json& j, Root& p);
    void from_json(const json& j, ImageType& p);
}


#endif //MAIN_DATA_H
