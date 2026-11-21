kde3_add_executable(kcharselect
  SOURCES
    kcharselectdia.cc
    main.cc
  MOC
    kcharselectdia.h
  LINK
    KDE3::kio
)

kde3_install_icons(kcharselect)

install(FILES KCharSelect.desktop DESTINATION ${KDE3_XDG_APPS_INSTALL_DIR})
install(FILES kcharselectui.rc DESTINATION ${KDE3_DATA_INSTALL_DIR}/kcharselect)
install(FILES kcharselect.upd DESTINATION ${KDE3_DATA_INSTALL_DIR}/kconf_update)
