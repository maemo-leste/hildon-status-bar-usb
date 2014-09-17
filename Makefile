all: usb_plugin.so

install:
	install -d "$(DESTDIR)/usr/lib/hildon-desktop/"
	install -d "$(DESTDIR)/usr/share/applications/hildon-status-menu/"
	install -m 644 usb_plugin.so "$(DESTDIR)/usr/lib/hildon-desktop/"
	install -m 644 hildon-status-menu-usb.desktop "$(DESTDIR)/usr/share/applications/hildon-status-menu/"

clean:
	$(RM) usb_plugin.so

usb_plugin.so: usb-plugin.c hal-helper.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(shell pkg-config --cflags --libs hal libhildondesktop-1) -W -Wall -O2 -shared $^ -o $@

.PHONY: all install clean
