add_subdirectory(pics)

# noinst_LTLIBRARIES = libkdf_common.la
kde3_add_library(kdf_common OBJECT
  SOURCES
    kdfwidget.cpp
    kdfconfig.cpp
    mntconfig.cpp
    disklist.cpp
    disks.cpp
    listview.cpp
    optiondialog.cpp
    stdoption.cpp
  MOC
    disklist.h
    disks.h
    kdfconfig.h
    kdfwidget.h
    listview.h
    mntconfig.h
    optiondialog.h
  LINK
    KDE3::kio
)

kde3_add_executable(kdf
  SOURCES
    kdf.cpp
  MOC
    kdf.h
  LINK
    kdf_common
)

kde3_add_executable(kwikdisk
  SOURCES
    kwikdisk.cpp
  MOC
    kwikdisk.h
  LINK
    kdf_common
)

kde3_add_module(kcm_kdf
  SOURCES
    kcmdf.cpp
  MOC
    kcmdf.h
  LINK
    kdf_common
)

install(FILES kdf.desktop kwikdisk.desktop kcmdf.desktop
  DESTINATION ${KDE3_XDG_APPS_INSTALL_DIR})
install(FILES kdfui.rc DESTINATION ${KDE3_DATA_INSTALL_DIR}/kdf)
