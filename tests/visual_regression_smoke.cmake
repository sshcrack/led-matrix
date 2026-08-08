if(NOT DEFINED PREVIEW_GEN OR NOT DEFINED OUTPUT_DIR OR NOT DEFINED BASELINE)
    message(FATAL_ERROR "PREVIEW_GEN, OUTPUT_DIR and BASELINE are required")
endif()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(METRICS "${OUTPUT_DIR}/metrics.json")
set(SCENES "audio_spectrum,audio_pulse_tunnel,audio_aurora,audio_kaleidoscope,wave_pattern")

execute_process(
    COMMAND "${PREVIEW_GEN}"
        --output "${OUTPUT_DIR}"
        --scenes "${SCENES}"
        --frames 24
        --fps 24
        --virtual-time-only
        --strict
        --metrics-out "${METRICS}"
    RESULT_VARIABLE PREVIEW_RESULT
    OUTPUT_VARIABLE PREVIEW_STDOUT
    ERROR_VARIABLE PREVIEW_STDERR
)
if(NOT PREVIEW_RESULT EQUAL 0)
    message(FATAL_ERROR "preview_gen failed (${PREVIEW_RESULT})\n${PREVIEW_STDOUT}\n${PREVIEW_STDERR}")
endif()

file(READ "${METRICS}" ACTUAL_JSON)
file(READ "${BASELINE}" BASELINE_JSON)
set(SCENE_NAMES audio_spectrum audio_pulse_tunnel audio_aurora audio_kaleidoscope wave_pattern)
set(METRIC_NAMES lit_fraction_average mean_luma_average temporal_change_average)

foreach(SCENE IN LISTS SCENE_NAMES)
    string(JSON FRAME_COUNT ERROR_VARIABLE FRAME_ERROR GET "${ACTUAL_JSON}" "${SCENE}" "frames")
    if(FRAME_ERROR OR NOT FRAME_COUNT EQUAL 24)
        message(FATAL_ERROR "${SCENE}: expected 24 captured frames, got '${FRAME_COUNT}' (${FRAME_ERROR})")
    endif()

    foreach(METRIC IN LISTS METRIC_NAMES)
        string(JSON ACTUAL ERROR_VARIABLE ACTUAL_ERROR GET "${ACTUAL_JSON}" "${SCENE}" "${METRIC}")
        string(JSON MINIMUM ERROR_VARIABLE MIN_ERROR GET "${BASELINE_JSON}" "${SCENE}" "${METRIC}" "min")
        string(JSON MAXIMUM ERROR_VARIABLE MAX_ERROR GET "${BASELINE_JSON}" "${SCENE}" "${METRIC}" "max")
        if(ACTUAL_ERROR OR MIN_ERROR OR MAX_ERROR)
            message(FATAL_ERROR "Missing visual metric ${SCENE}.${METRIC}")
        endif()
        if(ACTUAL LESS MINIMUM OR ACTUAL GREATER MAXIMUM)
            message(FATAL_ERROR
                "Visual regression: ${SCENE}.${METRIC}=${ACTUAL}, expected ${MINIMUM}..${MAXIMUM}")
        endif()
    endforeach()
endforeach()

message(STATUS "Visual regression metrics are within baseline ranges")
