/*
 * mem_hog.c - Simple memory stress program
 *
 * Allocates memory in chunks until allocation fails or limit is reached.
 * Use with cgroup memory limits to verify enforcement.
 *
 * Compile: gcc -O2 -o mem_hog mem_hog.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#define CHUNK_SIZE (1024 * 1024)  /* 1 MB chunks */

volatile int running = 1;

void sigint_handler(int sig) {
    (void)sig;
    running = 0;
}

int main(int argc, char **argv) {
    int max_mb = 1024;  /* Default: try to allocate up to 1GB */
    
    if (argc > 1) {
        max_mb = atoi(argv[1]);
    }

    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    printf("mem_hog: Attempting to allocate up to %d MB\n", max_mb);
    printf("mem_hog: Allocating in %d KB chunks\n", CHUNK_SIZE / 1024);

    int allocated = 0;
    void **chunks = malloc(sizeof(void*) * max_mb);
    if (!chunks) {
        fprintf(stderr, "mem_hog: Failed to allocate chunk array\n");
        return 1;
    }

    while (running && allocated < max_mb) {
        chunks[allocated] = malloc(CHUNK_SIZE);
        
        if (!chunks[allocated]) {
            printf("mem_hog: Allocation failed at %d MB\n", allocated);
            break;
        }

        /* Touch the memory to ensure it's actually allocated */
        memset(chunks[allocated], 'x', CHUNK_SIZE);
        
        allocated++;
        
        if (allocated % 10 == 0) {
            printf("mem_hog: Allocated %d MB\n", allocated);
        }

        /* Small delay to make it easier to observe */
        usleep(10000);  /* 10ms */
    }

    printf("mem_hog: Total allocated: %d MB\n", allocated);
    
    /* Keep memory allocated for a moment */
    if (running) {
        printf("mem_hog: Holding memory for 2 seconds...\n");
        sleep(2);
    }

    /* Free memory */
    printf("mem_hog: Freeing memory...\n");
    for (int i = 0; i < allocated; i++) {
        free(chunks[i]);
    }
    free(chunks);

    printf("mem_hog: Done\n");
    return 0;
}
