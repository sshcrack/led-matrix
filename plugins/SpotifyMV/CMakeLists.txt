register_plugin(SpotifyMV
    matrix/SpotifyMVPlugin.cpp
    matrix/SpotifyMVPlugin.h
    scenes/SpotifyMVScene.cpp
    scenes/SpotifyMVScene.h
    DESKTOP
    desktop/SpotifyMVDesktop.cpp
    desktop/SpotifyMVDesktop.h
    desktop/SpotifyMVDesktopMessages.cpp
    desktop/SpotifyMVDesktopPlayback.cpp
    desktop/SpotifyMVDesktopSearch.cpp
    desktop/SpotifyMVDesktopSeek.cpp
    desktop/SpotifyMVDesktopUi.cpp
    desktop/SpotifyMVPacket.cpp
    desktop/SpotifyMVPacket.h
    desktop/YouTubeSearcher.cpp
    desktop/YouTubeSearcher.h
)

if(ENABLE_DESKTOP)
    find_package(cpr CONFIG REQUIRED)
    target_link_libraries(SpotifyMV PRIVATE cpr::cpr)
endif()


