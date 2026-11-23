kde3_add_library(kpdfconf OBJECT
  SOURCES
    dlggeneral.ui
    dlgperformance.ui
    dlgaccessibility.ui
    dlgpresentation.ui
    preferencesdialog.cpp
    settings.kcfgc
  LINK
    KDE3::kio
)

install(FILES kpdf.kcfg DESTINATION ${KDE3_KCFG_INSTALL_DIR})
