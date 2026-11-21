kde3_add_module(kview_browserplugin
  SOURCES
    kmyfileitemlist.cpp
    kviewbrowser.cpp
  MOC
    kviewbrowser.h
  LINK
    KDE3::kio
    KDE3::kparts
    KDE3::kdeprint
)

install(FILES
  kviewbrowser.desktop
  kviewbrowser.rc
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kviewviewer/kpartplugins)
