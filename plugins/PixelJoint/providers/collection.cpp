#include "collection.h"
#include <random>
#include <spdlog/spdlog.h>

using std::nullopt;

optional<std::variant<std::unique_ptr<Post, void (*)(Post *)>, std::shared_ptr<Post>>>
ImageProviders::Collection::get_next_image() {
    if (images.empty()) {
        spdlog::debug("Empty, returning...");
        return nullopt;
    }

    auto curr_img = images[0];
    images.erase(images.begin());

    already_shown.push_back(curr_img);

    return curr_img;
}

void ImageProviders::Collection::flush() {
    std::ranges::shuffle(already_shown, std::random_device());

    images.reserve(images.size() + already_shown.size());
    images.insert(images.end(), already_shown.begin(), already_shown.end());

    already_shown.clear();
}

ImageProviders::Collection::Collection(const json &arguments) : General(arguments) {
    vector<string> imgs = arguments.get<vector<string>>();
    images.reserve(imgs.size());

    for (const auto &item: imgs) {
        images.push_back({new Post(item), [](Post *post) {
            delete post;
        }});
    }
}

json ImageProviders::Collection::to_json() {
    vector<std::shared_ptr<Post>> total = images;
    total.reserve(images.size() + already_shown.size());

    total.insert(total.end(), already_shown.begin(), already_shown.end());

    vector<string> stringified;
    stringified.reserve(total.size());

    for (auto item: total)
        stringified.push_back(item->get_image_url());

    return stringified;
}

string ImageProviders::Collection::get_name() const {
    return "collection";
}


std::unique_ptr<ImageProviders::General, void (*)(ImageProviders::General *)>
ImageProviders::CollectionWrapper::create_default() {
    return {new Collection(json::parse("[]")), [](General *scene) {
        delete scene;
    }};
}

std::unique_ptr<ImageProviders::General, void (*)(ImageProviders::General *)>
ImageProviders::CollectionWrapper::from_json(const json &json) {
    return {new Collection(json), [](General *p) {
        delete p;
    }};
}
