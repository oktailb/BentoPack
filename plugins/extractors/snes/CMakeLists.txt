add_library(extractor_snes SHARED
    snes_extractor.cpp
    snes_extractor.h
    snes_dialog.cpp
    snes_dialog.h
    snespreset.cpp
    snespreset.h
    snesromanalyzer.cpp
    snesromanalyzer.h
    snessavestateloader.cpp
    snessavestateloader.h
)

target_include_directories(extractor_snes PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(extractor_snes PRIVATE
    BentoPackCore
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

set_target_properties(extractor_snes PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/extractors"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/extractors"
)

install(TARGETS extractor_snes
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/bentopack/plugins/extractors
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/plugins/extractors
)
