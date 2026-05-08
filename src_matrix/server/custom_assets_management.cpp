#include "custom_assets_management.h"
#include "shared/common/utils/utils.h"
#include "shared/matrix/server/server_utils.h"
#include "shared/matrix/server/MimeTypes.h"
#include <filesystem>
#include <fstream>
#include <optional>
#include <spdlog/spdlog.h>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {
struct AssetTypeConfig {
    std::string type;
    fs::path directory;
    std::string extension;
};

std::optional<AssetTypeConfig> asset_type_from_param(const std::string &type) {
    const auto root = get_exec_dir() / "data";
    if (type == "lua") {
        return AssetTypeConfig{type, root / "custom_lua", ".lua"};
    }
    if (type == "shader" || type == "shaders") {
        return AssetTypeConfig{type, root / "custom_shaders", ".frag"};
    }
    return std::nullopt;
}

bool is_safe_filename(const std::string &filename) {
    return !filename.empty() &&
           filename.find('/') == std::string::npos &&
           filename.find('\\') == std::string::npos &&
           filename.find("..") == std::string::npos;
}

std::optional<std::pair<std::string, std::string>> parse_multipart_file(const std::string &content_type, const std::string &body) {
    const auto boundary_pos = content_type.find("boundary=");
    if (boundary_pos == std::string::npos) {
        return std::nullopt;
    }

    std::string boundary = content_type.substr(boundary_pos + 9);
    if (!boundary.empty() && boundary.front() == '"') {
        boundary.erase(0, 1);
    }
    if (!boundary.empty() && boundary.back() == '"') {
        boundary.pop_back();
    }
    if (boundary.empty()) {
        return std::nullopt;
    }

    const std::string boundary_marker = "--" + boundary;
    size_t pos = 0;
    while (true) {
        const auto part_start = body.find(boundary_marker, pos);
        if (part_start == std::string::npos) {
            break;
        }

        auto cursor = part_start + boundary_marker.size();
        if (body.compare(cursor, 2, "--") == 0) {
            break;
        }
        if (body.compare(cursor, 2, "\r\n") == 0) {
            cursor += 2;
        }

        const auto headers_end = body.find("\r\n\r\n", cursor);
        if (headers_end == std::string::npos) {
            break;
        }
        const std::string headers = body.substr(cursor, headers_end - cursor);
        const auto filename_pos = headers.find("filename=\"");
        const auto data_start = headers_end + 4;
        const auto data_end = body.find("\r\n" + boundary_marker, data_start);
        if (data_end == std::string::npos) {
            break;
        }

        if (filename_pos != std::string::npos) {
            const auto filename_start = filename_pos + 10;
            const auto filename_end = headers.find('"', filename_start);
            if (filename_end != std::string::npos) {
                std::string filename = headers.substr(filename_start, filename_end - filename_start);
                std::string data = body.substr(data_start, data_end - data_start);
                return std::make_pair(filename, data);
            }
        }

        pos = data_end + 2;
    }

    return std::nullopt;
}
}

