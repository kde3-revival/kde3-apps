kde3_add_module(kcm_kwallet
  SOURCES
    walletconfigwidget.ui
    konfigurator.cpp
  MOC
    konfigurator.h
  LINK
    KDE3::kdeui
    KDE3::kwalletclient
)

install(FILES kwalletconfig.desktop DESTINATION ${KDE3_XDG_APPS_INSTALL_DIR})
install(FILES kwallet_config.desktop kwalletmanager_show.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
