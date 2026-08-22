#include "scene_lab.h"

#include <algorithm>
#include <cctype>

#include "matrix_control/SceneLabRuntime.h"
#include "shared/matrix/runtime_inputs.h"
#include "shared/matrix/server/server_utils.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/utils/uuid.h"

using json = nlohmann::json;

namespace {
json parse_body(const restinio::request_handle_t &req)
{
    if (req->body().empty()) return json::object();
    return json::parse(req->body());
}

std::string variant_slug(std::string label)
{
    std::string out = "custom.";
    bool separator = false;
    for (unsigned char c : label) {
        if (std::isalnum(c)) {
            out.push_back(static_cast<char>(std::tolower(c)));
            separator = false;
        } else if (!separator && out.size() > 7) {
            out.push_back('-');
            separator = true;
        }
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    if (out == "custom.") out += uuid::generate_uuid_v4().substr(0, 8);
    return out;
}

json session_payload()
{
    return SceneLabRuntime::instance().status_json(RuntimeInputs::snapshot());
}
}

std::unique_ptr<Server::router_t> Server::add_scene_lab_routes(std::unique_ptr<router_t> router)
{
    router->http_get("/scene_lab", [](auto req, auto) {
        return reply_with_json(req, session_payload());
    });

    router->http_post("/scene_lab/start", [](auto req, auto) {
        try {
            const auto body = parse_body(req);
            const auto scene = body.value("scene", std::string{});
            if (scene.empty()) return reply_with_error(req, "Scene not given");
            const auto state = SceneLabRuntime::instance().start(
                scene, body.value("variant", std::string{}),
                body.value("properties", json::object()), body.value("fps", 20),
                RuntimeInputs::snapshot());
            exit_canvas_update.store(true);
            return reply_with_json(req, SceneLabRuntime::instance().status_json(RuntimeInputs::snapshot()));
        } catch (const std::exception &e) {
            return reply_with_error(req, e.what());
        }
    });

    router->http_post("/scene_lab/update", [](auto req, auto) {
        try {
            const auto current = SceneLabRuntime::instance().snapshot(RuntimeInputs::snapshot());
            if (!current.active) return reply_with_error(req, "Scene Lab is not active");
            const auto body = parse_body(req);
            std::optional<std::uint64_t> expected_generation;
            if (body.contains("expected_generation"))
                expected_generation = body.at("expected_generation").template get<std::uint64_t>();
            SceneLabRuntime::instance().update(
                body.value("variant", current.variant),
                body.value("properties", current.properties),
                body.value("fps", current.fps), RuntimeInputs::snapshot(), expected_generation);
            exit_canvas_update.store(true);
            return reply_with_json(req, session_payload());
        } catch (const std::exception &e) {
            return reply_with_error(req, e.what());
        }
    });

    router->http_post("/scene_lab/heartbeat", [](auto req, auto) {
        SceneLabRuntime::instance().heartbeat();
        return reply_with_json(req, session_payload());
    });

    router->http_post("/scene_lab/stop", [](auto req, auto) {
        SceneLabRuntime::instance().stop();
        exit_canvas_update.store(true);
        return reply_with_json(req, session_payload());
    });

    router->http_post("/scene_lab/save_variant", [](auto req, auto) {
        try {
            const auto state = SceneLabRuntime::instance().snapshot(RuntimeInputs::snapshot());
            if (!state.active || !state.scene) return reply_with_error(req, "Scene Lab is not active");
            const auto body = parse_body(req);
            const std::string label = body.value("label", std::string{});
            if (label.empty()) return reply_with_error(req, "Variant label not given");
            ConfigData::CustomSceneVariant variant;
            variant.id = body.value("id", std::string{});
            if (variant.id.empty()) variant.id = variant_slug(label);
            if (!variant.id.starts_with("custom.")) variant.id = "custom." + variant_slug(variant.id).substr(7);
            variant.label = label;
            variant.description = body.value("description", std::string("Saved from Scene Lab"));
            const auto updated = SceneLabRuntime::instance().update(
                variant.id, state.properties, state.fps, RuntimeInputs::snapshot(), state.generation);
            variant.properties = updated.properties;
            config->set_custom_scene_variant(state.scene_name, variant);
            if (!config->save())
                return reply_with_error(req, "Could not persist Scene Lab variant", restinio::status_internal_server_error());
            exit_canvas_update.store(true);
            return reply_with_json(req, {
                {"success", true}, {"scene", state.scene_name}, {"variant", variant},
                {"generation", updated.generation}
            });
        } catch (const std::exception &e) {
            return reply_with_error(req, e.what());
        }
    });

    router->http_post("/scene_lab/save_preset", [](auto req, auto) {
        try {
            const auto state = SceneLabRuntime::instance().snapshot(RuntimeInputs::snapshot());
            if (!state.active || !state.scene) return reply_with_error(req, "Scene Lab is not active");
            const auto body = parse_body(req);
            const std::string label = body.value("display_name", std::string("Scene Lab look"));
            json scene_json{
                {"type", state.scene_name}, {"arguments", state.properties},
                {"uuid", uuid::generate_uuid_v4()}
            };
            if (!state.variant.empty()) scene_json["variant"] = state.variant;
            auto preset = std::make_shared<ConfigData::Preset>();
            preset->display_name = label;
            preset->transition_duration = 750;
            preset->transition_name = "blend";
            preset->scenes.push_back(std::shared_ptr<Scenes::Scene>(Scenes::Scene::from_json(scene_json)));
            std::string id;
            do { id = uuid::generate_uuid_v4(); } while (config->get_presets().contains(id));
            config->set_presets(id, preset);
            config->save();
            return reply_with_json(req, {{"success", true}, {"id", id}, {"display_name", label}});
        } catch (const std::exception &e) {
            return reply_with_error(req, e.what());
        }
    });

    return router;
}
