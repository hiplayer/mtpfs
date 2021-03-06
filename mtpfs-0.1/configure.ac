AC_INIT([MTPfs], [0.1], [],[mtpfs])
AM_INIT_AUTOMAKE
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL

AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([strcasecmp])
AC_CHECK_FUNCS([strdup])
AC_CHECK_FUNCS([strncasecmp])
AC_CHECK_FUNCS([strtoul])
AC_FUNC_MALLOC
AC_PREREQ
AC_STRUCT_ST_BLOCKS
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T

PKG_CHECK_MODULES(FUSE, fuse >= 2.2)
AC_SUBST(FUSE_CFLAGS)
AC_SUBST(FUSE_LIBS)

PKG_CHECK_MODULES(MTP, libmtp >= 1.1.0)
AC_SUBST(MTP_CFLAGS)
AC_SUBST(MTP_LIBS)

PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.6 \
                        gthread-2.0 >= 1.2 \
                        gio-2.0 >= 2.6)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

AC_ARG_ENABLE(debug,
              AC_HELP_STRING([--enable-debug],
                             [enable debugging features]),
              , enable_debug=no)
if test "x$enable_debug" != xyes -a "x$enable_debug" != xno; then
   AC_MSG_ERROR(You may not pass an argument to --enable-debug)
fi

if test "x$enable_debug" = xyes; then
   AC_DEFINE(DEBUG,1,[Define if debug logging is enabled])
else
   AC_DEFINE(DEBUG,0,[Define if debug logging is enabled])
fi


AC_CONFIG_FILES([Makefile])
AC_OUTPUT
