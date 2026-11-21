add_subdirectory(pics)

kde3_add_executable(kruler
  SOURCES
    klineal.cpp
    main.cpp
  MOC
    klineal.h
  LINK
    KDE3::kdeui
)

install(FILES kruler.desktop DESTINATION ${KDE3_XDG_APPS_INSTALL_DIR})

install(FILES eventsrc DESTINATION ${KDE3_DATA_INSTALL_DIR}/kruler)
install(FILES move.wav DESTINATION ${KDE3_DATA_INSTALL_DIR}/kruler/sounds)

# install-data-local: hide the old menu entry
install(FILES uninstall.desktop
  DESTINATION ${KDE3_APPS_INSTALL_DIR}/Graphics RENAME kruler.desktop)
