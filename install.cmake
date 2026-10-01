# Install licenses and documentation
install(FILES
    "${CMAKE_CURRENT_LIST_DIR}/LICENSE"
    "${CMAKE_CURRENT_LIST_DIR}/plugins/LICENSE-PLUGINS.md"
    "${CMAKE_CURRENT_LIST_DIR}/addons/LICENSE-ADDONS.md"
    "${CMAKE_CURRENT_LIST_DIR}/README.md"
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/bentopack"
    COMPONENT bentopack
)

if (WIN32)
    # 1. Bundle libgit2 runtime DLL
    if(LIBGIT2_DLL AND EXISTS "${LIBGIT2_DLL}")
        install(FILES "${LIBGIT2_DLL}"
            DESTINATION "${CMAKE_INSTALL_BINDIR}"
            COMPONENT bentopack
        )
    elseif(EXISTS "${CMAKE_BINARY_DIR}/bin/libgit2.dll")
        install(FILES "${CMAKE_BINARY_DIR}/bin/libgit2.dll"
            DESTINATION "${CMAKE_INSTALL_BINDIR}"
            COMPONENT bentopack
        )
    endif()

    # 2. Locate Qt plugins directory across multiple standard layouts (official SDK, MSYS2, custom)
    set(_qt_plugins_dir "")
    foreach(_candidate
        "${Qt6_DIR}/../../../plugins"
        "${Qt6Core_DIR}/../../../plugins"
        "${Qt6Core_DIR}/../../../../share/qt6/plugins"
        "${Qt6Core_DIR}/../../../share/qt6/plugins"
        "$ENV{MINGW_PREFIX}/share/qt6/plugins"
        "$ENV{MINGW_PREFIX}/lib/qt6/plugins"
        "C:/Qt/6.11.2/mingw_64/plugins"
    )
        if(EXISTS "${_candidate}/platforms")
            set(_qt_plugins_dir "${_candidate}")
            break()
        endif()
    endforeach()

    if(_qt_plugins_dir AND EXISTS "${_qt_plugins_dir}")
        message(STATUS "Found Qt plugins directory for bundling: ${_qt_plugins_dir}")
        install(DIRECTORY "${_qt_plugins_dir}/platforms"
            DESTINATION "${CMAKE_INSTALL_BINDIR}"
            COMPONENT bentopack
            FILES_MATCHING
                PATTERN "*windows*"
                PATTERN "*offscreen*"
                PATTERN "*minimal*"
        )
        install(DIRECTORY "${_qt_plugins_dir}/imageformats"
            DESTINATION "${CMAKE_INSTALL_BINDIR}"
            COMPONENT bentopack
            FILES_MATCHING
                PATTERN "*webp*"
                PATTERN "*png*"
                PATTERN "*ico*"
                PATTERN "*svg*"
                PATTERN "*jpeg*"
                PATTERN "*gif*"
        )
        if(EXISTS "${_qt_plugins_dir}/styles")
            install(DIRECTORY "${_qt_plugins_dir}/styles"
                DESTINATION "${CMAKE_INSTALL_BINDIR}"
                COMPONENT bentopack
            )
        endif()
        if(EXISTS "${_qt_plugins_dir}/iconengines")
            install(DIRECTORY "${_qt_plugins_dir}/iconengines"
                DESTINATION "${CMAKE_INSTALL_BINDIR}"
                COMPONENT bentopack
            )
        endif()
    endif()

    # 3. Automated windeployqt hook (supports windeployqt, windeployqt6, windeployqt-qt6)
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt6 windeployqt-qt6 windeployqt
        HINTS
            "${Qt6_DIR}/../../../bin"
            "${Qt6_DIR}/bin"
            "${Qt6Core_DIR}/../../../bin"
            "${Qt6Core_DIR}/bin"
            "${QT_HOST_PATH}/bin"
            ENV MINGW_PREFIX
            "C:/Qt/6.11.2/mingw_64/bin"
    )

    if(WINDEPLOYQT_EXECUTABLE)
        message(STATUS "Found windeployqt tool: ${WINDEPLOYQT_EXECUTABLE}")
        install(CODE "
            message(STATUS \"Deploying Qt runtime and dependencies via windeployqt...\")
            file(TO_NATIVE_PATH \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\" _native_bin_dir)
            file(TO_NATIVE_PATH \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/bentopack.exe\" _native_exe_path)
            execute_process(
                COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
                    --compiler-runtime
                    --no-translations
                    --no-opengl-sw
                    --include-plugins qoffscreen,qminimal,qwindows,qwebp,qsvg,qico,qjpeg,qgif
                    --dir \"\${_native_bin_dir}\"
                    \"\${_native_exe_path}\"
                RESULT_VARIABLE _wdq_res
            )
            if(NOT _wdq_res EQUAL 0)
                message(WARNING \"windeployqt exited with code \${_wdq_res}\")
            endif()
        " COMPONENT bentopack)
    else()
        message(WARNING \"Neither windeployqt nor windeployqt6 was found. Automatic Qt deployment disabled.\")
    endif()

    # 4. Bundle any DLLs present in build output directory (MinGW runtimes, deployed Qt libs, libgit2, plugins)
    file(GLOB _build_bin_dlls "${CMAKE_BINARY_DIR}/bin/*.dll")
    if(_build_bin_dlls)
        install(FILES ${_build_bin_dlls}
            DESTINATION "${CMAKE_INSTALL_BINDIR}"
            COMPONENT bentopack
        )
    endif()
endif()

if(UNIX AND NOT APPLE AND NOT HAIKU)
    install(FILES BentoPack/res/linux/BentoPack.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
        RENAME bentopack.desktop
    )
    install(FILES BentoPack/res/icons/bentopack.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps
        RENAME bentopack.png
    )
    install(FILES
        docs/man/bentopack.1 docs/man/bentopack-cli.1
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
    )
endif()

if (HAIKU)
    message("This is haiku")
    install(FILES ${CMAKE_CURRENT_LIST_DIR}/LICENSE
        DESTINATION data/licenses
        RENAME "Apache License Version 2.0"
    )
    install(FILES ${CMAKE_CURRENT_LIST_DIR}/LICENSE
        DESTINATION data/licenses
        RENAME "Apache v2"
    )
    configure_file(
        BentoPack/src/haiku.PackageInfo.in
        generated/haiku.PackageInfo
    )
endif()
