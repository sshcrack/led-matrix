# incremental_preview.cmake
# Invoked by the generate_scene_previews_incremental custom target.
#
# Expected input variables (set via -D on the cmake command line):
#   PREVIEW_GEN         - absolute path to the preview_gen binary
#   PLUGIN_DIR          - directory containing built plugin subdirectories
#   LIBRARY_PATH        - value for LD_LIBRARY_PATH
#   OUTPUT_DIR          - staging directory for generated GIFs
#   MANIFEST_FILE       - path to the persisted fingerprint manifest JSON
#   FPS                 - frames per second for preview GIFs
#   FRAMES              - total frames per GIF
#   WIDTH               - matrix pixel width
#   HEIGHT              - matrix pixel height
#   GENERATOR_VERSION   - project version string (used to detect generator changes)
#   SCENES_OVERRIDE     - optional comma-separated list of scene names to force-regenerate

cmake_minimum_required(VERSION 3.14)

# ---------------------------------------------------------------------------
# Helper: stat a file returning mtime;size or "0;0" if missing
# ---------------------------------------------------------------------------
function(file_stat PATH OUT_MTIME OUT_SIZE)
    if(EXISTS "${PATH}")
        file(TIMESTAMP "${PATH}" _mtime "%s" UTC)
        file(SIZE "${PATH}" _size)
        set(${OUT_MTIME} "${_mtime}" PARENT_SCOPE)
        set(${OUT_SIZE} "${_size}" PARENT_SCOPE)
    else()
        set(${OUT_MTIME} "0" PARENT_SCOPE)
        set(${OUT_SIZE} "0" PARENT_SCOPE)
    endif()
endfunction()

# ---------------------------------------------------------------------------
# 1. Ensure output directory exists
# ---------------------------------------------------------------------------
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

# ---------------------------------------------------------------------------
# 2. Run preview_gen --dump-manifest to learn scene->plugin mapping
# ---------------------------------------------------------------------------
set(_scene_manifest_file "${OUTPUT_DIR}/scene_manifest.json")

execute_process(
    COMMAND ${CMAKE_COMMAND} -E env
        "LD_LIBRARY_PATH=${LIBRARY_PATH}"
        "PLUGIN_DIR=${PLUGIN_DIR}"
        "${PREVIEW_GEN}"
        --dump-manifest
        --manifest-out "${_scene_manifest_file}"
    RESULT_VARIABLE _dump_result
    OUTPUT_QUIET
    ERROR_QUIET
)

if(NOT _dump_result EQUAL 0)
    message(WARNING
        "preview_gen --dump-manifest failed (exit code ${_dump_result}). "
        "Falling back to full regeneration.")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env
            "LD_LIBRARY_PATH=${LIBRARY_PATH}"
            "PLUGIN_DIR=${PLUGIN_DIR}"
            "${PREVIEW_GEN}"
            --output "${OUTPUT_DIR}"
            --fps "${FPS}"
            --frames "${FRAMES}"
            --width "${WIDTH}"
            --height "${HEIGHT}"
        RESULT_VARIABLE _gen_result
    )
    if(NOT _gen_result EQUAL 0)
        message(WARNING "Full preview generation exited with code ${_gen_result}")
    endif()
    return()
endif()

# ---------------------------------------------------------------------------
# 3. Read the stored fingerprint manifest (if any)
# ---------------------------------------------------------------------------
set(_stored_manifest "")
if(EXISTS "${MANIFEST_FILE}")
    file(READ "${MANIFEST_FILE}" _stored_manifest)
endif()

# string(JSON ...) requires CMake 3.19+. Fall back to full regen if older.
if(CMAKE_VERSION VERSION_LESS "3.19")
    set(_stored_manifest "")
    message(STATUS "CMake < 3.19: running full preview generation.")
endif()

# ---------------------------------------------------------------------------
# 4. Parse scene_manifest.json to build list of scenes needing regeneration
# ---------------------------------------------------------------------------
file(READ "${_scene_manifest_file}" _scene_manifest_content)

string(JSON _scene_count LENGTH "${_scene_manifest_content}")
if(_scene_count EQUAL 0)
    message(STATUS "No scenes found in manifest; nothing to do.")
    return()
endif()

set(_scenes_to_regenerate "")
math(EXPR _last_idx "${_scene_count} - 1")

