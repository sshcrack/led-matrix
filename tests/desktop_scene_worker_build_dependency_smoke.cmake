if(NOT DEFINED BUILD_DIR OR NOT DEFINED WORKER_FILE)
    message(FATAL_ERROR "BUILD_DIR and WORKER_FILE are required")
endif()

if(NOT EXISTS "${WORKER_FILE}")
    message(FATAL_ERROR "scene worker must exist before dependency smoke test: ${WORKER_FILE}")
endif()

set(BACKUP_FILE "${WORKER_FILE}.dependency-smoke-backup")
file(REMOVE "${BACKUP_FILE}")
file(RENAME "${WORKER_FILE}" "${BACKUP_FILE}")

set(BUILD_COMMAND "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --target led-matrix-desktop)
if(DEFINED CONFIG AND NOT CONFIG STREQUAL "")
    list(APPEND BUILD_COMMAND --config "${CONFIG}")
endif()

execute_process(
    COMMAND ${BUILD_COMMAND}
    RESULT_VARIABLE BUILD_RESULT
    OUTPUT_VARIABLE BUILD_STDOUT
    ERROR_VARIABLE BUILD_STDERR
)

if(NOT EXISTS "${WORKER_FILE}")
    file(RENAME "${BACKUP_FILE}" "${WORKER_FILE}")
    message(FATAL_ERROR
        "building led-matrix-desktop did not reproduce led-matrix-scene-worker\n"
        "build exit code: ${BUILD_RESULT}\nstdout:\n${BUILD_STDOUT}\nstderr:\n${BUILD_STDERR}")
endif()

file(REMOVE "${BACKUP_FILE}")

if(NOT BUILD_RESULT EQUAL 0)
    message(FATAL_ERROR
        "desktop targeted build failed while checking scene-worker dependency\n"
        "stdout:\n${BUILD_STDOUT}\nstderr:\n${BUILD_STDERR}")
endif()

message(STATUS "desktop target rebuilds its required scene worker")
