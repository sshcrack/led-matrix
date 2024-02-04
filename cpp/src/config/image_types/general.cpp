#include "general.h"
#include "pages.h"
#include "collection.h"
#include "spdlog/spdlog.h"
#include <stdexcept>
#include "fmt/core.h"


ImageTypes::General::General(const json& arguments) : initial_arguments(arguments) {
    spdlog::debug("Initial arguments for general are {}", arguments.dump());
}

ImageTypes::General* ImageTypes::General::from_json(const json &j) {
    spdlog::debug("Getting type of {}", to_string(j));
    string t = j["type"].get<string>();
    const json& arguments = j["argument"];

    if(t == "pages")
        return new ImageTypes::Pages(arguments);

    if(t == "collection")
        return new ImageTypes::Collection(arguments);

    throw std::runtime_error(fmt::format("Invalid type {}", t));
}
