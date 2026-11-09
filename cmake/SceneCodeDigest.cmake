# Script mode: cmake -DSOURCE_DIR=<repo> -DOUTPUT=<header> -P SceneCodeDigest.cmake
#
# A portable source-compatibility identity, not a binary hash or a guarantee of
# pixel-identical floating-point results across architectures. Hash the actual
# files, independent of Git history, staging, checkout location or build flavor.
# Keep this input policy in sync with docs/SCENE_EXECUTION.md and its tests.
cmake_minimum_required(VERSION 3.21)
if(NOT IS_DIRECTORY "${SOURCE_DIR}" OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "SceneCodeDigest requires SOURCE_DIR and OUTPUT")
endif()

set(files)
foreach(root plugins shared/common shared/matrix shared/fonts src_scene_worker cmake)
    file(GLOB_RECURSE entries LIST_DIRECTORIES false RELATIVE "${SOURCE_DIR}"
        "${SOURCE_DIR}/${root}/*")
    list(APPEND files ${entries})
endforeach()
foreach(path CMakeLists.txt shared/CMakeLists.txt vcpkg.json vcpkg-configuration.json)
    if(EXISTS "${SOURCE_DIR}/${path}")
        list(APPEND files "${path}")
    endif()
endforeach()
list(REMOVE_DUPLICATES files)
list(SORT files)

# Version the recipe so fingerprints from the old Git-tree scheme cannot match.
set(manifest "led-matrix-scene-source-v2\n")
set(count 0)
foreach(path IN LISTS files)
    # UI, documentation, reference media and local build/editor products cannot
    # change the matrix renderer. Vendor source trees are traversed as ordinary
    # files, including local edits inside submodules (but never their Git data).
    if(path MATCHES "(^|/)(desktop|web|docs|img|tests|\\.git|\\.github|__pycache__|node_modules|CMakeFiles|vcpkg_installed|build|install)(/|$)"
       OR path MATCHES "(^|/)(cmake-build-[^/]*|[^/]*_build[^/]*)(/|$)"
       OR path MATCHES "(^|/)(README[^/]*|LICENSE[^/]*|AUTHORS|COPYING[^/]*|\\.[^/]*)$"
       OR path MATCHES "\\.(md|dox|pdf|pyc|o|obj|so|dll|a|lib|swp|swo|tmp|bak)$")
        continue()
    endif()
    # Git's text heuristic: a NUL in the first 8 KiB marks binary data. Inspect
    # hex byte pairs because CMake strings cannot reliably process NUL bytes.
    # Known resource formats stay byte-exact even for tiny NUL-free fixtures.
    file(READ "${SOURCE_DIR}/${path}" probe LIMIT 8192 HEX)
    string(REGEX REPLACE "(..)" "\\1;" probe "${probe}")
    string(FIND ";${probe}" ";00;" nul)
    if(nul EQUAL -1 AND NOT path MATCHES "\\.(png|jpg|jpeg|gif|webp|ico|bmp|wav|mp3|ogg|flac|mp4|ttf|otf|woff|woff2|bin|dat)$")
        file(READ "${SOURCE_DIR}/${path}" content)
        string(REPLACE "\r\n" "\n" content "${content}")
        string(SHA256 hash "${content}")
    else()
        file(SHA256 "${SOURCE_DIR}/${path}" hash)
    endif()
    # Include the path as well as the content: renames/deletions matter too.
    string(APPEND manifest "${path}:${hash}\n")
    math(EXPR count "${count} + 1")
endforeach()
if(count EQUAL 0)
    message(FATAL_ERROR "No scene fingerprint inputs found in ${SOURCE_DIR}")
endif()
string(SHA256 digest "${manifest}")
set(content "#pragma once\n#define LED_MATRIX_SCENE_CODE_DIGEST \"v2-sha256-${digest}\"\n")
if(EXISTS "${OUTPUT}")
    file(READ "${OUTPUT}" old)
endif()
if(NOT old STREQUAL content)
    file(WRITE "${OUTPUT}" "${content}")
endif()
