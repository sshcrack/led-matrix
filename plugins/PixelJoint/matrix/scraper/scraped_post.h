#pragma once

#include <expected>

#include "shared/matrix/post.h"
#include <string>
#include <optional>
#include <vector>
#include <memory>

namespace pixeljoint {

class ScrapedPost {
private:
    std::string thumbnail;
    std::string post_url;
    static constexpr const char* BASE_URL = "https://pixeljoint.com";
    static constexpr const char* SEARCH_URL = "/pixels/new_icons.asp?q=1";

public:
    ScrapedPost(std::string_view thumbnail, std::string_view url);

    std::expected<std::unique_ptr<Post>, std::string> fetch_link() const;
    const std::string& get_post_url() const { return post_url; }
    const std::string& get_thumbnail() const { return thumbnail; }

    static std::vector<std::unique_ptr<ScrapedPost, void(*)(ScrapedPost *)>> get_posts(int page);
    static std::optional<int> get_pages();
};

} // namespace pixeljoint