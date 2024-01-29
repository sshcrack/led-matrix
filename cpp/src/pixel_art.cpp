#include "pixel_art.h"
#include <vector>
#include "cpr/cpr.h"
#include "libxml/HTMLparser.h"
#include "libxml/xpath.h"
#include <iostream>

bool Post::fetch_link() {
    std::string base = "https://pixeljoint.com";
    auto url = cpr::Url{base + this->url};
    
    auto response = cpr::Get(url);
    htmlDocPtr doc = htmlReadMemory(response.text.c_str(), response.text.length(), nullptr, nullptr, HTML_PARSE_NOWARNING | HTML_PARSE_NOERROR);
    
    xmlXPathContextPtr context = xmlXPathNewContext(doc);
    xmlXPathObjectPtr mainimg = xmlXPathEvalExpression((xmlChar *)"//*[@id=\"mainimg\"]", context);
    if(mainimg->nodesetval->nodeNr == 0) {
        return false;
    }


    xmlNodePtr img = mainimg->nodesetval->nodeTab[0];
    std::string src = std::string(reinterpret_cast<char *>(xmlGetProp(img, (xmlChar *)"src")));

    this->image = base + src;
    return true;
}

std::vector<Post> get_posts(u_int32_t page) {
    std::cout << "Getting posts from page " << page << "...\n";

    std::string url_str = "https://pixeljoint.com/pixels/new_icons.asp?q=1&pg=" + std::to_string(page);
    auto url = cpr::Url{url_str};

    // define the user agent for the GET request
    cpr::Header headers = {
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/113.0.0.0 Safari/537.36"},
        {"Cookie", "v=av=&dimo=%3C%3D&anim=&iso=&ob=search&dint=&pg=2&search=&tran=&colors=2&d=&colorso=%3E%3D&dim=128&owner=0; path=/"}
    };
    // make an HTTP GET request to retrieve the target page
    cpr::Response response = cpr::Get(url, headers);

    // parse the HTML document returned by the server
    htmlDocPtr doc = htmlReadMemory(response.text.c_str(), response.text.length(), nullptr, nullptr, HTML_PARSE_NOWARNING | HTML_PARSE_NOERROR);

    std::vector<Post> pixel_posts;

    xmlXPathContextPtr context = xmlXPathNewContext(doc);
    xmlXPathObjectPtr posts_links = xmlXPathEvalExpression((xmlChar *)"//a[contains(@class, 'imglink')]", context);

    // iterate over the list of industry card elements
    for (int i = 0; i < posts_links->nodesetval->nodeNr; ++i)
    {
        // get the current element of the loop
        xmlNodePtr post_link = posts_links->nodesetval->nodeTab[i];
        // set the libxml2 context to the current element
        // to limit the XPath selectors to its children
        xmlXPathSetContextNode(post_link, context);

        xmlNodePtr image_html_element = xmlXPathEvalExpression((xmlChar *)".//img", context)->nodesetval->nodeTab[0];
        std::string thumbnail = std::string(reinterpret_cast<char *>(xmlGetProp(image_html_element, (xmlChar *)"src")));

        std::string url = std::string(reinterpret_cast<char *>(xmlGetProp(post_link, (xmlChar *)"href")));

        Post pixel_post = Post(thumbnail, url);
        pixel_posts.push_back(pixel_post);
    }

    return pixel_posts;
} 