add_library(filter_normalgenerator SHARED
    normalgeneratorfilter.cpp
    normalgeneratorfilter.h
    normalgeneratordialog.cpp
    normalgeneratordialog.h
)

target_include_directories(filter_normalgenerator PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(filter_normalgenerator PRIVATE
    BentoPackWidgets
    BentoPackCore
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

set_target_properties(filter_normalgenerator PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/filters"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/filters"
)

install(TARGETS filter_normalgenerator
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/bentopack/plugins/filters
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/plugins/filters
)
