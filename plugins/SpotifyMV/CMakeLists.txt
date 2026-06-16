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

if(ENABLE_DESKTOP)
    find_package(cpr CONFIG REQUIRED)
    target_link_libraries(SpotifyMV PRIVATE cpr::cpr)
endif()


