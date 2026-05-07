# ScriptedScenes – Lua scripted scene plugin (matrix and desktop)
#
# lua and sol2 are installed via vcpkg through the "scripted-scenes-matrix"
# and "scripted-scenes-desktop" features in vcpkg.json.

find_package(Lua REQUIRED)
find_package(sol2 CONFIG REQUIRED)

if(ENABLE_DESKTOP)
    register_plugin(ScriptedScenes
        DESKTOP
        desktop/ScriptedScenesDesktop.cpp
        desktop/ScriptedScenesDesktop.h
    )
else()
    register_plugin(ScriptedScenes
        matrix/ScriptedScenes.cpp
        matrix/ScriptedScenes.h
        matrix/LuaScene.cpp
        matrix/LuaScene.h
        matrix/LatestLuaScene.cpp
        matrix/LatestLuaScene.h
    )
endif()

# sol2::sol2 is header-only, so we must explicitly link the Lua library
target_link_libraries(ScriptedScenes PRIVATE sol2::sol2 ${LUA_LIBRARIES})
target_include_directories(ScriptedScenes PRIVATE ${LUA_INCLUDE_DIR})
