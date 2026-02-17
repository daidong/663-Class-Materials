CC=gcc
CFLAGS=-O2 -Wall -Wextra -std=c11

all: latcli

latcli: src/latcli.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f latcli
