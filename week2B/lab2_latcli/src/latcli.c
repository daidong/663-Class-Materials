// Lab 2 workload: per-iteration latency CLI
//
// This program allocates a working set and repeatedly touches it.
// Each iteration prints: iter=<i> latency_us=<x>
//
// The lab uses cgroup memory limits to create memory pressure.

#define _GNU_SOURCE
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [--iters N] [--workset-mb M] [--stride B] [--touch-per-iter K]\n"
        "  --iters N           Number of iterations (default: 20000)\n"
        "  --workset-mb M      Working set size in MiB (default: 256)\n"
        "  --stride B          Stride in bytes between touches (default: 4096)\n"
        "  --touch-per-iter K  Touches per iteration (default: 64)\n",
        prog);
}

static int parse_u64(const char *s, uint64_t *out) {
    errno = 0;
    char *end = NULL;
    unsigned long long v = strtoull(s, &end, 10);
    if (errno || !end || *end != '\0') return -1;
    *out = (uint64_t)v;
    return 0;
}

int main(int argc, char **argv) {
    uint64_t iters = 20000;
    uint64_t workset_mb = 256;
    uint64_t stride = 4096;
    uint64_t touch_per_iter = 64;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--iters") == 0 && i + 1 < argc) {
            if (parse_u64(argv[++i], &iters) != 0) return 2;
        } else if (strcmp(argv[i], "--workset-mb") == 0 && i + 1 < argc) {
            if (parse_u64(argv[++i], &workset_mb) != 0) return 2;
        } else if (strcmp(argv[i], "--stride") == 0 && i + 1 < argc) {
            if (parse_u64(argv[++i], &stride) != 0) return 2;
        } else if (strcmp(argv[i], "--touch-per-iter") == 0 && i + 1 < argc) {
            if (parse_u64(argv[++i], &touch_per_iter) != 0) return 2;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            usage(argv[0]);
            return 2;
        }
    }

    const uint64_t bytes = workset_mb * 1024ull * 1024ull;
    if (bytes == 0 || stride == 0) {
        fprintf(stderr, "workset and stride must be > 0\n");
        return 2;
    }

    unsigned char *buf = (unsigned char *)malloc((size_t)bytes);
    if (!buf) {
        perror("malloc");
        return 1;
    }

    // Touch once to commit pages.
    for (uint64_t off = 0; off < bytes; off += 4096) {
        buf[off] ^= 1;
    }

    for (uint64_t i = 0; i < iters; i++) {
        uint64_t start = now_ns();

        // Page-stride touches: emphasizes page-level behavior.
        uint64_t idx = (i * 1315423911ull) % bytes;
        for (uint64_t k = 0; k < touch_per_iter; k++) {
            idx = (idx + stride) % bytes;
            buf[idx] ^= (unsigned char)(k + 1);
        }

        uint64_t end = now_ns();
        uint64_t us = (end - start) / 1000ull;
        printf("iter=%" PRIu64 " latency_us=%" PRIu64 "\n", i, us);
    }

    free(buf);
    return 0;
}
