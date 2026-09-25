set(CMAKE_AUTOMOC ON)

set(ALL_EXTRACTOR_PLUGINS
    extractor_godot
    extractor_unity
    extractor_unreal
    extractor_json
    extractor_gif
    extractor_spritesheet
)

set(ALL_FILTER_PLUGINS
    filter_backgroundremoval
    filter_despill
    filter_outline
    filter_colorswap
    filter_coloradjust
    filter_pixelrescale
    filter_retropalette
    filter_atlaspacking
    filter_tightpolygonpacking
)

set(PLUGIN_INCLUDES
    ${CMAKE_SOURCE_DIR}/plugins/extractors/godot
    ${CMAKE_SOURCE_DIR}/plugins/extractors/unity
    ${CMAKE_SOURCE_DIR}/plugins/extractors/unreal
    ${CMAKE_SOURCE_DIR}/plugins/extractors/json
    ${CMAKE_SOURCE_DIR}/plugins/extractors/gif
    ${CMAKE_SOURCE_DIR}/plugins/extractors/spritesheet
    ${CMAKE_SOURCE_DIR}/plugins/filters/backgroundremoval
    ${CMAKE_SOURCE_DIR}/plugins/filters/despill
    ${CMAKE_SOURCE_DIR}/plugins/filters/outline
    ${CMAKE_SOURCE_DIR}/plugins/filters/colorswap
    ${CMAKE_SOURCE_DIR}/plugins/filters/coloradjust
    ${CMAKE_SOURCE_DIR}/plugins/filters/pixelrescale
    ${CMAKE_SOURCE_DIR}/plugins/filters/retropalette
    ${CMAKE_SOURCE_DIR}/plugins/filters/atlaspacking
    ${CMAKE_SOURCE_DIR}/plugins/filters/tightpolygonpacking
)

