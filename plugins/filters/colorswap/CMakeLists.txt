add_library(filter_colorswap SHARED
    colorswapfilter.cpp
    colorswapfilter.h
    colorswapfilterdialog.cpp
    colorswapfilterdialog.h
)

target_include_directories(filter_colorswap PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(filter_colorswap PRIVATE
    BentoPackWidgets
    BentoPackCore
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

set_target_properties(filter_colorswap PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/filters"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/filters"
)

install(TARGETS filter_colorswap
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/bentopack/plugins/filters
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/plugins/filters
)
