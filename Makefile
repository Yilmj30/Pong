CC = gcc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
SRC = src/server.c src/app.c src/game.c src/protocol.c
.PHONY: all test clean client
all: server
server: $(SRC) src/app.h src/game.h src/protocol.h
	$(CC) $(CFLAGS) -Isrc $(SRC) -o $@
client:
	python3 client/pong.py
test: server
	$(CC) $(CFLAGS) -Isrc tests/test_game.c src/game.c -o tests/test_game
	./tests/test_game
	python3 tests/integration.py
clean:
	rm -f server tests/test_game
