usb_plugin.so: usb-plugin.c hal-helper.c hal-helper.h
	$(CC) $(CFLAGS) $(LDFLAGS) usb-plugin.c hal-helper.c -shared -o usb_plugin.so -fPIC `pkg-config --libs --cflags libhildondesktop-1 hal dbus-1 glib-2.0 hildon-1`

clean:
	$(RM) usb_plugin.so
