vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO strinque/winpp
    REF 58cfaca78eb653bcaf8ed461851c8157b5216159
    SHA512 9d0693eabb9101669ffaedc31bced2503f7bf346a2d5a3204910e00cb347d96ae979062a565cf792d2599f0999fe98ab532e91baf5b0bcbfa5f2ef2b73e09801
    HEAD_REF master
)

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug")

# Handle copyright
file(INSTALL ${SOURCE_PATH}/LICENSE DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
