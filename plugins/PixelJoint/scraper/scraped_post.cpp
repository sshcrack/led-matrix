#include "scraped_post.h"
#include "shared/utils/image_fetch.h"
#include <libxml/xpath.h>
#include <spdlog/spdlog.h>

namespace pixeljoint {
    ScrapedPost::ScrapedPost(const std::string_view thumbnail, const std::string_view url)
        : thumbnail(thumbnail)
          , post_url(url) {
    }

    std::expected<std::unique_ptr<Post>, std::string> ScrapedPost::fetch_link() const {
        const auto doc_opt = utils::fetch_page(post_url, BASE_URL);
        if (!doc_opt)
            return std::unexpected("Could not fetch page " + post_url);


        const auto doc = doc_opt.value();
        const auto context = xmlXPathNewContext(doc);

        const auto mainimg = xmlXPathEvalExpression(BAD_CAST "//*[@id=\"mainimg\"]", context);

        if (!mainimg->nodesetval || mainimg->nodesetval->nodeNr == 0) {
            xmlXPathFreeObject(mainimg);
            xmlXPathFreeContext(context);
            xmlFreeDoc(doc);

            return std::unexpected("No main image found on page");
        }

        const auto img = mainimg->nodesetval->nodeTab[0];
        const auto src_prop = xmlGetProp(img, BAD_CAST "src");
        if (!src_prop) {
            xmlXPathFreeObject(mainimg);
            xmlXPathFreeContext(context);
            xmlFreeDoc(doc);

            return std::unexpected("No src attributes found on the main image");
        }

        std::string img_url = BASE_URL + std::string(reinterpret_cast<char *>(src_prop));
        xmlFree(src_prop);

        xmlXPathFreeObject(mainimg);
        xmlXPathFreeContext(context);
        xmlFreeDoc(doc);

        return std::make_unique<Post>(img_url);
    }

    std::optional<int> ScrapedPost::get_pages() {
        const auto doc_opt = utils::fetch_page(SEARCH_URL, BASE_URL);
        if (!doc_opt) {
            spdlog::error("Failed to fetch search page");
            return std::nullopt;
        }

        const auto doc = doc_opt.value();
        const auto context = xmlXPathNewContext(doc);

        const auto page_option = xmlXPathEvalExpression(
            BAD_CAST "//div[1]/span/select/option[last()]",
            context
        );

        if (!page_option->nodesetval || page_option->nodesetval->nodeNr == 0) {
            spdlog::error("No pagination found");

            xmlXPathFreeObject(page_option);
            xmlXPathFreeContext(context);
            xmlFreeDoc(doc);
            return std::nullopt;
        }

        xmlNodePtr el = page_option->nodesetval->nodeTab[0];
        auto value_prop = xmlGetProp(el, BAD_CAST "value");
        if (!value_prop) {
            spdlog::error("No value attribute found on page option");

            xmlXPathFreeObject(page_option);
            xmlXPathFreeContext(context);
            xmlFreeDoc(doc);
            return std::nullopt;
        }

        const std::string value(reinterpret_cast<char *>(value_prop));
        xmlFree(value_prop);

        xmlXPathFreeObject(page_option);
        xmlXPathFreeContext(context);
        xmlFreeDoc(doc);

        try {
            return std::stoi(value);
        } catch (const std::exception &e) {
            spdlog::error("Failed to parse page number: {}", e.what());
            return std::nullopt;
        }
    }

    std::vector<std::unique_ptr<ScrapedPost, void(*)(ScrapedPost *)> > ScrapedPost::get_posts(int page) {
        std::vector<std::unique_ptr<ScrapedPost, void(*)(ScrapedPost *)> > posts;

        const std::string url = SEARCH_URL + std::string("&pg=") + std::to_string(page);
        const auto doc_opt = utils::fetch_page(url, BASE_URL);
        if (!doc_opt) {
            spdlog::error("Failed to fetch page {}", page);
            return posts;
        }

        const auto doc = doc_opt.value();
        const auto context = xmlXPathNewContext(doc);

        const auto posts_links = xmlXPathEvalExpression(
            BAD_CAST "//a[contains(@class, 'imglink')]",
            context
        );

        if (!posts_links->nodesetval) {
            xmlXPathFreeObject(posts_links);
            xmlXPathFreeContext(context);
            xmlFreeDoc(doc);

            return posts;
        };

        for (int i = 0; i < posts_links->nodesetval->nodeNr; ++i) {
            const auto post_link = posts_links->nodesetval->nodeTab[i];
            xmlXPathSetContextNode(post_link, context);

            const auto image_result = xmlXPathEvalExpression(BAD_CAST ".//img", context);
            if (!image_result->nodesetval || image_result->nodesetval->nodeNr == 0) {
                xmlXPathFreeObject(image_result);
                continue;
            }

            const auto image_element = image_result->nodesetval->nodeTab[0];
            const auto src = xmlGetProp(image_element, BAD_CAST "src");
            const auto href = xmlGetProp(post_link, BAD_CAST "href");

            if (src && href) {
                std::string src_str(reinterpret_cast<char *>(src));
                std::string href_str(reinterpret_cast<char *>(href));
                posts.push_back({
                    new ScrapedPost(src_str, href_str),
                    [](ScrapedPost *post) { delete post; }
                });
            }

            xmlXPathFreeObject(image_result);
            if (src) xmlFree(src);
            if (href) xmlFree(href);
        }

        xmlXPathFreeObject(posts_links);
        xmlXPathFreeContext(context);
        xmlFreeDoc(doc);
        return posts;
    }
} // namespace pixeljoint
