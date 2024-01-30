#include "pixel_art.h"
#include <vector>
#include "cpr/cpr.h"
#include "libxml/HTMLparser.h"
#include "libxml/xpath.h"
#include <iostream>
#include <optional>
#include <curl/curl.h>
#include <cstddef>
#include <cstring>
#include <Magick++.h>
#include <magick/image.h>

using namespace std;

string base = "https://pixeljoint.com";
string search_url = "/pixels/new_icons.asp?q=1";

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

void rm_nonprinting (std::string& str)
{
    str.erase (std::remove_if (str.begin(), str.end(),
                               [](unsigned char c){
                                   return !std::isprint(c);
                               }),
               str.end());
}

bool download_image(const string &url_str, const string& tmp) {
    const char *out_file = tmp.c_str();

    string merged_url = base + url_str;
    cout << "Downloading " << merged_url << " to " << out_file << endl;

    if (strlen(out_file) > FILENAME_MAX) {
        cout << "File name too long" << endl;
        return false;
    }


    FILE *fp;
    auto curl = curl_easy_init();
    if (curl) {
        errno = 0;
        fp = fopen(out_file, "wb");
        if(fp == nullptr) {
            cout << "Could not open file" << errno << endl;
            return false;
        }


        CURLcode res;
        curl_easy_setopt(curl, CURLOPT_URL, merged_url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        fclose(fp);
        if (res == CURLcode::CURLE_OK) {
            cout << "done" << endl;
            return true;
        } else {
            cout << "curl error " << res << endl;
        }
    }

    return false;
}

htmlDocPtr fetch_page(const string &url_str) {
    cout << "url" << endl;
    auto url = cpr::Url{base + url_str};

    cpr::Header headers = {
            {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/113.0.0.0 Safari/537.36"},
            {"Cookie",     "v=av=&dimo=%3C%3D&anim=&iso=&ob=search&dint=&pg=2&search=&tran=&colors=2&d=&colorso=%3E%3D&dim=128&owner=0; path=/"}};

    cout << "send" << endl;
    auto response = cpr::Get(url, headers);
    cout << "return" << endl;
    return htmlReadMemory(response.text.c_str(), response.text.length(), nullptr, nullptr,
                          HTML_PARSE_NOWARNING | HTML_PARSE_NOERROR);
}

bool Post::fetch_link() {
    htmlDocPtr doc = fetch_page(this->url);

    xmlXPathContextPtr context = xmlXPathNewContext(doc);
    xmlXPathObjectPtr mainimg = xmlXPathEvalExpression((xmlChar *) "//*[@id=\"mainimg\"]", context);
    if (mainimg->nodesetval->nodeNr == 0) {
        return false;
    }

    xmlNodePtr img = mainimg->nodesetval->nodeTab[0];
    string src = string(reinterpret_cast<char *>(xmlGetProp(img, (xmlChar *) "src")));

    this->image = src;
    return true;
}

optional<int> get_page_size() {
    // parse the HTML document returned by the server
    htmlDocPtr doc = fetch_page(search_url);

    vector<Post> pixel_posts;

    xmlXPathContextPtr context = xmlXPathNewContext(doc);
    xmlXPathObjectPtr page_option = xmlXPathEvalExpression(
            (xmlChar *) "//div[1]/span/select/option[last()]", context);
    optional<int> page_end = std::nullopt;

    int size = page_option->nodesetval->nodeNr;
    if (size != 0) {
        int last_index = size - 1;

        xmlNodePtr el = page_option->nodesetval->nodeTab[last_index];
        xmlXPathSetContextNode(el, context);
        string value = string(reinterpret_cast<char *>(xmlGetProp(el, (xmlChar *) "value")));
        try {
            page_end = stoi(value);
        }
        catch (exception &err) {
            cout << "Conversion failure" << endl;
        }
    }
    return page_end;
}

vector<Post> get_posts(int page) {
    cout << "Getting posts from page " << page << "...\n" << endl;
    string url = search_url + "&pg=" + to_string(page);

    // parse the HTML document returned by the server
    htmlDocPtr doc = fetch_page(url);

    vector<Post> pixel_posts;

    xmlXPathContextPtr context = xmlXPathNewContext(doc);
    xmlXPathObjectPtr posts_links = xmlXPathEvalExpression((xmlChar *) "//a[contains(@class, 'imglink')]", context);

    // iterate over the list of industry card elements
    for (int i = 0; i < posts_links->nodesetval->nodeNr; ++i) {
        // get the current element of the loop
        xmlNodePtr post_link = posts_links->nodesetval->nodeTab[i];
        // set the libxml2 context to the current element
        // to limit the XPath selectors to its children
        xmlXPathSetContextNode(post_link, context);

        xmlNodePtr image_html_element = xmlXPathEvalExpression((xmlChar *) ".//img", context)->nodesetval->nodeTab[0];
        string thumbnail = string(reinterpret_cast<char *>(xmlGetProp(image_html_element, (xmlChar *) "src")));

        string a_href = string(reinterpret_cast<char *>(xmlGetProp(post_link, (xmlChar *) "href")));

        Post pixel_post = Post(thumbnail, a_href);
        pixel_posts.push_back(pixel_post);
    }

    return pixel_posts;
}