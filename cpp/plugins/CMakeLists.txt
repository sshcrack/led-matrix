
file(GLOB_RECURSE PLUGIN_SOURCES
        *.cpp
)


foreach(PLUGIN_SOURCE ${PLUGIN_SOURCES})
    get_filename_component(LIB_NAME ${PLUGIN_SOURCE} NAME_WE)
    set(LIB_NAMES ${LIB_NAMES} ${LIB_NAME})
    add_library(${LIB_NAME} SHARED ${PLUGIN_SOURCE})
endforeach(PLUGIN_SOURCE ${PLUGIN_SOURCES})
