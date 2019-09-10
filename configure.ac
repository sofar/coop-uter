#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([coop-uter], [1], [sofar@foo-projects.org])
AM_INIT_AUTOMAKE([foreign -Wall -Werror -Wno-portability silent-rules subdir-objects color-tests no-dist-gzip dist-xz])
AC_CONFIG_SRCDIR([dump.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
PKG_CHECK_MODULES([modbus], [libmodbus])
PKG_CHECK_MODULES([mosquitto], [libmosquitto])
PKG_CHECK_MODULES([gpiod], [libgpiod])
PKG_CHECK_MODULES([config], [libconfig])

# Checks for header files.
AC_CHECK_HEADERS([limits.h])
AC_CHECK_HEADERS([stdlib.h])
AC_CHECK_HEADERS([string.h])
AC_CHECK_HEADERS([unistd.h])
AC_CHECK_HEADER_STDBOOL

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT8_T
AC_TYPE_UINT16_T

# Checks for library functions.
AC_CHECK_FUNCS([gethostname])
AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([strdup])
AC_CHECK_FUNCS([strerror])

AC_OUTPUT
