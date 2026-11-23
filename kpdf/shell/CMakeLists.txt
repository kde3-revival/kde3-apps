kde3_add_executable(kpdf
  SOURCES
    main.cpp
    shell.cpp
  MOC
    shell.h
  LINK
    KDE3::kparts
)

install(FILES kpdf.desktop DESTINATION ${KDE3_XDG_APPS_INSTALL_DIR})
install(FILES shell.rc DESTINATION ${KDE3_DATA_INSTALL_DIR}/kpdf)
