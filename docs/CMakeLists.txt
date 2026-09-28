cmake_minimum_required(VERSION 3.20)

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
