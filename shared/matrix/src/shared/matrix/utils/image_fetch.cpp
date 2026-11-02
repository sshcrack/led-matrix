#include "shared/matrix/utils/image_fetch.h"
#include "cpr/cpr.h"
#include "spdlog/spdlog.h"
#include "curl/curl.h"
#include <memory>
#include <chrono>
#include <cerrno>
#include <cstring>

#include "shared/matrix/utils/consts.h"

namespace fs = std::filesystem;

namespace utils {
    namespace {
        size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
            return fwrite(ptr, 1, size * nmemb, stream);
        }

        int check_cancelled(void *context, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
            return static_cast<const std::stop_token *>(context)->stop_requested() ? 1 : 0;
        }
    }

    static std::string FILE_PROTOCOL = "file://";



    std::expected<void, std::string> download_image(const std::string &url_str, const std::string &file_out,
                                                  std::stop_token stop_token) {
        if (stop_token.stop_requested())
            return std::unexpected("Image download cancelled");
        if (file_out.length() >= FILENAME_MAX) {
            return std::unexpected("File name too long");
        }

        if (is_local_file_url(url_str)) {
            const auto local_file = Constants::upload_dir / url_str.substr(FILE_PROTOCOL.length());
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
        curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 5L);
        curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_LIMIT, 1024L);
        curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_TIME, 10L);
        curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 5L);
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
} // namespace utils
