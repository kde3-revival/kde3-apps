#    This file is part of the KDE Utilities distribution
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.

cmake_minimum_required(VERSION 3.20...4.4)

project(kdeutils VERSION 3.5.10 LANGUAGES C CXX)

set(KDEUTILS_VERSION "${PROJECT_VERSION}")

# kdelibs: the CMake package, install prefix, compiler settings, RUNPATH
include(cmake/KDELibs.cmake)

include(ConfigureChecks.cmake)
configure_file(config.h.cmake "${PROJECT_BINARY_DIR}/config.h")
# automake passes -DHAVE_CONFIG_H (DEFS)
add_compile_definitions(HAVE_CONFIG_H)

# -I$(top_builddir) of every Makefile: config.h
include_directories("${PROJECT_BINARY_DIR}")

if(KDEUTILS_BUILD_KCALC)
  add_subdirectory(kcalc)
endif()
add_subdirectory(kcharselect)
add_subdirectory(kdf)
add_subdirectory(ktimer)
