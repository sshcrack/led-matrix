#include "ShaderCache.h"
#include <fstream>
#include <spdlog/spdlog.h>

ShaderCache::ShaderCache(const std::filesystem::path& cacheDir)
    : mCacheDir(cacheDir), mCacheFile(cacheDir / "shader_cache.json")
{
    ensureCacheDirExists();
    load();
}

std::optional<std::string> ShaderCache::get(const std::string& url) const
{
    if (mCache.contains(url))
    {
        return mCache[url].get<std::string>();
    }
    return std::nullopt;
}

void ShaderCache::set(const std::string& url, const std::string& response)
{
    mCache[url] = response;
    save();
}

bool ShaderCache::has(const std::string& url) const
{
    return mCache.contains(url);
}

void ShaderCache::load()
{
    try
    {
        if (std::filesystem::exists(mCacheFile))
        {
            std::ifstream file(mCacheFile);
            if (file.is_open())
            {
                file >> mCache;
                spdlog::info("Loaded shader cache from {}", mCacheFile.string());
            }
        }
        else
        {
            mCache = nlohmann::json::object();
        }
    }
    catch (const std::exception& e)
    {
        spdlog::error("Failed to load shader cache: {}", e.what());
        mCache = nlohmann::json::object();
    }
}

void ShaderCache::save() const
{
    try
    {
        ensureCacheDirExists();
        std::ofstream file(mCacheFile);
        if (file.is_open())
        {
            file << mCache.dump(4);
            spdlog::debug("Saved shader cache to {}", mCacheFile.string());
        }
    }
    catch (const std::exception& e)
    {
        spdlog::error("Failed to save shader cache: {}", e.what());
    }
}

std::vector<std::string> ShaderCache::getKeys() const
{
    std::vector<std::string> keys;
    for (auto& [key, value] : mCache.items())
    {
        keys.push_back(key);
    }
    return keys;
}

void ShaderCache::remove(const std::string& url)
{
    if (mCache.contains(url))
    {
        mCache.erase(url);
        save();
    }
}

void ShaderCache::clear()
{
    mCache.clear();
    save();
}

void ShaderCache::ensureCacheDirExists() const
{
    try
    {
        if (!std::filesystem::exists(mCacheDir))
        {
            std::filesystem::create_directories(mCacheDir);
        }
    }
    catch (const std::exception& e)
    {
        spdlog::error("Failed to create cache directory: {}", e.what());
    }
}
