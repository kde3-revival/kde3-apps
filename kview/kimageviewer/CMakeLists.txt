kde3_add_library(kimageviewer SHARED VERSION_INFO 0:0
  SOURCES
    canvas.cpp
    viewer.cpp
  MOC
    viewer.h
  LINK
    KDE3::kparts
)

# The headers are not installed (kimageviewerinclude_HEADERS is commented
# out in Makefile.am): the interfaces are only used inside kview.

install(FILES
  kimageviewercanvas.desktop
  kimageviewer.desktop
  DESTINATION ${KDE3_SERVICETYPES_INSTALL_DIR})
