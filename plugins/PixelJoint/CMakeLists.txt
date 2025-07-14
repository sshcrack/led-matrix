
register_plugin(PixelJoint
        matrix/PixelJoint.cpp
        matrix/scenes/image/ImageScene.cpp
        matrix/providers/collection.cpp
        matrix/providers/pages.cpp
        matrix/scraper/scraped_post.cpp
)

# Curl
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIR})
target_link_libraries(PixelJoint PRIVATE ${CURL_LIBRARIES})


# HTTP Requests

find_package(cpr CONFIG REQUIRED)
target_link_libraries(PixelJoint PRIVATE cpr::cpr)

# HTTP Parsing

find_package(LibXml2 REQUIRED)
target_link_libraries(PixelJoint PRIVATE LibXml2::LibXml2)

