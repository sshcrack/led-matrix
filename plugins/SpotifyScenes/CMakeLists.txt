message(STATUS "Linking curl and cpr")

# Curl
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIR})
target_link_libraries(SpotifyScenes PRIVATE ${CURL_LIBRARIES})


# HTTP Requests

find_package(cpr CONFIG REQUIRED)
target_link_libraries(SpotifyScenes PRIVATE cpr::cpr)
