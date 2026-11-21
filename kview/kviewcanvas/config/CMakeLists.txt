kde3_add_module(kcm_kviewcanvasconfig
  SOURCES
    generalconfigwidget.ui
    confmodules.cpp
  MOC
    confmodules.h
  LINK
    KDE3::kdeui
)

install(FILES kviewcanvasconfig.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kconfiguredialog)
