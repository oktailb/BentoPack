add_library(extractor_aseprite SHARED
    asepriteextractor.cpp
    asepriteextractor.h
)

target_include_directories(extractor_aseprite PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(extractor_aseprite PRIVATE
    BentoPackCore
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

set_target_properties(extractor_aseprite PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/extractors"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/extractors"
)

install(TARGETS extractor_aseprite
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/bentopack/plugins/extractors
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/plugins/extractors
)
