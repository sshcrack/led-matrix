cmake_minimum_required(VERSION 3.5.0)
project(SharedToolsDesktop)

set(CMAKE_CXX_STANDARD 23)
add_library(${PROJECT_NAME} SHARED
        src/shared/desktop/plugin_loader/loader.cpp
        src/shared/desktop/config.cpp
        src/shared/desktop/autostart.cpp
        src/shared/desktop/utils.cpp
        src/shared/desktop/glfw.cpp
        src/shared/desktop/WebsocketClient.cpp
        src/shared/desktop/UdpSender.cpp
        src/shared/desktop/UpdateChecker.cpp
        src/shared/desktop/UpdateManager.cpp
)
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(${PROJECT_NAME} PRIVATE SHARED_DESKTOP_EXPORTS)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_link_libraries(${PROJECT_NAME} PRIVATE SharedToolsCommon)

find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)

find_package(ixwebsocket CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC ixwebsocket::ixwebsocket)

find_package(cpr CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE cpr::cpr)

if (WIN32)
    target_link_libraries(${PROJECT_NAME} PUBLIC bcrypt)
endif()

find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(hello-imgui CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE hello-imgui::hello_imgui nlohmann_json::nlohmann_json)
target_include_directories(${PROJECT_NAME}
        PRIVATE
        # where the library itself will look for its internal headers
        src
        PUBLIC
        # where top-level project will look for the library's public headers
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        # where external projects will look for the library's public headers
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION "bin"
)

if(WIN32)
        # Look for FFTW3 DLL in vcpkg bin directory
        set(VCPKG_BIN_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin")
        file(GLOB CPR_DLLS "${VCPKG_BIN_DIR}/cpr.dll")

        if(CPR_DLLS)
                install(FILES ${CPR_DLLS}
                        DESTINATION bin
                )
        else()
                message(FATAL_ERROR "No CPR DLLs found in ${VCPKG_BIN_DIR}.")
        endif()

                file(GLOB LIBCURL_DLLS "${VCPKG_BIN_DIR}/libcurl.dll")

        if(LIBCURL_DLLS)
                install(FILES ${LIBCURL_DLLS}
                        DESTINATION bin
                )
        else()
                message(FATAL_ERROR "No libcurl DLLs found in ${VCPKG_BIN_DIR}.")
        endif()
endif()