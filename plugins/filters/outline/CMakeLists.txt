add_library(filter_outline SHARED
    outlinefilter.cpp
    outlinefilter.h
    outlinefilterdialog.cpp
    outlinefilterdialog.h
)

target_include_directories(filter_outline PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(filter_outline PRIVATE
    SpriteStudioCore
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

set_target_properties(filter_outline PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/filters"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/filters"
)

install(TARGETS filter_outline
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/spritestudio/plugins/filters
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/plugins/filters
)
