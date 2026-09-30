# Packaging script for Unity 6 Addon (UPM tarball)
# Arguments:
#   SRC_DIR: Source directory of the Unity addon (e.g. ${PROJECT_SOURCE_DIR}/addons/unity)
#   STAGING_DIR: Temporary staging directory (e.g. ${CMAKE_BINARY_DIR}/addons_staging/unity)
#   LICENSE_FILE: Path to LICENSE file
#   PACKAGE_VERSION: Version of the package (e.g. ${PROJECT_VERSION})
#   OUTPUT_TGZ: Target tgz path (e.g. ${CMAKE_BINARY_DIR}/dist/com.bentopack.importer-${PROJECT_VERSION}.tgz)

string(REPLACE "\"" "" SRC_DIR "${SRC_DIR}")
string(REPLACE "\"" "" STAGING_DIR "${STAGING_DIR}")
string(REPLACE "\"" "" LICENSE_FILE "${LICENSE_FILE}")
string(REPLACE "\"" "" PACKAGE_VERSION "${PACKAGE_VERSION}")
string(REPLACE "\"" "" OUTPUT_TGZ "${OUTPUT_TGZ}")

if(NOT DEFINED SRC_DIR OR NOT DEFINED STAGING_DIR OR NOT DEFINED OUTPUT_TGZ)
    message(FATAL_ERROR "SRC_DIR, STAGING_DIR, and OUTPUT_TGZ must be defined.")
endif()

# Unity Package Manager (UPM / npm tarball spec) requires the root directory
# inside the tarball to be named "package/".
# Unity extracts tarballs expecting: "package/package.json".
set(ADDON_DEST "${STAGING_DIR}/package")

# 1. Clean previous staging
if(EXISTS "${STAGING_DIR}")
    file(REMOVE_RECURSE "${STAGING_DIR}")
endif()

file(MAKE_DIRECTORY "${ADDON_DEST}")

# 2. Copy source files
file(GLOB_RECURSE ADDON_FILES RELATIVE "${SRC_DIR}" "${SRC_DIR}/*")
foreach(REL_PATH ${ADDON_FILES})
    set(SRC_FILE "${SRC_DIR}/${REL_PATH}")
    set(DST_FILE "${ADDON_DEST}/${REL_PATH}")
    get_filename_component(DST_DIR "${DST_FILE}" DIRECTORY)
    file(MAKE_DIRECTORY "${DST_DIR}")
    file(COPY_FILE "${SRC_FILE}" "${DST_FILE}")
endforeach()

# 3. Synchronize version in package.json if PACKAGE_VERSION is provided
if(DEFINED PACKAGE_VERSION AND NOT "${PACKAGE_VERSION}" STREQUAL "" AND EXISTS "${ADDON_DEST}/package.json")
    file(READ "${ADDON_DEST}/package.json" PKG_JSON_CONTENT)
    string(REGEX REPLACE "\"version\":[ \t]*\"[^\"]+\"" "\"version\": \"${PACKAGE_VERSION}\"" PKG_JSON_CONTENT "${PKG_JSON_CONTENT}")
    file(WRITE "${ADDON_DEST}/package.json" "${PKG_JSON_CONTENT}")
endif()

# 4. Copy LICENSE as LICENSE.md and provide .meta file for Unity asset database
if(DEFINED LICENSE_FILE AND EXISTS "${LICENSE_FILE}")
    file(COPY_FILE "${LICENSE_FILE}" "${ADDON_DEST}/LICENSE.md")
    set(LICENSE_META "fileFormatVersion: 2\nguid: 89b1c2deef34bbec4ccaec8936f5ada2\nTextScriptImporter:\n  externalObjects: {}\n  userData: \n  assetBundleName: \n  assetBundleVariant: \n")
    file(WRITE "${ADDON_DEST}/LICENSE.md.meta" "${LICENSE_META}")
endif()

# 5. Create TGZ archive containing "package"
get_filename_component(OUTPUT_TGZ_DIR "${OUTPUT_TGZ}" DIRECTORY)
file(MAKE_DIRECTORY "${OUTPUT_TGZ_DIR}")

# Remove existing tgz if any
if(EXISTS "${OUTPUT_TGZ}")
    file(REMOVE "${OUTPUT_TGZ}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar "zcf" "${OUTPUT_TGZ}" "package"
    WORKING_DIRECTORY "${STAGING_DIR}"
    RESULT_VARIABLE TAR_RES
)

if(NOT TAR_RES EQUAL 0)
    message(FATAL_ERROR "Failed to create Unity addon tgz archive: ${OUTPUT_TGZ}")
endif()

message(STATUS "Unity UPM Addon package created successfully: ${OUTPUT_TGZ}")
