register_plugin(AudioVisualizer
        matrix/AudioVisualizer.cpp
        matrix/AudioVisualizer.h
        matrix/scenes/AudioSpectrumScene.cpp
        matrix/scenes/AudioSpectrumScene.h
        DESKTOP
        desktop/config.h
        desktop/AudioVisualizerDesktop.cpp
        desktop/AudioVisualizerDesktop.h
        desktop/record.cpp
        desktop/record.h
        desktop/audio_processor.h
        desktop/audio_processor.cpp
        desktop/frequency_analyzer/BarkAnalyzer.h
        desktop/frequency_analyzer/BarkAnalyzer.cpp
        desktop/frequency_analyzer/factory.h
        desktop/frequency_analyzer/factory.cpp
        desktop/frequency_analyzer/FrequencyAnalyzer.h
        desktop/frequency_analyzer/FullOctaveAnalyzer.h
        desktop/frequency_analyzer/FullOctaveAnalyzer.cpp
        desktop/frequency_analyzer/LinearAnalyzer.h
        desktop/frequency_analyzer/LinearAnalyzer.cpp
        desktop/frequency_analyzer/LogarithmicAnalyzer.h
        desktop/frequency_analyzer/LogarithmicAnalyzer.cpp
        desktop/frequency_analyzer/MelAnalyzer.h
        desktop/frequency_analyzer/MelAnalyzer.cpp
        desktop/frequency_analyzer/ThirdOctaveAnalyzer.h
        desktop/frequency_analyzer/ThirdOctaveAnalyzer.cpp
        desktop/frequency_analyzer/utils.h
        desktop/frequency_analyzer/utils.cpp
)

if(ENABLE_DESKTOP)
        find_package(PkgConfig REQUIRED) # Include functions provided by PkgConfig module.
        find_package(FFTW3f CONFIG REQUIRED)
        find_package(portaudio CONFIG REQUIRED)

        pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0) # This calls pkgconfig with appropriate arguments

        target_link_libraries(AudioVisualizer PRIVATE portaudio)
        target_link_libraries(AudioVisualizer PRIVATE implot)
        target_link_libraries(AudioVisualizer PRIVATE FFTW3::fftw3f)
        target_include_directories(AudioVisualizer PRIVATE ${CMAKE_SOURCE_DIR}/thirdparty)




        if(WIN32)
                #TODO this is probably not the best way to install all libraries in the bin directory
                get_target_property(PORTAUDIO_DLL portaudio IMPORTED_LOCATION_RELEASE)
                if(PORTAUDIO_DLL)
                        install(FILES "${PORTAUDIO_DLL}" DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
                endif()


                get_target_property(FFTW_DLL FFTW3::fftw3f IMPORTED_LOCATION_RELEASE)
                if(FFTW_DLL)
                        install(FILES "${FFTW_DLL}" DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
                endif()
        endif()
endif()