std::unique_ptr<Server::router_t> Server::add_custom_assets_routes(std::unique_ptr<router_t> router) {
    router->http_get("/api/custom-assets/:type", [](auto req, auto params) {
        const auto type = std::string(params["type"]);
        const auto cfg_opt = asset_type_from_param(type);
        if (!cfg_opt.has_value()) {
            return reply_with_error(req, "Invalid asset type", restinio::status_bad_request());
        }

        const auto cfg = cfg_opt.value();
        std::error_code ec;
        fs::create_directories(cfg.directory, ec);
        if (ec) {
            return reply_with_error(req, "Failed to prepare asset directory", restinio::status_internal_server_error());
        }

        json result = json::array();
        for (const auto &entry : fs::directory_iterator(cfg.directory, ec)) {
            if (ec) {
                break;
            }
            if (!entry.is_regular_file() || entry.path().extension() != cfg.extension) {
                continue;
            }

            json item = {
                {"filename", entry.path().filename().string()},
                {"size", static_cast<uint64_t>(entry.file_size())},
                {"last_modified", entry.last_write_time().time_since_epoch().count()},
            };
            result.push_back(item);
        }

        return reply_with_json(req, result);
    });

    router->http_post("/api/custom-assets/:type", [](auto req, auto params) {
        const auto type = std::string(params["type"]);
        const auto cfg_opt = asset_type_from_param(type);
        if (!cfg_opt.has_value()) {
            return reply_with_error(req, "Invalid asset type", restinio::status_bad_request());
        }

        const auto cfg = cfg_opt.value();
        std::error_code ec;
        fs::create_directories(cfg.directory, ec);
        if (ec) {
            return reply_with_error(req, "Failed to prepare asset directory", restinio::status_internal_server_error());
        }

        const auto content_type = req->header().get_field(restinio::http_field::content_type);
        const auto parsed_file = parse_multipart_file(content_type, req->body());
        if (!parsed_file.has_value()) {
            return reply_with_error(req, "No multipart file found in request", restinio::status_bad_request());
        }

        auto [filename, data] = parsed_file.value();
        if (!is_safe_filename(filename)) {
            return reply_with_error(req, "Invalid filename", restinio::status_bad_request());
        }
        if (fs::path(filename).extension() != cfg.extension) {
            return reply_with_error(req, "Invalid file extension", restinio::status_bad_request());
        }

        const auto target_path = cfg.directory / filename;
        std::ofstream out(target_path, std::ios::binary);
        if (!out.is_open()) {
            return reply_with_error(req, "Could not open target file", restinio::status_internal_server_error());
        }
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
        out.close();

        spdlog::info("Uploaded custom asset '{}' to '{}'", filename, target_path.string());
        return reply_with_json(req, json{
            {"success", true},
            {"filename", filename},
        });
    });

    router->http_delete("/api/custom-assets/:type/:filename", [](auto req, auto params) {
        const auto type = std::string(params["type"]);
        const auto filename = std::string(params["filename"]);
        const auto cfg_opt = asset_type_from_param(type);
        if (!cfg_opt.has_value()) {
            return reply_with_error(req, "Invalid asset type", restinio::status_bad_request());
        }
        if (!is_safe_filename(filename)) {
            return reply_with_error(req, "Invalid filename", restinio::status_bad_request());
        }

        const auto cfg = cfg_opt.value();
        const auto target_path = cfg.directory / filename;
        std::error_code ec;
        if (!fs::exists(target_path, ec) || ec) {
            return reply_with_error(req, "File not found", restinio::status_not_found());
        }

        fs::remove(target_path, ec);
        if (ec) {
            return reply_with_error(req, "Could not delete file", restinio::status_internal_server_error());
        }

        spdlog::info("Deleted custom asset '{}' from '{}'", filename, target_path.string());
        return reply_with_json(req, json{
            {"success", true},
            {"filename", filename},
        });
    });

    router->http_get("/api/custom-assets/:type/:filename/download", [](auto req, auto params) {
        const auto type = std::string(params["type"]);
        const auto filename = std::string(params["filename"]);
        const auto cfg_opt = asset_type_from_param(type);
        if (!cfg_opt.has_value()) {
            return reply_with_error(req, "Invalid asset type", restinio::status_bad_request());
        }
        if (!is_safe_filename(filename)) {
            return reply_with_error(req, "Invalid filename", restinio::status_bad_request());
        }

        const auto cfg = cfg_opt.value();
        const auto target_path = cfg.directory / filename;
        std::error_code ec;
        if (!fs::exists(target_path, ec) || ec) {
            return reply_with_error(req, "File not found", restinio::status_not_found());
        }

        auto response = req->create_response(restinio::status_ok())
            .append_header_date_field()
            .append_header(restinio::http_field::content_type, MimeTypes::getType(target_path.string()))
            .append_header(restinio::http_field::content_disposition, "attachment; filename=\"" + filename + "\"");
        Server::add_cors_headers(response);
        return response.set_body(restinio::sendfile(target_path)).done();
    });

    return std::move(router);
}

