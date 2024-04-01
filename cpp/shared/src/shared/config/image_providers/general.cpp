#include "config/image_providers/general.h"
#include "spdlog/spdlog.h"
#include <stdexcept>
#include "fmt/core.h"


ImageProviders::General::General(const json& arguments) : initial_arguments(arguments) {
    spdlog::debug("Initial arguments for general are {}", arguments.dump());
}

ImageProviders::General* ImageProviders::General::from_json(const json &j) {
    spdlog::debug("Getting type of {}", to_string(j));
    string t = j["type"].get<string>();
    const json& arguments = j["arguments"];
/*
    if(t == "pages")
        return new ImageProviders::Pages(arguments);

    if(t == "collection")
        return new ImageProviders::Collection(arguments);
*/
    throw std::runtime_error(fmt::format("Invalid type {}", t));
}
