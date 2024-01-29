#include "pixel_art.h"
#include <vector>
#include "cpr/cpr.h"
#include "libxml/HTMLparser.h"
#include "libxml/xpath.h"
#include <iostream>
#include <optional>
#include <curl/curl.h>

std::string base = "https://pixeljoint.com/";
std::string search_url = "/pixels/new_icons.asp?q=1";

void download_image(std::string url_str) {
    auto curl = curl_easy_init();
    if(curl) {
        CURLcode res;
        curl_easy_setopt(curl, CURLOPT_URL, base + url_str);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
}

htmlDocPtr fetch_page(std::string url_str)
{
    auto url = cpr::Url{base + url_str};

    cpr::Header headers = {
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/113.0.0.0 Safari/537.36"},
        {"Cookie", "v=av=&dimo=%3C%3D&anim=&iso=&ob=search&dint=&pg=2&search=&tran=&colors=2&d=&colorso=%3E%3D&dim=128&owner=0; path=/"}};

    auto response = cpr::Get(url, headers);
    return htmlReadMemory(response.text.c_str(), response.text.length(), nullptr, nullptr, HTML_PARSE_NOWARNING | HTML_PARSE_NOERROR);
}

bool Post::fetch_link()
{
    htmlDocPtr doc = fetch_page(this->url);

    xmlXPathContextPtr context = xmlXPathNewContext(doc);
    xmlXPathObjectPtr mainimg = xmlXPathEvalExpression((xmlChar *)"//*[@id=\"mainimg\"]", context);
    if (mainimg->nodesetval->nodeNr == 0)
    {
        return false;
    }

    xmlNodePtr img = mainimg->nodesetval->nodeTab[0];
    std::string src = std::string(reinterpret_cast<char *>(xmlGetProp(img, (xmlChar *)"src")));

    this->image = src;
    return true;
}

std::optional<int> get_page_size()
{
    // parse the HTML document returned by the server
    htmlDocPtr doc = fetch_page(search_url);

    std::vector<Post> pixel_posts;

    xmlXPathContextPtr context = xmlXPathNewContext(doc);
    xmlXPathObjectPtr page_option = xmlXPathEvalExpression((xmlChar *)"/html/body/div[3]/div[2]/div[1]/div/div[1]/span/select/option[last()]", context);
    std::optional<int> page_end;

    if (page_option->nodesetval->nodeNr != 0)
    {
        xmlNodePtr el = page_option->nodesetval->nodeTab[0];
        std::string value = std::string(reinterpret_cast<char *>(xmlGetProp(el, (xmlChar *)"value")));
        try
        {
            page_end = std::stoi(value);
        }
        catch (exception &err)
        {
            cout << "Conversion failure" << endl;
        }
    }

    return page_end;
}

std::vector<Post> get_posts(int page)
{
    std::cout << "Getting posts from page " << page << "...\n";
    std::string url = search_url + "&pg=" + std::to_string(page);

    // parse the HTML document returned by the server
    htmlDocPtr doc = fetch_page(url);

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