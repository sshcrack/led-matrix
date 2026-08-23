register_plugin(
    Countdown
    matrix/Countdown.cpp
    matrix/Constants.cpp
    scenes/CountdownScene.cpp
)

if(NOT ENABLE_DESKTOP)
    # Install fonts to plugin fonts directory so they can be loaded at runtime
    install(FILES
        "${RPI_RGB_LED_MATRIX_FONTS_DIR}/7x13.bdf"
        "${RPI_RGB_LED_MATRIX_FONTS_DIR}/5x8.bdf"
        "${RPI_RGB_LED_MATRIX_FONTS_DIR}/4x6.bdf"
        DESTINATION "lib/led-matrix/plugins/Countdown/fonts"
    )
elseif(ENABLE_SCENE_WORKER)
    install(FILES
        "${RPI_RGB_LED_MATRIX_FONTS_DIR}/7x13.bdf"
        "${RPI_RGB_LED_MATRIX_FONTS_DIR}/5x8.bdf"
        "${RPI_RGB_LED_MATRIX_FONTS_DIR}/4x6.bdf"
        DESTINATION "lib/led-matrix-desktop/scene-plugins/Countdown/fonts"
    )
endif()

if(TARGET CountdownSceneWorker)
    # Keep worker builds runnable directly from the build tree, not only after
    # installation. These scenes resolve fonts relative to their plugin DSO.
    add_custom_command(TARGET CountdownSceneWorker POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:CountdownSceneWorker>/fonts"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RPI_RGB_LED_MATRIX_FONTS_DIR}/7x13.bdf"
            "${RPI_RGB_LED_MATRIX_FONTS_DIR}/5x8.bdf"
            "${RPI_RGB_LED_MATRIX_FONTS_DIR}/4x6.bdf"
            "$<TARGET_FILE_DIR:CountdownSceneWorker>/fonts"
    )
endif()
