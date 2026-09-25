add_library(filter_coloradjust SHARED
    coloradjustfilter.cpp
    coloradjustfilter.h
    coloradjustfilterdialog.cpp
    coloradjustfilterdialog.h
)

target_include_directories(filter_coloradjust PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(filter_coloradjust PRIVATE
    BentoPackCore
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

set_target_properties(filter_coloradjust PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/filters"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/filters"
)

install(TARGETS filter_coloradjust
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/bentopack/plugins/filters
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/plugins/filters
)
