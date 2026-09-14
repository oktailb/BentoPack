set(CMAKE_AUTOMOC ON)

add_executable(test_extractors
    test_extractors.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/model/spritedocument.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/packer/atlaspacker.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/extractor.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/extractor/extractorregistry.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/extractor/spriteextractor.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/extractor/gifextractor.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/extractor/jsonextractor.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/extractor/godotextractor.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/image/spritedetector.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/config/appconfig.cpp

    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/model/spritedocument.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/packer/atlaspacker.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/image/spritedetector.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/config/appconfig.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/extractor.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/extractorregistry.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/spriteextractor.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/gifextractor.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/jsonextractor.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/godotextractor.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/export.h
)

target_include_directories(test_extractors PRIVATE
    ${CMAKE_SOURCE_DIR}/SpriteStudio
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include
    ${CMAKE_BINARY_DIR}/SpriteStudio
)

target_compile_definitions(test_extractors PRIVATE
    SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample"
)

target_link_libraries(test_extractors PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Test
)

set_target_properties(test_extractors PROPERTIES
    WIN32_EXECUTABLE FALSE
)

if (MINGW)
    target_link_options(test_extractors PRIVATE -mconsole)
endif()

add_test(NAME test_extractors COMMAND test_extractors)

add_executable(test_controllers
    test_controllers.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/controller/projectcontroller.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/controller/animationcontroller.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/controller/atlasviewcontroller.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/model/spritedocument.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/packer/atlaspacker.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/animation/animationplayer.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/commands/commands.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/atlasboxitem.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/config/appconfig.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/project/sessionmanager.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/project/projectmanager.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/extractor/spriteextractor.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/extractor/gifextractor.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/extractor/jsonextractor.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/extractor/godotextractor.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/extractor/extractorregistry.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/extractor.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/image/spritedetector.cpp

    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/controller/projectcontroller.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/controller/animationcontroller.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/controller/atlasviewcontroller.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/model/spritedocument.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/packer/atlaspacker.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/image/spritedetector.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/animation/animationplayer.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/commands/commands.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/atlasboxitem.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/config/appconfig.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/project/sessionmanager.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/project/projectmanager.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/spriteextractor.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/gifextractor.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/jsonextractor.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/godotextractor.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/extractorregistry.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/extractor.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/export.h
)

target_include_directories(test_controllers PRIVATE
    ${CMAKE_SOURCE_DIR}/SpriteStudio
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include
    ${CMAKE_BINARY_DIR}/SpriteStudio
)

target_compile_definitions(test_controllers PRIVATE
    SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample"
    QM_DIR="${CMAKE_BINARY_DIR}/SpriteStudio"
)

target_link_libraries(test_controllers PRIVATE
    Qt6::Core
    Qt6::CorePrivate
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Test
)

if(LibGit2_FOUND)
    target_link_libraries(test_controllers PRIVATE LibGit2::LibGit2)
    if(WIN32 AND LIBGIT2_DLL)
        add_custom_command(
            TARGET test_controllers POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${LIBGIT2_DLL}" "$<TARGET_FILE_DIR:test_controllers>"
            COMMENT "Copying libgit2.dll to test directory"
        )
    endif()
endif()

set_target_properties(test_controllers PROPERTIES
    WIN32_EXECUTABLE FALSE
)

if (MINGW)
    target_link_options(test_controllers PRIVATE -mconsole)
endif()

add_test(NAME test_controllers COMMAND test_controllers)

add_executable(test_project
    test_project.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/project/sessionmanager.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/project/projectmanager.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/controller/projectcontroller.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/model/spritedocument.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/packer/atlaspacker.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/commands/commands.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/config/appconfig.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/extractor/spriteextractor.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/extractor/gifextractor.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/extractor/jsonextractor.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/extractor/godotextractor.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/extractor/extractorregistry.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/extractor.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/image/spritedetector.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/widgets/githistorydock.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/widgets/settingsdialog.cpp

    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/project/sessionmanager.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/project/projectmanager.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/controller/projectcontroller.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/widgets/githistorydock.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/widgets/settingsdialog.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/model/spritedocument.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/packer/atlaspacker.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/commands/commands.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/config/appconfig.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/extractor.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/extractorregistry.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/spriteextractor.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/gifextractor.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/jsonextractor.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/godotextractor.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/export.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/image/spritedetector.h
)

target_include_directories(test_project PRIVATE
    ${CMAKE_SOURCE_DIR}/SpriteStudio
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include
    ${CMAKE_BINARY_DIR}/SpriteStudio
)

target_compile_definitions(test_project PRIVATE
    SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample"
)

target_link_libraries(test_project PRIVATE
    Qt6::Core
    Qt6::CorePrivate
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Test
)

if(LibGit2_FOUND)
    target_link_libraries(test_project PRIVATE LibGit2::LibGit2)
    if(WIN32 AND LIBGIT2_DLL)
        add_custom_command(
            TARGET test_project POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${LIBGIT2_DLL}" "$<TARGET_FILE_DIR:test_project>"
            COMMENT "Copying libgit2.dll to test directory"
        )
    endif()
endif()

set_target_properties(test_project PROPERTIES
    WIN32_EXECUTABLE FALSE
)

if (MINGW)
    target_link_options(test_project PRIVATE -mconsole)
endif()

add_test(NAME test_project COMMAND test_project)

add_executable(test_core
    test_core.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/model/spritedocument.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/packer/atlaspacker.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/commands/commands.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/config/appconfig.cpp

    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/model/spritedocument.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/packer/atlaspacker.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/commands/commands.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/config/appconfig.h
)

target_include_directories(test_core PRIVATE
    ${CMAKE_SOURCE_DIR}/SpriteStudio
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include
    ${CMAKE_BINARY_DIR}/SpriteStudio
)

target_compile_definitions(test_core PRIVATE
    SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample"
)

target_link_libraries(test_core PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

set_target_properties(test_core PROPERTIES
    WIN32_EXECUTABLE FALSE
)

if (MINGW)
    target_link_options(test_core PRIVATE -mconsole)
endif()

add_test(NAME test_core COMMAND test_core)

