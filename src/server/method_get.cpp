#include "restinio/all.hpp"
#include "shared/utils/shared.h"
#include "shared/post.h"
#include <filesystem>
#include <shared/server/MimeTypes.h>

#include "nlohmann/json.hpp"
#include "shared/utils/canvas_image.h"
#include "shared/server/server_utils.h"
#include "shared/utils/utils.h"
#include "shared/utils/consts.h"
#include "utils/canvas_consts.h"
#include "shared/plugin_loader/loader.h"
#include <spdlog/spdlog.h>

using namespace std;
using namespace spdlog;
using namespace restinio;
using json = nlohmann::json;

request_handling_status_t handle_get(const request_handle_t &req) {
    auto target = req->header().path();
    const auto qp = parse_query(req->header().query());

    if (target == "/toggle") {
        turned_off.store(!turned_off.load());
        exit_canvas_update.store(true);

        reply_with_json(req, {{"turned_off", turned_off.load()}});
        return request_accepted();
    }

    if (target == "/set_enabled") {
        if (!qp.has("enabled")) {
            reply_with_error(req, "No enabled given");
            return request_accepted();
        }

        turned_off.store(qp["enabled"] == "false");
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
            for (const auto &key: presets | views::keys) {
                keys.push_back(key);
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
        return request_accepted();
    }

    if (target.starts_with("/image")) {
        if (!qp.has("url")) {
            reply_with_error(req, "No url given");
            return request_accepted();
        }

        string remote_url{qp["url"]};

        std::unique_ptr<Post, void(*)(Post *)> post = {new Post(remote_url), [](Post *p) { delete p; }};
        filesystem::path file_path(Constants::post_dir / post->get_filename());
        filesystem::path processing_path = to_processed_path(file_path);
        if (!exists(processing_path)) {
            // ReSharper disable once CppTooWideScopeInitStatement
            auto res = post->process_images(Constants::width, Constants::height);

            if (!res.has_value() || !exists(processing_path)) {
                reply_with_error(req, "Could not get file", status_internal_server_error());
                return request_accepted();
            }
        }

        string ext = file_path.extension();
        string content_type = MimeTypes::getType("file" + ext);

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

    if (target == "/get_curr") {
        std::vector<json> scenes;

        for (const auto &item: config->get_curr()->scenes) {
            json j;
            j["name"] = item->get_name();
            j["properties"] = item->to_json();

            scenes.push_back(j);
        }

        reply_with_json(req, scenes);
        return request_accepted();
    }

    if (target == "/list_scenes") {
        auto scenes = Plugins::PluginManager::instance()->get_scenes();
        std::vector<json> j;

        for (const auto &item: scenes) {
            auto properties = item->get_default()->get_properties();
            std::vector<json> properties_json;

            for (const auto &item1: properties) {
                json j1;
                item1->dump_to_json(j1);

                json j2;
                j2["name"] = item1->getName();
                j2["default_value"] = j1[item1->getName()];
                j2["type_id"] = item1->get_type_id();

                properties_json.push_back(j2);
            }

            json j1 = {
                {"name", item->get_name()},
                {"properties", properties_json}
            };

            j.push_back(j1);
        }

        reply_with_json(req, j);
        return request_accepted();
    }

    if (target == "/status") {
        json j;
        j["turned_off"] = turned_off.load();
        j["current"] = config->get_curr_id();

        reply_with_json(req, j);
        return request_accepted();
    }


    return request_rejected();
}
