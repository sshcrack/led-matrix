# Resources
file(READ ${LIB_NAME}/icons/weather_icons/descriptions.json RESOURCE_DATA HEX)
configure_file(${LIB_NAME}/icons/weather_icons.h.in icons/weather_icons.h @ONLY)
