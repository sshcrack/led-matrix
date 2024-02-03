#include "collection.h"
#include "random"

optional<Post> ImageTypes::Collection::get_next_image() {
    if(images.empty())
        return nullopt;

    auto curr_img = images[0];
    images.erase(images.begin());

    already_shown.push_back(curr_img);
}

void ImageTypes::Collection::flush() {
    std::shuffle(already_shown.begin(), already_shown.end(), random_device());

    images.reserve(images.size() + already_shown.size());
    images.insert(images.end(), already_shown.begin(), already_shown.end());

    already_shown.clear();
}

ImageTypes::Collection::Collection(const json &arguments) : General(arguments) {
    vector<string> imgs = arguments.template get<vector<string>>();
    images.reserve(imgs.size());

    for (const auto &item: imgs)
        images.push_back(*new Post(item));
};