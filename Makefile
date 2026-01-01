CC = cc
CFLAGS = -O2 -Wall -Iinclude
PKG = gtk4 gtk4-layer-shell-0

BUILD_DIR ?= build
BIN = $(BUILD_DIR)/simple-gui

PREFIX ?= /usr
DESTDIR ?=
BINDIR  = $(PREFIX)/bin
DATADIR = $(PREFIX)/share/simple-gui
APPDIR  = $(PREFIX)/share/applications
SYSTEMDUSERDIR = $(PREFIX)/lib/systemd/user

TOPDIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

SRC = src/main.c src/app.c src/state.c src/config.c src/desktop_match.c src/dock.c src/hypr.c src/hypr_events.c src/watch.c src/launcher.c src/searcher.c

.PHONY: all clean install uninstall

all: $(BIN)

$(BIN): $(SRC)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BIN) $(SRC) \
		$(shell pkg-config --cflags --libs $(PKG))

clean:
	rm -rf $(BUILD_DIR)

install: $(BIN)
	install -Dm755 "$(BIN)" "$(DESTDIR)$(BINDIR)/simple-gui"
	install -Dm644 "$(TOPDIR)/data/config.ini" "$(DESTDIR)$(DATADIR)/config.ini"
	install -Dm644 "$(TOPDIR)/data/style.css"  "$(DESTDIR)$(DATADIR)/style.css"
	install -Dm644 "$(TOPDIR)/packaging/simple-gui.desktop" "$(DESTDIR)$(APPDIR)/simple-gui.desktop"
	install -Dm644 "$(TOPDIR)/packaging/simple-gui.service" "$(DESTDIR)$(SYSTEMDUSERDIR)/simple-gui.service"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/simple-gui"
	rm -f "$(DESTDIR)$(DATADIR)/config.ini" "$(DESTDIR)$(DATADIR)/style.css"
	rm -f "$(DESTDIR)$(APPDIR)/simple-gui.desktop"
	rm -f "$(DESTDIR)$(SYSTEMDUSERDIR)/simple-gui.service"
