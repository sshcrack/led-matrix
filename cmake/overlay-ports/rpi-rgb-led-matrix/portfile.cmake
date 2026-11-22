vcpkg_check_linkage(ONLY_DYNAMIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO sshcrack/rpi-rgb-led-matrix
    # v0.2.3: bulk FrameCanvas::GetRGB() readback (led-matrix#148), on top of the
    # portable emulator core (v0.2.x) and emulator-independent headers (v0.1.17).
    REF v0.2.3
    SHA512 7449de8d41c94441aa16ec141bae0d5d9ca2e638a85b970cae1a6919711b5b896e95a97d1bd86e39194c8314c2be4557f8c5ffcb4c1cdaf6bf91eed8d5f628ac
    HEAD_REF master
)

# `emulator` is the headless core (no dependencies); `emulator-sdl` adds the
# SDL window. ENABLE_EMULATOR_SDL is always passed so the library never picks
# SDL up just because it happens to be installed.
vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
    emulator ENABLE_EMULATOR
    emulator-sdl ENABLE_EMULATOR_SDL
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
)



vcpkg_cmake_install()

# CMake installs headers for both configurations; keep one copy. The Debug
# package config must remain until vcpkg_cmake_config_fixup() has merged it.
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_copy_pdbs()
vcpkg_cmake_config_fixup(PACKAGE_NAME "rpi-rgb-led-matrix")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING")
