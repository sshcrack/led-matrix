register_plugin(AudioVisualizer
        matrix/AudioVisualizer.cpp
        matrix/AudioVisualizer.h
        matrix/scenes/AudioSpectrumScene.cpp
        matrix/scenes/AudioSpectrumScene.h
        DESKTOP
        desktop/config.h
        desktop/AudioVisualizerDesktop.cpp
        desktop/AudioVisualizerDesktop.h
        desktop/BeatDetector.cpp
        desktop/BeatDetector.h
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
        # set(PA_BUILD_SHARED OFF CACHE BOOL "" FORCE)
        # set(PA_BUILD_STATIC ON CACHE BOOL "" FORCE)


        # set(PA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        # set(PA_BUILD_TESTS OFF CACHE BOOL "" FORCE)


        # if(WIN32)
        #         set(PA_USE_WASAPI ON CACHE BOOL "" FORCE)
        # endif()


        # add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/portaudio EXCLUDE_FROM_ALL)

        find_package(PortAudio CONFIG REQUIRED)
        target_link_libraries(AudioVisualizer PRIVATE PortAudio::portaudio)



        find_package(implot CONFIG REQUIRED)
        target_link_libraries(AudioVisualizer PRIVATE implot::implot)

        find_package(FFTW3f CONFIG REQUIRED)
        target_link_libraries(AudioVisualizer PRIVATE FFTW3::fftw3f)
        if(WIN32)
                # Look for FFTW3 DLL in vcpkg bin directory
                set(VCPKG_BIN_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin")
                file(GLOB FFTW3_DLLS "${VCPKG_BIN_DIR}/fftw3*.dll")

                if(FFTW3_DLLS)
                        install(FILES ${FFTW3_DLLS}
                                DESTINATION plugins/AudioVisualizer
                                COMPONENT AudioVisualizer
                        )
                else()
                        message(WARNING "No FFTW3 DLLs found in ${VCPKG_BIN_DIR}. The plugin might not load")
                endif()
        endif()


        target_include_directories(AudioVisualizer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty)
endif()
