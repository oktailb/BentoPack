add_library(filter_tightpolygonpacking SHARED
    tightpolygonpackingfilter.cpp
    tightpolygonpackingfilter.h
    ../atlaspacking/atlaspackingdialog.cpp
    ../atlaspacking/atlaspackingdialog.h
)

target_include_directories(filter_tightpolygonpacking PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../atlaspacking
)

target_link_libraries(filter_tightpolygonpacking PRIVATE
    BentoPackWidgets
    BentoPackCore
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
)

set_target_properties(filter_tightpolygonpacking PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/filters"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/filters"
)

install(TARGETS filter_tightpolygonpacking
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/bentopack/plugins/filters
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/plugins/filters
)
