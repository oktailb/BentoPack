add_library(filter_backgroundremoval SHARED
    backgroundremovalfilter.cpp
    backgroundremovalfilter.h
    backgroundremovaldialog.cpp
    backgroundremovaldialog.h
)

target_include_directories(filter_backgroundremoval PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(filter_backgroundremoval PRIVATE
    BentoPackCore
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

set_target_properties(filter_backgroundremoval PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/filters"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/filters"
)

install(TARGETS filter_backgroundremoval
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/bentopack/plugins/filters
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/plugins/filters
)
