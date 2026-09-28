# Packaging script for Unity 6 Addon
# Arguments:
#   SRC_DIR: Source directory of the Godot addon (e.g. ${PROJECT_SOURCE_DIR}/addons/unity)
#   STAGING_DIR: Temporary staging directory (e.g. ${CMAKE_BINARY_DIR}/addons_staging/unity)
#   LICENSE_FILE: Path to LICENSE file
#   OUTPUT_TGZ: Target tgz path (e.g. ${CMAKE_BINARY_DIR}/dist/unity-bentopack-addon-${PROJECT_VERSION}.tgz)

string(REPLACE "\"" "" SRC_DIR "${SRC_DIR}")
string(REPLACE "\"" "" STAGING_DIR "${STAGING_DIR}")
string(REPLACE "\"" "" LICENSE_FILE "${LICENSE_FILE}")
string(REPLACE "\"" "" OUTPUT_TGZ "${OUTPUT_TGZ}")

if(NOT DEFINED SRC_DIR OR NOT DEFINED STAGING_DIR OR NOT DEFINED OUTPUT_TGZ)
    message(FATAL_ERROR "SRC_DIR, STAGING_DIR, and OUTPUT_TGZ must be defined.")
endif()

set(ADDON_DEST "${STAGING_DIR}/addons/bentopack")

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

# 3. Copy LICENSE
if(DEFINED LICENSE_FILE AND EXISTS "${LICENSE_FILE}")
    file(COPY_FILE "${LICENSE_FILE}" "${ADDON_DEST}/LICENSE")
endif()

# 4. Create TGZ archive
get_filename_component(OUTPUT_TGZ_DIR "${OUTPUT_TGZ}" DIRECTORY)
file(MAKE_DIRECTORY "${OUTPUT_TGZ_DIR}")

# Remove existing tgz if any
if(EXISTS "${OUTPUT_TGZ}")
    file(REMOVE "${OUTPUT_TGZ}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar "zcf" "${OUTPUT_TGZ}" "addons/bentopack"
    WORKING_DIRECTORY "${STAGING_DIR}"
    RESULT_VARIABLE TAR_RES
)

if(NOT TAR_RES EQUAL 0)
    message(FATAL_ERROR "Failed to create Unity addon tgz archive: ${OUTPUT_TGZ}")
endif()

message(STATUS "Unity 6 Addon package created successfully: ${OUTPUT_TGZ}")
