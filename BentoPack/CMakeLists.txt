cmake_minimum_required(VERSION 3.20)

# Créer un fichier d'en-tête avec les informations de version
configure_file(
    src/version.h.in
    generated/version.h
)

# Configuration de la licence & signature cryptographique commerciale
set(COMMERCIAL_SIGNING_KEY "" CACHE STRING "Secret key for building official commercial releases")
if(COMMERCIAL_SIGNING_KEY STREQUAL "")
    set(SPRITESTUDIO_COMMERCIAL_BUILD 0)
    set(SPRITESTUDIO_LICENSE_TOKEN "")
    message(STATUS "Build target: BentoPack Community Edition")
else()
    set(SPRITESTUDIO_COMMERCIAL_BUILD 1)
    set(SPRITESTUDIO_LICENSE_TOKEN "${COMMERCIAL_SIGNING_KEY}")
    message(STATUS "Build target: BentoPack Commercial Edition (Token provided)")
endif()

configure_file(
    src/license_config.h.in
    generated/license_config.h
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Spécifiez les langues à supporter (code ISO 639-1 ou code Qt)
set(${PROJECT_NAME}_TS_FILES
    i18n/bentopack_fr_FR.ts # Français
    i18n/bentopack_en_US.ts # English
    i18n/bentopack_ja_JA.ts # Japanese
)

set(BentoPackCore_SRC
    src/mainwindow.cpp
    src/mainwindow_events.cpp
    src/mainwindow_callbacks.cpp
    src/mainwindow_animation.cpp
    src/mainwindow_frames.cpp
    src/mainwindow_atlas.cpp
    src/controller/projectcontroller.cpp
    src/controller/animationcontroller.cpp
    src/controller/atlasviewcontroller.cpp
    src/model/spritedocument.cpp
    src/packer/atlaspacker.cpp
    src/packer/maxrectspacker.cpp
    src/packer/tightpolygonpacker.cpp
    src/packer/vramtexturecompressor.cpp
    src/animation/animationplayer.cpp
    src/commands/commands.cpp
    src/extractor.cpp
    src/extractor/extractorregistry.cpp
    src/arrangementmodel.cpp
    src/aboutdialog.cpp
    src/atlasboxitem.cpp
    src/config/appconfig.cpp
    src/image/spritedetector.cpp
    src/project/sessionmanager.cpp
    src/project/projectmanager.cpp
    src/widgets/githistorydock.cpp
    src/widgets/settingsdialog.cpp
    src/widgets/filmstriplistwidget.cpp
    src/widgets/timelinefilmstripwidget.cpp
    src/widgets/exportdialog.cpp
    src/widgets/filterdialogbase.cpp
    src/filters/filterregistry.cpp
    src/commands/filtercommands.cpp
    src/localizationmanager.cpp
    src/zip/miniz.c
    src/version.h.in
    src/haiku.PackageInfo.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/version.h
    src/cli/cliparser.cpp
    src/cli/clipackpipeline.cpp
    src/cli/native_commands.cpp
    src/cli/watch_daemon.cpp
    src/geometry/contourtracer.cpp
    src/geometry/polygonsimplifier.cpp
    src/geometry/triangulator.cpp
    src/geometry/polygonmerger.cpp
    src/license/licensemanager.cpp
    src/license/integrityguard.cpp
    src/commands/meshcommands.cpp
    src/widgets/polygonmeshdialog.cpp
    src/widgets/pixelcanvas.cpp
    src/widgets/pixeleditordialog.cpp
    src/widgets/branchselectiondialog.cpp
)

set(BentoPackCore_UI
    src/mainwindow.ui
    src/widgets/exportdialog.ui
)

set(BentoPackCore_INCLUDE
    include/bentopackcore_export.h
    include/mainwindow.h
    include/config/appconfig.h
    include/controller/projectcontroller.h
    include/controller/animationcontroller.h
    include/controller/atlasviewcontroller.h
    include/model/spritedocument.h
    include/packer/atlaspacker.h
    include/packer/maxrectspacker.h
    include/packer/tightpolygonpacker.h
    include/packer/vramtexturecompressor.h
    include/animation/animationplayer.h
    include/commands/commands.h
    include/commands/filtercommands.h
    include/image/spritedetector.h
    include/atlasboxitem.h
    include/project/sessionmanager.h
    include/project/projectmanager.h
    include/widgets/githistorydock.h
    include/widgets/settingsdialog.h
    include/widgets/filmstriplistwidget.h
    include/widgets/timelinefilmstripwidget.h
    include/widgets/exportdialog.h
    include/widgets/filterdialogbase.h
    include/widgets/branchselectiondialog.h
    include/filters/filterplugin.h
    include/filters/filterregistry.h
    include/localizationmanager.h
    include/extractor/extractor.h
    include/extractor/extractorregistry.h
    include/extractor/export.h
    include/arrangementmodel.h
    include/aboutdialog.h
    include/zip/miniz.h
    include/cli/cliparser.h
    include/cli/clipackpipeline.h
    include/cli/native_commands.h
    include/cli/watch_daemon.h
    include/geometry/contourtracer.h
    include/geometry/polygonsimplifier.h
    include/geometry/triangulator.h
    include/geometry/polygonmerger.h
    include/license/licensemanager.h
    include/license/integrityguard.h
    include/commands/meshcommands.h
    include/widgets/polygonmeshdialog.h
    include/widgets/pixelcanvas.h
    include/widgets/pixeleditordialog.h
)

# Bibliothèque partagée commune pour le Core, l'application et les plugins
add_library(BentoPackCore SHARED
    ${BentoPackCore_SRC}
    ${BentoPackCore_UI}
    ${BentoPackCore_INCLUDE}
)

set_target_properties(BentoPackCore PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

target_compile_definitions(BentoPackCore PRIVATE SPRITESTUDIO_CORE_LIBRARY)

qt_add_translations(BentoPackCore
    TS_FILES
        ${${PROJECT_NAME}_TS_FILES}
    SOURCES
        ${BentoPackCore_SRC}
        ${BentoPackCore_INCLUDE}
        ${BentoPackCore_UI}
    RESOURCE_PREFIX
        "/i18n"
)

qt6_add_resources(BentoPackCore "images"
    PREFIX "drawer"
    FILES
    play.png
    pause.png
    stop.png
    plus.png
    minus.png
    icons/tool_select.png
    icons/tool_slice.png
    icons/tool_trim.png
    icons/tool_remove_bg.png
    icons/bentopack.png
)

set_source_files_properties(${CMAKE_SOURCE_DIR}/LICENSE PROPERTIES
    QT_RESOURCE_ALIAS "LICENSE"
)
set_source_files_properties(${CMAKE_SOURCE_DIR}/plugins/LICENSE-PLUGINS.md PROPERTIES
    QT_RESOURCE_ALIAS "LICENSE-PLUGINS.md"
)
set_source_files_properties(res/pricing.json PROPERTIES
    QT_RESOURCE_ALIAS "pricing.json"
)

qt6_add_resources(BentoPackCore "text"
    PREFIX "text"
    FILES
    ${CMAKE_SOURCE_DIR}/LICENSE
    ${CMAKE_SOURCE_DIR}/plugins/LICENSE-PLUGINS.md
    about.txt
    res/pricing.json
)

target_include_directories(BentoPackCore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/zip>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/bentopack>
)

install(TARGETS BentoPackCore
    EXPORT BentoPackTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/bentopack
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/bentopack
    COMPONENT bentopack-dev
    FILES_MATCHING PATTERN "*.h"
)

target_link_libraries(BentoPackCore PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Network
)

if(LibGit2_FOUND)
    target_link_libraries(BentoPackCore PRIVATE LibGit2::LibGit2)
endif()

if(TARGET basisu_encoder)
    target_include_directories(BentoPackCore PRIVATE
        $<BUILD_INTERFACE:${basis_universal_SOURCE_DIR}>
        $<BUILD_INTERFACE:${basis_universal_SOURCE_DIR}/encoder>
        $<BUILD_INTERFACE:${basis_universal_SOURCE_DIR}/transcoder>
    )
    target_link_libraries(BentoPackCore PRIVATE basisu_encoder)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(BentoPackCore PRIVATE -fno-strict-aliasing)
    endif()
endif()

# Ressources d'icône Windows
set(APP_EXTRA_SOURCES "")
if(WIN32)
    set(APP_EXTRA_SOURCES res/windows/BentoPack.rc)
endif()

# Exécutable principal BentoPack
qt_add_executable(${PROJECT_NAME}
    src/main.cpp
    ${APP_EXTRA_SOURCES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    BentoPackCore
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "bentopack"
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

if(UNIX AND NOT APPLE)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink "$<TARGET_FILE_NAME:${PROJECT_NAME}>" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/BentoPack"
        COMMENT "Creating BentoPack symlinks"
    )
endif()

if(LibGit2_FOUND)
    if(WIN32 AND LIBGIT2_DLL)
        add_custom_command(
            TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${LIBGIT2_DLL}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            COMMENT "Copying libgit2.dll to application directory"
        )
    endif()
endif()

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Exécutable console bentopack-cli
add_executable(bentopack-cli
    src/cli/main_cli.cpp
)

target_link_libraries(bentopack-cli PRIVATE
    BentoPackCore
)

set_target_properties(bentopack-cli PROPERTIES
    OUTPUT_NAME "bentopack-cli"
    WIN32_EXECUTABLE FALSE
)

if (MINGW)
    target_link_options(bentopack-cli PRIVATE -mconsole)
endif()

if(LibGit2_FOUND)
    if(WIN32 AND LIBGIT2_DLL)
        add_custom_command(
            TARGET bentopack-cli POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${LIBGIT2_DLL}" "$<TARGET_FILE_DIR:bentopack-cli>"
            COMMENT "Copying libgit2.dll to CLI directory"
        )
    endif()
endif()

target_include_directories(bentopack-cli PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)

install(TARGETS ${PROJECT_NAME} bentopack-cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    COMPONENT bentopack
)

if(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
    add_custom_target(haiku_package
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/haiku_pkg
        COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --prefix ${CMAKE_BINARY_DIR}/haiku_pkg
        COMMAND package create -i ${CMAKE_BINARY_DIR}/generated/haiku.PackageInfo -C ${CMAKE_BINARY_DIR}/haiku_pkg ${CMAKE_BINARY_DIR}/${PROJECT_NAME}-${PROJECT_VERSION}-1-x86_64.hpkg
        DEPENDS ${PROJECT_NAME} bentopack-cli
        COMMENT "Creating Haiku HPKG package"
    )
endif()

