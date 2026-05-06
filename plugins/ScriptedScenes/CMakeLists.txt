# ScriptedScenes – Lua scripted scene plugin (matrix builds only)
#
# lua and sol2 are installed via vcpkg through the "scripted-scenes-matrix"
# feature in vcpkg.json (auto-detected by the CMakeLists.txt plugin loop).
# No system Lua headers are required.

if(ENABLE_DESKTOP)
    # This plugin has no desktop implementation.
    return()
endif()

find_package(Lua REQUIRED)
find_package(sol2 CONFIG REQUIRED)

register_plugin(ScriptedScenes
    matrix/ScriptedScenes.cpp
    matrix/ScriptedScenes.h
    matrix/LuaScene.cpp
    matrix/LuaScene.h
    matrix/LatestLuaScene.cpp
    matrix/LatestLuaScene.h
)

# sol2::sol2 is header-only, so we must explicitly link the Lua library
target_link_libraries(ScriptedScenes PRIVATE sol2::sol2 ${LUA_LIBRARIES})
target_include_directories(ScriptedScenes PRIVATE ${LUA_INCLUDE_DIR})
