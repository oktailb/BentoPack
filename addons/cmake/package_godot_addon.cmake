# Packaging script for Godot 4 Addon
# Arguments:
#   SRC_DIR: Source directory of the Godot addon (e.g. ${PROJECT_SOURCE_DIR}/addons/godot)
#   STAGING_DIR: Temporary staging directory (e.g. ${CMAKE_BINARY_DIR}/addons_staging/godot)
#   LICENSE_FILE: Path to LICENSE file
#   OUTPUT_ZIP: Target zip path (e.g. ${CMAKE_BINARY_DIR}/dist/godot-bentopack-addon-${PROJECT_VERSION}.zip)

string(REPLACE "\"" "" SRC_DIR "${SRC_DIR}")
string(REPLACE "\"" "" STAGING_DIR "${STAGING_DIR}")
string(REPLACE "\"" "" LICENSE_FILE "${LICENSE_FILE}")
string(REPLACE "\"" "" OUTPUT_ZIP "${OUTPUT_ZIP}")

if(NOT DEFINED SRC_DIR OR NOT DEFINED STAGING_DIR OR NOT DEFINED OUTPUT_ZIP)
    message(FATAL_ERROR "SRC_DIR, STAGING_DIR, and OUTPUT_ZIP must be defined.")
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
    # Skip .uid, .import, and editor caches
    if(REL_PATH MATCHES "\\.uid$" OR REL_PATH MATCHES "\\.import$")
        continue()
    endif()

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

# 4. Create ZIP archive
get_filename_component(OUTPUT_ZIP_DIR "${OUTPUT_ZIP}" DIRECTORY)
file(MAKE_DIRECTORY "${OUTPUT_ZIP_DIR}")

# Remove existing zip if any
if(EXISTS "${OUTPUT_ZIP}")
    file(REMOVE "${OUTPUT_ZIP}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar "cf" "${OUTPUT_ZIP}" --format=zip "addons/bentopack"
    WORKING_DIRECTORY "${STAGING_DIR}"
    RESULT_VARIABLE TAR_RES
)

if(NOT TAR_RES EQUAL 0)
    message(FATAL_ERROR "Failed to create Godot addon zip archive: ${OUTPUT_ZIP}")
endif()

message(STATUS "Godot 4 Addon package created successfully: ${OUTPUT_ZIP}")
