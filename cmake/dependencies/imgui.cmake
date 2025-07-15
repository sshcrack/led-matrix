include(FetchContent)

FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.1
)
FetchContent_MakeAvailable(imgui)

# imgui doesn't provide a CMakeLists.txt, we have to add sources manually
# Note: FetchContent_MakeAvailable provides ${imgui_SOURCE_DIR}
set(IMGUI_SOURCES
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(
        imgui PUBLIC $<BUILD_INTERFACE:${imgui_SOURCE_DIR}> $<INSTALL_INTERFACE:include/imgui>
)
set_target_properties(imgui PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_STANDARD 11)
add_library(imgui::imgui ALIAS imgui)

# install(DIRECTORY ${imgui_SOURCE_DIR}/ DESTINATION include/imgui)

# Create CMake Package File for imgui so that it can be found with find_package(imgui)
# and exports target imgui::imgui to link against
# install(
#         TARGETS imgui
#         EXPORT imguiConfig
#         ARCHIVE DESTINATION lib
#         LIBRARY DESTINATION lib
#         RUNTIME DESTINATION bin
# )
export(
        TARGETS imgui
        NAMESPACE imgui::
        FILE "${imgui_BINARY_DIR}/imguiConfig.cmake"
)

# install(
#         EXPORT imguiConfig
#         DESTINATION lib/cmake/imgui
#         EXPORT_LINK_INTERFACE_LIBRARIES
#         NAMESPACE imgui::
# )