hildondesktoplib_LTLIBRARIES = usb_plugin.la

usb_plugin_la_SOURCES = usb-plugin.c \
						udev-helper.c

usb_plugin_la_CFLAGS = \
	$(LIBGUDEV_CFLAGS) $(LIBHILDONDESKTOP_CFLAGS)

usb_plugin_la_LIBADD = \
	$(LIBGUDEV_LIBS) $(LIBHILDONDESKTOP_LIBS)

usb_plugin_la_LDFLAGS = -Wl,--as-needed, -module -avoid-version

hildonstatusmenudesktopentry_DATA = hildon-status-menu-usb.desktop
EXTRA_DIST = $(hildonstatusmenudesktopentry_DATA)
