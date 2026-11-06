# Script mode: cmake -DSOURCE_DIR=<repo> -DOUTPUT=<header> -P SceneCodeDigest.cmake
#
# Fingerprint of all code that decides what a scene draws (plugins/ + shared/).
# The Pi only offloads a scene to a desktop worker that reports the same
# fingerprint, so a stale desktop install can never stream an old version of a
# scene. Git tree hashes are platform independent (Windows CRLF checkouts hash
# the same), and uncommitted edits add a hash of the diff and untracked files.
# Runs on every build; the header is only rewritten when the value changes.
set(paths plugins shared)
execute_process(COMMAND git rev-parse HEAD:plugins HEAD:shared
    WORKING_DIRECTORY "${SOURCE_DIR}" OUTPUT_VARIABLE trees RESULT_VARIABLE rc ERROR_QUIET)
if(NOT rc EQUAL 0 OR trees STREQUAL "")
    set(digest "unknown")
else()
    string(SHA1 digest "${trees}")
    string(SUBSTRING "${digest}" 0 16 digest)
    execute_process(COMMAND git status --porcelain --untracked-files=all -- ${paths}
        WORKING_DIRECTORY "${SOURCE_DIR}" OUTPUT_VARIABLE status ERROR_QUIET)
    if(NOT status STREQUAL "")
        execute_process(COMMAND git diff HEAD -- ${paths}
            WORKING_DIRECTORY "${SOURCE_DIR}" OUTPUT_VARIABLE diff ERROR_QUIET)
        execute_process(COMMAND git ls-files --others --exclude-standard -- ${paths}
            WORKING_DIRECTORY "${SOURCE_DIR}" OUTPUT_VARIABLE untracked ERROR_QUIET)
        string(REPLACE "\n" ";" untracked "${untracked}")
        set(extra "")
        foreach(file IN LISTS untracked)
            if(file AND EXISTS "${SOURCE_DIR}/${file}")
                file(SHA1 "${SOURCE_DIR}/${file}" h)
                string(APPEND extra "${file}:${h}\n")
            endif()
        endforeach()
        string(SHA1 dirty "${diff}${extra}")
        string(SUBSTRING "${dirty}" 0 8 dirty)
        set(digest "${digest}-dirty-${dirty}")
    endif()
endif()
set(content "#pragma once\n#define LED_MATRIX_SCENE_CODE_DIGEST \"${digest}\"\n")
if(EXISTS "${OUTPUT}")
    file(READ "${OUTPUT}" old)
endif()
if(NOT old STREQUAL content)
    file(WRITE "${OUTPUT}" "${content}")
endif()
