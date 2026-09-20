add_library(extractor_json SHARED
    jsonextractor.cpp
    jsonextractor.h
    jsonExtractordialog.cpp
    jsonExtractordialog.h
    jsonExtractordialog.ui
)

target_include_directories(extractor_json PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(extractor_json PRIVATE
    SpriteStudioCore
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

set_target_properties(extractor_json PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/extractors"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/extractors"
)

install(TARGETS extractor_json
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/spritestudio/plugins/extractors
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/plugins/extractors
)
