add_subdirectory(config)

kde3_add_module(libkviewcanvas
  SOURCES
    kimageholder.cpp
    kimagecanvas.cpp
  MOC
    kimageholder.h
    kimagecanvas.h
  LINK
    kimageviewer
    KDE3::kparts
    KDE3::kdeprint
    KDE3::kutils
)

install(FILES kviewcanvas.desktop DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
