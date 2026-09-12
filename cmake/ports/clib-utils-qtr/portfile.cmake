# header-only library
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO QTR-Modding/CLibUtilsQTR
    REF d219ca7fb38a5e5ee830e0c9f7e28f43da9ac946
    SHA512 712f9d8efc9536cc3caa00dc6ae1eb039879ddf98ceaa923e2aa763488eb97583825de09ddc9178425f5a42ace8c626918a97429aca7e07874acc0978d410640
    HEAD_REF main
)

# Install codes
set(CLibUtilsQTR_SOURCE	${SOURCE_PATH}/include/CLibUtilsQTR)
file(INSTALL ${CLibUtilsQTR_SOURCE} DESTINATION ${CURRENT_PACKAGES_DIR}/include)
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
