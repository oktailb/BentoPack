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
        -DLICENSE_FILE="${PROJECT_SOURCE_DIR}/LICENSE"
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
        -DLICENSE_FILE="${PROJECT_SOURCE_DIR}/LICENSE"
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

# -------------------------------------------------------------------------
# Unreal Engine 5 Plugin Packaging Target
# -------------------------------------------------------------------------
set(UNREAL_ADDON_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/unreal")
set(UNREAL_ADDON_STAGING_DIR "${CMAKE_CURRENT_BINARY_DIR}/addons_staging/unreal")
set(UNREAL_ADDON_DIST_DIR "${CMAKE_CURRENT_BINARY_DIR}/dist")
set(UNREAL_ADDON_ZIP "${UNREAL_ADDON_DIST_DIR}/unreal-bentopack-plugin-${PROJECT_VERSION}.zip")

add_custom_target(package_unreal_addon
    COMMAND ${CMAKE_COMMAND}
        -DSRC_DIR="${UNREAL_ADDON_SRC_DIR}"
        -DSTAGING_DIR="${UNREAL_ADDON_STAGING_DIR}"
        -DLICENSE_FILE="${PROJECT_SOURCE_DIR}/LICENSE"
        -DPACKAGE_VERSION="${PROJECT_VERSION}"
        -DOUTPUT_ZIP="${UNREAL_ADDON_ZIP}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_unreal_addon.cmake"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Packaging Unreal Engine 5 Plugin into ${UNREAL_ADDON_ZIP}..."
    VERBATIM
)

# -------------------------------------------------------------------------
# Unreal Engine 5 Plugin Build Target (RunUAT)
# -------------------------------------------------------------------------
if(WIN32)
    set(RUN_UAT_NAME "RunUAT.bat")
    set(DEFAULT_HOST_PLATFORM "Win64")
elseif(APPLE)
    set(RUN_UAT_NAME "RunUAT.sh")
    set(DEFAULT_HOST_PLATFORM "Mac")
else()
    set(RUN_UAT_NAME "RunUAT.sh")
    set(DEFAULT_HOST_PLATFORM "Linux")
endif()

find_program(UNREAL_RUN_UAT
    NAMES ${RUN_UAT_NAME}
    PATHS
        "$ENV{UNREAL_ENGINE_PATH}/Engine/Build/BatchFiles"
        "$ENV{UE_ENGINE_PATH}/Engine/Build/BatchFiles"
        "/opt/unreal-engine/Engine/Build/BatchFiles"
        "/opt/unreal-engine-5/Engine/Build/BatchFiles"
        "$ENV{HOME}/UE5/Engine/Build/BatchFiles"
    DOC "Path to Unreal Engine RunUAT tool"
)

set(UNREAL_BUILD_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/addons_build/unreal")

if(UNREAL_RUN_UAT)
    message(STATUS "Found Unreal Engine RunUAT: ${UNREAL_RUN_UAT}")

    add_custom_target(build_unreal_addon
        COMMAND ${CMAKE_COMMAND} -E echo "==> Compiling BentoPack Unreal Engine Plugin via RunUAT..."
        COMMAND ${CMAKE_COMMAND} -E make_directory "${UNREAL_BUILD_OUTPUT_DIR}"
        COMMAND ${UNREAL_RUN_UAT} BuildPlugin
            -Plugin=${UNREAL_ADDON_SRC_DIR}/BentoPack.uplugin
            -Package=${UNREAL_BUILD_OUTPUT_DIR}
            -TargetPlatforms=${DEFAULT_HOST_PLATFORM}
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT "Compiling BentoPack Unreal Engine 5 plugin binaries..."
        VERBATIM
    )

    if(DEFINED UNREAL_PROJECT_DIR)
        add_custom_command(TARGET build_unreal_addon POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "Deploying plugin to Unreal project..."
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${UNREAL_BUILD_OUTPUT_DIR}"
                "${UNREAL_PROJECT_DIR}/Plugins/BentoPack"
            VERBATIM
        )
    endif()
else()
    add_custom_target(build_unreal_addon
        COMMAND ${CMAKE_COMMAND} -E echo "Error: Unreal Engine RunUAT tool not found."
        COMMAND ${CMAKE_COMMAND} -E echo "Please set UNREAL_ENGINE_PATH or UE_ENGINE_PATH to your Unreal Engine directory."
        COMMAND ${CMAKE_COMMAND} -E false
        COMMENT "Unreal Engine RunUAT not found - cannot compile plugin"
        VERBATIM
    )
endif()

install(DIRECTORY unreal/
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/bentopack/addons/unreal/BentoPack
    COMPONENT bentopack
    PATTERN "Intermediate" EXCLUDE
    PATTERN "Binaries" EXCLUDE
)

message ("Unreal Engine 5 Addon Configured")

# -------------------------------------------------------------------------
# Meta-target to package all engine addons
# -------------------------------------------------------------------------
add_custom_target(package_all_addons
    DEPENDS package_godot_addon package_unity_addon package_unreal_addon
    COMMENT "Packaging all BentoPack engine plugins (Godot 4, Unity 6, Unreal Engine 5)..."
)