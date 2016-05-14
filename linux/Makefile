# Makefile for Quake2 (Quake2World.net) Linux distributable

TARGET = target
QUAKE2 = $(TARGET)/quake2
BINDIR = $(QUAKE2)/bin
DATADIR = $(QUAKE2)/share
DIST = $(TARGET)/quake2-quetoo.org-x86_64.tar.gz
REMOTE_USER = $(shell whoami)
HTTP_REPOSITORY = quetoo.org:/var/www/quetoo.org/files
HTTP_TARGET = $(REMOTE_USER)@$(HTTP_REPOSITORY)

all: install

pre-install:
	install -d $(TARGET)
	cp -R quake2 $(QUAKE2)
	find $(QUAKE2) -name .turd -delete


install-bin: pre-install
	make -C .. DESTDIR=$(realpath $(BINDIR)) install

install-lib:
	# Nothing to do here on Linux

install: pre-install install-bin install-lib

dist:
	tar -czvf $(DIST) -C $(TARGET) quake2

dist-release:
	rsync -rzhP $(DIST) $(HTTP_TARGET)

clean:
	rm -rf $(TARGET)/*
