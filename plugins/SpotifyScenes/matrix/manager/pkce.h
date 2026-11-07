#pragma once
#include <cstdint>
#include <picosha2.h>
#include <string>
#include <vector>

// RFC 7636 (PKCE) and RFC 4648 §5 helpers. Header-only so tests/pkce_smoke.cpp can use them directly.
namespace Pkce {
    inline std::string base64url(const std::string &in) {
        static constexpr char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        std::string out;
        size_t i = 0;
        for (; i + 2 < in.size(); i += 3) {
            const unsigned v = (uint8_t(in[i]) << 16) | (uint8_t(in[i + 1]) << 8) | uint8_t(in[i + 2]);
            out += tbl[v >> 18]; out += tbl[(v >> 12) & 63]; out += tbl[(v >> 6) & 63]; out += tbl[v & 63];
        }
        if (i + 1 == in.size()) {
            const unsigned v = uint8_t(in[i]) << 16;
            out += tbl[v >> 18]; out += tbl[(v >> 12) & 63];
        } else if (i + 2 == in.size()) {
            const unsigned v = (uint8_t(in[i]) << 16) | (uint8_t(in[i + 1]) << 8);
            out += tbl[v >> 18]; out += tbl[(v >> 12) & 63]; out += tbl[(v >> 6) & 63];
        }
        return out;
    }

    /// code_challenge for method S256: BASE64URL(SHA256(verifier)) without padding.
    inline std::string challenge(const std::string &verifier) {
        std::vector<unsigned char> hash(picosha2::k_digest_size);
        picosha2::hash256(verifier.begin(), verifier.end(), hash.begin(), hash.end());
        return base64url(std::string(hash.begin(), hash.end()));
    }
}
