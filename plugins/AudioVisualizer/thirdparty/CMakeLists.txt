set(IMPLOT_HEADERS ./implot/implot.h ./implot/implot_internal.h)
set(IMPLOT_SRC     ./implot/implot.cpp ./implot/implot_items.cpp ./implot/implot_demo.cpp)
add_library(implot STATIC ${IMPLOT_HEADERS} ${IMPLOT_SRC})

find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(implot PRIVATE hello-imgui::hello_imgui)
target_compile_definitions(implot PUBLIC IMPLOT_DEBUG IMPLOT_DLL_EXPORT IMPLOT_BACKEND_ENABLE_OPENGL3 IMGUI_IMPL_OPENGL_LOADER_GLAD)