register_plugin(SpotifyScenes
        matrix/SpotifyScenes.cpp
        matrix/SpotifyScenes.h
        matrix/scenes/CoverOnlyScene.cpp
        matrix/scenes/CoverOnlyScene.h
        matrix/manager/shared_spotify.cpp
        matrix/manager/shared_spotify.h
        matrix/manager/song_bpm_getter.cpp
        matrix/manager/song_bpm_getter.h
        matrix/manager/spotify.cpp
        matrix/manager/spotify.h
        matrix/manager/state.cpp
        matrix/manager/state.h
        matrix/manager/track.cpp
        matrix/manager/track.h
)

if(NOT ENABLE_DESKTOP)
        # Curl
        find_package(CURL REQUIRED)
        include_directories(${CURL_INCLUDE_DIR})
        target_link_libraries(SpotifyScenes PRIVATE ${CURL_LIBRARIES})


        # HTTP Requests

        find_package(cpr CONFIG REQUIRED)
        target_link_libraries(SpotifyScenes PRIVATE cpr::cpr)
endif()