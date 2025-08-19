vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO sshcrack/traypp
    REF "c9e7270577010dfecf6ba4a3e87829f338ec00d1"#"v${VERSION}"
    SHA512 fa0d7afed1a9aa612fa8abcf78341c5a8111983f01bd49c36f53136fcc910d8a138e146bda7ad58292df327a3ca430a117e7b4a2f3312382fb4076473119dc51
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(PACKAGE_NAME "tray")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")