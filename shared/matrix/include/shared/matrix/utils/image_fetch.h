#pragma once

#include <string>
#include <optional>
#include <filesystem>
#include "libxml/HTMLparser.h"
#include <expected>
#include <stop_token>

namespace utils {
    constexpr const char* DEFAULT_USER_AGENT = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";

    struct DownloadResult {
        bool success;
        std::string error_message;
    };

    /// Downloads an image from a URL to a file (also supports file:/// protocol)
    std::expected<void, std::string> download_image(const std::string& url_str, const std::string& file_out,
                                                  std::stop_token stop_token = {});
    inline std::expected<void, std::string> download_image(const std::string& url_str, const std::filesystem::path& file_out,
                                                         std::stop_token stop_token = {}) {
        return download_image(url_str, file_out.string(), stop_token);
    }
    std::expected<htmlDocPtr, std::string> fetch_page(const std::string& url_str, const std::string& base_url = "");

    bool is_local_file_url(const std::string &url);
}
