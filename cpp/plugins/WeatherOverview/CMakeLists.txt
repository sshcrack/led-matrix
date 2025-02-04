# HTTP Requests

find_package(cpr CONFIG REQUIRED)
target_link_libraries(WeatherOverview PRIVATE cpr::cpr)


# Install fonts
install(FILES 
    "${CMAKE_CURRENT_SOURCE_DIR}/../rpi-rgb-led-matrix/fonts/7x13.bdf"
    "${CMAKE_CURRENT_SOURCE_DIR}/../rpi-rgb-led-matrix/fonts/5x8.bdf"
    DESTINATION share/weather-overview/fonts
)

# Define font directory with default value
set(WEATHER_FONT_DIRECTORY "${CMAKE_INSTALL_PREFIX}/share/weather-overview/fonts" CACHE PATH "Path to font directory")

# Set font directory path for compilation
target_compile_definitions(WeatherOverview PRIVATE
    FONT_DIRECTORY="${WEATHER_FONT_DIRECTORY}")
