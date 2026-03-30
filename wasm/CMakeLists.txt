cmake_minimum_required(VERSION 3.15)
project(led-matrix-wasm CXX)

# ---------------------------------------------------------------------------
# Require Emscripten toolchain
# ---------------------------------------------------------------------------
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    message(FATAL_ERROR
        "This CMakeLists.txt must be built with the Emscripten toolchain.\n"
        "Use:  emcmake cmake <source-dir>  (or set -DCMAKE_TOOLCHAIN_FILE=...)")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Fetch header-only dependencies that are vcpkg-managed in native builds
# ---------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
    DOWNLOAD_NO_EXTRACT TRUE
    DOWNLOAD_DIR "${CMAKE_CURRENT_BINARY_DIR}/_deps/nlohmann")

FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(magic_enum
    GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
    GIT_TAG        v0.9.5
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(magic_enum)

FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        11.1.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(fmt)

# ---------------------------------------------------------------------------
# nlohmann/json single-header: place it where #include "nlohmann/json.hpp" works
# ---------------------------------------------------------------------------
set(NLOHMANN_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/_deps/nlohmann")
# The FetchContent download puts json.hpp directly in _deps/nlohmann
# Include path: "-I _deps/nlohmann/.." makes "nlohmann/json.hpp" resolvable.
get_filename_component(NLOHMANN_PARENT "${NLOHMANN_INCLUDE_DIR}" DIRECTORY)

# ---------------------------------------------------------------------------
# Source paths (relative to repository root)
# ---------------------------------------------------------------------------
set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(PLUGINS_EXAMPLE "${REPO_ROOT}/plugins/ExampleScenes/matrix")

set(WASM_SOURCES
    # WASM runtime
    "${CMAKE_CURRENT_SOURCE_DIR}/runtime/wasm_runtime.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/runtime/wasm_scene.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/runtime/wasm_stubs.cpp"

    # ExampleScenes – WASM v1 target scenes
    "${PLUGINS_EXAMPLE}/scenes/ColorPulseScene.cpp"
    "${PLUGINS_EXAMPLE}/scenes/PropertyDemoScene.cpp"
    "${PLUGINS_EXAMPLE}/scenes/RenderingDemoScene.cpp"

    # Shared matrix utilities (no heavy dependencies)
    "${REPO_ROOT}/shared/matrix/src/shared/matrix/utils/FrameTimer.cpp"
)

# ---------------------------------------------------------------------------
# Build target
# ---------------------------------------------------------------------------
add_executable(scene_preview ${WASM_SOURCES})

target_compile_features(scene_preview PRIVATE cxx_std_23)

# WASM override headers must come BEFORE the shared matrix headers so that
# the shim versions of led-matrix.h, Magick++.h, etc. are picked up first.
target_include_directories(scene_preview PRIVATE
    # 1. WASM shims (override platform-specific headers)
    "${CMAKE_CURRENT_SOURCE_DIR}/include"

    # 2. Shared project headers
    "${REPO_ROOT}/shared/matrix/include"
    "${REPO_ROOT}/shared/common/include"

    # 3. Plugin scene headers
    "${PLUGINS_EXAMPLE}"
    "${PLUGINS_EXAMPLE}/scenes"

    # 4. Fetched dependencies
    "${NLOHMANN_PARENT}"
)

target_link_libraries(scene_preview PRIVATE
    magic_enum::magic_enum
    spdlog::spdlog
    fmt::fmt
)

# ---------------------------------------------------------------------------
# Emscripten link flags
#   - MODULARIZE + EXPORT_NAME: produce a factory function loadable as an
#     ES module or via importScripts()
#   - EXPORTED_FUNCTIONS: our public C ABI + malloc/free for string passing
#   - EXPORTED_RUNTIME_METHODS: JS helpers for UTF-8 string conversion
#   - ALLOW_MEMORY_GROWTH: safe for varying canvas sizes
#   - NO_EXIT_RUNTIME: keep the module alive between calls
#   - --no-entry: no main() / _start entry point
# ---------------------------------------------------------------------------
set(EMSCRIPTEN_LINK_FLAGS
    "-sMODULARIZE=1"
    "-sEXPORT_NAME=createScenePreview"
    "-sEXPORTED_FUNCTIONS=['_wasm_list_scenes','_wasm_scene_create','_wasm_scene_set_properties','_wasm_scene_render_frame','_wasm_scene_destroy','_wasm_get_frame_buffer','_wasm_get_buffer_size','_malloc','_free']"
    "-sEXPORTED_RUNTIME_METHODS=['UTF8ToString','stringToUTF8','lengthBytesUTF8']"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sNO_EXIT_RUNTIME=1"
    "--no-entry"
    "-O2"
)
string(JOIN " " EMSCRIPTEN_LINK_FLAGS_STR ${EMSCRIPTEN_LINK_FLAGS})

set_target_properties(scene_preview PROPERTIES
    SUFFIX ".js"
    LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS_STR}"
)

# ---------------------------------------------------------------------------
# Install: copy JS + WASM artefacts into the React public directory so the
# web worker can load them without an extra build step.
# ---------------------------------------------------------------------------
install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/scene_preview.js"
        "${CMAKE_CURRENT_BINARY_DIR}/scene_preview.wasm"
    DESTINATION "${REPO_ROOT}/react-web/public/wasm"
)
