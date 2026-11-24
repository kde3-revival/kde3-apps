# kwhatsapp: WhatsApp Web (the official web client) in kde3-wpepart's
# KWPEPart, app mode.  The part is loaded at run time (KLibLoader), so
# kde3-wpepart is a run time dependency only.
kde3_add_executable(kwhatsapp
  SOURCES
    main.cpp
    kwhatsapp.cpp
    kwhatsappballoon.cpp
  MOC
    kwhatsapp.h
    kwhatsappballoon.h
  LINK
    KDE3::kparts
    KDE3::kio
)

kde3_install_icons(kwhatsapp)

install(FILES kwhatsapp.desktop DESTINATION ${KDE3_XDG_APPS_INSTALL_DIR})
install(FILES kwhatsappui.rc eventsrc DESTINATION ${KDE3_DATA_INSTALL_DIR}/kwhatsapp)
