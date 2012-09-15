# Makefile for Quake2 (Quake2World.net) Linux distributable

DATA = ../data
TARGET = target
QUAKE2 = $(TARGET)/quake2
LIBRARIES = $(QUAKE2)/lib
BINARIES = $(QUAKE2)/bin
RESOURCES = $(QUAKE2)/share
TGZ = $(TARGET)/quake2-quake2world.net-x86_64.tar.gz
HTTP_REPOSITORY = quake2world.net:/var/www/quake2world/files

all: bundle

pre-install:
	install -d $(TARGET)
	svn export --force quake2 $(QUAKE2)

install: pre-install
	install ../bin/quake2 $(BINARIES)/quake2
	install ../bin/game.so $(LIBRARIES)/baseq2/game.so

install-data:
	svn export --force $(DATA) $(RESOURCES)

bundle: install install-data

image: bundle
	tar -czvf $(TGZ) -C $(QUAKE2)

release-image: image
	scp $(TGZ) $(HTTP_REPOSITORY)

clean:
	rm -rf $(TARGET)
