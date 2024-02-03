#include "post.h"
#include "pixel_art.h"
#include <vector>
#include "../consts.h"
#include "cpr/cpr.h"
#include "libxml/HTMLparser.h"
#include "libxml/xpath.h"
#include <iostream>
#include <optional>
#include <curl/curl.h>
#include <cstring>
#include <Magick++.h>
#include "image.h"
#include <optional>
#include "../utils.h"
#include "spdlog/spdlog.h"

using namespace spdlog;
using namespace std;
using namespace Constants;
string search_url = "/pixels/new_icons.asp?q=1";

bool ScrapedPost::fetch_link() {
    htmlDocPtr doc = fetch_page(this->post_url);

    xmlXPathContextPtr context = xmlXPathNewContext(doc);
    xmlXPathObjectPtr mainimg = xmlXPathEvalExpression((xmlChar *) "//*[@id=\"mainimg\"]", context);
    if (mainimg->nodesetval->nodeNr == 0) {
        return false;
    }

    xmlNodePtr img = mainimg->nodesetval->nodeTab[0];
    string src = string(reinterpret_cast<char *>(xmlGetProp(img, (xmlChar *) "src")));

    this->image_url = src;
    this->file_name = src.substr(src.find_last_of('/') +1);
    return true;
}

optional<int> ScrapedPost::get_pages() {
    info("Getting page size...");

    // parse the HTML document returned by the server
    htmlDocPtr doc = fetch_page(search_url);

    vector<ScrapedPost> pixel_posts;

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

    debug("Obtained page size of " + to_string(page_end.value_or(-1)));
    return page_end;
}

vector<ScrapedPost> ScrapedPost::get_posts(int page) {
    info("Getting posts from page " + to_string(page) + "...");
    string url = search_url + "&pg=" + to_string(page);

    // parse the HTML document returned by the server
    htmlDocPtr doc = fetch_page(url);

    vector<ScrapedPost> pixel_posts;

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

        ScrapedPost pixel_post = ScrapedPost(thumbnail, a_href);
        pixel_posts.push_back(pixel_post);
    }

    return pixel_posts;
}

bool ScrapedPost::has_fetched_image() {
    return this->image_url != this->thumbnail;
}

string ScrapedPost::get_post_url() {
    return this->post_url;
}

string Post::get_filename() {
    return this->file_name;
}

string Post::get_image() {
    return this->image_url;
}

optional<vector<Magick::Image>> Post::fetch_images(int width, int height) {
    if (!filesystem::exists(root_dir)) {
        try {
            auto res = filesystem::create_directory(root_dir);
            if(!res) {
                error("Could not create directory at {}.", root_dir);
                exit(-1);
            }
        } catch (exception& ex) {
            error("Could not create directory at {} with exception: {}", root_dir, ex.what());
            exit(-1);
        }
    }

    tmillis_t start_loading = GetTimeInMillis();

    // Downloading image first
    if (!filesystem::exists(file_name))
        download_image(image_url, file_name);

    vector<Magick::Image> frames;
    string err_msg;

    bool contain_img = true;
    if (!LoadImageAndScale(file_name, width, height, true, true, contain_img, &frames, &err_msg)) {
        error("Error loading image: {}", err_msg);
        return nullopt;
    }


    optional<vector<Magick::Image>> res;
    res = frames;
    debug("Loading/Scaling Image took {}s.", (GetTimeInMillis() - start_loading) / 1000.0);

    return res;
}
