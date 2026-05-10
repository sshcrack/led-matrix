register_plugin(Shadertoy
        matrix/ShadertoyPlugin.cpp
        matrix/ShadertoyPlugin.h
        matrix/scenes/ShadertoyScene.cpp
        matrix/scenes/ShadertoyScene.h
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
