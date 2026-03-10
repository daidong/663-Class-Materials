/*
 * cpu_hog.c - Simple CPU stress program
 *
 * Runs an infinite loop to consume CPU.
 * Use with cgroup CPU limits to verify enforcement.
 *
 * Compile: gcc -O2 -o cpu_hog cpu_hog.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

volatile int running = 1;

void sigint_handler(int sig) {
    (void)sig;
    running = 0;
}

int main(int argc, char **argv) {
    int duration = 60;  /* Default: run for 60 seconds */
    
    if (argc > 1) {
        duration = atoi(argv[1]);
    }

    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    printf("cpu_hog: Starting CPU stress for %d seconds\n", duration);
    printf("cpu_hog: Press Ctrl+C to stop\n");

    volatile unsigned long counter = 0;
    time_t start = time(NULL);
    time_t now;

    while (running) {
        /* Busy loop */
        counter++;
        
        /* Check time periodically */
        if (counter % 10000000 == 0) {
            now = time(NULL);
            if (now - start >= duration) {
                break;
            }
        }
    }

    printf("cpu_hog: Done. Counter = %lu\n", counter);
    return 0;
}
