# noinst_LTLIBRARIES = libknumber.la
kde3_add_library(knumber OBJECT
  SOURCES
    knumber.cpp
    knumber_priv.cpp
  LINK
    KDE3Qt::Qt
    ${GMP_LIBRARY}
    m
)
target_compile_definitions(knumber PRIVATE _GNU_SOURCE _ISOC99_SOURCE)
# $(all_includes): knumber.h includes kdelibs_export.h
target_include_directories(knumber SYSTEM PRIVATE
  "${GMP_INCLUDE_DIR}" "${KDE3_INCLUDE_DIR}")
