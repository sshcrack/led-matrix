
register_plugin(PixelJoint
        matrix/PixelJoint.cpp
        matrix/PixelJoint.h
        scenes/image/ImageScene.cpp
        scenes/image/ImageScene.h
        matrix/providers/collection.cpp
        matrix/providers/collection.h
        matrix/providers/pages.cpp
        matrix/providers/pages.h
        matrix/scraper/scraped_post.cpp
        matrix/scraper/scraped_post.h
)

if(NOT ENABLE_DESKTOP)
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
endif()

if(TARGET PixelJointSceneWorker)
        find_package(CURL REQUIRED)
        find_package(cpr CONFIG REQUIRED)
        find_package(LibXml2 REQUIRED)
        target_link_libraries(PixelJointSceneWorker PRIVATE ${CURL_LIBRARIES} cpr::cpr LibXml2::LibXml2)
endif()
