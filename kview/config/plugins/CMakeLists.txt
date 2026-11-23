kde3_add_module(kcm_kviewpluginsconfig
  SOURCES
    kviewpluginsconfig.cpp
  MOC
    kviewpluginsconfig.h
  LINK
    KDE3::kutils
)

install(FILES kviewpluginsconfig.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kconfiguredialog)
