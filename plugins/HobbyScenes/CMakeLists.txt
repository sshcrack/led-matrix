register_plugin(HobbyScenes
        matrix/HobbyScenes.cpp
        matrix/HobbyScenes.h
        matrix/scenes/Draw.h
        matrix/scenes/FigureDraw.h
        matrix/scenes/HorseDraw.h
        matrix/scenes/HobbyScene.cpp
        matrix/scenes/HobbyScene.h
        matrix/scenes/HorseScenes.cpp
        matrix/scenes/HorseScenes.h
        matrix/scenes/HikingScenes.cpp
        matrix/scenes/HikingScenes.h
        matrix/scenes/SportScenes.cpp
        matrix/scenes/SportScenes.h
)

if(ENABLE_EMULATOR)
    add_executable(horse_gait_loop_test tests/gait_loop.cpp)
    add_test(NAME horse_gait_loop COMMAND horse_gait_loop_test)
endif()
