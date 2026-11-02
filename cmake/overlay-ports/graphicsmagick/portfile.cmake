# Keep the baseline port's build and patches, but use the signed upstream
# release archive: Heptapod's generated archives can return an anti-bot page.
vcpkg_download_distfile(ARCHIVE
    URLS
        "https://downloads.sourceforge.net/project/graphicsmagick/graphicsmagick/${VERSION}/GraphicsMagick-${VERSION}.tar.xz"
        "https://downloads.sourceforge.net/project/graphicsmagick/graphicsmagick-history/1.3/GraphicsMagick-${VERSION}.tar.xz"
    FILENAME "GraphicsMagick-${VERSION}.tar.xz"
    SHA512 2c12a24198a8ee1f86b4c43bf8903d129c891ec2c27ba22287dfc8a5f65c9223b2caa67a37b9a91b10b049ba8425e757738f5d92a9ae70abee76d323af9bd1a0
)

vcpkg_extract_source_archive(SOURCE_PATH
    ARCHIVE "${ARCHIVE}"
    PATCHES
        dependencies.diff
        magick-types.diff
)

set(options "")
if(VCPKG_TARGET_IS_WINDOWS)
    set(options ac_cv_header_dirent_dirent_h=no)
endif()

vcpkg_make_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    AUTORECONF
    OPTIONS
        ${options}
        # Before enabling another lib, make sure that the build does not
        # hard-code the library name and dependencies (cf. dependencies.diff).
        --with-heif=no
        --with-fpx=no  ###
        --with-gs=no
        --with-jbig=no
        --with-jp2=no
        --with-jxl=no
        --with-lcms2=no
        --with-libzip=no
        --with-lzma=no
        --with-modules=no
        --with-mpeg2=no
        --with-trio=no
        --with-x=no
        --with-xml=no
        --with-wmf=no
        --with-zstd=no
)
vcpkg_make_install()
vcpkg_copy_pdbs()
vcpkg_fixup_pkgconfig()

file(REMOVE "${CURRENT_PACKAGES_DIR}/tools/${PORT}/debug/bin/gm${VCPKG_TARGET_EXECUTABLE_SUFFIX}")
vcpkg_copy_tool_dependencies("${CURRENT_PACKAGES_DIR}/tools/${PORT}/bin")

set(config_scripts
    "GraphicsMagick++-config"
    "GraphicsMagick-config"
    "GraphicsMagickWand-config"
)
string(REGEX REPLACE "^([A-Za-z]):/" "/\\1/" literal_prefix "${CURRENT_INSTALLED_DIR}")
foreach(filename IN LISTS config_scripts)
    set(file "${CURRENT_PACKAGES_DIR}/tools/graphicsmagick/bin/${filename}")
    vcpkg_replace_string("${file}" "${literal_prefix}" "'\"\${prefix}\"'")
    vcpkg_replace_string("${file}" "while test" "prefix=$(CDPATH= cd -- \"$(dirname -- \"$0\")/../../..\" && pwd -P)\n\nwhile test")
    if(NOT VCPKG_BUILD_TYPE)
        set(debug_file "${CURRENT_PACKAGES_DIR}/tools/graphicsmagick/debug/bin/${filename}")
        vcpkg_replace_string("${debug_file}" "${literal_prefix}" "'\"\${prefix}\"'")
        vcpkg_replace_string("${debug_file}" "while test" "prefix=$(CDPATH= cd -- \"$(dirname -- \"$0\")/../../../..\" && pwd -P)\n\nwhile test")
    endif()
endforeach()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/Copyright.txt")
