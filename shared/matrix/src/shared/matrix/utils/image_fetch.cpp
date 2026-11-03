#include "shared/matrix/utils/image_fetch.h"
#include "cpr/cpr.h"
#include "spdlog/spdlog.h"
#include "curl/curl.h"
#include <memory>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <optional>
#include <arpa/inet.h>

#include "shared/matrix/utils/consts.h"

namespace fs = std::filesystem;

namespace utils {
    namespace {
        size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
            return fwrite(ptr, 1, size * nmemb, stream);
        }

        int check_cancelled(void *context, curl_off_t, curl_off_t dlnow, curl_off_t, curl_off_t) {
            if (dlnow > MAX_IMAGE_DOWNLOAD_BYTES) return 1;
            return static_cast<const std::stop_token *>(context)->stop_requested() ? 1 : 0;
        }

        int reject_internal_destination(void *, char *conn_primary_ip, char *, int, int) {
            if (conn_primary_ip && is_internal_address(conn_primary_ip)) {
                spdlog::warn("Refusing image download from internal address {}", conn_primary_ip);
                return CURL_PREREQFUNC_ABORT;
            }
            return CURL_PREREQFUNC_OK;
        }
    }

    static std::string FILE_PROTOCOL = "file://";



    std::expected<void, std::string> download_image(const std::string &url_str, const std::string &file_out,
                                                  std::stop_token stop_token, DownloadOptions options) {
        if (stop_token.stop_requested())
            return std::unexpected("Image download cancelled");
        if (file_out.length() >= FILENAME_MAX) {
            return std::unexpected("File name too long");
        }

        if (is_local_file_url(url_str)) {
            const auto local_file = resolve_local_file_url(url_str);
            if (!local_file)
                return std::unexpected("Local file is outside the upload directory");
            try {
                fs::copy_file(*local_file, file_out);
                return {};
            } catch (fs::filesystem_error &e) {
                return std::unexpected(e.what());
            }
        }

        if (!is_http_url(url_str))
            return std::unexpected("Only http(s) URLs are supported");

        spdlog::debug("Downloading {} to {}", url_str, file_out);

        std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
        if (!curl) {
            return std::unexpected("Failed to initialize CURL");
        }

        FILE *fp = fopen(file_out.c_str(), "wb");
        if (!fp) {
            return std::unexpected("Failed to open output file: " + std::string(strerror(errno)));
        }

        curl_easy_setopt(curl.get(), CURLOPT_URL, url_str.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, DEFAULT_USER_AGENT);
        curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 5L);
        curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_LIMIT, 1024L);
        curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_TIME, 10L);
        curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 5L);
        curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "http,https");
        curl_easy_setopt(curl.get(), CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
        curl_easy_setopt(curl.get(), CURLOPT_MAXFILESIZE_LARGE, static_cast<curl_off_t>(MAX_IMAGE_DOWNLOAD_BYTES));
        if (options.block_internal_hosts)
            curl_easy_setopt(curl.get(), CURLOPT_PREREQFUNCTION, reject_internal_destination);
        curl_easy_setopt(curl.get(), CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, check_cancelled);
        curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, &stop_token);

        const auto res = curl_easy_perform(curl.get());
        long status = 0;
        curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
        const bool write_failed = fclose(fp) != 0;

        if (res != CURLE_OK || write_failed || status < 200 || status >= 300) {
            std::error_code ignored;
            fs::remove(file_out, ignored);
            if (res != CURLE_OK)
                return std::unexpected(curl_easy_strerror(res));
            if (write_failed)
                return std::unexpected("Failed to finish writing downloaded image");
            return std::unexpected("HTTP request failed with status code: " + std::to_string(status));
        }

        return {};
    }

    std::optional<std::string> fetch_content_type(const std::string &url_str, DownloadOptions options) {
        if (!is_http_url(url_str)) return std::nullopt;

        std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
        if (!curl) return std::nullopt;

        curl_easy_setopt(curl.get(), CURLOPT_URL, url_str.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_NOBODY, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, DEFAULT_USER_AGENT);
        curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 5L);
        curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 15L);
        curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 5L);
        curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "http,https");
        curl_easy_setopt(curl.get(), CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
        if (options.block_internal_hosts)
            curl_easy_setopt(curl.get(), CURLOPT_PREREQFUNCTION, reject_internal_destination);

        if (curl_easy_perform(curl.get()) != CURLE_OK) return std::nullopt;
        long status = 0;
        curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
        if (status < 200 || status >= 300) return std::nullopt;

        char *content_type = nullptr;
        curl_easy_getinfo(curl.get(), CURLINFO_CONTENT_TYPE, &content_type);
        if (!content_type) return std::nullopt;
        std::string result(content_type);
        result = result.substr(0, result.find(';'));
        return result;
    }

    std::expected<htmlDocPtr, std::string> fetch_page(const std::string &url_str, const std::string &base_url) {
        cpr::Session session;
        session.SetUrl(cpr::Url{base_url + url_str});
        session.SetConnectTimeout(cpr::ConnectTimeout{std::chrono::seconds(5)});
        session.SetTimeout(cpr::Timeout{std::chrono::seconds(30)});
        session.SetLowSpeed(cpr::LowSpeed{1024, std::chrono::seconds(10)});
        session.SetHeader({
            {"User-Agent", DEFAULT_USER_AGENT},
            {"Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8"}
        });

        auto response = session.Get();

        if (response.status_code != 200) {
            spdlog::error(" {}", response.status_code);
            return std::unexpected("HTTP request failed with status code: " + std::to_string(response.status_code));
        }

        htmlDocPtr doc = htmlReadMemory(
            response.text.c_str(),
            response.text.length(),
            nullptr,
            nullptr,
            HTML_PARSE_NOWARNING | HTML_PARSE_NOERROR
        );

        if (!doc) {
            return std::unexpected("Failed to parse HTML document");
        }

        return doc;
    }

    bool is_local_file_url(const std::string &url) {
        return url.starts_with(FILE_PROTOCOL);
    }

    bool is_http_url(const std::string &url) {
        std::string lower = url.substr(0, 8);
        std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return lower.starts_with("http://") || lower.starts_with("https://");
    }

    std::optional<fs::path> resolve_local_file_url(const std::string &url) {
        if (!is_local_file_url(url)) return std::nullopt;
        const fs::path relative(url.substr(FILE_PROTOCOL.length()));
        if (relative.empty() || relative.is_absolute() || relative.has_root_name()) return std::nullopt;

        std::error_code ec;
        const auto root = fs::weakly_canonical(Constants::upload_dir, ec);
        if (ec) return std::nullopt;
        const auto candidate = fs::weakly_canonical(root / relative, ec);
        if (ec) return std::nullopt;

        const auto rel = candidate.lexically_relative(root);
        if (rel.empty() || *rel.begin() == "..") return std::nullopt;
        return candidate;
    }

    bool is_internal_address(const std::string &ip) {
        in_addr v4{};
        if (inet_pton(AF_INET, ip.c_str(), &v4) == 1) {
            const auto addr = ntohl(v4.s_addr);
            return (addr >> 24) == 127 || (addr >> 24) == 0 || (addr >> 16) == 0xA9FE;
        }

        in6_addr v6{};
        if (inet_pton(AF_INET6, ip.c_str(), &v6) == 1) {
            const auto *b = v6.s6_addr;
            static constexpr unsigned char mapped_prefix[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
            if (std::memcmp(b, mapped_prefix, sizeof(mapped_prefix)) == 0) {
                char mapped[INET_ADDRSTRLEN]{};
                inet_ntop(AF_INET, b + 12, mapped, sizeof(mapped));
                return is_internal_address(mapped);
            }
            if (IN6_IS_ADDR_LOOPBACK(&v6) || IN6_IS_ADDR_UNSPECIFIED(&v6) || IN6_IS_ADDR_LINKLOCAL(&v6))
                return true;
            return false;
        }

        // Not an IP literal: be conservative.
        return true;
    }
} // namespace utils
