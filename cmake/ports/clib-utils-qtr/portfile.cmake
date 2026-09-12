# header-only library
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO QTR-Modding/CLibUtilsQTR
    REF v2.4.0
    SHA512 09f628bc589945c02ee6ed6c08b7261b3aa3627820bb647028d76a9d49ae4ff80dfeff0a491bab3a71b6d3ebbb7edbc3622632ebec06a453931d39b945c703e8
    HEAD_REF main
)

# Install codes
set(CLibUtilsQTR_SOURCE	${SOURCE_PATH}/include/CLibUtilsQTR)
file(INSTALL ${CLibUtilsQTR_SOURCE} DESTINATION ${CURRENT_PACKAGES_DIR}/include)
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
