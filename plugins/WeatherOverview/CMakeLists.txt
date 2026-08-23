register_plugin(WeatherOverview
    matrix/WeatherOverview.cpp
    matrix/WeatherOverview.h
    matrix/WeatherParser.cpp
    matrix/WeatherParser.h
    matrix/Constants.cpp
    matrix/Constants.h
    scenes/WeatherScene.cpp
    scenes/WeatherScene.h
    scenes/WeatherAmbienceScene.cpp
    scenes/WeatherAmbienceScene.h
    scenes/weather_rendering.cpp
    scenes/weather_particles.cpp
    scenes/weather_cache.cpp
)

if(NOT ENABLE_DESKTOP)
    # HTTP Requests

    find_package(cpr CONFIG REQUIRED)
    target_link_libraries(WeatherOverview PRIVATE cpr::cpr)


    # Install fonts to fonts directory
    install(FILES
        "${RPI_RGB_LED_MATRIX_FONTS_DIR}/7x13.bdf"
        "${RPI_RGB_LED_MATRIX_FONTS_DIR}/5x8.bdf"
        "${RPI_RGB_LED_MATRIX_FONTS_DIR}/4x6.bdf"
        DESTINATION "lib/led-matrix/plugins/WeatherOverview/fonts"
    )
elseif(ENABLE_SCENE_WORKER)
    install(FILES
        "${RPI_RGB_LED_MATRIX_FONTS_DIR}/7x13.bdf"
        "${RPI_RGB_LED_MATRIX_FONTS_DIR}/5x8.bdf"
        "${RPI_RGB_LED_MATRIX_FONTS_DIR}/4x6.bdf"
        DESTINATION "lib/led-matrix-desktop/scene-plugins/WeatherOverview/fonts"
    )
endif()

if(TARGET WeatherOverviewSceneWorker)
    find_package(cpr CONFIG REQUIRED)
    target_link_libraries(WeatherOverviewSceneWorker PRIVATE cpr::cpr)
    # Keep worker builds runnable directly from the build tree, not only after
    # installation. Weather scenes resolve fonts relative to their plugin DSO.
    add_custom_command(TARGET WeatherOverviewSceneWorker POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:WeatherOverviewSceneWorker>/fonts"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RPI_RGB_LED_MATRIX_FONTS_DIR}/7x13.bdf"
            "${RPI_RGB_LED_MATRIX_FONTS_DIR}/5x8.bdf"
            "${RPI_RGB_LED_MATRIX_FONTS_DIR}/4x6.bdf"
            "$<TARGET_FILE_DIR:WeatherOverviewSceneWorker>/fonts"
    )
endif()
