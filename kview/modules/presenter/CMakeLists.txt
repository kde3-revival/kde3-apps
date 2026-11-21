kde3_add_module(kview_presenterplugin
  SOURCES
    imagelistitem.cpp
    imagelistdialog.ui
    kviewpresenter.cpp
  MOC
    kviewpresenter.h
  LINK
    KDE3::kio
    KDE3::kparts
)

install(FILES
  kviewpresenter.desktop
  kviewpresenter.rc
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kview/kpartplugins)
