AC_INIT([audiostreamer], [0.1], [bug-automake@gnu.org])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CC
AC_CONFIG_HEADERS([config.h])
PKG_CHECK_MODULES(GST, gstreamer-1.0 >= 1.14)
AC_CONFIG_FILES([
 Makefile
 src/Makefile
])
AC_OUTPUT
