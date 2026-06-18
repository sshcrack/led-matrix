
# Function to parse vcpkg.json and check if a feature exists
function(check_vcpkg_feature_exists FEATURE_NAME RESULT_VAR)
    set(VCPKG_JSON_PATH "${CMAKE_SOURCE_DIR}/vcpkg.json")
    
    if(NOT EXISTS "${VCPKG_JSON_PATH}")
        message(WARNING "vcpkg.json not found at ${VCPKG_JSON_PATH}")
        set(${RESULT_VAR} FALSE PARENT_SCOPE)
        return()
    endif()
    
    file(READ "${VCPKG_JSON_PATH}" VCPKG_JSON_CONTENT)
    
    set(${RESULT_VAR} FALSE)
    
    string(JSON FEATURE_COUNT ERROR_VARIABLE FEATURES_ERROR LENGTH "${VCPKG_JSON_CONTENT}" "features")
    if(FEATURES_ERROR)
        set(${RESULT_VAR} FALSE PARENT_SCOPE)
        return()
    endif()
    
    if(FEATURE_COUNT GREATER 0)
        math(EXPR MAX_IDX "${FEATURE_COUNT} - 1")
        foreach(IDX RANGE ${MAX_IDX})
        string(JSON FEATURE_KEY MEMBER "${VCPKG_JSON_CONTENT}" "features" ${IDX})
        if(FEATURE_KEY STREQUAL "${FEATURE_NAME}")
            set(${RESULT_VAR} TRUE)
            break()
        endif()
        endforeach()
    endif()
    
    if(${RESULT_VAR})
        message(STATUS "Found vcpkg feature: ${FEATURE_NAME}")
    endif()
endfunction()
