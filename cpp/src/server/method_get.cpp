#include "restinio/all.hpp"
#include "shared/utils/shared.h"
#include "shared/post.h"
#include <filesystem>
#include "nlohmann/json.hpp"
#include "shared/utils/canvas_image.h"
#include "server_utils.h"
#include "shared/utils/utils.h"
#include "shared/utils/consts.h"
#include "utils/canvas_consts.h"
#include <spdlog/spdlog.h>

using namespace std;
using namespace spdlog;
using namespace restinio;
using json = nlohmann::json;

request_handling_status_t handle_get(const request_handle_t &req) {

    auto target = req->header().path();
    const auto qp = restinio::parse_query(req->header().query());

    if (target == "/toggle") {
        turned_off.store(!turned_off.load());
        exit_canvas_update.store(true);

        reply_with_json(req, {{"turned_off", turned_off.load()}});
        return request_accepted();
    }

    if (target == "/skip") {
        skip_image.store(true);
        reply_success(req);

        return request_accepted();
    }

    if (target == "/set_preset") {
        if (!qp.has("id")) {
            reply_with_error(req, "No Id given");
            return request_accepted();
        }

        string id{qp["id"]};
        auto presets = config->get_presets();
        if (presets.find(id) == presets.end()) {
            reply_with_error(req, "Invalid id");
            return request_accepted();
        }

        config->set_curr(id);
        reply_success(req);
        return request_accepted();
    }

    if (target == "/presets") {
        auto presets = config->get_presets();
        if (!qp.has("id")) {
            vector<string> keys;
            for (const auto &item: presets) {
                keys.push_back(item.first);
            }

            json j = keys;
            reply_with_json(req, j);
            return request_accepted();
        }

        string id{qp["id"]};
        auto p = presets.find(id);
        if (p == presets.end()) {
            reply_with_error(req, "Could not find id");
            return request_accepted();
        }

        json j = p->second;
        reply_with_json(req, j);
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

    if (target.starts_with("/image")) {
        if (!qp.has("url")) {
            reply_with_error(req, "No url given");
            return request_accepted();
        }

        string remote_url{qp["url"]};

        auto post = new Post(remote_url);
        filesystem::path file_path(Constants::root_dir + post->get_filename());
        filesystem::path processing_path = to_processed_path(file_path);
        if (!filesystem::exists(processing_path)) {
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
                .append_header(http_field::content_type, content_type)
                .set_body(sendfile(processing_path))
                .done();

        return request_accepted();
    }

    if (target == "/list_presets") {
        auto groups = config->get_presets();
        json j(groups);

        reply_with_json(req, j);
        return request_accepted();
    }

    return request_rejected();
}
