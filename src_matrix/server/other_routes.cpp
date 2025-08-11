#include "other_routes.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/server/server_utils.h"
#include "nlohmann/json.hpp"
#include "shared/matrix/plugin_loader/loader.h"
#include "shared/matrix/post.h"
#include <filesystem>
#include "shared/matrix/server/MimeTypes.h"
#include "shared/matrix/utils/consts.h"
#include "shared/matrix/canvas_consts.h"
#include "shared/matrix/utils/canvas_image.h"
#include <spdlog/spdlog.h>

using json = nlohmann::json;

std::unique_ptr<Server::router_t> Server::add_other_routes(std::unique_ptr<router_t> router)
{
    // Root redirect
    router->http_get("/", [](auto req, auto)
                     {
        auto response = req->create_response(restinio::status_see_other())
            .append_header(restinio::http_field::location, "/web/");
        Server::add_cors_headers(response);
        return response.done(); });

    // Static file serving
    router->http_get("/web/:path(.*)", [](auto req, auto params)
                     {
        auto exec_dir = get_exec_dir();

        const auto requested_path = params["path"];
        const filesystem::path web_dir = exec_dir / "web";
        filesystem::path file_path = web_dir / requested_path;
        if(!filesystem::exists(file_path))
            file_path = web_dir / "index.html"; // Fallback to index.html if not found


        // Ensure the requested path is within the web directory
        const auto canonical_web = filesystem::canonical(web_dir);
        std::error_code ec;
        const auto canonical_file = filesystem::canonical(file_path, ec);
        
        if (ec || !canonical_file.string().starts_with(canonical_web.string())) {
            return reply_with_error(req, "Invalid path", restinio::status_forbidden());
        }

        // Serve index file if directory
        if (filesystem::is_directory(file_path))
            file_path = file_path / "index.html";

        if (!filesystem::exists(file_path)) {
            return reply_with_error(req, "File not found", restinio::status_not_found());
        }

        const string content_type = MimeTypes::getType(file_path.string());
        
        spdlog::trace("Serving {}", file_path.c_str());
        auto response = req->create_response(restinio::status_ok())
            .append_header_date_field()
            .append_header(restinio::http_field::content_type, content_type)
            .append_header(restinio::http_field::cache_control, "public, max-age=31536000");
        Server::add_cors_headers(response);
        return response.set_body(restinio::sendfile(file_path)).done(); });

    router->http_get("/list", [](auto req, auto)
                     {
        json file_list = json::array();

        auto iterator = filesystem::directory_iterator(Constants::post_dir);
        for (const auto &entry: iterator) {
            string file_name = entry.path().filename().string();
            if (!file_name.ends_with(".p.gif"))
                continue;

            replace(file_name, ".p.gif", ".gif");
            file_list.push_back(file_name);
        }

        return reply_with_json(req, file_list); });

    router->http_get("/image", [](auto req, auto)
                     {
        const auto qp = restinio::parse_query(req->header().query());
        if (!qp.has("url")) {
            return reply_with_error(req, "No url given");
        }

        const string remote_url{qp["url"]};

        const std::unique_ptr<Post, void(*)(Post *)> post = {new Post(remote_url), [](Post *p) { delete p; }};
        const filesystem::path file_path(Constants::post_dir / post->get_filename());
        const filesystem::path processing_path = to_processed_path(file_path);
        if (!exists(processing_path)) {
            const auto res = post->process_images(Constants::width, Constants::height, true);

            if (!res.has_value() || !exists(processing_path)) {
                return reply_with_error(req, "Could not get file", restinio::status_internal_server_error());
            }
        }

        const string ext = file_path.extension();
        const string content_type = MimeTypes::getType("file" + ext);

        auto response = req->create_response(restinio::status_ok())
                .append_header_date_field()
                .append_header(restinio::http_field::content_type, content_type);
        Server::add_cors_headers(response);
        response.set_body(restinio::sendfile(processing_path)).done();

        return restinio::request_accepted(); });

    return std::move(router);
}
