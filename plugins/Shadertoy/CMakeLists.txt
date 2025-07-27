register_plugin(Shadertoy
        matrix/ShadertoyPlugin.cpp
        matrix/ShadertoyPlugin.h
        matrix/scenes/ShadertoyScene.cpp
        matrix/scenes/ShadertoyScene.h
        matrix/scraper/Scraper.cpp
        matrix/scraper/Scraper.h
        DESKTOP
        desktop/ShadertoyDesktop.cpp
        desktop/ShadertoyDesktop.h
        desktop/CanvasPacket.cpp
        desktop/CanvasPacket.h
)

if (ENABLE_DESKTOP)
    find_package(GLEW REQUIRED)
    add_subdirectory(thirdparty/shadertoy-headless EXCLUDE_FROM_ALL)

    target_link_libraries(Shadertoy PRIVATE shadertoy-headless)
    target_link_libraries(Shadertoy PRIVATE GLEW::GLEW)

    if(WIN32)
        set(VCPKG_BIN_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin")
        file(GLOB GLEW_DLLS "${VCPKG_BIN_DIR}/glew*.dll")

        if(GLEW_DLLS)
                install(FILES ${GLEW_DLLS}
                        DESTINATION plugins/Shadertoy
                        COMPONENT Shadertoy
                )
        else()
            message(WARNING "No GLEW DLLs found in ${VCPKG_BIN_DIR}. The plugin might not load")
        endif()

        file(GLOB BROTLI_DLLS "${VCPKG_BIN_DIR}/brotli*.dll")

        if(BROTLI_DLLS)
                install(FILES ${BROTLI_DLLS}
                        DESTINATION plugins/Shadertoy
                        COMPONENT Shadertoy
                )
        else()
            message(WARNING "No Brotli DLLs found in ${VCPKG_BIN_DIR}. The plugin might not load")
        endif()
    endif()
else()
    find_package(cpr CONFIG REQUIRED)
    target_link_libraries(Shadertoy PRIVATE cpr::cpr)
endif ()