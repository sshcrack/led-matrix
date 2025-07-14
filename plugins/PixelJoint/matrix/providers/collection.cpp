#include "collection.h"
#include <random>
#include "spdlog/spdlog.h"

using std::nullopt;

std::expected<std::optional<std::variant<std::unique_ptr<Post, void(*)(Post *)>, std::shared_ptr<Post> > >, string>
ImageProviders::Collection::get_next_image() {
    if (images.empty()) {
        return nullopt;
    }

    auto curr_img = images[0];
    images.erase(images.begin());

    already_shown.push_back(curr_img);

    return curr_img;
}

void ImageProviders::Collection::flush() {
    images.reserve(images.size() + already_shown.size());
    images.insert(images.end(), already_shown.begin(), already_shown.end());

    already_shown.clear();
    std::ranges::shuffle(images, std::random_device());
}

ImageProviders::Collection::Collection() : General() {
}

void ImageProviders::Collection::register_properties() {
    add_property(images_raw);
}

void ImageProviders::Collection::load_properties(const nlohmann::json &j) {
    General::load_properties(j);
    images.reserve(images_raw->get().size());

    spdlog::trace("Adding a total of {} images to collection", images_raw->get().size());
    for (const auto &item: images_raw->get()) {
        images.push_back({
            new Post(item), [](Post *post) {
                delete post;
            }
        });
    }
}


string ImageProviders::Collection::get_name() const {
    return "collection";
}

std::unique_ptr<ImageProviders::General, void (*)(ImageProviders::General *)>
ImageProviders::CollectionWrapper::create() {
    return {
        new Collection(), [](General *p) {
            delete p;
        }
    };
}
