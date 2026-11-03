#pragma once

#include <string>
#include <optional>
#include <string_view>
#include <vector>

#include <restinio/core.hpp>

namespace Server::RequestGuard {
    struct Policy {
        // Optional shared secret. Empty means no token is required.
        std::string token;
        // Extra browser origins (e.g. "http://localhost:5173") that may call the API cross-origin.
        std::vector<std::string> allowed_origins;
        // Disables the cross-origin check entirely (only for ENABLE_CORS development builds).
        bool allow_any_origin = false;
    };

    struct RequestView {
        std::string_view path;
        std::string_view host;
        std::string_view origin;
        std::string_view sec_fetch_site;
        std::string_view authorization;
        std::string_view api_token_header;
        std::string_view query_token;
        std::string_view cookie_header;
        bool has_origin = false;
    };

    enum class Verdict {
        Allow,
        CrossOrigin,
        Unauthorized,
    };

    [[nodiscard]] Verdict evaluate(const Policy &policy, const RequestView &request);

    inline constexpr std::string_view TOKEN_COOKIE = "led_matrix_token";

    // Extracts a cookie value from a Cookie header ("a=1; b=2").
    [[nodiscard]] std::string_view cookie_value(std::string_view cookie_header, std::string_view name);

    // Reads LED_MATRIX_API_TOKEN and LED_MATRIX_ALLOWED_ORIGINS (comma separated).
    [[nodiscard]] Policy policy_from_env();

    // Returns std::nullopt when the request may proceed, otherwise the rejection that was sent.
    [[nodiscard]] std::optional<restinio::request_handling_status_t> reject_if_needed(
        const Policy &policy, const restinio::request_handle_t &req);
}
