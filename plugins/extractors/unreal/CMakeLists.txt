add_library(extractor_unreal SHARED
    unrealextractor.cpp
    unrealextractor.h
)

target_include_directories(extractor_unreal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(extractor_unreal PRIVATE
    SpriteStudioCore
    Qt6::Core
    Qt6::Gui
)

set_target_properties(extractor_unreal PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/extractors"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/extractors"
)

install(TARGETS extractor_unreal
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/spritestudio/plugins/extractors
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/plugins/extractors
)
