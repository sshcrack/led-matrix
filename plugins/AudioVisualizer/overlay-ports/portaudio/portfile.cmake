vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO PortAudio/portaudio
    REF 8bb34026b02e514e11b4b6b20e42f3acbb9e9d98
    SHA512 234cbd2e318b29105f74b36f4cc1bd5754431a174a7b5e3cc28359e1737d362766e45c65f0cc893d422a1c0c7ec981515b1b5c31090ca65f534543dd74ea9172
    PATCHES
        use-system-pulseaudio.patch
)

string(COMPARE EQUAL "${VCPKG_CRT_LINKAGE}" "static" PA_DLL_LINK_WITH_STATIC_RUNTIME)

if("${VCPKG_LIBRARY_LINKAGE}" STREQUAL "dynamic")
    set(PA_BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
else()
    set(PA_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
endif()

vcpkg_list(SET options)
if(VCPKG_TARGET_IS_WINDOWS)
    vcpkg_list(APPEND options
        -DPA_USE_ASIOSDK=OFF
        -DPA_DLL_LINK_WITH_STATIC_RUNTIME=${PA_DLL_LINK_WITH_STATIC_RUNTIME}
        -DPA_LIBNAME_ADD_SUFFIX=OFF
    )

    vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
        FEATURES
        wasapi PA_USE_WASAPI
    )
elseif(VCPKG_TARGET_IS_IOS OR VCPKG_TARGET_IS_OSX)
    vcpkg_list(APPEND options
        # avoid absolute paths
        -DCOREAUDIO_LIBRARY:STRING=-Wl,-framework,CoreAudio
        -DAUDIOTOOLBOX_LIBRARY:STRING=-Wl,-framework,AudioToolbox
        -DAUDIOUNIT_LIBRARY:STRING=-Wl,-framework,AudioUnit
        -DCOREFOUNDATION_LIBRARY:STRING=-Wl,-framework,CoreFoundation
        -DCORESERVICES_LIBRARY:STRING=-Wl,-framework,CoreServices
    )
else()
    vcpkg_list(APPEND options
        -DPA_USE_JACK=ON
        -DPA_USE_ALSA=ON
        -DPA_USE_PULSEAUDIO=ON
    )
endif()


vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${options}
        ${FEATURE_OPTIONS}
        -DPA_BUILD_SHARED_LIBS=${PA_BUILD_SHARED_LIBS}
        -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH
        -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH
    OPTIONS_DEBUG
        -DPA_ENABLE_DEBUG_OUTPUT:BOOL=ON
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/${PORT})
vcpkg_copy_pdbs()
vcpkg_fixup_pkgconfig()

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
    "${CURRENT_PACKAGES_DIR}/share/doc"
)

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.txt")
