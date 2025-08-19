register_plugin(WeatherOverview
    matrix/WeatherOverview.cpp
    matrix/WeatherOverview.h
    matrix/WeatherParser.cpp
    matrix/WeatherParser.h
    matrix/Constants.cpp
    matrix/Constants.h
    matrix/scenes/WeatherScene.cpp
    matrix/scenes/WeatherScene.h
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
        DESTINATION "plugins/WeatherOverview/fonts"
    )
endif()
