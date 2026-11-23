# kimageviewer first: the library the parts link to (SUBDIRS = kimageviewer .)
add_subdirectory(kimageviewer)

# -I$(top_srcdir)/kview of this directory, kviewcanvas, kviewviewer and
# modules: "kimageviewer/viewer.h", <kimageviewer/canvas.h>, version.h
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

kde3_add_kdeinit_executable(kview
  SOURCES
    kview.cpp
    main.cpp
  MOC
    kview.h
  LINK
    KDE3::kparts
    KDE3::kutils
)

install(FILES kview.desktop DESTINATION ${KDE3_XDG_APPS_INSTALL_DIR})
install(FILES kviewui.rc DESTINATION ${KDE3_DATA_INSTALL_DIR}/kview)

kde3_install_icons(kview)

add_subdirectory(config)
add_subdirectory(kviewcanvas)
add_subdirectory(kviewviewer)
add_subdirectory(modules)
