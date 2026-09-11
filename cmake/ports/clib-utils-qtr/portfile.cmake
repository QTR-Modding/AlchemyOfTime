# header-only library
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO QTR-Modding/CLibUtilsQTR
    REF 1dc46f58d361bfffa63d73973af486540f5789e8
    SHA512 29325418bb68b044555b46ac6830d8726b61ec0ee149ef410e0c9f629ae035655653155c8eabcf0710f6e5e0c81e44ea1549ec0400eeba8c010720de26ed1cbc
    HEAD_REF main
)

# Install codes
set(CLibUtilsQTR_SOURCE	${SOURCE_PATH}/include/CLibUtilsQTR)
file(INSTALL ${CLibUtilsQTR_SOURCE} DESTINATION ${CURRENT_PACKAGES_DIR}/include)
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
