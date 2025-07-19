
# Function to parse vcpkg.json and check if a feature exists
function(check_vcpkg_feature_exists FEATURE_NAME RESULT_VAR)
    set(VCPKG_JSON_PATH "${CMAKE_SOURCE_DIR}/vcpkg.json")
    
    if(NOT EXISTS "${VCPKG_JSON_PATH}")
        message(WARNING "vcpkg.json not found at ${VCPKG_JSON_PATH}")
        set(${RESULT_VAR} FALSE PARENT_SCOPE)
        return()
    endif()
    
    # Read vcpkg.json file
    file(READ "${VCPKG_JSON_PATH}" VCPKG_JSON_CONTENT)
    
    # Check if the feature exists in the features section
    # This is a simple string search - more robust JSON parsing could be added if needed
    string(FIND "${VCPKG_JSON_CONTENT}" "\"${FEATURE_NAME}\":" FEATURE_FOUND_POS)
    
    if(FEATURE_FOUND_POS GREATER_EQUAL 0)
        set(${RESULT_VAR} TRUE PARENT_SCOPE)
        message(STATUS "Found vcpkg feature: ${FEATURE_NAME}")
    else()
        set(${RESULT_VAR} FALSE PARENT_SCOPE)
    endif()
endfunction()