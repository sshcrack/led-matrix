#include "Scraper.h"
#include "shared/matrix/utils/image_fetch.h"
#include <algorithm>
#include <random>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <sstream>

const std::string SHADERTOY_BASE_URL = "https://www.shadertoy.com/";

Scraper& Scraper::instance() {
    static Scraper instance;
    return instance;
}

Scraper::Scraper() {
    const char* api_key = std::getenv("SHADERTOY_API_KEY");
    if (!api_key || std::string(api_key).empty()) {
        spdlog::warn("SHADERTOY_API_KEY is not set. Falling back to HTML scraping. Setting the API Key generates less results but improves reliability.");
    }
}

std::expected<std::string, std::string> Scraper::scrapeNextShader(int minPage, int maxPage)
{
    std::unique_lock lock(mtx);
    if (shaderIds.empty() && fetchInProgress && fetchFuture.valid())
    {
        lock.unlock();
        fetchFuture.wait();
        lock.lock();
    }
    if (!shaderIds.empty())
    {
        auto result = returnShaderFromVector();
        if (shaderIds.size() < 5 && !fetchInProgress)
        {
            fetchInProgress = true;
            fetchFuture = std::async(std::launch::async, [this, minPage, maxPage]() {
                fetchShaders(minPage, maxPage);
            });
        }
        return result;
    }
    // If still empty, fetch synchronously
    fetchShadersSync(minPage, maxPage);
    if (!shaderIds.empty())
        return returnShaderFromVector();
    if (!fetchError.empty())
        return std::unexpected(fetchError);
    return std::unexpected("No shaders available");
}

std::expected<std::string, std::string> Scraper::returnShaderFromVector()
{
    int randomIndex = std::rand() % shaderIds.size();
    std::string shaderId = shaderIds[randomIndex];
    shaderIds.erase(shaderIds.begin() + randomIndex);
    return SHADERTOY_BASE_URL + "view/" + shaderId;
}

std::expected<std::string, std::string> Scraper::peekNextShader(int minPage, int maxPage)
{
    std::unique_lock lock(mtx);
    if (shaderIds.empty() && fetchInProgress && fetchFuture.valid())
    {
        lock.unlock();
        fetchFuture.wait();
        lock.lock();
    }
    if (!shaderIds.empty())
    {
        int randomIndex = std::rand() % shaderIds.size();
        std::string shaderId = shaderIds[randomIndex];
        return SHADERTOY_BASE_URL + "view/" + shaderId;
    }
    // If still empty, fetch synchronously
    fetchShadersSync(minPage, maxPage);
    if (!shaderIds.empty())
    {
        int randomIndex = std::rand() % shaderIds.size();
        std::string shaderId = shaderIds[randomIndex];
        return SHADERTOY_BASE_URL + "view/" + shaderId;
    }
    if (!fetchError.empty())
        return std::unexpected(fetchError);
    return std::unexpected("No shaders available");
}

void Scraper::fetchShaders(int minPage, int maxPage)
{
    fetchShadersSync(minPage, maxPage);
    std::unique_lock lock(mtx);
    fetchInProgress = false;
}

void Scraper::prefetchShadersAsync(int minPage, int maxPage)
{
    std::unique_lock lock(mtx);
    if (!fetchInProgress && shaderIds.size() < 5)
    {
        fetchInProgress = true;
        fetchFuture = std::async(std::launch::async, [this, minPage, maxPage]() {
            fetchShaders(minPage, maxPage);
        });
    }
}

void Scraper::fetchShadersSync(int minPage, int maxPage)
{
    std::vector<int> allPages;
    for (int i = minPage; i <= maxPage; ++i)
    {
        allPages.push_back(i);
    }
    std::vector<int> availablePages;
    for (int page : allPages)
    {
        if (std::find(alreadyScrapedPages.begin(), alreadyScrapedPages.end(), page) == alreadyScrapedPages.end())
        {
            availablePages.push_back(page);
        }
    }
    int currPage = -1;
    if (availablePages.empty())
    {
        alreadyScrapedPages.clear();
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(0, allPages.size() - 1);
        currPage = allPages[distrib(gen)];
    }
    else
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(0, availablePages.size() - 1);
        currPage = availablePages[distrib(gen)];
        alreadyScrapedPages.push_back(currPage);
    }

    const char* api_key = std::getenv("SHADERTOY_API_KEY");
    if (api_key && std::string(api_key).length() > 0) {
        fetchShadersApi(currPage, api_key);
    } else {
        fetchShadersScrape(currPage);
    }
}

