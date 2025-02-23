#include "pages.h"
#include <random>
#include <spdlog/spdlog.h>
#include <stdexcept>

vector<int> generate_rand_pages(int page_begin, int page_end) {
    vector<int> total_p;
    total_p.reserve(page_end - page_begin);

    // Page count starts at one, that's why we are adding one
    for (int i = page_begin; i < page_end; ++i)
        total_p.push_back(i + 1);

    std::ranges::shuffle(total_p, std::random_device());

    return total_p;
}


void ImageProviders::Pages::flush() {
    curr_posts.clear();
    total_pages = generate_rand_pages(pages_begin->get(), pages_end);
}


std::expected<std::optional<std::variant<std::unique_ptr<Post, void(*)(Post *)>, std::shared_ptr<Post> > >, string>
ImageProviders::Pages::get_next_image() {
    // Load new page of posts if needed
    while (curr_posts.empty() && !total_pages.empty()) {
        int page = total_pages.front();
        total_pages.erase(total_pages.begin());
        curr_posts = pixeljoint::ScrapedPost::get_posts(page);
    }
    
    // No more posts available
    if (curr_posts.empty()) {
        return std::nullopt;
    }

    // Get next post and fetch its link
     const auto post = curr_posts.erase(curr_posts.begin());
    
    if (auto link = post->get()->fetch_link(); !link) {
        return std::unexpected(link.error());
    } else {
        return std::move(link.value());
    }
}

ImageProviders::Pages::Pages() : General(), pages_end(-1) {
}

string ImageProviders::Pages::get_name() const {
    return "pages";
}

void ImageProviders::Pages::register_properties() {
    add_property(pages_begin);
    add_property(pages_end_raw);
}

void ImageProviders::Pages::load_properties(const nlohmann::json &j) {
    General::load_properties(j);


    if (pages_end_raw->get() == -1) {
        const auto pages = pixeljoint::ScrapedPost::get_pages();
        if (!pages.has_value())
            throw std::runtime_error("Could not get page size");

        pages_end = pages.value();
    }

    total_pages = generate_rand_pages(pages_begin->get(), pages_end);
}


std::unique_ptr<ImageProviders::General, void (*)(ImageProviders::General *)>
ImageProviders::PagesWrapper::create() {
    return {
        new Pages(), [](General *p) {
            delete p;
        }
    };
}
