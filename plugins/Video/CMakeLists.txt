register_plugin(Video
        matrix/VideoPlugin.cpp
        matrix/VideoPlugin.h
        matrix/scenes/VideoScene.cpp
        matrix/scenes/VideoScene.h
        DESKTOP
        desktop/VideoDesktop.cpp
        desktop/VideoDesktop.h
        desktop/VideoPacket.cpp
        desktop/VideoPacket.h
)

if(ENABLE_DESKTOP)
    # Desktop specific dependencies can be added here if needed
    # for example, we might need simple threading or system calls which are standard
else()
    # Matrix specific dependencies
endif()
