# kpdfcheck: the document core without the views, for tests (not installed)
kde3_add_executable(kpdfcheck NO_INSTALL
  SOURCES
    kpdfcheck.cpp
  LINK
    kpdfconf
    kpdfcore
    kpdfui
    KDE3::kio
    KDE3::kdeprint
    KDE3::kutils
    KDE3::kwalletclient
)
