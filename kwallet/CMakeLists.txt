# SUBDIRS = . icons konfigurator

kde3_add_executable(kwalletmanager
  SOURCES
    walletwidget.ui
    kwalletmanager.cpp
    kwalletmanager.skel
    main.cpp
    kwalletpopup.cpp
    kwalleteditor.cpp
    kwmapeditor.cpp
    allyourbase.cpp
    kbetterthankdialogbase.ui
  MOC
    allyourbase.h
    kwalleteditor.h
    kwalletmanager.h
    kwalletpopup.h
    kwmapeditor.h
  LINK
    KDE3::kio
    KDE3::kwalletclient
)

kde3_install_icons(kwalletmanager)

install(FILES kwalletmanager.desktop kwalletmanager-kwalletd.desktop
  DESTINATION ${KDE3_XDG_APPS_INSTALL_DIR})
install(FILES kwalletmanager.rc kwalleteditor.rc
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kwalletmanager)

add_subdirectory(icons)
add_subdirectory(konfigurator)
