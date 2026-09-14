# FindLibGit2.cmake
# Finds the LibGit2 library.
#
# Inputs:
#   LIBGIT2_ROOT or LIBGIT2_DIR or LibGit2_ROOT or LibGit2_DIR (CMake variable or Environment variable)
#
# Outputs:
#   LibGit2_FOUND
#   LibGit2_INCLUDE_DIRS
#   LibGit2_LIBRARIES
#   LibGit2::LibGit2 (imported target)

if(POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
endif()
if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

# Gather candidate search roots
set(_LIBGIT2_CANDIDATE_ROOTS)
foreach(_VAR IN ITEMS
    LIBGIT2_ROOT LIBGIT2_DIR LibGit2_ROOT LibGit2_DIR
    ENV LIBGIT2_ROOT ENV LIBGIT2_DIR ENV LibGit2_ROOT ENV LibGit2_DIR)
    if(DEFINED ${_VAR} AND NOT "${${_VAR}}" STREQUAL "")
        list(APPEND _LIBGIT2_CANDIDATE_ROOTS "${${_VAR}}")
    endif()
endforeach()
list(REMOVE_DUPLICATES _LIBGIT2_CANDIDATE_ROOTS)

# Collect all potential search directories (source tree, build directories, install prefixes)
set(_LIBGIT2_SEARCH_DIRS)
foreach(_ROOT IN LISTS _LIBGIT2_CANDIDATE_ROOTS)
    list(APPEND _LIBGIT2_SEARCH_DIRS "${_ROOT}")
    list(APPEND _LIBGIT2_SEARCH_DIRS "${_ROOT}/build")
    # Detect build subdirectories (e.g. build/Desktop_Qt_... or build/Release, build/Debug)
    file(GLOB _BUILD_SUBDIRS LIST_DIRECTORIES true "${_ROOT}/build/*")
    foreach(_DIR IN LISTS _BUILD_SUBDIRS)
        if(IS_DIRECTORY "${_DIR}")
            list(APPEND _LIBGIT2_SEARCH_DIRS "${_DIR}")
        endif()
    endforeach()
    # In case user pointed directly into a build directory, also check parent
    list(APPEND _LIBGIT2_SEARCH_DIRS "${_ROOT}/..")
endforeach()

find_path(LIBGIT2_INCLUDE_DIR
    NAMES git2.h
    HINTS ${_LIBGIT2_SEARCH_DIRS}
    PATH_SUFFIXES
        include
        gen_headers
)

find_library(LIBGIT2_LIBRARY
    NAMES git2 libgit2
    HINTS ${_LIBGIT2_SEARCH_DIRS}
    PATH_SUFFIXES
        lib
        bin
        .
        Release
        Debug
)

# On Windows, locate the shared runtime DLL if present
if(WIN32)
    find_file(LIBGIT2_DLL
        NAMES libgit2.dll git2.dll
        HINTS ${_LIBGIT2_SEARCH_DIRS}
        PATH_SUFFIXES
            .
            bin
            lib
            Release
            Debug
    )
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibGit2
    DEFAULT_MSG
    LIBGIT2_LIBRARY
    LIBGIT2_INCLUDE_DIR
)

if(LibGit2_FOUND)
    set(LIBGIT2_LIBRARIES ${LIBGIT2_LIBRARY})
    set(LIBGIT2_INCLUDE_DIRS ${LIBGIT2_INCLUDE_DIR})

    # If build directory also contains generated headers, add it
    foreach(_DIR IN LISTS _LIBGIT2_SEARCH_DIRS)
        if(EXISTS "${_DIR}/gen_headers")
            list(APPEND LIBGIT2_INCLUDE_DIRS "${_DIR}/gen_headers")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES LIBGIT2_INCLUDE_DIRS)

    if(NOT TARGET LibGit2::LibGit2)
        if(WIN32 AND LIBGIT2_DLL)
            add_library(LibGit2::LibGit2 SHARED IMPORTED)
            set_target_properties(LibGit2::LibGit2 PROPERTIES
                IMPORTED_IMPLIB "${LIBGIT2_LIBRARY}"
                IMPORTED_LOCATION "${LIBGIT2_DLL}"
                INTERFACE_INCLUDE_DIRECTORIES "${LIBGIT2_INCLUDE_DIRS}"
            )
        else()
            add_library(LibGit2::LibGit2 UNKNOWN IMPORTED)
            set_target_properties(LibGit2::LibGit2 PROPERTIES
                IMPORTED_LOCATION "${LIBGIT2_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${LIBGIT2_INCLUDE_DIRS}"
            )
        endif()

        if(WIN32)
            set_property(TARGET LibGit2::LibGit2 APPEND PROPERTY
                INTERFACE_LINK_LIBRARIES "rpcrt4;crypt32;ole32;ws2_32;secur32"
            )
        endif()
    endif()

    # Automatically copy DLL to runtime output folder for seamless execution
    if(WIN32 AND LIBGIT2_DLL AND DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
        file(MAKE_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
        file(COPY "${LIBGIT2_DLL}" DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    endif()
endif()

mark_as_advanced(LIBGIT2_INCLUDE_DIR LIBGIT2_LIBRARY LIBGIT2_DLL)