foreach(_i RANGE 0 ${_last_idx})
    string(JSON _entry GET "${_scene_manifest_content}" ${_i})
    string(JSON _scene_name  GET "${_entry}" "name")
    string(JSON _plugin_path GET "${_entry}" "plugin_path")

    # Skip scenes that require the desktop app — they need manual capture
    string(JSON _needs_desktop ERROR_VARIABLE _nd_err GET "${_entry}" "needs_desktop")
    if(NOT _nd_err AND "${_needs_desktop}" STREQUAL "true")
        continue()
    endif()

    # Honour explicit override list
    if(NOT "${SCENES_OVERRIDE}" STREQUAL "")
        string(REPLACE "," ";" _override_list "${SCENES_OVERRIDE}")
        if("${_scene_name}" IN_LIST _override_list)
            list(APPEND _scenes_to_regenerate "${_scene_name}")
        endif()
        continue()
    endif()

    set(_gif_path "${OUTPUT_DIR}/${_scene_name}.gif")
    file_stat("${_plugin_path}" _cur_mtime _cur_size)

    set(_needs_regen TRUE)
    if(NOT "${_stored_manifest}" STREQUAL "" AND EXISTS "${_gif_path}")
        string(JSON _stored_scene_obj ERROR_VARIABLE _json_err
            GET "${_stored_manifest}" "scenes" "${_scene_name}")

        if(NOT _json_err)
            string(JSON _s_mtime  ERROR_VARIABLE _e GET "${_stored_scene_obj}" "plugin_mtime")
            string(JSON _s_size   ERROR_VARIABLE _e GET "${_stored_scene_obj}" "plugin_size")
            string(JSON _s_fps    ERROR_VARIABLE _e GET "${_stored_scene_obj}" "fps")
            string(JSON _s_frames ERROR_VARIABLE _e GET "${_stored_scene_obj}" "frames")
            string(JSON _s_w      ERROR_VARIABLE _e GET "${_stored_scene_obj}" "width")
            string(JSON _s_h      ERROR_VARIABLE _e GET "${_stored_scene_obj}" "height")
            string(JSON _s_ver    ERROR_VARIABLE _e GET "${_stored_scene_obj}" "generator_version")

            if("${_s_mtime}"  STREQUAL "${_cur_mtime}"  AND
               "${_s_size}"   STREQUAL "${_cur_size}"   AND
               "${_s_fps}"    STREQUAL "${FPS}"         AND
               "${_s_frames}" STREQUAL "${FRAMES}"      AND
               "${_s_w}"      STREQUAL "${WIDTH}"       AND
               "${_s_h}"      STREQUAL "${HEIGHT}"      AND
               "${_s_ver}"    STREQUAL "${GENERATOR_VERSION}")
                set(_needs_regen FALSE)
            endif()
        endif()
    endif()

    if(_needs_regen)
        list(APPEND _scenes_to_regenerate "${_scene_name}")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# 5. Generate only the scenes that need regeneration
# ---------------------------------------------------------------------------
list(LENGTH _scenes_to_regenerate _regen_count)
if(_regen_count EQUAL 0)
    message(STATUS "All scene previews are up-to-date; nothing to regenerate.")
    return()
endif()

message(STATUS "Regenerating ${_regen_count} scene preview(s): ${_scenes_to_regenerate}")

string(REPLACE ";" "," _scenes_csv "${_scenes_to_regenerate}")

execute_process(
    COMMAND ${CMAKE_COMMAND} -E env
        "LD_LIBRARY_PATH=${LIBRARY_PATH}"
        "PLUGIN_DIR=${PLUGIN_DIR}"
        "${PREVIEW_GEN}"
        --output "${OUTPUT_DIR}"
        --fps "${FPS}"
        --frames "${FRAMES}"
        --width "${WIDTH}"
        --height "${HEIGHT}"
        --scenes "${_scenes_csv}"
    RESULT_VARIABLE _gen_result
)

if(NOT _gen_result EQUAL 0)
    message(WARNING
        "Incremental preview generation exited with code ${_gen_result}; "
        "some previews may be stale but existing previews remain usable.")
endif()

# ---------------------------------------------------------------------------
# 6. Update fingerprint manifest for successfully generated scenes
# ---------------------------------------------------------------------------
set(_new_manifest "{}")
if(EXISTS "${MANIFEST_FILE}")
    file(READ "${MANIFEST_FILE}" _new_manifest)
    string(JSON _scenes_type ERROR_VARIABLE _je TYPE "${_new_manifest}" "scenes")
    if(_je OR "${_scenes_type}" STREQUAL "NULL")
        string(JSON _new_manifest SET "${_new_manifest}" "scenes" "{}")
    endif()
else()
    string(JSON _new_manifest SET "{}" "scenes" "{}")
endif()

string(JSON _new_manifest SET "${_new_manifest}" "generator_version" "\"${GENERATOR_VERSION}\"")

foreach(_i RANGE 0 ${_last_idx})
    string(JSON _entry GET "${_scene_manifest_content}" ${_i})
    string(JSON _scene_name  GET "${_entry}" "name")
    string(JSON _plugin_path GET "${_entry}" "plugin_path")

    if(NOT "${_scene_name}" IN_LIST _scenes_to_regenerate)
        continue()
    endif()

    set(_gif_path "${OUTPUT_DIR}/${_scene_name}.gif")
    if(NOT EXISTS "${_gif_path}")
        continue() # Generation failed; don't update fingerprint
    endif()

    file_stat("${_plugin_path}" _cur_mtime _cur_size)

    set(_scene_obj "{}")
    string(JSON _scene_obj SET "${_scene_obj}" "plugin_path"       "\"${_plugin_path}\"")
    string(JSON _scene_obj SET "${_scene_obj}" "plugin_mtime"      "\"${_cur_mtime}\"")
    string(JSON _scene_obj SET "${_scene_obj}" "plugin_size"       "\"${_cur_size}\"")
    string(JSON _scene_obj SET "${_scene_obj}" "fps"               "\"${FPS}\"")
    string(JSON _scene_obj SET "${_scene_obj}" "frames"            "\"${FRAMES}\"")
    string(JSON _scene_obj SET "${_scene_obj}" "width"             "\"${WIDTH}\"")
    string(JSON _scene_obj SET "${_scene_obj}" "height"            "\"${HEIGHT}\"")
    string(JSON _scene_obj SET "${_scene_obj}" "generator_version" "\"${GENERATOR_VERSION}\"")

    string(JSON _new_manifest SET "${_new_manifest}" "scenes" "${_scene_name}" "${_scene_obj}")
endforeach()

file(WRITE "${MANIFEST_FILE}" "${_new_manifest}")
message(STATUS "Fingerprint manifest updated: ${MANIFEST_FILE}")
