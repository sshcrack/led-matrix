#include "general.h"
#include "pages.h"
#include "collection.h"
#include <stdexcept>
#include "fmt/core.h"


ImageTypes::General::General(const json& arguments) {
}

ImageTypes::General* ImageTypes::General::from_json(const json &j) {
    string t = j["type"].get<string>();
    const json& arguments = j["arguments"];

    if(t == "pages")
        return new ImageTypes::Pages(arguments);

    if(t == "collection")
        return new ImageTypes::Collection(arguments);

    throw std::runtime_error(fmt::format("Invalid type {}", t));
}
