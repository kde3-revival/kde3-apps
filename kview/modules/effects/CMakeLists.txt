kde3_add_module(kview_effectsplugin
  SOURCES
    kvieweffects.cpp
  MOC
    kvieweffects.h
  LINK
    KDE3::kio
    KDE3::kparts
    KDE3::kdeprint
)

install(FILES
  kvieweffects.desktop
  kvieweffects.rc
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kview/kpartplugins)
