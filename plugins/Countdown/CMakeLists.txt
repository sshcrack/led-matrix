register_plugin(
    Countdown
    matrix/Countdown.cpp
    matrix/Constants.cpp
    matrix/scenes/CountdownScene.cpp
)

if(NOT ENABLE_DESKTOP)
    # Install fonts to plugin fonts directory so they can be loaded at runtime
    install(FILES
        "${RPI_RGB_LED_MATRIX_FONTS_DIR}/7x13.bdf"
        "${RPI_RGB_LED_MATRIX_FONTS_DIR}/5x8.bdf"
        "${RPI_RGB_LED_MATRIX_FONTS_DIR}/4x6.bdf"
        DESTINATION "plugins/Countdown/fonts"
    )
endif()
