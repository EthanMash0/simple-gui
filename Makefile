CC = cc
CFLAGS = -O2 -Wall -Iinclude
PKG = gtk4 gtk4-layer-shell-0

SRC = src/main.c src/config.c src/hypr.c
BIN = hyprdock

all:
	$(CC) $(CFLAGS) -o $(BIN) $(SRC) \
		$(shell pkg-config --cflags --libs $(PKG))

clean:
	rm -f $(BIN)
