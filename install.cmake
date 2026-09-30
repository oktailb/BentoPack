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

    # 2. Bundle Qt platform plugins (windows for GUI, offscreen for headless CLI, minimal for fallback)
    set(_qt_plugins_dir "${Qt6_DIR}/../../../plugins")
    if(EXISTS "${_qt_plugins_dir}")
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
    endif()

    # 3. Automated windeployqt hook to bundle compiler runtimes, Qt libraries, styles, iconengines, and TLS
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin" "${Qt6_DIR}/bin")
    if(WINDEPLOYQT_EXECUTABLE)
        install(CODE "
            message(STATUS \"Deploying Qt runtime and dependencies via windeployqt...\")
            execute_process(
                COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
                    --compiler-runtime
                    --no-translations
                    --no-opengl-sw
                    --include-plugins qoffscreen,qminimal,qwindows,qwebp,qsvg,qico,qjpeg,qgif
                    --dir \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\"
                    \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/bentopack.exe\"
                RESULT_VARIABLE _wdq_res
            )
            if(NOT _wdq_res EQUAL 0)
                message(WARNING \"windeployqt exited with code \${_wdq_res}\")
            endif()
        " COMPONENT bentopack)
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
