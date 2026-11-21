# Configure checks for config.h, following configure.in.in and the
# subdirectories' configure.in.in.  Only what the sources actually test
# is checked.

include(CheckIncludeFile)
include(CheckFunctionExists)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckTypeSize)
include(CheckCSourceRuns)
include(CMakePushCheckState)

# ---- system ----------------------------------------------------------------
check_include_file(stdlib.h HAVE_STDLIB_H)
check_type_size("unsigned long" SIZEOF_UNSIGNED_LONG)

# AC_C_BIGENDIAN (khexedit/configure.in.in)
if(CMAKE_C_BYTE_ORDER STREQUAL "BIG_ENDIAN")
  set(WORDS_BIGENDIAN 1)
endif()

# ---- kcalc (kcalc/configure.in.in, kcalc/knumber/configure.in.in) ---------
# Without GMP configure left kcalc out (DO_NOT_COMPILE); here that is an
# explicit choice.
option(KDEUTILS_BUILD_KCALC "Build KCalc (needs the GMP library)" ON)
if(KDEUTILS_BUILD_KCALC)
  find_path(GMP_INCLUDE_DIR gmp.h)
  find_library(GMP_LIBRARY gmp)
  if(NOT GMP_INCLUDE_DIR OR NOT GMP_LIBRARY)
    message(FATAL_ERROR "GMP (gmp.h, libgmp) was not found; KCalc needs it "
      "for its arbitrary precision numbers.  Install it, or configure with "
      "-DKDEUTILS_BUILD_KCALC=OFF to build without KCalc.")
  endif()
  mark_as_advanced(GMP_INCLUDE_DIR GMP_LIBRARY)
  set(HAVE_GMP 1)

  check_include_file(ieeefp.h HAVE_IEEEFP_H)

  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_LIBRARIES m)
  set(CMAKE_REQUIRED_DEFINITIONS -D_ISOC99_SOURCE -D_GNU_SOURCE)
  # KDE_C_LONG_DOUBLE: long double is longer than double and printf
  # knows %Lf
  check_c_source_runs("
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
int main(void) {
  long double foo = 1.0;
  char buffer[10];
  int result = (sizeof(long double) <= sizeof(double));
  if (!result) {
    foo = foo * 3;
    sprintf(buffer, \"%0.0Lf\", foo);
    result = strcmp(buffer, \"3\");
    foo = powl(fabsl(foo), 1);
  }
  return result;
}" HAVE_LONG_DOUBLE)
  cmake_pop_check_state()

  check_library_exists(m asinl "" _have_asinl)
  check_library_exists(m sqrtl "" _have_sqrtl)
  if(_have_asinl AND _have_sqrtl)
    set(HAVE_L_FUNCS 1)
  endif()
  check_library_exists(m isinf "" HAVE_FUNC_ISINF)
  check_library_exists(m round "" HAVE_FUNC_ROUND)
  check_library_exists(m roundl "" HAVE_FUNC_ROUNDL)
endif()
