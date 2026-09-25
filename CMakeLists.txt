cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
endif()
if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 11)
set(PROJECT_VERSION_PATCH 0)
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

project(BentoPack       VERSION ${PROJECT_VERSION}
                        LANGUAGES C CXX
                        DESCRIPTION "High-density 2D sprite sheet and polygonal mesh packing studio"
            )

include("build.cmake")
include(GNUInstallDirs)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Concurrent
    Network
    LinguistTools
    Test
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if(NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif()
if(NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif()
if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(LibGit2 QUIET)
if(LibGit2_FOUND)
    message(STATUS "LibGit2 found: ${LIBGIT2_LIBRARIES} (includes: ${LIBGIT2_INCLUDE_DIRS})")
    add_compile_definitions(HAVE_LIBGIT2=1)
else()
    message(STATUS "LibGit2 not found. Git versioning in .bento projects is disabled.")
    message(STATUS "To enable LibGit2, configure with -DLIBGIT2_ROOT=<path_to_libgit2>")
endif()

# M9: VRAM Hardware Texture Compression (Basis Universal / KTX2)
option(ENABLE_VRAM_COMPRESSION "Enable KTX2 / Basis Universal VRAM texture compression" ON)
if(ENABLE_VRAM_COMPRESSION)
    set(BASISU_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BASISU_BUILD_PYTHON OFF CACHE BOOL "" FORCE)
    set(BASISU_SUPPORT_ASTCENC OFF CACHE BOOL "" FORCE)
    set(BASISU_ZSTD ON CACHE BOOL "" FORCE)
    set(BASISU_BUILD_X64 ON CACHE BOOL "" FORCE)

    # Provide compat include for platforms missing <sys/timex.h> (e.g. Haiku OS)
    include(CheckIncludeFile)
    check_include_file("sys/timex.h" HAVE_SYS_TIMEX_H)
    if(HAIKU OR NOT HAVE_SYS_TIMEX_H)
        include_directories(BEFORE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/compat/sys_timex")
    endif()

    if(DEFINED BASIS_UNIVERSAL_DIR AND EXISTS "${BASIS_UNIVERSAL_DIR}/CMakeLists.txt")
        message(STATUS "Using Basis Universal from BASIS_UNIVERSAL_DIR: ${BASIS_UNIVERSAL_DIR}")
        add_subdirectory(${BASIS_UNIVERSAL_DIR} basis_universal EXCLUDE_FROM_ALL)
        set(basis_universal_SOURCE_DIR "${BASIS_UNIVERSAL_DIR}")
    elseif(EXISTS "cots/basis_universal/CMakeLists.txt")
        message(STATUS "Using Basis Universal from local cache")
        set(LOCAL_BASIS_DIR "cots/basis_universal")
        add_subdirectory(${LOCAL_BASIS_DIR} basis_universal EXCLUDE_FROM_ALL)
        set(basis_universal_SOURCE_DIR "${LOCAL_BASIS_DIR}")
    else()
        include(FetchContent)
        message(STATUS "Fetching Basis Universal (Khronos Group)...")
        FetchContent_Declare(
            basis_universal
            GIT_REPOSITORY https://github.com/BinomialLLC/basis_universal.git
            GIT_TAG v2_50
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(basis_universal)
    endif()

    if(TARGET basisu_encoder)
        if(HAIKU OR NOT HAVE_SYS_TIMEX_H)
            target_include_directories(basisu_encoder BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/compat/sys_timex")
        endif()
        message(STATUS "Basis Universal (KTX2 / UASTC / ETC1S) enabled successfully.")
        add_compile_definitions(HAVE_BASIS_UNIVERSAL=1)
        add_compile_definitions(BASISD_SUPPORT_KTX2_ZSTD=1)
        add_compile_definitions(BASISD_SUPPORT_KTX2=1)
    endif()
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
if(MINGW)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--export-all-symbols")
endif()

add_subdirectory(BentoPack)
add_subdirectory(plugins)

enable_testing()
add_subdirectory(tests)

include(CMakePackageConfigHelpers)

# Generate BentoPackConfigVersion.cmake
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/BentoPackConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Generate BentoPackConfig.cmake
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/BentoPackConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/BentoPackConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BentoPack"
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR
)

# Install CMake SDK export for bentopack-dev
install(EXPORT BentoPackTargets
    FILE BentoPackTargets.cmake
    NAMESPACE BentoPack::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BentoPack"
    COMPONENT bentopack-dev
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/BentoPackConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/BentoPackConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BentoPack"
    COMPONENT bentopack-dev
)

if (WIN32)
#    install(FILES ${CMAKE_CXX_COMPILER}/bin/*.dll

#        DESTINATION .
#    )
endif()

find_package(Doxygen QUIET OPTIONAL_COMPONENTS dot)
if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    if(DOXYGEN_DOT_FOUND)
        set(DOXYGEN_HAVE_DOT "YES")
    else()
        set(DOXYGEN_HAVE_DOT "NO")
    endif()

    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    add_custom_target(doxygen
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen..."
        VERBATIM
    )
    add_custom_target(docs DEPENDS doxygen)
    message(STATUS "Doxygen found: ${DOXYGEN_EXECUTABLE} (documentation target 'doxygen' / 'docs' available)")
else()
    message(STATUS "Doxygen not found. Documentation targets will not be built.")
endif()

# -------------------------------------------------------------------------
# Godot 4 Addon Packaging Target
# -------------------------------------------------------------------------
set(GODOT_ADDON_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/addons/godot")
set(GODOT_ADDON_STAGING_DIR "${CMAKE_CURRENT_BINARY_DIR}/addons_staging/godot/addons/bentopack")
set(GODOT_ADDON_DIST_DIR "${CMAKE_CURRENT_BINARY_DIR}/dist")
set(GODOT_ADDON_ZIP "${GODOT_ADDON_DIST_DIR}/godot-bentopack-addon-${PROJECT_VERSION}.zip")

add_custom_target(package_godot_addon
    COMMAND ${CMAKE_COMMAND}
        -DSRC_DIR="${GODOT_ADDON_SRC_DIR}"
        -DSTAGING_DIR="${CMAKE_CURRENT_BINARY_DIR}/addons_staging/godot"
        -DLICENSE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        -DOUTPUT_ZIP="${GODOT_ADDON_ZIP}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_godot_addon.cmake"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Packaging Godot 4 Addon into ${GODOT_ADDON_ZIP}..."
    VERBATIM
)

install(DIRECTORY addons/godot/
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/bentopack/addons/godot/addons/bentopack
    COMPONENT bentopack
    PATTERN "*.uid" EXCLUDE
    PATTERN "*.import" EXCLUDE
)


if(UNIX AND NOT APPLE AND NOT HAIKU)
    install(FILES BentoPack/res/linux/BentoPack.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
        RENAME bentopack.desktop
    )
    install(FILES BentoPack/res/icons/bentopack.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps
        RENAME bentopack.png
    )
    install(FILES
        docs/man/bentopack.1 docs/man/bentopack-cli.1
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
    )
endif()

if (HAIKU)
    message("This is haiku")
    install(FILES ${CMAKE_CURRENT_LIST_DIR}/LICENSE
        DESTINATION data/licenses
        RENAME "Apache License Version 2.0"
    )
    install(FILES ${CMAKE_CURRENT_LIST_DIR}/LICENSE
        DESTINATION data/licenses
        RENAME "Apache v2"
    )
    configure_file(
        BentoPack/src/haiku.PackageInfo.in
        generated/haiku.PackageInfo
    )
endif()

set(CPACK_RESOURCE_FILE_LICENSE  "${CMAKE_CURRENT_LIST_DIR}/LICENSE")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_PACKAGE_CONTACT "vincent.lecoq@gmail.com")
set(CPACK_PACKAGE_VENDOR "Oktailb")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "BentoPack - High-density 2D sprite sheet and polygonal mesh studio")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}.${CMAKE_SYSTEM_PROCESSOR}")

# Linux CPack configuration
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_RPM_PACKAGE_LICENSE "Apache-2.0")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Graphics")

# Windows CPack configuration
set(CPACK_NSIS_DISPLAY_NAME "BentoPack Studio")
set(CPACK_NSIS_PACKAGE_NAME "BentoPack Studio")
set(CPACK_NSIS_MODIFY_PATH ON)

# macOS CPack configuration
set(CPACK_DMG_VOLUME_NAME "BentoPack Studio")
set(CPACK_DMG_FORMAT "UDZO")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CPACK_GENERATOR "TGZ;RPM;DEB")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(CPACK_GENERATOR "ZIP;NSIS")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(CPACK_GENERATOR "DragNDrop;TGZ")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

# CPack Component packaging for bentopack & bentopack-dev
set(CPACK_COMPONENTS_ALL bentopack bentopack-dev)
set(CPACK_COMPONENT_BENTOPACK_DISPLAY_NAME "BentoPack Application and Plugins")
set(CPACK_COMPONENT_BENTOPACK-DEV_DISPLAY_NAME "BentoPack Development SDK (Headers & CMake)")
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_RPM_COMPONENT_INSTALL ON)

include(CPack)
