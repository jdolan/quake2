# Makefile for Quake II.app, requires http://macdylibbundler.sourceforge.net/

TARGET = target
QUAKE2 = $(TARGET)/Quake\ II.app
BINDIR = $(QUAKE2)/Contents/MacOS
DATADIR = $(QUAKE2)/Contents/Resources
DIST = $(TARGET)/Quake\ II\ \(Quetoo.org\).dmg
VOLUME_NAME = Quake\ II\ \(Quetoo.org\)
REMOTE_USER = $(shell whoami)
HTTP_REPOSITORY = quetoo.org:/var/www/quetoo.org/files
HTTP_TARGET = $(REMOTE_USER)@$(HTTP_REPOSITORY)

all: install

pre-install:
	install -d $(TARGET)
	cp -R Quake\ II.app $(QUAKE2)
	find $(QUAKE2) -name .turd -delete

install-bin: pre-install
	make -C .. DESTDIR=apple/$(BINDIR) install

install-lib: install-bin
	install -d $(TARGET)/lib
	dylibbundler -b \
		-x "$(BINDIR)/quake2" \
		-d $(TARGET)/lib -of -p @executable_path/lib
	mv $(TARGET)/lib $(BINDIR)/lib

install: pre-install install-bin install-lib

dist:
	ln -f -s /Applications $(TARGET)/Applications
	hdiutil create $(DIST) -srcfolder $(TARGET) -volname $(VOLUME_NAME)

dist-release:
	rsync -rzhP $(DIST) $(HTTP_TARGET)

clean:
	rm -rf $(TARGET)/*
