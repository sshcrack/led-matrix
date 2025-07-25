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
        desktop/udpBandsPacket.cpp
        desktop/udpBandsPacket.h
        desktop/AudioProcessor.h
        desktop/AudioProcessor.cpp
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
        # Setting up PortAudio

        # Make lib shared
        set(PA_BUILD_SHARED OFF CACHE BOOL "" FORCE)
        set(PA_BUILD_STATIC ON CACHE BOOL "" FORCE)


        set(PA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(PA_BUILD_TESTS OFF CACHE BOOL "" FORCE)


        if(WIN32)
                set(PA_USE_WASAPI ON CACHE BOOL "" FORCE)
        endif()


        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/portaudio ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/portaudio/build EXCLUDE_FROM_ALL)

        target_link_libraries(AudioVisualizer PRIVATE portaudio)



        add_subdirectory(thirdparty)
        target_link_libraries(AudioVisualizer PRIVATE implot)

        find_package(FFTW3f CONFIG REQUIRED)
        target_link_libraries(AudioVisualizer PRIVATE FFTW3::fftw3f)
        target_include_directories(AudioVisualizer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty)
endif()
