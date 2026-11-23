# kbluetooth: the Bluetooth applet of the system tray, on BlueZ 5 over the
# system D-Bus.  GIO's GDBus talks to bluetoothd; kde3-qt runs the GLib
# main loop, so its replies and signals arrive in the Qt event loop.

find_package(PkgConfig REQUIRED)
pkg_check_modules(GIO QUIET IMPORTED_TARGET gio-2.0)
if(NOT GIO_FOUND)
  message(FATAL_ERROR "GIO (gio-2.0, part of GLib) was not found; kbluetooth "
    "needs it to talk to BlueZ over D-Bus.  Install it, or configure with "
    "-DKDE3_APPS_KBLUETOOTH=OFF.")
endif()

kde3_add_executable(kbluetooth
  SOURCES
    main.cpp
    bluez.cpp
    agent.cpp
    agentdialog.cpp
    rfkill.cpp
    icons.cpp
    tray.cpp
    popup.cpp
    scandialog.cpp
  MOC
    bluez.h
    agent.h
    agentdialog.h
    rfkill.h
    tray.h
    popup.h
    scandialog.h
  LINK
    KDE3::kdeui
    PkgConfig::GIO
)

# the icons of the tray (off, on, connected) and of the application;
# drawn for kbluetooth, the SVG sources are in icon-src/
kde3_install_icons(kbluetooth kbluetooth_off kbluetooth_connected)

install(FILES kbluetooth.desktop DESTINATION ${KDE3_XDG_APPS_INSTALL_DIR})
install(FILES kbluetooth-autostart.desktop DESTINATION ${KDE3_AUTOSTART_INSTALL_DIR}
        RENAME kbluetooth.desktop)
install(FILES eventsrc DESTINATION ${KDE3_DATA_INSTALL_DIR}/kbluetooth)
