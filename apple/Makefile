# Makefile for Quake II.app, requires http://macdylibbundler.sourceforge.net/

DATA = ../data
TARGET = target
QUAKE2 = $(TARGET)/Quake\ II.app
BINDIR = $(QUAKE2)/Contents/MacOS
DATADIR = $(QUAKE2)/Contents/Resources
DIST = $(TARGET)/Quake\ II\ \(Quake2World.net\).dmg
VOLUME_NAME = Quake\ II\ \(Quake2World.net\)
REMOTE_USER = $(shell whoami)
HTTP_REPOSITORY = quake2world.net:/var/www/quake2world.net/files
HTTP_TARGET = $(REMOTE_USER)@$(HTTP_REPOSITORY)

all: install-lib

pre-install:
	install -d $(TARGET)
	cp -R Quake\ II.app $(QUAKE2)
	find $(QUAKE2) -name .turd -delete

install: pre-install
	install ../bin/quake2 $(BINDIR)/quake2
	install ../bin/game.so $(BINDIR)/baseq2/game.so

install-lib: install
	dylibbundler -b \
		-x "$(BINDIR)/quake2" \
	-d "$(BINDIR)" -of -p @executable_path

install-data:
	cp -R $(DATA)/ $(DATADIR)
	find $(DATADIR) -name .turd -delete

dist:
	ln -f -s /Applications $(TARGET)/Applications
	hdiutil create $(DIST) -srcfolder $(TARGET) -volname $(VOLUME_NAME)

dist-release:
	rsync -rzhP $(DIST) $(HTTP_TARGET)

clean:
	rm -rf $(TARGET)/*
	
.PHONY: all pre-install install install-lib install-data dist dist-release
