register_plugin(AudioVisualizer
        matrix/AudioVisualizer.cpp
        matrix/AudioVisualizer.h
        matrix/scenes/AudioSpectrumScene.cpp
        matrix/scenes/AudioSpectrumScene.h
        DESKTOP
        desktop/AudioVisualizerDesktop.cpp
        desktop/AudioVisualizerDesktop.h
        desktop/record.cpp
        desktop/record.h
)

if(ENABLE_DESKTOP)
        find_package(PkgConfig REQUIRED) # Include functions provided by PkgConfig module.

        pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0) # This calls pkgconfig with appropriate arguments
        # Use results of pkg_check_modules() call.
        include_directories(${PORTAUDIO_INCLUDE_DIRS})

        target_link_libraries(AudioVisualizer PRIVATE ${PORTAUDIO_LIBRARIES})
endif()
