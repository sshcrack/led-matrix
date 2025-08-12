# update_package_version.cmake
# Script to update React Native package.json version to match CMake project version

cmake_minimum_required(VERSION 3.5.0)

# Function to update package.json version
function(update_package_json_version PACKAGE_JSON_PATH PROJECT_VERSION)
    # Check if package.json exists
    if(NOT EXISTS "${PACKAGE_JSON_PATH}")
        message(FATAL_ERROR "package.json not found at: ${PACKAGE_JSON_PATH}")
    endif()

    # Read the current package.json file
    file(READ "${PACKAGE_JSON_PATH}" PACKAGE_JSON_CONTENT)

    # Create a backup of the original file
    # configure_file("${PACKAGE_JSON_PATH}" "${PACKAGE_JSON_PATH}.backup" COPYONLY)
    # message(STATUS "Created backup: ${PACKAGE_JSON_PATH}.backup")

    # Use regex to find and replace the version field
    # This pattern matches: "version": "any.version.here",
    string(REGEX REPLACE
        "\"version\"[ \t]*:[ \t]*\"[^\"]*\""
        "\"version\": \"${PROJECT_VERSION}\""
        UPDATED_CONTENT
        "${PACKAGE_JSON_CONTENT}"
    )

    # Check if the replacement was successful
    if("${UPDATED_CONTENT}" STREQUAL "${PACKAGE_JSON_CONTENT}")
        message(STATUS "No version field found or version was already ${PROJECT_VERSION} in ${PACKAGE_JSON_PATH}")
    else()
        # Write the updated content back to the file
        file(WRITE "${PACKAGE_JSON_PATH}" "${UPDATED_CONTENT}")
        message(STATUS "Updated package.json version to ${PROJECT_VERSION}")
    endif()
endfunction()

# Main execution when script is called directly
if(CMAKE_SCRIPT_MODE_FILE)
    # Validate required variables
    if(NOT DEFINED PACKAGE_JSON_PATH)
        message(FATAL_ERROR "PACKAGE_JSON_PATH variable must be defined")
    endif()

    if(NOT DEFINED PROJECT_VERSION)
        message(FATAL_ERROR "PROJECT_VERSION variable must be defined")
    endif()

    # Update the package.json version
    update_package_json_version("${PACKAGE_JSON_PATH}" "${PROJECT_VERSION}")
endif()

# Function to update package.json version during configure step
function(update_package_json_version_configure)
    # Parse arguments
    cmake_parse_arguments(
        ARGS
        "" # No options
        "PACKAGE_JSON_PATH" # Single value arguments
        "" # No multi-value arguments
        ${ARGN}
    )

    # Set default values
    if(NOT DEFINED ARGS_PACKAGE_JSON_PATH)
        set(ARGS_PACKAGE_JSON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/react-native/package.json")
    endif()

    # Convert relative path to absolute if needed
    if(NOT IS_ABSOLUTE "${ARGS_PACKAGE_JSON_PATH}")
        get_filename_component(ARGS_PACKAGE_JSON_PATH
            "${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_PACKAGE_JSON_PATH}"
            ABSOLUTE
        )
    endif()

    # Update package.json version immediately during configure
    update_package_json_version("${ARGS_PACKAGE_JSON_PATH}" "${PROJECT_VERSION}")
endfunction()

# Custom target to run this script (for build-time updates)
macro(add_package_json_version_update)
    # Parse arguments
    cmake_parse_arguments(
        ARGS
        "CONFIGURE_TIME" # Option to run during configure instead of build
        "PACKAGE_JSON_PATH;TARGET_NAME" # Single value arguments
        "" # No multi-value arguments
        ${ARGN}
    )

    # Set default values
    if(NOT DEFINED ARGS_PACKAGE_JSON_PATH)
        set(ARGS_PACKAGE_JSON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/react-native/package.json")
    endif()

    if(NOT DEFINED ARGS_TARGET_NAME)
        set(ARGS_TARGET_NAME "update_package_json_version")
    endif()

    # Convert relative path to absolute if needed
    if(NOT IS_ABSOLUTE "${ARGS_PACKAGE_JSON_PATH}")
        get_filename_component(ARGS_PACKAGE_JSON_PATH
            "${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_PACKAGE_JSON_PATH}"
            ABSOLUTE
        )
    endif()

    # If CONFIGURE_TIME is specified, update immediately during configure
    if(ARGS_CONFIGURE_TIME)
        update_package_json_version("${ARGS_PACKAGE_JSON_PATH}" "${PROJECT_VERSION}")
        message(STATUS "Updated package.json version to ${PROJECT_VERSION} at configure time: ${ARGS_PACKAGE_JSON_PATH}")
    else()
        # Add custom target to update package.json version during build
        add_custom_target(${ARGS_TARGET_NAME}
            COMMAND ${CMAKE_COMMAND}
            -DPACKAGE_JSON_PATH="${ARGS_PACKAGE_JSON_PATH}"
            -DPROJECT_VERSION="${PROJECT_VERSION}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/update_package_version.cmake"
            COMMENT "Updating package.json version to ${PROJECT_VERSION} at ${ARGS_PACKAGE_JSON_PATH}"
            VERBATIM
        )

        # Make React Native targets depend on version update
        if(TARGET react_native_install)
            add_dependencies(react_native_install ${ARGS_TARGET_NAME})
        endif()

        message(STATUS "Added package.json version update target '${ARGS_TARGET_NAME}' for ${ARGS_PACKAGE_JSON_PATH}")
    endif()
endmacro()

# Integration Instructions:
# 1. Save this script as cmake/update_package_version.cmake
# 2. In your main CMakeLists.txt, add this line after the project() declaration:
#    include(cmake/update_package_version.cmake)
# 3. Then call the macro where you handle React Native builds:
#
# Usage examples:
#
# CONFIGURE-TIME UPDATES (happens during cmake configure step):
# Default usage (uses ./react-native/package.json):
#    add_package_json_version_update(CONFIGURE_TIME)
#
# Custom package.json path at configure time:
#    add_package_json_version_update(CONFIGURE_TIME PACKAGE_JSON_PATH "frontend/package.json")
#
# Alternative function for configure-time only:
#    update_package_json_version_configure(PACKAGE_JSON_PATH "react-native/package.json")
#
# BUILD-TIME UPDATES (happens during build step - original behavior):
# Default usage (uses ./react-native/package.json):
#    add_package_json_version_update()
#
# Custom package.json path:
#    add_package_json_version_update(PACKAGE_JSON_PATH "frontend/package.json")
#
# Custom path with custom target name:
#    add_package_json_version_update(PACKAGE_JSON_PATH "web/package.json" TARGET_NAME "update_web_version")
#
# Multiple package.json files:
#    add_package_json_version_update(CONFIGURE_TIME PACKAGE_JSON_PATH "react-native/package.json")
#    add_package_json_version_update(CONFIGURE_TIME PACKAGE_JSON_PATH "web-app/package.json")
#
# RECOMMENDED FOR YOUR USE CASE:
# Since you want it to happen during configure step, use:
#    add_package_json_version_update(CONFIGURE_TIME PACKAGE_JSON_PATH "react-native/package.json")
#
# Manual usage:
# You can also run this script manually:
# cmake -DPACKAGE_JSON_PATH="./react-native/package.json" -DPROJECT_VERSION="1.11.0" -P cmake/update_package_version.cmake