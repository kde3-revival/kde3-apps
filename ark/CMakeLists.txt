add_subdirectory(pics)

add_compile_definitions(_LARGEFILE64_SOURCE)

# noinst_LTLIBRARIES = libark_common.la
kde3_add_library(ark_common OBJECT
  SOURCES
    settings.kcfgc
    archiveformatinfo.cpp
  LINK
    KDE3::kio
)

kde3_add_kdeinit_executable(ark
  SOURCES
    main.cpp
    arkapp.cpp
    mainwindow.cpp
  MOC
    arkapp.h
    mainwindow.h
  LINK
    ark_common
    KDE3::kparts
)

kde3_add_module(libarkpart
  SOURCES
    ark_part.cpp
    arkfactory.cpp
    zip.cpp
    tar.cpp
    filelistview.cpp
    arch.cpp
    lha.cpp
    compressedfile.cpp
    zoo.cpp
    rar.cpp
    ar.cpp
    arkutils.cpp
    archiveformatdlg.cpp
    arkwidget.cpp
    searchbar.cpp
    addition.ui
    extraction.ui
    general.ui
    arkviewer.cpp
    sevenzip.cpp
    extractiondialog.cpp
    ace.cpp
    tarlistingthread.cpp
  MOC
    ace.h
    ar.h
    arch.h
    archiveformatdlg.h
    ark_part.h
    arkviewer.h
    arkwidget.h
    compressedfile.h
    extractiondialog.h
    filelistview.h
    lha.h
    rar.h
    searchbar.h
    sevenzip.h
    tar.h
    zip.h
    zoo.h
  LINK
    ark_common
    KDE3::kparts
)

kde3_install_icons(ark)

install(FILES arkui.rc ark_part.rc ark_part_readonly.rc
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/ark)
install(FILES ark.desktop DESTINATION ${KDE3_XDG_APPS_INSTALL_DIR})
install(FILES ark.kcfg DESTINATION ${KDE3_KCFG_INSTALL_DIR})
install(FILES ark_part.desktop DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
