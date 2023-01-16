vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO strinque/winpp
    REF d129064ced7bdcfbe768fa23fe00adbfbe56d49e
    SHA512 c4e0dfb7fe265a92d5bc5c78a7c1583b7a8c17d367dcec9380d202ecc7f1085733baeebc04e70f3e38c1fb0dc88050419456a9e114b76ea6696ee5bc886a359d
    HEAD_REF master
)

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug")

# Handle copyright
file(INSTALL ${SOURCE_PATH}/LICENSE DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
