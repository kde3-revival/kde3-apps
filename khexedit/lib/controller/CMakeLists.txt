include_directories("${CMAKE_CURRENT_SOURCE_DIR}/..")

# noinst_LTLIBRARIES = libkcontroller.la
kde3_add_library(khecontroller OBJECT
  SOURCES
    kcontroller.cpp
    ktabcontroller.cpp
    knavigator.cpp
    keditor.cpp
    kvalueeditor.cpp
    kchareditor.cpp
  LINK
    KDE3::kdecore
)
