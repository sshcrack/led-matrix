#include "pages.h"
#include "random"
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

optional<Post> ImageProviders::Pages::get_next_image() {
    while (!curr_posts.empty() || !total_pages.empty()) {
        if (curr_posts.empty()) {
            int next_page = total_pages[0];
            total_pages.erase(total_pages.begin());

            curr_posts = ScrapedPost::get_posts(next_page);
            if (curr_posts.empty()) {
                spdlog::debug("Current posts are empty, returning");
                return nullopt;
            }
        }

        ScrapedPost curr = curr_posts[0];
        curr_posts.erase(curr_posts.begin());

        return curr.fetch_link();
    }

    return nullopt;
}

ImageProviders::Pages::Pages(const json &arguments) : General(arguments) {
    int p_begin = arguments["begin"].get<int>();
    int p_end = arguments["end"].get<int>();

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

    return json{
            {"type",      "pages"},
            {"arguments", args}
    };
}
