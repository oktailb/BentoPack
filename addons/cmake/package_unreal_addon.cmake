# Packaging script for Unreal Engine 5 Plugin
# Arguments:
#   SRC_DIR: Source directory of the Unreal plugin (e.g. ${PROJECT_SOURCE_DIR}/addons/unreal)
#   STAGING_DIR: Temporary staging directory (e.g. ${CMAKE_BINARY_DIR}/addons_staging/unreal)
#   LICENSE_FILE: Path to LICENSE file
#   PACKAGE_VERSION: Version of the package (e.g. ${PROJECT_VERSION})
#   OUTPUT_ZIP: Target zip path (e.g. ${CMAKE_BINARY_DIR}/dist/unreal-bentopack-plugin-${PROJECT_VERSION}.zip)

string(REPLACE "\"" "" SRC_DIR "${SRC_DIR}")
string(REPLACE "\"" "" STAGING_DIR "${STAGING_DIR}")
string(REPLACE "\"" "" LICENSE_FILE "${LICENSE_FILE}")
string(REPLACE "\"" "" PACKAGE_VERSION "${PACKAGE_VERSION}")
string(REPLACE "\"" "" OUTPUT_ZIP "${OUTPUT_ZIP}")

if(NOT DEFINED SRC_DIR OR NOT DEFINED STAGING_DIR OR NOT DEFINED OUTPUT_ZIP)
    message(FATAL_ERROR "SRC_DIR, STAGING_DIR, and OUTPUT_ZIP must be defined.")
endif()

# Unreal Engine plugins extract cleanly into Plugins/BentoPack
set(ADDON_DEST "${STAGING_DIR}/BentoPack")

# 1. Clean previous staging
if(EXISTS "${STAGING_DIR}")
    file(REMOVE_RECURSE "${STAGING_DIR}")
endif()

file(MAKE_DIRECTORY "${ADDON_DEST}")

# 2. Copy source files
file(GLOB_RECURSE ADDON_FILES RELATIVE "${SRC_DIR}" "${SRC_DIR}/*")
foreach(REL_PATH ${ADDON_FILES})
    # Exclude intermediate build files or binaries if any
    if(REL_PATH MATCHES "^Binaries" OR REL_PATH MATCHES "^Intermediate")
        continue()
    endif()

    set(SRC_FILE "${SRC_DIR}/${REL_PATH}")
    set(DST_FILE "${ADDON_DEST}/${REL_PATH}")
    get_filename_component(DST_DIR "${DST_FILE}" DIRECTORY)
    file(MAKE_DIRECTORY "${DST_DIR}")
    file(COPY_FILE "${SRC_FILE}" "${DST_FILE}")
endforeach()

# 3. Synchronize version in BentoPack.uplugin if PACKAGE_VERSION is provided
if(DEFINED PACKAGE_VERSION AND NOT "${PACKAGE_VERSION}" STREQUAL "" AND EXISTS "${ADDON_DEST}/BentoPack.uplugin")
    file(READ "${ADDON_DEST}/BentoPack.uplugin" UPLUGIN_CONTENT)
    string(REGEX REPLACE "\"VersionName\":[ \t]*\"[^\"]+\"" "\"VersionName\": \"${PACKAGE_VERSION}\"" UPLUGIN_CONTENT "${UPLUGIN_CONTENT}")
    file(WRITE "${ADDON_DEST}/BentoPack.uplugin" "${UPLUGIN_CONTENT}")
endif()

# 4. Copy LICENSE
if(DEFINED LICENSE_FILE AND EXISTS "${LICENSE_FILE}")
    file(COPY_FILE "${LICENSE_FILE}" "${ADDON_DEST}/LICENSE.md")
endif()

# 5. Create ZIP archive
get_filename_component(OUTPUT_ZIP_DIR "${OUTPUT_ZIP}" DIRECTORY)
file(MAKE_DIRECTORY "${OUTPUT_ZIP_DIR}")

# Remove existing zip if any
if(EXISTS "${OUTPUT_ZIP}")
    file(REMOVE "${OUTPUT_ZIP}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar "cf" "${OUTPUT_ZIP}" --format=zip "BentoPack"
    WORKING_DIRECTORY "${STAGING_DIR}"
    RESULT_VARIABLE TAR_RES
)

if(NOT TAR_RES EQUAL 0)
    message(FATAL_ERROR "Failed to create Unreal plugin zip archive: ${OUTPUT_ZIP}")
endif()

message(STATUS "Unreal Engine 5 Plugin package created successfully: ${OUTPUT_ZIP}")
