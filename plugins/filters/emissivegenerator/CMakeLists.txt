add_library(filter_emissivegenerator SHARED
    emissivegeneratorfilter.cpp
    emissivegeneratorfilter.h
    emissivegeneratordialog.cpp
    emissivegeneratordialog.h
)

target_include_directories(filter_emissivegenerator PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(filter_emissivegenerator PRIVATE
    BentoPackWidgets
    BentoPackCore
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

set_target_properties(filter_emissivegenerator PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/filters"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/filters"
)

install(TARGETS filter_emissivegenerator
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/bentopack/plugins/filters
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/plugins/filters
)
