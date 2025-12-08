#include "Collection.h"
#include <random>
#include "spdlog/spdlog.h"

using std::nullopt;

ShaderProviders::Collection::Collection() : General() {
}

void ShaderProviders::Collection::register_properties() {
    add_property(urls);
}

void ShaderProviders::Collection::load_properties(const nlohmann::json &j) {
    General::load_properties(j);
    available_urls.reserve(urls->get().size());

    spdlog::trace("Adding a total of {} shader URLs to collection", urls->get().size());
    for (const auto &url: urls->get()) {
        // Validate that URL starts with www.shadertoy.com or https://www.shadertoy.com or http://www.shadertoy.com
        bool valid = false;
        if (url.starts_with("https://www.shadertoy.com/") || 
            url.starts_with("http://www.shadertoy.com/") ||
            url.starts_with("www.shadertoy.com/")) {
            valid = true;
        }
        
        if (!valid) {
            throw std::runtime_error(fmt::format("Invalid Shadertoy URL '{}'. URLs must start with 'www.shadertoy.com', 'http://www.shadertoy.com', or 'https://www.shadertoy.com'", url));
        }
        
        // Ensure URL has https:// prefix
        std::string normalized_url = url;
        if (!normalized_url.starts_with("http://") && !normalized_url.starts_with("https://")) {
            normalized_url = "https://" + normalized_url;
        }
        
        available_urls.push_back(normalized_url);
    }
}

std::expected<std::string, std::string> ShaderProviders::Collection::get_next_shader() {
    if (available_urls.empty()) {
        return std::unexpected("No shader URLs available in collection");
    }

    auto curr_url = available_urls[0];
    available_urls.erase(available_urls.begin());

    already_shown.push_back(curr_url);

    spdlog::info("Collection provider returning shader URL: {}", curr_url);
    return curr_url;
}

void ShaderProviders::Collection::flush() {
    available_urls.reserve(available_urls.size() + already_shown.size());
    available_urls.insert(available_urls.end(), already_shown.begin(), already_shown.end());

    already_shown.clear();
    std::ranges::shuffle(available_urls, std::random_device());
}

string ShaderProviders::Collection::get_name() const {
    return "shader_collection";
}

std::unique_ptr<ShaderProviders::General, void (*)(ShaderProviders::General *)>
ShaderProviders::CollectionWrapper::create() {
    return {
        new Collection(), [](General *p) {
            delete p;
        }
    };
}
