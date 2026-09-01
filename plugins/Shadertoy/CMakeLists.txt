register_plugin(Shadertoy
        matrix/ShadertoyPlugin.cpp
        matrix/ShadertoyPlugin.h
        scenes/ShadertoyScene.cpp
        scenes/ShadertoyScene.h
        scenes/ShaderMetadata.cpp
        scenes/ShaderMetadata.h
        matrix/providers/Random.cpp
        matrix/providers/Random.h
        matrix/providers/Collection.cpp
        matrix/providers/Collection.h
        matrix/scraper/Scraper.cpp
        matrix/scraper/Scraper.h
        DESKTOP
        desktop/ShadertoyDesktop.cpp
        desktop/ShadertoyDesktop.h
        desktop/CanvasPacket.cpp
        desktop/CanvasPacket.h
        desktop/ShaderCache.cpp
        desktop/ShaderCache.h
)

if(ENABLE_DESKTOP)
        find_package(GLEW REQUIRED)
        find_package(shadertoy-headless CONFIG REQUIRED)

        target_link_libraries(Shadertoy PRIVATE shadertoy_headless::shadertoy-headless)
        target_link_libraries(Shadertoy PRIVATE GLEW::GLEW)
else()
        find_package(cpr CONFIG REQUIRED)
        target_link_libraries(Shadertoy PRIVATE cpr::cpr)
endif()

if(TARGET ShadertoySceneWorker)
        find_package(cpr CONFIG REQUIRED)
        target_link_libraries(ShadertoySceneWorker PRIVATE cpr::cpr)
endif()

# Built-in one-file shaders are normal scenes on the matrix/emulator. Keeping
# them as data instead of C++ makes adding an AI-authored visual a one-file
# change while still packaging the exact source used in production.
if(NOT ENABLE_DESKTOP)
        if(ENABLE_EMULATOR)
                target_compile_definitions(Shadertoy PRIVATE
                        SHADERTOY_BUILTIN_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders")
        else()
                target_compile_definitions(Shadertoy PRIVATE
                        SHADERTOY_BUILTIN_SHADER_DIR="/usr/share/led-matrix/shaders")
        endif()
        install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/shaders/"
                DESTINATION share/led-matrix/shaders
                COMPONENT Shadertoy
                FILES_MATCHING PATTERN "*.frag")
endif()

# Worker builds do not execute these desktop-rendered scenes, but pointing the
# registry at source data keeps descriptor/smoke tooling useful in build trees.
if(TARGET ShadertoySceneWorker)
        target_compile_definitions(ShadertoySceneWorker PRIVATE
                SHADERTOY_BUILTIN_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders")
endif()

if(ENABLE_DESKTOP)
        # Compile/render harness for agents and regression checks. It uses the
        # exact vcpkg renderer consumed by the desktop plugin, so a PNG preview
        # validates both GLSL and the real integration rather than a web mock.
        add_executable(shadertoy_shader_preview
                ${CMAKE_SOURCE_DIR}/tests/shadertoy_shader_preview.cpp)
        target_compile_features(shadertoy_shader_preview PRIVATE cxx_std_23)
        find_package(glfw3 CONFIG REQUIRED)
        find_package(Stb REQUIRED)
        target_include_directories(shadertoy_shader_preview PRIVATE ${Stb_INCLUDE_DIR})
        target_link_libraries(shadertoy_shader_preview PRIVATE
                shadertoy_headless::shadertoy-headless
                GLEW::GLEW
                glfw)
        set_target_properties(shadertoy_shader_preview PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")

        if(WIN32)
                set(SHADERTOY_PREVIEW_RUNNER "")
        else()
                find_program(SHADERTOY_XVFB_RUN xvfb-run)
                if(SHADERTOY_XVFB_RUN)
                        set(SHADERTOY_PREVIEW_RUNNER "${SHADERTOY_XVFB_RUN};-a")
                endif()
        endif()

        if(WIN32 OR SHADERTOY_XVFB_RUN)
                file(GLOB BUILTIN_SHADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag")
                foreach(SHADER_FILE ${BUILTIN_SHADERS})
                        get_filename_component(SHADER_NAME "${SHADER_FILE}" NAME_WE)
                        add_test(NAME "shadertoy_shader_${SHADER_NAME}_smoke"
                                COMMAND ${SHADERTOY_PREVIEW_RUNNER}
                                        $<TARGET_FILE:shadertoy_shader_preview>
                                        "${SHADER_FILE}"
                                        "${CMAKE_BINARY_DIR}/shader-previews/${SHADER_NAME}.png"
                                        128 64 45)
                        if(SHADER_NAME MATCHES "^music_")
                                add_test(NAME "shadertoy_shader_${SHADER_NAME}_idle_smoke"
                                        COMMAND ${SHADERTOY_PREVIEW_RUNNER}
                                                $<TARGET_FILE:shadertoy_shader_preview>
                                                "${SHADER_FILE}"
                                                "${CMAKE_BINARY_DIR}/shader-previews/idle/${SHADER_NAME}.png"
                                                128 64 45 none)
                        endif()
                endforeach()

                # The tunnel previously had both an atan/FFT discontinuity and a
                # depth-wrap pop. A long no-audio run crosses multiple ring wraps;
                # fail if any adjacent frame changes enough to indicate a seam.
                set(TUNNEL_SHADER "${CMAKE_CURRENT_SOURCE_DIR}/shaders/music_spectrum_tunnel.frag")
                if(EXISTS "${TUNNEL_SHADER}")
                        add_test(NAME shadertoy_shader_music_spectrum_tunnel_temporal_smoke
                                COMMAND ${SHADERTOY_PREVIEW_RUNNER}
                                        $<TARGET_FILE:shadertoy_shader_preview>
                                        "${TUNNEL_SHADER}"
                                        "${CMAKE_BINARY_DIR}/shader-previews/temporal/music_spectrum_tunnel.png"
                                        128 64 180 none 0.012)
                endif()
        endif()
endif()
