#include "scraped_post.h"
#include "shared/utils/image_fetch.h"
#include <libxml/xpath.h>
#include <spdlog/spdlog.h>

namespace pixeljoint {
    namespace {
        struct XmlXPathObjGuard {
            xmlXPathObjectPtr obj;

            explicit XmlXPathObjGuard(xmlXPathObjectPtr o) : obj(o) {
            }

            ~XmlXPathObjGuard() { xmlXPathFreeObject(obj); }
        };

        // Add cleanup helper
        void cleanup_xml_node(xmlNodePtr node) {
            if (node) {
                if (node->children) xmlFreeNodeList(node->children);
                xmlFree(node);
            }
        }
    }

    ScrapedPost::ScrapedPost(std::string_view thumbnail, std::string_view url)
        : thumbnail(thumbnail)
          , post_url(url) {
    }

    std::optional<std::unique_ptr<Post> > ScrapedPost::fetch_link() const {
        auto doc_opt = utils::fetch_page(post_url, BASE_URL);
        if (!doc_opt) {
            spdlog::error("Failed to fetch page: {}", post_url);
            return std::nullopt;
        }

        auto doc = doc_opt.value();
        auto context = xmlXPathNewContext(doc);

        auto mainimg = xmlXPathEvalExpression(BAD_CAST "//*[@id=\"mainimg\"]", context);

        if (!mainimg->nodesetval || mainimg->nodesetval->nodeNr == 0) {
            xmlXPathFreeObject(mainimg);
            xmlXPathFreeContext(context);
            xmlFreeDoc(doc);

            spdlog::error("No main image found on page");
            return std::nullopt;
        }

        xmlNodePtr img = mainimg->nodesetval->nodeTab[0];
        auto src_prop = xmlGetProp(img, BAD_CAST "src");
        if (!src_prop) {
            xmlXPathFreeObject(mainimg);
            xmlXPathFreeContext(context);
            xmlFreeDoc(doc);

            spdlog::error("No src attribute found on main image");
            return std::nullopt;
        }

        std::string img_url = BASE_URL + std::string(reinterpret_cast<char *>(src_prop));
        xmlFree(src_prop);

        xmlXPathFreeObject(mainimg);
        xmlXPathFreeContext(context);
        xmlFreeDoc(doc);

        return std::make_unique<Post>(img_url);
    }

    std::optional<int> ScrapedPost::get_pages() {
        auto doc_opt = utils::fetch_page(SEARCH_URL, BASE_URL);
        if (!doc_opt) {
            spdlog::error("Failed to fetch search page");
            return std::nullopt;
        }

        auto doc = doc_opt.value();
        auto context = xmlXPathNewContext(doc);

        auto page_option = xmlXPathEvalExpression(
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

        std::string value(reinterpret_cast<char *>(value_prop));
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

        std::string url = SEARCH_URL + std::string("&pg=") + std::to_string(page);
        auto doc_opt = utils::fetch_page(url, BASE_URL);
        if (!doc_opt) {
            spdlog::error("Failed to fetch page {}", page);
            return posts;
        }

        auto doc = doc_opt.value();
        auto context = xmlXPathNewContext(doc);

        auto posts_links = xmlXPathEvalExpression(
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
            xmlNodePtr post_link = posts_links->nodesetval->nodeTab[i];
            xmlXPathSetContextNode(post_link, context);

            auto image_result = xmlXPathEvalExpression(BAD_CAST ".//img", context);
            if (!image_result->nodesetval || image_result->nodesetval->nodeNr == 0) {
                xmlXPathFreeObject(image_result);
                continue;
            }

            xmlNodePtr image_element = image_result->nodesetval->nodeTab[0];
            auto src = xmlGetProp(image_element, BAD_CAST "src");
            auto href = xmlGetProp(post_link, BAD_CAST "href");

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
