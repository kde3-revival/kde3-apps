kde3_add_module(kcm_kviewgeneralconfig
  SOURCES
    kviewconfmodules.cpp
  MOC
    kviewconfmodules.h
  LINK
    KDE3::kdeui
)

install(FILES kviewgeneralconfig.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kconfiguredialog)
install(FILES kview.setdlg DESTINATION ${KDE3_DATA_INSTALL_DIR}/kview)

add_subdirectory(plugins)
