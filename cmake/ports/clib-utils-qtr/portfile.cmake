# header-only library
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO QTR-Modding/CLibUtilsQTR
    REF v2.5.0
    SHA512 3b213eb3df1ada939b6dd6d877384fae9196d1a042cc08f1de50888d2ac6e401e54380006b9ed9291062cb75e406a5960a8d2e6cc31f5c8574d94c33f04a6c8f
    HEAD_REF main
)

# Install codes
set(CLibUtilsQTR_SOURCE	${SOURCE_PATH}/include/CLibUtilsQTR)
file(INSTALL ${CLibUtilsQTR_SOURCE} DESTINATION ${CURRENT_PACKAGES_DIR}/include)
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
