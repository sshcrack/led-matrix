#include "collection.h"
#include "random"
#include "../../utils/consts.h"
#include "spdlog/spdlog.h"

optional<Post> ImageProviders::Collection::get_next_image() {
    if(images.empty()) {
        spdlog::debug("Empty, returning...");
        return nullopt;
    }

    auto curr_img = images[0];
    images.erase(images.begin());

    already_shown.push_back(curr_img);

    return curr_img;
}

void ImageProviders::Collection::flush() {
    std::shuffle(already_shown.begin(), already_shown.end(), random_device());

    images.reserve(images.size() + already_shown.size());
    images.insert(images.end(), already_shown.begin(), already_shown.end());

    already_shown.clear();
}

ImageProviders::Collection::Collection(const json &arguments) : General(arguments) {
    vector<string> imgs = arguments.template get<vector<string>>();
    images.reserve(imgs.size());

    for (const auto &item: imgs) {
        spdlog::debug("Adding {} to group", item);
        images.push_back(*new Post(item));
    }
}

json ImageProviders::Collection::to_json() {
    vector<Post> total = images;
    total.reserve(images.size() + already_shown.size());

    total.insert(total.end(), already_shown.begin(), already_shown.end());

    vector<string> stringified;
    stringified.reserve(total.size());

    for (auto item: total)
        stringified.push_back(item.get_image_name());

    return json{
            {"type", "collection"},
            {"arguments", stringified}
    };
};