# 1. test_extractors
add_executable(test_extractors test_extractors.cpp)
target_include_directories(test_extractors PRIVATE ${PLUGIN_INCLUDES})
target_compile_definitions(test_extractors PRIVATE SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample")
target_link_libraries(test_extractors PRIVATE BentoPackCore ${ALL_EXTRACTOR_PLUGINS} Qt6::Test)
set_target_properties(test_extractors PROPERTIES WIN32_EXECUTABLE FALSE)
if (MINGW)
    target_link_options(test_extractors PRIVATE -mconsole)
endif()
add_test(NAME test_extractors COMMAND test_extractors)

# 2. test_controllers
add_executable(test_controllers test_controllers.cpp)
target_include_directories(test_controllers PRIVATE ${PLUGIN_INCLUDES})
target_compile_definitions(test_controllers PRIVATE
    SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample"
    QM_DIR="${CMAKE_BINARY_DIR}/BentoPack"
)
target_link_libraries(test_controllers PRIVATE BentoPackCore ${ALL_EXTRACTOR_PLUGINS} ${ALL_FILTER_PLUGINS} Qt6::Test)
set_target_properties(test_controllers PROPERTIES WIN32_EXECUTABLE FALSE)
if (MINGW)
    target_link_options(test_controllers PRIVATE -mconsole)
endif()
add_test(NAME test_controllers COMMAND test_controllers)

# 3. test_project
add_executable(test_project test_project.cpp)
target_compile_definitions(test_project PRIVATE SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample")
target_link_libraries(test_project PRIVATE BentoPackCore Qt6::Test)
set_target_properties(test_project PROPERTIES WIN32_EXECUTABLE FALSE)
if (MINGW)
    target_link_options(test_project PRIVATE -mconsole)
endif()
add_test(NAME test_project COMMAND test_project)

# 4. test_core
add_executable(test_core test_core.cpp)
target_compile_definitions(test_core PRIVATE SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample")
target_link_libraries(test_core PRIVATE BentoPackCore Qt6::Test)
set_target_properties(test_core PROPERTIES WIN32_EXECUTABLE FALSE)
if (MINGW)
    target_link_options(test_core PRIVATE -mconsole)
endif()
add_test(NAME test_core COMMAND test_core)

# 5. test_cli
add_executable(test_cli test_cli.cpp)
target_compile_definitions(test_cli PRIVATE SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample")
target_link_libraries(test_cli PRIVATE BentoPackCore Qt6::Test)
set_target_properties(test_cli PROPERTIES WIN32_EXECUTABLE FALSE)
if (MINGW)
    target_link_options(test_cli PRIVATE -mconsole)
endif()
add_test(NAME test_cli COMMAND test_cli)

# 6. test_mesh
add_executable(test_mesh test_mesh.cpp)
target_include_directories(test_mesh PRIVATE ${PLUGIN_INCLUDES})
target_compile_definitions(test_mesh PRIVATE SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample")
target_link_libraries(test_mesh PRIVATE BentoPackCore ${ALL_EXTRACTOR_PLUGINS} filter_atlaspacking Qt6::Test)
set_target_properties(test_mesh PROPERTIES WIN32_EXECUTABLE FALSE)
if (MINGW)
    target_link_options(test_mesh PRIVATE -mconsole)
endif()
add_test(NAME test_mesh COMMAND test_mesh)

# 7. test_pixel_editor
add_executable(test_pixel_editor test_pixel_editor.cpp)
target_compile_definitions(test_pixel_editor PRIVATE SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample")
target_link_libraries(test_pixel_editor PRIVATE BentoPackCore Qt6::Test)
set_target_properties(test_pixel_editor PROPERTIES WIN32_EXECUTABLE FALSE)
if (MINGW)
    target_link_options(test_pixel_editor PRIVATE -mconsole)
endif()
add_test(NAME test_pixel_editor COMMAND test_pixel_editor)

# 8. test_vram_compression (M9)
add_executable(test_vram_compression test_vram_compression.cpp)
target_include_directories(test_vram_compression PRIVATE ${PLUGIN_INCLUDES})
target_compile_definitions(test_vram_compression PRIVATE SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample")
target_link_libraries(test_vram_compression PRIVATE BentoPackCore ${ALL_EXTRACTOR_PLUGINS} Qt6::Test)
set_target_properties(test_vram_compression PROPERTIES WIN32_EXECUTABLE FALSE)
if (MINGW)
    target_link_options(test_vram_compression PRIVATE -mconsole)
endif()
add_test(NAME test_vram_compression COMMAND test_vram_compression)

set_tests_properties(test_extractors test_controllers test_project test_core test_cli test_mesh test_pixel_editor test_vram_compression
    PROPERTIES ENVIRONMENT "SPRITESTUDIO_PLUGIN_PATH=${CMAKE_BINARY_DIR}/bin/plugins"
)

if(WIN32)
    # Copy plugin DLLs next to test executables in bin/ so Windows loader resolves them
    add_custom_command(
        TARGET test_extractors POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:extractor_godot>
            $<TARGET_FILE:extractor_unity>
            $<TARGET_FILE:extractor_unreal>
            $<TARGET_FILE:extractor_json>
            $<TARGET_FILE:extractor_gif>
            $<TARGET_FILE:extractor_spritesheet>
            "${CMAKE_BINARY_DIR}/bin"
        COMMENT "Copying extractor plugin DLLs to bin directory for tests"
    )

    add_custom_command(
        TARGET test_controllers POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:extractor_godot>
            $<TARGET_FILE:extractor_unity>
            $<TARGET_FILE:extractor_unreal>
            $<TARGET_FILE:extractor_json>
            $<TARGET_FILE:extractor_gif>
            $<TARGET_FILE:extractor_spritesheet>
            $<TARGET_FILE:filter_backgroundremoval>
            $<TARGET_FILE:filter_despill>
            $<TARGET_FILE:filter_outline>
            $<TARGET_FILE:filter_colorswap>
            $<TARGET_FILE:filter_coloradjust>
            $<TARGET_FILE:filter_pixelrescale>
            $<TARGET_FILE:filter_retropalette>
            $<TARGET_FILE:filter_atlaspacking>
            $<TARGET_FILE:filter_tightpolygonpacking>
            "${CMAKE_BINARY_DIR}/bin"
        COMMENT "Copying extractor and filter plugin DLLs to bin directory for test_controllers"
    )

    # Also prepend paths in CTest environment
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.22")
        set_tests_properties(test_extractors test_controllers test_project test_core test_cli test_mesh test_pixel_editor test_vram_compression
            PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/bin"
                "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/bin/plugins/extractors"
                "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/bin/plugins/filters"
        )
    endif()
endif()

