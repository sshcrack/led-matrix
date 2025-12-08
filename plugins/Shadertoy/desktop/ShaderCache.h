#pragma once
#include <string>
#include <filesystem>
#include <optional>
#include <vector>

class ShaderCache {
public:
    ShaderCache(const std::filesystem::path& cacheDir);
    
    // Get cached response for a URL
    std::optional<std::string> get(const std::string& url) const;
    
    // Set cache entry by loading from file
    void setFromFile(const std::string& url, const std::filesystem::path& filePath);
    
    // Check if URL is cached
    bool has(const std::string& url) const;
    
    // Get all keys (URLs) currently cached
    std::vector<std::string> getKeys() const;
    
    // Remove a cache entry
    void remove(const std::string& url);
    
    // Clear all cache
    void clear();

private:
    std::filesystem::path mCacheDir;
    
    // Convert URL to safe filename
    std::string urlToFilename(const std::string& url) const;
    
    // Get the cache file path for a URL
    std::filesystem::path getCacheFilePath(const std::string& url) const;
    
    void ensureCacheDirExists() const;
};
