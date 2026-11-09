# header-only library
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO QTR-Modding/CLibUtilsQTR
    REF ee33468b3894b8976316c1c309ff592618ad4a27
    SHA512 39c86a7f00794e7fa8f7ad556b0ea93330a279f1e22fab91bf66a2e10aa0c6ce13837e46fbf1124adcdd5fd1e292d3177ccf57662b8a7faec91f0f01a45d3572
    HEAD_REF main
)

# Install codes
set(CLibUtilsQTR_SOURCE	${SOURCE_PATH}/include/CLibUtilsQTR)
file(INSTALL ${CLibUtilsQTR_SOURCE} DESTINATION ${CURRENT_PACKAGES_DIR}/include)
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")