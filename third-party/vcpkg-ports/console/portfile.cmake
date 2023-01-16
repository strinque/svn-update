vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO strinque/console
    REF 037de4e51f6e6d926694d8e7fd6e0e52fd44301c
    SHA512 6cfe1808ed47f62fba14c487cbc0c30f6afc7cfbef7859cb82b82e415cf22c61f00981b30e86ff5353efa3b7afa102bd4a518580eef5a97cd960a65b4587d76d
    HEAD_REF master
)

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug")

# Handle copyright
file(INSTALL ${SOURCE_PATH}/LICENSE DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
