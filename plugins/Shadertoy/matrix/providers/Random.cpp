#include "Random.h"
#include "../scraper/Scraper.h"
#include "spdlog/spdlog.h"

ShaderProviders::Random::Random() : General() {
}

string ShaderProviders::Random::get_name() const {
    return "random";
}

void ShaderProviders::Random::register_properties() {
    add_property(min_page);
    add_property(max_page);
}

void ShaderProviders::Random::flush() {
    // Nothing to flush for random provider, scraper handles its own state
}

void ShaderProviders::Random::tick() {
    // Prefetch shaders in the background
    Scraper::instance().prefetchShadersAsync(min_page->get(), max_page->get());
}

std::expected<std::string, std::string> ShaderProviders::Random::get_next_shader() {
    auto result = Scraper::instance().scrapeNextShader(min_page->get(), max_page->get());
    if (result) {
        spdlog::info("Random provider got shader URL: {}", *result);
        return *result;
    } else {
        spdlog::warn("Random provider failed to get shader: {}", result.error());
        return std::unexpected(result.error());
    }
}

std::unique_ptr<ShaderProviders::General, void (*)(ShaderProviders::General *)>
ShaderProviders::RandomWrapper::create() {
    return {
        new Random(), [](General *p) {
            delete p;
        }
    };
}
