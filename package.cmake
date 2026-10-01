set(CPACK_RESOURCE_FILE_LICENSE  "${CMAKE_CURRENT_LIST_DIR}/LICENSE")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_PACKAGE_CONTACT "vincent.lecoq@gmail.com")
set(CPACK_PACKAGE_VENDOR "Oktailb")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "BentoPack - High-density 2D sprite sheet and polygonal mesh studio")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}.${CMAKE_SYSTEM_PROCESSOR}")

# Linux CPack configuration
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_RPM_PACKAGE_LICENSE "Apache-2.0")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Graphics")

# Windows CPack configuration
set(CPACK_NSIS_DISPLAY_NAME "BentoPack Studio")
set(CPACK_NSIS_PACKAGE_NAME "BentoPack Studio")
set(CPACK_NSIS_MODIFY_PATH ON)
set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_LIST_DIR}/BentoPack/res/windows/BentoPack.ico")
set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_LIST_DIR}/BentoPack/res/windows/BentoPack.ico")

set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\bentopack.exe")
set(CPACK_NSIS_MENU_LINKS
    "bin/bentopack.exe" "BentoPack Studio"
    "bin/bentopack-cli.exe" "BentoPack CLI"
)
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
endif()

# Register official installation path in Windows Registry and ensure $INSTDIR\bin is in PATH
set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
  WriteRegStr HKLM \\\"Software\\\\BentoPack Studio\\\" \\\"InstallLocation\\\" \\\"$INSTDIR\\\"
  WriteRegStr HKLM \\\"Software\\\\BentoPack Studio\\\" \\\"Path\\\" \\\"$INSTDIR\\\\bin\\\"
  Push \\\"$INSTDIR\\\\bin\\\"
  Call AddToPath
")
set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
  DeleteRegKey HKLM \\\"Software\\\\BentoPack Studio\\\"
  Push \\\"$INSTDIR\\\\bin\\\"
  Call un.RemoveFromPath
")

# macOS CPack configuration
set(CPACK_DMG_VOLUME_NAME "BentoPack Studio")
set(CPACK_DMG_FORMAT "UDZO")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CPACK_GENERATOR "TGZ;RPM;DEB")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    find_program(MAKENSIS_EXECUTABLE makensis
        HINTS
            "C:/Program Files (x86)/NSIS"
            "C:/Program Files/NSIS"
            "$ENV{ProgramFiles\(x86\)}/NSIS"
            "$ENV{ProgramFiles}/NSIS"
    )
    if(MAKENSIS_EXECUTABLE)
        set(CPACK_GENERATOR "ZIP;NSIS")
    else()
        set(CPACK_GENERATOR "ZIP")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(CPACK_GENERATOR "DragNDrop;TGZ")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

# CPack Component packaging for bentopack & bentopack-dev
set(CPACK_COMPONENTS_ALL bentopack bentopack_dev)
set(CPACK_COMPONENT_BENTOPACK_DISPLAY_NAME "BentoPack Application and Plugins")
set(CPACK_COMPONENT_BENTOPACK-DEV_DISPLAY_NAME "BentoPack Development SDK (Headers & CMake)")
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_RPM_COMPONENT_INSTALL ON)
# Pour NSIS : soit désactiver le component install pour avoir un setup .exe monolithique propre :
set(CPACK_NSIS_COMPONENT_INSTALL OFF)

# Préserver le nom avec tiret pour les paquets Linux deb/rpm si souhaité :
set(CPACK_DEBIAN_BENTOPACK_DEV_PACKAGE_NAME "bentopack-dev")
set(CPACK_RPM_BENTOPACK_DEV_PACKAGE_NAME "bentopack-dev")

include(CPack)
