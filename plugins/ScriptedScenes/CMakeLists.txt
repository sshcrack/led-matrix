# ScriptedScenes – Lua scripted scene plugin (matrix builds only)
#
# Requires the Lua 5.x development headers and library.
# On Debian/Ubuntu/Raspberry Pi OS: sudo apt install liblua5.4-dev
# On macOS: brew install lua
#
# The plugin is silently skipped if Lua is not found so the rest of
# the build is unaffected.

if(ENABLE_DESKTOP)
    # This plugin has no desktop implementation.
    return()
endif()

find_package(Lua QUIET)
if(NOT LUA_FOUND)
    message(STATUS
        "[ScriptedScenes] Lua development headers not found – "
        "skipping plugin. Install with: sudo apt install liblua5.4-dev")
    return()
endif()

register_plugin(ScriptedScenes
    matrix/ScriptedScenes.cpp
    matrix/ScriptedScenes.h
    matrix/LuaScene.cpp
    matrix/LuaScene.h
)

target_link_libraries(ScriptedScenes PRIVATE ${LUA_LIBRARIES})
target_include_directories(ScriptedScenes PRIVATE ${LUA_INCLUDE_DIR})
