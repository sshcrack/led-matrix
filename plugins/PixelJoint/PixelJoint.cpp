#include "PixelJoint.h"
#include "scenes/image/ImageScene.h"
#include "providers/collection.h"
#include "providers/pages.h"

#include <vector>
#include <restinio/uri_helpers.hpp>
#include <shared/utils/shared.h>
#include <shared/server/server_utils.h>

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


std::optional<request_handling_status_t> PixelJoint::handle_request(const request_handle_t &req) {
    if (http_method_get() != req->header().method())
        return std::nullopt;

    auto target = req->header().path();
    const auto qp = parse_query(req->header().query());

    if (target != "/pixeljoint/providers")
        return std::nullopt;

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

    const auto& presets = config->get_presets();
    if (!presets.contains(preset_id)) {
        reply_with_error(req, "Preset with id not found");
        return request_accepted();
    }

    const auto& preset = presets.at(preset_id);
    for (const auto & scene : preset.get()->scenes) {
        if (scene->get_uuid() != scene_id)
            continue;

        auto scene_json =  scene->to_json();
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
