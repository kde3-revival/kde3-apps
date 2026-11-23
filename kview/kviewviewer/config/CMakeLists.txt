kde3_add_module(kcm_kviewviewerpluginsconfig
  SOURCES
    kviewviewerpluginsconfig.cpp
  MOC
    kviewviewerpluginsconfig.h
  LINK
    KDE3::kutils
)

install(FILES kviewviewerpluginsconfig.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kconfiguredialog)
