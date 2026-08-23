add_subdirectory(common)

if(ENABLE_DESKTOP)
    add_subdirectory(desktop)
    # The scene worker is a separate process on purpose: matrix and desktop
    # plugin runtimes both expose Plugins::PluginManager and must never be
    # linked into the same executable.
    if(ENABLE_SCENE_WORKER)
        add_subdirectory(matrix)
    endif()
else()
    add_subdirectory(matrix)
endif()
