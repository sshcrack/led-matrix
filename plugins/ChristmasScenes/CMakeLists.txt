register_plugin(ChristmasScenes
    matrix/ChristmasScenes.cpp
    matrix/ChristmasScenes.h
    matrix/Constants.cpp
    matrix/Constants.h
    matrix/scenes/SnowfallScene.cpp
    matrix/scenes/SnowfallScene.h
    matrix/scenes/ChristmasTreeScene.cpp
    matrix/scenes/ChristmasTreeScene.h
    matrix/scenes/FireplaceScene.cpp
    matrix/scenes/FireplaceScene.h
    matrix/scenes/ChristmasCountdownScene.cpp
    matrix/scenes/ChristmasCountdownScene.h
)

# Install fonts for text rendering in countdown scene
if(NOT ENABLE_DESKTOP)
    install(FILES
        "${RPI_RGB_LED_MATRIX_FONTS_DIR}/7x13.bdf"
        "${RPI_RGB_LED_MATRIX_FONTS_DIR}/5x8.bdf"
        "${RPI_RGB_LED_MATRIX_FONTS_DIR}/4x6.bdf"
        DESTINATION "plugins/ChristmasScenes/fonts"
    )
endif()
