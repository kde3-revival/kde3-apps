kde3_add_executable(ksnapshot
  SOURCES
    ksnapshotiface.skel
    main.cpp
    ksnapshot.cpp
    quickshot.cpp
    regiongrabber.cpp
    windowgrabber.cpp
    ksnapshotwidget.ui
  MOC
    ksnapshot.h
    quickshot.h
    regiongrabber.h
    windowgrabber.h
  LINK
    KDE3::kdeprint
    KDE3::kio
)
target_compile_definitions(ksnapshot PRIVATE KSNAPVERSION="0.7")
if(HAVE_X11_EXTENSIONS_SHAPE_H)
  # XShapeGetRectangles()
  target_link_libraries(ksnapshot PRIVATE X11::Xext)
endif()

install(FILES ksnapshot.desktop DESTINATION ${KDE3_XDG_APPS_INSTALL_DIR})

kde3_install_icons(ksnapshot)
