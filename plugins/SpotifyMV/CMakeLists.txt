register_plugin(SpotifyMV
    matrix/SpotifyMVPlugin.cpp
    matrix/SpotifyMVPlugin.h
    matrix/scenes/SpotifyMVScene.cpp
    matrix/scenes/SpotifyMVScene.h
    DESKTOP
    desktop/SpotifyMVDesktop.cpp
    desktop/SpotifyMVDesktop.h
    desktop/SpotifyMVPacket.cpp
    desktop/SpotifyMVPacket.h
    desktop/YouTubeSearcher.cpp
    desktop/YouTubeSearcher.h
)

if(NOT ENABLE_DESKTOP)
    target_include_directories(SpotifyMV PRIVATE
        ${CMAKE_SOURCE_DIR}/plugins/SpotifyScenes
    )
endif()
