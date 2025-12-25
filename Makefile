CC = cc
CFLAGS = -O2 -Wall -Iinclude
PKG = gtk4 gtk4-layer-shell-0

BUILD_DIR ?= build
BIN = $(BUILD_DIR)/hyprdock

PREFIX ?= /usr
DESTDIR ?=
BINDIR  = $(PREFIX)/bin
DATADIR = $(PREFIX)/share/hyprdock
APPDIR  = $(PREFIX)/share/applications
# ICONDIR = $(PREFIX)/share/icons/hicolor/256x256/apps
SYSTEMDUSERDIR = $(PREFIX)/lib/systemd/user

SRC = src/main.c src/app.c src/state.c src/config.c src/desktop_match.c src/dock.c src/hypr.c src/hypr_events.c src/watch.c src/launcher.c

all: $(BIN)

$(BIN): $(SRC)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BIN) $(SRC) \
		$(shell pkg-config --cflags --libs $(PKG))

clean:
	rm -rf $(BUILD_DIR)

install: hyprdock
	install -Dm755 hyprdock "$(DESTDIR)$(BINDIR)/hyprdock"
	install -Dm644 config.ini "$(DESTDIR)$(DATADIR)/config.ini"
	install -Dm644 style.css  "$(DESTDIR)$(DATADIR)/style.css"
	install -Dm644 packaging/hyprdock.desktop "$(DESTDIR)$(APPDIR)/hyprdock.desktop"
	# install -Dm644 packaging/hyprdock.png "$(DESTDIR)$(ICONDIR)/hyprdock.png"
	install -Dm644 packaging/hyprdock.service "$(DESTDIR)$(SYSTEMDUSERDIR)/hyprdock.service"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/hyprdock"
	rm -f "$(DESTDIR)$(DATADIR)/config.ini" "$(DESTDIR)$(DATADIR)/style.css"
	rm -f "$(DESTDIR)$(APPDIR)/hyprdock.desktop"
	# rm -f "$(DESTDIR)$(ICONDIR)/hyprdock.png"
	rm -f "$(DESTDIR)$(SYSTEMDUSERDIR)/hyprdock.service"
