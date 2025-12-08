#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <filesystem>
#include <optional>

class ShaderCache {
public:
    ShaderCache(const std::filesystem::path& cacheDir);
    
    // Get cached response for a URL
    std::optional<std::string> get(const std::string& url) const;
    
    // Set cache entry
    void set(const std::string& url, const std::string& response);
    
    // Check if URL is cached
    bool has(const std::string& url) const;
    
    // Load cache from disk
    void load();
    
    // Save cache to disk
    void save() const;
    
    // Get all keys
    std::vector<std::string> getKeys() const;
    
    // Remove a cache entry
    void remove(const std::string& url);
    
    // Clear all cache
    void clear();

private:
    std::filesystem::path mCacheDir;
    std::filesystem::path mCacheFile;
    nlohmann::json mCache;
    
    void ensureCacheDirExists() const;
};
