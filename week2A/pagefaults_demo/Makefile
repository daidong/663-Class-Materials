CC := gcc
CFLAGS := -O2 -Wall -Wextra -std=c11

all: my_program

my_program: my_program.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f my_program

.PHONY: all clean
