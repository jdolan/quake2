# Makefile for Quake2 (Quake2World.net) Linux distributable

DATA = ../data
TARGET = target
QUAKE2 = $(TARGET)/quake2
LIBDIR = $(QUAKE2)/lib
BINDIR = $(QUAKE2)/bin
DATADIR = $(QUAKE2)/share
DIST = $(TARGET)/quake2-quake2world.net-x86_64.tar.gz
REMOTE_USER = $(shell whoami)
HTTP_REPOSITORY = quake2world.net:/var/www/quake2world/files
HTTP_TARGET = $(REMOTE_USER)@$(HTTP_REPOSITORY)

all: install

pre-install:
	install -d $(TARGET)
	cp -R quake2 $(QUAKE2)
	find $(QUAKE2) -name .turd -delete

install: pre-install
	install ../bin/quake2 $(BINDIR)/quake2
	install ../bin/game.so $(LIBDIR)/baseq2/game.so

install-data:
	cp -R $(DATA)/ $(DATADIR)
	find $(DATADIR) -name .turd -delete

dist:
	tar -czvf $(DIST) -C $(QUAKE2)

dist-release:
	rsync -rzhP $(DIST) $(HTTP_TARGET)

clean:
	rm -rf $(TARGET)/*

.PHONY: all pre-install install install-data dist dist-release
