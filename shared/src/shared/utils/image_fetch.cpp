#include "shared/utils/image_fetch.h"
#include <cpr/cpr.h>
#include <spdlog/spdlog.h>
#include <curl/curl.h>
#include <memory>

namespace fs = std::filesystem;

namespace utils {
    namespace {
        size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
            return fwrite(ptr, size, nmemb, stream);
        }
    }

    static std::string FILE_PROTOCOL = "file://";

    std::expected<void, std::string> download_image(const std::string &url_str, const std::string &file_out) {
        if (file_out.length() >= FILENAME_MAX) {
            return std::unexpected("File name too long");
        }

        if (url_str.starts_with(FILE_PROTOCOL)) {
            const auto local_file = url_str.substr(FILE_PROTOCOL.length());
            try {
                fs::copy_file(local_file, file_out);
                return {};
            } catch (fs::filesystem_error &e) {
                return std::unexpected(e.what());
            }
        }

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

        const auto res = curl_easy_perform(curl.get());
        fclose(fp);

        if (res != CURLE_OK) {
            return std::unexpected(curl_easy_strerror(res));
        }

        return {};
    }

    std::optional<htmlDocPtr> fetch_page(const std::string &url_str, const std::string &base_url) {
        cpr::Session session;
        session.SetUrl(cpr::Url{base_url + url_str});
        session.SetHeader({
            {"User-Agent", DEFAULT_USER_AGENT},
            {"Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8"}
        });

        auto response = session.Get();

        if (response.status_code != 200) {
            spdlog::error("HTTP request failed with status code: {}", response.status_code);
            return std::nullopt;
        }

        htmlDocPtr doc = htmlReadMemory(
            response.text.c_str(),
            response.text.length(),
            nullptr,
            nullptr,
            HTML_PARSE_NOWARNING | HTML_PARSE_NOERROR
        );

        if (!doc) {
            spdlog::error("Failed to parse HTML document");
            return std::nullopt;
        }

        return doc;
    }
} // namespace utils
