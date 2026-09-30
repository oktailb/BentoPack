cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
endif()
if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

include(version.cmake)

project(BentoPack       VERSION ${PROJECT_VERSION}
                        LANGUAGES C CXX
                        DESCRIPTION "High-density 2D sprite sheet and polygonal mesh packing studio"
            )

include("build.cmake")
include(GNUInstallDirs)
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "bentopack")


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

find_package(WebP QUIET)
if(WebP_FOUND)
    message(STATUS "WebP found: ${WEBP_LIBRARIES}")
    add_compile_definitions(HAVE_LIBWEBP=1)
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
install(EXPORT BentoPackCoreTargets
    FILE BentoPackCoreTargets.cmake
    NAMESPACE BentoPack::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/BentoPack
    COMPONENT bentopack_dev
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/BentoPackConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/BentoPackConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/BentoPack
    COMPONENT bentopack_dev
)


add_subdirectory(docs)
add_subdirectory(addons)
include(install.cmake)
include(package.cmake)