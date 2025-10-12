# header-only library
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO QTR-Modding/CLibUtilsQTR
    REF 474fed0334b72cb025a935dd67cf4ec817bda9ad
    SHA512 9b7911455afeb6a1703cb6a662f0a5471bee0acb700cbaacbac8aa159c52814bb10715372d08a01bd8e6209361a86927236a38e2021c056b69f6bb60d263d902
    HEAD_REF main
)

# Install codes
set(CLibUtilsQTR_SOURCE	${SOURCE_PATH}/include/CLibUtilsQTR)
file(INSTALL ${CLibUtilsQTR_SOURCE} DESTINATION ${CURRENT_PACKAGES_DIR}/include)
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")