void Scraper::fetchShadersApi(int currPage, const char* api_key)
{
    std::string api_url = "https://www.shadertoy.com/api/v1/shaders/query/string?sort=love&key=" + std::string(api_key) + "&from=" + std::to_string(currPage * 12) + "&num=12";
    cpr::Session session;
    session.SetUrl(cpr::Url{api_url});
    session.SetHeader({{"User-Agent", utils::DEFAULT_USER_AGENT},
                       {"Accept", "application/json"}});
    auto response = session.Get();
    if (response.status_code != 200) {
        fetchError = "API request failed with status code: " + std::to_string(response.status_code);
        return;
    }
    std::string text = response.text;
    if (text.empty()) {
        fetchError = "Received empty response from Shadertoy API";
        return;
    }
    nlohmann::json jsonData;
    try {
        jsonData = nlohmann::json::parse(text, nullptr, false);
    } catch (std::exception &e) {
        fetchError = "Failed to parse API JSON data: " + std::string(e.what());
        return;
    }
    if (jsonData.is_discarded() || !jsonData.contains("Results") || !jsonData["Results"].is_array() || jsonData["Results"].empty()) {
        fetchError = "Invalid or empty shader data from API";
        return;
    }
    std::vector<std::string> newShaderIds;
    for (const auto &shaderId : jsonData["Results"]) {
        if (!shaderId.is_string()) continue;
        newShaderIds.push_back(shaderId.get<std::string>());
    }
    {
        std::scoped_lock lock(mtx);
        shaderIds.insert(shaderIds.end(), newShaderIds.begin(), newShaderIds.end());
        fetchError.clear();
    }
}

void Scraper::fetchShadersScrape(int currPage)
{
    std::string path = "results?query=&sort=love&from=" + std::to_string(currPage * 12) + "&num=12";
    cpr::Session session;
    session.SetUrl(cpr::Url{SHADERTOY_BASE_URL + path});
    session.SetHeader({{"User-Agent", utils::DEFAULT_USER_AGENT},
                       {"Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8"}});
    auto response = session.Get();
    if (response.status_code != 200)
    {
        fetchError = "HTTP request failed with status code: " + std::to_string(response.status_code);
        return;
    }
    std::string text = response.text;
    if (text.empty())
    {
        fetchError = "Received empty response from Shadertoy";
        return;
    }
    std::istringstream stream(text);
    std::string line;
    std::string shaderLine;
    while (std::getline(stream, line))
    {
        if (!line.contains("gShaders="))
            continue;
        shaderLine = line;
        break;
    }
    if (shaderLine.empty())
    {
        fetchError = "No shader data found in response";
        return;
    }
    auto lastPos = shaderLine.rfind("];");
    if (lastPos == std::string::npos)
    {
        fetchError = "Invalid shader data format";
        return;
    }
    auto shaderData = shaderLine.substr(0, lastPos + 1);
    shaderData = shaderData.substr(shaderData.find('=') + 1);
    nlohmann::json jsonData;
    try
    {
        jsonData = nlohmann::json::parse(shaderData, nullptr, false);
    }
    catch (std::exception &e)
    {
        fetchError = "Failed to parse JSON data: " + std::string(e.what());
        return;
    }
    if (jsonData.is_discarded() || !jsonData.is_array() || jsonData.empty())
    {
        fetchError = "Invalid or empty shader data";
        return;
    }
    std::vector<std::string> newShaderIds;
    for (const auto &shader : jsonData)
    {
        if (!shader.is_object() || !shader.contains("info") || !shader["info"].is_object())
            continue;
        if (!shader["info"].contains("id") || !shader["info"]["id"].is_string())
            continue;
        std::string id = shader["info"]["id"].get<std::string>();
        newShaderIds.push_back(id);
    }
    {
        std::scoped_lock lock(mtx);
        shaderIds.insert(shaderIds.end(), newShaderIds.begin(), newShaderIds.end());
        fetchError.clear();
    }
}