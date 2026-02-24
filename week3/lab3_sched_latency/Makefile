CC=gcc
# -D_GNU_SOURCE enables CPU_SET/CPU_ZERO + sched_setaffinity in some libc setups.
# -D_POSIX_C_SOURCE enables clock_nanosleep + TIMER_ABSTIME.
CFLAGS=-O2 -Wall -Wextra -std=c11 -pthread -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L

all: wakeup_lat cpu_hog

wakeup_lat: wakeup_lat.c
	$(CC) $(CFLAGS) -o $@ $^

cpu_hog: cpu_hog.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f wakeup_lat cpu_hog
