#include "ShaderCache.h"
#include <fstream>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cctype>

ShaderCache::ShaderCache(const std::filesystem::path& cacheDir)
    : mCacheDir(cacheDir)
{
    ensureCacheDirExists();
}

std::string ShaderCache::urlToFilename(const std::string& url) const
{
    // Simple hash-based approach: convert URL to a safe filename
    // Replace unsafe characters with underscores
    std::string filename;
    for (char c : url)
    {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.')
        {
            filename += c;
        }
        else
        {
            filename += '_';
        }
    }
    
    // Limit filename length to avoid filesystem issues
    if (filename.length() > 200)
    {
        filename = filename.substr(0, 200);
    }
    
    return filename + ".json";
}

std::filesystem::path ShaderCache::getCacheFilePath(const std::string& url) const
{
    return mCacheDir / urlToFilename(url);
}

std::optional<std::string> ShaderCache::get(const std::string& url) const
{
    auto filePath = getCacheFilePath(url);
    
    try
    {
        if (std::filesystem::exists(filePath))
        {
            std::ifstream file(filePath);
            if (file.is_open())
            {
                std::string content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
                return content;
            }
        }
    }
    catch (const std::exception& e)
    {
        spdlog::error("Failed to read cache file for {}: {}", url, e.what());
    }
    
    return std::nullopt;
}

void ShaderCache::setFromFile(const std::string& url, const std::filesystem::path& filePath)
{
    try
    {
        ensureCacheDirExists();
        
        if (!std::filesystem::exists(filePath))
        {
            spdlog::error("Source file does not exist: {}", filePath.string());
            return;
        }
        
        auto cacheFilePath = getCacheFilePath(url);
        
        // Read from source file
        std::ifstream source(filePath, std::ios::binary);
        if (!source.is_open())
        {
            spdlog::error("Failed to open source file: {}", filePath.string());
            return;
        }
        
        // Write to cache file
        std::ofstream dest(cacheFilePath, std::ios::binary);
        if (!dest.is_open())
        {
            spdlog::error("Failed to create cache file: {}", cacheFilePath.string());
            return;
        }
        
        dest << source.rdbuf();
        spdlog::info("Cached shader response for {} to {}", url, cacheFilePath.string());
    }
    catch (const std::exception& e)
    {
        spdlog::error("Failed to set cache from file: {}", e.what());
    }
}

bool ShaderCache::has(const std::string& url) const
{
    try
    {
        return std::filesystem::exists(getCacheFilePath(url));
    }
    catch (const std::exception& e)
    {
        spdlog::error("Error checking cache: {}", e.what());
        return false;
    }
}

std::vector<std::string> ShaderCache::getKeys() const
{
    std::vector<std::string> keys;
    
    try
    {
        if (!std::filesystem::exists(mCacheDir))
        {
            return keys;
        }
        
        // Iterate through all .json files in cache directory
        for (const auto& entry : std::filesystem::directory_iterator(mCacheDir))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".json")
            {
                // For now, just return the filename without extension
                // In a real implementation, you might want to store metadata
                keys.push_back(entry.path().stem().string());
            }
        }
    }
    catch (const std::exception& e)
    {
        spdlog::error("Failed to enumerate cache files: {}", e.what());
    }
    
    return keys;
}

void ShaderCache::remove(const std::string& url)
{
    try
    {
        auto filePath = getCacheFilePath(url);
        if (std::filesystem::exists(filePath))
        {
            std::filesystem::remove(filePath);
            spdlog::info("Removed cache file: {}", filePath.string());
        }
    }
    catch (const std::exception& e)
    {
        spdlog::error("Failed to remove cache entry: {}", e.what());
    }
}

void ShaderCache::clear()
{
    try
    {
        if (std::filesystem::exists(mCacheDir))
        {
            for (const auto& entry : std::filesystem::directory_iterator(mCacheDir))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".json")
                {
                    std::filesystem::remove(entry.path());
                }
            }
            spdlog::info("Cleared all cache entries");
        }
    }
    catch (const std::exception& e)
    {
        spdlog::error("Failed to clear cache: {}", e.what());
    }
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
