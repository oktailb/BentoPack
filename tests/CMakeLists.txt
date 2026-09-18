set(CMAKE_AUTOMOC ON)

# 1. test_extractors
add_executable(test_extractors test_extractors.cpp)
target_compile_definitions(test_extractors PRIVATE SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample")
target_link_libraries(test_extractors PRIVATE SpriteStudioCore Qt6::Test)
set_target_properties(test_extractors PROPERTIES WIN32_EXECUTABLE FALSE)
if (MINGW)
    target_link_options(test_extractors PRIVATE -mconsole)
endif()
add_test(NAME test_extractors COMMAND test_extractors)

# 2. test_controllers
add_executable(test_controllers test_controllers.cpp)
target_compile_definitions(test_controllers PRIVATE
    SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample"
    QM_DIR="${CMAKE_BINARY_DIR}/SpriteStudio"
)
target_link_libraries(test_controllers PRIVATE SpriteStudioCore Qt6::Test)
set_target_properties(test_controllers PROPERTIES WIN32_EXECUTABLE FALSE)
if (MINGW)
    target_link_options(test_controllers PRIVATE -mconsole)
endif()
add_test(NAME test_controllers COMMAND test_controllers)

# 3. test_project
add_executable(test_project test_project.cpp)
target_compile_definitions(test_project PRIVATE SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample")
target_link_libraries(test_project PRIVATE SpriteStudioCore Qt6::Test)
set_target_properties(test_project PROPERTIES WIN32_EXECUTABLE FALSE)
if (MINGW)
    target_link_options(test_project PRIVATE -mconsole)
endif()
add_test(NAME test_project COMMAND test_project)

# 4. test_core
add_executable(test_core test_core.cpp)
target_compile_definitions(test_core PRIVATE SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample")
target_link_libraries(test_core PRIVATE SpriteStudioCore Qt6::Test)
set_target_properties(test_core PROPERTIES WIN32_EXECUTABLE FALSE)
if (MINGW)
    target_link_options(test_core PRIVATE -mconsole)
endif()
add_test(NAME test_core COMMAND test_core)

# 5. test_cli
add_executable(test_cli test_cli.cpp)
target_compile_definitions(test_cli PRIVATE SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample")
target_link_libraries(test_cli PRIVATE SpriteStudioCore Qt6::Test)
set_target_properties(test_cli PROPERTIES WIN32_EXECUTABLE FALSE)
if (MINGW)
    target_link_options(test_cli PRIVATE -mconsole)
endif()
add_test(NAME test_cli COMMAND test_cli)

# 6. test_mesh
add_executable(test_mesh test_mesh.cpp)
target_compile_definitions(test_mesh PRIVATE SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample")
target_link_libraries(test_mesh PRIVATE SpriteStudioCore Qt6::Test)
set_target_properties(test_mesh PROPERTIES WIN32_EXECUTABLE FALSE)
if (MINGW)
    target_link_options(test_mesh PRIVATE -mconsole)
endif()
add_test(NAME test_mesh COMMAND test_mesh)



