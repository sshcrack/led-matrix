if(NOT DEFINED PROBE OR NOT DEFINED CRASH_DIR OR NOT DEFINED MODE)
    message(FATAL_ERROR "PROBE, CRASH_DIR and MODE are required")
endif()

file(REMOVE_RECURSE "${CRASH_DIR}")
file(MAKE_DIRECTORY "${CRASH_DIR}")

execute_process(
    COMMAND "${PROBE}" "${CRASH_DIR}" "${MODE}"
    RESULT_VARIABLE PROBE_RESULT
    OUTPUT_VARIABLE PROBE_STDOUT
    ERROR_VARIABLE PROBE_STDERR
)

if(PROBE_RESULT EQUAL 0)
    message(FATAL_ERROR "crash probe unexpectedly exited successfully")
endif()

file(GLOB REPORTS "${CRASH_DIR}/crash-crash-reporter-probe-*.txt")
list(LENGTH REPORTS REPORT_COUNT)
if(NOT REPORT_COUNT EQUAL 1)
    message(FATAL_ERROR
        "expected exactly one finalized crash report, found ${REPORT_COUNT}\n"
        "probe result=${PROBE_RESULT}\nstdout=${PROBE_STDOUT}\nstderr=${PROBE_STDERR}")
endif()
list(GET REPORTS 0 REPORT)
file(READ "${REPORT}" CONTENT)

foreach(REQUIRED_TEXT
        "LED Matrix crash report"
        "process: crash-reporter-probe"
        "git_revision:"
        "activity: rendering scene 'crash_probe'"
        "breadcrumb immediately before intentional crash"
        "stack_trace:")
    string(FIND "${CONTENT}" "${REQUIRED_TEXT}" POSITION)
    if(POSITION EQUAL -1)
        message(FATAL_ERROR "crash report is missing '${REQUIRED_TEXT}':\n${CONTENT}")
    endif()
endforeach()

if(MODE STREQUAL "exception")
    string(FIND "${CONTENT}" "reason: caught_top_level_exception" POSITION)
    if(POSITION EQUAL -1)
        message(FATAL_ERROR "caught exception did not use the explicit report path:\n${CONTENT}")
    endif()
    string(FIND "${CONTENT}" "synthetic failure" POSITION)
    if(POSITION EQUAL -1)
        message(FATAL_ERROR "caught exception message is missing from report:\n${CONTENT}")
    endif()
endif()

if(MODE STREQUAL "segfault")
    if(WIN32_TEST)
        string(FIND "${CONTENT}" "reason: unhandled_windows_exception" POSITION)
        if(POSITION EQUAL -1)
            message(FATAL_ERROR "Windows access violation did not use the SEH crash path:\n${CONTENT}")
        endif()
    else()
        string(FIND "${CONTENT}" "reason: fatal_posix_signal" POSITION)
        if(POSITION EQUAL -1)
            message(FATAL_ERROR "POSIX segfault did not use the fatal signal path:\n${CONTENT}")
        endif()
        string(FIND "${CONTENT}" "SIGSEGV" POSITION)
        if(POSITION EQUAL -1)
            message(FATAL_ERROR "POSIX report does not identify SIGSEGV:\n${CONTENT}")
        endif()
        string(FIND "${CONTENT}" "process_memory_map:" POSITION)
        if(POSITION EQUAL -1)
            message(FATAL_ERROR "POSIX report does not include /proc/self/maps:\n${CONTENT}")
        endif()
        string(FIND "${CONTENT}" "thread_id:" POSITION)
        if(POSITION EQUAL -1)
            message(FATAL_ERROR "POSIX report does not include the crashing thread id:\n${CONTENT}")
        endif()
        string(FIND "${CONTENT}" "instruction_pointer:" POSITION)
        if(POSITION EQUAL -1)
            message(FATAL_ERROR "POSIX report does not include the fault instruction pointer:\n${CONTENT}")
        endif()
    endif()
endif()

if(WIN32_TEST)
    file(GLOB DUMPS "${CRASH_DIR}/crash-crash-reporter-probe-*.dmp")
    list(LENGTH DUMPS DUMP_COUNT)
    if(NOT DUMP_COUNT EQUAL 1)
        message(FATAL_ERROR "expected one Windows minidump, found ${DUMP_COUNT}")
    endif()
    list(GET DUMPS 0 DUMP)
    file(SIZE "${DUMP}" DUMP_SIZE)
    if(DUMP_SIZE LESS 4096)
        message(FATAL_ERROR "Windows minidump is unexpectedly small (${DUMP_SIZE} bytes): ${DUMP}")
    endif()
    string(FIND "${CONTENT}" "minidump: created alongside this report" POSITION)
    if(POSITION EQUAL -1)
        message(FATAL_ERROR "Windows report does not confirm minidump creation:\n${CONTENT}")
    endif()
endif()

file(GLOB PENDING "${CRASH_DIR}/.*.pending")
if(PENDING)
    message(FATAL_ERROR "pending crash artifacts were not finalized: ${PENDING}")
endif()

message(STATUS "validated ${MODE} crash report: ${REPORT}")
