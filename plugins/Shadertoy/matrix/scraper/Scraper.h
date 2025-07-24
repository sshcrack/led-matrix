#pragma once
#include <expected>
#include <string>
#include <vector>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <spdlog/spdlog.h>

class Scraper
{
public:
    static Scraper& instance();

    // Delete copy constructor and assignment operator
    Scraper(const Scraper&) = delete;
    Scraper& operator=(const Scraper&) = delete;

    [[nodiscard]] bool hasShadersLeft() {
        std::shared_lock lock(mtx);
        return !shaderIds.empty();
    }

    std::expected<std::string, std::string> scrapeNextShader(int minPage, int maxPage);
    std::expected<std::string, std::string> peekNextShader(int minPage, int maxPage);
    void prefetchShadersAsync(int minPage, int maxPage);

private:
    Scraper() = default;
    ~Scraper() {
        if (fetchFuture.valid()) {
            spdlog::info("Scraper: Waiting for fetch future to complete in destructor...");
            fetchFuture.wait();
        }
    }

    std::expected<std::string, std::string> returnShaderFromVector();
    void fetchShaders(int minPage, int maxPage);
    void fetchShadersSync(int minPage, int maxPage);

    std::vector<std::string> shaderIds;
    std::vector<int> alreadyScrapedPages;
    std::future<void> fetchFuture;
    std::shared_mutex mtx;
    bool fetchInProgress = false;
    std::string fetchError;
};