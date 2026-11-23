add_subdirectory(config)

kde3_add_module(libkviewviewer
  SOURCES
    printimagesettings.ui
    imagesettings.cpp
    kviewkonqextension.cpp
    kviewviewer.cpp
  MOC
    imagesettings.h
    kviewkonqextension.h
    kviewviewer.h
  LINK
    kimageviewer
    KDE3::kdeprint
    KDE3::kutils
)

install(FILES kviewviewer.desktop DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
install(FILES
  kviewviewer.rc
  kviewviewer_ro.rc
  kviewpopup.rc
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kviewviewer)
