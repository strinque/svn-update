vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO strinque/winpp
    REF d3f5e4da04c77e7a6cf93c57f4a801e4a5ad7074
    SHA512 174ba1226b122efb557a724fcb69c6ac5e07de1cde1aa1ec8d9d7dc718035d43d62d902a769ffcac8ee5d18b86570eb68ae4505fe24b26e1199c4d81c526b258
    HEAD_REF master
)

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug")

# Handle copyright
file(INSTALL ${SOURCE_PATH}/LICENSE DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
