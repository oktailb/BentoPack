add_library(extractor_unity SHARED
    unityextractor.cpp
    unityextractor.h
)

target_include_directories(extractor_unity PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(extractor_unity PRIVATE
    BentoPackCore
    Qt6::Core
    Qt6::Gui
)

set_target_properties(extractor_unity PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/extractors"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/extractors"
)

install(TARGETS extractor_unity
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/bentopack/plugins/extractors
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/plugins/extractors
)
