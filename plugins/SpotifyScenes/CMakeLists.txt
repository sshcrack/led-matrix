register_plugin(SpotifyScenes
        matrix/SpotifyScenes.cpp
        matrix/scenes/CoverOnlyScene.cpp
        matrix/manager/shared_spotify.cpp
        matrix/manager/song_bpm_getter.cpp
        matrix/manager/spotify.cpp
        matrix/manager/state.cpp
        matrix/manager/track.cpp
)


# Curl
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIR})
target_link_libraries(SpotifyScenes PRIVATE ${CURL_LIBRARIES})


# HTTP Requests

find_package(cpr CONFIG REQUIRED)
target_link_libraries(SpotifyScenes PRIVATE cpr::cpr)
