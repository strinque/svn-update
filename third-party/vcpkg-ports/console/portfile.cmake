vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO strinque/console
    REF 81ab862a2cf6406fc9975c1ab181b43cd1e48989
    SHA512 9074c615a5d5dab5493cf6f232e457b447d8fd73fa14bdc5c567906a7129ecaefe2ffa6efaf523c7c18800cbd6db60c61343c5442f867af29e35cd4cc0914882
    HEAD_REF master
)

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug")

# Handle copyright
file(INSTALL ${SOURCE_PATH}/LICENSE DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
