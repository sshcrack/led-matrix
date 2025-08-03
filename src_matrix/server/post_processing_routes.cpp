#include "post_processing_routes.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/server/server_utils.h"
#include "shared/matrix/post_processor.h"
#include "nlohmann/json.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include "../matrix_control/canvas.h"

using json = nlohmann::json;

std::unique_ptr<Server::router_t> Server::add_post_processing_routes(std::unique_ptr<router_t> router) {
    
    // Trigger flash effect
    router->http_get("/post_processing/flash", [](auto req, auto) {
        if (global_post_processor) {
            // Parse query parameters for duration and intensity
            auto query_params = parse_query_parameters(req->header().query());
            
            float duration = 0.5f;
            float intensity = 1.0f;
            
            if (query_params.find("duration") != query_params.end()) {
                try {
                    duration = std::stof(query_params["duration"]);
                    duration = std::clamp(duration, 0.1f, 5.0f); // Limit duration
                } catch (...) {
                    // Invalid duration, use default
                }
            }
            
            if (query_params.find("intensity") != query_params.end()) {
                try {
                    intensity = std::stof(query_params["intensity"]);
                    intensity = std::clamp(intensity, 0.0f, 1.0f); // Limit intensity
                } catch (...) {
                    // Invalid intensity, use default
                }
            }
            
            global_post_processor->add_effect(PostProcessType::Flash, duration, intensity);
            
            json response;
            response["status"] = "success";
            response["message"] = "Flash effect triggered";
            response["duration"] = duration;
            response["intensity"] = intensity;
            
            return reply_json(req, response);
        } else {
            return reply_with_error(req, "Post-processor not available", restinio::status_service_unavailable());
        }
    });
    
    // Trigger rotate effect
    router->http_get("/post_processing/rotate", [](auto req, auto) {
        if (global_post_processor) {
            // Parse query parameters for duration and intensity
            auto query_params = parse_query_parameters(req->header().query());
            
            float duration = 1.0f;
            float intensity = 1.0f;
            
            if (query_params.find("duration") != query_params.end()) {
                try {
                    duration = std::stof(query_params["duration"]);
                    duration = std::clamp(duration, 0.5f, 10.0f); // Limit duration
                } catch (...) {
                    // Invalid duration, use default
                }
            }
            
            if (query_params.find("intensity") != query_params.end()) {
                try {
                    intensity = std::stof(query_params["intensity"]);
                    intensity = std::clamp(intensity, 0.0f, 2.0f); // Limit intensity (can be > 1 for multiple rotations)
                } catch (...) {
                    // Invalid intensity, use default
                }
            }
            
            global_post_processor->add_effect(PostProcessType::Rotate, duration, intensity);
            
            json response;
            response["status"] = "success";
            response["message"] = "Rotate effect triggered";
            response["duration"] = duration;
            response["intensity"] = intensity;
            
            return reply_json(req, response);
        } else {
            return reply_with_error(req, "Post-processor not available", restinio::status_service_unavailable());
        }
    });
    
    // Clear all post-processing effects
    router->http_get("/post_processing/clear", [](auto req, auto) {
        if (global_post_processor) {
            global_post_processor->clear_effects();
            
            json response;
            response["status"] = "success";
            response["message"] = "All post-processing effects cleared";
            
            return reply_json(req, response);
        } else {
            return reply_with_error(req, "Post-processor not available", restinio::status_service_unavailable());
        }
    });
    
    // Get post-processing status
    router->http_get("/post_processing/status", [](auto req, auto) {
        json response;
        response["post_processor_available"] = (global_post_processor != nullptr);
        
        if (global_post_processor) {
            response["has_active_effects"] = global_post_processor->has_active_effects();
        }
        
        return reply_json(req, response);
    });

    // Configure beat response settings
    router->http_get("/post_processing/config", [](auto req, auto) {
        auto query_params = parse_query_parameters(req->header().query());
        
        json response;
        response["status"] = "success";
        
        // For now, return current hardcoded settings
        // In future, these could be configurable via config file
        response["settings"] = {
            {"beat_flash_enabled", true},
            {"beat_flash_duration", 0.4f},
            {"beat_flash_intensity", 0.8f},
            {"beat_rotate_enabled", false},
            {"beat_rotate_duration", 1.0f},
            {"beat_rotate_intensity", 1.0f}
        };
        
        return reply_json(req, response);
    });

    return router;
}