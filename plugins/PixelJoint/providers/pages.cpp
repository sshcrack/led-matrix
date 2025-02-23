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

    std::shuffle(total_p.begin(), total_p.end(), std::random_device());

    return total_p;
}


void ImageProviders::Pages::flush() {
    curr_posts.clear();
    total_pages = generate_rand_pages(page_begin, page_end);
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

ImageProviders::Pages::Pages(const json &arguments) : General(arguments) {
    int p_begin = arguments.value("begin", 0);
    int p_end = arguments.value("end", -1);

    if (p_end == -1) {
        auto pages = pixeljoint::ScrapedPost::get_pages();
        if (!pages.has_value())
            throw std::runtime_error("Could not get page size");

        p_end = pages.value();
    }


    page_begin = p_begin;
    page_end = p_end;

    total_pages = generate_rand_pages(page_begin, page_end);
}

json ImageProviders::Pages::to_json() {
    json args = json();
    args["begin"] = page_begin;
    args["end"] = page_end;

    return args;
}

string ImageProviders::Pages::get_name() const {
    return "pages";
}


std::unique_ptr<ImageProviders::General, void (*)(ImageProviders::General *)>
ImageProviders::PagesWrapper::create_default() {
    json j = {
        {"begin", 1},
        {"end", -1}
    };

    return {
        new Pages(j), [](General *p) {
            delete p;
        }
    };
}

std::unique_ptr<ImageProviders::General, void (*)(ImageProviders::General *)>
ImageProviders::PagesWrapper::from_json(const json &json) {
    return {
        new Pages(json), [](General *p) {
            delete p;
        }
    };
}
