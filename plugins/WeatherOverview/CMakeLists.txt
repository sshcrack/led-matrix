# HTTP Requests

find_package(cpr CONFIG REQUIRED)
target_link_libraries(WeatherOverview PRIVATE cpr::cpr)


# Install fonts to fonts directory
install(FILES 
    "${CMAKE_CURRENT_SOURCE_DIR}/../rpi-rgb-led-matrix/fonts/7x13.bdf"
    "${CMAKE_CURRENT_SOURCE_DIR}/../rpi-rgb-led-matrix/fonts/5x8.bdf"
    "${CMAKE_CURRENT_SOURCE_DIR}/../rpi-rgb-led-matrix/fonts/4x6.bdf"
    DESTINATION "plugins/weather-overview/fonts"
)
