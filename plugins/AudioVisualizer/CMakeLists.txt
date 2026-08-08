register_plugin(AudioVisualizer
        matrix/AudioVisualizer.cpp
        matrix/AudioVisualizer.h
        matrix/scenes/AudioSpectrumScene.cpp
        matrix/scenes/AudioSpectrumScene.h
        matrix/scenes/AudioReactiveScenes.cpp
        matrix/scenes/AudioReactiveScenes.h
        matrix/scenes/MusicDirectorScene.cpp
        matrix/scenes/MusicDirectorScene.h
        DESKTOP
        desktop/config.h
        desktop/AudioVisualizerDesktop.cpp
        desktop/AudioVisualizerDesktop.h
        desktop/record.cpp
        desktop/record.h
        desktop/MusicAnalyzer.cpp
        desktop/MusicAnalyzer.h
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
        set(PA_BUILD_SHARED OFF CACHE BOOL "" FORCE)
        set(PA_BUILD_STATIC ON CACHE BOOL "" FORCE)


        set(PA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(PA_BUILD_TESTS OFF CACHE BOOL "" FORCE)


        if(WIN32)
                set(PA_USE_WASAPI ON CACHE BOOL "" FORCE)
        endif()


        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/portaudio EXCLUDE_FROM_ALL)

        target_compile_definitions(portaudio PRIVATE
                PA_WASAPI_MAX_CONST_DEVICE_COUNT=64
        )
        if(NOT MSVC)
                target_compile_options(portaudio PRIVATE -Wno-macro-redefined)
                target_compile_definitions(portaudio PRIVATE
                        _WIN32_WINNT=0x0601
                        WINVER=0x0601
                )
        endif()

        target_link_libraries(AudioVisualizer PRIVATE portaudio)



        find_package(implot CONFIG REQUIRED)
        target_link_libraries(AudioVisualizer PRIVATE implot::implot)

        find_package(FFTW3f CONFIG REQUIRED)
        target_link_libraries(AudioVisualizer PRIVATE FFTW3::fftw3f)


        target_include_directories(AudioVisualizer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty)

        # Deterministic regression/validation executable for the exact music
        # analyzer used by the desktop plugin. With no arguments it exercises
        # slow/fast tempos, silence/noise rejection, tempo changes and stale
        # lock decay; pass a PCM16 WAV and optional expected BPM for real audio.
        add_executable(audio_analyzer_benchmark
                ${CMAKE_SOURCE_DIR}/tests/audio_analyzer_benchmark.cpp
                desktop/MusicAnalyzer.cpp
        )
        target_compile_features(audio_analyzer_benchmark PRIVATE cxx_std_23)
        target_include_directories(audio_analyzer_benchmark PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/desktop
        )
        target_link_libraries(audio_analyzer_benchmark PRIVATE
                SharedToolsCommon
                FFTW3::fftw3f
        )
        add_test(NAME audio_analyzer_benchmark COMMAND audio_analyzer_benchmark)
endif()
