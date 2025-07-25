add_subdirectory(common)

if(ENABLE_DESKTOP)
    add_subdirectory(desktop)
else()
    add_subdirectory(matrix)
endif()
