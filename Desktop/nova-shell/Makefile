CC = gcc
CFLAGS = -Wall -Wextra -Iinclude

SRC = src/main.c src/pipe.c src/monitor.c src/safety.c \
      src/summary.c src/suggest.c src/history.c

OUT = build/nova-shell

all:
	$(CC) $(CFLAGS) $(SRC) -o $(OUT)

clean:
	rm -f build/*
