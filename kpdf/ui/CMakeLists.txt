kde3_add_library(kpdfui OBJECT
  SOURCES
    pagepainter.cpp
    pageview.cpp
    pageviewutils.cpp
    minibar.cpp
    thumbnaillist.cpp
    searchwidget.cpp
    toc.cpp
    propertiesdialog.cpp
    presentationwidget.cpp
  MOC
    minibar.h
    pageview.h
    presentationwidget.h
    searchwidget.h
    thumbnaillist.h
    toc.h
  LINK
    KDE3::kio
    KDE3::kutils
)
# conf/settings.h
add_dependencies(kpdfui kpdfconf)
