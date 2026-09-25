add_library(extractor_gif SHARED
    gifextractor.cpp
    gifextractor.h
)

target_include_directories(extractor_gif PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(extractor_gif PRIVATE
    BentoPackCore
    Qt6::Core
    Qt6::Gui
)

set_target_properties(extractor_gif PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/extractors"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/extractors"
)

install(TARGETS extractor_gif
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/bentopack/plugins/extractors
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/plugins/extractors
)
