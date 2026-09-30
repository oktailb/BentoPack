cmake_minimum_required(VERSION 3.20)

# -------------------------------------------------------------------------
# Godot 4 Addon Packaging Target
# -------------------------------------------------------------------------
set(GODOT_ADDON_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/godot")
set(GODOT_ADDON_STAGING_DIR "${CMAKE_CURRENT_BINARY_DIR}/addons_staging/godot/addons/bentopack")
set(GODOT_ADDON_DIST_DIR "${CMAKE_CURRENT_BINARY_DIR}/dist")
set(GODOT_ADDON_ZIP "${GODOT_ADDON_DIST_DIR}/godot-bentopack-addon-${PROJECT_VERSION}.zip")

add_custom_target(package_godot_addon
    COMMAND ${CMAKE_COMMAND}
        -DSRC_DIR="${GODOT_ADDON_SRC_DIR}"
        -DSTAGING_DIR="${CMAKE_CURRENT_BINARY_DIR}/addons_staging/godot"
        -DLICENSE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/LICENSE-ADDONS.md"
        -DOUTPUT_ZIP="${GODOT_ADDON_ZIP}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_godot_addon.cmake"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Packaging Godot 4 Addon into ${GODOT_ADDON_ZIP}..."
    VERBATIM
)

install(DIRECTORY godot/
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/bentopack/addons/godot/addons/bentopack
    COMPONENT bentopack
    PATTERN "*.uid" EXCLUDE
    PATTERN "*.import" EXCLUDE
)

message ("Godot Addon Configured")

# -------------------------------------------------------------------------
# Unity 6 Addon Packaging Target
# -------------------------------------------------------------------------
set(UNITY_ADDON_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/unity")
set(UNITY_ADDON_STAGING_DIR "${CMAKE_CURRENT_BINARY_DIR}/addons_staging/unity")
set(UNITY_ADDON_DIST_DIR "${CMAKE_CURRENT_BINARY_DIR}/dist")
set(UNITY_ADDON_TGZ "${UNITY_ADDON_DIST_DIR}/com.bentopack.importer-${PROJECT_VERSION}.tgz")

add_custom_target(package_unity_addon
    COMMAND ${CMAKE_COMMAND}
        -DSRC_DIR="${UNITY_ADDON_SRC_DIR}"
        -DSTAGING_DIR="${UNITY_ADDON_STAGING_DIR}"
        -DLICENSE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/LICENSE-ADDONS.md"
        -DPACKAGE_VERSION="${PROJECT_VERSION}"
        -DOUTPUT_TGZ="${UNITY_ADDON_TGZ}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_unity_addon.cmake"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Packaging Unity Addon into ${UNITY_ADDON_TGZ}..."
    VERBATIM
)

install(DIRECTORY unity/
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/bentopack/addons/unity/com.bentopack.importer
    COMPONENT bentopack
)

message ("Unity Addon Configured")