if (WIN32)
    # Bundle Qt imageformats plugins (e.g. qwebp.dll, qpng.dll) so .bento projects open out-of-the-box
    set(_qt_imgfmt_src "${Qt6_DIR}/../../../plugins/imageformats")
    if(EXISTS "${_qt_imgfmt_src}")
        install(DIRECTORY "${_qt_imgfmt_src}"
            DESTINATION "${CMAKE_INSTALL_BINDIR}"
            COMPONENT bentopack
            FILES_MATCHING PATTERN "*webp*" PATTERN "*png*" PATTERN "*ico*" PATTERN "*svg*"
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
