add_library(extractor_casm SHARED
    casm_extractor.cpp
    casm_extractor.h
    casm_dialog.cpp
    casm_dialog.h
    casm_generator.cpp
    casm_generator.h
    casm_options.h
)

target_include_directories(extractor_casm PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(extractor_casm PRIVATE
    BentoPackCore
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

set_target_properties(extractor_casm PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/extractors"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/extractors"
)

install(TARGETS extractor_casm
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/bentopack/plugins/extractors
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/plugins/extractors
)
