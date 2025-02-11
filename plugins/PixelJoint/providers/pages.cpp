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

    std::shuffle(total_p.begin(), total_p.end(), random_device());

    return total_p;
}


void ImageProviders::Pages::flush() {
    curr_posts.clear();
    total_pages = generate_rand_pages(page_begin, page_end);
}

optional<std::variant<std::unique_ptr<Post, void (*)(Post *)>, std::shared_ptr<Post>>>
ImageProviders::Pages::get_next_image() {
    while (!curr_posts.empty() || !total_pages.empty()) {
        if (curr_posts.empty()) {
            int next_page = total_pages[0];
            total_pages.erase(total_pages.begin());

            curr_posts = std::move(ScrapedPost::get_posts(next_page));
            if (curr_posts.empty()) {
                spdlog::debug("Current posts are empty, returning");
                return nullopt;
            }
        }

        auto curr = curr_posts.erase(curr_posts.begin()).base();

        auto link = curr->get()->fetch_link();
        if (!link.has_value())
            return nullopt;

        return std::move(link.value());
    }

    return nullopt;
}

ImageProviders::Pages::Pages(const json &arguments) : General(arguments) {
    int p_begin = arguments.value("begin", 0);
    int p_end = arguments.value("end", -1);

    if (p_end == -1) {
        auto pages = ScrapedPost::get_pages();
        if (!pages.has_value())
            throw runtime_error("Could not get page size");

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
    json j = {{"begin", 1},
              {"end",   -1}};

    return {new ImageProviders::Pages(j), [](ImageProviders::General *p) {
        delete p;
    }};
}

std::unique_ptr<ImageProviders::General, void (*)(ImageProviders::General *)>
ImageProviders::PagesWrapper::from_json(const json &json) {
    return {new ImageProviders::Pages(json), [](ImageProviders::General *p) {
        delete p;
    }};
}
