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

project(SpriteStudio    VERSION ${PROJECT_VERSION}
                        LANGUAGES C CXX
                        DESCRIPTION "Little sprites and animated texture management tool"
            )

include("build.cmake")
include(GNUInstallDirs)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Concurrent
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
    message(STATUS "LibGit2 not found. Git versioning in .ssp projects is disabled.")
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
        message(STATUS "Basis Universal (KTX2 / UASTC / ETC1S) enabled successfully.")
        add_compile_definitions(HAVE_BASIS_UNIVERSAL=1)
        add_compile_definitions(BASISD_SUPPORT_KTX2_ZSTD=1)
        add_compile_definitions(BASISD_SUPPORT_KTX2=1)
    endif()
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(SpriteStudio)
add_subdirectory(plugins)

enable_testing()
add_subdirectory(tests)

install(TARGETS
    ${PROJECT_NAME}
    spritestudio-cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

include(CMakePackageConfigHelpers)

# Generate SpriteStudioConfigVersion.cmake
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/SpriteStudioConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Generate SpriteStudioConfig.cmake
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/SpriteStudioConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/SpriteStudioConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/SpriteStudio"
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR
)

# Install CMake SDK export for spritestudio-dev
install(EXPORT SpriteStudioTargets
    FILE SpriteStudioTargets.cmake
    NAMESPACE SpriteStudio::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/SpriteStudio"
    COMPONENT spritestudio-dev
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/SpriteStudioConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/SpriteStudioConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/SpriteStudio"
    COMPONENT spritestudio-dev
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

if(UNIX AND NOT APPLE AND NOT HAIKU)
    install(FILES SpriteStudio/res/linux/SpriteStudio.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
    )
    install(FILES SpriteStudio/res/icons/spritestudio.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps
    )
    install(FILES docs/man/spritestudio.1 docs/man/spritestudio-cli.1
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
        SpriteStudio/src/haiku.PackageInfo.in
        generated/haiku.PackageInfo
    )
endif()

set(CPACK_RESOURCE_FILE_LICENSE  "${CMAKE_CURRENT_LIST_DIR}/LICENSE")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_PACKAGE_CONTACT "vincent.lecoq@gmail.com")
set(CPACK_PACKAGE_VENDOR "Oktailb")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Animated Sprite Management and Assembly Tool")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}.${CMAKE_SYSTEM_PROCESSOR}")

# Linux CPack configuration
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Graphics")

# Windows CPack configuration
set(CPACK_NSIS_DISPLAY_NAME "Sprite Studio")
set(CPACK_NSIS_PACKAGE_NAME "Sprite Studio")
set(CPACK_NSIS_MODIFY_PATH ON)

# macOS CPack configuration
set(CPACK_DMG_VOLUME_NAME "Sprite Studio")
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

# CPack Component packaging for spritestudio & spritestudio-dev
set(CPACK_COMPONENTS_ALL spritestudio spritestudio-dev)
set(CPACK_COMPONENT_SPRITESTUDIO_DISPLAY_NAME "SpriteStudio Application and Plugins")
set(CPACK_COMPONENT_SPRITESTUDIO-DEV_DISPLAY_NAME "SpriteStudio Development SDK (Headers & CMake)")
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_RPM_COMPONENT_INSTALL ON)

include(CPack)
