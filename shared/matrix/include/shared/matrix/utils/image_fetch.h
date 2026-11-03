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

    constexpr long long MAX_IMAGE_DOWNLOAD_BYTES = 50LL * 1024 * 1024;

    struct DownloadOptions {
        // Refuse loopback, link-local and unspecified destinations (including after redirects).
        // Set for URLs supplied directly by API clients so they cannot reach services on the Pi itself.
        bool block_internal_hosts = false;
    };

    /// Downloads an image from an http(s) URL to a file. file:// URLs are resolved inside the upload directory only.
    std::expected<void, std::string> download_image(const std::string& url_str, const std::string& file_out,
                                                  std::stop_token stop_token = {}, DownloadOptions options = {});
    inline std::expected<void, std::string> download_image(const std::string& url_str, const std::filesystem::path& file_out,
                                                         std::stop_token stop_token = {}, DownloadOptions options = {}) {
        return download_image(url_str, file_out.string(), stop_token, options);
    }

    /// Issues a HEAD request and returns the Content-Type header, with the same protections as download_image.
    std::optional<std::string> fetch_content_type(const std::string &url_str, DownloadOptions options = {});

    /// Resolves a file:// URL to a path inside the upload directory, or nullopt if it escapes it.
    std::optional<std::filesystem::path> resolve_local_file_url(const std::string &url);

    /// True for loopback, link-local and unspecified IPv4/IPv6 literals.
    bool is_internal_address(const std::string &ip);

    bool is_http_url(const std::string &url);
    std::expected<htmlDocPtr, std::string> fetch_page(const std::string& url_str, const std::string& base_url = "");

    bool is_local_file_url(const std::string &url);
}
