#include "restinio/all.hpp"
#include "../shared.h"
#include <filesystem>
#include "nlohmann/json.hpp"
#include "../matrix_control/image.h"
#include "server_utils.h"
#include "../utils.h"
#include "../consts.h"
#include <spdlog/spdlog.h>

using namespace std;
using namespace spdlog;
using namespace restinio;
using json = nlohmann::json;

request_handling_status_t handle_get(const request_handle_t &req) {

    auto target = req->header().path();
    const auto qp = restinio::parse_query(req->header().query());

    if (target == "/skip") {
        skip_image.store(true);
        reply_success(req);

        return request_accepted();
    }

    if (target == "/preset") {
        if (!qp.has("id")) {
            reply_with_error(req, "No Id given");
            return request_accepted();
        }

        string id{qp["id"]};
        auto groups = config->get_groups();
        if (groups.find(id) == groups.end()) {
            reply_with_error(req, "Invalid id");
            return request_accepted();
        }

        config->set_curr(id);
        reply_success(req);
        return request_accepted();
    }

    if (target == "/list") {
        auto res = req->create_response(status_ok())
                .append_header_date_field()
                .append_header(http_field::content_type, "application/json; charset=utf8");
        res.append_body("[");

        filesystem::directory_iterator iterator = filesystem::directory_iterator(Constants::root_dir);
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
        return request_accepted();
    }

    string img_prefix = "/image/";
    if (target.starts_with(img_prefix)) {
        string file_name(target.substr(img_prefix.length()));
        if (!is_valid_filename(file_name)) {
            reply_with_error(req, "Invalid filename");
            return request_accepted();
        }

        filesystem::path file_path(Constants::root_dir + file_name);
        filesystem::path processing_path = to_processed_path(file_path);
        if (!filesystem::exists(processing_path)) {
            auto post = new Post(Constants::post_img_url + file_path.filename().string());
            auto res = post->process_images(Constants::width, Constants::height);

            if (!res.has_value() || !filesystem::exists(processing_path)) {
                reply_with_error(req, "Could not get file", status_internal_server_error());
                return request_accepted();
            }
        }

        string ext = file_path.extension();
        transform(ext.begin(), ext.end(), ext.begin(),
                  [](unsigned char c) { return tolower(c); }
        );

        string mime;
        if (ext == ".gif") {
            mime = "gif";
        }
        if (ext == ".png") {
            mime = "png";
        }
        if (ext == "jpeg" || ext == "jpg") {
            mime = "jpeg";
        }

        string content_type = "application/octet-stream";
        if (!mime.empty()) {
            content_type = "image/" + mime;
        }

        req->create_response(status_ok())
                .append_header_date_field()
                .append_header(http_field::content_type, fmt::format("{}", content_type))
                .set_body(sendfile(processing_path))
                .done();

        return request_accepted();
    }

    if(target == "/list_presets") {
        auto groups = config->get_groups();
        json j(groups);

        reply_with_json(req, j);
        return request_accepted();
    }

    return request_rejected();
}
