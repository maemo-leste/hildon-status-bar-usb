AC_INIT([Status area USB applet],
        [3.1.0],
        [merlijn@wizzup.org], [hildon-status-bar-usb])

AM_INIT_AUTOMAKE
AC_CONFIG_FILES([Makefile])

AC_PROG_CC
AC_PROG_LIBTOOL

PKG_CHECK_MODULES(LIBGUDEV, gudev-1.0)
AC_SUBST(LIBGUDEV_CFLAGS)
AC_SUBST(LIBGUDEV_LIBS)

PKG_CHECK_MODULES(LIBHILDONDESKTOP, libhildondesktop-1)
AC_SUBST(LIBHILDONDESKTOP_CFLAGS)
AC_SUBST(LIBHILDONDESKTOP_LIBS)

hildonstatusmenudesktopentrydir="`$PKG_CONFIG --variable=hildonstatusmenudesktopentrydir libhildondesktop-1`"
AC_SUBST(hildonstatusmenudesktopentrydir)

hildondesktoplibdir="`$PKG_CONFIG --variable=hildondesktoplibdir osso-af-settings`"
AC_SUBST(hildondesktoplibdir)

AC_OUTPUT
