// Lab 3 load generator: create CPU contention.
//
// Example:
//   ./cpu_hog --threads 4 --cpu 0
//
// Use with nice/affinity to study tail latency.

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    int cpu;
    int id;
} worker_arg_t;

static void pin_to_cpu(int cpu) {
    if (cpu < 0) return;
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0) {
        perror("sched_setaffinity");
    }
}

static void *worker(void *p) {
    worker_arg_t *a = (worker_arg_t *)p;
    pin_to_cpu(a->cpu);

    volatile unsigned long long x = (unsigned long long)a->id + 1;
    while (1) {
        // Busy work: keeps the CPU runnable.
        x = x * 1664525ull + 1013904223ull;
        if ((x & 0xFFFFF) == 0) {
            // Small pause to reduce terminal/measurement noise.
            __asm__ volatile("" ::: "memory");
        }
    }
    return NULL;
}

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s [--threads N] [--cpu C]\n"
            "  --threads N  number of hog threads (default: 4)\n"
            "  --cpu C      pin all threads to CPU core (default: -1 = no pin)\n",
            prog);
}

int main(int argc, char **argv) {
    int threads = 4;
    int cpu = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            threads = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--cpu") == 0 && i + 1 < argc) {
            cpu = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            usage(argv[0]);
            return 2;
        }
    }

    if (threads <= 0) {
        fprintf(stderr, "--threads must be > 0\n");
        return 2;
    }

    pthread_t *t = calloc((size_t)threads, sizeof(pthread_t));
    worker_arg_t *args = calloc((size_t)threads, sizeof(worker_arg_t));
    if (!t || !args) {
        perror("calloc");
        return 1;
    }

    fprintf(stderr, "cpu_hog: starting %d threads (cpu=%d)\n", threads, cpu);

    for (int i = 0; i < threads; i++) {
        args[i].cpu = cpu;
        args[i].id = i;
        int rc = pthread_create(&t[i], NULL, worker, &args[i]);
        if (rc != 0) {
            errno = rc;
            perror("pthread_create");
            return 1;
        }
    }

    // Wait forever.
    for (int i = 0; i < threads; i++) {
        pthread_join(t[i], NULL);
    }

    return 0;
}
