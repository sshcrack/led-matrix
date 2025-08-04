#include "post_processing_routes.h"
#include "shared/matrix/server/server_utils.h"
#include "shared/matrix/post_processor.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include "shared/matrix/canvas_consts.h"

using json = nlohmann::json;

std::unique_ptr<Server::router_t> Server::add_post_processing_routes(std::unique_ptr<router_t> router) {
    
    // Trigger flash effect
    router->http_get("/post_processing/flash", [](const auto& req, auto) {
        if (Constants::global_post_processor) {
            // Parse query parameters for duration and intensity
            auto query_params = restinio::parse_query(req->header().query());
            
            float duration = 0.5f;
            float intensity = 1.0f;
            
            if (query_params.has("duration")) {
                try {
                    duration = std::stof(std::string(query_params["duration"]));
                    duration = std::clamp(duration, 0.1f, 5.0f); // Limit duration
                } catch (...) {
                    // Invalid duration, use default
                }
            }
            
            if (query_params.has("intensity")) {
                try {
                    intensity = std::stof(std::string(query_params["intensity"]));
                    intensity = std::clamp(intensity, 0.0f, 1.0f); // Limit intensity
                } catch (...) {
                    // Invalid intensity, use default
                }
            }
            
            Constants::global_post_processor->add_effect("flash", duration, intensity);
            
            json response;
            response["status"] = "success";
            response["message"] = "Flash effect triggered";
            response["duration"] = duration;
            response["intensity"] = intensity;
            
            return reply_with_json(req, response);
        } else {
            return reply_with_error(req, "Post-processor not available", restinio::status_service_unavailable());
        }
    });
    
    // Trigger rotate effect
    router->http_get("/post_processing/rotate", [](const auto&  req, auto) {
        if (Constants::global_post_processor) {
            // Parse query parameters for duration and intensity
            auto query_params = restinio::parse_query(req->header().query());
            
            float duration = 1.0f;
            float intensity = 1.0f;
            
            if (query_params.has("duration")) {
                try {
                    duration = std::stof(std::string(query_params["duration"]));
                    duration = std::clamp(duration, 0.5f, 10.0f); // Limit duration
                } catch (...) {
                    // Invalid duration, use default
                }
            }
            
            if (query_params.has("intensity")) {
                try {
                    intensity = std::stof(std::string(query_params["intensity"]));
                    intensity = std::clamp(intensity, 0.0f, 2.0f); // Limit intensity (can be > 1 for multiple rotations)
                } catch (...) {
                    // Invalid intensity, use default
                }
            }
            
            Constants::global_post_processor->add_effect("rotate", duration, intensity);
            
            json response;
            response["status"] = "success";
            response["message"] = "Rotate effect triggered";
            response["duration"] = duration;
            response["intensity"] = intensity;
            
            return reply_with_json(req, response);
        } else {
            return reply_with_error(req, "Post-processor not available", restinio::status_service_unavailable());
        }
    });
    
    // Clear all post-processing effects
    router->http_get("/post_processing/clear", [](const auto&  req, auto) {
        if (Constants::global_post_processor) {
            Constants::global_post_processor->clear_effects();
            
            json response;
            response["status"] = "success";
            response["message"] = "All post-processing effects cleared";
            
            return reply_with_json(req, response);
        } else {
            return reply_with_error(req, "Post-processor not available", restinio::status_service_unavailable());
        }
    });
    
    // Get post-processing status
    router->http_get("/post_processing/status", [](const auto& req, auto) {
        json response;
        response["post_processor_available"] = (Constants::global_post_processor != nullptr);
        
        if (Constants::global_post_processor) {
            response["has_active_effects"] = Constants::global_post_processor->has_active_effects();
            response["registered_effects"] = Constants::global_post_processor->get_registered_effects();
        }
        
        return reply_with_json(req, response);
    });

    // Generic endpoint to trigger any registered effect
    router->http_get("/post_processing/effect/:effect_name", [](const auto& req, auto params) {
        if (Constants::global_post_processor) {
            auto effect_name = std::string(params["effect_name"]);
            auto query_params = restinio::parse_query(req->header().query());
            
            float duration = 1.0f;
            float intensity = 1.0f;
            
            if (query_params.has("duration")) {
                try {
                    duration = std::stof(std::string(query_params["duration"]));
                    duration = std::clamp(duration, 0.1f, 10.0f);
                } catch (...) {
                    // Invalid duration, use default
                }
            }
            
            if (query_params.has("intensity")) {
                try {
                    intensity = std::stof(std::string(query_params["intensity"]));
                    intensity = std::clamp(intensity, 0.0f, 2.0f);
                } catch (...) {
                    // Invalid intensity, use default
                }
            }
            
            if (Constants::global_post_processor->add_effect(effect_name, duration, intensity)) {
                json response;
                response["status"] = "success";
                response["message"] = "Effect '" + effect_name + "' triggered";
                response["effect_name"] = effect_name;
                response["duration"] = duration;
                response["intensity"] = intensity;
                
                return reply_with_json(req, response);
            }

            return reply_with_error(req, "Unknown effect: " + effect_name, restinio::status_bad_request());
        }
        return reply_with_error(req, "Post-processor not available", restinio::status_service_unavailable());
    });

    // Configure beat response settings
    router->http_get("/post_processing/config", [](const auto& req, auto) {
        auto query_params = restinio::parse_query(req->header().query());
        
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
        
        return reply_with_json(req, response);
    });

    return router;
}