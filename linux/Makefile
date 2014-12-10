# Makefile for Quake2 (Quake2World.net) Linux distributable

TARGET = target
QUAKE2 = $(TARGET)/quake2
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

install-bin: pre-install
	make -C .. DESTDIR=$(realpath $(BINDIR)) install

post-install: install
	find $(QUAKE2) -name .turd -delete

install: pre-install install post-install

dist:
	tar -czvf $(DIST) -C $(QUAKE2)

dist-release:
	rsync -rzhP $(DIST) $(HTTP_TARGET)

clean:
	rm -rf $(TARGET)/*
