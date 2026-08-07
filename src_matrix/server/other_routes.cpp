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

#include "shared/matrix/interrupt.h"
#include "shared/matrix/diagnostics.h"
#include "shared/matrix/audio_state.h"
#include "shared/matrix/server/common.h"

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

    router->http_get("/web", [](auto req, auto)
                     { return handle_web_request(req, "/web/"); });

    // Static file serving
    router->http_get("/web/:path(.*)", [](auto req, auto params)
                     {
                         const auto requested_path = params["path"];
                         return handle_web_request(req, requested_path); });

    router->http_get("/uploads/:filename", [](auto req, auto params)
                     {
        const auto filename = std::string(params["filename"]);
        const filesystem::path file_path = Constants::upload_dir / filename;
        if (!filesystem::exists(file_path)) {
            return reply_with_error(req, "File not found", restinio::status_not_found());
        }

        // Prevent directory traversal
        if (filename.find("..") != std::string::npos || filename.find("/") != std::string::npos) {
            return reply_with_error(req, "Invalid path", restinio::status_forbidden());
        }
        std::error_code can_ec;
        const auto canonical = filesystem::canonical(file_path, can_ec);
        std::error_code upload_ec;
        const auto canonical_upload = filesystem::canonical(Constants::upload_dir, upload_ec);
        if (can_ec || upload_ec) {
            return reply_with_error(req, "Invalid path", restinio::status_forbidden());
        }
        if (!canonical.string().starts_with(canonical_upload.string())) {
            return reply_with_error(req, "Invalid path", restinio::status_forbidden());
        }

        const string ext = file_path.extension();
        const string content_type = MimeTypes::getType("file" + ext);
        auto response = req->create_response(restinio::status_ok())
            .append_header_date_field()
            .append_header(restinio::http_field::content_type, content_type);
        Server::add_cors_headers(response);
        return response.set_body(restinio::sendfile(file_path)).done(); });

    router->http_get("/diagnostics", [](auto req, auto)
                     {
        auto result = Diagnostics::RuntimeDiagnostics::instance().snapshot();
        result["desktop_connections"] = Server::desktop_connection_count.load();

        const auto audio = AudioState::snapshot();
        result["audio"] = {
            {"available", audio.available},
            {"fresh", audio.fresh()},
            {"age_seconds", audio.age_seconds},
            {"sequence", audio.sequence},
            {"timestamp_ms", audio.timestamp_ms},
            {"bpm", audio.feature(AudioProtocol::Feature::Bpm)},
            {"beat_phase", audio.feature(AudioProtocol::Feature::BeatPhase)},
            {"beat_confidence", audio.feature(AudioProtocol::Feature::BeatConfidence)},
            {"beat_strength", audio.feature(AudioProtocol::Feature::BeatStrength)},
            {"loudness", audio.feature(AudioProtocol::Feature::Loudness)},
            {"rms", audio.feature(AudioProtocol::Feature::Rms)},
            {"kick", audio.feature(AudioProtocol::Feature::Kick)},
            {"snare", audio.feature(AudioProtocol::Feature::Snare)},
            {"hihat", audio.feature(AudioProtocol::Feature::Hihat)},
            {"onset", audio.feature(AudioProtocol::Feature::OnsetStrength)},
            {"stereo_width", audio.feature(AudioProtocol::Feature::StereoWidth)},
            {"stereo_balance", audio.feature(AudioProtocol::Feature::StereoBalance)},
            {"spectral_centroid", audio.feature(AudioProtocol::Feature::SpectralCentroid)},
            {"energy_trend", audio.feature(AudioProtocol::Feature::EnergyTrend)},
            {"drop", audio.feature(AudioProtocol::Feature::Drop)},
            {"section_change", audio.feature(AudioProtocol::Feature::SectionChange)},
            {"bands", {
                {"sub_bass", audio.feature(AudioProtocol::Feature::SubBass)},
                {"bass", audio.feature(AudioProtocol::Feature::Bass)},
                {"low_mid", audio.feature(AudioProtocol::Feature::LowMid)},
                {"mid", audio.feature(AudioProtocol::Feature::Mid)},
                {"high_mid", audio.feature(AudioProtocol::Feature::HighMid)},
                {"treble", audio.feature(AudioProtocol::Feature::Treble)},
                {"air", audio.feature(AudioProtocol::Feature::Air)}
            }}
        };

        auto *plugin_manager = Plugins::PluginManager::instance();
        result["registry"] = plugin_manager->get_validation_report().to_json();
        result["plugins"] = json::array();
        for (const auto *plugin : plugin_manager->get_plugins()) {
            result["plugins"].push_back({
                {"name", plugin->get_plugin_name()},
                {"location", plugin->get_plugin_location()}
            });
        }

        return reply_with_json(req, result); });

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

        auto post = std::make_unique<Post>(remote_url);
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

restinio::request_handling_status_t Server::handle_web_request(const restinio::request_handle_t &req, const restinio::string_view_t requested_path)
{
#ifndef LED_MATRIX_SHARE_DIR
#define LED_MATRIX_SHARE_DIR "."
#endif
    const filesystem::path web_dir = filesystem::path(LED_MATRIX_SHARE_DIR) / "web";
    filesystem::path file_path = web_dir / requested_path;
    if (!filesystem::exists(file_path))
        file_path = web_dir / "index.html"; // Fallback to index.html if not found

    // Ensure the requested path is within the web directory
    std::error_code web_ec;
    const auto canonical_web = filesystem::canonical(web_dir, web_ec);
    std::error_code ec;
    const auto canonical_file = filesystem::canonical(file_path, ec);
    if (web_ec) {
        return reply_with_error(req, "Invalid path", restinio::status_forbidden());
    }

    if (ec || !canonical_file.string().starts_with(canonical_web.string()))
    {
        return reply_with_error(req, "Invalid path", restinio::status_forbidden());
    }

    // Serve index file if directory
    if (filesystem::is_directory(file_path))
        file_path = file_path / "index.html";

    if (!filesystem::exists(file_path))
    {
        return reply_with_error(req, "File not found", restinio::status_not_found());
    }

    const string content_type = MimeTypes::getType(file_path.string());

    spdlog::trace("Serving {}", file_path.c_str());
    auto response = req->create_response(restinio::status_ok())
                        .append_header_date_field()
                        .append_header(restinio::http_field::content_type, content_type);

    if (content_type == "application/javascript" || content_type == "text/css" || file_path.extension() == ".ico")
        response.append_header(restinio::http_field::cache_control, "public, max-age=31536000");

    Server::add_cors_headers(response);
    return response.set_body(restinio::sendfile(file_path)).done();
}