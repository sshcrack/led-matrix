register_plugin(
    Countdown
    matrix/Countdown.cpp
    matrix/Constants.cpp
    matrix/scenes/CountdownScene.cpp
)

if(NOT ENABLE_DESKTOP)
    # Install fonts to plugin fonts directory so they can be loaded at runtime
    install(FILES
        "${CMAKE_SOURCE_DIR}/thirdparty/rpi-rgb-led-matrix/fonts/7x13.bdf"
        "${CMAKE_SOURCE_DIR}/thirdparty/rpi-rgb-led-matrix/fonts/5x8.bdf"
        "${CMAKE_SOURCE_DIR}/thirdparty/rpi-rgb-led-matrix/fonts/4x6.bdf"
        DESTINATION "plugins/Countdown/fonts"
    )
endif()
