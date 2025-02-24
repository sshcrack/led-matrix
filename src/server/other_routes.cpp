#include "other_routes.h"
#include "shared/utils/shared.h"
#include "shared/server/server_utils.h"
#include "nlohmann/json.hpp"
#include "shared/plugin_loader/loader.h"
#include "shared/post.h"
#include <filesystem>
#include "shared/server/MimeTypes.h"
#include "shared/utils/consts.h"
#include "utils/canvas_consts.h"
#include "shared/utils/canvas_image.h"


using json = nlohmann::json;

std::unique_ptr<Server::router_t> Server::add_other_routes(std::unique_ptr<router_t> router) {
    router->http_get("/list", [](auto req, auto) {
        auto res = req->create_response(restinio::status_ok())
                .append_header_date_field()
                .append_header(restinio::http_field::content_type, "application/json; charset=utf8");
        res.append_body("[");

        auto iterator = filesystem::directory_iterator(Constants::post_dir);
        bool is_first = true;
        for (const auto &entry: iterator) {
            string file_name = entry.path().filename().string();
            if (!file_name.ends_with(".p.gif"))
                continue;

            replace(file_name, ".p.gif", ".gif");
            string to_send = fmt::format("\"{}\"", file_name);
            if (!is_first) {
                to_send.insert(0, ",");
            } else {
                is_first = false;
            }

            res.append_body(to_send);
        }

        res.append_body("]").done();
        return restinio::request_accepted();
    });

    router->http_get("/image", [](auto req, auto qp) {
        if (!qp.has("url")) {
            return reply_with_error(req, "No url given");
        }

        const string remote_url{qp["url"]};

        const std::unique_ptr<Post, void(*)(Post *)> post = {new Post(remote_url), [](Post *p) { delete p; }};
        const filesystem::path file_path(Constants::post_dir / post->get_filename());
        const filesystem::path processing_path = to_processed_path(file_path);
        if (!exists(processing_path)) {
            const auto res = post->process_images(Constants::width, Constants::height);

            if (!res.has_value() || !exists(processing_path)) {
                return reply_with_error(req, "Could not get file", restinio::status_internal_server_error());
            }
        }

        const string ext = file_path.extension();
        const string content_type = MimeTypes::getType("file" + ext);

        req->create_response(restinio::status_ok())
                .append_header_date_field()
                .append_header(restinio::http_field::content_type, content_type)
                .set_body(sendfile(processing_path))
                .done();

        return restinio::request_accepted();
    });

    return std::move(router);
}
