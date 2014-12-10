# Makefile for Quake II.app, requires http://macdylibbundler.sourceforge.net/

TARGET = target
QUAKE2 = $(TARGET)/Quake\ II.app
BINDIR = $(QUAKE2)/Contents/MacOS
DATADIR = $(QUAKE2)/Contents/Resources
DIST = $(TARGET)/Quake\ II\ \(Quake2World.net\).dmg
VOLUME_NAME = Quake\ II\ \(Quake2World.net\)
REMOTE_USER = $(shell whoami)
HTTP_REPOSITORY = quake2world.net:/var/www/quake2world.net/files
HTTP_TARGET = $(REMOTE_USER)@$(HTTP_REPOSITORY)

all: install

pre-install:
	install -d $(TARGET)
	cp -R Quake\ II.app $(QUAKE2)

install-bin: pre-install
	make -C .. DESTDIR=apple/$(BINDIR) install

install-lib: install-bin
	dylibbundler -b \
		-x "$(BINDIR)/quake2" \
	-d "$(BINDIR)" -of -p @executable_path

post-install: install-lib
	find $(QUAKE2) -name .turd -delete
	mv $(BINDIR)/install-data.py $(DATADIR)

install: pre-install install-bin install-lib post-install

dist:
	ln -f -s /Applications $(TARGET)/Applications
	hdiutil create $(DIST) -srcfolder $(TARGET) -volname $(VOLUME_NAME)

dist-release:
	rsync -rzhP $(DIST) $(HTTP_TARGET)

clean:
	rm -rf $(TARGET)/*
