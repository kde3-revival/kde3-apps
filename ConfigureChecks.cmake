# Configure checks for config.h, following configure.in.in and the
# subdirectories' configure.in.in.  Only what the sources actually test
# is checked: configure.in.in's header and function checks
# (fcntl.h, sys/time.h, unistd.h, paths.h, setenv, usleep, mkstemps,
# ...) are not used by KView, KSnapshot or KRuler.

include(CheckIncludeFiles)
include(CMakePushCheckState)

find_package(X11 REQUIRED)

# ---- ksnapshot (ksnapshot/configure.in.in) ---------------------------------
# The X Shape extension gives the window grabber the shape of windows that
# are not rectangular (cut out of the grabbed pixmap).
option(KDEGRAPHICS_WITH_XSHAPE
  "Cut non-rectangular windows to their shape in KSnapshot (X Shape extension)" ON)
if(KDEGRAPHICS_WITH_XSHAPE)
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_INCLUDES ${X11_INCLUDE_DIR})
  check_include_files("X11/Xlib.h;X11/Xutil.h;X11/extensions/shape.h"
    HAVE_X11_EXTENSIONS_SHAPE_H)
  cmake_pop_check_state()
  if(NOT HAVE_X11_EXTENSIONS_SHAPE_H OR NOT X11_Xext_FOUND)
    message(FATAL_ERROR "The X Shape extension (X11/extensions/shape.h and "
      "libXext) was not found; KSnapshot uses it to cut windows to their "
      "shape.  Install the libXext development files, or configure with "
      "-DKDEGRAPHICS_WITH_XSHAPE=OFF to build without it.")
  endif()
endif()
