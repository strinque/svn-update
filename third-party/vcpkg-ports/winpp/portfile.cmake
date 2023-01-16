vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO strinque/winpp
    REF 35c54dde7a9b1fa9d0a5f4d2fd74dd7539f74996
    SHA512 47c383923e53988d2ed8e7eaf164bbfc72228ab661fb9cc06d21741aa1ed0e89c4110ad920bac29676a5157e0caa713ed030ed5f0a35f9631e9f6893ef2765b8
    HEAD_REF master
)

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug")

# Handle copyright
file(INSTALL ${SOURCE_PATH}/LICENSE DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
