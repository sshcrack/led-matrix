#include "manager/pkce.h"

#include <iostream>

int main() {
    int failures = 0;
    const auto expect = [&](const char *name, const std::string &actual, const std::string &expected) {
        if (actual != expected) {
            std::cerr << "FAIL: " << name << " (got '" << actual << "', expected '" << expected << "')\n";
            ++failures;
        }
    };
    // RFC 7636 appendix B
    expect("rfc7636 challenge", Pkce::challenge("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk"),
           "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM");
    // RFC 4648 §10 vectors (padding stripped)
    expect("b64 f", Pkce::base64url("f"), "Zg");
    expect("b64 fo", Pkce::base64url("fo"), "Zm8");
    expect("b64 foo", Pkce::base64url("foo"), "Zm9v");
    expect("b64 foobar", Pkce::base64url("foobar"), "Zm9vYmFy");
    expect("b64 url alphabet", Pkce::base64url(std::string("\xfb\xff", 2)), "-_8");
    return failures ? 1 : 0;
}
