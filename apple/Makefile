# Makefile for Quake II.app, requires http://macdylibbundler.sourceforge.net/

DATA = ../data
TARGET = target
QUAKE2 = $(TARGET)/Quake\ II.app
LIBRARIES = $(QUAKE2)/Contents/Libraries
BINARIES = $(QUAKE2)/Contents/MacOS
RESOURCES = $(QUAKE2)/Contents/Resources
DMG = $(TARGET)/Quake\ II\ \(Quake2World.net\).dmg
VOLUME_NAME = Quake\ II\ \(Quake2World.net\)
HTTP_REPOSITORY = quake2world.net:/var/www/quake2world/files

all: bundle

pre-install:
	install -d $(TARGET)
	svn export --force Quake\ II.app $(QUAKE2)

install: pre-install
	install ../bin/quake2 $(BINARIES)/quake2
	install ../bin/game.so $(BINARIES)/baseq2/game.so

install-data:
	svn export --force $(DATA) $(RESOURCES)

bundle: install install-data
	dylibbundler -b \
		-x "$(BINARIES)/quake2" \
	-d "$(LIBRARIES)" -of -p @executable_path/../Libraries

image: bundle
	ln -f -s /Applications $(TARGET)/Applications
	hdiutil create $(DMG) -srcfolder $(TARGET) -volname $(VOLUME_NAME)

release-image: image
	scp $(DMG) $(HTTP_REPOSITORY)

clean:
	rm -rf $(TARGET)
