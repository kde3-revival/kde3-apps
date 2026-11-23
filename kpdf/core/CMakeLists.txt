kde3_add_library(kpdfcore OBJECT
  SOURCES
    document.cpp
    link.cpp
    page.cpp
    pagetransition.cpp
    textpage.cpp
    generator_kimgio/generator_kimgio.cpp
    generator_poppler/generator_poppler.cpp
  MOC
    document.h
  LINK
    KDE3::kio
    KDE3::kdeprint
    KDE3::kwalletclient
    PkgConfig::POPPLER
)
# conf/settings.h
add_dependencies(kpdfcore kpdfconf)
