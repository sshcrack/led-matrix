# Executed by CPack just before it generates a DEB/TGZ, via
# CPACK_PROJECT_CONFIG_FILE. Refuse to ship a package without the web UI.
#
# The matrix serves its UI straight out of ${LED_MATRIX_SHARE_DIR}/web. If
# react-web/dist is not installed, that directory does not exist on the target
# and every /web/ request answers
#
#     403 {"error":"Invalid path"}
#
# which looks like a routing or reverse-proxy problem rather than a broken
# package. Fail at packaging time instead, while the context is still obvious.
#
# SKIP_WEB_BUILD on its own is NOT an error: CI builds the frontend in the
# `prepare` job and passes react-web/dist in via the web-build artifact while
# configuring with -DSKIP_WEB_BUILD=ON. The real invariant is simply that a
# frontend build is present.
#
# Note: CPack project config files are `include()`d in CPack's own scope and do
# not see the project's cache variables, so CMakeLists bakes the resolved
# values into LED_MATRIX_SKIP_WEB_BUILD / LED_MATRIX_WEB_DIST.

if(NOT EXISTS "${LED_MATRIX_WEB_DIST}/index.html")
    if(LED_MATRIX_SKIP_WEB_BUILD)
        message(FATAL_ERROR
            "Cannot package: ${LED_MATRIX_WEB_DIST}/index.html is missing.\n"
            "SKIP_WEB_BUILD=ON means the frontend is not built during this "
            "configure, so react-web/dist must already be populated (CI does "
            "this by downloading the web-build artifact into react-web/dist/).\n"
            "Either reconfigure with -DSKIP_WEB_BUILD=OFF, or populate "
            "react-web/dist first:\n"
            "  cd react-web && pnpm install && pnpm run build\n"
            "Without it the package has no web UI and the matrix serves "
            "403 {\"error\":\"Invalid path\"} for /web/.")
    else()
        message(FATAL_ERROR
            "Cannot package: ${LED_MATRIX_WEB_DIST}/index.html is missing, so "
            "the package would contain no web UI. Build the frontend first:\n"
            "  cd react-web && pnpm install && pnpm run build")
    endif()
endif()