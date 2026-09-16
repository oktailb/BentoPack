cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
endif()
if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 10)
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

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(SpriteStudio)

enable_testing()
add_subdirectory(tests)

install(TARGETS
    ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if (WIN32)
#    install(FILES ${CMAKE_CXX_COMPILER}/bin/*.dll

#        DESTINATION .
#    )
endif()

if(UNIX AND NOT APPLE AND NOT HAIKU)
    install(FILES SpriteStudio/res/linux/SpriteStudio.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
    )
    install(FILES SpriteStudio/res/icons/spritestudio.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps
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

include(CPack)
