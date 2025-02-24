#include "PixelJoint.h"
#include "scenes/image/ImageScene.h"
#include "providers/collection.h"
#include "providers/pages.h"

#include <vector>
#include <restinio/uri_helpers.hpp>
#include <restinio/helpers/file_upload.hpp>
#include <restinio/helpers/multipart_body.hpp>
#include <shared/utils/shared.h>
#include <shared/server/server_utils.h>
#include <shared/utils/consts.h>

#include "../../shared/PicoSHA2/picosha2.h"

using std::vector;
using namespace ImageProviders;

vector<std::unique_ptr<ImageProviderWrapper, void (*)(Plugins::ImageProviderWrapper *)> >
PixelJoint::create_image_providers() {
    auto scenes = vector<std::unique_ptr<ImageProviderWrapper, void (*)(Plugins::ImageProviderWrapper *)> >();
    auto deleteWrapper = [](Plugins::ImageProviderWrapper *scene) {
        delete scene;
    };

    scenes.push_back({new CollectionWrapper(), deleteWrapper});
    scenes.push_back({new PagesWrapper(), deleteWrapper});

    return scenes;
}


vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)> > PixelJoint::create_scenes() {
    auto scenes = vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)> >();
    scenes.push_back({
        new ImageSceneWrapper(), [](Plugins::SceneWrapper *scene) {
            delete scene;
        }
    });

    return scenes;
}

PixelJoint::PixelJoint() = default;

extern "C" [[maybe_unused]] PixelJoint *createPixelJoint() {
    return new PixelJoint();
}

extern "C" [[maybe_unused]] void destroyPixelJoint(PixelJoint *c) {
    delete c;
}


std::optional<request_handling_status_t>
handle_providers(const request_handle_t &req, const query_string_params_t &qp) {
    if (!qp.has("preset_id")) {
        reply_with_error(req, "No Preset Id given");
        return request_accepted();
    }

    if (!qp.has("scene_id")) {
        reply_with_error(req, "No Scene Id given");
        return request_accepted();
    }
    const string preset_id{qp["preset_id"]};
    const string scene_id{qp["scene_id"]};

    const auto &presets = config->get_presets();
    if (!presets.contains(preset_id)) {
        reply_with_error(req, "Preset with id not found");
        return request_accepted();
    }

    const auto &preset = presets.at(preset_id);
    for (const auto &scene: preset.get()->scenes) {
        if (scene->get_uuid() != scene_id)
            continue;

        auto scene_json = scene->to_json();
        if (!scene_json.contains("providers")) {
            reply_with_error(req, "Invalid scene type or scene has no property for providers");
            return request_accepted();
        }

        reply_with_json(req, scene_json["providers"]);
        return request_accepted();
    }

    reply_with_error(req, "Scene not found");
    return request_accepted();
}


void store_file_to_disk(
    string_view_t full_file,
    const string_view_t raw_content,
    std::string &upload_filename) {
    auto ext = std::filesystem::path(full_file).extension();
    std::string hash;
    picosha2::hash256_hex_string(full_file, hash);

    if (!filesystem::exists(Constants::upload_dir))
        filesystem::create_directory(Constants::upload_dir);

    upload_filename = hash + ext.c_str();
    spdlog::debug("Uploading file as {}", upload_filename.c_str());

    std::ofstream dest_file;
    dest_file.exceptions(std::ofstream::failbit);
    dest_file.open(
        Constants::upload_dir / upload_filename,
        std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
    dest_file.write(raw_content.data(), raw_content.size());
}


std::optional<request_handling_status_t> handle_upload(const request_handle_t &req) {
    spdlog::debug("Handling upload");
    using namespace restinio::file_upload;

    std::string upload_filename = "";
    const auto enumeration_result = enumerate_parts_with_files(
        *req,
        [&upload_filename](const part_description_t &part) {
            if ("photo" == part.name) {
                spdlog::debug("Storing photo with filename");
                // We can handle the name only in 'filename' parameter.
                if (part.filename) {
                    // NOTE: the validity of filename is not checked.
                    // This is just for simplification of the example.
                    store_file_to_disk(*part.filename, part.body, upload_filename);

                    spdlog::debug("Outer {}", upload_filename.c_str());
                    // There is no need to handle other parts.
                    return handling_result_t::stop_enumeration;
                }
            }

            // We expect only one part with name 'file'.
            // So if that part is not found yet there is some error
            // and there is no need to continue.
            return handling_result_t::terminate_enumeration;
        });

    if (!enumeration_result || 1u != *enumeration_result) {
        spdlog::debug("Enumeration err");
        reply_with_error(req, "No file uploaded. Can't find 'photo' part");
        return request_accepted();
    }

    spdlog::debug("Reply with path {}", upload_filename.c_str());
    reply_with_json(req, {{"path", std::string(upload_filename.c_str())}});
    return request_accepted();
}


std::unique_ptr<router_t> PixelJoint::register_routes(std::unique_ptr<router_t> router) {
    auto target = router->header().path();
    const auto qp = parse_query(router->header().query());

    if (http_method_get() == router->header().method()) {
        if (target == "/pixeljoint/providers")
            return handle_providers(router, qp);
    }

    if (http_method_post() == router->header().method()) {
        if (target == "/pixeljoint/upload_img") {
            return handle_upload(router);
        }
    }
    return std::nullopt;
}
