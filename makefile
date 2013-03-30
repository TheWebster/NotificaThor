SHELL   = /bin/sh
INSTDIR = $(prefix)/usr/bin
MANDIR  = $(prefix)/usr/share/man

all:
	make -Csrc
	make -Cdocs

clean-all:
	make -Csrc clean
	make -Cdocs clean
	rm -r bin

install: all
	cp bin/* $(INSTDIR)
	cp docs/NotificaThor.1.gz $(MANDIR)/man1
	cp docs/thor-cli.1.gz $(MANDIR)/man1
	cp docs/NotificaThor-themes.5.gz $(MANDIR)/man5

uninstall:
	rm $(INSTDIR)/notificathor $(INSTDIR)/thor-cli
	rm $(MANDIR)/man1/NotificaThor.1.gz
	rm $(MANDIR)/man1/thor-cli.1.gz
	rm $(MANDIR)/man5/NotificaThor-themes.5.gz
	

