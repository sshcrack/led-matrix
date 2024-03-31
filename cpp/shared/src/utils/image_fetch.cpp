#include "utils/image_fetch.h"
#include "cpr/cpr.h"
#include "libxml/HTMLparser.h"
#include <iostream>
#include <curl/curl.h>
#include <cstring>
#include <Magick++.h>
#include "spdlog/spdlog.h"

using namespace std;
using namespace spdlog;


size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

void rm_nonprinting(std::string &str) {
    str.erase(std::remove_if(str.begin(), str.end(),
                             [](unsigned char c) {
                                 return !std::isprint(c);
                             }),
              str.end());
}

bool download_image(const string &url_str, const string &tmp) {
    const char *out_file = tmp.c_str();

    debug("Downloading " + url_str + " to " + out_file);

    if (strlen(out_file) > FILENAME_MAX) {
        error("File name too long");
        return false;
    }


    FILE *fp;
    auto curl = curl_easy_init();
    if (curl) {
        errno = 0;
        fp = fopen(out_file, "wb");
        if (fp == nullptr) {
            error("Could not open file " + to_string(errno));
            return false;
        }


        CURLcode res;
        curl_easy_setopt(curl, CURLOPT_URL, url_str.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        fclose(fp);
        if (res == CURLcode::CURLE_OK) {
            return true;
        } else {
            error("curl error " + to_string(res));
        }
    }

    return false;
}

string page_base = "https://pixeljoint.com";
htmlDocPtr fetch_page(const string &url_str) {
    auto url = cpr::Url{page_base + url_str};

    cpr::Header headers = {
            {"User-Agent",
             "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/113.0.0.0 Safari/537.36"},
            {"Cookie",     "v=ob=rating; path=/"}
    };
    auto response = cpr::Get(url, headers);
    return htmlReadMemory(response.text.c_str(), response.text.length(), nullptr, nullptr,
                          HTML_PARSE_NOWARNING | HTML_PARSE_NOERROR);
}
