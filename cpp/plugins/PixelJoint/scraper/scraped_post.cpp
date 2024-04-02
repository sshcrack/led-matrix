#include "scraped_post.h"
#include "shared/utils/image_fetch.h"
#include <vector>
#include "libxml/xpath.h"
#include <iostream>
#include <optional>
#include <spdlog/spdlog.h>

using namespace spdlog;
using namespace std;
string search_url = "/pixels/new_icons.asp?q=1";

optional<Post> ScrapedPost::fetch_link() {
    if (cached_post)
        return cached_post;

    htmlDocPtr doc = fetch_page(this->post_url);

    xmlXPathContextPtr context = xmlXPathNewContext(doc);
    xmlXPathObjectPtr mainimg = xmlXPathEvalExpression((xmlChar *) "//*[@id=\"mainimg\"]", context);
    if (mainimg->nodesetval->nodeNr == 0) {
        return nullopt;
    }

    xmlNodePtr img = mainimg->nodesetval->nodeTab[0];
    string img_path(
            string(reinterpret_cast<char *>(xmlGetProp(img, (xmlChar *) "src")))
    );


    string img_url = "https://pixeljoint.com" + img_path;
    cached_post = Post(img_url);

    return cached_post;
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

string ScrapedPost::get_post_url() {
    return this->post_url;
}