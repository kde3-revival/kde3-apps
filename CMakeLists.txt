#    This file is part of kde3-apps, the applications of the KDE3 revival
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.

cmake_minimum_required(VERSION 3.20...4.4)

project(kde3-apps VERSION 3.5.10 LANGUAGES C CXX)

set(KDE3_APPS_VERSION "${PROJECT_VERSION}")

# One option per application, so each can be built and packaged on its
# own.  The handbook of an application follows its option.
option(KDE3_APPS_KWHATSAPP   "Build kwhatsapp, WhatsApp Web as a KDE application (kde3-wpepart at run time)" ON)
option(KDE3_APPS_ARK         "Build Ark, the archive manager" ON)
option(KDE3_APPS_KCALC       "Build KCalc, the scientific calculator (needs the GMP library)" ON)
option(KDE3_APPS_KCHARSELECT "Build KCharSelect, the character selector" ON)
option(KDE3_APPS_KDF         "Build KDiskFree, KwikDisk and the disk usage module of KInfoCenter" ON)
option(KDE3_APPS_KHEXEDIT    "Build KHexEdit, the binary file editor" ON)
option(KDE3_APPS_KTIMER      "Build KTimer, the countdown launcher" ON)
option(KDE3_APPS_KWALLET     "Build KWalletManager, the KWallet manager" ON)
option(KDE3_APPS_KVIEW       "Build KView, the image viewer and its KParts" ON)
option(KDE3_APPS_KRULER      "Build KRuler, the screen ruler" ON)
option(KDE3_APPS_KSNAPSHOT   "Build KSnapshot, the screen capture program" ON)
option(KDE3_APPS_KWEATHER    "Build KWeather, the weather applet of the panel (Open-Meteo)" ON)
option(KDE3_APPS_KBLUETOOTH  "Build kbluetooth, the Bluetooth applet of the system tray (BlueZ 5, needs GIO)" ON)
option(KDE3_APPS_KPDF        "Build KPDF, the PDF viewer and its KPart (needs poppler-glib and cairo)" ON)

# kdelibs: the CMake package, install prefix, compiler settings, RUNPATH
include(cmake/KDELibs.cmake)

include(ConfigureChecks.cmake)
configure_file(config.h.cmake "${PROJECT_BINARY_DIR}/config.h")
# automake passes -DHAVE_CONFIG_H (DEFS)
add_compile_definitions(HAVE_CONFIG_H)

# -I$(top_builddir) of every Makefile: config.h
include_directories("${PROJECT_BINARY_DIR}")

# from kdeutils
foreach(_app ark kcalc kcharselect kdf ktimer kwallet khexedit
        # from kdegraphics
        kview kruler ksnapshot kpdf
        # from kdetoys
        kweather
        # new
        kbluetooth)
  string(TOUPPER "${_app}" _APP)
  if(KDE3_APPS_${_APP})
    add_subdirectory(${_app})
  endif()
endforeach()

# new applications
if(KDE3_APPS_KWHATSAPP)
  add_subdirectory(kwhatsapp)
endif()

add_subdirectory(doc)
