add_library(filter_despill SHARED
    despillfilter.cpp
    despillfilter.h
    despillfilterdialog.cpp
    despillfilterdialog.h
)

target_include_directories(filter_despill PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(filter_despill PRIVATE
    SpriteStudioCore
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

set_target_properties(filter_despill PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/filters"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/filters"
)

install(TARGETS filter_despill
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/spritestudio/plugins/filters
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/plugins/filters
)
