register_plugin(HorseScenes
        matrix/HorseScenes.cpp
        matrix/HorseScenes.h
        matrix/scenes/HorseDraw.h
        matrix/scenes/HorseScenes.cpp
        matrix/scenes/HorseScenes.h
)

if(ENABLE_EMULATOR)
    add_executable(horse_gait_loop_test tests/gait_loop.cpp)
    add_test(NAME horse_gait_loop COMMAND horse_gait_loop_test)
endif()
