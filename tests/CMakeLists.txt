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

# 2. test_app_config
add_executable(test_app_config test_app_config.cpp)
target_link_libraries(test_app_config PRIVATE BentoPackCore Qt6::Test)
set_target_properties(test_app_config PROPERTIES WIN32_EXECUTABLE FALSE)
if (MINGW)
    target_link_options(test_app_config PRIVATE -mconsole)
endif()
add_test(NAME test_app_config COMMAND test_app_config)

# 3. test_controller_project
add_executable(test_controller_project test_controller_project.cpp)
target_include_directories(test_controller_project PRIVATE ${PLUGIN_INCLUDES})
target_compile_definitions(test_controller_project PRIVATE SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample")
target_link_libraries(test_controller_project PRIVATE BentoPackWidgets BentoPackCore ${ALL_EXTRACTOR_PLUGINS} Qt6::Test)
set_target_properties(test_controller_project PROPERTIES WIN32_EXECUTABLE FALSE)
if (MINGW)
    target_link_options(test_controller_project PRIVATE -mconsole)
endif()
add_test(NAME test_controller_project COMMAND test_controller_project)

# 4. test_controller_animation
add_executable(test_controller_animation test_controller_animation.cpp)
target_compile_definitions(test_controller_animation PRIVATE SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample")
target_link_libraries(test_controller_animation PRIVATE BentoPackWidgets BentoPackCore Qt6::Test)
set_target_properties(test_controller_animation PROPERTIES WIN32_EXECUTABLE FALSE)
if (MINGW)
    target_link_options(test_controller_animation PRIVATE -mconsole)
endif()
add_test(NAME test_controller_animation COMMAND test_controller_animation)

# 5. test_controller_atlas
add_executable(test_controller_atlas test_controller_atlas.cpp)
target_compile_definitions(test_controller_atlas PRIVATE
    SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample"
    QM_DIR="${CMAKE_BINARY_DIR}/BentoPack"
)
target_link_libraries(test_controller_atlas PRIVATE BentoPackWidgets BentoPackCore Qt6::Test)
set_target_properties(test_controller_atlas PROPERTIES WIN32_EXECUTABLE FALSE)
if (MINGW)
    target_link_options(test_controller_atlas PRIVATE -mconsole)
endif()
add_test(NAME test_controller_atlas COMMAND test_controller_atlas)

# 6. test_filters
add_executable(test_filters test_filters.cpp)
target_include_directories(test_filters PRIVATE ${PLUGIN_INCLUDES})
target_compile_definitions(test_filters PRIVATE SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample")
target_link_libraries(test_filters PRIVATE BentoPackWidgets BentoPackCore ${ALL_EXTRACTOR_PLUGINS} ${ALL_FILTER_PLUGINS} Qt6::Test)
set_target_properties(test_filters PROPERTIES WIN32_EXECUTABLE FALSE)
if (MINGW)
    target_link_options(test_filters PRIVATE -mconsole)
endif()
add_test(NAME test_filters COMMAND test_filters)

# 3. test_project
add_executable(test_project test_project.cpp)
target_compile_definitions(test_project PRIVATE SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample")
target_link_libraries(test_project PRIVATE BentoPackWidgets BentoPackCore Qt6::Test)
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
target_link_libraries(test_mesh PRIVATE BentoPackWidgets BentoPackCore ${ALL_EXTRACTOR_PLUGINS} filter_atlaspacking Qt6::Test)
set_target_properties(test_mesh PROPERTIES WIN32_EXECUTABLE FALSE)
if (MINGW)
    target_link_options(test_mesh PRIVATE -mconsole)
endif()
add_test(NAME test_mesh COMMAND test_mesh)

# 7. test_pixel_editor
add_executable(test_pixel_editor test_pixel_editor.cpp)
target_compile_definitions(test_pixel_editor PRIVATE SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample")
target_link_libraries(test_pixel_editor PRIVATE BentoPackWidgets BentoPackCore Qt6::Test)
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

# 9. test_robustness
add_executable(test_robustness test_robustness.cpp)
target_include_directories(test_robustness PRIVATE ${PLUGIN_INCLUDES})
target_compile_definitions(test_robustness PRIVATE SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample")
target_link_libraries(test_robustness PRIVATE BentoPackWidgets BentoPackCore ${ALL_EXTRACTOR_PLUGINS} Qt6::Test)
set_target_properties(test_robustness PROPERTIES WIN32_EXECUTABLE FALSE)
if (MINGW)
    target_link_options(test_robustness PRIVATE -mconsole)
endif()
add_test(NAME test_robustness COMMAND test_robustness)

# 10. test_gui_integration
add_executable(test_gui_integration test_gui_integration.cpp)
target_compile_definitions(test_gui_integration PRIVATE SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample")
target_link_libraries(test_gui_integration PRIVATE BentoPackWidgets BentoPackCore Qt6::Test)
set_target_properties(test_gui_integration PROPERTIES WIN32_EXECUTABLE FALSE)
if (MINGW)
    target_link_options(test_gui_integration PRIVATE -mconsole)
endif()
add_test(NAME test_gui_integration COMMAND test_gui_integration)

# 11. test_concurrency_and_security
add_executable(test_concurrency_and_security test_concurrency_and_security.cpp)
target_include_directories(test_concurrency_and_security PRIVATE ${PLUGIN_INCLUDES})
target_compile_definitions(test_concurrency_and_security PRIVATE SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample")
target_link_libraries(test_concurrency_and_security PRIVATE BentoPackWidgets BentoPackCore ${ALL_EXTRACTOR_PLUGINS} Qt6::Test)
set_target_properties(test_concurrency_and_security PROPERTIES WIN32_EXECUTABLE FALSE)
if (MINGW)
    target_link_options(test_concurrency_and_security PRIVATE -mconsole)
endif()
add_test(NAME test_concurrency_and_security COMMAND test_concurrency_and_security)

set(ALL_TEST_TARGETS
    test_extractors
    test_app_config
    test_controller_project
    test_controller_animation
    test_controller_atlas
    test_filters
    test_project
    test_core
    test_cli
    test_mesh
    test_pixel_editor
    test_vram_compression
    test_robustness
    test_gui_integration
    test_concurrency_and_security
)

set_tests_properties(${ALL_TEST_TARGETS}
    PROPERTIES ENVIRONMENT "BENTOPACK_PLUGIN_PATH=${CMAKE_BINARY_DIR}/bin/plugins"
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
        TARGET test_filters POST_BUILD
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
        COMMENT "Copying extractor and filter plugin DLLs to bin directory for test_filters"
    )

    # Also prepend paths in CTest environment
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.22")
        set_tests_properties(${ALL_TEST_TARGETS}
            PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/bin"
                "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/bin/plugins/extractors"
                "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/bin/plugins/filters"
        )
    endif()
endif()

