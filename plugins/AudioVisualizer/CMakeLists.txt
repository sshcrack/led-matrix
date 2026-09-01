register_plugin(AudioVisualizer
        matrix/AudioPreviewProvider.cpp
        matrix/AudioPreviewProvider.h
        matrix/AudioVisualizer.cpp
        matrix/AudioVisualizer.h
        scenes/AudioSpectrumScene.cpp
        scenes/AudioSpectrumScene.h
        scenes/AudioParticleScene.cpp
        scenes/AudioParticleScene.h
        scenes/MusicDirectorScene.cpp
        scenes/MusicDirectorScene.h
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

if(ENABLE_EMULATOR)
        add_executable(audio_spectrum_visual_regression
                ${CMAKE_SOURCE_DIR}/tests/audio_spectrum_visual_regression.cpp
        )
        target_compile_features(audio_spectrum_visual_regression PRIVATE cxx_std_23)
        target_include_directories(audio_spectrum_visual_regression PRIVATE
                ${CMAKE_SOURCE_DIR}
        )
        target_link_libraries(audio_spectrum_visual_regression PRIVATE
                AudioVisualizer
                SharedToolsCommon
                SharedToolsMatrix
                PkgConfig::GraphicsMagick
                magic_enum::magic_enum
                restinio::restinio
                rpi_rgb_led_matrix::rpi-rgb-led-matrix
        )
        add_test(NAME audio_spectrum_visual_regression
                COMMAND ${CMAKE_COMMAND} -E env
                        "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:AudioVisualizer>:$<TARGET_FILE_DIR:SharedToolsCommon>:$<TARGET_FILE_DIR:SharedToolsMatrix>:$<TARGET_FILE_DIR:rpi_rgb_led_matrix::rpi-rgb-led-matrix>:$ENV{LD_LIBRARY_PATH}"
                        $<TARGET_FILE:audio_spectrum_visual_regression>
        )
endif()

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

        if(NOT WIN32)
            # Regression for a desktop-only flicker where render() used to skip an
            # entire ImGui frame whenever the audio sender held stateMutex. The
            # plugin host then briefly lost its vertical scrollbar and its content
            # width changed, producing a visible flash/zoom reset. This white-box
            # harness links the plugin DSO directly, so keep it on ELF platforms;
            # the production fix itself is platform-independent and is compiled by
            # the normal Windows desktop target.
            add_executable(audio_visualizer_render_regression
                    ${CMAKE_SOURCE_DIR}/tests/audio_visualizer_render_regression.cpp
            )
            target_compile_features(audio_visualizer_render_regression PRIVATE cxx_std_23)
            target_compile_definitions(audio_visualizer_render_regression PRIVATE PLUGIN_NAME="AudioVisualizer")
            target_link_libraries(audio_visualizer_render_regression PRIVATE
                    AudioVisualizer
                    SharedToolsCommon
                    SharedToolsDesktop
                    hello-imgui::hello_imgui
                    implot::implot
                    portaudio
            )
            add_test(NAME audio_visualizer_render_regression
                    COMMAND ${CMAKE_COMMAND} -E env
                            "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:AudioVisualizer>:$<TARGET_FILE_DIR:SharedToolsCommon>:$<TARGET_FILE_DIR:SharedToolsDesktop>:$ENV{LD_LIBRARY_PATH}"
                            $<TARGET_FILE:audio_visualizer_render_regression>
            )
        endif()
endif()
