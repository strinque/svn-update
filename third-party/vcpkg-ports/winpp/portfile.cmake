vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO strinque/winpp
    REF e358e0f75c688a645f68c110660f71c9a47eccb9
    SHA512 61a1036fb93df19c74e7d50f7d3abc1b94f7328454affef09627a93c17b7f6e34b20e8463214ea57bcd1ddaa3f6cf9268dead14f36583b694b83628eda04df2d
    HEAD_REF master
)

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug")

# Handle copyright
file(INSTALL ${SOURCE_PATH}/LICENSE DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
