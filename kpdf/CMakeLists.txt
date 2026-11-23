# KPDF: the KPDF interface of KDE 3.5.10 on poppler-glib (the xpdf code
# it carried is gone)

find_package(PkgConfig REQUIRED)
# GDateTime document properties: poppler 20.09
pkg_check_modules(POPPLER REQUIRED IMPORTED_TARGET poppler-glib>=20.09 cairo cairo-ps)

# --enable-force-kpdf-drm of configure.in.in
option(KDE3_APPS_KPDF_FORCE_DRM
  "KPDF always obeys the copy and print restrictions of PDF files (no setting to ignore them)" OFF)
if(KDE3_APPS_KPDF_FORCE_DRM)
  add_compile_definitions(KPDF_FORCE_DRM=1)
else()
  add_compile_definitions(KPDF_FORCE_DRM=0)
endif()

# INCLUDES = -I$(srcdir)/.. -I$(top_builddir)/kpdf of the subdirectories:
# "core/page.h", "conf/settings.h"
include_directories("${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")

add_subdirectory(conf)
add_subdirectory(core)
add_subdirectory(ui)

kde3_add_module(libkpdfpart
  SOURCES
    dcop.skel
    part.cpp
  MOC
    part.h
  LINK
    kpdfconf
    kpdfcore
    kpdfui
    KDE3::kparts
    KDE3::kdeprint
    KDE3::kutils
    KDE3::kwalletclient
    m
)

install(FILES kpdf_part.desktop DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
install(FILES part.rc DESTINATION ${KDE3_DATA_INSTALL_DIR}/kpdfpart)

kde3_install_icons(kpdf)

add_subdirectory(shell)

option(KDE3_APPS_KPDF_TESTS "Build kpdfcheck, a test driver of KPDF's document core (not installed)" OFF)
if(KDE3_APPS_KPDF_TESTS)
  add_subdirectory(tests)
endif()
