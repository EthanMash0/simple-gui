CC = cc
CFLAGS = -O2 -Wall -Iinclude
PKG = gtk4 gtk4-layer-shell-0

SRC = src/main.c src/app.c src/state.c src/config.c src/desktop_match.c src/dock.c src/hypr.c src/hypr_events.c src/watch.c src/launcher.c
BIN = hyprdock

all:
	$(CC) $(CFLAGS) -o $(BIN) $(SRC) \
		$(shell pkg-config --cflags --libs $(PKG))

clean:
	rm -f $(BIN)
