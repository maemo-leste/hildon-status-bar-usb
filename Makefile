# /usr/bin/ld: relocation R_X86_64_PC32 against symbol `stderr@@GLIBC_2.2.5'
# can not be used when making a shared object; recompile with -fPIC
CFLAGS += -fPIC

all: usb_plugin.so

install:
	install -d "$(DESTDIR)/usr/lib/hildon-desktop/"
	install -d "$(DESTDIR)/usr/share/applications/hildon-status-menu/"
	install -m 644 usb_plugin.so "$(DESTDIR)/usr/lib/hildon-desktop/"
	install -m 644 hildon-status-menu-usb.desktop "$(DESTDIR)/usr/share/applications/hildon-status-menu/"

clean:
	$(RM) usb_plugin.so

usb_plugin.so: usb-plugin.c udev-helper.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(shell pkg-config --cflags --libs gudev-1.0 libusbgx libhildondesktop-1) -W -Wall -O2 -shared $^ -o $@

.PHONY: all install clean
