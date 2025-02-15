#pragma once

#include <string>
#include <optional>
#include "libxml/HTMLparser.h"

namespace utils {
    constexpr const char* DEFAULT_USER_AGENT = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";
    
    struct DownloadResult {
        bool success;
        std::string error_message;
    };

    DownloadResult download_image(const std::string& url_str, const std::string& out_file);
    std::optional<htmlDocPtr> fetch_page(const std::string& url_str, const std::string& base_url = "");
}