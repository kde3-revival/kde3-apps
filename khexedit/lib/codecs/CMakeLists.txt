include_directories("${CMAKE_CURRENT_SOURCE_DIR}/..")

# noinst_LTLIBRARIES = libkhecodecs.la
kde3_add_library(khecodecs OBJECT
  SOURCES
    kcharcodec.cpp
    kbytecodec.cpp
    ktextcharcodec.cpp
    kebcdic1047charcodec.cpp
    kbinarybytecodec.cpp
    koctalbytecodec.cpp
    kdecimalbytecodec.cpp
    khexadecimalbytecodec.cpp
  LINK
    KDE3::kdecore
)
