kde3_add_executable(ktimer
  SOURCES
    main.cpp
    ktimer.cpp
    prefwidget.ui
  MOC
    ktimer.h
  LINK
    KDE3::kio
)

kde3_install_icons(ktimer)

install(FILES ktimer.desktop DESTINATION ${KDE3_XDG_APPS_INSTALL_DIR})
