vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO sshcrack/shadertoy
    REF v2.0.0
    SHA512 5f7f3ab764abaf073b5f0b5de701fbe2dd6d59f4fb1bb4a9632208fd463f7926e56eab0ef12a804ee95aa8661ea128e063f0cc79e8c1a3a6a1b8f2f5f65eb1fc
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DSHADERTOY_BUILD_GUI=OFF
        -DSHADERTOY_BUILD_PREVIEW_TOOL=OFF
        -DSHADERTOY_BUILD_C_API=OFF
        -DSHADERTOY_BUILD_C_API_STATIC=OFF
        -DBUILD_TESTING=OFF
)

vcpkg_cmake_install()
file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/README.md"
    "${CURRENT_PACKAGES_DIR}/LICENSE"
    "${CURRENT_PACKAGES_DIR}/debug/README.md"
    "${CURRENT_PACKAGES_DIR}/debug/LICENSE"
)

vcpkg_copy_pdbs()
vcpkg_cmake_config_fixup(PACKAGE_NAME "shadertoy" CONFIG_PATH lib/cmake/shadertoy)

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
