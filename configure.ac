AC_INIT([um25c], [0.1.0], [qball@gmpclient.org])

AC_CONFIG_SRCDIR([src/um25c.c])

dnl ---------------------------------------------------------------------
dnl Setup automake to be silent and in foreign mode.
dnl We want xz distribution
dnl ---------------------------------------------------------------------
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects dist-xz])
AM_SILENT_RULES([yes])

##
# Check for compiler
##
AC_PROG_CC([clang gcc cc])
AC_PROG_CC_C99
AM_PROG_CC_C_O

dnl ---------------------------------------------------------------------
dnl Base CFLAGS
dnl ---------------------------------------------------------------------
AM_CFLAGS="-Wall -Wextra -Wparentheses -Winline -pedantic  -Wunreachable-code"

dnl ---------------------------------------------------------------------
dnl System extensions
dnl ---------------------------------------------------------------------
AC_USE_SYSTEM_EXTENSIONS

AC_CHECK_HEADER([math.h],, AC_MSG_ERROR("Could not find math.h header file"))
AC_SEARCH_LIBS([trunc],[m],,      AC_MSG_ERROR("Could not find trunc in math library"))
AC_SEARCH_LIBS([round],[m],,      AC_MSG_ERROR("Could not find round in math library"))

AC_SEARCH_LIBS([clock_gettime],  [rt],, AC_MSG_ERROR("Could not find clock_gettime in rt library"))
AC_SEARCH_LIBS([clock_nanosleep],[rt],, AC_MSG_ERROR("Could not find clock_nanosleep in rt library"))
##
# Create makefile
##
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
