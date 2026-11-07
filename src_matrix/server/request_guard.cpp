#include "request_guard.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <optional>

#include <restinio/all.hpp>
#include <spdlog/spdlog.h>

#include "shared/matrix/server/server_utils.h"

namespace Server::RequestGuard {
    namespace {
        std::string to_lower(std::string_view value) {
            std::string out(value);
            std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return out;
        }

        std::string_view trim(std::string_view value) {
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.remove_prefix(1);
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.remove_suffix(1);
            return value;
        }

        // "http://Host:80" -> "host", "ws://host:8080/" -> "host:8080"
        std::string origin_authority(std::string_view origin) {
            const auto scheme_end = origin.find("://");
            if (scheme_end == std::string_view::npos) return {};
            const auto scheme = to_lower(origin.substr(0, scheme_end));
            auto authority = origin.substr(scheme_end + 3);
            authority = authority.substr(0, authority.find('/'));
            auto result = to_lower(authority);
            const bool secure = scheme == "https" || scheme == "wss";
            const std::string default_port = secure ? ":443" : ":80";
            if (result.ends_with(default_port)) result.resize(result.size() - default_port.size());
            return result;
        }

        std::string host_authority(std::string_view host) {
            auto result = to_lower(trim(host));
            if (result.ends_with(":80")) result.resize(result.size() - 3);
            return result;
        }

        bool constant_time_equals(std::string_view a, std::string_view b) {
            if (a.size() != b.size()) return false;
            unsigned char diff = 0;
            for (size_t i = 0; i < a.size(); ++i)
                diff |= static_cast<unsigned char>(a[i] ^ b[i]);
            return diff == 0;
        }

        bool is_public_path(std::string_view path) {
            // The static web UI must load without a token so it can ask for one.
            return path == "/" || path == "/web" || path.starts_with("/web/");
        }

        bool origin_allowed(const Policy &policy, const RequestView &request) {
            if (policy.allow_any_origin) return true;

            if (request.has_origin) {
                if (std::ranges::any_of(policy.allowed_origins, [&](const std::string &allowed) {
                    return origin_authority(allowed) == origin_authority(request.origin);
                }))
                    return true;
                // Opaque origins ("null") come from sandboxed frames and file:// pages.
                const auto authority = origin_authority(request.origin);
                if (authority.empty()) return false;
                return authority == host_authority(request.host);
            }

            // No Origin header: only modern browsers' fetch metadata can tell us the request is cross-site
            // (e.g. <img src> or top-level navigations that trigger GET routes). Non-browser clients send neither.
            return to_lower(request.sec_fetch_site) != "cross-site";
        }

        bool token_valid(const Policy &policy, const RequestView &request) {
            if (policy.token.empty() || is_public_path(request.path)) return true;

            auto bearer = trim(request.authorization);
            if (bearer.size() > 7 && to_lower(bearer.substr(0, 7)) == "bearer ") {
                if (constant_time_equals(trim(bearer.substr(7)), policy.token)) return true;
            }
            if (!request.api_token_header.empty() && constant_time_equals(trim(request.api_token_header), policy.token))
                return true;
            // The web UI stores the token in a SameSite=Strict cookie so <img>, downloads and WebSockets carry it.
            const auto cookie = cookie_value(request.cookie_header, TOKEN_COOKIE);
            if (!cookie.empty() && constant_time_equals(cookie, policy.token)) return true;
            // Query tokens are for scripted clients that cannot set headers.
            return !request.query_token.empty() && constant_time_equals(request.query_token, policy.token);
        }
    }

    std::string_view cookie_value(std::string_view cookie_header, std::string_view name) {
        while (!cookie_header.empty()) {
            const auto separator = cookie_header.find(';');
            const auto pair = trim(cookie_header.substr(0, separator));
            const auto equals = pair.find('=');
            if (equals != std::string_view::npos && trim(pair.substr(0, equals)) == name)
                return trim(pair.substr(equals + 1));
            if (separator == std::string_view::npos) break;
            cookie_header.remove_prefix(separator + 1);
        }
        return {};
    }

    Verdict evaluate(const Policy &policy, const RequestView &request) {
        // OAuth returns from Spotify without our SameSite cookie. The callback
        // authenticates its one-time, expiring state before exchanging any code.
        if (request.path == "/spotify/callback") return Verdict::Allow;
        if (!origin_allowed(policy, request)) return Verdict::CrossOrigin;
        if (!token_valid(policy, request)) return Verdict::Unauthorized;
        return Verdict::Allow;
    }

    Policy policy_from_env() {
        Policy policy;
        if (const char *token = std::getenv("LED_MATRIX_API_TOKEN"))
            policy.token = std::string(trim(token));

        if (const char *origins = std::getenv("LED_MATRIX_ALLOWED_ORIGINS")) {
            std::string_view rest(origins);
            while (!rest.empty()) {
                const auto comma = rest.find(',');
                const auto item = trim(rest.substr(0, comma));
                if (!item.empty()) policy.allowed_origins.emplace_back(item);
                if (comma == std::string_view::npos) break;
                rest.remove_prefix(comma + 1);
            }
        }

#ifdef ENABLE_CORS
        policy.allow_any_origin = true;
        spdlog::warn("ENABLE_CORS build: cross-origin request protection is disabled");
#endif
        if (policy.token.empty())
            spdlog::info("LED_MATRIX_API_TOKEN not set; API is open to clients on the local network");
        else
            spdlog::info("API token authentication enabled");
        return policy;
    }

    std::optional<restinio::request_handling_status_t> reject_if_needed(
        const Policy &policy, const restinio::request_handle_t &req) {
        const auto &header = req->header();

        std::string query_token;
        if (!policy.token.empty()) {
            try {
                const auto qp = restinio::parse_query(header.query());
                if (const auto value = qp.get_param("token"))
                    query_token = std::string(*value);
            } catch (const std::exception &) {
                // Malformed query strings are treated as having no token.
            }
        }

        const auto origin = header.opt_value_of("Origin");
        // get_field_or returns an owned string. Keep these alive until evaluate()
        // has finished reading the RequestView's string_views.
        const auto host = header.get_field_or(restinio::http_field::host, "");
        const auto fetch_site = header.get_field_or("Sec-Fetch-Site", "");
        const auto authorization = header.get_field_or(restinio::http_field::authorization, "");
        const auto api_token = header.get_field_or("X-Api-Token", "");
        const auto cookie = header.get_field_or(restinio::http_field::cookie, "");
        RequestView view{
            .path = header.path(),
            .host = host,
            .origin = origin.value_or(""),
            .sec_fetch_site = fetch_site,
            .authorization = authorization,
            .api_token_header = api_token,
            .query_token = query_token,
            .cookie_header = cookie,
            .has_origin = origin.has_value(),
        };

        switch (evaluate(policy, view)) {
            case Verdict::Allow:
                return std::nullopt;
            case Verdict::CrossOrigin:
                spdlog::warn("Rejected cross-origin request to {} from origin '{}'", header.path(), view.origin);
                return reply_with_error(req, "Cross-origin requests are not allowed", restinio::status_forbidden());
            case Verdict::Unauthorized:
                return reply_with_error(req, "Missing or invalid API token", restinio::status_unauthorized());
        }
        return std::nullopt;
    }
}
