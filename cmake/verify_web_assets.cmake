# Executed by CPack just before it generates a DEB/TGZ, via
# CPACK_PROJECT_CONFIG_FILE. Refuse to ship a package without the web UI.
#
# The matrix serves its UI straight out of the installed tree
# (${LED_MATRIX_SHARE_DIR}/web). SKIP_WEB_BUILD is sticky in the CMake cache
# and silently removes the install(DIRECTORY react-web/dist ...) rule, so a
# package built that way has no web assets at all and the running matrix
# answers every /web/ request with 403 {"error":"Invalid path"} — a very
# confusing failure to debug on the Pi.
#
# This must fail here rather than at runtime, so SKIP_WEB_BUILD must never be
# silently carried into a deploy package.
#
# Note: CPack project config files are `include()`d in CPack's own scope and do
# not see the project's cache variables, so CMakeLists bakes the resolved
# values into LED_MATRIX_SKIP_WEB_BUILD / LED_MATRIX_WEB_DIST.

if(LED_MATRIX_SKIP_WEB_BUILD)
    message(FATAL_ERROR
        "Cannot package: SKIP_WEB_BUILD is ON, so the package would contain no "
        "web UI and the matrix would serve 403 {\"error\":\"Invalid path\"} for "
        "/web/. Reconfigure with -DSKIP_WEB_BUILD=OFF (build_upload.sh already "
        "does this).")
endif()

if(NOT EXISTS "${LED_MATRIX_WEB_DIST}/index.html")
    message(FATAL_ERROR
        "Cannot package: ${LED_MATRIX_WEB_DIST}/index.html is missing, so the "
        "package would contain no web UI. Build the frontend first:\n"
        "  cd react-web && pnpm install && pnpm run build")
endif()